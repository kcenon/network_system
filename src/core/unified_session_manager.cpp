/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "kcenon/network/core/unified_session_manager.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace kcenon::network::core {

/**
 * @class unified_session_manager::impl
 * @brief PIMPL implementation hiding all template-related code
 *
 * This inner class contains all the implementation details, keeping
 * the header minimal and avoiding template exposure in user code.
 */
class unified_session_manager::impl {
public:
    explicit impl(const unified_session_config& config)
        : config_(config)
        , session_count_(0)
        , total_accepted_(0)
        , total_rejected_(0)
        , total_cleaned_up_(0) {}

    ~impl() { clear_all_sessions(); }

    // =========================================================================
    // Connection Acceptance
    // =========================================================================

    [[nodiscard]] auto can_accept_connection() const -> bool {
        return session_count_.load(std::memory_order_acquire) < config_.max_sessions;
    }

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

    auto add_session(session_handle handle, const std::string& session_id) -> bool {
        if (!can_accept_connection()) {
            total_rejected_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        std::unique_lock lock(sessions_mutex_);
        std::string id = session_id.empty() ? generate_id("session_") : session_id;

        auto [it, inserted] = sessions_.emplace(id, std::move(handle));
        if (inserted) {
            session_count_.fetch_add(1, std::memory_order_release);
            total_accepted_.fetch_add(1, std::memory_order_relaxed);
        }
        return inserted;
    }

    auto add_session_with_id(session_handle handle, const std::string& session_id)
        -> std::string {
        if (!can_accept_connection()) {
            total_rejected_.fetch_add(1, std::memory_order_relaxed);
            return "";
        }

        std::unique_lock lock(sessions_mutex_);
        std::string id = session_id.empty() ? generate_id("session_") : session_id;

        auto [it, inserted] = sessions_.emplace(id, std::move(handle));
        if (inserted) {
            session_count_.fetch_add(1, std::memory_order_release);
            total_accepted_.fetch_add(1, std::memory_order_relaxed);
            return id;
        }
        return "";
    }

    auto remove_session(const std::string& session_id) -> bool {
        std::unique_lock lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            sessions_.erase(it);
            session_count_.fetch_sub(1, std::memory_order_release);
            return true;
        }
        return false;
    }

    [[nodiscard]] auto get_session(const std::string& session_id) -> session_handle* {
        std::shared_lock lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        return (it != sessions_.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] auto get_session(const std::string& session_id) const
        -> const session_handle* {
        std::shared_lock lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        return (it != sessions_.end()) ? &it->second : nullptr;
    }

    auto with_session(const std::string& session_id,
                      const std::function<void(session_handle&)>& callback) -> bool {
        std::shared_lock lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            callback(it->second);
            return true;
        }
        return false;
    }

    [[nodiscard]] auto has_session(const std::string& session_id) const -> bool {
        std::shared_lock lock(sessions_mutex_);
        return sessions_.find(session_id) != sessions_.end();
    }

    [[nodiscard]] auto get_all_session_ids() const -> std::vector<std::string> {
        std::shared_lock lock(sessions_mutex_);
        std::vector<std::string> ids;
        ids.reserve(sessions_.size());
        for (const auto& [id, handle] : sessions_) {
            ids.push_back(id);
        }
        return ids;
    }

    // =========================================================================
    // Iteration
    // =========================================================================

    auto for_each_mutable(const std::function<void(session_handle&)>& callback)
        -> void {
        std::shared_lock lock(sessions_mutex_);
        for (auto& [id, handle] : sessions_) {
            callback(handle);
        }
    }

    auto for_each_const(
        const std::function<void(const session_handle&)>& callback) const -> void {
        std::shared_lock lock(sessions_mutex_);
        for (const auto& [id, handle] : sessions_) {
            callback(handle);
        }
    }

    auto broadcast(std::vector<uint8_t> data) -> size_t {
        std::shared_lock lock(sessions_mutex_);
        size_t sent_count = 0;

        for (auto& [id, handle] : sessions_) {
            if (handle.is_connected()) {
                std::vector<uint8_t> copy = data;
                auto result = handle.send(std::move(copy));
                if (result.is_ok()) {
                    ++sent_count;
                }
            }
        }
        return sent_count;
    }

    // =========================================================================
    // Activity Tracking & Cleanup
    // =========================================================================

    auto update_activity(const std::string& session_id) -> bool {
        std::shared_lock lock(sessions_mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second.update_activity();
            return true;
        }
        return false;
    }

