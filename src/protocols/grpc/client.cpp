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
#include "kcenon/network/protocols/grpc/frame.h"
#include "kcenon/network/protocols/http2/http2_client.h"

#include <atomic>
#include <charconv>
#include <mutex>
#include <thread>

namespace network_system::protocols::grpc
{

// Implementation class using HTTP/2 transport
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
        auto port_str = target_.substr(colon_pos + 1);

        unsigned short port = 0;
        auto [ptr, ec] = std::from_chars(
            port_str.data(),
            port_str.data() + port_str.size(),
            port);

        if (ec != std::errc())
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Invalid port number",
                "grpc::client");
        }

        // Create HTTP/2 client
        http2_client_ = std::make_shared<http2::http2_client>("grpc-client");
        http2_client_->set_timeout(config_.default_timeout);

        // Connect using HTTP/2
        auto result = http2_client_->connect(host_, port);
        if (result.is_err())
        {
            const auto& err = result.error();
            return error_void(err.code, err.message, "grpc::client",
                              get_error_details(err));
        }

        connected_.store(true);
        return ok();
    }

    auto disconnect() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (http2_client_ && connected_.load())
        {
            http2_client_->disconnect();
        }

        connected_.store(false);
        http2_client_.reset();
    }

    auto is_connected() const -> bool
    {
        return connected_.load() && http2_client_ && http2_client_->is_connected();
    }

    auto wait_for_connected(std::chrono::milliseconds timeout) -> bool
    {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (is_connected())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return is_connected();
    }

    auto target() const -> const std::string&
    {
        return target_;
    }

    auto call_raw(const std::string& method,
                  const std::vector<uint8_t>& request,
                  const call_options& options) -> Result<grpc_message>
    {
        if (!is_connected())
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

        // Build gRPC headers
        std::vector<http2::http_header> headers;
        headers.emplace_back(header_names::content_type, grpc_content_type);
        headers.emplace_back(header_names::te, "trailers");
        headers.emplace_back(header_names::grpc_accept_encoding,
                             std::string(compression::identity) + "," +
                             compression::gzip + "," + compression::deflate);

        // Add timeout header if deadline is set
        if (options.deadline.has_value())
        {
            auto now = std::chrono::system_clock::now();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                options.deadline.value() - now);
            if (remaining.count() > 0)
            {
                headers.emplace_back(header_names::grpc_timeout,
                                     format_timeout(static_cast<uint64_t>(remaining.count())));
            }
        }

        // Add custom metadata
        for (const auto& [key, value] : options.metadata)
        {
            headers.emplace_back(key, value);
        }

        // Serialize request with gRPC framing
        grpc_message request_msg{std::vector<uint8_t>{request.begin(), request.end()}};
        auto serialized_request = request_msg.serialize();

        // Send HTTP/2 POST request
        auto response_result = http2_client_->post(method, serialized_request, headers);
        if (response_result.is_err())
        {
            const auto& err = response_result.error();
            return error<grpc_message>(err.code, err.message, "grpc::client",
                                       get_error_details(err));
        }

        const auto& response = response_result.value();

        // Check HTTP status - gRPC always uses 200 OK for successful transport
        if (response.status_code != 200)
        {
            return error<grpc_message>(
                static_cast<int>(status_code::unavailable),
                "HTTP error: " + std::to_string(response.status_code),
                "grpc::client");
        }

        // Extract gRPC status from trailers/headers
        status_code grpc_status = status_code::ok;
        std::string grpc_message_str;

        for (const auto& header : response.headers)
        {
            if (header.name == trailer_names::grpc_status)
            {
                int status_int = 0;
                auto [ptr, ec] = std::from_chars(
                    header.value.data(),
                    header.value.data() + header.value.size(),
                    status_int);
                if (ec == std::errc())
                {
                    grpc_status = static_cast<status_code>(status_int);
                }
            }
            else if (header.name == trailer_names::grpc_message)
            {
                grpc_message_str = header.value;
            }
        }

        // Check gRPC status
        if (grpc_status != status_code::ok)
        {
            return error<grpc_message>(
                static_cast<int>(grpc_status),
                grpc_message_str.empty() ?
                    std::string(status_code_to_string(grpc_status)) : grpc_message_str,
                "grpc::client");
        }

        // Parse response message
        if (response.body.empty())
        {
            // Empty response is valid for some RPCs
            return ok(grpc_message{});
        }

        auto parse_result = grpc_message::parse(response.body);
        if (parse_result.is_err())
        {
            const auto& err = parse_result.error();
            return error<grpc_message>(err.code, err.message, "grpc::client",
                                       get_error_details(err));
        }

        return ok(std::move(parse_result.value()));
    }

    auto call_raw_async(const std::string& method,
                        const std::vector<uint8_t>& request,
                        std::function<void(Result<grpc_message>)> callback,
                        const call_options& options) -> void
    {
        // Execute asynchronously in a separate thread
        std::thread([this, method, request, callback, options]() {
            auto result = call_raw(method, request, options);
            if (callback)
            {
                callback(std::move(result));
            }
        }).detach();
    }

    auto server_stream_raw(const std::string& method,
                           const std::vector<uint8_t>& request,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::server_stream_reader>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::server_stream_reader>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        // Server streaming requires different HTTP/2 handling
        // For now, return unimplemented
        return error<std::unique_ptr<grpc_client::server_stream_reader>>(
            static_cast<int>(status_code::unimplemented),
            "Server streaming not yet implemented",
            "grpc::client",
            "Server streaming requires dedicated stream handling");
    }

    auto client_stream_raw(const std::string& method,
                           const call_options& options)
        -> Result<std::unique_ptr<grpc_client::client_stream_writer>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::client_stream_writer>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::client_stream_writer>>(
            static_cast<int>(status_code::unimplemented),
            "Client streaming not yet implemented",
            "grpc::client",
            "Client streaming requires dedicated stream handling");
    }

    auto bidi_stream_raw(const std::string& method,
                         const call_options& options)
        -> Result<std::unique_ptr<grpc_client::bidi_stream>>
    {
        if (!is_connected())
        {
            return error<std::unique_ptr<grpc_client::bidi_stream>>(
                error_codes::network_system::connection_failed,
                "Not connected to server",
                "grpc::client");
        }

        return error<std::unique_ptr<grpc_client::bidi_stream>>(
            static_cast<int>(status_code::unimplemented),
            "Bidirectional streaming not yet implemented",
            "grpc::client",
            "Bidirectional streaming requires dedicated stream handling");
    }

private:
    std::string target_;
    std::string host_;
    grpc_channel_config config_;
    std::shared_ptr<http2::http2_client> http2_client_;
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
