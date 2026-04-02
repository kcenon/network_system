// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/detail/unified/i_connection.h"
#include "internal/core/messaging_udp_client.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace kcenon::network::unified::adapters {

/**
 * @class udp_connection_adapter
 * @brief Adapter that wraps messaging_udp_client to implement i_connection
 *
 * This adapter bridges the existing UDP client implementation with the new
 * unified interface. Note that UDP is connectionless - "connect" simply sets
 * the target endpoint, and the connection is considered "connected" when the
 * client is running.
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Ownership
 * The adapter owns the underlying client via shared_ptr for proper RAII.
 *
 * ### UDP Semantics
 * - connect() sets target endpoint and starts the client
 * - is_connected() returns true when client is running
 * - send() sends datagrams to the configured target endpoint
 * - Data arrives via on_data callback
 */
class udp_connection_adapter : public i_connection {
public:
    /**
     * @brief Constructs an adapter with a unique connection ID
     * @param connection_id Unique identifier for this connection
     */
    explicit udp_connection_adapter(std::string_view connection_id);

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~udp_connection_adapter() override;

    // Non-copyable, non-movable (contains mutex)
    udp_connection_adapter(const udp_connection_adapter&) = delete;
    udp_connection_adapter& operator=(const udp_connection_adapter&) = delete;
    udp_connection_adapter(udp_connection_adapter&&) = delete;
    udp_connection_adapter& operator=(udp_connection_adapter&&) = delete;

    // =========================================================================
    // i_transport interface implementation
    // =========================================================================

    [[nodiscard]] auto send(std::span<const std::byte> data) -> VoidResult override;
    [[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
    [[nodiscard]] auto is_connected() const noexcept -> bool override;
    [[nodiscard]] auto id() const noexcept -> std::string_view override;
    [[nodiscard]] auto remote_endpoint() const noexcept -> endpoint_info override;
    [[nodiscard]] auto local_endpoint() const noexcept -> endpoint_info override;

    // =========================================================================
    // i_connection interface implementation
    // =========================================================================

    [[nodiscard]] auto connect(const endpoint_info& endpoint) -> VoidResult override;
    [[nodiscard]] auto connect(std::string_view url) -> VoidResult override;
    auto close() noexcept -> void override;
    auto set_callbacks(connection_callbacks callbacks) -> void override;
    auto set_options(connection_options options) -> void override;
    auto set_timeout(std::chrono::milliseconds timeout) -> void override;
    [[nodiscard]] auto is_connecting() const noexcept -> bool override;
    auto wait_for_stop() -> void override;

private:
    /**
     * @brief Sets up internal callbacks to bridge to unified callbacks
     */
    auto setup_internal_callbacks() -> void;

    std::string connection_id_;
    std::shared_ptr<core::messaging_udp_client> client_;

    mutable std::mutex callbacks_mutex_;
    connection_callbacks callbacks_;

    mutable std::mutex endpoint_mutex_;
    endpoint_info remote_endpoint_;
    endpoint_info local_endpoint_;

    std::atomic<bool> is_connecting_{false};
    connection_options options_;
};

}  // namespace kcenon::network::unified::adapters
