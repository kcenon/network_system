/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/core/session_traits.h"

#include <chrono>
#include <memory>

namespace kcenon::network::core {

/**
 * @struct session_info_base
 * @brief Wrapper for session with optional activity tracking
 *
 * Uses template specialization based on traits to avoid storage overhead
 * when activity tracking is disabled.
 *
 * @tparam SessionType The session type being wrapped
 * @tparam HasActivityTracking Whether to include activity tracking fields
 */
template <typename SessionType, bool HasActivityTracking>
struct session_info_base;

/**
 * @brief Specialization with activity tracking enabled
 *
 * Includes timestamps for creation and last activity for idle detection.
 */
template <typename SessionType>
struct session_info_base<SessionType, true> {
    std::shared_ptr<SessionType> session;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;

    explicit session_info_base(std::shared_ptr<SessionType> sess)
        : session(std::move(sess))
        , created_at(std::chrono::steady_clock::now())
        , last_activity(created_at) {}

    /**
     * @brief Updates the last activity timestamp to current time
     */
    void update_activity() { last_activity = std::chrono::steady_clock::now(); }

    /**
     * @brief Calculates the idle duration since last activity
     * @return Duration since last activity in milliseconds
     */
    [[nodiscard]] auto idle_duration() const -> std::chrono::milliseconds {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_activity);
    }
};

/**
 * @brief Specialization without activity tracking (minimal overhead)
 *
 * Only stores the session pointer, no timestamp fields.
 */
template <typename SessionType>
struct session_info_base<SessionType, false> {
    std::shared_ptr<SessionType> session;

    explicit session_info_base(std::shared_ptr<SessionType> sess)
        : session(std::move(sess)) {}
};

/**
 * @brief Convenience alias that selects the appropriate session_info_base
 *        based on session_traits
 *
 * @tparam SessionType The session type to wrap
 */
template <typename SessionType>
using session_info_t =
    session_info_base<SessionType,
                      session_traits<SessionType>::has_activity_tracking>;

} // namespace kcenon::network::core