    auto cleanup_idle_sessions() -> size_t {
        std::vector<std::string> to_remove;

        // Identify idle sessions under read lock
        {
            std::shared_lock lock(sessions_mutex_);
            for (const auto& [id, handle] : sessions_) {
                if (handle.has_activity_tracking() &&
                    handle.idle_duration() > config_.idle_timeout) {
                    to_remove.push_back(id);
                }
            }
        }

        // Stop and remove idle sessions
        size_t removed = 0;
        for (const auto& id : to_remove) {
            // Stop the session before removal
            {
                std::shared_lock lock(sessions_mutex_);
                auto it = sessions_.find(id);
                if (it != sessions_.end()) {
                    it->second.stop();
                }
            }

            if (remove_session(id)) {
                ++removed;
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

    auto clear_all_sessions() -> void {
        // Stop all sessions first
        {
            std::shared_lock lock(sessions_mutex_);
            for (auto& [id, handle] : sessions_) {
                handle.stop();
            }
        }

        std::unique_lock lock(sessions_mutex_);
        sessions_.clear();
        session_count_.store(0, std::memory_order_release);
    }

    // =========================================================================
    // Metrics
    // =========================================================================

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
        if (config_.max_sessions == 0) {
            return 0.0;
        }
        return static_cast<double>(session_count_.load(std::memory_order_acquire)) /
               config_.max_sessions;
    }

    auto increment_rejected() -> void {
        total_rejected_.fetch_add(1, std::memory_order_relaxed);
    }

    // =========================================================================
    // Configuration
    // =========================================================================

    auto set_max_sessions(size_t max_sessions) -> void {
        config_.max_sessions = max_sessions;
    }

    [[nodiscard]] auto get_config() const -> const unified_session_config& {
        return config_;
    }

    [[nodiscard]] auto get_stats() const -> unified_session_manager::stats {
        return stats{
            .active_sessions = session_count_.load(std::memory_order_acquire),
            .max_sessions = config_.max_sessions,
            .total_accepted = total_accepted_.load(std::memory_order_relaxed),
            .total_rejected = total_rejected_.load(std::memory_order_relaxed),
            .total_cleaned_up = total_cleaned_up_.load(std::memory_order_relaxed),
            .utilization = get_utilization(),
            .backpressure_active = is_backpressure_active(),
            .idle_timeout = config_.idle_timeout};
    }

    // =========================================================================
    // ID Generation
    // =========================================================================

    static auto generate_id(const std::string& prefix) -> std::string {
        static std::atomic<uint64_t> counter{0};
        return prefix + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
    }

private:
    unified_session_config config_;
    mutable std::shared_mutex sessions_mutex_;
    std::unordered_map<std::string, session_handle> sessions_;

    std::atomic<size_t> session_count_;
    std::atomic<uint64_t> total_accepted_;
    std::atomic<uint64_t> total_rejected_;
    std::atomic<uint64_t> total_cleaned_up_;
};

// ============================================================================
// unified_session_manager Implementation (delegating to impl)
// ============================================================================

unified_session_manager::unified_session_manager()
    : impl_(std::make_unique<impl>(unified_session_config{})) {}

unified_session_manager::unified_session_manager(const unified_session_config& config)
    : impl_(std::make_unique<impl>(config)) {}

unified_session_manager::~unified_session_manager() = default;

auto unified_session_manager::can_accept_connection() const -> bool {
    return impl_->can_accept_connection();
}

auto unified_session_manager::is_backpressure_active() const -> bool {
    return impl_->is_backpressure_active();
}

auto unified_session_manager::add_session(session_handle handle,
                                          const std::string& session_id) -> bool {
    return impl_->add_session(std::move(handle), session_id);
}

auto unified_session_manager::remove_session(const std::string& session_id) -> bool {
    return impl_->remove_session(session_id);
}

auto unified_session_manager::get_session(const std::string& session_id)
    -> session_handle* {
    return impl_->get_session(session_id);
}

auto unified_session_manager::get_session(const std::string& session_id) const
    -> const session_handle* {
    return impl_->get_session(session_id);
}

auto unified_session_manager::with_session(
    const std::string& session_id,
    const std::function<void(session_handle&)>& callback) -> bool {
    return impl_->with_session(session_id, callback);
}

auto unified_session_manager::has_session(const std::string& session_id) const
    -> bool {
    return impl_->has_session(session_id);
}

auto unified_session_manager::get_all_session_ids() const
    -> std::vector<std::string> {
    return impl_->get_all_session_ids();
}

auto unified_session_manager::for_each(
    const std::function<void(session_handle&)>& callback) -> void {
    impl_->for_each_mutable(callback);
}

auto unified_session_manager::for_each(
    const std::function<void(const session_handle&)>& callback) const -> void {
    impl_->for_each_const(callback);
}

auto unified_session_manager::broadcast(std::vector<uint8_t> data) -> size_t {
    return impl_->broadcast(std::move(data));
}

auto unified_session_manager::update_activity(const std::string& session_id)
    -> bool {
    return impl_->update_activity(session_id);
}

auto unified_session_manager::cleanup_idle_sessions() -> size_t {
    return impl_->cleanup_idle_sessions();
}

auto unified_session_manager::clear_all_sessions() -> void {
    impl_->clear_all_sessions();
}

auto unified_session_manager::stop_all_sessions() -> void {
    impl_->clear_all_sessions();
}

auto unified_session_manager::get_session_count() const -> size_t {
    return impl_->get_session_count();
}

auto unified_session_manager::get_total_accepted() const -> uint64_t {
    return impl_->get_total_accepted();
}

auto unified_session_manager::get_total_rejected() const -> uint64_t {
    return impl_->get_total_rejected();
}

auto unified_session_manager::get_total_cleaned_up() const -> uint64_t {
    return impl_->get_total_cleaned_up();
}

auto unified_session_manager::get_utilization() const -> double {
    return impl_->get_utilization();
}

auto unified_session_manager::set_max_sessions(size_t max_sessions) -> void {
    impl_->set_max_sessions(max_sessions);
}

auto unified_session_manager::get_config() const
    -> const unified_session_config& {
    return impl_->get_config();
}

auto unified_session_manager::get_stats() const -> stats {
    return impl_->get_stats();
}

auto unified_session_manager::generate_id(const std::string& prefix)
    -> std::string {
    return impl::generate_id(prefix);
}

auto unified_session_manager::add_session_impl(session_handle handle,
                                               const std::string& session_id)
    -> bool {
    return impl_->add_session(std::move(handle), session_id);
}

auto unified_session_manager::add_session_with_id_impl(
    session_handle handle, const std::string& session_id) -> std::string {
    return impl_->add_session_with_id(std::move(handle), session_id);
}

auto unified_session_manager::increment_rejected() -> void {
    impl_->increment_rejected();
}

} // namespace kcenon::network::core
