// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "internal/protocols/http2/http2_client.h"

#include "kcenon/network/detail/tracing/span.h"
#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/tracing_config.h"

#include <algorithm>
#include <thread>

namespace kcenon::network::protocols::http2
{
    // http2_response implementation
    auto http2_response::get_header(const std::string& name) const -> std::optional<std::string>
    {
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        for (const auto& header : headers)
        {
            std::string lower_header_name = header.name;
            std::transform(lower_header_name.begin(), lower_header_name.end(),
                           lower_header_name.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lower_header_name == lower_name)
            {
                return header.value;
            }
        }
        return std::nullopt;
    }

    auto http2_response::get_body_string() const -> std::string
    {
        return std::string(body.begin(), body.end());
    }

    // http2_client implementation
    http2_client::http2_client(std::string_view client_id)
        : client_id_(client_id)
        , encoder_(local_settings_.header_table_size)
        , decoder_(local_settings_.header_table_size)
    {
    }

    http2_client::~http2_client()
    {
        try
        {
            disconnect();
        }
        catch (...)
        {
            // Suppress exceptions in destructor
        }
    }

    auto http2_client::connect(const std::string& host, unsigned short port) -> VoidResult
    {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        // Create tracing span for connect operation
        auto span = tracing::is_tracing_enabled()
            ? std::make_optional(tracing::trace_context::create_span("http2.client.connect"))
            : std::nullopt;
        if (span)
        {
            span->set_attribute("net.peer.name", host)
                 .set_attribute("net.peer.port", static_cast<int64_t>(port))
                 .set_attribute("net.transport", "tcp")
                 .set_attribute("http.flavor", "2.0")
                 .set_attribute("client.id", client_id_);
        }

        if (is_connected_)
        {
            if (span)
            {
                span->set_error("Already connected");
            }
            return error_void(error_codes::common_errors::already_exists,
                              "Already connected", "http2_client::connect");
        }

        if (host.empty())
        {
            if (span)
            {
                span->set_error("Host cannot be empty");
            }
            return error_void(error_codes::common_errors::invalid_argument,
                              "Host cannot be empty", "http2_client::connect");
        }

        host_ = host;
        port_ = port;

        try
        {
            // A failed connection can still own a socket and work guard.
            // Release them while their original execution context is alive.
            stop_io();
            socket_.reset();
            work_guard_.reset();
            ssl_context_.reset();
            io_context_.reset();
            goaway_received_ = false;

            // Create I/O context
            io_context_ = std::make_unique<asio::io_context>();

            // Create SSL context with TLS 1.3
            ssl_context_ = std::make_unique<asio::ssl::context>(asio::ssl::context::tlsv13_client);

            // Set ALPN for HTTP/2
            SSL_CTX* native_ctx = ssl_context_->native_handle();
            static const unsigned char alpn_protos[] = { 2, 'h', '2' };
            SSL_CTX_set_alpn_protos(native_ctx, alpn_protos, sizeof(alpn_protos));

            // Verify peer certificate
            ssl_context_->set_default_verify_paths();
            ssl_context_->set_verify_mode(asio::ssl::verify_peer);

            // Create socket
            asio::ip::tcp::resolver resolver(*io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));

            socket_ = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket>>(
                *io_context_, *ssl_context_);

            // Set SNI hostname
            SSL_set_tlsext_host_name(socket_->native_handle(), host.c_str());

            // Connect TCP
            asio::connect(socket_->lowest_layer(), endpoints);

            // Perform SSL handshake
            socket_->handshake(asio::ssl::stream_base::client);

            // Verify ALPN negotiation
            const unsigned char* alpn_result = nullptr;
            unsigned int alpn_len = 0;
            SSL_get0_alpn_selected(socket_->native_handle(), &alpn_result, &alpn_len);

            if (alpn_len != 2 || alpn_result == nullptr ||
                std::string(reinterpret_cast<const char*>(alpn_result), alpn_len) != "h2")
            {
                socket_->lowest_layer().close();
                if (span)
                {
                    span->set_error("Server does not support HTTP/2 via ALPN");
                }
                return error_void(error_codes::network_system::connection_failed,
                                  "Server does not support HTTP/2 via ALPN",
                                  "http2_client::connect");
            }

            is_running_ = true;

            // Send connection preface
            auto preface_result = send_connection_preface();
            if (preface_result.is_err())
            {
                is_connected_ = false;
                is_running_ = false;
                if (span)
                {
                    span->set_error(preface_result.error().message);
                }
                return preface_result;
            }

            // Send initial SETTINGS
            auto settings_result = send_settings();
            if (settings_result.is_err())
            {
                is_connected_ = false;
                is_running_ = false;
                if (span)
                {
                    span->set_error(settings_result.error().message);
                }
                return settings_result;
            }

            // The server connection preface must start with a non-ACK
            // SETTINGS frame on stream zero (RFC 9113 section 3.4).
            // Bound both reads by one deadline so a partial frame cannot
            // leave connect() blocked indefinitely.
            const auto deadline = std::chrono::steady_clock::now() + timeout_.load();
            auto read_preface_bytes = [&](asio::mutable_buffer buffer) {
                asio::steady_timer timer(*io_context_, deadline);
                std::error_code read_error;
                bool expired = false;
                timer.async_wait([&](std::error_code ec) {
                    if (!ec) {
                        expired = true;
                        std::error_code ignored;
                        socket_->lowest_layer().cancel(ignored);
                    }
                });
                asio::async_read(*socket_, buffer,
                    [&](std::error_code ec, std::size_t) {
                        read_error = ec;
                        timer.cancel();
                    });
                io_context_->restart();
                io_context_->run();
                if (expired) throw std::runtime_error("Server SETTINGS timed out");
                if (read_error) throw std::system_error(read_error);
            };
            std::vector<uint8_t> initial_header(FRAME_HEADER_SIZE);
            read_preface_bytes(asio::buffer(initial_header));
            auto header = frame_header::parse(initial_header);
            if (header.is_err() || header.value().type != frame_type::settings ||
                header.value().stream_id != 0 ||
                (header.value().flags & frame_flags::ack) != 0 ||
                header.value().length > local_settings_.max_frame_size) {
                throw std::runtime_error("Invalid server SETTINGS preface");
            }
            std::vector<uint8_t> initial_payload(header.value().length);
            if (!initial_payload.empty()) read_preface_bytes(asio::buffer(initial_payload));
            initial_header.insert(initial_header.end(), initial_payload.begin(), initial_payload.end());
            auto initial_frame = frame::parse(initial_header);
            if (initial_frame.is_err()) throw std::runtime_error("Invalid server SETTINGS payload");
            auto initial_result = process_frame(std::move(initial_frame.value()));
            if (initial_result.is_err()) throw std::runtime_error(initial_result.error().message);

            // All subsequent SSL operations run on this one executor thread.
            // Publish the connected state while submissions are locked so no
            // caller writes synchronously between startup and the first read.
            work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
                asio::make_work_guard(*io_context_));
            io_context_->restart();
            {
                std::lock_guard<std::mutex> lock(io_submission_mutex_);
                io_active_ = true;
                is_connected_ = true;
                io_future_ = std::async(std::launch::async, [this]() { run_io(); });
            }

