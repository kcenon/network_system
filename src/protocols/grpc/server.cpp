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

#include "kcenon/network/protocols/grpc/server.h"

#include "kcenon/network/tracing/span.h"
#include "kcenon/network/tracing/trace_context.h"
#include "kcenon/network/tracing/tracing_config.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

#if NETWORK_GRPC_OFFICIAL
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#endif

namespace kcenon::network::protocols::grpc
{

#if NETWORK_GRPC_OFFICIAL

// ============================================================================
// Official gRPC Library Implementation
// ============================================================================

// Generic service that adapts our handler-based API to gRPC's service model
class generic_service_impl : public ::grpc::Service
{
public:
    using unary_method_handler = std::function<
        ::grpc::Status(::grpc::ServerContext*, const ::grpc::ByteBuffer*, ::grpc::ByteBuffer*)>;

    auto add_method(const std::string& method_name, unary_method_handler handler) -> void
    {
        // Register method with gRPC
        auto* method = AddMethod(new ::grpc::internal::RpcServiceMethod(
            method_name.c_str(),
            ::grpc::internal::RpcMethod::NORMAL_RPC,
            nullptr));

        handlers_[method_name] = std::move(handler);
    }

private:
    std::unordered_map<std::string, unary_method_handler> handlers_;
};

// Server context adapter
class official_server_context : public server_context
{
public:
    explicit official_server_context(::grpc::ServerContext* ctx)
        : ctx_(ctx)
    {
        // Copy client metadata
        for (const auto& md : ctx_->client_metadata())
        {
            metadata_.emplace_back(
                std::string(md.first.data(), md.first.size()),
                std::string(md.second.data(), md.second.size()));
        }
    }

    auto client_metadata() const -> const grpc_metadata& override
    {
        return metadata_;
    }

    auto add_trailing_metadata(const std::string& key,
                               const std::string& value) -> void override
    {
        ctx_->AddTrailingMetadata(key, value);
    }

    auto set_trailing_metadata(grpc_metadata metadata) -> void override
    {
        for (const auto& [key, value] : metadata)
        {
            ctx_->AddTrailingMetadata(key, value);
        }
    }

    auto is_cancelled() const -> bool override
    {
        return ctx_->IsCancelled();
    }

    auto deadline() const
        -> std::optional<std::chrono::system_clock::time_point> override
    {
        auto deadline = ctx_->deadline();
        if (deadline == std::chrono::system_clock::time_point::max())
        {
            return std::nullopt;
        }
        return deadline;
    }

    auto peer() const -> std::string override
    {
        return ctx_->peer();
    }

    auto auth_context() const -> std::string override
    {
        auto auth = ctx_->auth_context();
        if (auth)
        {
            auto peer_identity = auth->GetPeerIdentity();
            if (!peer_identity.empty())
            {
                return std::string(peer_identity[0].data(), peer_identity[0].size());
            }
        }
        return "";
    }

private:
    ::grpc::ServerContext* ctx_;
    grpc_metadata metadata_;
};

// Implementation class using official gRPC
class grpc_server::impl
{
public:
    explicit impl(grpc_server_config config)
        : config_(std::move(config))
        , running_(false)
        , port_(0)
    {
    }

    ~impl()
    {
        stop();
    }

