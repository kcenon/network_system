// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/detail/unified/i_listener.h"
#include "internal/core/messaging_udp_server.h"

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
