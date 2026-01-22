/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/core/session_concept.h"
#include "kcenon/network/core/session_model.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace kcenon::network::core {

// Import VoidResult from network namespace for convenience
using kcenon::network::VoidResult;

/**
 * @class session_handle
 * @brief Value-semantic wrapper for type-erased sessions
 *
 * This class provides a convenient, value-semantic interface to work with
 * type-erased sessions. It wraps a session_concept and provides:
 *
 * - Clean API for session operations
 * - Type-safe recovery of the original session type via as<T>()
 * - RAII semantics for session lifetime management
 *
 * ## Usage Example
 *
 * ```cpp
 * // Create a handle from any session type
 * auto tcp_session = std::make_shared<messaging_session>(...);
 * session_handle handle(tcp_session);
 *
 * // Use the type-erased interface
 * if (handle.is_connected()) {
 *     handle.send(data);
 * }
 *
 * // Recover the original type when needed
 * if (auto* tcp = handle.as<messaging_session>()) {
 *     tcp->set_receive_callback(...);  // Protocol-specific operation
 * }
 * ```
 *
 * ## Thread Safety
 *
 * - session_handle itself is NOT thread-safe
 * - Operations on the underlying session ARE thread-safe (as per session implementation)
 * - Share session_handle via shared_ptr for concurrent access
 *
 * @see session_concept
 * @see session_model
 */
class session_handle {
public:
    /**
     * @brief Creates an empty (invalid) handle
     */
    session_handle() = default;

    /**
     * @brief Creates a handle from a type-erased session concept
     * @param session_concept_ptr Unique pointer to the session concept
     */
    explicit session_handle(std::unique_ptr<session_concept> session_concept_ptr)
        : concept_(std::move(session_concept_ptr)) {}

    /**
     * @brief Creates a handle from a concrete session type
     * @tparam SessionType The session type
     * @param session Shared pointer to the session
     */
    template <typename SessionType>
    explicit session_handle(std::shared_ptr<SessionType> session)
        : concept_(make_session_model(std::move(session))) {}

    // Move-only semantics
    session_handle(session_handle&&) = default;
    session_handle& operator=(session_handle&&) = default;

    session_handle(const session_handle&) = delete;
    session_handle& operator=(const session_handle&) = delete;

    ~session_handle() = default;

    // =========================================================================
    // Validity Check
    // =========================================================================

    /**
     * @brief Checks if the handle contains a valid session
     * @return true if the handle is valid
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return concept_ != nullptr;
    }

    /**
     * @brief Checks if the handle is valid
     * @return true if the handle contains a session
     */
    [[nodiscard]] auto valid() const noexcept -> bool {
        return concept_ != nullptr;
    }

    // =========================================================================
    // Core Session Operations
    // =========================================================================

    /**
     * @brief Gets the session's unique identifier
     * @return Session ID, or empty string if invalid
     */
    [[nodiscard]] auto id() const -> std::string_view {
        return concept_ ? concept_->id() : std::string_view{};
    }

    /**
     * @brief Checks if the session is currently connected
     * @return true if connected, false if disconnected or invalid
     */
    [[nodiscard]] auto is_connected() const -> bool {
        return concept_ && concept_->is_connected();
    }

