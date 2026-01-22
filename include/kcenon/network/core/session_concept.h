/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/utils/result_types.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace kcenon::network::core {

// Import VoidResult from network namespace for convenience
using kcenon::network::VoidResult;

/**
 * @class session_concept
 * @brief Type-erased interface for session management
 *
 * This abstract base class defines the contract for all session types
 * in a type-erased manner. It enables storing heterogeneous session types
 * in the same container without template parameters.
 *
 * ## Type Erasure Pattern
 *
 * The Type Erasure pattern consists of three parts:
 * 1. **Concept** (this class): Abstract interface defining operations
 * 2. **Model**: Template that wraps concrete types and implements the concept
 * 3. **Handle**: Value-semantic wrapper exposing the interface
 *
 * ## Benefits
 *
 * - **Reduced compilation time**: No template instantiation in user code
 * - **Smaller binary size**: Single implementation instead of per-type instantiation
 * - **Simpler API**: Users work with concrete classes, not templates
 * - **Type recovery**: Original type can be retrieved via as<T>() when needed
 *
 * ## Performance Considerations
 *
 * - Virtual call overhead (~1-2ns per call) is negligible for session management
 * - Session operations (connect, disconnect) are not hot paths
 * - Per-message I/O still uses direct session pointers for performance
 *
 * @see session_model
 * @see session_handle
 */
class session_concept {
public:
    virtual ~session_concept() = default;

    // Non-copyable (managed via unique_ptr)
    session_concept(const session_concept&) = delete;
    session_concept& operator=(const session_concept&) = delete;

    // Movable
    session_concept(session_concept&&) = default;
    session_concept& operator=(session_concept&&) = default;

    // =========================================================================
    // Core Session Operations
    // =========================================================================

    /**
     * @brief Gets the session's unique identifier
     * @return Session ID as string view
     */
    [[nodiscard]] virtual auto id() const -> std::string_view = 0;

    /**
     * @brief Checks if the session is currently connected
     * @return true if connected, false otherwise
     */
    [[nodiscard]] virtual auto is_connected() const -> bool = 0;

    /**
     * @brief Sends data through the session
     * @param data Data to send (moved)
     * @return VoidResult indicating success or failure
     */
    [[nodiscard]] virtual auto send(std::vector<uint8_t>&& data)
        -> VoidResult = 0;

    /**
     * @brief Closes the session
     */
    virtual auto close() -> void = 0;

    /**
     * @brief Stops the session (alias for protocol-specific implementations)
     *
     * Some session types use stop_session() instead of close().
     * This provides a unified interface for both naming conventions.
     */
    virtual auto stop() -> void = 0;

    // =========================================================================
    // Type Information
    // =========================================================================

    /**
     * @brief Gets the runtime type information of the wrapped session
     * @return Reference to type_info of the concrete session type
     */
    [[nodiscard]] virtual auto type() const noexcept -> const std::type_info& = 0;

    /**
     * @brief Gets a raw pointer to the underlying session
     * @return void pointer to the session object
     *
     * Used internally for type recovery. Prefer using as<T>() through
     * session_handle for type-safe access.
     */
    [[nodiscard]] virtual auto get_raw() noexcept -> void* = 0;

    /**
     * @brief Gets a raw const pointer to the underlying session
     * @return const void pointer to the session object
     */
    [[nodiscard]] virtual auto get_raw() const noexcept -> const void* = 0;

    // =========================================================================
    // Activity Tracking (optional, based on traits)
    // =========================================================================

    /**
     * @brief Checks if this session type supports activity tracking
     * @return true if activity tracking is enabled
     */
    [[nodiscard]] virtual auto has_activity_tracking() const noexcept -> bool = 0;

    /**
     * @brief Gets the creation timestamp
     * @return Time point when the session was created
     *
     * Only valid if has_activity_tracking() returns true.
     */
    [[nodiscard]] virtual auto created_at() const
        -> std::chrono::steady_clock::time_point = 0;

    /**
     * @brief Gets the last activity timestamp
     * @return Time point of last activity
     *
     * Only valid if has_activity_tracking() returns true.
     */
    [[nodiscard]] virtual auto last_activity() const
        -> std::chrono::steady_clock::time_point = 0;

    /**
     * @brief Updates the last activity timestamp to current time
     *
     * Only has effect if has_activity_tracking() returns true.
     */
    virtual auto update_activity() -> void = 0;

    /**
     * @brief Calculates idle duration since last activity
     * @return Duration since last activity, or zero if tracking disabled
     */
    [[nodiscard]] virtual auto idle_duration() const -> std::chrono::milliseconds = 0;

protected:
    session_concept() = default;
};

} // namespace kcenon::network::core
