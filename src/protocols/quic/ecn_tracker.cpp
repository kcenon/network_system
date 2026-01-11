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

#include "kcenon/network/protocols/quic/ecn_tracker.h"

namespace kcenon::network::protocols::quic
{

auto ecn_result_to_string(ecn_result result) noexcept -> const char*
{
    switch (result)
    {
    case ecn_result::none:
        return "none";
    case ecn_result::congestion_signal:
        return "congestion_signal";
    case ecn_result::ecn_failure:
        return "ecn_failure";
    default:
        return "unknown";
    }
}

ecn_tracker::ecn_tracker()
    : state_{}
{
    // Start in testing mode
    state_.testing = true;
    state_.capable = false;
    state_.failed = false;
}

auto ecn_tracker::process_ecn_counts(const ecn_counts& counts,
                                     uint64_t packets_acked,
                                     std::chrono::steady_clock::time_point sent_time)
    -> ecn_result
{
    // If ECN has already failed, don't process further
    if (state_.failed)
    {
        return ecn_result::ecn_failure;
    }

    // RFC 9000 Section 13.4.2.1: Verify ECN counts are valid
    // ECN counts must be monotonically increasing
    if (counts.ect0 < state_.counts.ect0 ||
        counts.ect1 < state_.counts.ect1 ||
        counts.ecn_ce < state_.counts.ecn_ce)
    {
        // ECN counts decreased - this is invalid, disable ECN
        state_.failed = true;
        state_.testing = false;
        state_.capable = false;
        return ecn_result::ecn_failure;
    }

    // Validate ECN if still in testing phase
    if (state_.testing)
    {
        if (!validate_ecn(counts, packets_acked))
        {
            state_.failed = true;
            state_.testing = false;
            state_.capable = false;
            return ecn_result::ecn_failure;
        }

        // Check if we have enough data to complete validation
        uint64_t total_marks = counts.ect0 + counts.ect1 + counts.ecn_ce;
        if (total_marks >= validation_state::kValidationThreshold)
        {
            state_.testing = false;
            state_.capable = true;
        }
    }

    // Check for ECN-CE increase (congestion signal)
    // RFC 9002 Section 7.1: ECN-CE marks indicate congestion
    ecn_result result = ecn_result::none;
    if (counts.ecn_ce > state_.counts.ecn_ce)
    {
        // ECN-CE count increased - congestion experienced
        result = ecn_result::congestion_signal;
        state_.last_congestion_sent_time = sent_time;
    }

    // Update stored counts
    state_.counts = counts;

    return result;
}

auto ecn_tracker::validate_ecn(const ecn_counts& counts,
                               uint64_t packets_acked) -> bool
{
    // RFC 9000 Section 13.4.2.1:
    // The total increase in ECT(0), ECT(1), and ECN-CE counts MUST be
    // at least the number of newly acknowledged packets that were
    // originally sent with an ECT codepoint.

    uint64_t total_increase =
        (counts.ect0 - state_.counts.ect0) +
        (counts.ect1 - state_.counts.ect1) +
        (counts.ecn_ce - state_.counts.ecn_ce);

    // We sent packets with ECT(0) marking, so we expect the ECN counts
    // to reflect at least those packets
    if (total_increase < packets_acked && state_.packets_sent_with_ect > 0)
    {
        // ECN counts don't account for all packets - ECN is being stripped
        // by the network path, or the peer doesn't support ECN properly
        return false;
    }

    return true;
}

auto ecn_tracker::on_packets_sent(uint64_t packet_count) noexcept -> void
{
    if (!state_.failed)
    {
        state_.packets_sent_with_ect += packet_count;
    }
}

auto ecn_tracker::reset() noexcept -> void
{
    state_ = validation_state{};
    state_.testing = true;
}

auto ecn_tracker::disable() noexcept -> void
{
    state_.failed = true;
    state_.testing = false;
    state_.capable = false;
}

} // namespace kcenon::network::protocols::quic
