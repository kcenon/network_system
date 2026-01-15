/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/core/session_info.h"
#include "kcenon/network/core/session_traits.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace kcenon::network::core {

/**
 * @struct session_config
 * @brief Configuration for session management
 */
struct session_config {
    size_t max_sessions{1000};
    std::chrono::milliseconds idle_timeout{std::chrono::minutes(5)};
    std::chrono::milliseconds cleanup_interval{std::chrono::seconds(30)};
    bool enable_backpressure{true};
    double backpressure_threshold{0.8};
};

/**
 * @class session_manager_base
 * @brief Thread-safe session lifecycle management template
 *
 * Generic session manager that works with any session type.
 * Behavior is customized via session_traits<SessionType>.
 *
 * Features:
 * - Thread-safe session tracking using shared_mutex
 * - Connection limit enforcement with backpressure
 * - Optional idle session cleanup (when traits enable activity tracking)
 * - Metrics collection (accepted, rejected, cleaned up)
 *
 * @tparam SessionType The session/connection type to manage
 *
 * Thread Safety:
 * - All methods are thread-safe using shared_mutex for concurrent reads
 *   and exclusive writes.
 *
 * Usage Example:
 * @code
 * session_config config;
 * config.max_sessions = 1000;
 *
 * auto manager = std::make_shared<session_manager_base<MySession>>(config);
 *
 * if (manager->can_accept_connection()) {
 *     auto session = create_session();
 *     manager->add_session(session, "session_123");
 * }
 * @endcode
 */
template <typename SessionType>
class session_manager_base {
public:
    using session_ptr = std::shared_ptr<SessionType>;
    using traits = session_traits<SessionType>;
    using info_type = session_info_t<SessionType>;

    /**
     * @brief Constructs a session manager with the given configuration
     * @param config Session management configuration
     */
    explicit session_manager_base(const session_config& config = session_config())
        : config_(config)
        , session_count_(0)
        , total_accepted_(0)
        , total_rejected_(0)
        , total_cleaned_up_(0) {}

    // Non-copyable, non-movable
    session_manager_base(const session_manager_base&) = delete;
    session_manager_base& operator=(const session_manager_base&) = delete;
    session_manager_base(session_manager_base&&) = delete;
    session_manager_base& operator=(session_manager_base&&) = delete;

    virtual ~session_manager_base() = default;

    // =========================================================================
    // Connection Acceptance
    // =========================================================================

    /**
     * @brief Check if new connection can be accepted
     * @return true if under max_sessions limit
     */
    [[nodiscard]] auto can_accept_connection() const -> bool {
        return session_count_.load(std::memory_order_acquire) < config_.max_sessions;
    }

    /**
     * @brief Check if backpressure should be applied
     * @return true if session count exceeds backpressure threshold
     */
    [[nodiscard]] auto is_backpressure_active() const -> bool {
        if (!config_.enable_backpressure) {
            return false;
        }
        auto count = session_count_.load(std::memory_order_acquire);
        auto threshold =
            static_cast<size_t>(config_.max_sessions * config_.backpressure_threshold);
        return count >= threshold;
    }

    // =========================================================================
    // Session CRUD
    // =========================================================================

    /**
     * @brief Add a session to the manager
     * @param session Session to add
     * @param session_id Optional session ID (auto-generated if empty)
     * @return true if added, false if rejected (limit reached)
     */
    auto add_session(session_ptr session, const std::string& session_id = "") -> bool {
        if (!can_accept_connection()) {
            total_rejected_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        std::unique_lock lock(sessions_mutex_);
        std::string id = session_id.empty() ? generate_id() : session_id;
        active_sessions_.emplace(id, info_type(std::move(session)));
        session_count_.fetch_add(1, std::memory_order_release);
        total_accepted_.fetch_add(1, std::memory_order_relaxed);

        return true;
    }

    /**
     * @brief Add a session and return the assigned ID
     * @param session Session to add
     * @param session_id Optional session ID (auto-generated if empty)
     * @return Session ID if added, empty string if rejected
     *
     * This method is useful when auto-generating IDs and needing
     * to know the assigned ID for subsequent operations.
     */
    auto add_session_with_id(session_ptr session, const std::string& session_id = "")
        -> std::string {
        if (!can_accept_connection()) {
            total_rejected_.fetch_add(1, std::memory_order_relaxed);
            return "";
        }

        std::unique_lock lock(sessions_mutex_);
        std::string id = session_id.empty() ? generate_id() : session_id;
        active_sessions_.emplace(id, info_type(std::move(session)));
        session_count_.fetch_add(1, std::memory_order_release);
        total_accepted_.fetch_add(1, std::memory_order_relaxed);

        return id;
    }

    /**
     * @brief Remove session by ID
     * @param session_id Session ID to remove
     * @return true if removed
     */
    auto remove_session(const std::string& session_id) -> bool {
        std::unique_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            active_sessions_.erase(it);
            session_count_.fetch_sub(1, std::memory_order_release);
            return true;
        }
        return false;
    }

