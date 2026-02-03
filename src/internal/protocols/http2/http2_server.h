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

#include "frame.h"
#include "hpack.h"
#include "http2_client.h"
#include "http2_request.h"
#include "http2_server_stream.h"
#include "kcenon/network/utils/result_types.h"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::network::protocols::http2
{
    // Forward declaration
    class http2_server_connection;

    /*!
     * \struct tls_config
     * \brief TLS configuration for HTTP/2 server
     */
    struct tls_config
    {
        std::string cert_file;                    //!< Path to certificate file (PEM)
        std::string key_file;                     //!< Path to private key file (PEM)
        std::string ca_file;                      //!< Path to CA certificate file (optional)
        bool verify_client = false;               //!< Require client certificate
    };

    /*!
     * \class http2_server
     * \brief HTTP/2 server with TLS support
     *
     * ### Thread Safety
     * - All public methods are thread-safe
     * - Request handler is called from I/O thread
     * - Multiple connections handled concurrently
     *
     * ### Features
     * - HTTP/2 protocol support (RFC 7540)
     * - HPACK header compression (RFC 7541)
     * - TLS 1.3 with ALPN "h2" negotiation
     * - Stream multiplexing
     * - Flow control
     * - Connection-level and stream-level flow control
     *
     * ### Usage Example
     * \code
     * auto server = std::make_shared<http2_server>("my-server");
     *
     * server->set_request_handler([](http2_server_stream& stream,
     *                                const http2_request& request) {
     *     if (request.method == "GET" && request.path == "/api/health") {
     *         stream.send_headers(200, {{"content-type", "application/json"}});
     *         stream.send_data(R"({"status": "ok"})", true);
     *     } else {
     *         stream.send_headers(404, {});
     *         stream.send_data("Not Found", true);
     *     }
     * });
     *
     * // Start with TLS
     * tls_config config;
     * config.cert_file = "/path/to/cert.pem";
     * config.key_file = "/path/to/key.pem";
     * server->start_tls(8443, config);
     *
     * // Wait for shutdown signal
     * server->wait();
     *
     * // Stop server
     * server->stop();
     * \endcode
     */
    class http2_server : public std::enable_shared_from_this<http2_server>
    {
    public:
        //! Request handler function type
        using request_handler_t = std::function<void(
            http2_server_stream& stream,
            const http2_request& request)>;

        //! Error handler function type
        using error_handler_t = std::function<void(
            const std::string& error_message)>;

        /*!
         * \brief Construct HTTP/2 server
         * \param server_id Unique server identifier for logging
         */
        explicit http2_server(std::string_view server_id);

        /*!
         * \brief Destructor - stops server gracefully
         */
        ~http2_server();

        // Non-copyable
        http2_server(const http2_server&) = delete;
        http2_server& operator=(const http2_server&) = delete;

        // Non-movable (contains std::atomic members)
        http2_server(http2_server&&) = delete;
        http2_server& operator=(http2_server&&) = delete;

        /*!
         * \brief Start HTTP/2 server without TLS (h2c - HTTP/2 cleartext)
         * \param port Port to listen on
         * \return Success or error
         *
         * \note h2c is not widely supported. Prefer start_tls() for production.
         */
        auto start(unsigned short port) -> VoidResult;

        /*!
         * \brief Start HTTP/2 server with TLS
         * \param port Port to listen on
         * \param config TLS configuration
         * \return Success or error
         *
         * Starts the server with TLS and ALPN "h2" negotiation.
         */
        auto start_tls(unsigned short port, const tls_config& config) -> VoidResult;

        /*!
         * \brief Stop the server
         * \return Success or error
         *
         * Sends GOAWAY to all connections and stops accepting new connections.
         */
        auto stop() -> VoidResult;

        /*!
         * \brief Check if server is running
         * \return True if server is accepting connections
         */
        [[nodiscard]] auto is_running() const -> bool;

        /*!
         * \brief Wait for server to stop
         *
         * Blocks until stop() is called.
         */
        auto wait() -> void;

        /*!
         * \brief Set request handler
         * \param handler Function to handle incoming requests
         *
         * The handler is called for each complete HTTP/2 request.
         * Handler should not block for long periods.
         */
        auto set_request_handler(request_handler_t handler) -> void;

        /*!
         * \brief Set error handler
         * \param handler Function to handle errors
         */
        auto set_error_handler(error_handler_t handler) -> void;

        /*!
         * \brief Set server settings
         * \param settings HTTP/2 settings to use
         */
        auto set_settings(const http2_settings& settings) -> void;

        /*!
         * \brief Get current settings
         * \return Current HTTP/2 settings
         */
        [[nodiscard]] auto get_settings() const -> http2_settings;

        /*!
         * \brief Get number of active connections
         * \return Active connection count
         */
        [[nodiscard]] auto active_connections() const -> size_t;

        /*!
         * \brief Get total number of active streams across all connections
         * \return Active stream count
         */
        [[nodiscard]] auto active_streams() const -> size_t;

        /*!
         * \brief Get server identifier
         * \return Server ID string
         */
        [[nodiscard]] auto server_id() const -> std::string_view;

    private:
        // Internal connection handling
        auto do_accept() -> void;
        auto do_accept_tls() -> void;
        auto handle_accept(std::error_code ec, asio::ip::tcp::socket socket) -> void;
        auto handle_accept_tls(std::error_code ec, asio::ip::tcp::socket socket) -> void;

        // Connection management
        auto add_connection(std::shared_ptr<http2_server_connection> conn) -> void;
        auto remove_connection(uint64_t connection_id) -> void;
        auto cleanup_dead_connections() -> void;
        auto start_cleanup_timer() -> void;

        // I/O thread management
        auto run_io() -> void;
        auto stop_io() -> void;

        // Member variables
        std::string server_id_;                   //!< Server identifier

        // ASIO context
        std::unique_ptr<asio::io_context> io_context_;
        std::unique_ptr<asio::ssl::context> ssl_context_;
        std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
        std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
        std::future<void> io_future_;

        // Server state
        std::atomic<bool> is_running_{false};
        std::atomic<bool> use_tls_{false};
        std::promise<void> stop_promise_;
        std::future<void> stop_future_;

        // Handlers
        request_handler_t request_handler_;
        error_handler_t error_handler_;

        // Connection management
        std::mutex connections_mutex_;
        std::map<uint64_t, std::shared_ptr<http2_server_connection>> connections_;
        std::atomic<uint64_t> next_connection_id_{1};

        // Settings
        http2_settings settings_;

        // HPACK encoder for all connections
        std::shared_ptr<hpack_encoder> encoder_;
        std::shared_ptr<hpack_decoder> decoder_;

        // Cleanup timer
        std::unique_ptr<asio::steady_timer> cleanup_timer_;

        // Constants
        static constexpr std::string_view CONNECTION_PREFACE =
            "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
        static constexpr size_t FRAME_HEADER_SIZE = 9;
    };

    /*!
     * \class http2_server_connection
     * \brief Represents a single HTTP/2 connection on the server side
     *
     * Handles frame parsing, stream management, and HPACK encoding/decoding
     * for a single client connection.
     */
    class http2_server_connection : public std::enable_shared_from_this<http2_server_connection>
    {
    public:
        /*!
         * \brief Construct server connection with plain socket
         * \param connection_id Unique connection identifier
         * \param socket TCP socket
         * \param settings Server HTTP/2 settings
         * \param request_handler Request callback
         * \param error_handler Error callback
         */
        http2_server_connection(uint64_t connection_id,
                                asio::ip::tcp::socket socket,
                                const http2_settings& settings,
                                http2_server::request_handler_t request_handler,
                                http2_server::error_handler_t error_handler);

        /*!
         * \brief Construct server connection with TLS socket
         * \param connection_id Unique connection identifier
         * \param socket TLS socket
         * \param settings Server HTTP/2 settings
         * \param request_handler Request callback
         * \param error_handler Error callback
         */
        http2_server_connection(uint64_t connection_id,
                                std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> socket,
                                const http2_settings& settings,
                                http2_server::request_handler_t request_handler,
                                http2_server::error_handler_t error_handler);

        /*!
         * \brief Destructor
         */
        ~http2_server_connection();

        // Non-copyable, non-movable
        http2_server_connection(const http2_server_connection&) = delete;
        http2_server_connection& operator=(const http2_server_connection&) = delete;
        http2_server_connection(http2_server_connection&&) = delete;
        http2_server_connection& operator=(http2_server_connection&&) = delete;

        /*!
         * \brief Start connection handling
         * \return Success or error
         */
        auto start() -> VoidResult;

        /*!
         * \brief Stop connection
         * \return Success or error
         */
        auto stop() -> VoidResult;

        /*!
         * \brief Check if connection is alive
         * \return True if connection is active
         */
        [[nodiscard]] auto is_alive() const -> bool;

        /*!
         * \brief Get connection identifier
         * \return Connection ID
         */
        [[nodiscard]] auto connection_id() const -> uint64_t;

        /*!
         * \brief Get number of active streams
         * \return Stream count
         */
        [[nodiscard]] auto stream_count() const -> size_t;

    private:
        // Connection setup
        auto read_connection_preface() -> void;
        auto send_settings() -> VoidResult;
        auto handle_settings_frame(const settings_frame& frame) -> VoidResult;
        auto send_settings_ack() -> VoidResult;

        // Frame I/O
        auto start_reading() -> void;
        auto read_frame_header() -> void;
        auto read_frame_payload(uint32_t length) -> void;
        auto send_frame(const frame& f) -> VoidResult;
        auto process_frame(std::unique_ptr<frame> f) -> VoidResult;

        // Stream management
        auto get_or_create_stream(uint32_t stream_id) -> http2_stream*;
        auto close_stream(uint32_t stream_id) -> void;

        // Frame handlers
        auto handle_headers_frame(const headers_frame& f) -> VoidResult;
        auto handle_data_frame(const data_frame& f) -> VoidResult;
        auto handle_rst_stream_frame(const rst_stream_frame& f) -> VoidResult;
        auto handle_ping_frame(const ping_frame& f) -> VoidResult;
        auto handle_goaway_frame(const goaway_frame& f) -> VoidResult;
        auto handle_window_update_frame(const window_update_frame& f) -> VoidResult;

        // Request completion
        auto dispatch_request(uint32_t stream_id) -> void;

        uint64_t connection_id_;                  //!< Connection identifier
        bool use_tls_;                            //!< TLS mode flag

        // Socket (one of these is used)
        std::unique_ptr<asio::ip::tcp::socket> plain_socket_;
        std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> tls_socket_;

        // Connection state
        std::atomic<bool> is_alive_{true};
        std::atomic<bool> preface_received_{false};
        std::atomic<bool> goaway_sent_{false};

        // Stream management
        std::mutex streams_mutex_;
        std::map<uint32_t, http2_stream> streams_;
        uint32_t last_stream_id_{0};

        // Settings
        http2_settings local_settings_;
        http2_settings remote_settings_;

        // HPACK
        hpack_encoder encoder_;
        hpack_decoder decoder_;

        // Flow control
        int32_t connection_window_size_{65535};

        // Handlers
        http2_server::request_handler_t request_handler_;
        http2_server::error_handler_t error_handler_;

        // Read buffer
        std::vector<uint8_t> read_buffer_;
        std::array<uint8_t, 9> frame_header_buffer_;
    };

} // namespace kcenon::network::protocols::http2
