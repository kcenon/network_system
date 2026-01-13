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

#include "kcenon/network/protocols/http2/http2_server.h"

#include <algorithm>
#include <stdexcept>

namespace kcenon::network::protocols::http2
{
    // =========================================================================
    // http2_server implementation
    // =========================================================================

    http2_server::http2_server(std::string_view server_id)
        : server_id_(server_id)
        , stop_future_(stop_promise_.get_future())
        , encoder_(std::make_shared<hpack_encoder>(settings_.header_table_size))
        , decoder_(std::make_shared<hpack_decoder>(settings_.header_table_size))
    {
    }

    http2_server::~http2_server()
    {
        if (is_running_) {
            stop();
        }
    }

    auto http2_server::start(unsigned short port) -> VoidResult
    {
        if (is_running_) {
            return error_void(
                error_codes::common_errors::already_exists,
                "Server already running",
                "http2_server");
        }

        try {
            io_context_ = std::make_unique<asio::io_context>();
            work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
                io_context_->get_executor());

            acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
                *io_context_,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));

            use_tls_ = false;
            is_running_ = true;

            // Start I/O thread
            io_future_ = std::async(std::launch::async, [this]() { run_io(); });

            // Start accepting connections
            do_accept();
            start_cleanup_timer();

            return ok();
        } catch (const std::exception& e) {
            return error_void(
                error_codes::network_system::bind_failed,
                std::string("Failed to start server: ") + e.what(),
                "http2_server");
        }
    }

    auto http2_server::start_tls(unsigned short port, const tls_config& config) -> VoidResult
    {
        if (is_running_) {
            return error_void(
                error_codes::common_errors::already_exists,
                "Server already running",
                "http2_server");
        }

        try {
            io_context_ = std::make_unique<asio::io_context>();
            work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
                io_context_->get_executor());

            // Setup TLS context
            ssl_context_ = std::make_unique<asio::ssl::context>(asio::ssl::context::tls_server);
            ssl_context_->set_options(
                asio::ssl::context::default_workarounds |
                asio::ssl::context::no_sslv2 |
                asio::ssl::context::no_sslv3 |
                asio::ssl::context::no_tlsv1 |
                asio::ssl::context::no_tlsv1_1);

            ssl_context_->use_certificate_file(config.cert_file, asio::ssl::context::pem);
            ssl_context_->use_private_key_file(config.key_file, asio::ssl::context::pem);

            if (!config.ca_file.empty()) {
                ssl_context_->load_verify_file(config.ca_file);
            }

            if (config.verify_client) {
                ssl_context_->set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
            }

            // Set ALPN callback for HTTP/2
            SSL_CTX_set_alpn_select_cb(
                ssl_context_->native_handle(),
                [](SSL* /*ssl*/, const unsigned char** out, unsigned char* outlen,
                   const unsigned char* in, unsigned int inlen, void* /*arg*/) -> int {
                    // Look for "h2" in client's ALPN list
                    const unsigned char* client = in;
                    const unsigned char* end = in + inlen;
                    while (client < end) {
                        unsigned char len = *client++;
                        if (len == 2 && client + 2 <= end &&
                            client[0] == 'h' && client[1] == '2') {
                            *out = client;
                            *outlen = 2;
                            return SSL_TLSEXT_ERR_OK;
                        }
                        client += len;
                    }
                    return SSL_TLSEXT_ERR_NOACK;
                },
                nullptr);

            acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
                *io_context_,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));

            use_tls_ = true;
            is_running_ = true;

            // Start I/O thread
            io_future_ = std::async(std::launch::async, [this]() { run_io(); });

            // Start accepting connections
            do_accept_tls();
            start_cleanup_timer();

            return ok();
        } catch (const std::exception& e) {
            return error_void(
                error_codes::network_system::bind_failed,
                std::string("Failed to start TLS server: ") + e.what(),
                "http2_server");
        }
    }

    auto http2_server::stop() -> VoidResult
    {
        if (!is_running_) {
            return ok();
        }

        is_running_ = false;

        // Close acceptor
        if (acceptor_ && acceptor_->is_open()) {
            std::error_code ec;
            acceptor_->close(ec);
        }

        // Stop all connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (auto& [id, conn] : connections_) {
                conn->stop();
            }
            connections_.clear();
        }

        // Stop cleanup timer
        if (cleanup_timer_) {
            cleanup_timer_->cancel();
        }

        stop_io();

        // Signal stop
        try {
            stop_promise_.set_value();
        } catch (...) {
            // Already set
        }

        return ok();
    }

    auto http2_server::is_running() const -> bool
    {
        return is_running_;
    }

    auto http2_server::wait() -> void
    {
        stop_future_.wait();
    }

    auto http2_server::set_request_handler(request_handler_t handler) -> void
    {
        request_handler_ = std::move(handler);
    }

    auto http2_server::set_error_handler(error_handler_t handler) -> void
    {
        error_handler_ = std::move(handler);
    }

    auto http2_server::set_settings(const http2_settings& settings) -> void
    {
        settings_ = settings;
        encoder_->set_max_table_size(settings.header_table_size);
        decoder_->set_max_table_size(settings.header_table_size);
    }

    auto http2_server::get_settings() const -> http2_settings
    {
        return settings_;
    }

    auto http2_server::active_connections() const -> size_t
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex_));
        return connections_.size();
    }

    auto http2_server::active_streams() const -> size_t
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(connections_mutex_));
        size_t total = 0;
        for (const auto& [id, conn] : connections_) {
            total += conn->stream_count();
        }
        return total;
    }

    auto http2_server::server_id() const -> std::string_view
    {
        return server_id_;
    }

    auto http2_server::do_accept() -> void
    {
        if (!is_running_ || !acceptor_) {
            return;
        }

        acceptor_->async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket socket) {
                handle_accept(ec, std::move(socket));
            });
    }

    auto http2_server::do_accept_tls() -> void
    {
        if (!is_running_ || !acceptor_) {
            return;
        }

        acceptor_->async_accept(
            [this](std::error_code ec, asio::ip::tcp::socket socket) {
                handle_accept_tls(ec, std::move(socket));
            });
    }

    auto http2_server::handle_accept(std::error_code ec, asio::ip::tcp::socket socket) -> void
    {
        if (!is_running_) {
            return;
        }

        if (ec) {
            if (error_handler_) {
                error_handler_(std::string("Accept error: ") + ec.message());
            }
        } else {
            uint64_t conn_id = next_connection_id_++;
            auto conn = std::make_shared<http2_server_connection>(
                conn_id,
                std::move(socket),
                settings_,
                request_handler_,
                error_handler_);

            add_connection(conn);
            conn->start();
        }

        // Continue accepting
        do_accept();
    }

    auto http2_server::handle_accept_tls(std::error_code ec, asio::ip::tcp::socket socket) -> void
    {
        if (!is_running_) {
            return;
        }

        if (ec) {
            if (error_handler_) {
                error_handler_(std::string("Accept error: ") + ec.message());
            }
            do_accept_tls();
            return;
        }

        auto tls_socket = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket>>(
            std::move(socket), *ssl_context_);

        auto* raw_socket = tls_socket.get();
        raw_socket->async_handshake(
            asio::ssl::stream_base::server,
            [this, tls_socket = std::move(tls_socket)](std::error_code ec) mutable {
                if (ec) {
                    if (error_handler_) {
                        error_handler_(std::string("TLS handshake error: ") + ec.message());
                    }
                } else {
                    uint64_t conn_id = next_connection_id_++;
                    auto conn = std::make_shared<http2_server_connection>(
                        conn_id,
                        std::move(tls_socket),
                        settings_,
                        request_handler_,
                        error_handler_);

                    add_connection(conn);
                    conn->start();
                }

                // Continue accepting
                do_accept_tls();
            });
    }

    auto http2_server::add_connection(std::shared_ptr<http2_server_connection> conn) -> void
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[conn->connection_id()] = std::move(conn);
    }

    auto http2_server::remove_connection(uint64_t connection_id) -> void
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(connection_id);
    }

    auto http2_server::cleanup_dead_connections() -> void
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto it = connections_.begin(); it != connections_.end();) {
            if (!it->second->is_alive()) {
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    auto http2_server::start_cleanup_timer() -> void
    {
        if (!io_context_) {
            return;
        }

        cleanup_timer_ = std::make_unique<asio::steady_timer>(*io_context_);
        cleanup_timer_->expires_after(std::chrono::seconds(30));
        cleanup_timer_->async_wait([this](std::error_code ec) {
            if (!ec && is_running_) {
                cleanup_dead_connections();
                start_cleanup_timer();
            }
        });
    }

    auto http2_server::run_io() -> void
    {
        if (io_context_) {
            io_context_->run();
        }
    }

    auto http2_server::stop_io() -> void
    {
        if (work_guard_) {
            work_guard_.reset();
        }

        if (io_context_) {
            io_context_->stop();
        }

        if (io_future_.valid()) {
            io_future_.wait();
        }
    }

    // =========================================================================
    // http2_server_connection implementation
    // =========================================================================

    http2_server_connection::http2_server_connection(
        uint64_t connection_id,
        asio::ip::tcp::socket socket,
        const http2_settings& settings,
        http2_server::request_handler_t request_handler,
        http2_server::error_handler_t error_handler)
        : connection_id_(connection_id)
        , use_tls_(false)
        , plain_socket_(std::make_unique<asio::ip::tcp::socket>(std::move(socket)))
        , local_settings_(settings)
        , encoder_(settings.header_table_size)
        , decoder_(settings.header_table_size)
        , request_handler_(std::move(request_handler))
        , error_handler_(std::move(error_handler))
        , frame_header_buffer_{}
    {
    }

    http2_server_connection::http2_server_connection(
        uint64_t connection_id,
        std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> socket,
        const http2_settings& settings,
        http2_server::request_handler_t request_handler,
        http2_server::error_handler_t error_handler)
        : connection_id_(connection_id)
        , use_tls_(true)
        , tls_socket_(std::move(socket))
        , local_settings_(settings)
        , encoder_(settings.header_table_size)
        , decoder_(settings.header_table_size)
        , request_handler_(std::move(request_handler))
        , error_handler_(std::move(error_handler))
        , frame_header_buffer_{}
    {
    }

    http2_server_connection::~http2_server_connection()
    {
        stop();
    }

    auto http2_server_connection::start() -> VoidResult
    {
        // Read connection preface from client
        read_connection_preface();
        return ok();
    }

    auto http2_server_connection::stop() -> VoidResult
    {
        if (!is_alive_) {
            return ok();
        }

        is_alive_ = false;

        // Close socket
        std::error_code ec;
        if (use_tls_ && tls_socket_) {
            tls_socket_->lowest_layer().close(ec);
        } else if (plain_socket_) {
            plain_socket_->close(ec);
        }

        return ok();
    }

    auto http2_server_connection::is_alive() const -> bool
    {
        return is_alive_;
    }

    auto http2_server_connection::connection_id() const -> uint64_t
    {
        return connection_id_;
    }

    auto http2_server_connection::stream_count() const -> size_t
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(streams_mutex_));
        return streams_.size();
    }

    auto http2_server_connection::read_connection_preface() -> void
    {
        constexpr size_t PREFACE_SIZE = 24; // "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
        auto buffer = std::make_shared<std::vector<uint8_t>>(PREFACE_SIZE);

        auto read_handler = [this, buffer](std::error_code ec, std::size_t bytes_read) {
            if (ec || bytes_read != 24) {
                if (error_handler_) {
                    error_handler_("Failed to read connection preface");
                }
                stop();
                return;
            }

            // Verify preface
            const std::string expected = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
            if (std::string(buffer->begin(), buffer->end()) != expected) {
                if (error_handler_) {
                    error_handler_("Invalid connection preface");
                }
                stop();
                return;
            }

            preface_received_ = true;

            // Send our SETTINGS frame
            auto result = send_settings();
            if (result.is_err()) {
                if (error_handler_) {
                    error_handler_("Failed to send settings");
                }
                stop();
                return;
            }

            // Start reading frames
            start_reading();
        };

        if (use_tls_) {
            asio::async_read(*tls_socket_, asio::buffer(*buffer), read_handler);
        } else {
            asio::async_read(*plain_socket_, asio::buffer(*buffer), read_handler);
        }
    }

    auto http2_server_connection::send_settings() -> VoidResult
    {
        std::vector<setting_parameter> params = {
            {static_cast<uint16_t>(setting_identifier::max_concurrent_streams),
             local_settings_.max_concurrent_streams},
            {static_cast<uint16_t>(setting_identifier::initial_window_size),
             local_settings_.initial_window_size},
            {static_cast<uint16_t>(setting_identifier::max_frame_size),
             local_settings_.max_frame_size},
            {static_cast<uint16_t>(setting_identifier::header_table_size),
             local_settings_.header_table_size},
        };

        settings_frame frame(params, false);
        return send_frame(frame);
    }

    auto http2_server_connection::handle_settings_frame(const settings_frame& frame) -> VoidResult
    {
        if (frame.is_ack()) {
            // Settings acknowledgment received
            return ok();
        }

        // Apply peer settings
        for (const auto& param : frame.settings()) {
            switch (static_cast<setting_identifier>(param.identifier)) {
                case setting_identifier::header_table_size:
                    remote_settings_.header_table_size = param.value;
                    encoder_.set_max_table_size(param.value);
                    break;
                case setting_identifier::enable_push:
                    remote_settings_.enable_push = (param.value != 0);
                    break;
                case setting_identifier::max_concurrent_streams:
                    remote_settings_.max_concurrent_streams = param.value;
                    break;
                case setting_identifier::initial_window_size:
                    remote_settings_.initial_window_size = param.value;
                    break;
                case setting_identifier::max_frame_size:
                    remote_settings_.max_frame_size = param.value;
                    break;
                case setting_identifier::max_header_list_size:
                    remote_settings_.max_header_list_size = param.value;
                    break;
            }
        }

        // Send ACK
        return send_settings_ack();
    }

    auto http2_server_connection::send_settings_ack() -> VoidResult
    {
        settings_frame frame({}, true);
        return send_frame(frame);
    }

    auto http2_server_connection::start_reading() -> void
    {
        read_frame_header();
    }

    auto http2_server_connection::read_frame_header() -> void
    {
        if (!is_alive_) {
            return;
        }

        auto read_handler = [this](std::error_code ec, std::size_t bytes_read) {
            if (ec || bytes_read != 9) {
                if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
                    if (error_handler_) {
                        error_handler_(std::string("Read error: ") + ec.message());
                    }
                }
                stop();
                return;
            }

            auto header_result = frame_header::parse(frame_header_buffer_);
            if (header_result.is_err()) {
                if (error_handler_) {
                    error_handler_("Failed to parse frame header");
                }
                stop();
                return;
            }

            auto header = header_result.value();
            if (header.length > 0) {
                read_frame_payload(header.length);
            } else {
                // Process frame with empty payload
                auto frame_result = frame::parse(frame_header_buffer_);
                if (frame_result.is_ok()) {
                    process_frame(std::move(frame_result.value()));
                }
                read_frame_header();
            }
        };

        if (use_tls_) {
            asio::async_read(*tls_socket_, asio::buffer(frame_header_buffer_), read_handler);
        } else {
            asio::async_read(*plain_socket_, asio::buffer(frame_header_buffer_), read_handler);
        }
    }

    auto http2_server_connection::read_frame_payload(uint32_t length) -> void
    {
        if (!is_alive_) {
            return;
        }

        read_buffer_.resize(9 + length);
        std::copy(frame_header_buffer_.begin(), frame_header_buffer_.end(), read_buffer_.begin());

        auto payload_buffer = asio::buffer(read_buffer_.data() + 9, length);

        auto read_handler = [this](std::error_code ec, std::size_t /*bytes_read*/) {
            if (ec) {
                if (ec != asio::error::eof && ec != asio::error::operation_aborted) {
                    if (error_handler_) {
                        error_handler_(std::string("Read payload error: ") + ec.message());
                    }
                }
                stop();
                return;
            }

            auto frame_result = frame::parse(read_buffer_);
            if (frame_result.is_ok()) {
                process_frame(std::move(frame_result.value()));
            } else {
                if (error_handler_) {
                    error_handler_("Failed to parse frame");
                }
            }

            read_frame_header();
        };

        if (use_tls_) {
            asio::async_read(*tls_socket_, payload_buffer, read_handler);
        } else {
            asio::async_read(*plain_socket_, payload_buffer, read_handler);
        }
    }

    auto http2_server_connection::send_frame(const frame& f) -> VoidResult
    {
        auto data = f.serialize();

        std::error_code ec;
        if (use_tls_) {
            asio::write(*tls_socket_, asio::buffer(data), ec);
        } else {
            asio::write(*plain_socket_, asio::buffer(data), ec);
        }

        if (ec) {
            return error_void(
                error_codes::network_system::send_failed,
                std::string("Failed to send frame: ") + ec.message(),
                "http2_server_connection");
        }

        return ok();
    }

    auto http2_server_connection::process_frame(std::unique_ptr<frame> f) -> VoidResult
    {
        auto& header = f->header();

        switch (header.type) {
            case frame_type::settings: {
                auto* settings_f = dynamic_cast<settings_frame*>(f.get());
                if (settings_f) {
                    return handle_settings_frame(*settings_f);
                }
                break;
            }
            case frame_type::headers: {
                auto* headers_f = dynamic_cast<headers_frame*>(f.get());
                if (headers_f) {
                    return handle_headers_frame(*headers_f);
                }
                break;
            }
            case frame_type::data: {
                auto* data_f = dynamic_cast<data_frame*>(f.get());
                if (data_f) {
                    return handle_data_frame(*data_f);
                }
                break;
            }
            case frame_type::rst_stream: {
                auto* rst_f = dynamic_cast<rst_stream_frame*>(f.get());
                if (rst_f) {
                    return handle_rst_stream_frame(*rst_f);
                }
                break;
            }
            case frame_type::ping: {
                auto* ping_f = dynamic_cast<ping_frame*>(f.get());
                if (ping_f) {
                    return handle_ping_frame(*ping_f);
                }
                break;
            }
            case frame_type::goaway: {
                auto* goaway_f = dynamic_cast<goaway_frame*>(f.get());
                if (goaway_f) {
                    return handle_goaway_frame(*goaway_f);
                }
                break;
            }
            case frame_type::window_update: {
                auto* wu_f = dynamic_cast<window_update_frame*>(f.get());
                if (wu_f) {
                    return handle_window_update_frame(*wu_f);
                }
                break;
            }
            default:
                // Ignore unknown frame types
                break;
        }

        return ok();
    }

    auto http2_server_connection::get_or_create_stream(uint32_t stream_id) -> http2_stream*
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);

        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            return &it->second;
        }

        // Create new stream
        http2_stream stream;
        stream.stream_id = stream_id;
        stream.state = stream_state::open;
        stream.window_size = static_cast<int32_t>(local_settings_.initial_window_size);

        auto [iter, inserted] = streams_.emplace(stream_id, std::move(stream));
        if (stream_id > last_stream_id_) {
            last_stream_id_ = stream_id;
        }

        return &iter->second;
    }

    auto http2_server_connection::close_stream(uint32_t stream_id) -> void
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        streams_.erase(stream_id);
    }

    auto http2_server_connection::handle_headers_frame(const headers_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;
        auto* stream = get_or_create_stream(stream_id);

        if (!stream) {
            return error_void(
                error_codes::common_errors::internal_error,
                "Failed to create stream",
                "http2_server_connection");
        }

        // Decode headers
        auto header_block = f.header_block();
        std::vector<uint8_t> block_data(header_block.begin(), header_block.end());
        auto headers_result = decoder_.decode(block_data);

        if (headers_result.is_err()) {
            // Send GOAWAY with COMPRESSION_ERROR
            goaway_frame goaway(last_stream_id_,
                static_cast<uint32_t>(error_code::compression_error));
            send_frame(goaway);
            stop();
            return error_void(
                headers_result.error().code,
                headers_result.error().message,
                "http2_server_connection");
        }

        stream->request_headers = headers_result.value();

        if (f.is_end_stream()) {
            stream->state = stream_state::half_closed_remote;
            dispatch_request(stream_id);
        } else if (f.is_end_headers()) {
            stream->headers_complete = true;
        }

        return ok();
    }

    auto http2_server_connection::handle_data_frame(const data_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;

        std::lock_guard<std::mutex> lock(streams_mutex_);
        auto it = streams_.find(stream_id);
        if (it == streams_.end()) {
            // Stream doesn't exist
            rst_stream_frame rst(stream_id, static_cast<uint32_t>(error_code::stream_closed));
            return send_frame(rst);
        }

        auto& stream = it->second;
        auto data = f.data();
        stream.request_body.insert(stream.request_body.end(), data.begin(), data.end());

        // Send WINDOW_UPDATE if needed
        size_t data_size = data.size();
        if (data_size > 0) {
            // Update connection window
            window_update_frame conn_wu(0, static_cast<uint32_t>(data_size));
            send_frame(conn_wu);

            // Update stream window
            window_update_frame stream_wu(stream_id, static_cast<uint32_t>(data_size));
            send_frame(stream_wu);
        }

        if (f.is_end_stream()) {
            stream.state = stream_state::half_closed_remote;
            stream.body_complete = true;

            // Need to unlock before dispatching
            lock.~lock_guard();
            dispatch_request(stream_id);
        }

        return ok();
    }

    auto http2_server_connection::handle_rst_stream_frame(const rst_stream_frame& f) -> VoidResult
    {
        close_stream(f.header().stream_id);
        return ok();
    }

    auto http2_server_connection::handle_ping_frame(const ping_frame& f) -> VoidResult
    {
        if (f.is_ack()) {
            return ok();
        }

        // Send PING ACK
        ping_frame ack(f.opaque_data(), true);
        return send_frame(ack);
    }

    auto http2_server_connection::handle_goaway_frame(const goaway_frame& /*f*/) -> VoidResult
    {
        // Client is closing connection
        stop();
        return ok();
    }

    auto http2_server_connection::handle_window_update_frame(const window_update_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;
        int32_t increment = static_cast<int32_t>(f.window_size_increment());

        if (stream_id == 0) {
            // Connection-level flow control
            connection_window_size_ += increment;
        } else {
            // Stream-level flow control
            std::lock_guard<std::mutex> lock(streams_mutex_);
            auto it = streams_.find(stream_id);
            if (it != streams_.end()) {
                it->second.window_size += increment;
            }
        }

        return ok();
    }

    auto http2_server_connection::dispatch_request(uint32_t stream_id) -> void
    {
        if (!request_handler_) {
            return;
        }

        http2_stream* stream = nullptr;
        {
            std::lock_guard<std::mutex> lock(streams_mutex_);
            auto it = streams_.find(stream_id);
            if (it == streams_.end()) {
                return;
            }
            stream = &it->second;
        }

        // Create request from headers
        auto request = http2_request::from_headers(stream->request_headers);
        request.body = stream->request_body;

        // Create server stream for response
        auto encoder_ptr = std::make_shared<hpack_encoder>(encoder_);
        auto weak_this = weak_from_this();

        http2_server_stream server_stream(
            stream_id,
            std::move(request),
            encoder_ptr,
            [weak_this](const frame& f) -> VoidResult {
                auto self = weak_this.lock();
                if (!self) {
                    return error_void(
                        error_codes::common_errors::internal_error,
                        "Connection closed",
                        "http2_server_stream");
                }
                return self->send_frame(f);
            },
            remote_settings_.max_frame_size);

        // Call request handler
        try {
            request_handler_(server_stream, server_stream.request());
        } catch (const std::exception& e) {
            if (error_handler_) {
                error_handler_(std::string("Request handler exception: ") + e.what());
            }
        }

        // Close stream after handler completes
        close_stream(stream_id);
    }

} // namespace kcenon::network::protocols::http2