    auto start(uint16_t port) -> VoidResult
    {
        auto span = tracing::is_tracing_enabled()
            ? std::make_optional(tracing::trace_context::create_span("grpc.server.start"))
            : std::nullopt;
        if (span)
        {
            span->set_attribute("rpc.system", "grpc")
                 .set_attribute("net.host.port", static_cast<int64_t>(port))
                 .set_attribute("rpc.grpc.use_tls", false);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (running_.load())
        {
            if (span)
            {
                span->set_error("Server is already running");
            }
            return error_void(
                error_codes::network_system::server_already_running,
                "Server is already running",
                "grpc::server");
        }

        if (port == 0)
        {
            if (span)
            {
                span->set_error("Invalid port number");
            }
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid port number",
                "grpc::server");
        }

        ::grpc::ServerBuilder builder;

        // Configure server address
        std::string server_address = "0.0.0.0:" + std::to_string(port);
        builder.AddListeningPort(server_address, ::grpc::InsecureServerCredentials(), &bound_port_);

        // Apply configuration
        builder.SetMaxReceiveMessageSize(static_cast<int>(config_.max_message_size));
        builder.SetMaxSendMessageSize(static_cast<int>(config_.max_message_size));

        if (config_.max_concurrent_streams > 0)
        {
            builder.AddChannelArgument(GRPC_ARG_MAX_CONCURRENT_STREAMS,
                                       static_cast<int>(config_.max_concurrent_streams));
        }

        // Enable reflection for debugging
        ::grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ::grpc::EnableDefaultHealthCheckService(true);

        // Build and start the server
        server_ = builder.BuildAndStart();

        if (!server_)
        {
            if (span)
            {
                span->set_error("Failed to start gRPC server");
            }
            return error_void(
                error_codes::common_errors::internal_error,
                "Failed to start gRPC server",
                "grpc::server");
        }

        port_ = static_cast<uint16_t>(bound_port_);
        running_.store(true);

        // Start server thread
        server_thread_ = std::thread([this]() {
            server_->Wait();
        });

        if (span)
        {
            span->set_attribute("net.host.port.bound", static_cast<int64_t>(bound_port_));
        }

        return ok();
    }

    auto start_tls(uint16_t port,
                   const std::string& cert_path,
                   const std::string& key_path,
                   const std::string& ca_path) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_.load())
        {
            return error_void(
                error_codes::network_system::server_already_running,
                "Server is already running",
                "grpc::server");
        }

        if (cert_path.empty() || key_path.empty())
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Certificate and key paths are required for TLS",
                "grpc::server");
        }

        // Read certificate files
        std::string cert_contents, key_contents, ca_contents;

        {
            std::ifstream cert_file(cert_path);
            if (!cert_file)
            {
                return error_void(
                    error_codes::common_errors::invalid_argument,
                    "Failed to read certificate file",
                    "grpc::server",
                    cert_path);
            }
            cert_contents = std::string(
                std::istreambuf_iterator<char>(cert_file),
                std::istreambuf_iterator<char>());
        }

        {
            std::ifstream key_file(key_path);
            if (!key_file)
            {
                return error_void(
                    error_codes::common_errors::invalid_argument,
                    "Failed to read key file",
                    "grpc::server",
                    key_path);
            }
            key_contents = std::string(
                std::istreambuf_iterator<char>(key_file),
                std::istreambuf_iterator<char>());
        }

        if (!ca_path.empty())
        {
            std::ifstream ca_file(ca_path);
            if (ca_file)
            {
                ca_contents = std::string(
                    std::istreambuf_iterator<char>(ca_file),
                    std::istreambuf_iterator<char>());
            }
        }

        // Create SSL credentials
        ::grpc::SslServerCredentialsOptions ssl_opts;
        ssl_opts.pem_key_cert_pairs.push_back({key_contents, cert_contents});

        if (!ca_contents.empty())
        {
            ssl_opts.pem_root_certs = ca_contents;
            ssl_opts.client_certificate_request =
                GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;
        }

        auto creds = ::grpc::SslServerCredentials(ssl_opts);

        ::grpc::ServerBuilder builder;
        std::string server_address = "0.0.0.0:" + std::to_string(port);
        builder.AddListeningPort(server_address, creds, &bound_port_);

        // Apply configuration
        builder.SetMaxReceiveMessageSize(static_cast<int>(config_.max_message_size));
        builder.SetMaxSendMessageSize(static_cast<int>(config_.max_message_size));

        // Enable reflection
        ::grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        ::grpc::EnableDefaultHealthCheckService(true);

        server_ = builder.BuildAndStart();

        if (!server_)
        {
            return error_void(
                error_codes::common_errors::internal_error,
                "Failed to start gRPC TLS server",
                "grpc::server");
        }

        port_ = static_cast<uint16_t>(bound_port_);
        running_.store(true);

        server_thread_ = std::thread([this]() {
            server_->Wait();
        });

        return ok();
    }

    auto stop() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_.load())
        {
            return;
        }

        if (server_)
        {
            server_->Shutdown();
        }

        if (server_thread_.joinable())
        {
            server_thread_.join();
        }

        running_.store(false);
        port_ = 0;
        stop_cv_.notify_all();
    }

    auto wait() -> void
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_cv_.wait(lock, [this] { return !running_.load(); });
    }

    auto is_running() const -> bool
    {
        return running_.load();
    }

    auto port() const -> uint16_t
    {
        return port_;
    }

    auto register_service(grpc_service* service) -> VoidResult
    {
        if (service == nullptr)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Service cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        services_.push_back(service);

        return ok();
    }

    auto register_unary_method(const std::string& full_method_name,
                               unary_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server",
                "Method name must start with '/'");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server",
                full_method_name);
        }

        method_handler mh;
        mh.type = method_type::unary;
        mh.unary = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

    auto register_server_streaming_method(const std::string& full_method_name,
                                          server_streaming_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server");
        }

        method_handler mh;
        mh.type = method_type::server_streaming;
        mh.server_streaming = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

    auto register_client_streaming_method(const std::string& full_method_name,
                                          client_streaming_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server");
        }

        method_handler mh;
        mh.type = method_type::client_streaming;
        mh.client_streaming = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

    auto register_bidi_streaming_method(const std::string& full_method_name,
                                        bidi_streaming_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server");
        }

        method_handler mh;
        mh.type = method_type::bidi_streaming;
        mh.bidi_streaming = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

