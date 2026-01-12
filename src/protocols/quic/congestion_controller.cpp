/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "kcenon/network/protocols/quic/congestion_controller.h"

#include <algorithm>
#include <cmath>

namespace kcenon::network::protocols::quic
{

auto congestion_state_to_string(congestion_state state) noexcept -> const char*
{
    switch (state)
    {
    case congestion_state::slow_start:
        return "slow_start";
    case congestion_state::congestion_avoidance:
        return "congestion_avoidance";
    case congestion_state::recovery:
        return "recovery";
    default:
        return "unknown";
    }
}

congestion_controller::congestion_controller()
    : congestion_controller(kDefaultMaxDatagramSize)
{
}

congestion_controller::congestion_controller(size_t max_datagram_size)
    : state_(congestion_state::slow_start)
    , max_datagram_size_(max_datagram_size)
    , congestion_recovery_start_()
{
    initial_window_ = kInitialWindowPackets * max_datagram_size_;
    minimum_window_ = kMinimumWindowPackets * max_datagram_size_;
    cwnd_ = initial_window_;
}

auto congestion_controller::can_send(size_t bytes) const noexcept -> bool
{
    if (bytes == 0)
    {
        return bytes_in_flight_ < cwnd_;
    }
    return bytes_in_flight_ + bytes <= cwnd_;
}

auto congestion_controller::available_window() const noexcept -> size_t
{
    if (bytes_in_flight_ >= cwnd_)
    {
        return 0;
    }
    return cwnd_ - bytes_in_flight_;
}

auto congestion_controller::on_packet_sent(size_t bytes) -> void
{
    bytes_in_flight_ += bytes;
}

auto congestion_controller::on_packet_acked(const sent_packet& packet,
                                             std::chrono::steady_clock::time_point ack_time)
    -> void
{
    // Update bytes in flight
    if (packet.in_flight && bytes_in_flight_ >= packet.sent_bytes)
    {
        bytes_in_flight_ -= packet.sent_bytes;
    }

    // Don't increase cwnd in recovery
    if (is_in_recovery(packet.sent_time))
    {
        return;
    }

    // Exit recovery if we're receiving ACKs for packets sent after recovery started
    if (state_ == congestion_state::recovery)
    {
        state_ = congestion_state::congestion_avoidance;
    }

    // Congestion window increase (RFC 9002 Section 7.3.1)
    if (state_ == congestion_state::slow_start)
    {
        // Slow start: increase cwnd by the number of bytes acknowledged
        cwnd_ += packet.sent_bytes;

        // Check if we've exceeded slow start threshold
        if (cwnd_ >= ssthresh_)
        {
            state_ = congestion_state::congestion_avoidance;
        }
    }
    else
    {
        // Congestion avoidance: AIMD
        // cwnd += max_datagram_size * acked_bytes / cwnd
        auto increment = (max_datagram_size_ * packet.sent_bytes) / cwnd_;
        if (increment == 0)
        {
            increment = 1;  // Always increase by at least 1 byte
        }
        cwnd_ += increment;
    }
}

auto congestion_controller::on_packet_lost(const sent_packet& packet) -> void
{
    // Update bytes in flight
    if (packet.in_flight && bytes_in_flight_ >= packet.sent_bytes)
    {
        bytes_in_flight_ -= packet.sent_bytes;
    }

    // Trigger congestion event
    on_congestion_event(packet.sent_time);
}

auto congestion_controller::on_congestion_event(
    std::chrono::steady_clock::time_point sent_time) -> void
{
    // Only respond to congestion once per RTT (RFC 9002 Section 7.3.2)
    if (is_in_recovery(sent_time))
    {
        return;
    }

    // Enter recovery
    congestion_recovery_start_ = std::chrono::steady_clock::now();
    state_ = congestion_state::recovery;

    // Reduce cwnd and set ssthresh
    // ssthresh = cwnd * kLossReductionFactor
    // cwnd = max(ssthresh, minimum_window)
    ssthresh_ = static_cast<size_t>(
        static_cast<double>(cwnd_) * kLossReductionFactor);
    cwnd_ = std::max(ssthresh_, minimum_window_);
}

auto congestion_controller::on_ecn_congestion(
    std::chrono::steady_clock::time_point sent_time) -> void
{
    // RFC 9002 Section 7.1: Processing ECN Information
    // ECN-CE marks indicate congestion without packet loss
    // Respond same as packet loss, but only once per RTT

    // Only respond to congestion once per RTT
    if (is_in_recovery(sent_time))
    {
        return;
    }

    // Enter recovery
    congestion_recovery_start_ = std::chrono::steady_clock::now();
    state_ = congestion_state::recovery;

    // Reduce cwnd and set ssthresh
    // RFC 9002 Section 7.1: "a sender that receives an ACK frame with
    // ECN feedback indicating the CE codepoint MUST enter congestion
    // avoidance"
    ssthresh_ = static_cast<size_t>(
        static_cast<double>(cwnd_) * kLossReductionFactor);
    cwnd_ = std::max(ssthresh_, minimum_window_);
}

auto congestion_controller::on_persistent_congestion(const rtt_estimator& rtt) -> void
{
    // RFC 9002 Section 7.6: Reset cwnd to minimum
    cwnd_ = minimum_window_;
    ssthresh_ = cwnd_;
    state_ = congestion_state::slow_start;
    congestion_recovery_start_ = std::chrono::steady_clock::time_point{};
}

auto congestion_controller::is_in_recovery(
    std::chrono::steady_clock::time_point sent_time) const noexcept -> bool
{
    if (state_ != congestion_state::recovery)
    {
        return false;
    }

    // We're in recovery if the packet was sent before recovery started
    return sent_time <= congestion_recovery_start_;
}

auto congestion_controller::set_max_datagram_size(size_t size) -> void
{
    max_datagram_size_ = size;
    initial_window_ = kInitialWindowPackets * max_datagram_size_;
    minimum_window_ = kMinimumWindowPackets * max_datagram_size_;

    // Ensure cwnd is at least minimum window
    cwnd_ = std::max(cwnd_, minimum_window_);
}

auto congestion_controller::reset() -> void
{
    state_ = congestion_state::slow_start;
    cwnd_ = initial_window_;
    ssthresh_ = std::numeric_limits<size_t>::max();
    bytes_in_flight_ = 0;
    congestion_recovery_start_ = std::chrono::steady_clock::time_point{};
}

} // namespace kcenon::network::protocols::quic
