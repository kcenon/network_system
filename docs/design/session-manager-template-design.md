# SessionManager<T> Template Design

> **Issue**: #461 (Phase 2.2 of #412)
> **Status**: Draft
> **Author**: TBD
> **Date**: 2026-01-15

## 1. Executive Summary

This document outlines the design for consolidating `session_manager` and `ws_session_manager` into a unified `session_manager_base<T>` template class, reducing code duplication by approximately 70% while maintaining protocol-specific functionality through traits-based customization.

## 2. Current State Analysis

### 2.1 Existing Implementations

| Component | File | Purpose |
|-----------|------|---------|
| `session_manager` | `core/session_manager.h` | TCP session management |
| `ws_session_manager` | `core/ws_session_manager.h` | WebSocket connection management |

### 2.2 Common Functionality (~80%)

```cpp
// Shared across both implementations
bool can_accept_connection() const;
bool is_backpressure_active() const;
bool add_session/connection(...);
bool remove_session/connection(const std::string& id);
T get_session/connection(const std::string& id) const;
std::vector<T> get_all_sessions/connections() const;
size_t get_session/connection_count() const;
uint64_t get_total_accepted() const;
uint64_t get_total_rejected() const;
```

### 2.3 Protocol-Specific Differences

| Feature | session_manager (TCP) | ws_session_manager (WS) |
|---------|----------------------|-------------------------|
| **Storage** | `session_info` with timestamps | Direct pointer storage |
| **Activity tracking** | Yes (`last_activity`) | No |
| **Idle cleanup** | Yes (`cleanup_idle_sessions`) | No |
| **Stop on clear** | Yes (`stop_session()` called) | No |
| **Add return type** | `bool` | `std::string` (ID) |
| **ID generation** | Private method | Static public method |
| **Stats struct** | Yes | No |
| **Utilization** | Yes (`get_utilization()`) | No |

## 3. Proposed Design

### 3.1 Architecture Overview

```
session_manager_base<SessionType, Traits>
           │
           ├── session_traits<SessionType> (customization point)
           │
           ├── session_manager = session_manager_base<messaging_session>
           │       └── session_traits<messaging_session> (activity tracking enabled)
           │
           └── ws_session_manager = session_manager_base<ws_connection>
                   └── session_traits<ws_connection> (activity tracking disabled)
```

### 3.2 Session Traits

```cpp
namespace kcenon::network::core {

/**
 * @brief Customization point for session manager behavior
 *
 * Specialize this struct for different session types to control:
 * - Activity tracking (idle timeout support)
 * - Cleanup behavior (graceful stop on clear)
 * - ID generation strategy
 */
template<typename SessionType>
struct session_traits {
    /// Enable activity timestamp tracking (required for idle cleanup)
    static constexpr bool has_activity_tracking = false;

    /// Call session's stop method when clearing all sessions
    static constexpr bool stop_on_clear = false;

    /// ID prefix for auto-generated session IDs
    static constexpr const char* id_prefix = "session_";

    /// Stop method name (for concepts, if using C++20)
    // Requires: void SessionType::stop_session() or equivalent
};

// Specialization for TCP messaging sessions
template<>
struct session_traits<session::messaging_session> {
    static constexpr bool has_activity_tracking = true;
    static constexpr bool stop_on_clear = true;
    static constexpr const char* id_prefix = "session_";
};

// Specialization for WebSocket connections
template<>
struct session_traits<ws_connection> {
    static constexpr bool has_activity_tracking = false;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "ws_conn_";
};

} // namespace kcenon::network::core
```

### 3.3 Session Info Wrapper

```cpp
namespace kcenon::network::core {

/**
 * @brief Wrapper for session with optional activity tracking
 *
 * Uses conditional member based on traits to avoid storage overhead
 * when activity tracking is disabled.
 */
template<typename SessionType, bool HasActivityTracking>
struct session_info_base;

// Specialization with activity tracking
template<typename SessionType>
struct session_info_base<SessionType, true> {
    std::shared_ptr<SessionType> session;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;

    explicit session_info_base(std::shared_ptr<SessionType> sess)
        : session(std::move(sess))
        , created_at(std::chrono::steady_clock::now())
        , last_activity(created_at)
    {}

    void update_activity() {
        last_activity = std::chrono::steady_clock::now();
    }

    [[nodiscard]] auto idle_duration() const -> std::chrono::milliseconds {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_activity);
    }
};

// Specialization without activity tracking (minimal overhead)
template<typename SessionType>
struct session_info_base<SessionType, false> {
    std::shared_ptr<SessionType> session;

    explicit session_info_base(std::shared_ptr<SessionType> sess)
        : session(std::move(sess))
    {}
};

// Convenience alias
template<typename SessionType>
using session_info_t = session_info_base<
    SessionType,
    session_traits<SessionType>::has_activity_tracking
>;

} // namespace kcenon::network::core
```