private:
    // Method type enumeration (same as prototype)
    enum class method_type
    {
        unary,
        server_streaming,
        client_streaming,
        bidi_streaming
    };

    struct method_handler
    {
        method_type type;
        unary_handler unary;
        server_streaming_handler server_streaming;
        client_streaming_handler client_streaming;
        bidi_streaming_handler bidi_streaming;
    };

    grpc_server_config config_;
    std::atomic<bool> running_;
    uint16_t port_;
    int bound_port_ = 0;

    std::unique_ptr<::grpc::Server> server_;
    std::thread server_thread_;

    std::vector<grpc_service*> services_;
    std::unordered_map<std::string, method_handler> methods_;

    mutable std::mutex mutex_;
    std::condition_variable stop_cv_;
};

#else // !NETWORK_GRPC_OFFICIAL

// ============================================================================
// Prototype Implementation (existing code)
// ============================================================================

// Method type enumeration
enum class method_type
{
    unary,
    server_streaming,
    client_streaming,
    bidi_streaming
};

// Method handler variant
struct method_handler
{
    method_type type;
    unary_handler unary;
    server_streaming_handler server_streaming;
    client_streaming_handler client_streaming;
    bidi_streaming_handler bidi_streaming;
};

// Implementation class (PIMPL pattern)
class grpc_server::impl
{
public:
    explicit impl(grpc_server_config config)
        : config_(std::move(config))
        , running_(false)
        , port_(0)
    {
    }

    ~impl()
    {
        stop();
    }

    auto start(uint16_t port) -> VoidResult
    {
        auto span = tracing::is_tracing_enabled()
            ? std::make_optional(tracing::trace_context::create_span("grpc.server.start"))
            : std::nullopt;
        if (span)
        {
            span->set_attribute("rpc.system", "grpc")
                 .set_attribute("net.host.port", static_cast<int64_t>(port))
                 .set_attribute("rpc.grpc.use_tls", false);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (running_.load())
        {
            if (span)
            {
                span->set_error("Server is already running");
            }
            return error_void(
                error_codes::network_system::server_already_running,
                "Server is already running",
                "grpc::server");
        }

        if (port == 0)
        {
            if (span)
            {
                span->set_error("Invalid port number");
            }
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid port number",
                "grpc::server");
        }

        // In a full implementation, this would:
        // 1. Create HTTP/2 server socket
        // 2. Start accepting connections
        // 3. Spawn worker threads

        port_ = port;
        running_.store(true);

        return ok();
    }

