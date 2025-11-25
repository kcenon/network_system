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

#include "kcenon/network/protocols/grpc/client.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace network_system::protocols::grpc
{

// Implementation class (PIMPL pattern)
class grpc_client::impl
{
public:
    explicit impl(std::string target, grpc_channel_config config)
        : target_(std::move(target))
        , config_(std::move(config))
        , connected_(false)
    {
    }

    ~impl()
    {
        disconnect();
    }

    auto connect() -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connected_.load())
        {
            return ok();
        }

        // Parse target address
        auto colon_pos = target_.find(':');
        if (colon_pos == std::string::npos)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid target address format",
                "grpc::client",
                "Expected format: host:port");
        }

        host_ = target_.substr(0, colon_pos);
        port_ = target_.substr(colon_pos + 1);

        // In a full implementation, this would establish HTTP/2 connection
        // For this prototype, we just mark as connected
        connected_.store(true);

        return ok();
    }

    auto disconnect() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_.store(false);
    }

    auto is_connected() const -> bool
    {
        return connected_.load();
    }

    auto wait_for_connected(std::chrono::milliseconds timeout) -> bool
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (connected_.load())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return connected_.load();
    }

    auto target() const -> const std::string&
    {
        return target_;
    }

    auto call_raw(const std::string& method,
                  const std::vector<uint8_t>& request,
                  const call_options& options) -> Result<grpc_message>
    {
        if (!connected_.load())
        {
            return error<grpc_message>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        // Validate method format
        if (method.empty() || method[0] != '/')
        {
            return error<grpc_message>(
                error_codes::common_errors::invalid_argument,
                "Invalid method format",
                "grpc::client",
                "Method must start with '/'");
        }

        // Check deadline
        if (options.deadline.has_value())
        {
            if (std::chrono::system_clock::now() > options.deadline.value())
            {
                return error<grpc_message>(
                    static_cast<int>(status_code::deadline_exceeded),
                    "Deadline exceeded before call started",
                    "grpc::client");
            }
        }

        // Prototype: Return unimplemented status
        // In a full implementation, this would:
        // 1. Create HTTP/2 request with gRPC headers
        // 2. Send the request with gRPC framing
        // 3. Wait for response
        // 4. Parse gRPC response and trailers

        return error<grpc_message>(
            static_cast<int>(status_code::unimplemented),
            "gRPC client call not yet implemented",
            "grpc::client",
            "This is a prototype - use official gRPC library for production");
    }

    auto call_raw_async(const std::string& method,
                        const std::vector<uint8_t>& request,
                        std::function<void(Result<grpc_message>)> callback,
                        const call_options& options) -> void
    {
        // In prototype, call synchronously in current thread
        auto result = call_raw(method, request, options);
        if (callback)
        {
            callback(std::move(result));
        }
    }

    auto server_stream_raw(const std::string& method,
                           const std::vector<uint8_t>& request,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::server_stream_reader>>
    {
        if (!connected_.load())
        {
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::server_stream_reader>>(
            static_cast<int>(status_code::unimplemented),
            "Server streaming not yet implemented",
            "grpc::client");
    }

    auto client_stream_raw(const std::string& method,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::client_stream_writer>>
    {
        if (!connected_.load())
        {
            return error<std::unique_ptr<grpc_client::client_stream_writer>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::client_stream_writer>>(
            static_cast<int>(status_code::unimplemented),
            "Client streaming not yet implemented",
            "grpc::client");
    }

    auto bidi_stream_raw(const std::string& method,
                         const call_options& options)
        -> Result<std::unique_ptr<grpc_client::bidi_stream>>
    {
        if (!connected_.load())
        {
            return error<std::unique_ptr<grpc_client::bidi_stream>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::bidi_stream>>(
            static_cast<int>(status_code::unimplemented),
            "Bidirectional streaming not yet implemented",
            "grpc::client");
    }

private:
    std::string target_;
    std::string host_;
    std::string port_;
    grpc_channel_config config_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;
};

// grpc_client implementation

grpc_client::grpc_client(const std::string& target,
                         const grpc_channel_config& config)
    : impl_(std::make_unique<impl>(target, config))
{
}

grpc_client::~grpc_client() = default;

grpc_client::grpc_client(grpc_client&&) noexcept = default;
grpc_client& grpc_client::operator=(grpc_client&&) noexcept = default;

auto grpc_client::connect() -> VoidResult
{
    return impl_->connect();
}

auto grpc_client::disconnect() -> void
{
    impl_->disconnect();
}

auto grpc_client::is_connected() const -> bool
{
    return impl_->is_connected();
}

auto grpc_client::wait_for_connected(std::chrono::milliseconds timeout) -> bool
{
    return impl_->wait_for_connected(timeout);
}

auto grpc_client::target() const -> const std::string&
{
    return impl_->target();
}

auto grpc_client::call_raw(const std::string& method,
                           const std::vector<uint8_t>& request,
                           const call_options& options) -> Result<grpc_message>
{
    return impl_->call_raw(method, request, options);
}

auto grpc_client::call_raw_async(const std::string& method,
                                 const std::vector<uint8_t>& request,
                                 std::function<void(Result<grpc_message>)> callback,
                                 const call_options& options) -> void
{
    impl_->call_raw_async(method, request, std::move(callback), options);
}

auto grpc_client::server_stream_raw(const std::string& method,
                                    const std::vector<uint8_t>& request,
                                    const call_options& options)
    -> Result<std::unique_ptr<server_stream_reader>>
{
    return impl_->server_stream_raw(method, request, options);
}

auto grpc_client::client_stream_raw(const std::string& method,
                                    const call_options& options)
    -> Result<std::unique_ptr<client_stream_writer>>
{
    return impl_->client_stream_raw(method, options);
}

auto grpc_client::bidi_stream_raw(const std::string& method,
                                  const call_options& options)
    -> Result<std::unique_ptr<bidi_stream>>
{
    return impl_->bidi_stream_raw(method, options);
}

} // namespace network_system::protocols::grpc
