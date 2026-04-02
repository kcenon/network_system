// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/detail/unified/i_listener.h"
#include "internal/http/websocket_server.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kcenon::network::unified::adapters {

/**
 * @class ws_listener_adapter
 * @brief Adapter that wraps messaging_ws_server to implement i_listener
 *
 * This adapter bridges the existing WebSocket server implementation with the new
 * unified interface, enabling protocol factory functions to return i_listener
 * while using the battle-tested underlying implementation.
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Connection Management
 * Accepted WebSocket connections are tracked internally and can be accessed via
 * send_to(), broadcast(), and close_connection() methods.
 */
class ws_listener_adapter : public i_listener {
public:
    /**
     * @brief Constructs an adapter with a unique listener ID
     * @param listener_id Unique identifier for this listener
     */
    explicit ws_listener_adapter(std::string_view listener_id);

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~ws_listener_adapter() override;

    // Non-copyable, non-movable (contains mutex)
    ws_listener_adapter(const ws_listener_adapter&) = delete;
    ws_listener_adapter& operator=(const ws_listener_adapter&) = delete;
    ws_listener_adapter(ws_listener_adapter&&) = delete;
    ws_listener_adapter& operator=(ws_listener_adapter&&) = delete;

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

    // =========================================================================
    // WebSocket-specific configuration
    // =========================================================================

    /**
     * @brief Sets the WebSocket path for the server
     * @param path The WebSocket path to handle (e.g., "/ws", "/socket.io")
     */
    auto set_path(std::string_view path) -> void;

private:
    /**
     * @brief Sets up internal callbacks to bridge to unified callbacks
     */
    auto setup_internal_callbacks() -> void;

    std::string listener_id_;
    std::shared_ptr<core::messaging_ws_server> server_;
    std::string ws_path_ = "/";

    mutable std::mutex callbacks_mutex_;
    listener_callbacks callbacks_;
    accept_callback_t accept_callback_;

    mutable std::mutex endpoint_mutex_;
    endpoint_info local_endpoint_;

    // Connection tracking: connection_id -> ws_connection
    mutable std::mutex connections_mutex_;
    std::unordered_map<std::string, std::shared_ptr<core::ws_connection>> connections_;
};

}  // namespace kcenon::network::unified::adapters
