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

#include "kcenon/network/protocols/quic/stream.h"

#include <algorithm>
#include <cstring>

namespace kcenon::network::protocols::quic
{

stream::stream(uint64_t id, bool is_local, uint64_t initial_max_data)
    : id_{id}
    , is_local_{is_local}
    , max_send_offset_{initial_max_data}
    , max_recv_offset_{initial_max_data}
    , recv_window_size_{initial_max_data}
{
}

// ============================================================================
// Send Side
// ============================================================================

auto stream::can_send() const noexcept -> bool
{
    if (is_unidirectional() && !is_local_) {
        // Cannot send on peer-initiated unidirectional stream
        return false;
    }

    switch (send_state_) {
        case send_stream_state::ready:
        case send_stream_state::send:
            return true;
        default:
            return false;
    }
}

auto stream::write(std::span<const uint8_t> data) -> Result<size_t>
{
    if (!can_send()) {
        return error<size_t>(stream_error::stream_state_error,
                            "Stream cannot send data in current state",
                            "quic::stream");
    }

    if (fin_sent_) {
        return error<size_t>(stream_error::stream_state_error,
                            "Cannot write after FIN sent",
                            "quic::stream");
    }

    // Check flow control
    const auto available = available_send_window();
    if (available == 0) {
        return error<size_t>(stream_error::flow_control_error,
                            "Send window exhausted",
                            "quic::stream");
    }

    // Write up to available window
    size_t to_write = std::min(data.size(), available);
    send_buffer_.insert(send_buffer_.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(to_write));

    // Update state
    if (send_state_ == send_stream_state::ready && !send_buffer_.empty()) {
        send_state_ = send_stream_state::send;
    }

    return ok(std::move(to_write));
}

auto stream::finish() -> VoidResult
{
    if (!can_send()) {
        return error_void(stream_error::stream_state_error,
                         "Stream cannot be finished in current state",
                         "quic::stream");
    }

    if (fin_sent_) {
        return error_void(stream_error::stream_state_error,
                         "FIN already sent",
                         "quic::stream");
    }

    fin_sent_ = true;
    update_send_state();
    return ok();
}

auto stream::reset(uint64_t error_code) -> VoidResult
{
    if (send_state_ == send_stream_state::reset_sent ||
        send_state_ == send_stream_state::reset_recvd ||
        send_state_ == send_stream_state::data_recvd) {
        return error_void(stream_error::stream_state_error,
                         "Stream already in terminal state",
                         "quic::stream");
    }

    reset_error_code_ = error_code;
    send_state_ = send_stream_state::reset_sent;
    send_buffer_.clear();
    return ok();
}

auto stream::next_stream_frame(size_t max_size) -> std::optional<stream_frame>
{
    if (send_buffer_.empty() && !fin_sent_) {
        return std::nullopt;
    }

    // Check if we have pending data or need to send FIN
    if (send_buffer_.empty() && fin_sent_ && !fin_acked_) {
        // Send FIN-only frame
        stream_frame frame;
        frame.stream_id = id_;
        frame.offset = send_offset_;
        frame.fin = true;
        return frame;
    }

    if (send_buffer_.empty()) {
        return std::nullopt;
    }

    // Calculate how much we can send (considering flow control)
    const auto window_available = available_send_window();
    if (window_available == 0) {
        return std::nullopt;
    }

    // Reserve space for frame header (stream_id + offset + length as varints)
    // Maximum varint size is 8 bytes each, plus 1 byte for frame type
    constexpr size_t max_header_size = 1 + 8 + 8 + 8;
    if (max_size <= max_header_size) {
        return std::nullopt;
    }

    const size_t max_payload = max_size - max_header_size;
    const size_t to_send = std::min({send_buffer_.size(), window_available, max_payload});

    stream_frame frame;
    frame.stream_id = id_;
    frame.offset = send_offset_;
    frame.data.reserve(to_send);

    // Copy data from buffer
    for (size_t i = 0; i < to_send; ++i) {
        frame.data.push_back(send_buffer_[i]);
    }

    // Remove sent data from buffer
    send_buffer_.erase(send_buffer_.begin(), send_buffer_.begin() + to_send);
    send_offset_ += to_send;

    // Set FIN if this is the last data and FIN was requested
    if (fin_sent_ && send_buffer_.empty()) {
        frame.fin = true;
    }

    return frame;
}

void stream::acknowledge_data(uint64_t offset, uint64_t length)
{
    // Track acknowledgments
    if (offset + length > acked_offset_) {
        // Simple implementation: assume in-order ACKs
        acked_offset_ = offset + length;
    }

    // Check if FIN is acknowledged
    if (fin_sent_ && send_buffer_.empty() && acked_offset_ >= send_offset_) {
        fin_acked_ = true;
    }

    update_send_state();
}

// ============================================================================
// Receive Side
// ============================================================================

auto stream::has_data() const noexcept -> bool
{
    return !recv_ready_.empty();
}

auto stream::read(std::span<uint8_t> buffer) -> Result<size_t>
{
    if (is_unidirectional() && is_local_) {
        // Cannot receive on locally-initiated unidirectional stream
        return error<size_t>(stream_error::stream_state_error,
                            "Cannot read from local unidirectional stream",
                            "quic::stream");
    }

    if (recv_state_ == recv_stream_state::reset_recvd) {
        return error<size_t>(stream_error::stream_reset,
                            "Stream was reset by peer",
                            "quic::stream");
    }

    if (recv_ready_.empty()) {
        if (recv_fin_ && recv_buffer_.empty()) {
            // All data has been read and FIN was received
            recv_state_ = recv_stream_state::data_read;
            return ok(static_cast<size_t>(0));
        }
        return ok(static_cast<size_t>(0));
    }

    size_t to_read = std::min(buffer.size(), recv_ready_.size());
    for (size_t i = 0; i < to_read; ++i) {
        buffer[i] = recv_ready_[i];
    }

    recv_ready_.erase(recv_ready_.begin(), recv_ready_.begin() + static_cast<std::ptrdiff_t>(to_read));

    // Update receive state
    update_recv_state();

    return ok(std::move(to_read));
}

auto stream::stop_sending(uint64_t error_code) -> VoidResult
{
    if (is_unidirectional() && is_local_) {
        return error_void(stream_error::stream_state_error,
                         "Cannot stop sending on local unidirectional stream",
                         "quic::stream");
    }

    stop_sending_error_code_ = error_code;
    return ok();
}

auto stream::receive_data(uint64_t offset, std::span<const uint8_t> data, bool fin)
    -> VoidResult
{
    if (is_unidirectional() && is_local_) {
        return error_void(stream_error::stream_state_error,
                         "Cannot receive on local unidirectional stream",
                         "quic::stream");
    }

    if (recv_state_ == recv_stream_state::reset_recvd ||
        recv_state_ == recv_stream_state::reset_read) {
        return error_void(stream_error::stream_state_error,
                         "Stream was reset",
                         "quic::stream");
    }

    // Check for final size violations
    if (final_size_.has_value()) {
        if (offset + data.size() > *final_size_) {
            return error_void(stream_error::final_size_error,
                             "Data exceeds final size",
                             "quic::stream");
        }
    }

    if (fin) {
        const uint64_t new_final_size = offset + data.size();
        if (final_size_.has_value() && *final_size_ != new_final_size) {
            return error_void(stream_error::final_size_error,
                             "Final size changed",
                             "quic::stream");
        }
        final_size_ = new_final_size;
        recv_fin_ = true;
    }

    // Check flow control
    if (offset + data.size() > max_recv_offset_) {
        return error_void(stream_error::flow_control_error,
                         "Received data exceeds flow control limit",
                         "quic::stream");
    }

    // Handle data placement
    if (offset < recv_offset_) {
        // Overlapping retransmission
        const size_t overlap = static_cast<size_t>(recv_offset_ - offset);
        if (overlap >= data.size()) {
            // Entirely duplicate, ignore
            update_recv_state();
            return ok();
        }
        // Trim leading overlap
        offset = recv_offset_;
        data = data.subspan(overlap);
    }

    if (offset == recv_offset_) {
        // Contiguous data - append directly
        recv_ready_.insert(recv_ready_.end(), data.begin(), data.end());
        recv_offset_ += data.size();

        // Try to reassemble any buffered data
        reassemble_data();
    } else {
        // Gap in data - buffer for later
        recv_buffer_[offset] = std::vector<uint8_t>(data.begin(), data.end());
    }

    update_recv_state();
    return ok();
}

auto stream::receive_reset(uint64_t error_code, uint64_t final_size) -> VoidResult
{
    if (is_unidirectional() && is_local_) {
        return error_void(stream_error::stream_state_error,
                         "Cannot receive reset on local unidirectional stream",
                         "quic::stream");
    }

    // Check final size consistency
    if (final_size_.has_value() && *final_size_ != final_size) {
        return error_void(stream_error::final_size_error,
                         "Reset final size differs from previously received",
                         "quic::stream");
    }

    final_size_ = final_size;
    reset_error_code_ = error_code;
    recv_state_ = recv_stream_state::reset_recvd;

    // Clear buffered data
    recv_buffer_.clear();

    return ok();
}

auto stream::receive_stop_sending(uint64_t error_code) -> VoidResult
{
    if (!can_send()) {
        return error_void(stream_error::stream_state_error,
                         "Cannot handle STOP_SENDING in current state",
                         "quic::stream");
    }

    stop_sending_error_code_ = error_code;
    // The sender SHOULD reset the stream in response
    return ok();
}

// ============================================================================
// Flow Control
// ============================================================================

void stream::set_max_send_data(uint64_t max)
{
    if (max > max_send_offset_) {
        max_send_offset_ = max;
    }
}

auto stream::available_send_window() const noexcept -> size_t
{
    const uint64_t sent = send_offset_ + send_buffer_.size();
    if (sent >= max_send_offset_) {
        return 0;
    }
    return static_cast<size_t>(max_send_offset_ - sent);
}

void stream::set_max_recv_data(uint64_t max)
{
    if (max > max_recv_offset_) {
        max_recv_offset_ = max;
    }
}

auto stream::should_send_max_stream_data() const noexcept -> bool
{
    // Send update when we've consumed more than threshold of the window
    const uint64_t consumed = recv_offset_;
    const uint64_t threshold = static_cast<uint64_t>(
        recv_window_size_ * window_update_threshold_);

    return consumed > (max_recv_offset_ - recv_window_size_ + threshold);
}

auto stream::generate_max_stream_data() -> std::optional<uint64_t>
{
    if (!should_send_max_stream_data()) {
        return std::nullopt;
    }

    // Increase the window
    max_recv_offset_ = recv_offset_ + recv_window_size_;
    return max_recv_offset_;
}

// ============================================================================
// Internal Helpers
// ============================================================================

void stream::reassemble_data()
{
    while (!recv_buffer_.empty()) {
        auto it = recv_buffer_.find(recv_offset_);
        if (it == recv_buffer_.end()) {
            break; // Gap in data
        }

        recv_ready_.insert(recv_ready_.end(), it->second.begin(), it->second.end());
        recv_offset_ += it->second.size();
        recv_buffer_.erase(it);
    }
}

void stream::update_send_state()
{
    switch (send_state_) {
        case send_stream_state::ready:
            if (!send_buffer_.empty()) {
                send_state_ = send_stream_state::send;
            }
            break;

        case send_stream_state::send:
            if (fin_sent_ && send_buffer_.empty()) {
                send_state_ = send_stream_state::data_sent;
            }
            break;

        case send_stream_state::data_sent:
            if (fin_acked_) {
                send_state_ = send_stream_state::data_recvd;
            }
            break;

        case send_stream_state::reset_sent:
            // Wait for acknowledgment
            break;

        case send_stream_state::reset_recvd:
        case send_stream_state::data_recvd:
            // Terminal states
            break;
    }
}

void stream::update_recv_state()
{
    switch (recv_state_) {
        case recv_stream_state::recv:
            if (recv_fin_) {
                recv_state_ = recv_stream_state::size_known;
            }
            break;

        case recv_stream_state::size_known:
            if (final_size_.has_value() && recv_offset_ >= *final_size_) {
                recv_state_ = recv_stream_state::data_recvd;
            }
            break;

        case recv_stream_state::data_recvd:
            if (recv_ready_.empty()) {
                recv_state_ = recv_stream_state::data_read;
            }
            break;

        case recv_stream_state::reset_recvd:
        case recv_stream_state::data_read:
        case recv_stream_state::reset_read:
            // Terminal states
            break;
    }
}

// ============================================================================
// State String Conversion
// ============================================================================

auto send_state_to_string(send_stream_state state) -> const char*
{
    switch (state) {
        case send_stream_state::ready:       return "ready";
        case send_stream_state::send:        return "send";
        case send_stream_state::data_sent:   return "data_sent";
        case send_stream_state::reset_sent:  return "reset_sent";
        case send_stream_state::reset_recvd: return "reset_recvd";
        case send_stream_state::data_recvd:  return "data_recvd";
        default:                              return "unknown";
    }
}

auto recv_state_to_string(recv_stream_state state) -> const char*
{
    switch (state) {
        case recv_stream_state::recv:        return "recv";
        case recv_stream_state::size_known:  return "size_known";
        case recv_stream_state::data_recvd:  return "data_recvd";
        case recv_stream_state::reset_recvd: return "reset_recvd";
        case recv_stream_state::data_read:   return "data_read";
        case recv_stream_state::reset_read:  return "reset_read";
        default:                              return "unknown";
    }
}

} // namespace kcenon::network::protocols::quic
