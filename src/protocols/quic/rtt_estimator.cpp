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

#include "kcenon/network/protocols/quic/rtt_estimator.h"

#include <algorithm>
#include <cmath>

namespace kcenon::network::protocols::quic
{

rtt_estimator::rtt_estimator()
    : smoothed_rtt_(kInitialRtt)
    , rttvar_(kInitialRtt / 2)
    , min_rtt_(std::chrono::microseconds::max())
    , latest_rtt_(std::chrono::microseconds{0})
    , max_ack_delay_(kDefaultMaxAckDelay)
    , initial_rtt_(kInitialRtt)
    , first_sample_(true)
{
}

rtt_estimator::rtt_estimator(std::chrono::microseconds initial_rtt,
                             std::chrono::microseconds max_ack_delay)
    : smoothed_rtt_(initial_rtt)
    , rttvar_(initial_rtt / 2)
    , min_rtt_(std::chrono::microseconds::max())
    , latest_rtt_(std::chrono::microseconds{0})
    , max_ack_delay_(max_ack_delay)
    , initial_rtt_(initial_rtt)
    , first_sample_(true)
{
}

auto rtt_estimator::update(std::chrono::microseconds latest_rtt,
                           std::chrono::microseconds ack_delay,
                           bool is_handshake_confirmed) -> void
{
    latest_rtt_ = latest_rtt;

    // Update min_rtt (RFC 9002 Section 5.2)
    if (latest_rtt < min_rtt_)
    {
        min_rtt_ = latest_rtt;
    }

    // Adjust RTT sample by subtracting ACK delay (RFC 9002 Section 5.3)
    auto adjusted_rtt = latest_rtt;
    if (is_handshake_confirmed)
    {
        // Only subtract ACK delay for handshake-confirmed samples
        // and only up to max_ack_delay
        auto effective_ack_delay = std::min(ack_delay, max_ack_delay_);

        // Don't adjust RTT below min_rtt
        if (adjusted_rtt > min_rtt_ + effective_ack_delay)
        {
            adjusted_rtt -= effective_ack_delay;
        }
        else if (adjusted_rtt > min_rtt_)
        {
            adjusted_rtt = min_rtt_;
        }
    }

    if (first_sample_)
    {
        // First RTT sample (RFC 9002 Section 5.3)
        smoothed_rtt_ = adjusted_rtt;
        rttvar_ = adjusted_rtt / 2;
        first_sample_ = false;
    }
    else
    {
        // Subsequent RTT samples (RFC 9002 Section 5.3)
        // rttvar = 3/4 * rttvar + 1/4 * |smoothed_rtt - adjusted_rtt|
        auto rtt_diff = smoothed_rtt_ > adjusted_rtt
                            ? smoothed_rtt_ - adjusted_rtt
                            : adjusted_rtt - smoothed_rtt_;

        rttvar_ = std::chrono::microseconds{
            (3 * rttvar_.count() + rtt_diff.count()) / 4};

        // smoothed_rtt = 7/8 * smoothed_rtt + 1/8 * adjusted_rtt
        smoothed_rtt_ = std::chrono::microseconds{
            (7 * smoothed_rtt_.count() + adjusted_rtt.count()) / 8};
    }
}

auto rtt_estimator::pto() const noexcept -> std::chrono::microseconds
{
    // PTO = smoothed_rtt + max(4*rttvar, kGranularity) + max_ack_delay
    // (RFC 9002 Section 6.2.1)
    auto granularity_us = std::chrono::duration_cast<std::chrono::microseconds>(kGranularity);
    auto four_rttvar = std::chrono::microseconds{4 * rttvar_.count()};
    auto pto_value = smoothed_rtt_ + std::max(four_rttvar, granularity_us) + max_ack_delay_;

    return pto_value;
}

auto rtt_estimator::reset() -> void
{
    smoothed_rtt_ = initial_rtt_;
    rttvar_ = initial_rtt_ / 2;
    min_rtt_ = std::chrono::microseconds::max();
    latest_rtt_ = std::chrono::microseconds{0};
    first_sample_ = true;
}

} // namespace kcenon::network::protocols::quic