### 3.4 Base Template Class

```cpp
namespace kcenon::network::core {

/**
 * @brief Thread-safe session lifecycle management
 *
 * Generic session manager that works with any session type.
 * Behavior is customized via session_traits<SessionType>.
 *
 * @tparam SessionType The session/connection type to manage
 */
template<typename SessionType>
class session_manager_base {
public:
    using session_ptr = std::shared_ptr<SessionType>;
    using traits = session_traits<SessionType>;
    using info_type = session_info_t<SessionType>;

    explicit session_manager_base(const session_config& config = session_config())
        : config_(config)
        , session_count_(0)
        , total_accepted_(0)
        , total_rejected_(0)
        , total_cleaned_up_(0)
    {}

    //
    // Connection Acceptance
    //

    [[nodiscard]] auto can_accept_connection() const -> bool {
        return session_count_.load(std::memory_order_acquire) < config_.max_sessions;
    }

    [[nodiscard]] auto is_backpressure_active() const -> bool {
        if (!config_.enable_backpressure) {
            return false;
        }
        auto count = session_count_.load(std::memory_order_acquire);
        auto threshold = static_cast<size_t>(
            config_.max_sessions * config_.backpressure_threshold);
        return count >= threshold;
    }

    //
    // Session CRUD
    //

    /**
     * @brief Add a session to the manager
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

    [[nodiscard]] auto get_session(const std::string& session_id) const -> session_ptr {
        std::shared_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        return (it != active_sessions_.end()) ? it->second.session : nullptr;
    }

    [[nodiscard]] auto get_all_sessions() const -> std::vector<session_ptr> {
        std::shared_lock lock(sessions_mutex_);
        std::vector<session_ptr> sessions;
        sessions.reserve(active_sessions_.size());
        for (const auto& [id, info] : active_sessions_) {
            sessions.push_back(info.session);
        }
        return sessions;
    }

    [[nodiscard]] auto get_all_session_ids() const -> std::vector<std::string> {
        std::shared_lock lock(sessions_mutex_);
        std::vector<std::string> ids;
        ids.reserve(active_sessions_.size());
        for (const auto& [id, info] : active_sessions_) {
            ids.push_back(id);
        }
        return ids;
    }

    //
    // Activity Tracking (only available when traits::has_activity_tracking)
    //

    template<typename T = SessionType>
    auto update_activity(const std::string& session_id)
        -> std::enable_if_t<session_traits<T>::has_activity_tracking, void> {
        std::unique_lock lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            it->second.update_activity();
        }
    }

    template<typename T = SessionType>
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

    template<typename T = SessionType>
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
                    try { session->stop_session(); } catch (...) {}
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

    //
    // Lifecycle Management
    //

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
                    try { session->stop_session(); } catch (...) {}
                }
            }
        }

        std::unique_lock lock(sessions_mutex_);
        active_sessions_.clear();
        session_count_.store(0, std::memory_order_release);
    }

    //
    // Metrics
    //

    [[nodiscard]] auto get_session_count() const -> size_t {
        return session_count_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto get_total_accepted() const -> uint64_t {
        return total_accepted_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_total_rejected() const -> uint64_t {
        return total_rejected_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_total_cleaned_up() const -> uint64_t {
        return total_cleaned_up_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto get_utilization() const -> double {
        if (config_.max_sessions == 0) return 0.0;
        return static_cast<double>(session_count_.load(std::memory_order_acquire))
               / config_.max_sessions;
    }

    //
    // Configuration
    //

    auto set_max_sessions(size_t max_sessions) -> void {
        config_.max_sessions = max_sessions;
    }

    [[nodiscard]] auto get_config() const -> const session_config& {
        return config_;
    }

    //
    // Statistics (comprehensive view)
    //

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

    [[nodiscard]] auto get_stats() const -> stats {
        return stats{
            .active_sessions = session_count_.load(std::memory_order_acquire),
            .max_sessions = config_.max_sessions,
            .total_accepted = total_accepted_.load(std::memory_order_relaxed),
            .total_rejected = total_rejected_.load(std::memory_order_relaxed),
            .total_cleaned_up = total_cleaned_up_.load(std::memory_order_relaxed),
            .utilization = get_utilization(),
            .backpressure_active = is_backpressure_active(),
            .idle_timeout = config_.idle_timeout
        };
    }

    //
    // ID Generation (public for external use)
    //

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
```

### 3.5 Type Aliases for Backward Compatibility

```cpp
namespace kcenon::network::core {

// Backward-compatible type aliases
using session_manager = session_manager_base<session::messaging_session>;
using ws_session_manager = session_manager_base<ws_connection>;

// For explicit template usage
template<typename T>
using generic_session_manager = session_manager_base<T>;

} // namespace kcenon::network::core
```

