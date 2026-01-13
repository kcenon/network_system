/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include "kcenon/network/protocols/http2/http2_server_stream.h"

namespace kcenon::network::protocols::http2
{
    http2_server_stream::http2_server_stream(uint32_t stream_id,
                                             http2_request request,
                                             std::shared_ptr<hpack_encoder> encoder,
                                             frame_sender_t frame_sender,
                                             uint32_t max_frame_size)
        : stream_id_(stream_id)
        , request_(std::move(request))
        , encoder_(std::move(encoder))
        , frame_sender_(std::move(frame_sender))
        , max_frame_size_(max_frame_size)
    {
    }

    auto http2_server_stream::send_headers(int status_code,
                                           const std::vector<http_header>& headers,
                                           bool end_stream) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == stream_state::closed ||
            state_ == stream_state::half_closed_local) {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Stream is closed for sending",
                "http2_server_stream");
        }

        if (headers_sent_) {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Headers already sent",
                "http2_server_stream");
        }

        // Build response headers
        auto response_headers = build_response_headers(status_code, headers);

        // Encode headers using HPACK
        auto encoded = encoder_->encode(response_headers);

        // Create HEADERS frame
        headers_frame frame(stream_id_, std::move(encoded), end_stream, true);
        auto result = frame_sender_(frame);

        if (result.is_err()) {
            return result;
        }

        headers_sent_ = true;

        if (end_stream) {
            state_ = stream_state::half_closed_local;
        }

        return ok();
    }

    auto http2_server_stream::send_data(const std::vector<uint8_t>& data,
                                        bool end_stream) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == stream_state::closed ||
            state_ == stream_state::half_closed_local) {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Stream is closed for sending",
                "http2_server_stream");
        }

        if (!headers_sent_) {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Headers must be sent before data",
                "http2_server_stream");
        }

        // Split data into frames respecting max_frame_size
        size_t offset = 0;
        while (offset < data.size()) {
            size_t chunk_size = std::min(
                static_cast<size_t>(max_frame_size_),
                data.size() - offset);

            std::vector<uint8_t> chunk(data.begin() + static_cast<std::ptrdiff_t>(offset),
                                       data.begin() + static_cast<std::ptrdiff_t>(offset + chunk_size));

            bool is_last = (offset + chunk_size >= data.size());
            bool frame_end_stream = end_stream && is_last;

            data_frame frame(stream_id_, std::move(chunk), frame_end_stream);
            auto result = frame_sender_(frame);

            if (result.is_err()) {
                return result;
            }

            offset += chunk_size;
        }

        // Handle empty data case with end_stream
        if (data.empty() && end_stream) {
            data_frame frame(stream_id_, std::vector<uint8_t>{}, true);
            auto result = frame_sender_(frame);
            if (result.is_err()) {
                return result;
            }
        }

        if (end_stream) {
            state_ = stream_state::half_closed_local;
        }

        return ok();
    }

    auto http2_server_stream::send_data(std::string_view data, bool end_stream) -> VoidResult
    {
        std::vector<uint8_t> bytes(data.begin(), data.end());
        return send_data(bytes, end_stream);
    }

    auto http2_server_stream::start_response(int status_code,
                                             const std::vector<http_header>& headers) -> VoidResult
    {
        return send_headers(status_code, headers, false);
    }

    auto http2_server_stream::write(const std::vector<uint8_t>& chunk) -> VoidResult
    {
        return send_data(chunk, false);
    }

    auto http2_server_stream::end_response() -> VoidResult
    {
        return send_data(std::vector<uint8_t>{}, true);
    }

    auto http2_server_stream::reset(uint32_t err_code) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == stream_state::closed) {
            return ok();
        }

        rst_stream_frame frame(stream_id_, err_code);
        auto result = frame_sender_(frame);

        state_ = stream_state::closed;

        return result;
    }

    auto http2_server_stream::stream_id() const -> uint32_t
    {
        return stream_id_;
    }

    auto http2_server_stream::method() const -> std::string_view
    {
        return request_.method;
    }

    auto http2_server_stream::path() const -> std::string_view
    {
        return request_.path;
    }

    auto http2_server_stream::headers() const -> const std::vector<http_header>&
    {
        return request_.headers;
    }

    auto http2_server_stream::request() const -> const http2_request&
    {
        return request_;
    }

    auto http2_server_stream::is_open() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == stream_state::open ||
               state_ == stream_state::half_closed_remote;
    }

    auto http2_server_stream::headers_sent() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return headers_sent_;
    }

    auto http2_server_stream::state() const -> stream_state
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    auto http2_server_stream::update_window(int32_t increment) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        window_size_ += increment;
    }

    auto http2_server_stream::window_size() const -> int32_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return window_size_;
    }

    auto http2_server_stream::build_response_headers(int status_code,
                                                     const std::vector<http_header>& additional)
        -> std::vector<http_header>
    {
        std::vector<http_header> headers;
        headers.reserve(additional.size() + 1);

        // :status pseudo-header must be first
        headers.emplace_back(":status", std::to_string(status_code));

        // Add additional headers
        for (const auto& header : additional) {
            headers.push_back(header);
        }

        return headers;
    }

} // namespace kcenon::network::protocols::http2