    /**
     * @brief Get session by ID
     * @param session_id Session ID to lookup
     * @return Session pointer, or nullptr if not found
     */
    [[nodiscard]] auto get_session(const std::string& session_id) const -> session_ptr {
        std::shared_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        return (it != active_sessions_.end()) ? it->second.session : nullptr;
    }

    /**
     * @brief Get all active sessions
     * @return Vector of session pointers
     */
    [[nodiscard]] auto get_all_sessions() const -> std::vector<session_ptr> {
        std::shared_lock lock(sessions_mutex_);
        std::vector<session_ptr> sessions;
        sessions.reserve(active_sessions_.size());
        for (const auto& [id, info] : active_sessions_) {
            sessions.push_back(info.session);
        }
        return sessions;
    }

    /**
     * @brief Get all session IDs
     * @return Vector of session IDs
     */
    [[nodiscard]] auto get_all_session_ids() const -> std::vector<std::string> {
        std::shared_lock lock(sessions_mutex_);
        std::vector<std::string> ids;
        ids.reserve(active_sessions_.size());
        for (const auto& [id, info] : active_sessions_) {
            ids.push_back(id);
        }
        return ids;
    }

    // =========================================================================
    // Activity Tracking (only available when traits::has_activity_tracking)
    // =========================================================================

