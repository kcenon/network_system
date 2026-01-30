/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
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

#include "network_udp/unified/adapters/udp_connection_adapter.h"

#include <algorithm>
#include <cstring>

namespace kcenon::network::unified::adapters {

udp_connection_adapter::udp_connection_adapter(std::string_view connection_id)
    : connection_id_(connection_id)
    , client_(std::make_shared<core::messaging_udp_client>(connection_id))
{
    setup_internal_callbacks();
}

udp_connection_adapter::~udp_connection_adapter()
{
    close();
}

auto udp_connection_adapter::send(std::span<const std::byte> data) -> VoidResult
{
    if (!client_ || !client_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "UDP client is not running"
        );
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    return client_->send(std::move(buffer));
}

auto udp_connection_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
    if (!client_ || !client_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "UDP client is not running"
        );
    }

    return client_->send(std::move(data));
}

auto udp_connection_adapter::is_connected() const noexcept -> bool
{
    // For UDP, "connected" means the client is running with a target endpoint
    return client_ && client_->is_running();
}

auto udp_connection_adapter::id() const noexcept -> std::string_view
{
    return connection_id_;
}

auto udp_connection_adapter::remote_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return remote_endpoint_;
}

auto udp_connection_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto udp_connection_adapter::connect(const endpoint_info& endpoint) -> VoidResult
{
    if (!client_) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Client is not initialized"
        );
    }

    if (client_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::already_connected),
            "Already running"
        );
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        remote_endpoint_ = endpoint;
    }

    is_connecting_ = true;
    auto result = client_->start_client(endpoint.host, endpoint.port);

    if (result.is_ok()) {
        is_connecting_ = false;
        // For UDP, we're immediately "connected" once started
        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_connected) {
            callbacks_.on_connected();
        }
    } else {
        is_connecting_ = false;
    }

    return result;
}

auto udp_connection_adapter::connect(std::string_view url) -> VoidResult
{
    // Parse URL for UDP: expect format "udp://host:port" or just "host:port"
    std::string url_str(url);

    // Remove udp:// prefix if present
    const std::string udp_prefix = "udp://";
    if (url_str.substr(0, udp_prefix.size()) == udp_prefix) {
        url_str = url_str.substr(udp_prefix.size());
    }

    // Find the last colon for port separation
    auto colon_pos = url_str.rfind(':');
    if (colon_pos == std::string::npos) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "URL must contain port number (format: host:port)"
        );
    }

    std::string host = url_str.substr(0, colon_pos);
    std::string port_str = url_str.substr(colon_pos + 1);

    uint16_t port = 0;
    try {
        port = static_cast<uint16_t>(std::stoi(port_str));
    } catch (...) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Invalid port number in URL"
        );
    }

    return connect(endpoint_info{host, port});
}

auto udp_connection_adapter::close() noexcept -> void
{
    if (client_ && client_->is_running()) {
        (void)client_->stop_client();

        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_disconnected) {
            callbacks_.on_disconnected();
        }
    }
    is_connecting_ = false;
}

auto udp_connection_adapter::set_callbacks(connection_callbacks callbacks) -> void
{
    {
        std::lock_guard lock(callbacks_mutex_);
        callbacks_ = std::move(callbacks);
    }

    // Re-setup internal callbacks to use the new user callbacks
    setup_internal_callbacks();
}

auto udp_connection_adapter::set_options(connection_options options) -> void
{
    options_ = options;
}

auto udp_connection_adapter::set_timeout(std::chrono::milliseconds timeout) -> void
{
    options_.connect_timeout = timeout;
}

auto udp_connection_adapter::is_connecting() const noexcept -> bool
{
    return is_connecting_.load();
}

auto udp_connection_adapter::wait_for_stop() -> void
{
    if (client_) {
        client_->wait_for_stop();
    }
}

auto udp_connection_adapter::setup_internal_callbacks() -> void
{
    if (!client_) {
        return;
    }

    // Bridge receive callback
    client_->set_receive_callback(
        [this](const std::vector<uint8_t>& data,
               const interfaces::i_udp_client::endpoint_info& sender) {
            // Update local endpoint information if we receive from the expected target
            {
                std::lock_guard ep_lock(endpoint_mutex_);
                // Note: local_endpoint_ would require socket info which isn't easily available
            }

            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_data) {
                std::span<const std::byte> byte_span(
                    reinterpret_cast<const std::byte*>(data.data()),
                    data.size()
                );
                callbacks_.on_data(byte_span);
            }
        });

    // Bridge error callback
    client_->set_error_callback([this](std::error_code ec) {
        is_connecting_ = false;

        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_error) {
            callbacks_.on_error(ec);
        }
    });
}

}  // namespace kcenon::network::unified::adapters
