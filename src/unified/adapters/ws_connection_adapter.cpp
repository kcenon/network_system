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

#include "internal/unified/adapters/ws_connection_adapter.h"

#include <algorithm>
#include <cstring>
#include <regex>

namespace kcenon::network::unified::adapters {

ws_connection_adapter::ws_connection_adapter(std::string_view connection_id)
    : connection_id_(connection_id)
    , client_(std::make_shared<core::messaging_ws_client>(connection_id))
{
    setup_internal_callbacks();
}

ws_connection_adapter::~ws_connection_adapter()
{
    close();
}

auto ws_connection_adapter::send(std::span<const std::byte> data) -> VoidResult
{
    if (!client_ || !client_->is_connected()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "WebSocket is not connected"
        );
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    return client_->send_binary(std::move(buffer));
}

auto ws_connection_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
    if (!client_ || !client_->is_connected()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "WebSocket is not connected"
        );
    }

    return client_->send_binary(std::move(data));
}

auto ws_connection_adapter::is_connected() const noexcept -> bool
{
    return client_ && client_->is_connected();
}

auto ws_connection_adapter::id() const noexcept -> std::string_view
{
    return connection_id_;
}

auto ws_connection_adapter::remote_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return remote_endpoint_;
}

auto ws_connection_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto ws_connection_adapter::connect(const endpoint_info& endpoint) -> VoidResult
{
    if (!client_) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Client is not initialized"
        );
    }

    if (client_->is_connected() || client_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::already_connected),
            "Already connected or connecting"
        );
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        remote_endpoint_ = endpoint;
    }

    is_connecting_ = true;

    // Use endpoint's port, default to 80 if not specified
    uint16_t port = endpoint.port;
    if (port == 0) {
        port = 80;
    }

    auto result = client_->start_client(endpoint.host, port, ws_path_);

    if (!result.is_ok()) {
        is_connecting_ = false;
    }

    return result;
}

auto ws_connection_adapter::connect(std::string_view url) -> VoidResult
{
    if (!client_) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Client is not initialized"
        );
    }

    if (client_->is_connected() || client_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::already_connected),
            "Already connected or connecting"
        );
    }

    std::string host;
    uint16_t port = 0;
    std::string path;
    bool secure = false;

    if (!parse_websocket_url(url, host, port, path, secure)) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Invalid WebSocket URL format. Expected: ws://host:port/path or wss://host:port/path"
        );
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        remote_endpoint_ = endpoint_info{host, port};
    }

    is_connecting_ = true;

    auto result = client_->start_client(host, port, path);

    if (!result.is_ok()) {
        is_connecting_ = false;
    }

    return result;
}

auto ws_connection_adapter::close() noexcept -> void
{
    if (client_ && client_->is_running()) {
        (void)client_->stop();
    }
    is_connecting_ = false;
}

auto ws_connection_adapter::set_callbacks(connection_callbacks callbacks) -> void
{
    {
        std::lock_guard lock(callbacks_mutex_);
        callbacks_ = std::move(callbacks);
    }

    setup_internal_callbacks();
}

auto ws_connection_adapter::set_options(connection_options options) -> void
{
    options_ = options;
}

auto ws_connection_adapter::set_timeout(std::chrono::milliseconds timeout) -> void
{
    options_.connect_timeout = timeout;
}

auto ws_connection_adapter::is_connecting() const noexcept -> bool
{
    return is_connecting_.load();
}

auto ws_connection_adapter::wait_for_stop() -> void
{
    if (client_) {
        client_->wait_for_stop();
    }
}

auto ws_connection_adapter::set_path(std::string_view path) -> void
{
    ws_path_ = std::string(path);
}

auto ws_connection_adapter::setup_internal_callbacks() -> void
{
    if (!client_) {
        return;
    }

    // Bridge binary message callback to on_data
    client_->set_binary_callback([this](const std::vector<uint8_t>& data) {
        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_data) {
            std::span<const std::byte> byte_span(
                reinterpret_cast<const std::byte*>(data.data()),
                data.size()
            );
            callbacks_.on_data(byte_span);
        }
    });

    // Also bridge text messages as binary data for unified interface
    client_->set_text_callback([this](const std::string& text) {
        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_data) {
            std::span<const std::byte> byte_span(
                reinterpret_cast<const std::byte*>(text.data()),
                text.size()
            );
            callbacks_.on_data(byte_span);
        }
    });

    // Bridge connected callback
    client_->set_connected_callback([this]() {
        is_connecting_ = false;

        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_connected) {
            callbacks_.on_connected();
        }
    });

    // Bridge disconnected callback
    client_->set_disconnected_callback([this](uint16_t /*code*/, std::string_view /*reason*/) {
        is_connecting_ = false;

        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_disconnected) {
            callbacks_.on_disconnected();
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

auto ws_connection_adapter::parse_websocket_url(std::string_view url,
                                                std::string& host,
                                                uint16_t& port,
                                                std::string& path,
                                                bool& secure) -> bool
{
    std::string url_str(url);

    // Check for WebSocket schemes
    secure = false;
    std::string remaining;

    if (url_str.substr(0, 6) == "wss://") {
        secure = true;
        remaining = url_str.substr(6);
    } else if (url_str.substr(0, 5) == "ws://") {
        secure = false;
        remaining = url_str.substr(5);
    } else {
        // No scheme, assume ws://
        remaining = url_str;
    }

    // Find path start
    auto path_pos = remaining.find('/');
    std::string host_port;
    if (path_pos != std::string::npos) {
        host_port = remaining.substr(0, path_pos);
        path = remaining.substr(path_pos);
    } else {
        host_port = remaining;
        path = "/";
    }

    // Parse host and port
    auto colon_pos = host_port.rfind(':');

    // Check if this is IPv6 (contains '[')
    bool is_ipv6 = host_port.find('[') != std::string::npos;

    if (is_ipv6) {
        auto bracket_end = host_port.find(']');
        if (bracket_end == std::string::npos) {
            return false;  // Invalid IPv6 format
        }

        host = host_port.substr(0, bracket_end + 1);

        if (bracket_end + 1 < host_port.size() && host_port[bracket_end + 1] == ':') {
            std::string port_str = host_port.substr(bracket_end + 2);
            try {
                port = static_cast<uint16_t>(std::stoi(port_str));
            } catch (...) {
                return false;
            }
        } else {
            port = secure ? 443 : 80;
        }
    } else if (colon_pos != std::string::npos) {
        host = host_port.substr(0, colon_pos);
        std::string port_str = host_port.substr(colon_pos + 1);
        try {
            port = static_cast<uint16_t>(std::stoi(port_str));
        } catch (...) {
            return false;
        }
    } else {
        host = host_port;
        port = secure ? 443 : 80;
    }

    return !host.empty();
}

}  // namespace kcenon::network::unified::adapters
