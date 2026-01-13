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

#pragma once

#include "kcenon/network/protocols/http2/frame.h"
#include "kcenon/network/protocols/http2/hpack.h"
#include "kcenon/network/protocols/http2/http2_client.h"
#include "kcenon/network/protocols/http2/http2_request.h"
#include "kcenon/network/utils/result_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace kcenon::network::protocols::http2
{
    // Forward declaration
    class http2_connection;

    /*!
     * \class http2_server_stream
     * \brief Server-side HTTP/2 stream for sending responses
     *
     * Represents a single HTTP/2 stream on the server side, providing methods
     * to send response headers and data frames to the client.
     *
     * ### Thread Safety
     * - All public methods are thread-safe
     * - Stream state transitions are protected by mutex
     *
     * ### Usage Example
     * \code
     * // In request handler:
     * void handle_request(http2_server_stream& stream, const http2_request& request) {
     *     if (request.method == "GET" && request.path == "/api/health") {
     *         stream.send_headers(200, {{"content-type", "application/json"}});
     *         stream.send_data(R"({"status": "ok"})", true);
     *     } else {
     *         stream.send_headers(404, {});
     *         stream.send_data("Not Found", true);
     *     }
     * }
     * \endcode
     */
    class http2_server_stream
    {
    public:
        //! Function type for sending frames
        using frame_sender_t = std::function<VoidResult(const frame&)>;

        /*!
         * \brief Construct server stream
         * \param stream_id HTTP/2 stream identifier
         * \param request The parsed HTTP/2 request
         * \param encoder HPACK encoder for header compression
         * \param frame_sender Function to send frames to the connection
         * \param max_frame_size Maximum frame payload size
         */
        http2_server_stream(uint32_t stream_id,
                            http2_request request,
                            std::shared_ptr<hpack_encoder> encoder,
                            frame_sender_t frame_sender,
                            uint32_t max_frame_size = 16384);

        /*!
         * \brief Destructor
         */
        ~http2_server_stream() = default;

        // Non-copyable, non-movable (contains mutex)
        http2_server_stream(const http2_server_stream&) = delete;
        http2_server_stream& operator=(const http2_server_stream&) = delete;
        http2_server_stream(http2_server_stream&&) = delete;
        http2_server_stream& operator=(http2_server_stream&&) = delete;

        /*!
         * \brief Send response headers
         * \param status_code HTTP status code (e.g., 200, 404)
         * \param headers Additional response headers
         * \param end_stream True to close the stream after headers (no body)
         * \return Success or error
         *
         * Sends a HEADERS frame with the response status and headers.
         * If end_stream is true, the stream will be half-closed (local).
         */
        auto send_headers(int status_code,
                          const std::vector<http_header>& headers,
                          bool end_stream = false) -> VoidResult;

        /*!
         * \brief Send response data
         * \param data Data to send
         * \param end_stream True to close the stream after this data
         * \return Success or error
         *
         * Sends one or more DATA frames with the response body.
         * Data is automatically split into frames respecting max_frame_size.
         */
        auto send_data(const std::vector<uint8_t>& data,
                       bool end_stream = false) -> VoidResult;

        /*!
         * \brief Send response data from string
         * \param data String data to send
         * \param end_stream True to close the stream after this data
         * \return Success or error
         */
        auto send_data(std::string_view data, bool end_stream = false) -> VoidResult;

        /*!
         * \brief Start a streaming response
         * \param status_code HTTP status code
         * \param headers Response headers
         * \return Success or error
         *
         * Sends initial HEADERS frame without END_STREAM flag,
         * allowing multiple DATA frames to be sent via write().
         */
        auto start_response(int status_code,
                            const std::vector<http_header>& headers) -> VoidResult;

        /*!
         * \brief Write data chunk for streaming response
         * \param chunk Data chunk to send
         * \return Success or error
         *
         * Use this after start_response() to send body data in chunks.
         */
        auto write(const std::vector<uint8_t>& chunk) -> VoidResult;

        /*!
         * \brief End the streaming response
         * \return Success or error
         *
         * Sends an empty DATA frame with END_STREAM flag to complete the response.
         */
        auto end_response() -> VoidResult;

        /*!
         * \brief Reset the stream with an error code
         * \param err_code HTTP/2 error code (default: CANCEL)
         * \return Success or error
         *
         * Sends a RST_STREAM frame to immediately terminate the stream.
         */
        auto reset(uint32_t err_code = static_cast<uint32_t>(error_code::cancel))
            -> VoidResult;

        /*!
         * \brief Get stream identifier
         * \return Stream ID (odd numbers for client-initiated)
         */
        [[nodiscard]] auto stream_id() const -> uint32_t;

        /*!
         * \brief Get request method
         * \return HTTP method string
         */
        [[nodiscard]] auto method() const -> std::string_view;

        /*!
         * \brief Get request path
         * \return Request path string
         */
        [[nodiscard]] auto path() const -> std::string_view;

        /*!
         * \brief Get request headers
         * \return Reference to request headers vector
         */
        [[nodiscard]] auto headers() const -> const std::vector<http_header>&;

        /*!
         * \brief Get the full request
         * \return Reference to the request object
         */
        [[nodiscard]] auto request() const -> const http2_request&;

        /*!
         * \brief Check if stream is open for sending
         * \return True if stream can send data
         */
        [[nodiscard]] auto is_open() const -> bool;

        /*!
         * \brief Check if headers have been sent
         * \return True if headers have been sent
         */
        [[nodiscard]] auto headers_sent() const -> bool;

        /*!
         * \brief Get current stream state
         * \return Current stream state
         */
        [[nodiscard]] auto state() const -> stream_state;

        /*!
         * \brief Update flow control window
         * \param increment Window size increment
         */
        auto update_window(int32_t increment) -> void;

        /*!
         * \brief Get available window size
         * \return Current window size for sending
         */
        [[nodiscard]] auto window_size() const -> int32_t;

    private:
        /*!
         * \brief Build response headers with :status pseudo-header
         * \param status_code HTTP status code
         * \param additional Additional headers
         * \return Complete header list for HPACK encoding
         */
        auto build_response_headers(int status_code,
                                    const std::vector<http_header>& additional)
            -> std::vector<http_header>;

        uint32_t stream_id_;                          //!< Stream identifier
        http2_request request_;                       //!< Original request
        std::shared_ptr<hpack_encoder> encoder_;      //!< HPACK encoder
        frame_sender_t frame_sender_;                 //!< Frame sender function
        uint32_t max_frame_size_;                     //!< Maximum frame payload size

        stream_state state_{stream_state::open};      //!< Current stream state
        bool headers_sent_{false};                    //!< Headers have been sent
        int32_t window_size_{65535};                  //!< Flow control window

        mutable std::mutex mutex_;                    //!< State protection mutex
    };

} // namespace kcenon::network::protocols::http2
