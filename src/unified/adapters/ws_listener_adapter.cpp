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

#include "kcenon/network/unified/adapters/ws_listener_adapter.h"

#include <algorithm>
#include <cstring>

namespace kcenon::network::unified::adapters {

ws_listener_adapter::ws_listener_adapter(std::string_view listener_id)
    : listener_id_(listener_id)
    , server_(std::make_shared<core::messaging_ws_server>(listener_id))
{
    setup_internal_callbacks();
}

ws_listener_adapter::~ws_listener_adapter()
{
    stop();
}

auto ws_listener_adapter::start(const endpoint_info& bind_address) -> VoidResult
{
    if (!server_) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Server is not initialized"
        );
    }

    if (server_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::already_connected),
            "Already listening"
        );
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        local_endpoint_ = bind_address;
    }

    {
        std::lock_guard lock(connections_mutex_);
        connections_.clear();
    }

    return server_->start_server(bind_address.port, ws_path_);
}

auto ws_listener_adapter::start(uint16_t port) -> VoidResult
{
    return start(endpoint_info{"0.0.0.0", port});
}

auto ws_listener_adapter::stop() noexcept -> void
{
    if (server_ && server_->is_running()) {
        // Notify disconnection for all connections
        {
            std::lock_guard cb_lock(callbacks_mutex_);
            std::lock_guard conn_lock(connections_mutex_);

            if (callbacks_.on_disconnect) {
                for (const auto& [conn_id, _] : connections_) {
                    callbacks_.on_disconnect(conn_id);
                }
            }
            connections_.clear();
        }

        (void)server_->stop_server();
    }
}

auto ws_listener_adapter::set_callbacks(listener_callbacks callbacks) -> void
{
    {
        std::lock_guard lock(callbacks_mutex_);
        callbacks_ = std::move(callbacks);
    }

    setup_internal_callbacks();
}

auto ws_listener_adapter::set_accept_callback(accept_callback_t callback) -> void
{
    std::lock_guard lock(callbacks_mutex_);
    accept_callback_ = std::move(callback);
}

auto ws_listener_adapter::is_listening() const noexcept -> bool
{
    return server_ && server_->is_running();
}

auto ws_listener_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto ws_listener_adapter::connection_count() const noexcept -> size_t
{
    std::lock_guard lock(connections_mutex_);
    return connections_.size();
}

auto ws_listener_adapter::send_to(std::string_view connection_id,
                                   std::span<const std::byte> data) -> VoidResult
{
    std::shared_ptr<core::ws_connection> conn;
    {
        std::lock_guard lock(connections_mutex_);
        auto it = connections_.find(std::string(connection_id));
        if (it == connections_.end()) {
            return error_void(
                static_cast<int>(std::errc::no_such_device_or_address),
                "Connection not found: " + std::string(connection_id)
            );
        }
        conn = it->second;
    }

    if (!conn || !conn->is_connected()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "Connection is no longer valid"
        );
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    return conn->send_binary(std::move(buffer));
}

auto ws_listener_adapter::broadcast(std::span<const std::byte> data) -> VoidResult
{
    if (!server_ || !server_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "Server is not running"
        );
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    server_->broadcast_binary(buffer);
    return ok();
}

auto ws_listener_adapter::close_connection(std::string_view connection_id) noexcept -> void
{
    std::shared_ptr<core::ws_connection> conn;
    {
        std::lock_guard lock(connections_mutex_);
        auto it = connections_.find(std::string(connection_id));
        if (it != connections_.end()) {
            conn = it->second;
            connections_.erase(it);
        }
    }

    if (conn) {
        conn->close();
    }
}

auto ws_listener_adapter::wait_for_stop() -> void
{
    if (server_) {
        server_->wait_for_stop();
    }
}

auto ws_listener_adapter::set_path(std::string_view path) -> void
{
    ws_path_ = std::string(path);
}

auto ws_listener_adapter::setup_internal_callbacks() -> void
{
    if (!server_) {
        return;
    }

    // Bridge connection callback
    server_->set_connection_callback(
        [this](std::shared_ptr<interfaces::i_websocket_session> session) {
            if (!session) {
                return;
            }

            // Use the session's ID as connection ID
            std::string conn_id(session->id());

            // Try to get the actual ws_connection from server
            auto ws_conn = server_->get_connection(conn_id);
            if (ws_conn) {
                std::lock_guard lock(connections_mutex_);
                connections_[conn_id] = ws_conn;
            }

            // Call user callbacks
            {
                std::lock_guard lock(callbacks_mutex_);
                if (callbacks_.on_accept) {
                    callbacks_.on_accept(conn_id);
                }
            }
        });

    // Bridge disconnection callback
    server_->set_disconnection_callback(
        [this](std::string_view session_id, uint16_t /*code*/, std::string_view /*reason*/) {
            std::string conn_id(session_id);

            {
                std::lock_guard lock(connections_mutex_);
                connections_.erase(conn_id);
            }

            {
                std::lock_guard lock(callbacks_mutex_);
                if (callbacks_.on_disconnect) {
                    callbacks_.on_disconnect(conn_id);
                }
            }
        });

    // Bridge binary message callback
    server_->set_binary_callback(
        [this](std::string_view session_id, const std::vector<uint8_t>& data) {
            std::string conn_id(session_id);

            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_data) {
                std::span<const std::byte> byte_span(
                    reinterpret_cast<const std::byte*>(data.data()),
                    data.size()
                );
                callbacks_.on_data(conn_id, byte_span);
            }
        });

    // Also bridge text messages as binary data for unified interface
    server_->set_text_callback(
        [this](std::string_view session_id, const std::string& text) {
            std::string conn_id(session_id);

            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_data) {
                std::span<const std::byte> byte_span(
                    reinterpret_cast<const std::byte*>(text.data()),
                    text.size()
                );
                callbacks_.on_data(conn_id, byte_span);
            }
        });

    // Bridge error callback
    server_->set_error_callback(
        [this](std::string_view session_id, std::error_code ec) {
            std::string conn_id(session_id);

            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_error) {
                callbacks_.on_error(conn_id, ec);
            }
        });
}

}  // namespace kcenon::network::unified::adapters