    /**
     * @brief Sends data through the session
     * @param data Data to send (moved)
     * @return VoidResult indicating success or failure
     */
    [[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult {
        if (!concept_) {
            return VoidResult::err(-1, "Invalid session handle");
        }
        return concept_->send(std::move(data));
    }

    /**
     * @brief Closes the session
     */
    auto close() -> void {
        if (concept_) {
            concept_->close();
        }
    }

    /**
     * @brief Stops the session
     */
    auto stop() -> void {
        if (concept_) {
            concept_->stop();
        }
    }

    // =========================================================================
    // Type Recovery
    // =========================================================================

    /**
     * @brief Attempts to cast to a specific session type
     * @tparam T The target session type
     * @return Pointer to the session if type matches, nullptr otherwise
     *
     * This method provides type-safe access to the underlying session
     * when protocol-specific operations are needed.
     *
     * ```cpp
     * if (auto* tcp = handle.as<messaging_session>()) {
     *     tcp->set_receive_callback(...);
     * }
     * ```
     */
    template <typename T>
    [[nodiscard]] auto as() noexcept -> T* {
        if (!concept_ || concept_->type() != typeid(T)) {
            return nullptr;
        }
        return static_cast<T*>(concept_->get_raw());
    }

    /**
     * @brief Attempts to cast to a specific session type (const version)
     * @tparam T The target session type
     * @return Const pointer to the session if type matches, nullptr otherwise
     */
    template <typename T>
    [[nodiscard]] auto as() const noexcept -> const T* {
        if (!concept_ || concept_->type() != typeid(T)) {
            return nullptr;
        }
        return static_cast<const T*>(concept_->get_raw());
    }

    /**
     * @brief Checks if the session is of a specific type
     * @tparam T The type to check
     * @return true if the session is of type T
     */
    template <typename T>
    [[nodiscard]] auto is_type() const noexcept -> bool {
        return concept_ && concept_->type() == typeid(T);
    }

    /**
     * @brief Gets type information of the wrapped session
     * @return Reference to type_info, or typeid(void) if invalid
     */
    [[nodiscard]] auto type() const noexcept -> const std::type_info& {
        return concept_ ? concept_->type() : typeid(void);
    }

    // =========================================================================
    // Activity Tracking
    // =========================================================================

    /**
     * @brief Checks if activity tracking is enabled for this session
     * @return true if activity tracking is available
     */
    [[nodiscard]] auto has_activity_tracking() const noexcept -> bool {
        return concept_ && concept_->has_activity_tracking();
    }

    /**
     * @brief Gets the creation timestamp
     * @return Creation time point, or epoch if invalid
     */
    [[nodiscard]] auto created_at() const -> std::chrono::steady_clock::time_point {
        return concept_ ? concept_->created_at()
                        : std::chrono::steady_clock::time_point{};
    }

    /**
     * @brief Gets the last activity timestamp
     * @return Last activity time point, or epoch if invalid
     */
    [[nodiscard]] auto last_activity() const -> std::chrono::steady_clock::time_point {
        return concept_ ? concept_->last_activity()
                        : std::chrono::steady_clock::time_point{};
    }

    /**
     * @brief Updates the last activity timestamp
     */
    auto update_activity() -> void {
        if (concept_) {
            concept_->update_activity();
        }
    }

    /**
     * @brief Gets the idle duration since last activity
     * @return Idle duration, or zero if invalid
     */
    [[nodiscard]] auto idle_duration() const -> std::chrono::milliseconds {
        return concept_ ? concept_->idle_duration() : std::chrono::milliseconds{0};
    }

    // =========================================================================
    // Access to Underlying Concept
    // =========================================================================

    /**
     * @brief Gets the underlying session concept (non-owning)
     * @return Pointer to the concept, or nullptr if invalid
     */
    [[nodiscard]] auto get_concept() noexcept -> session_concept* {
        return concept_.get();
    }

    /**
     * @brief Gets the underlying session concept (non-owning, const)
     * @return Const pointer to the concept, or nullptr if invalid
     */
    [[nodiscard]] auto get_concept() const noexcept -> const session_concept* {
        return concept_.get();
    }

    /**
     * @brief Releases ownership of the session concept
     * @return Unique pointer to the concept
     */
    [[nodiscard]] auto release() noexcept -> std::unique_ptr<session_concept> {
        return std::move(concept_);
    }

    /**
     * @brief Resets the handle to empty state
     */
    auto reset() noexcept -> void {
        concept_.reset();
    }

private:
    std::unique_ptr<session_concept> concept_;
};

/**
 * @brief Factory function to create a session_handle
 * @tparam SessionType The concrete session type
 * @param session Shared pointer to the session
 * @return session_handle wrapping the session
 */
template <typename SessionType>
[[nodiscard]] auto make_session_handle(std::shared_ptr<SessionType> session)
    -> session_handle {
    return session_handle(std::move(session));
}

} // namespace kcenon::network::core
