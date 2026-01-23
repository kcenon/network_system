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

#pragma once

#include "kcenon/network/unified/i_listener.h"
#include "kcenon/network/core/messaging_udp_server.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kcenon::network::unified::adapters {

/**
 * @class udp_listener_adapter
 * @brief Adapter that wraps messaging_udp_server to implement i_listener
 *
 * This adapter bridges the existing UDP server implementation with the new
 * unified interface. Note that UDP is connectionless - there are no real
 * "connections" to accept. Instead, we track unique remote endpoints that
 * send data to us.
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Ownership
 * The adapter owns the underlying server via shared_ptr for proper RAII.
 *
 * ### UDP Semantics
 * - start() binds to a local port and begins receiving datagrams
 * - Each unique sender endpoint is tracked as a virtual "connection"
 * - on_accept callback is triggered when a new endpoint sends data
 * - send_to() and broadcast() send datagrams to specific/all endpoints
 */
class udp_listener_adapter : public i_listener {
public:
    /**
     * @brief Constructs an adapter with a unique listener ID
     * @param listener_id Unique identifier for this listener
     */
    explicit udp_listener_adapter(std::string_view listener_id);

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~udp_listener_adapter() override;

    // Non-copyable, non-movable (contains mutex)
    udp_listener_adapter(const udp_listener_adapter&) = delete;
    udp_listener_adapter& operator=(const udp_listener_adapter&) = delete;
    udp_listener_adapter(udp_listener_adapter&&) = delete;
    udp_listener_adapter& operator=(udp_listener_adapter&&) = delete;

    // =========================================================================
    // i_listener interface implementation
    // =========================================================================

    [[nodiscard]] auto start(const endpoint_info& bind_address) -> VoidResult override;
    [[nodiscard]] auto start(uint16_t port) -> VoidResult override;
    auto stop() noexcept -> void override;
    auto set_callbacks(listener_callbacks callbacks) -> void override;
    auto set_accept_callback(accept_callback_t callback) -> void override;
    [[nodiscard]] auto is_listening() const noexcept -> bool override;
    [[nodiscard]] auto local_endpoint() const noexcept -> endpoint_info override;
    [[nodiscard]] auto connection_count() const noexcept -> size_t override;
    [[nodiscard]] auto send_to(std::string_view connection_id,
                                std::span<const std::byte> data) -> VoidResult override;
    [[nodiscard]] auto broadcast(std::span<const std::byte> data) -> VoidResult override;
    auto close_connection(std::string_view connection_id) noexcept -> void override;
    auto wait_for_stop() -> void override;

private:
    /**
     * @brief Sets up internal callbacks to bridge to unified callbacks
     */
    auto setup_internal_callbacks() -> void;

    /**
     * @brief Generates a connection ID from an endpoint
     */
    static auto make_connection_id(const std::string& address, uint16_t port) -> std::string;

    /**
     * @brief Parses a connection ID back to endpoint info
     */
    static auto parse_connection_id(std::string_view connection_id) -> std::optional<endpoint_info>;

    std::string listener_id_;
    std::shared_ptr<core::messaging_udp_server> server_;

    mutable std::mutex callbacks_mutex_;
    listener_callbacks callbacks_;
    accept_callback_t accept_callback_;

    mutable std::mutex endpoint_mutex_;
    endpoint_info local_endpoint_;

    // Track known remote endpoints (for UDP's virtual "connections")
    mutable std::mutex connections_mutex_;
    std::unordered_map<std::string, endpoint_info> known_endpoints_;
};

}  // namespace kcenon::network::unified::adapters
