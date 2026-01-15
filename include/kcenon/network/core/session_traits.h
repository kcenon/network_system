/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

namespace kcenon::network::session {
class messaging_session;
} // namespace kcenon::network::session

namespace kcenon::network::core {

class ws_connection;

/**
 * @struct session_traits
 * @brief Customization point for session manager behavior
 *
 * Specialize this struct for different session types to control:
 * - Activity tracking (idle timeout support)
 * - Cleanup behavior (graceful stop on clear)
 * - ID generation strategy
 *
 * @tparam SessionType The session type to customize behavior for
 */
template <typename SessionType>
struct session_traits {
    /// Enable activity timestamp tracking (required for idle cleanup)
    static constexpr bool has_activity_tracking = false;

    /// Call session's stop method when clearing all sessions
    static constexpr bool stop_on_clear = false;

    /// ID prefix for auto-generated session IDs
    static constexpr const char* id_prefix = "session_";
};

/**
 * @brief Session traits specialization for TCP messaging sessions
 *
 * TCP sessions have activity tracking enabled for idle timeout detection
 * and stop_on_clear enabled for graceful shutdown.
 */
template <>
struct session_traits<session::messaging_session> {
    static constexpr bool has_activity_tracking = true;
    static constexpr bool stop_on_clear = true;
    static constexpr const char* id_prefix = "session_";
};

/**
 * @brief Session traits specialization for WebSocket connections
 *
 * WebSocket connections have activity tracking disabled (no idle cleanup)
 * and stop_on_clear disabled (connections are simply removed).
 */
template <>
struct session_traits<ws_connection> {
    static constexpr bool has_activity_tracking = false;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "ws_conn_";
};

} // namespace kcenon::network::core