            if (span)
            {
                span->set_status(tracing::span_status::ok);
            }
            return ok();
        }
        catch (const std::exception& e)
        {
            is_connected_ = false;
            is_running_ = false;
            stop_io();
            if (span)
            {
                span->set_error(std::string("Connection failed: ") + e.what());
            }
            return error_void(error_codes::network_system::connection_failed,
                              std::string("Connection failed: ") + e.what(),
                              "http2_client::connect");
        }
    }

    auto http2_client::disconnect() -> VoidResult
    {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        if (!is_connected_)
        {
            stop_io();
            return ok();
        }

        try
        {
            // Send GOAWAY frame
            if (socket_ && socket_->lowest_layer().is_open())
            {
                goaway_frame goaway(next_stream_id_ - 2,
                                    static_cast<uint32_t>(error_code::no_error));
                send_frame(goaway);
            }
        }
        catch (...)
        {
            // Ignore errors during GOAWAY
        }

        stop_io();

        is_connected_ = false;

        return ok();
    }

    auto http2_client::is_connected() const -> bool
    {
        return is_connected_ && !goaway_received_;
    }

    auto http2_client::get(const std::string& path,
                           const std::vector<http_header>& headers)
        -> Result<http2_response>
    {
        return send_request("GET", path, headers, {});
    }

    auto http2_client::post(const std::string& path,
                            const std::string& body,
                            const std::vector<http_header>& headers)
        -> Result<http2_response>
    {
        std::vector<uint8_t> body_bytes(body.begin(), body.end());
        return send_request("POST", path, headers, body_bytes);
    }

    auto http2_client::post(const std::string& path,
                            const std::vector<uint8_t>& body,
                            const std::vector<http_header>& headers)
        -> Result<http2_response>
    {
        return send_request("POST", path, headers, body);
    }

    auto http2_client::put(const std::string& path,
                           const std::string& body,
                           const std::vector<http_header>& headers)
        -> Result<http2_response>
    {
        std::vector<uint8_t> body_bytes(body.begin(), body.end());
        return send_request("PUT", path, headers, body_bytes);
    }

    auto http2_client::del(const std::string& path,
                           const std::vector<http_header>& headers)
        -> Result<http2_response>
    {
        return send_request("DELETE", path, headers, {});
    }

    auto http2_client::set_timeout(std::chrono::milliseconds timeout) -> void
    {
        timeout_ = timeout;
    }

    auto http2_client::get_timeout() const -> std::chrono::milliseconds
    {
        return timeout_.load();
    }

    auto http2_client::get_settings() const -> http2_settings
    {
        return local_settings_;
    }

    auto http2_client::set_settings(const http2_settings& settings) -> void
    {
        local_settings_ = settings;
        encoder_.set_max_table_size(settings.header_table_size);
        decoder_.set_max_table_size(settings.header_table_size);
    }

    auto http2_client::start_stream(const std::string& path,
                                    const std::vector<http_header>& headers,
                                    std::function<void(std::vector<uint8_t>)> on_data,
                                    std::function<void(std::vector<http_header>)> on_headers,
                                    std::function<void(int)> on_complete)
        -> Result<uint32_t>
    {
        if (!is_connected())
        {
            return error<uint32_t>(error_codes::network_system::connection_closed,
                                   "Not connected",
                                   "http2_client::start_stream");
        }

        // Create stream with callbacks
        http2_stream& stream = create_stream();
        stream.state = stream_state::open;
        stream.is_streaming = true;
        stream.on_data = std::move(on_data);
        stream.on_headers = std::move(on_headers);
        stream.on_complete = std::move(on_complete);

        // Build headers for POST (streaming requests use POST)
        auto request_headers = build_headers("POST", path, headers);
        stream.request_headers = request_headers;

        // Encode headers with HPACK
        auto encoded_headers = encoder_.encode(request_headers);

        // Send HEADERS frame (not end_stream since we might send more data)
        headers_frame hf(stream.stream_id, std::move(encoded_headers), false, true);

        auto send_result = send_frame(hf);
        if (send_result.is_err())
        {
            close_stream(stream.stream_id);
            const auto& err = send_result.error();
            return error<uint32_t>(err.code, err.message,
                                   "http2_client::start_stream",
                                   get_error_details(err));
        }

        uint32_t result_id = stream.stream_id;
        return ok(std::move(result_id));
    }

    auto http2_client::write_stream(uint32_t stream_id,
                                    const std::vector<uint8_t>& data,
                                    bool end_stream) -> VoidResult
    {
        if (!is_connected())
        {
            return error_void(error_codes::network_system::connection_closed,
                              "Not connected",
                              "http2_client::write_stream");
        }

        auto* stream = get_stream(stream_id);
        if (!stream)
        {
            return error_void(error_codes::common_errors::not_found,
                              "Stream not found",
                              "http2_client::write_stream");
        }

        if (stream->state == stream_state::closed ||
            stream->state == stream_state::half_closed_local)
        {
            return error_void(error_codes::common_errors::internal_error,
                              "Stream not writable",
                              "http2_client::write_stream");
        }

        // Send DATA frame
        if (end_stream) stream->state = stream_state::half_closed_local;
        data_frame df(stream_id, std::vector<uint8_t>(data), end_stream);
        auto send_result = send_frame(df);
        if (send_result.is_err())
        {
            return send_result;
        }

        return ok();
    }

    auto http2_client::close_stream_writer(uint32_t stream_id) -> VoidResult
    {
        if (!is_connected())
        {
            return error_void(error_codes::network_system::connection_closed,
                              "Not connected",
                              "http2_client::close_stream_writer");
        }

        auto* stream = get_stream(stream_id);
        if (!stream)
        {
            return error_void(error_codes::common_errors::not_found,
                              "Stream not found",
                              "http2_client::close_stream_writer");
        }

        if (stream->state == stream_state::closed ||
            stream->state == stream_state::half_closed_local)
        {
            return ok();  // Already closed
        }

        // Publish the state before the peer can respond to END_STREAM.
        stream->state = stream_state::half_closed_local;
        // Send empty DATA frame with END_STREAM
        data_frame df(stream_id, {}, true);
        auto send_result = send_frame(df);
        if (send_result.is_err())
        {
            return send_result;
        }

        return ok();
    }

    auto http2_client::cancel_stream(uint32_t stream_id) -> VoidResult
    {
        if (!is_connected())
        {
            return error_void(error_codes::network_system::connection_closed,
                              "Not connected",
                              "http2_client::cancel_stream");
        }

        auto* stream = get_stream(stream_id);
        if (!stream)
        {
            return error_void(error_codes::common_errors::not_found,
                              "Stream not found",
                              "http2_client::cancel_stream");
        }

        if (stream->state == stream_state::closed)
        {
            return ok();  // Already closed
        }

        // Send RST_STREAM frame
        rst_stream_frame rsf(stream_id, static_cast<uint32_t>(error_code::cancel));
        auto send_result = send_frame(rsf);
        if (send_result.is_err())
        {
            return send_result;
        }

        stream->state = stream_state::closed;
        return ok();
    }

    // Private methods

    auto http2_client::send_connection_preface() -> VoidResult
    {
        try
        {
            asio::write(*socket_,
                        asio::buffer(CONNECTION_PREFACE.data(), CONNECTION_PREFACE.size()));
            return ok();
        }
        catch (const std::exception& e)
        {
            return error_void(error_codes::network_system::send_failed,
                              std::string("Failed to send connection preface: ") + e.what(),
                              "http2_client::send_connection_preface");
        }
    }

    auto http2_client::send_settings() -> VoidResult
    {
        std::vector<setting_parameter> params = {
            {static_cast<uint16_t>(setting_identifier::header_table_size),
             local_settings_.header_table_size},
            {static_cast<uint16_t>(setting_identifier::enable_push),
             local_settings_.enable_push ? 1u : 0u},
            {static_cast<uint16_t>(setting_identifier::max_concurrent_streams),
             local_settings_.max_concurrent_streams},
            {static_cast<uint16_t>(setting_identifier::initial_window_size),
             local_settings_.initial_window_size},
            {static_cast<uint16_t>(setting_identifier::max_frame_size),
             local_settings_.max_frame_size},
            {static_cast<uint16_t>(setting_identifier::max_header_list_size),
             local_settings_.max_header_list_size}
        };

        settings_frame settings(params, false);
        return send_frame(settings);
    }

    auto http2_client::handle_settings_frame(const settings_frame& frame) -> VoidResult
    {
        if (frame.is_ack())
        {
            // Our settings were acknowledged
            return ok();
        }

        // Apply remote settings
        for (const auto& param : frame.settings())
        {
            switch (static_cast<setting_identifier>(param.identifier))
            {
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

        return send_settings_ack();
    }

    auto http2_client::send_settings_ack() -> VoidResult
    {
        settings_frame ack({}, true);
        return send_frame(ack);
    }

    auto http2_client::send_frame(const frame& f) -> VoidResult
    {
        std::unique_lock<std::mutex> lock(io_submission_mutex_);
        if (!socket_ || !is_running_)
        {
            return error_void(error_codes::network_system::connection_closed,
                              "Connection closed", "http2_client::send_frame");
        }

        try
        {
            auto data = f.serialize();
            if (!io_active_)
            {
                // Connection preface only: the reader has not started yet.
                asio::write(*socket_, asio::buffer(data));
                return ok();
            }

            auto completion = std::make_shared<std::promise<VoidResult>>();
            auto result = completion->get_future();
            const bool on_io_thread = io_context_->get_executor().running_in_this_thread();
            asio::post(*io_context_, [this, data = std::move(data), completion]() mutable {
                if (!is_running_)
                {
                    completion->set_value(error_void(
                        error_codes::network_system::connection_closed,
                        "Connection closed", "http2_client::send_frame"));
                    return;
                }
                const bool idle = pending_writes_.empty();
                pending_writes_.push_back({std::move(data), completion});
                if (idle) write_next_frame();
            });
            lock.unlock();
            // Frame handlers enqueue protocol replies without waiting on their
            // own executor. Public callers retain synchronous send results.
            return on_io_thread ? ok() : result.get();
        }
        catch (const std::exception& e)
        {
            return error_void(error_codes::network_system::send_failed,
                              std::string("Failed to send frame: ") + e.what(),
                              "http2_client::send_frame");
        }
    }

    auto http2_client::write_next_frame() -> void
    {
        if (pending_writes_.empty()) return;
        asio::async_write(*socket_, asio::buffer(pending_writes_.front().data),
            [this](std::error_code ec, std::size_t) {
                auto completion = std::move(pending_writes_.front().completion);
                pending_writes_.pop_front();
                if (ec)
                {
                    is_connected_ = false;
                    completion->set_value(error_void(
                        error_codes::network_system::send_failed, ec.message(),
                        "http2_client::send_frame"));
                    while (!pending_writes_.empty())
                    {
                        pending_writes_.front().completion->set_value(error_void(
                            error_codes::network_system::send_failed, ec.message(),
                            "http2_client::send_frame"));
                        pending_writes_.pop_front();
                    }
                    return;
                }
                completion->set_value(ok());
                write_next_frame();
            });
    }

    auto http2_client::read_next_frame() -> void
    {
        if (!is_running_) return;
        auto bytes = std::make_shared<std::vector<uint8_t>>(FRAME_HEADER_SIZE);
        asio::async_read(*socket_, asio::buffer(*bytes),
            [this, bytes](std::error_code ec, std::size_t) {
                if (ec || !is_running_)
                {
                    if (ec) is_connected_ = false;
                    return;
                }
                auto header = frame_header::parse(*bytes);
                if (header.is_err())
                {
                    is_connected_ = false;
                    return;
                }
                const auto length = header.value().length;
                bytes->resize(FRAME_HEADER_SIZE + length);
                auto complete = [this, bytes](std::error_code payload_error, std::size_t) {
                    if (payload_error || !is_running_)
                    {
                        if (payload_error) is_connected_ = false;
                        return;
                    }
                    try
                    {
                        auto parsed = frame::parse(*bytes);
                        if (parsed.is_err())
                        {
                            is_connected_ = false;
                            return;
                        }
                        process_frame(std::move(parsed.value()));
                        read_next_frame();
                    }
                    catch (...)
                    {
                        is_connected_ = false;
                    }
                };
                if (length == 0)
                    complete({}, 0);
                else
                    asio::async_read(*socket_,
                        asio::buffer(bytes->data() + FRAME_HEADER_SIZE, length),
                        std::move(complete));
            });
    }

    auto http2_client::process_frame(std::unique_ptr<frame> f) -> VoidResult
    {
        if (!f)
        {
            return error_void(error_codes::common_errors::invalid_argument,
                              "Null frame", "http2_client::process_frame");
        }

        switch (f->header().type)
        {
        case frame_type::settings:
            if (auto* sf = dynamic_cast<settings_frame*>(f.get()))
            {
                return handle_settings_frame(*sf);
            }
            break;

        case frame_type::headers:
            if (auto* hf = dynamic_cast<headers_frame*>(f.get()))
            {
                return handle_headers_frame(*hf);
            }
            break;

        case frame_type::data:
            if (auto* df = dynamic_cast<data_frame*>(f.get()))
            {
                return handle_data_frame(*df);
            }
            break;

        case frame_type::rst_stream:
            if (auto* rf = dynamic_cast<rst_stream_frame*>(f.get()))
            {
                return handle_rst_stream_frame(*rf);
            }
            break;

        case frame_type::goaway:
            if (auto* gf = dynamic_cast<goaway_frame*>(f.get()))
            {
                return handle_goaway_frame(*gf);
            }
            break;

        case frame_type::window_update:
            if (auto* wf = dynamic_cast<window_update_frame*>(f.get()))
            {
                return handle_window_update_frame(*wf);
            }
            break;

        case frame_type::ping:
            if (auto* pf = dynamic_cast<ping_frame*>(f.get()))
            {
                return handle_ping_frame(*pf);
            }
            break;

        default:
            // Ignore unknown frame types
            break;
        }

        return ok();
    }

    auto http2_client::allocate_stream_id() -> uint32_t
    {
        uint32_t id = next_stream_id_.fetch_add(2);
        return id;
    }

    auto http2_client::get_stream(uint32_t stream_id) -> http2_stream*
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    auto http2_client::create_stream() -> http2_stream&
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        uint32_t stream_id = allocate_stream_id();
        auto& stream = streams_[stream_id];
        stream.stream_id = stream_id;
        stream.state = stream_state::idle;
        stream.window_size = remote_settings_.initial_window_size;
        return stream;
    }

    auto http2_client::close_stream(uint32_t stream_id) -> void
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end())
        {
            it->second.state = stream_state::closed;
        }
    }

    auto http2_client::send_request(const std::string& method,
                                    const std::string& path,
                                    const std::vector<http_header>& headers,
                                    const std::vector<uint8_t>& body)
        -> Result<http2_response>
    {
        // Create tracing span for HTTP request
        auto span = tracing::is_tracing_enabled()
            ? std::make_optional(tracing::trace_context::create_span("http2.request"))
            : std::nullopt;
        if (span)
        {
            span->set_attribute("http.method", method)
                 .set_attribute("http.url", "https://" + host_ + ":" + std::to_string(port_) + path)
                 .set_attribute("http.flavor", "2.0")
                 .set_attribute("http.request.body_size", static_cast<int64_t>(body.size()))
                 .set_attribute("net.peer.name", host_)
                 .set_attribute("net.peer.port", static_cast<int64_t>(port_));
        }

        if (!is_connected())
        {
            if (span)
            {
                span->set_error("Not connected");
            }
            return error<http2_response>(error_codes::network_system::connection_closed,
                                         "Not connected",
                                         "http2_client::send_request");
        }

        // Create stream
        http2_stream& stream = create_stream();
        stream.state = stream_state::open;

        if (span)
        {
            span->set_attribute("http2.stream_id", static_cast<int64_t>(stream.stream_id));
        }

        // Build headers
        auto request_headers = build_headers(method, path, headers);
        stream.request_headers = request_headers;
        stream.request_body = body;

        // Encode headers with HPACK
        auto encoded_headers = encoder_.encode(request_headers);

        // Send HEADERS frame
        bool end_stream = body.empty();
        if (end_stream) stream.state = stream_state::half_closed_local;
        headers_frame hf(stream.stream_id, std::move(encoded_headers), end_stream, true);

        auto send_result = send_frame(hf);
        if (send_result.is_err())
        {
            close_stream(stream.stream_id);
            const auto& err = send_result.error();
            if (span)
            {
                span->set_error(err.message);
            }
            return error<http2_response>(err.code, err.message,
                                         "http2_client::send_request",
                                         get_error_details(err));
        }

        // Send DATA frame if body exists
        if (!body.empty())
        {
            stream.state = stream_state::half_closed_local;
            data_frame df(stream.stream_id, std::vector<uint8_t>(body), true);
            send_result = send_frame(df);
            if (send_result.is_err())
            {
                close_stream(stream.stream_id);
                const auto& err = send_result.error();
                if (span)
                {
                    span->set_error(err.message);
                }
                return error<http2_response>(err.code, err.message,
                                             "http2_client::send_request",
                                             get_error_details(err));
            }
        }

        // Wait for response with timeout
        auto future = stream.promise.get_future();
        auto status = future.wait_for(timeout_.load());

        if (status == std::future_status::timeout)
        {
            close_stream(stream.stream_id);
            if (span)
            {
                span->set_error("Request timeout");
            }
            return error<http2_response>(error_codes::common_errors::timeout,
                                         "Request timeout",
                                         "http2_client::send_request");
        }

        auto response = future.get();
        if (span)
        {
            span->set_attribute("http.status_code", static_cast<int64_t>(response.status_code))
                 .set_attribute("http.response.body_size", static_cast<int64_t>(response.body.size()));
            if (response.status_code >= 400)
            {
                span->set_status(tracing::span_status::error, "HTTP " + std::to_string(response.status_code));
            }
            else
            {
                span->set_status(tracing::span_status::ok);
            }
        }
        return ok(std::move(response));
    }

    auto http2_client::build_headers(const std::string& method,
                                     const std::string& path,
                                     const std::vector<http_header>& additional)
        -> std::vector<http_header>
    {
        std::vector<http_header> headers;

        // Pseudo-headers (must come first)
        headers.emplace_back(":method", method);
        headers.emplace_back(":scheme", "https");
        headers.emplace_back(":authority", host_);
        headers.emplace_back(":path", path.empty() ? "/" : path);

        // Standard headers
        headers.emplace_back("user-agent", "network_system/http2_client");

        // Additional headers
        for (const auto& h : additional)
        {
            // Skip pseudo-headers in additional
            if (!h.name.empty() && h.name[0] != ':')
            {
                headers.push_back(h);
            }
        }

        return headers;
    }

    auto http2_client::handle_headers_frame(const headers_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;
        auto* stream = get_stream(stream_id);

        if (!stream)
        {
            return error_void(error_codes::common_errors::not_found,
                              "Unknown stream ID",
                              "http2_client::handle_headers_frame");
        }

        // Decode headers
        auto decode_result = decoder_.decode(f.header_block());
        if (decode_result.is_err())
        {
            const auto& err = decode_result.error();
            return error_void(err.code, err.message,
                              "http2_client::handle_headers_frame",
                              get_error_details(err));
        }

        auto& decoded_headers = decode_result.value();

        // Append to stream headers
        stream->response_headers.insert(stream->response_headers.end(),
                                        decoded_headers.begin(),
                                        decoded_headers.end());

        if (f.is_end_headers())
        {
            stream->headers_complete = true;

            // Call streaming callback if set
            if (stream->is_streaming && stream->on_headers)
            {
                stream->on_headers(stream->response_headers);
            }
        }

        if (f.is_end_stream())
        {
            stream->body_complete = true;
            stream->state = stream_state::closed;

            // Extract status code
            int status_code = 0;
            for (const auto& h : stream->response_headers)
            {
                if (h.name == ":status")
                {
                    try
                    {
                        status_code = std::stoi(h.value);
                    }
                    catch (...)
                    {
                        status_code = 0;
                    }
                    break;
                }
            }

            // Handle streaming vs non-streaming
            if (stream->is_streaming)
            {
                if (stream->on_complete)
                {
                    stream->on_complete(status_code);
                }
            }
            else
            {
                // Build response for non-streaming
                http2_response response;
                response.headers = stream->response_headers;
                response.body = stream->response_body;
                response.status_code = status_code;
                stream->promise.set_value(std::move(response));
            }
        }

        return ok();
    }

    auto http2_client::handle_data_frame(const data_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;
        auto* stream = get_stream(stream_id);

        if (!stream)
        {
            return error_void(error_codes::common_errors::not_found,
                              "Unknown stream ID",
                              "http2_client::handle_data_frame");
        }

        // Get data
        auto data = f.data();

        // For streaming, call callback immediately; otherwise buffer
        if (stream->is_streaming && stream->on_data)
        {
            stream->on_data(std::vector<uint8_t>(data.begin(), data.end()));
        }
        else
        {
            stream->response_body.insert(stream->response_body.end(),
                                         data.begin(), data.end());
        }

        // Update flow control window
        stream->window_size -= static_cast<int32_t>(data.size());
        connection_window_size_ -= static_cast<int32_t>(data.size());

        // Send WINDOW_UPDATE if needed
        if (stream->window_size < static_cast<int32_t>(DEFAULT_WINDOW_SIZE / 2))
        {
            int32_t increment = DEFAULT_WINDOW_SIZE - stream->window_size;
            window_update_frame wuf(stream_id, increment);
            send_frame(wuf);
            stream->window_size += increment;
        }

        if (connection_window_size_ < static_cast<int32_t>(DEFAULT_WINDOW_SIZE / 2))
        {
            int32_t increment = DEFAULT_WINDOW_SIZE - connection_window_size_;
            window_update_frame wuf(0, increment);  // Stream 0 = connection level
            send_frame(wuf);
            connection_window_size_ += increment;
        }

        if (f.is_end_stream())
        {
            stream->body_complete = true;
            stream->state = stream_state::closed;

            // Extract status code
            int status_code = 0;
            for (const auto& h : stream->response_headers)
            {
                if (h.name == ":status")
                {
                    try
                    {
                        status_code = std::stoi(h.value);
                    }
                    catch (...)
                    {
                        status_code = 0;
                    }
                    break;
                }
            }

            // Handle streaming vs non-streaming
            if (stream->is_streaming)
            {
                if (stream->on_complete)
                {
                    stream->on_complete(status_code);
                }
            }
            else
            {
                http2_response response;
                response.headers = stream->response_headers;
                response.body = stream->response_body;
                response.status_code = status_code;
                stream->promise.set_value(std::move(response));
            }
        }

        return ok();
    }

    auto http2_client::handle_rst_stream_frame(const rst_stream_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;
        auto* stream = get_stream(stream_id);

        if (stream)
        {
            stream->state = stream_state::closed;

            // Set error response
            http2_response response;
            response.status_code = 0;  // Indicate error
            stream->promise.set_value(std::move(response));
        }

        return ok();
    }

    auto http2_client::handle_goaway_frame(const goaway_frame& f) -> VoidResult
    {
        goaway_received_ = true;

        // Close all streams with ID > last_stream_id
        uint32_t last_stream = f.last_stream_id();
        {
            std::lock_guard<std::mutex> lock(streams_mutex_);
            for (auto& [id, stream] : streams_)
            {
                if (id > last_stream && stream.state != stream_state::closed)
                {
                    stream.state = stream_state::closed;
                    http2_response response;
                    response.status_code = 0;
                    stream.promise.set_value(std::move(response));
                }
            }
        }

        return ok();
    }

    auto http2_client::handle_window_update_frame(const window_update_frame& f) -> VoidResult
    {
        uint32_t stream_id = f.header().stream_id;
        int32_t increment = static_cast<int32_t>(f.window_size_increment());

        if (stream_id == 0)
        {
            // Connection-level window update
            connection_window_size_ += increment;
        }
        else
        {
            auto* stream = get_stream(stream_id);
            if (stream)
            {
                stream->window_size += increment;
            }
        }

        return ok();
    }

    auto http2_client::handle_ping_frame(const ping_frame& f) -> VoidResult
    {
        if (!f.is_ack())
        {
            // Send PING ACK
            ping_frame ack(f.opaque_data(), true);
            return send_frame(ack);
        }
        return ok();
    }

    auto http2_client::run_io() -> void
    {
        read_next_frame();
        io_context_->run();
    }

    auto http2_client::stop_io() -> void
    {
        {
            std::lock_guard<std::mutex> lock(io_submission_mutex_);
            is_running_ = false;
            is_connected_ = false;
            if (io_active_ && io_future_.valid())
            {
                // Cancel on the same executor as the SSL operations. Let run()
                // drain their handlers so every waiting writer is completed.
                asio::post(*io_context_, [this]() {
                    std::error_code ec;
                    socket_->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                    socket_->lowest_layer().close(ec);
                    work_guard_->reset();
                });
            }
            else
            {
                if (socket_)
                {
                    std::error_code ec;
                    socket_->lowest_layer().close(ec);
                }
                if (work_guard_) work_guard_->reset();
            }
        }

        if (io_future_.valid()) io_future_.wait();
        std::lock_guard<std::mutex> lock(io_submission_mutex_);
        io_future_ = {};
        io_active_ = false;
        if (io_context_) io_context_->stop();
    }

} // namespace kcenon::network::protocols::http2
