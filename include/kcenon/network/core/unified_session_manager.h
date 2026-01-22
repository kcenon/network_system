/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/core/session_handle.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::network::core {

/**
 * @struct unified_session_config
 * @brief Configuration for unified session management
 */
struct unified_session_config {
    size_t max_sessions{1000};
    std::chrono::milliseconds idle_timeout{std::chrono::minutes(5)};
    std::chrono::milliseconds cleanup_interval{std::chrono::seconds(30)};
    bool enable_backpressure{true};
    double backpressure_threshold{0.8};
};

/**
 * @class unified_session_manager
 * @brief Type-erased session manager that handles any session type
 *
 * This class replaces the template-based session_manager_base<SessionType>
 * with a single, non-template implementation using Type Erasure.
 *
 * ## Benefits
 *
 * - **Reduced compilation time**: No template instantiation in user code
 * - **Smaller binary size**: Single implementation instead of per-type
 * - **Simpler API**: Single class handles all session types
 * - **Heterogeneous storage**: Different session types in same manager
 *
 * ## Usage Example
 *
 * ```cpp
 * unified_session_manager manager;
 *
 * // Add any session type
 * auto tcp = std::make_shared<tcp_session>(...);
 * manager.add_session(tcp, "tcp_1");
 *
 * auto ws = std::make_shared<ws_session>(...);
 * manager.add_session(ws, "ws_1");
 *
 * // Type recovery when needed
 * if (auto handle = manager.get_session("tcp_1")) {
 *     if (auto* tcp_ptr = handle->as<tcp_session>()) {
 *         tcp_ptr->set_tcp_nodelay(true);  // Protocol-specific
 *     }
 * }
 *
 * // Iteration over all sessions
 * manager.for_each([](session_handle& handle) {
 *     if (handle.is_connected()) {
 *         handle.send(make_ping_packet());
 *     }
 * });
 * ```
 *
 * ## Thread Safety
 *
 * All methods are thread-safe using shared_mutex for concurrent reads
 * and exclusive writes.
 *
 * @see session_handle
 * @see session_concept
 */
class unified_session_manager {
public:
    /**
     * @brief Constructs a unified session manager with default configuration
     */
    unified_session_manager();

    /**
     * @brief Constructs a unified session manager with custom configuration
     * @param config Session management configuration
     */
    explicit unified_session_manager(const unified_session_config& config);

    ~unified_session_manager();

    // Non-copyable, non-movable
    unified_session_manager(const unified_session_manager&) = delete;
    unified_session_manager& operator=(const unified_session_manager&) = delete;
    unified_session_manager(unified_session_manager&&) = delete;
    unified_session_manager& operator=(unified_session_manager&&) = delete;

    // =========================================================================
    // Connection Acceptance
    // =========================================================================

    /**
     * @brief Check if new connection can be accepted
     * @return true if under max_sessions limit
     */
    [[nodiscard]] auto can_accept_connection() const -> bool;

    /**
     * @brief Check if backpressure should be applied
     * @return true if session count exceeds backpressure threshold
     */
    [[nodiscard]] auto is_backpressure_active() const -> bool;

    // =========================================================================
    // Session CRUD - Type-Erased API
    // =========================================================================

    /**
     * @brief Add a session using type erasure
     * @tparam SessionType The concrete session type
     * @param session Shared pointer to the session
     * @param session_id Session ID (auto-generated if empty)
     * @return true if added, false if rejected (limit reached)
     *
     * This template method is the entry point that converts a concrete
     * session type into the type-erased session_handle.
     */
    template <typename SessionType>
    auto add_session(std::shared_ptr<SessionType> session,
                     const std::string& session_id = "") -> bool {
        if (!can_accept_connection()) {
            increment_rejected();
            return false;
        }

        auto handle = session_handle(std::move(session));
        return add_session_impl(std::move(handle), session_id);
    }

    /**
     * @brief Add a pre-wrapped session_handle
     * @param handle The session handle to add
     * @param session_id Session ID (auto-generated if empty)
     * @return true if added, false if rejected
     */
    auto add_session(session_handle handle, const std::string& session_id = "")
        -> bool;

    /**
     * @brief Add a session and return the assigned ID
     * @tparam SessionType The concrete session type
     * @param session Shared pointer to the session
     * @param session_id Session ID (auto-generated if empty)
     * @return Session ID if added, empty string if rejected
     */
    template <typename SessionType>
    auto add_session_with_id(std::shared_ptr<SessionType> session,
                             const std::string& session_id = "") -> std::string {
        if (!can_accept_connection()) {
            increment_rejected();
            return "";
        }

        auto handle = session_handle(std::move(session));
        return add_session_with_id_impl(std::move(handle), session_id);
    }

    /**
     * @brief Remove session by ID
     * @param session_id Session ID to remove
     * @return true if removed, false if not found
     */
    auto remove_session(const std::string& session_id) -> bool;

    /**
     * @brief Get session handle by ID (non-owning reference)
     * @param session_id Session ID to lookup
     * @return Pointer to session_handle, or nullptr if not found
     *
     * The returned pointer is valid only while the session exists in the manager.
     * Use with_session() for safer access patterns.
     */
    [[nodiscard]] auto get_session(const std::string& session_id) -> session_handle*;

    /**
     * @brief Get session handle by ID (const version)
     * @param session_id Session ID to lookup
     * @return Const pointer to session_handle, or nullptr if not found
     */
    [[nodiscard]] auto get_session(const std::string& session_id) const
        -> const session_handle*;

    /**
     * @brief Execute a callback with a session (safer than get_session)
     * @param session_id Session ID to lookup
     * @param callback Function to execute with the session
     * @return true if session found and callback executed
     *
     * This method is safer than get_session() because it holds the lock
     * during callback execution, preventing the session from being removed.
     */
    auto with_session(const std::string& session_id,
                      const std::function<void(session_handle&)>& callback) -> bool;

    /**
     * @brief Check if a session exists
     * @param session_id Session ID to check
     * @return true if session exists
     */
    [[nodiscard]] auto has_session(const std::string& session_id) const -> bool;

    /**
     * @brief Get all session IDs
     * @return Vector of session IDs
     */
    [[nodiscard]] auto get_all_session_ids() const -> std::vector<std::string>;

    // =========================================================================
    // Iteration
    // =========================================================================

    /**
     * @brief Execute a callback for each session
     * @param callback Function to execute with each session handle
     *
     * Callback receives a mutable reference to allow session operations.
     * Iteration is performed under read lock for concurrent safety.
     */
    auto for_each(const std::function<void(session_handle&)>& callback) -> void;

    /**
     * @brief Execute a callback for each session (const version)
     * @param callback Function to execute with each session handle
     */
    auto for_each(const std::function<void(const session_handle&)>& callback) const
        -> void;

    /**
     * @brief Broadcast data to all connected sessions
     * @param data Data to send (will be moved)
     * @return Number of sessions that received the data
     */
    auto broadcast(std::vector<uint8_t> data) -> size_t;

    // =========================================================================
    // Activity Tracking & Cleanup
    // =========================================================================

    /**
     * @brief Update activity timestamp for a session
     * @param session_id Session ID to update
     * @return true if session found and updated
     */
    auto update_activity(const std::string& session_id) -> bool;

    /**
     * @brief Cleanup idle sessions that exceeded idle_timeout
     * @return Number of sessions cleaned up
     */
    auto cleanup_idle_sessions() -> size_t;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Clear all sessions
     *
     * Calls stop() on each session before removal.
     */
    auto clear_all_sessions() -> void;

    /**
     * @brief Stop all sessions (alias for clear_all_sessions)
     */
    auto stop_all_sessions() -> void;

    // =========================================================================
    // Metrics
    // =========================================================================

    /**
     * @brief Get current session count
     * @return Number of active sessions
     */
    [[nodiscard]] auto get_session_count() const -> size_t;

    /**
     * @brief Get total accepted connections
     * @return Total number of accepted connections
     */
    [[nodiscard]] auto get_total_accepted() const -> uint64_t;

    /**
     * @brief Get total rejected connections
     * @return Total number of rejected connections
     */
    [[nodiscard]] auto get_total_rejected() const -> uint64_t;

    /**
     * @brief Get total cleaned up sessions
     * @return Total sessions removed due to idle timeout
     */
    [[nodiscard]] auto get_total_cleaned_up() const -> uint64_t;

    /**
     * @brief Get current session utilization
     * @return Utilization ratio (0.0 to 1.0)
     */
    [[nodiscard]] auto get_utilization() const -> double;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set maximum sessions
     * @param max_sessions New maximum session limit
     */
    auto set_max_sessions(size_t max_sessions) -> void;

    /**
     * @brief Get current configuration
     * @return Reference to session configuration
     */
    [[nodiscard]] auto get_config() const -> const unified_session_config&;

    // =========================================================================
    // Statistics
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
    [[nodiscard]] auto get_stats() const -> stats;

    // =========================================================================
    // ID Generation
    // =========================================================================

    /**
     * @brief Generate a unique session ID
     * @param prefix Optional prefix for the ID
     * @return Generated unique ID
     */
    static auto generate_id(const std::string& prefix = "session_") -> std::string;

private:
    class impl;
    std::unique_ptr<impl> impl_;

    auto add_session_impl(session_handle handle, const std::string& session_id)
        -> bool;
    auto add_session_with_id_impl(session_handle handle,
                                  const std::string& session_id) -> std::string;
    auto increment_rejected() -> void;
};

} // namespace kcenon::network::core
