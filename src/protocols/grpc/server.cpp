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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <unordered_map>

namespace network_system::protocols::grpc
{

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
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_.load())
        {
            return error_void(
                error_codes::network_system::server_already_running,
                "Server is already running",
                "grpc::server");
        }

        if (port == 0)
        {
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

} // namespace network_system::protocols::grpc
