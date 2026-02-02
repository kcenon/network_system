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

#include "internal/protocols/quic/loss_detector.h"

#include <algorithm>
#include <cmath>

namespace kcenon::network::protocols::quic
{

loss_detector::loss_detector(rtt_estimator& rtt)
    : rtt_(rtt)
    , ecn_tracker_{}
    , spaces_{}
    , pto_count_{0}
    , handshake_confirmed_{false}
    , loss_detection_timer_{}
    , timer_armed_{false}
{
}

auto loss_detector::space_index(encryption_level level) const noexcept -> size_t
{
    // Map encryption levels to packet number spaces:
    // Initial -> 0, Handshake -> 1, Zero-RTT/Application -> 2
    switch (level)
    {
    case encryption_level::initial:
        return 0;
    case encryption_level::handshake:
        return 1;
    case encryption_level::zero_rtt:
    case encryption_level::application:
        return 2;
    default:
        return 2;
    }
}

auto loss_detector::on_packet_sent(const sent_packet& packet) -> void
{
    auto idx = space_index(packet.level);
    auto& space = spaces_[idx];

    space.sent_packets[packet.packet_number] = packet;

    if (packet.in_flight)
    {
        space.bytes_in_flight += packet.sent_bytes;
    }

    if (packet.ack_eliciting)
    {
        space.time_of_last_ack_eliciting = packet.sent_time;
    }

    set_loss_detection_timer();
}

auto loss_detector::on_ack_received(const ack_frame& ack,
                                    encryption_level level,
                                    std::chrono::steady_clock::time_point recv_time)
    -> loss_detection_result
{
    loss_detection_result result;
    auto idx = space_index(level);
    auto& space = spaces_[idx];

    auto largest_newly_acked = ack.largest_acknowledged;

    if (!space.largest_acked_set || largest_newly_acked > space.largest_acked)
    {
        space.largest_acked = largest_newly_acked;
        space.largest_acked_set = true;
    }

    // Process the ACK ranges to find newly acknowledged packets
    // First range starts at largest_acknowledged and goes back ack_range_count
    uint64_t current_pn = ack.largest_acknowledged;
    size_t first_range_count = ack.ranges.empty() ? 0 : ack.ranges[0].length;

    // Process first ack block (largest_acknowledged down to largest - first_range)
    for (uint64_t i = 0; i <= first_range_count && current_pn > 0; ++i)
    {
        auto it = space.sent_packets.find(current_pn - i);
        if (it != space.sent_packets.end())
        {
            auto& pkt = it->second;
            if (pkt.in_flight)
            {
                space.bytes_in_flight -= pkt.sent_bytes;
            }
            result.acked_packets.push_back(std::move(pkt));
            space.sent_packets.erase(it);
        }
    }

    // Also check largest_acknowledged itself
    {
        auto it = space.sent_packets.find(ack.largest_acknowledged);
        if (it != space.sent_packets.end())
        {
            auto& pkt = it->second;
            if (pkt.in_flight)
            {
                space.bytes_in_flight -= pkt.sent_bytes;
            }
            result.acked_packets.push_back(std::move(pkt));
            space.sent_packets.erase(it);
        }
    }

    // Process additional ACK ranges
    current_pn = ack.largest_acknowledged;
    if (!ack.ranges.empty())
    {
        current_pn -= ack.ranges[0].length + 1;  // After first range
    }

    for (size_t r = 1; r < ack.ranges.size(); ++r)
    {
        // Skip the gap
        auto gap = ack.ranges[r].gap;
        if (current_pn >= gap + 2)
        {
            current_pn -= gap + 2;
        }
        else
        {
            break;
        }

        // Process this range
        auto range_len = ack.ranges[r].length;
        for (uint64_t i = 0; i <= range_len && current_pn > 0; ++i)
        {
            auto it = space.sent_packets.find(current_pn - i);
            if (it != space.sent_packets.end())
            {
                auto& pkt = it->second;
                if (pkt.in_flight)
                {
                    space.bytes_in_flight -= pkt.sent_bytes;
                }
                result.acked_packets.push_back(std::move(pkt));
                space.sent_packets.erase(it);
            }
        }
        if (current_pn >= range_len + 1)
        {
            current_pn -= range_len + 1;
        }
        else
        {
            break;
        }
    }

    // Update RTT if we got a sample from the largest acknowledged packet
    if (!result.acked_packets.empty())
    {
        // Find the largest acked packet to compute RTT
        auto largest_it = std::find_if(
            result.acked_packets.begin(),
            result.acked_packets.end(),
            [largest_newly_acked](const sent_packet& p) {
                return p.packet_number == largest_newly_acked;
            });

        if (largest_it != result.acked_packets.end())
        {
            auto latest_rtt = std::chrono::duration_cast<std::chrono::microseconds>(
                recv_time - largest_it->sent_time);
            auto ack_delay_us = std::chrono::microseconds{
                static_cast<int64_t>(ack.ack_delay)};  // Encoded value
            rtt_.update(latest_rtt, ack_delay_us, handshake_confirmed_);
        }

        // Reset PTO count since we got an ACK
        pto_count_ = 0;
    }

    // Detect lost packets
    auto lost = detect_lost_packets(level, recv_time);
    if (!lost.empty())
    {
        result.event = loss_detection_event::packet_lost;
        result.lost_packets = std::move(lost);
    }

    // Process ECN counts if present (RFC 9000 Section 13.4, RFC 9002 Section 7.1)
    if (ack.ecn && !result.acked_packets.empty())
    {
        // Find the earliest sent time among acked packets for congestion recovery tracking
        auto earliest_sent_time = result.acked_packets[0].sent_time;
        for (const auto& pkt : result.acked_packets)
        {
            if (pkt.sent_time < earliest_sent_time)
            {
                earliest_sent_time = pkt.sent_time;
            }
        }

        auto ecn_signal = ecn_tracker_.process_ecn_counts(
            *ack.ecn,
            result.acked_packets.size(),
            earliest_sent_time);

        result.ecn_signal = ecn_signal;
        if (ecn_signal == ecn_result::congestion_signal)
        {
            result.ecn_congestion_sent_time = ecn_tracker_.last_congestion_sent_time();
        }
    }

    set_loss_detection_timer();

    return result;
}

auto loss_detector::detect_lost_packets(encryption_level level,
                                         std::chrono::steady_clock::time_point now)
    -> std::vector<sent_packet>
{
    auto idx = space_index(level);
    auto& space = spaces_[idx];
    std::vector<sent_packet> lost;

    if (!space.largest_acked_set)
    {
        return lost;
    }

    // Calculate loss delay (RFC 9002 Section 6.1.2)
    auto smoothed = rtt_.smoothed_rtt();
    auto min_rtt = rtt_.min_rtt();
    if (min_rtt == std::chrono::microseconds::max())
    {
        min_rtt = smoothed;
    }
    auto max_rtt = std::max(smoothed, min_rtt);
    auto loss_delay_us = static_cast<int64_t>(
        kTimeThreshold * static_cast<double>(max_rtt.count()));
    auto granularity_us = std::chrono::duration_cast<std::chrono::microseconds>(
        kGranularity).count();
    auto loss_delay = std::chrono::microseconds{
        std::max(loss_delay_us, granularity_us)};

    auto lost_send_time = now - loss_delay;

    // Reset loss time for this space
    space.loss_time = std::chrono::steady_clock::time_point{};

    for (auto it = space.sent_packets.begin(); it != space.sent_packets.end();)
    {
        auto& [pn, packet] = *it;

        if (pn > space.largest_acked)
        {
            ++it;
            continue;
        }

        // Check if packet is lost by packet threshold or time threshold
        bool time_lost = packet.sent_time <= lost_send_time;
        bool reorder_lost = (space.largest_acked >= pn + kPacketThreshold);

        if (time_lost || reorder_lost)
        {
            if (packet.in_flight)
            {
                space.bytes_in_flight -= packet.sent_bytes;
            }
            lost.push_back(std::move(packet));
            it = space.sent_packets.erase(it);
        }
        else
        {
            // This packet might be lost later by time threshold
            auto potential_loss_time = packet.sent_time + loss_delay;
            if (space.loss_time == std::chrono::steady_clock::time_point{} ||
                potential_loss_time < space.loss_time)
            {
                space.loss_time = potential_loss_time;
            }
            ++it;
        }
    }

    return lost;
}

auto loss_detector::next_timeout() const
    -> std::optional<std::chrono::steady_clock::time_point>
{
    if (!timer_armed_)
    {
        return std::nullopt;
    }
    return loss_detection_timer_;
}

auto loss_detector::on_timeout() -> loss_detection_result
{
    loss_detection_result result;
    auto now = std::chrono::steady_clock::now();

    auto [loss_time, loss_level] = get_loss_time_and_space();
    if (loss_time != std::chrono::steady_clock::time_point{} && loss_time <= now)
    {
        // Time threshold loss detection
        auto lost = detect_lost_packets(loss_level, now);
        if (!lost.empty())
        {
            result.event = loss_detection_event::packet_lost;
            result.lost_packets = std::move(lost);
        }
    }
    else
    {
        // PTO timeout
        result.event = loss_detection_event::pto_expired;
        ++pto_count_;
    }

    set_loss_detection_timer();

    return result;
}

auto loss_detector::get_loss_time_and_space() const
    -> std::pair<std::chrono::steady_clock::time_point, encryption_level>
{
    auto earliest_loss_time = std::chrono::steady_clock::time_point::max();
    auto earliest_level = encryption_level::initial;

    for (size_t i = 0; i < spaces_.size(); ++i)
    {
        auto& space = spaces_[i];
        if (space.loss_time != std::chrono::steady_clock::time_point{} &&
            space.loss_time < earliest_loss_time)
        {
            earliest_loss_time = space.loss_time;
            switch (i)
            {
            case 0:
                earliest_level = encryption_level::initial;
                break;
            case 1:
                earliest_level = encryption_level::handshake;
                break;
            case 2:
                earliest_level = encryption_level::application;
                break;
            }
        }
    }

    if (earliest_loss_time == std::chrono::steady_clock::time_point::max())
    {
        earliest_loss_time = std::chrono::steady_clock::time_point{};
    }

    return {earliest_loss_time, earliest_level};
}

auto loss_detector::get_pto_time_and_space() const
    -> std::pair<std::chrono::steady_clock::time_point, encryption_level>
{
    auto pto_duration = rtt_.pto() * (1 << pto_count_);  // Exponential backoff
    auto earliest_pto_time = std::chrono::steady_clock::time_point::max();
    auto earliest_level = encryption_level::application;

    for (size_t i = 0; i < spaces_.size(); ++i)
    {
        auto& space = spaces_[i];

        // Skip spaces with no ack-eliciting packets
        bool has_ack_eliciting = false;
        for (const auto& [pn, pkt] : space.sent_packets)
        {
            if (pkt.ack_eliciting)
            {
                has_ack_eliciting = true;
                break;
            }
        }

        if (!has_ack_eliciting)
        {
            continue;
        }

        // Don't include application space if handshake not confirmed
        if (i == 2 && !handshake_confirmed_)
        {
            continue;
        }

        auto pto_time = space.time_of_last_ack_eliciting + pto_duration;
        if (pto_time < earliest_pto_time)
        {
            earliest_pto_time = pto_time;
            switch (i)
            {
            case 0:
                earliest_level = encryption_level::initial;
                break;
            case 1:
                earliest_level = encryption_level::handshake;
                break;
            case 2:
                earliest_level = encryption_level::application;
                break;
            }
        }
    }

    if (earliest_pto_time == std::chrono::steady_clock::time_point::max())
    {
        earliest_pto_time = std::chrono::steady_clock::time_point{};
    }

    return {earliest_pto_time, earliest_level};
}

auto loss_detector::set_loss_detection_timer() -> void
{
    auto [loss_time, loss_level] = get_loss_time_and_space();

    if (loss_time != std::chrono::steady_clock::time_point{})
    {
        // Time threshold loss detection timer
        loss_detection_timer_ = loss_time;
        timer_armed_ = true;
        return;
    }

    // Check if there are any ack-eliciting packets in flight
    bool any_ack_eliciting = false;
    for (const auto& space : spaces_)
    {
        for (const auto& [pn, pkt] : space.sent_packets)
        {
            if (pkt.ack_eliciting)
            {
                any_ack_eliciting = true;
                break;
            }
        }
        if (any_ack_eliciting)
        {
            break;
        }
    }

    if (!any_ack_eliciting)
    {
        timer_armed_ = false;
        return;
    }

    auto [pto_time, pto_level] = get_pto_time_and_space();
    if (pto_time != std::chrono::steady_clock::time_point{})
    {
        loss_detection_timer_ = pto_time;
        timer_armed_ = true;
    }
    else
    {
        timer_armed_ = false;
    }
}

auto loss_detector::largest_acked(encryption_level level) const noexcept -> uint64_t
{
    auto idx = space_index(level);
    return spaces_[idx].largest_acked;
}

auto loss_detector::has_unacked_packets(encryption_level level) const -> bool
{
    auto idx = space_index(level);
    return !spaces_[idx].sent_packets.empty();
}

auto loss_detector::bytes_in_flight(encryption_level level) const -> size_t
{
    auto idx = space_index(level);
    return spaces_[idx].bytes_in_flight;
}

auto loss_detector::total_bytes_in_flight() const -> size_t
{
    size_t total = 0;
    for (const auto& space : spaces_)
    {
        total += space.bytes_in_flight;
    }
    return total;
}

auto loss_detector::discard_space(encryption_level level) -> void
{
    auto idx = space_index(level);
    spaces_[idx] = space_state{};
    set_loss_detection_timer();
}

} // namespace kcenon::network::protocols::quic
