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

#include "internal/protocols/quic/flow_control.h"

#include <algorithm>

namespace kcenon::network::protocols::quic
{

flow_controller::flow_controller(uint64_t initial_window)
    : send_limit_{initial_window}
    , receive_limit_{initial_window}
    , window_size_{initial_window}
    , last_sent_max_data_{initial_window}
{
}

// ============================================================================
// Send Side
// ============================================================================

auto flow_controller::available_send_window() const noexcept -> uint64_t
{
    if (bytes_sent_ >= send_limit_) {
        return 0;
    }
    return send_limit_ - bytes_sent_;
}

auto flow_controller::consume_send_window(uint64_t bytes) -> VoidResult
{
    if (bytes == 0) {
        return ok();
    }

    const uint64_t available = available_send_window();
    if (bytes > available) {
        return error_void(flow_control_error::send_blocked,
                         "Send window exhausted",
                         "quic::flow_controller");
    }

    bytes_sent_ += bytes;
    data_blocked_sent_ = false; // Reset blocked flag on successful send
    return ok();
}

void flow_controller::update_send_limit(uint64_t max_data)
{
    // Only update if new limit is higher (MAX_DATA can only increase)
    if (max_data > send_limit_) {
        send_limit_ = max_data;
        data_blocked_sent_ = false; // Can send again, reset blocked flag
    }
}

auto flow_controller::is_send_blocked() const noexcept -> bool
{
    return bytes_sent_ >= send_limit_;
}

// ============================================================================
// Receive Side
// ============================================================================

auto flow_controller::record_received(uint64_t bytes) -> VoidResult
{
    if (bytes == 0) {
        return ok();
    }

    const uint64_t new_total = bytes_received_ + bytes;
    if (new_total > receive_limit_) {
        return error_void(flow_control_error::receive_overflow,
                         "Received data exceeds flow control limit",
                         "quic::flow_controller");
    }

    bytes_received_ = new_total;
    return ok();
}

void flow_controller::record_consumed(uint64_t bytes)
{
    bytes_consumed_ += bytes;
    // Consumed cannot exceed received
    if (bytes_consumed_ > bytes_received_) {
        bytes_consumed_ = bytes_received_;
    }
}

// ============================================================================
// Flow Control Frame Generation
// ============================================================================

auto flow_controller::should_send_max_data() const noexcept -> bool
{
    // Send MAX_DATA when we've consumed enough of the window
    const uint64_t threshold = static_cast<uint64_t>(window_size_ * update_threshold_);

    // Calculate how much of the advertised window has been consumed
    const uint64_t consumed_from_last = bytes_consumed_ - (last_sent_max_data_ - window_size_);

    return consumed_from_last >= threshold;
}

auto flow_controller::generate_max_data() -> std::optional<uint64_t>
{
    if (!should_send_max_data()) {
        return std::nullopt;
    }

    // New limit is current consumption point plus window size
    const uint64_t new_limit = bytes_consumed_ + window_size_;

    // Only send if it's an increase
    if (new_limit <= last_sent_max_data_) {
        return std::nullopt;
    }

    receive_limit_ = new_limit;
    last_sent_max_data_ = new_limit;
    return new_limit;
}

auto flow_controller::should_send_data_blocked() const noexcept -> bool
{
    return is_send_blocked() && !data_blocked_sent_;
}

void flow_controller::mark_data_blocked_sent()
{
    data_blocked_sent_ = true;
}

// ============================================================================
// Configuration
// ============================================================================

void flow_controller::set_window_size(uint64_t window)
{
    window_size_ = window;
}

void flow_controller::set_update_threshold(double threshold)
{
    update_threshold_ = std::clamp(threshold, 0.0, 1.0);
}

// ============================================================================
// Reset
// ============================================================================

void flow_controller::reset(uint64_t initial_window)
{
    send_limit_ = initial_window;
    bytes_sent_ = 0;
    data_blocked_sent_ = false;

    receive_limit_ = initial_window;
    bytes_received_ = 0;
    bytes_consumed_ = 0;

    window_size_ = initial_window;
    last_sent_max_data_ = initial_window;
}

// ============================================================================
// Statistics
// ============================================================================

auto get_flow_control_stats(const flow_controller& fc) -> flow_control_stats
{
    flow_control_stats stats;

    // Send side
    stats.send_limit = fc.send_limit();
    stats.bytes_sent = fc.bytes_sent();
    stats.send_window_available = fc.available_send_window();
    stats.send_blocked = fc.is_send_blocked();

    // Receive side
    stats.receive_limit = fc.receive_limit();
    stats.bytes_received = fc.bytes_received();
    stats.bytes_consumed = fc.bytes_consumed();
    stats.receive_window_available = fc.receive_limit() - fc.bytes_received();

    return stats;
}

} // namespace kcenon::network::protocols::quic
