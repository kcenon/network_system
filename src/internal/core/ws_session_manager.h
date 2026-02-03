/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "internal/core/session_manager_base.h"

namespace kcenon::network::core {

// Forward declaration
class ws_connection;

/**
 * @class ws_session_manager
 * @brief Thread-safe WebSocket session lifecycle management.
 *
 * This class extends session_manager_base<ws_connection> to provide
 * WebSocket-specific connection management while leveraging the unified
 * template implementation for common functionality.
 *
 * Features:
 * - Thread-safe connection tracking
 * - Connection limit enforcement
 * - Backpressure signaling
 * - Connection metrics
 * - Automatic connection ID generation
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses shared_mutex for concurrent reads and exclusive writes
 * - Atomic counters for metrics
 *
 * Usage Example:
 * @code
 * session_config config;
 * config.max_sessions = 1000;
 * auto manager = std::make_shared<ws_session_manager>(config);
 *
 * // Check before accepting new connection
 * if (manager->can_accept_connection()) {
 *     auto conn = create_ws_connection();
 *     auto conn_id = manager->add_connection(conn);
 *     if (!conn_id.empty()) {
 *         // Connection accepted
 *     }
 * }
 * @endcode
 */
class ws_session_manager : public session_manager_base<ws_connection> {
public:
    using ws_connection_ptr = std::shared_ptr<ws_connection>;
    using base_type = session_manager_base<ws_connection>;

    /**
     * @brief Constructs a WebSocket session manager.
     *
     * @param config Session management configuration
     */
    explicit ws_session_manager(const session_config& config = session_config())
        : base_type(config) {}

    // =========================================================================
    // Backward-compatible API (aliases to base class methods)
    // =========================================================================

    /**
     * @brief Adds a connection to the manager.
     *
     * Thread-safe operation that adds a connection with automatic or
     * provided ID. If max_sessions limit is reached, the connection
     * is rejected.
     *
     * @param conn Connection to add
     * @param conn_id Optional connection ID (auto-generated if empty)
     * @return Connection ID that was used (empty string if rejected)
     */
    auto add_connection(ws_connection_ptr conn, const std::string& conn_id = "")
        -> std::string {
        return add_session_with_id(std::move(conn), conn_id);
    }

    /**
     * @brief Removes a connection from the manager.
     *
     * Thread-safe operation that removes a connection by ID.
     *
     * @param conn_id Connection ID to remove
     * @return True if connection was found and removed
     */
    auto remove_connection(const std::string& conn_id) -> bool {
        return remove_session(conn_id);
    }

    /**
     * @brief Gets a connection by ID.
     *
     * Thread-safe read operation.
     *
     * @param conn_id Connection ID to lookup
     * @return Shared pointer to connection, or nullptr if not found
     */
    [[nodiscard]] auto get_connection(const std::string& conn_id) const
        -> ws_connection_ptr {
        return get_session(conn_id);
    }

    /**
     * @brief Gets all active connections.
     *
     * Thread-safe operation that returns a snapshot of all connections.
     *
     * @return Vector of all active connections
     */
    [[nodiscard]] auto get_all_connections() const -> std::vector<ws_connection_ptr> {
        return get_all_sessions();
    }

    /**
     * @brief Gets all connection IDs.
     *
     * Thread-safe operation that returns a list of all connection IDs.
     *
     * @return Vector of connection IDs
     */
    [[nodiscard]] auto get_all_connection_ids() const -> std::vector<std::string> {
        return get_all_session_ids();
    }

    /**
     * @brief Gets the current connection count.
     *
     * @return Number of active connections
     */
    [[nodiscard]] auto get_connection_count() const -> size_t {
        return get_session_count();
    }

    /**
     * @brief Clears all connections.
     *
     * Thread-safe operation that removes all connections.
     */
    auto clear_all_connections() -> void { clear_all_sessions(); }

    /**
     * @brief Generates a unique connection ID.
     *
     * This is a static method that can be used to generate IDs externally.
     *
     * @return Generated connection ID
     */
    static auto generate_connection_id() -> std::string { return generate_id(); }
};

} // namespace kcenon::network::core