    auto start_tls(uint16_t port,
                   const std::string& cert_path,
                   const std::string& key_path,
                   const std::string& ca_path) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_.load())
        {
            return error_void(
                error_codes::network_system::server_already_running,
                "Server is already running",
                "grpc::server");
        }

        if (cert_path.empty() || key_path.empty())
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Certificate and key paths are required for TLS",
                "grpc::server");
        }

        // In a full implementation, this would configure TLS context
        cert_path_ = cert_path;
        key_path_ = key_path;
        ca_path_ = ca_path;

        port_ = port;
        running_.store(true);

        return ok();
    }

    auto stop() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_.load())
        {
            return;
        }

        running_.store(false);
        port_ = 0;

        // Notify waiting threads
        stop_cv_.notify_all();
    }

    auto wait() -> void
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_cv_.wait(lock, [this] { return !running_.load(); });
    }

    auto is_running() const -> bool
    {
        return running_.load();
    }

    auto port() const -> uint16_t
    {
        return port_;
    }

    auto register_service(grpc_service* service) -> VoidResult
    {
        if (service == nullptr)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Service cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        services_.push_back(service);

        return ok();
    }

    auto register_unary_method(const std::string& full_method_name,
                               unary_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server",
                "Method name must start with '/'");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server",
                full_method_name);
        }

        method_handler mh;
        mh.type = method_type::unary;
        mh.unary = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

    auto register_server_streaming_method(const std::string& full_method_name,
                                          server_streaming_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server");
        }

        method_handler mh;
        mh.type = method_type::server_streaming;
        mh.server_streaming = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

    auto register_client_streaming_method(const std::string& full_method_name,
                                          client_streaming_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server");
        }

        method_handler mh;
        mh.type = method_type::client_streaming;
        mh.client_streaming = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

    auto register_bidi_streaming_method(const std::string& full_method_name,
                                        bidi_streaming_handler handler) -> VoidResult
    {
        if (full_method_name.empty() || full_method_name[0] != '/')
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid method name format",
                "grpc::server");
        }

        if (!handler)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Handler cannot be null",
                "grpc::server");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (methods_.find(full_method_name) != methods_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered",
                "grpc::server");
        }

        method_handler mh;
        mh.type = method_type::bidi_streaming;
        mh.bidi_streaming = std::move(handler);
        methods_[full_method_name] = std::move(mh);

        return ok();
    }

private:
    grpc_server_config config_;
    std::atomic<bool> running_;
    uint16_t port_;

    std::string cert_path_;
    std::string key_path_;
    std::string ca_path_;

    std::vector<grpc_service*> services_;
    std::unordered_map<std::string, method_handler> methods_;

    mutable std::mutex mutex_;
    std::condition_variable stop_cv_;
};

#endif // !NETWORK_GRPC_OFFICIAL

// grpc_server implementation

grpc_server::grpc_server(const grpc_server_config& config)
    : impl_(std::make_unique<impl>(config))
{
}

grpc_server::~grpc_server() = default;

grpc_server::grpc_server(grpc_server&&) noexcept = default;
grpc_server& grpc_server::operator=(grpc_server&&) noexcept = default;

auto grpc_server::start(uint16_t port) -> VoidResult
{
    return impl_->start(port);
}

auto grpc_server::start_tls(uint16_t port,
                            const std::string& cert_path,
                            const std::string& key_path,
                            const std::string& ca_path) -> VoidResult
{
    return impl_->start_tls(port, cert_path, key_path, ca_path);
}

auto grpc_server::stop() -> void
{
    impl_->stop();
}

auto grpc_server::wait() -> void
{
    impl_->wait();
}

auto grpc_server::is_running() const -> bool
{
    return impl_->is_running();
}

auto grpc_server::port() const -> uint16_t
{
    return impl_->port();
}

auto grpc_server::register_service(grpc_service* service) -> VoidResult
{
    return impl_->register_service(service);
}

auto grpc_server::register_unary_method(const std::string& full_method_name,
                                        unary_handler handler) -> VoidResult
{
    return impl_->register_unary_method(full_method_name, std::move(handler));
}

auto grpc_server::register_server_streaming_method(const std::string& full_method_name,
                                                   server_streaming_handler handler) -> VoidResult
{
    return impl_->register_server_streaming_method(full_method_name, std::move(handler));
}

auto grpc_server::register_client_streaming_method(const std::string& full_method_name,
                                                   client_streaming_handler handler) -> VoidResult
{
    return impl_->register_client_streaming_method(full_method_name, std::move(handler));
}

auto grpc_server::register_bidi_streaming_method(const std::string& full_method_name,
                                                 bidi_streaming_handler handler) -> VoidResult
{
    return impl_->register_bidi_streaming_method(full_method_name, std::move(handler));
}

} // namespace kcenon::network::protocols::grpc
