// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/detail/unified/i_listener.h"
#include "internal/core/unified_messaging_server.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kcenon::network::unified::adapters {

/**
 * @class tcp_listener_adapter
 * @brief Adapter that wraps unified_messaging_server to implement i_listener
 *
 * This adapter bridges the existing TCP server implementation with the new
 * unified interface, enabling protocol factory functions to return i_listener
 * while using the battle-tested underlying implementation.
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Connection Management
 * Accepted connections are tracked internally and can be accessed via
 * send_to(), broadcast(), and close_connection() methods.
 */
class tcp_listener_adapter : public i_listener {
public:
    /**
     * @brief Constructs an adapter with a unique listener ID
     * @param listener_id Unique identifier for this listener
     */
    explicit tcp_listener_adapter(std::string_view listener_id);

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~tcp_listener_adapter() override;

    // Non-copyable, non-movable (contains mutex)
    tcp_listener_adapter(const tcp_listener_adapter&) = delete;
    tcp_listener_adapter& operator=(const tcp_listener_adapter&) = delete;
    tcp_listener_adapter(tcp_listener_adapter&&) = delete;
    tcp_listener_adapter& operator=(tcp_listener_adapter&&) = delete;

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
     * @brief Generates a unique connection ID for a session
     */
    auto generate_connection_id(const core::tcp_server::session_ptr& session) -> std::string;

    std::string listener_id_;
    std::shared_ptr<core::tcp_server> server_;

    mutable std::mutex callbacks_mutex_;
    listener_callbacks callbacks_;
    accept_callback_t accept_callback_;

    mutable std::mutex endpoint_mutex_;
    endpoint_info local_endpoint_;

    // Session tracking: connection_id -> session
    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, core::tcp_server::session_ptr> sessions_;
    std::unordered_map<void*, std::string> session_to_id_;  // Reverse lookup
};

}  // namespace kcenon::network::unified::adapters