## 4. API Compatibility

### 4.1 Breaking Changes

**None** - All existing APIs remain available through type aliases.

### 4.2 API Mapping

| Current API | New API | Notes |
|-------------|---------|-------|
| `session_manager::add_session()` | `session_manager_base::add_session()` | Same signature |
| `ws_session_manager::add_connection()` | `session_manager_base::add_session_with_id()` | Returns ID |
| `ws_session_manager::remove_connection()` | `session_manager_base::remove_session()` | Same behavior |
| `ws_session_manager::get_connection()` | `session_manager_base::get_session()` | Same behavior |
| `ws_session_manager::get_all_connections()` | `session_manager_base::get_all_sessions()` | Same behavior |
| `ws_session_manager::get_all_connection_ids()` | `session_manager_base::get_all_session_ids()` | Same behavior |
| `ws_session_manager::get_connection_count()` | `session_manager_base::get_session_count()` | Same behavior |
| `ws_session_manager::clear_all_connections()` | `session_manager_base::clear_all_sessions()` | Same behavior |

### 4.3 Deprecation Strategy

For gradual migration, the original header files can include deprecation warnings:

```cpp
// In ws_session_manager.h (deprecated wrapper)
#pragma once
#include "kcenon/network/core/session_manager_base.h"

namespace kcenon::network::core {

// Deprecated: Use session_manager_base<ws_connection> directly
class [[deprecated("Use session_manager_base<ws_connection> instead")]]
ws_session_manager_legacy : public session_manager_base<ws_connection> {
public:
    using session_manager_base<ws_connection>::session_manager_base;

    // Backward-compatible method aliases
    auto add_connection(ws_connection_ptr conn, const std::string& id = "") {
        return add_session_with_id(std::move(conn), id);
    }
    // ... other aliases
};

// Type alias (preferred)
using ws_session_manager = session_manager_base<ws_connection>;

} // namespace kcenon::network::core
```

## 5. File Structure

### 5.1 New Files

```
include/kcenon/network/core/
├── session_traits.h          # session_traits<T> definitions
├── session_info.h            # session_info_base<T, bool> template
├── session_manager_base.h    # session_manager_base<T> template
├── session_manager.h         # (modified) includes base, defines alias
└── ws_session_manager.h      # (modified) includes base, defines alias
```

### 5.2 Include Dependencies

```
session_manager_base.h
    ├── session_traits.h
    ├── session_info.h
    └── session_config (from existing session_manager.h)

session_manager.h
    ├── session_manager_base.h
    └── messaging_session.h

ws_session_manager.h
    ├── session_manager_base.h
    └── (forward declaration of ws_connection)
```

## 6. Testing Strategy

### 6.1 Unit Tests

1. **Template functionality tests**
   - Test with mock session type
   - Verify traits-based behavior switching

2. **TCP session manager tests**
   - Activity tracking
   - Idle cleanup
   - Stop on clear

3. **WebSocket session manager tests**
   - No activity tracking
   - ID generation
   - No stop on clear

### 6.2 Regression Tests

- Run all existing `session_manager_test.cpp` tests unchanged
- Verify API compatibility

## 7. Implementation Plan

| Step | Description | Effort |
|------|-------------|--------|
| 1 | Create `session_traits.h` | 0.5 day |
| 2 | Create `session_info.h` | 0.5 day |
| 3 | Create `session_manager_base.h` | 1 day |
| 4 | Update `session_manager.h` to use template | 0.5 day |
| 5 | Update `ws_session_manager.h` to use template | 0.5 day |
| 6 | Update dependent code | 0.5 day |
| 7 | Run tests and fix issues | 0.5 day |

**Total: ~4 days** (Phase 2.3)

## 8. Acceptance Criteria

- [ ] Template interface documented with method signatures
- [ ] Specialization points identified for TCP and WebSocket
- [ ] Session traits design completed
- [ ] Backward compatibility strategy defined (type aliases)
- [ ] Design reviewed and approved

## 9. Open Questions

1. **C++20 Concepts**: Should we use concepts for cleaner SFINAE?
   - Pro: Cleaner error messages, more readable code
   - Con: May require C++20 compiler support

2. **session_info exposure**: Should `session_info_t` be public API or internal?
   - Current `session_manager` exposes `session_info` struct
   - Recommendation: Keep public for compatibility

## 10. References

- Issue #412: [REFACTOR] Phase 2.0: Consolidate Session Manager Implementations
- Issue #461: [REFACTOR] Phase 2.2: Design unified SessionManager<T> template
- `docs/design/crtp-analysis.md`: Related CRTP refactoring analysis
