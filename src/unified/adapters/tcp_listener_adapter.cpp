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

#include "internal/unified/adapters/tcp_listener_adapter.h"

#include <algorithm>
#include <cstring>

namespace kcenon::network::unified::adapters {

tcp_listener_adapter::tcp_listener_adapter(std::string_view listener_id)
    : listener_id_(listener_id)
    , server_(std::make_shared<core::tcp_server>(listener_id))
{
    setup_internal_callbacks();
}

tcp_listener_adapter::~tcp_listener_adapter()
{
    stop();
}

auto tcp_listener_adapter::start(const endpoint_info& bind_address) -> VoidResult
{
    if (!server_) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Server is not initialized"
        );
    }

    {
        std::lock_guard lock(endpoint_mutex_);
        local_endpoint_ = bind_address;
    }

    return server_->start_server(bind_address.port);
}

auto tcp_listener_adapter::start(uint16_t port) -> VoidResult
{
    return start(endpoint_info{"0.0.0.0", port});
}

auto tcp_listener_adapter::stop() noexcept -> void
{
    if (server_ && server_->is_running()) {
        (void)server_->stop_server();
    }

    // Clear all tracked sessions
    {
        std::lock_guard lock(sessions_mutex_);
        sessions_.clear();
        session_to_id_.clear();
    }
}

auto tcp_listener_adapter::set_callbacks(listener_callbacks callbacks) -> void
{
    {
        std::lock_guard lock(callbacks_mutex_);
        callbacks_ = std::move(callbacks);
    }

    setup_internal_callbacks();
}

auto tcp_listener_adapter::set_accept_callback(accept_callback_t callback) -> void
{
    std::lock_guard lock(callbacks_mutex_);
    accept_callback_ = std::move(callback);
}

auto tcp_listener_adapter::is_listening() const noexcept -> bool
{
    return server_ && server_->is_running();
}

auto tcp_listener_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto tcp_listener_adapter::connection_count() const noexcept -> size_t
{
    std::lock_guard lock(sessions_mutex_);
    return sessions_.size();
}

auto tcp_listener_adapter::send_to(std::string_view connection_id,
                                   std::span<const std::byte> data) -> VoidResult
{
    core::tcp_server::session_ptr session;
    {
        std::lock_guard lock(sessions_mutex_);
        auto it = sessions_.find(std::string(connection_id));
        if (it == sessions_.end()) {
            return error_void(
                static_cast<int>(std::errc::no_such_device_or_address),
                "Connection not found: " + std::string(connection_id)
            );
        }
        session = it->second;
    }

    if (!session) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "Session is no longer valid"
        );
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    return session->send(std::move(buffer));
}

auto tcp_listener_adapter::broadcast(std::span<const std::byte> data) -> VoidResult
{
    std::vector<core::tcp_server::session_ptr> sessions_copy;
    {
        std::lock_guard lock(sessions_mutex_);
        sessions_copy.reserve(sessions_.size());
        for (const auto& [id, session] : sessions_) {
            if (session) {
                sessions_copy.push_back(session);
            }
        }
    }

    if (sessions_copy.empty()) {
        return ok();  // No connections, nothing to broadcast
    }

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    bool any_success = false;
    for (const auto& session : sessions_copy) {
        std::vector<uint8_t> buffer_copy = buffer;
        auto result = session->send(std::move(buffer_copy));
        if (result.is_ok()) {
            any_success = true;
        }
    }

    if (any_success) {
        return ok();
    }

    return error_void(
        static_cast<int>(std::errc::io_error),
        "Failed to send to any connection"
    );
}

auto tcp_listener_adapter::close_connection(std::string_view connection_id) noexcept -> void
{
    core::tcp_server::session_ptr session;
    {
        std::lock_guard lock(sessions_mutex_);
        auto it = sessions_.find(std::string(connection_id));
        if (it != sessions_.end()) {
            session = it->second;
            if (session) {
                session_to_id_.erase(session.get());
            }
            sessions_.erase(it);
        }
    }

    // Session will be closed when its shared_ptr goes out of scope
    // or we could explicitly close it if such method exists
}

auto tcp_listener_adapter::wait_for_stop() -> void
{
    if (server_) {
        server_->wait_for_stop();
    }
}

auto tcp_listener_adapter::setup_internal_callbacks() -> void
{
    if (!server_) {
        return;
    }

    // Bridge connection callback
    server_->set_connection_callback([this](core::tcp_server::session_ptr session) {
        if (!session) {
            return;
        }

        std::string conn_id = generate_connection_id(session);

        {
            std::lock_guard lock(sessions_mutex_);
            sessions_[conn_id] = session;
            session_to_id_[session.get()] = conn_id;
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
    server_->set_disconnection_callback([this](const std::string& session_id) {
        std::string conn_id;
        {
            std::lock_guard lock(sessions_mutex_);
            // Find the connection ID by session's internal ID
            for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
                if (it->second && std::string(it->second->id()) == session_id) {
                    conn_id = it->first;
                    if (it->second) {
                        session_to_id_.erase(it->second.get());
                    }
                    sessions_.erase(it);
                    break;
                }
            }
        }

        if (!conn_id.empty()) {
            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_disconnect) {
                callbacks_.on_disconnect(conn_id);
            }
        }
    });

    // Bridge receive callback
    server_->set_receive_callback([this](core::tcp_server::session_ptr session,
                                         const std::vector<uint8_t>& data) {
        if (!session) {
            return;
        }

        std::string conn_id;
        {
            std::lock_guard lock(sessions_mutex_);
            auto it = session_to_id_.find(session.get());
            if (it != session_to_id_.end()) {
                conn_id = it->second;
            }
        }

        if (!conn_id.empty()) {
            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_data) {
                std::span<const std::byte> byte_span(
                    reinterpret_cast<const std::byte*>(data.data()),
                    data.size()
                );
                callbacks_.on_data(conn_id, byte_span);
            }
        }
    });

    // Bridge error callback
    server_->set_error_callback([this](core::tcp_server::session_ptr session,
                                       std::error_code ec) {
        if (!session) {
            return;
        }

        std::string conn_id;
        {
            std::lock_guard lock(sessions_mutex_);
            auto it = session_to_id_.find(session.get());
            if (it != session_to_id_.end()) {
                conn_id = it->second;
            }
        }

        if (!conn_id.empty()) {
            std::lock_guard lock(callbacks_mutex_);
            if (callbacks_.on_error) {
                callbacks_.on_error(conn_id, ec);
            }
        }
    });
}

auto tcp_listener_adapter::generate_connection_id(
    const core::tcp_server::session_ptr& session) -> std::string
{
    // Use the session's internal ID
    return std::string(session->id());
}

}  // namespace kcenon::network::unified::adapters
