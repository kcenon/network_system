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

#include "internal/protocols/quic/pmtud_controller.h"

#include <algorithm>

namespace kcenon::network::protocols::quic
{

auto pmtud_state_to_string(pmtud_state state) noexcept -> const char*
{
    switch (state)
    {
    case pmtud_state::disabled:
        return "disabled";
    case pmtud_state::base:
        return "base";
    case pmtud_state::searching:
        return "searching";
    case pmtud_state::search_complete:
        return "search_complete";
    case pmtud_state::error:
        return "error";
    }
    return "unknown";
}

pmtud_controller::pmtud_controller()
    : pmtud_controller(pmtud_config{})
{
}

pmtud_controller::pmtud_controller(pmtud_config config)
    : config_(config)
    , current_mtu_(config_.min_mtu)
    , search_low_(config_.min_mtu)
    , search_high_(config_.max_probe_mtu)
{
}

void pmtud_controller::enable()
{
    if (state_ == pmtud_state::disabled)
    {
        state_ = pmtud_state::base;
        current_mtu_ = config_.min_mtu;
        search_low_ = config_.min_mtu;
        search_high_ = config_.max_probe_mtu;
        probe_count_ = 0;
        consecutive_failures_ = 0;
        probe_in_flight_ = false;
        start_search();
    }
}

void pmtud_controller::disable()
{
    state_ = pmtud_state::disabled;
    current_mtu_ = config_.min_mtu;
    probing_mtu_ = 0;
    probe_in_flight_ = false;
}

void pmtud_controller::reset()
{
    state_ = pmtud_state::disabled;
    current_mtu_ = config_.min_mtu;
    search_low_ = config_.min_mtu;
    search_high_ = config_.max_probe_mtu;
    probing_mtu_ = 0;
    probe_count_ = 0;
    consecutive_failures_ = 0;
    probe_in_flight_ = false;
}

auto pmtud_controller::should_probe(
    std::chrono::steady_clock::time_point now) const noexcept -> bool
{
    // Don't probe if disabled or probe already in flight
    if (state_ == pmtud_state::disabled || probe_in_flight_)
    {
        return false;
    }

    // In base state, always ready to start probing
    if (state_ == pmtud_state::base)
    {
        return true;
    }

    // In searching state, check if enough time has passed
    if (state_ == pmtud_state::searching)
    {
        auto elapsed = now - last_probe_time_;
        return elapsed >= config_.probe_interval;
    }

    // In search_complete state, check if re-validation is needed
    if (state_ == pmtud_state::search_complete)
    {
        auto elapsed = now - search_complete_time_;
        return elapsed >= config_.confirmation_interval;
    }

    // In error state, check if enough time for recovery
    if (state_ == pmtud_state::error)
    {
        auto elapsed = now - last_probe_time_;
        return elapsed >= config_.probe_timeout;
    }

    return false;
}

auto pmtud_controller::probe_size() const noexcept -> std::optional<size_t>
{
    if (state_ == pmtud_state::disabled)
    {
        return std::nullopt;
    }

    if (state_ == pmtud_state::base || state_ == pmtud_state::searching)
    {
        return probing_mtu_ > 0 ? std::optional<size_t>(probing_mtu_) : std::nullopt;
    }

    // For re-validation in search_complete state
    if (state_ == pmtud_state::search_complete)
    {
        return current_mtu_;
    }

    return std::nullopt;
}

void pmtud_controller::on_probe_sent(
    size_t size, std::chrono::steady_clock::time_point sent_time)
{
    probing_mtu_ = size;
    last_probe_time_ = sent_time;
    probe_in_flight_ = true;
    ++probe_count_;
}

void pmtud_controller::on_probe_acked(size_t size)
{
    probe_in_flight_ = false;
    consecutive_failures_ = 0;

    // Handle based on current state
    if (state_ == pmtud_state::base || state_ == pmtud_state::searching)
    {
        // Successful probe - update validated MTU
        if (size > current_mtu_)
        {
            current_mtu_ = size;
            search_low_ = size;
        }

        // Check if we've found the maximum
        if (search_high_ - search_low_ <= config_.probe_step)
        {
            complete_search();
        }
        else
        {
            // Continue searching
            state_ = pmtud_state::searching;
            probe_count_ = 0;
            probing_mtu_ = calculate_next_probe_size();
        }
    }
    else if (state_ == pmtud_state::search_complete)
    {
        // Re-validation successful
        search_complete_time_ = std::chrono::steady_clock::now();
    }
    else if (state_ == pmtud_state::error)
    {
        // Recovery successful
        state_ = pmtud_state::searching;
        probe_count_ = 0;
        probing_mtu_ = calculate_next_probe_size();
    }
}

void pmtud_controller::on_probe_lost(size_t size)
{
    probe_in_flight_ = false;
    ++consecutive_failures_;

    // Check for black hole
    if (consecutive_failures_ >= kBlackHoleThreshold)
    {
        handle_black_hole();
        return;
    }

    if (state_ == pmtud_state::base || state_ == pmtud_state::searching)
    {
        if (probe_count_ >= config_.max_probes)
        {
            // Too many failures at this size - reduce search range
            search_high_ = size;
            probe_count_ = 0;

            // Check if we've converged
            if (search_high_ - search_low_ <= config_.probe_step)
            {
                complete_search();
            }
            else
            {
                probing_mtu_ = calculate_next_probe_size();
            }
        }
        // Otherwise, will retry the same size on next should_probe()
    }
    else if (state_ == pmtud_state::search_complete)
    {
        // Re-validation failed - MTU may have decreased
        // Fall back to error state and restart search
        state_ = pmtud_state::error;
        search_high_ = current_mtu_;
        search_low_ = config_.min_mtu;
        current_mtu_ = config_.min_mtu;
        probe_count_ = 0;
    }
}

void pmtud_controller::on_packet_too_big(size_t reported_mtu)
{
    // RFC 8899: ICMP PTB should trigger immediate MTU reduction
    if (reported_mtu >= config_.min_mtu && reported_mtu < current_mtu_)
    {
        current_mtu_ = reported_mtu;
        search_high_ = reported_mtu;

        if (state_ == pmtud_state::search_complete)
        {
            // Need to restart search with new upper bound
            state_ = pmtud_state::searching;
            probe_count_ = 0;
            probing_mtu_ = calculate_next_probe_size();
        }
    }
    else if (reported_mtu < config_.min_mtu)
    {
        // PTB reports MTU below QUIC minimum - possible black hole
        handle_black_hole();
    }
}

auto pmtud_controller::next_timeout() const noexcept
    -> std::optional<std::chrono::steady_clock::time_point>
{
    if (state_ == pmtud_state::disabled)
    {
        return std::nullopt;
    }

    if (probe_in_flight_)
    {
        // Timeout for probe in flight
        return last_probe_time_ + config_.probe_timeout;
    }

    if (state_ == pmtud_state::searching || state_ == pmtud_state::base)
    {
        return last_probe_time_ + config_.probe_interval;
    }

    if (state_ == pmtud_state::search_complete)
    {
        return search_complete_time_ + config_.confirmation_interval;
    }

    if (state_ == pmtud_state::error)
    {
        return last_probe_time_ + config_.probe_timeout;
    }

    return std::nullopt;
}

void pmtud_controller::on_timeout(std::chrono::steady_clock::time_point now)
{
    if (state_ == pmtud_state::disabled)
    {
        return;
    }

    if (probe_in_flight_)
    {
        // Probe timeout - treat as loss
        auto elapsed = now - last_probe_time_;
        if (elapsed >= config_.probe_timeout)
        {
            on_probe_lost(probing_mtu_);
        }
    }
}

void pmtud_controller::start_search()
{
    state_ = pmtud_state::searching;
    search_low_ = current_mtu_;
    search_high_ = config_.max_probe_mtu;
    probe_count_ = 0;
    probing_mtu_ = calculate_next_probe_size();
}

auto pmtud_controller::calculate_next_probe_size() const noexcept -> size_t
{
    // Binary search - probe the midpoint
    size_t mid = search_low_ + (search_high_ - search_low_) / 2;

    // Round up to ensure we make progress
    if (mid == search_low_ && search_high_ > search_low_)
    {
        mid = search_low_ + config_.probe_step;
    }

    return std::min(mid, search_high_);
}

void pmtud_controller::complete_search()
{
    state_ = pmtud_state::search_complete;
    search_complete_time_ = std::chrono::steady_clock::now();
    probing_mtu_ = 0;
    probe_count_ = 0;
}

void pmtud_controller::handle_black_hole()
{
    // Black hole detected - reset to minimum MTU
    state_ = pmtud_state::error;
    current_mtu_ = config_.min_mtu;
    search_low_ = config_.min_mtu;
    search_high_ = config_.max_probe_mtu;
    probing_mtu_ = 0;
    probe_count_ = 0;
    consecutive_failures_ = 0;
    probe_in_flight_ = false;
}

} // namespace kcenon::network::protocols::quic
