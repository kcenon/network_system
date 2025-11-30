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
#include "kcenon/network/utils/result_types.h"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace network_system::protocols::http2
{
    /*!
     * \enum stream_state
     * \brief HTTP/2 stream state (RFC 7540 Section 5.1)
     */
    enum class stream_state
    {
        idle,              //!< Stream not yet opened
        open,              //!< Stream open and active
        half_closed_local, //!< Local end closed, remote can send
        half_closed_remote,//!< Remote end closed, local can send
        closed             //!< Stream fully closed
    };

    /*!
     * \struct http2_response
     * \brief HTTP/2 response data
     */
    struct http2_response
    {
        int status_code = 0;                              //!< HTTP status code
        std::vector<http_header> headers;                 //!< Response headers
        std::vector<uint8_t> body;                        //!< Response body

        /*!
         * \brief Get header value by name
         * \param name Header name (case-insensitive)
         * \return Header value if found
         */
        auto get_header(const std::string& name) const -> std::optional<std::string>;

        /*!
         * \brief Get body as string
         * \return Body as UTF-8 string
         */
        auto get_body_string() const -> std::string;
    };

    /*!
     * \struct http2_stream
     * \brief HTTP/2 stream state and data
     */
    struct http2_stream
    {
        uint32_t stream_id = 0;               //!< Stream identifier
        stream_state state = stream_state::idle;  //!< Current state
        std::vector<http_header> request_headers;  //!< Request headers
        std::vector<http_header> response_headers; //!< Response headers
        std::vector<uint8_t> request_body;    //!< Request body
        std::vector<uint8_t> response_body;   //!< Response body
        int32_t window_size = 65535;          //!< Flow control window
        std::promise<http2_response> promise; //!< Response promise
        bool headers_complete = false;        //!< Headers fully received
        bool body_complete = false;           //!< Body fully received

        //! Streaming support
        bool is_streaming = false;            //!< Whether this is a streaming request
        std::function<void(std::vector<uint8_t>)> on_data;  //!< Callback for streaming data
        std::function<void(std::vector<http_header>)> on_headers;  //!< Callback for headers
        std::function<void(int)> on_complete; //!< Callback when stream ends (status code)

        http2_stream() = default;
        http2_stream(http2_stream&&) = default;
        http2_stream& operator=(http2_stream&&) = default;
    };

    /*!
     * \struct http2_settings
     * \brief HTTP/2 connection settings
     */
    struct http2_settings
    {
        uint32_t header_table_size = 4096;        //!< HPACK dynamic table size
        bool enable_push = false;                 //!< Server push enabled
        uint32_t max_concurrent_streams = 100;    //!< Max concurrent streams
        uint32_t initial_window_size = 65535;     //!< Initial flow control window
        uint32_t max_frame_size = 16384;          //!< Max frame payload size
        uint32_t max_header_list_size = 8192;     //!< Max header list size
    };

    /*!
     * \class http2_client
     * \brief HTTP/2 client with TLS support
     *
     * ### Thread Safety
     * - All public methods are thread-safe
     * - Multiple requests can be made concurrently (multiplexing)
     * - Uses single TLS connection for all requests
     *
     * ### Features
     * - HTTP/2 protocol support (RFC 7540)
     * - HPACK header compression (RFC 7541)
     * - TLS 1.3 with ALPN negotiation
     * - Stream multiplexing
     * - Flow control
     * - Server push disabled by default
     *
     * ### Usage Example
     * \code
     * auto client = std::make_shared<http2_client>("my-client");
     *
     * // Connect to server
     * auto connect_result = client->connect("example.com", 443);
     * if (!connect_result) {
     *     std::cerr << "Connect failed: " << connect_result.error().message << "\n";
     *     return;
     * }
     *
     * // Simple GET request
     * auto response = client->get("/api/users");
     * if (response) {
     *     std::cout << "Status: " << response->status_code << "\n";
     *     std::cout << "Body: " << response->get_body_string() << "\n";
     * }
     *
     * // POST with JSON body
     * std::vector<http_header> headers = {
     *     {"content-type", "application/json"}
     * };
     * std::string json_body = R"({"name": "John"})";
     * auto post_response = client->post("/api/users", json_body, headers);
     *
     * // Disconnect
     * client->disconnect();
     * \endcode
     */
    class http2_client : public std::enable_shared_from_this<http2_client>
    {
    public:
        /*!
         * \brief Construct HTTP/2 client
         * \param client_id Unique client identifier for logging
         */
        explicit http2_client(std::string_view client_id);

        /*!
         * \brief Destructor - closes connection gracefully
         */
        ~http2_client();

        // Non-copyable
        http2_client(const http2_client&) = delete;
        http2_client& operator=(const http2_client&) = delete;

        // Non-movable (contains std::atomic members)
        http2_client(http2_client&&) = delete;
        http2_client& operator=(http2_client&&) = delete;

        /*!
         * \brief Connect to HTTP/2 server
         * \param host Server hostname
         * \param port Server port (default 443)
         * \return Success or error
         *
         * Establishes TLS connection with ALPN "h2", sends connection preface,
         * and exchanges SETTINGS frames.
         */
        auto connect(const std::string& host, unsigned short port = 443) -> VoidResult;

        /*!
         * \brief Disconnect from server
         * \return Success or error
         *
         * Sends GOAWAY frame and closes connection gracefully.
         */
        auto disconnect() -> VoidResult;

        /*!
         * \brief Check if connected
         * \return True if connected
         */
        auto is_connected() const -> bool;

        /*!
         * \brief Perform HTTP/2 GET request
         * \param path Request path (e.g., "/api/users")
         * \param headers Additional headers (optional)
         * \return Result containing response or error
         */
        auto get(const std::string& path,
                 const std::vector<http_header>& headers = {})
            -> Result<http2_response>;

        /*!
         * \brief Perform HTTP/2 POST request
         * \param path Request path
         * \param body Request body as string
         * \param headers Additional headers (optional)
         * \return Result containing response or error
         */
        auto post(const std::string& path,
                  const std::string& body,
                  const std::vector<http_header>& headers = {})
            -> Result<http2_response>;

        /*!
         * \brief Perform HTTP/2 POST request with binary body
         * \param path Request path
         * \param body Request body as bytes
         * \param headers Additional headers (optional)
         * \return Result containing response or error
         */
        auto post(const std::string& path,
                  const std::vector<uint8_t>& body,
                  const std::vector<http_header>& headers = {})
            -> Result<http2_response>;

        /*!
         * \brief Perform HTTP/2 PUT request
         * \param path Request path
         * \param body Request body
         * \param headers Additional headers (optional)
         * \return Result containing response or error
         */
        auto put(const std::string& path,
                 const std::string& body,
                 const std::vector<http_header>& headers = {})
            -> Result<http2_response>;

        /*!
         * \brief Perform HTTP/2 DELETE request
         * \param path Request path
         * \param headers Additional headers (optional)
         * \return Result containing response or error
         */
        auto del(const std::string& path,
                 const std::vector<http_header>& headers = {})
            -> Result<http2_response>;

        /*!
         * \brief Set request timeout
         * \param timeout Timeout duration
         */
        auto set_timeout(std::chrono::milliseconds timeout) -> void;

        /*!
         * \brief Get current timeout
         * \return Timeout duration
         */
        auto get_timeout() const -> std::chrono::milliseconds;

        /*!
         * \brief Start a streaming POST request
         * \param path Request path
         * \param headers Additional headers
         * \param on_data Callback for each data chunk received
         * \param on_headers Callback when headers are received
         * \param on_complete Callback when stream completes (with status code)
         * \return Stream ID on success, or error
         *
         * Use write_stream() to send data and close_stream_writer() when done.
         */
        auto start_stream(const std::string& path,
                          const std::vector<http_header>& headers,
                          std::function<void(std::vector<uint8_t>)> on_data,
                          std::function<void(std::vector<http_header>)> on_headers,
                          std::function<void(int)> on_complete)
            -> Result<uint32_t>;

        /*!
         * \brief Write data to an open stream
         * \param stream_id Stream identifier
         * \param data Data to write
         * \param end_stream Set to true to signal end of client data
         * \return Success or error
         */
        auto write_stream(uint32_t stream_id,
                          const std::vector<uint8_t>& data,
                          bool end_stream = false) -> VoidResult;

        /*!
         * \brief Close the write side of a stream
         * \param stream_id Stream identifier
         * \return Success or error
         */
        auto close_stream_writer(uint32_t stream_id) -> VoidResult;

        /*!
         * \brief Cancel a stream
         * \param stream_id Stream identifier
         * \return Success or error
         */
        auto cancel_stream(uint32_t stream_id) -> VoidResult;

        /*!
         * \brief Get local settings
         * \return Current local settings
         */
        auto get_settings() const -> http2_settings;

        /*!
         * \brief Update local settings
         * \param settings New settings to apply
         */
        auto set_settings(const http2_settings& settings) -> void;

    private:
        // Connection management
        auto send_connection_preface() -> VoidResult;
        auto send_settings() -> VoidResult;
        auto handle_settings_frame(const settings_frame& frame) -> VoidResult;
        auto send_settings_ack() -> VoidResult;

        // Frame I/O
        auto send_frame(const frame& f) -> VoidResult;
        auto read_frame() -> Result<std::unique_ptr<frame>>;
        auto process_frame(std::unique_ptr<frame> f) -> VoidResult;

        // Stream management
        auto allocate_stream_id() -> uint32_t;
        auto get_stream(uint32_t stream_id) -> http2_stream*;
        auto create_stream() -> http2_stream&;
        auto close_stream(uint32_t stream_id) -> void;

        // Request handling
        auto send_request(const std::string& method,
                          const std::string& path,
                          const std::vector<http_header>& headers,
                          const std::vector<uint8_t>& body)
            -> Result<http2_response>;
        auto build_headers(const std::string& method,
                           const std::string& path,
                           const std::vector<http_header>& additional) -> std::vector<http_header>;

        // Response handling
        auto handle_headers_frame(const headers_frame& f) -> VoidResult;
        auto handle_data_frame(const data_frame& f) -> VoidResult;
        auto handle_rst_stream_frame(const rst_stream_frame& f) -> VoidResult;
        auto handle_goaway_frame(const goaway_frame& f) -> VoidResult;
        auto handle_window_update_frame(const window_update_frame& f) -> VoidResult;
        auto handle_ping_frame(const ping_frame& f) -> VoidResult;

        // I/O thread
        auto run_io() -> void;
        auto stop_io() -> void;

        // Member variables
        std::string client_id_;                          //!< Client identifier
        std::string host_;                               //!< Connected host
        unsigned short port_ = 443;                      //!< Connected port

        // ASIO context
        std::unique_ptr<asio::io_context> io_context_;
        std::unique_ptr<asio::ssl::context> ssl_context_;
        std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> socket_;
        std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
        std::future<void> io_future_;

        // Connection state
        std::atomic<bool> is_connected_{false};
        std::atomic<bool> is_running_{false};
        std::atomic<bool> goaway_received_{false};

        // Stream management
        std::mutex streams_mutex_;
        std::map<uint32_t, http2_stream> streams_;
        std::atomic<uint32_t> next_stream_id_{1};        //!< Client streams are odd
        int32_t connection_window_size_ = 65535;

        // Settings (must be declared before encoder_/decoder_ for initialization order)
        http2_settings local_settings_;
        http2_settings remote_settings_;

        // HPACK (uses local_settings_.header_table_size in constructor)
        hpack_encoder encoder_;
        hpack_decoder decoder_;

        // Timeout
        std::chrono::milliseconds timeout_{30000};

        // Read buffer
        std::vector<uint8_t> read_buffer_;

        // Constants
        static constexpr std::string_view CONNECTION_PREFACE =
            "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        static constexpr size_t FRAME_HEADER_SIZE = 9;
        static constexpr size_t DEFAULT_WINDOW_SIZE = 65535;
    };

} // namespace network_system::protocols::http2
