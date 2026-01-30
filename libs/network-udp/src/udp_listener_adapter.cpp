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

#include "network_udp/unified/adapters/udp_listener_adapter.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace kcenon::network::unified::adapters {

udp_listener_adapter::udp_listener_adapter(std::string_view listener_id)
    : listener_id_(listener_id)
    , server_(std::make_shared<core::messaging_udp_server>(listener_id))
{
    setup_internal_callbacks();
}

udp_listener_adapter::~udp_listener_adapter()
{
    stop();
}

auto udp_listener_adapter::start(const endpoint_info& bind_address) -> VoidResult
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

    // Clear known endpoints when starting fresh
    {
        std::lock_guard lock(connections_mutex_);
        known_endpoints_.clear();
    }

    return server_->start_server(bind_address.port);
}

auto udp_listener_adapter::start(uint16_t port) -> VoidResult
{
    return start(endpoint_info{"0.0.0.0", port});
}

auto udp_listener_adapter::stop() noexcept -> void
{
    if (server_ && server_->is_running()) {
        // Notify disconnection for all known endpoints
        {
            std::lock_guard cb_lock(callbacks_mutex_);
            std::lock_guard conn_lock(connections_mutex_);

            if (callbacks_.on_disconnect) {
                for (const auto& [conn_id, _] : known_endpoints_) {
                    callbacks_.on_disconnect(conn_id);
                }
            }
            known_endpoints_.clear();
        }

        (void)server_->stop_server();
    }
}

auto udp_listener_adapter::set_callbacks(listener_callbacks callbacks) -> void
{
    {
        std::lock_guard lock(callbacks_mutex_);
        callbacks_ = std::move(callbacks);
    }
    setup_internal_callbacks();
}

auto udp_listener_adapter::set_accept_callback(accept_callback_t callback) -> void
{
    std::lock_guard lock(callbacks_mutex_);
    accept_callback_ = std::move(callback);
}

auto udp_listener_adapter::is_listening() const noexcept -> bool
{
    return server_ && server_->is_running();
}

auto udp_listener_adapter::local_endpoint() const noexcept -> endpoint_info
{
    std::lock_guard lock(endpoint_mutex_);
    return local_endpoint_;
}

auto udp_listener_adapter::connection_count() const noexcept -> size_t
{
    std::lock_guard lock(connections_mutex_);
    return known_endpoints_.size();
}

auto udp_listener_adapter::send_to(std::string_view connection_id,
                                    std::span<const std::byte> data) -> VoidResult
{
    if (!server_ || !server_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "Server is not running"
        );
    }

    auto ep_opt = parse_connection_id(connection_id);
    if (!ep_opt) {
        return error_void(
            static_cast<int>(std::errc::invalid_argument),
            "Invalid connection ID format"
        );
    }

    interfaces::i_udp_server::endpoint_info target_ep;
    target_ep.address = ep_opt->host;
    target_ep.port = ep_opt->port;

    std::vector<uint8_t> buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());

    return server_->send_to(target_ep, std::move(buffer));
}

auto udp_listener_adapter::broadcast(std::span<const std::byte> data) -> VoidResult
{
    if (!server_ || !server_->is_running()) {
        return error_void(
            static_cast<int>(std::errc::not_connected),
            "Server is not running"
        );
    }

    std::vector<std::pair<std::string, endpoint_info>> endpoints_copy;
    {
        std::lock_guard lock(connections_mutex_);
        endpoints_copy.reserve(known_endpoints_.size());
        for (const auto& [id, ep] : known_endpoints_) {
            endpoints_copy.emplace_back(id, ep);
        }
    }

    if (endpoints_copy.empty()) {
        return ok();  // No endpoints to broadcast to
    }

    VoidResult last_result = ok();
    for (const auto& [conn_id, ep] : endpoints_copy) {
        auto result = send_to(conn_id, data);
        if (!result.is_ok()) {
            last_result = result;  // Return last error if any failed
        }
    }

    return last_result;
}

auto udp_listener_adapter::close_connection(std::string_view connection_id) noexcept -> void
{
    std::string conn_id_str(connection_id);

    {
        std::lock_guard lock(connections_mutex_);
        known_endpoints_.erase(conn_id_str);
    }

    std::lock_guard lock(callbacks_mutex_);
    if (callbacks_.on_disconnect) {
        callbacks_.on_disconnect(connection_id);
    }
}

auto udp_listener_adapter::wait_for_stop() -> void
{
    if (server_) {
        server_->wait_for_stop();
    }
}

auto udp_listener_adapter::setup_internal_callbacks() -> void
{
    if (!server_) {
        return;
    }

    // Bridge receive callback - also handles "connection" tracking for UDP
    server_->set_receive_callback(
        [this](const std::vector<uint8_t>& data,
               const interfaces::i_udp_server::endpoint_info& sender) {
            std::string conn_id = make_connection_id(sender.address, sender.port);
            bool is_new_endpoint = false;

            {
                std::lock_guard lock(connections_mutex_);
                if (known_endpoints_.find(conn_id) == known_endpoints_.end()) {
                    known_endpoints_[conn_id] = endpoint_info{sender.address, sender.port};
                    is_new_endpoint = true;
                }
            }

            // If this is a new endpoint, trigger on_accept
            if (is_new_endpoint) {
                std::lock_guard lock(callbacks_mutex_);
                if (callbacks_.on_accept) {
                    callbacks_.on_accept(conn_id);
                }
                // Note: accept_callback_ with unique_ptr<i_connection> doesn't make
                // sense for UDP since there's no real connection object to hand over
            }

            // Forward data to callback
            {
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
    server_->set_error_callback([this](std::error_code ec) {
        std::lock_guard lock(callbacks_mutex_);
        if (callbacks_.on_error) {
            // For server-level errors, use empty connection ID
            callbacks_.on_error("", ec);
        }
    });
}

auto udp_listener_adapter::make_connection_id(const std::string& address, uint16_t port) -> std::string
{
    std::ostringstream oss;
    oss << address << ":" << port;
    return oss.str();
}

auto udp_listener_adapter::parse_connection_id(std::string_view connection_id) -> std::optional<endpoint_info>
{
    std::string conn_str(connection_id);
    auto colon_pos = conn_str.rfind(':');

    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    std::string host = conn_str.substr(0, colon_pos);
    std::string port_str = conn_str.substr(colon_pos + 1);

    uint16_t port = 0;
    try {
        port = static_cast<uint16_t>(std::stoi(port_str));
    } catch (...) {
        return std::nullopt;
    }

    return endpoint_info{host, port};
}

}  // namespace kcenon::network::unified::adapters
