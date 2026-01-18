/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/core/session_manager_base.h"
#include "kcenon/network/session/messaging_session.h"

namespace kcenon::network::core {

/**
 * @brief Session info struct for backward compatibility
 *
 * This struct is provided for backward compatibility with code that
 * accesses session_info directly. New code should use the template-based
 * session_info_t<SessionType> instead.
 */
struct session_info {
    std::shared_ptr<kcenon::network::session::messaging_session> session;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;

    explicit session_info(
        std::shared_ptr<kcenon::network::session::messaging_session> sess)
        : session(std::move(sess))
        , created_at(std::chrono::steady_clock::now())
        , last_activity(created_at) {}
};

/**
 * @class session_manager
 * @brief Thread-safe TCP session lifecycle management with backpressure
 *
 * This is a type alias for session_manager_base<messaging_session> that
 * provides backward compatibility with existing code while leveraging
 * the unified template implementation.
 *
 * This manager provides:
 * - Thread-safe session tracking
 * - Connection limit enforcement
 * - Idle session cleanup
 * - Backpressure signaling
 * - Session metrics
 *
 * ### Thread Safety
 * All methods are thread-safe using shared_mutex for concurrent reads
 * and exclusive writes.
 *
 * ### Production Usage
 * @code
 * session_config config;
 * config.max_sessions = 1000;
 * config.idle_timeout = std::chrono::minutes(5);
 *
 * auto manager = std::make_shared<session_manager>(config);
 *
 * // Check before accepting new connection
 * if (manager->can_accept_connection()) {
 *     auto session = create_session();
 *     manager->add_session(session);
 * } else {
 *     log_warning("Connection rejected: max sessions reached");
 *     socket.close();
 * }
 *
 * // Periodic cleanup
 * manager->cleanup_idle_sessions();
 * @endcode
 */
class session_manager : public session_manager_base<session::messaging_session> {
public:
    using session_ptr = std::shared_ptr<session::messaging_session>;
    using base_type = session_manager_base<session::messaging_session>;

    explicit session_manager(const session_config& config = session_config())
        : base_type(config) {}

    /**
     * @brief Get session info including activity timestamps
     * @param session_id Session ID to query
     * @return Optional session_info if found
     *
     * This method is provided for backward compatibility. It converts
     * the internal session_info_t to the legacy session_info struct.
     */
    [[nodiscard]] auto get_session_info(const std::string& session_id) const
        -> std::optional<session_info> {
        std::shared_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            session_info info(it->second.session);
            info.created_at = it->second.created_at;
            info.last_activity = it->second.last_activity;
            return info;
        }
        return std::nullopt;
    }

};

} // namespace kcenon::network::core
