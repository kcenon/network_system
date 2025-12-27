/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "kcenon/network/session/messaging_session.h"

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
 * @struct session_info
 * @brief Information about a managed session including activity tracking
 */
struct session_info {
    std::shared_ptr<kcenon::network::session::messaging_session> session;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;

    explicit session_info(std::shared_ptr<kcenon::network::session::messaging_session> sess)
        : session(std::move(sess))
        , created_at(std::chrono::steady_clock::now())
        , last_activity(created_at)
    {
    }
};

/**
 * @class session_manager
 * @brief Thread-safe session lifecycle management with backpressure
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
class session_manager {
public:
    using session_ptr = std::shared_ptr<kcenon::network::session::messaging_session>;

    explicit session_manager(const session_config& config = session_config())
        : config_(config)
        , session_count_(0)
        , total_accepted_(0)
        , total_rejected_(0)
        , total_cleaned_up_(0)
    {
    }

    /**
     * @brief Check if new connection can be accepted
     * @return true if under max_sessions limit
     */
    [[nodiscard]] bool can_accept_connection() const {
        return session_count_.load(std::memory_order_acquire) < config_.max_sessions;
    }

    /**
     * @brief Check if backpressure should be applied
     * @return true if session count exceeds backpressure threshold
     */
    [[nodiscard]] bool is_backpressure_active() const {
        if (!config_.enable_backpressure) {
            return false;
        }

        auto count = session_count_.load(std::memory_order_acquire);
        auto threshold = static_cast<size_t>(config_.max_sessions * config_.backpressure_threshold);

        return count >= threshold;
    }

    /**
     * @brief Get current session utilization
     * @return Utilization ratio (0.0 to 1.0)
     */
    [[nodiscard]] double get_utilization() const {
        if (config_.max_sessions == 0) {
            return 0.0;
        }

        auto count = session_count_.load(std::memory_order_acquire);
        return static_cast<double>(count) / config_.max_sessions;
    }

    /**
     * @brief Add session to manager (thread-safe)
     * @param session Session to add
     * @param session_id Optional session ID
     * @return true if added successfully, false if limit reached
     */
    bool add_session(session_ptr session, const std::string& session_id = "") {
        if (!can_accept_connection()) {
            total_rejected_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

        // Generate ID if not provided
        std::string id = session_id.empty() ?
            generate_session_id() : session_id;

        // Add to active sessions
        active_sessions_.emplace(id, session_info(session));
        session_count_.fetch_add(1, std::memory_order_release);
        total_accepted_.fetch_add(1, std::memory_order_relaxed);

        return true;
    }

    /**
     * @brief Remove session (thread-safe)
     * @param session_id Session ID to remove
     * @return true if removed
     */
    bool remove_session(const std::string& session_id) {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            active_sessions_.erase(it);
            session_count_.fetch_sub(1, std::memory_order_release);
            return true;
        }

        return false;
    }

    /**
     * @brief Get session by ID (thread-safe)
     */
    [[nodiscard]] session_ptr get_session(const std::string& session_id) const {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            return it->second.session;
        }

        return nullptr;
    }

    /**
     * @brief Update session activity timestamp (thread-safe)
     * @param session_id Session ID to update
     *
     * Call this method when a session sends or receives data
     * to keep the session alive and prevent idle timeout.
     */
    void update_activity(const std::string& session_id) {
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);

        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            it->second.last_activity = std::chrono::steady_clock::now();
        }
    }

    /**
     * @brief Get all active sessions (thread-safe)
     */
    [[nodiscard]] std::vector<session_ptr> get_all_sessions() const {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

        std::vector<session_ptr> sessions;
        sessions.reserve(active_sessions_.size());

        for (const auto& [id, info] : active_sessions_) {
            sessions.push_back(info.session);
        }

        return sessions;
    }

    /**
     * @brief Stop all sessions gracefully (thread-safe)
     */
    void stop_all_sessions() {
        // Get sessions under read lock
        std::vector<session_ptr> sessions;
        {
            std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
            sessions.reserve(active_sessions_.size());

            for (const auto& [id, info] : active_sessions_) {
                sessions.push_back(info.session);
            }
        }

        // Stop sessions without holding lock
        for (auto& session : sessions) {
            try {
                if (session) {
                    session->stop_session();
                }
            } catch (...) {
                // Ignore errors during shutdown
            }
        }

        // Clear all sessions
        std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
        active_sessions_.clear();
        session_count_.store(0, std::memory_order_release);
    }

    /**
     * @brief Cleanup idle sessions
     * @return Number of sessions cleaned up
     *
     * Identifies sessions that have been idle longer than the configured
     * idle_timeout, gracefully stops them, and removes them from the manager.
     *
     * This method should be called periodically, either manually or via
     * a timer mechanism.
     */
    size_t cleanup_idle_sessions() {
        auto now = std::chrono::steady_clock::now();
        std::vector<std::pair<std::string, session_ptr>> to_remove;

        // Identify idle sessions under read lock
        {
            std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

            for (const auto& [id, info] : active_sessions_) {
                auto idle_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - info.last_activity);

                if (idle_duration > config_.idle_timeout) {
                    to_remove.emplace_back(id, info.session);
                }
            }
        }

        // Gracefully stop and remove idle sessions
        size_t removed = 0;
        for (const auto& [id, session] : to_remove) {
            // Stop session gracefully before removal
            if (session) {
                try {
                    session->stop_session();
                } catch (...) {
                    // Ignore errors during cleanup
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

    /**
     * @brief Get session info including activity timestamps (thread-safe)
     * @param session_id Session ID to query
     * @return Optional session_info if found
     */
    [[nodiscard]] std::optional<session_info> get_session_info(const std::string& session_id) const {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            return it->second;
        }

        return std::nullopt;
    }

    /**
     * @brief Get idle duration for a session (thread-safe)
     * @param session_id Session ID to query
     * @return Idle duration, or nullopt if session not found
     */
    [[nodiscard]] std::optional<std::chrono::milliseconds> get_idle_duration(
        const std::string& session_id) const {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);

        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second.last_activity);
        }

        return std::nullopt;
    }

    /**
     * @brief Get statistics
     */
    struct stats {
        size_t active_sessions;
        size_t max_sessions;
        size_t total_accepted;
        size_t total_rejected;
        size_t total_cleaned_up;
        double utilization;
        bool backpressure_active;
        std::chrono::milliseconds idle_timeout;
    };

    [[nodiscard]] stats get_stats() const {
        stats s;
        s.active_sessions = session_count_.load(std::memory_order_acquire);
        s.max_sessions = config_.max_sessions;
        s.total_accepted = total_accepted_.load(std::memory_order_acquire);
        s.total_rejected = total_rejected_.load(std::memory_order_acquire);
        s.total_cleaned_up = total_cleaned_up_.load(std::memory_order_acquire);
        s.utilization = get_utilization();
        s.backpressure_active = is_backpressure_active();
        s.idle_timeout = config_.idle_timeout;

        return s;
    }

    /**
     * @brief Set maximum sessions
     */
    void set_max_sessions(size_t max_sessions) {
        config_.max_sessions = max_sessions;
    }

    /**
     * @brief Get current session count
     */
    [[nodiscard]] size_t get_session_count() const {
        return session_count_.load(std::memory_order_acquire);
    }

private:
    std::string generate_session_id() {
        static std::atomic<uint64_t> counter{0};
        return "session_" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
    }

private:
    session_config config_;

    mutable std::shared_mutex sessions_mutex_;
    std::unordered_map<std::string, session_info> active_sessions_;

    std::atomic<size_t> session_count_;
    std::atomic<uint64_t> total_accepted_;
    std::atomic<uint64_t> total_rejected_;
    std::atomic<uint64_t> total_cleaned_up_;
};

} // namespace kcenon::network::core