    /**
     * @brief Update session activity timestamp
     * @param session_id Session ID to update
     *
     * Only available when session_traits<SessionType>::has_activity_tracking is true.
     */
    template <typename T = SessionType>
    auto update_activity(const std::string& session_id)
        -> std::enable_if_t<session_traits<T>::has_activity_tracking, void> {
        std::unique_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            it->second.update_activity();
        }
    }

    /**
     * @brief Get idle duration for a session
     * @param session_id Session ID to query
     * @return Idle duration, or nullopt if session not found
     *
     * Only available when session_traits<SessionType>::has_activity_tracking is true.
     */
    template <typename T = SessionType>
    [[nodiscard]] auto get_idle_duration(const std::string& session_id) const
        -> std::enable_if_t<session_traits<T>::has_activity_tracking,
                            std::optional<std::chrono::milliseconds>> {
        std::shared_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            return it->second.idle_duration();
        }
        return std::nullopt;
    }

    /**
     * @brief Cleanup idle sessions that exceeded idle_timeout
     * @return Number of sessions cleaned up
     *
     * Only available when session_traits<SessionType>::has_activity_tracking is true.
     */
    template <typename T = SessionType>
    auto cleanup_idle_sessions()
        -> std::enable_if_t<session_traits<T>::has_activity_tracking, size_t> {
        std::vector<std::pair<std::string, session_ptr>> to_remove;

        // Identify idle sessions under read lock
        {
            std::shared_lock lock(sessions_mutex_);
            for (const auto& [id, info] : active_sessions_) {
                if (info.idle_duration() > config_.idle_timeout) {
                    to_remove.emplace_back(id, info.session);
                }
            }
        }

        // Stop and remove idle sessions
        size_t removed = 0;
        for (const auto& [id, session] : to_remove) {
            if constexpr (traits::stop_on_clear) {
                if (session) {
                    try {
                        session->stop_session();
                    } catch (...) {
                    }
                }
            }
            if (remove_session(id)) {
                removed++;
            }
        }

        if (removed > 0) {
            total_cleaned_up_.fetch_add(removed, std::memory_order_relaxed);
        }
        return removed;
    }

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Clear all sessions
     *
     * If traits::stop_on_clear is true, calls stop_session() on each
     * session before removal.
     */
    auto clear_all_sessions() -> void {
        if constexpr (traits::stop_on_clear) {
            // Get sessions under read lock
            std::vector<session_ptr> sessions;
            {
                std::shared_lock lock(sessions_mutex_);
                sessions.reserve(active_sessions_.size());
                for (const auto& [id, info] : active_sessions_) {
                    sessions.push_back(info.session);
                }
            }

            // Stop sessions without holding lock
            for (auto& session : sessions) {
                if (session) {
                    try {
                        session->stop_session();
                    } catch (...) {
                    }
                }
            }
        }

        std::unique_lock lock(sessions_mutex_);
        active_sessions_.clear();
        session_count_.store(0, std::memory_order_release);
    }

    /**
     * @brief Stop all sessions gracefully
     *
     * Alias for clear_all_sessions() for backward compatibility with session_manager.
     */
    auto stop_all_sessions() -> void { clear_all_sessions(); }

    // =========================================================================
    // Metrics
    // =========================================================================

    /**
     * @brief Get current session count
     * @return Number of active sessions
     */
    [[nodiscard]] auto get_session_count() const -> size_t {
        return session_count_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get total accepted connections
     * @return Total number of accepted connections since creation
     */
    [[nodiscard]] auto get_total_accepted() const -> uint64_t {
        return total_accepted_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get total rejected connections
     * @return Total number of rejected connections since creation
     */
    [[nodiscard]] auto get_total_rejected() const -> uint64_t {
        return total_rejected_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get total cleaned up sessions
     * @return Total number of sessions cleaned up due to idle timeout
     */
    [[nodiscard]] auto get_total_cleaned_up() const -> uint64_t {
        return total_cleaned_up_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get current session utilization
     * @return Utilization ratio (0.0 to 1.0)
     */
    [[nodiscard]] auto get_utilization() const -> double {
        if (config_.max_sessions == 0) {
            return 0.0;
        }
        return static_cast<double>(session_count_.load(std::memory_order_acquire)) /
               config_.max_sessions;
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set maximum sessions
     * @param max_sessions New maximum session limit
     */
    auto set_max_sessions(size_t max_sessions) -> void {
        config_.max_sessions = max_sessions;
    }

    /**
     * @brief Get current configuration
     * @return Reference to session configuration
     */
    [[nodiscard]] auto get_config() const -> const session_config& { return config_; }

    // =========================================================================
    // Statistics (comprehensive view)
    // =========================================================================

    /**
     * @struct stats
     * @brief Comprehensive session manager statistics
     */
    struct stats {
        size_t active_sessions;
        size_t max_sessions;
        uint64_t total_accepted;
        uint64_t total_rejected;
        uint64_t total_cleaned_up;
        double utilization;
        bool backpressure_active;
        std::chrono::milliseconds idle_timeout;
    };

    /**
     * @brief Get comprehensive statistics
     * @return Stats struct with all metrics
     */
    [[nodiscard]] auto get_stats() const -> stats {
        return stats{.active_sessions = session_count_.load(std::memory_order_acquire),
                     .max_sessions = config_.max_sessions,
                     .total_accepted = total_accepted_.load(std::memory_order_relaxed),
                     .total_rejected = total_rejected_.load(std::memory_order_relaxed),
                     .total_cleaned_up = total_cleaned_up_.load(std::memory_order_relaxed),
                     .utilization = get_utilization(),
                     .backpressure_active = is_backpressure_active(),
                     .idle_timeout = config_.idle_timeout};
    }

    // =========================================================================
    // ID Generation (public for external use)
    // =========================================================================

    /**
     * @brief Generate a unique session ID
     * @return Generated ID with traits-defined prefix
     */
    static auto generate_id() -> std::string {
        static std::atomic<uint64_t> counter{0};
        return std::string(traits::id_prefix) +
               std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
    }

protected:
    session_config config_;
    mutable std::shared_mutex sessions_mutex_;
    std::unordered_map<std::string, info_type> active_sessions_;

    std::atomic<size_t> session_count_;
    std::atomic<uint64_t> total_accepted_;
    std::atomic<uint64_t> total_rejected_;
    std::atomic<uint64_t> total_cleaned_up_;
};

} // namespace kcenon::network::core
