/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

#include "internal/core/session_concept.h"
#include "internal/core/session_traits.h"

#include <memory>
#include <type_traits>

namespace kcenon::network::core {

// Import VoidResult from network namespace for convenience
using kcenon::network::VoidResult;

/**
 * @class session_model
 * @brief Template that wraps concrete session types to implement session_concept
 *
 * This class is the "Model" part of the Type Erasure pattern. It wraps
 * any session type that satisfies the required interface and implements
 * the type-erased session_concept.
 *
 * ## How It Works
 *
 * 1. Takes a shared_ptr to any session type
 * 2. Stores it internally
 * 3. Implements session_concept by delegating to the wrapped session
 * 4. Uses session_traits<T> to customize behavior per session type
 *
 * ## Activity Tracking
 *
 * Activity tracking is conditionally enabled based on session_traits<T>:
 * - If `session_traits<T>::has_activity_tracking` is true, timestamps are tracked
 * - Otherwise, activity methods return sensible defaults
 *
 * ## Type Recovery
 *
 * The original session type can be recovered via:
 * ```cpp
 * if (concept->type() == typeid(MySession)) {
 *     auto* ptr = static_cast<MySession*>(concept->get_raw());
 * }
 * ```
 *
 * @tparam SessionType The concrete session type to wrap
 *
 * @see session_concept
 * @see session_traits
 */
template <typename SessionType>
class session_model final : public session_concept {
public:
    using session_ptr = std::shared_ptr<SessionType>;
    using traits = session_traits<SessionType>;

    /**
     * @brief Constructs a session model wrapping the given session
     * @param session Shared pointer to the session to wrap
     */
    explicit session_model(session_ptr session)
        : session_(std::move(session))
        , created_at_(std::chrono::steady_clock::now())
        , last_activity_(created_at_) {}

    ~session_model() override = default;

    // =========================================================================
    // Core Session Operations (delegated to wrapped session)
    // =========================================================================

    [[nodiscard]] auto id() const -> std::string_view override {
        if constexpr (has_id_method<SessionType>::value) {
            return session_->id();
        } else if constexpr (has_server_id_method<SessionType>::value) {
            return session_->server_id();
        } else {
            return "";
        }
    }

    [[nodiscard]] auto is_connected() const -> bool override {
        if constexpr (has_is_connected_method<SessionType>::value) {
            return session_->is_connected();
        } else if constexpr (has_is_stopped_method<SessionType>::value) {
            return !session_->is_stopped();
        } else {
            return true; // Assume connected if no method available
        }
    }

    [[nodiscard]] auto send(std::vector<uint8_t>&& data)
        -> VoidResult override {
        if constexpr (has_send_method<SessionType>::value) {
            return session_->send(std::move(data));
        } else if constexpr (has_send_packet_method<SessionType>::value) {
            session_->send_packet(std::move(data));
            return VoidResult::ok(std::monostate{});
        } else {
            return VoidResult::err(-1, "Session does not support send");
        }
    }

    auto close() -> void override {
        if constexpr (has_close_method<SessionType>::value) {
            session_->close();
        } else if constexpr (has_stop_session_method<SessionType>::value) {
            session_->stop_session();
        }
    }

    auto stop() -> void override {
        if constexpr (has_stop_session_method<SessionType>::value) {
            session_->stop_session();
        } else if constexpr (has_close_method<SessionType>::value) {
            session_->close();
        }
    }

    // =========================================================================
    // Type Information
    // =========================================================================

    [[nodiscard]] auto type() const noexcept -> const std::type_info& override {
        return typeid(SessionType);
    }

    [[nodiscard]] auto get_raw() noexcept -> void* override {
        return session_.get();
    }

    [[nodiscard]] auto get_raw() const noexcept -> const void* override {
        return session_.get();
    }

    // =========================================================================
    // Activity Tracking
    // =========================================================================

    [[nodiscard]] auto has_activity_tracking() const noexcept -> bool override {
        return traits::has_activity_tracking;
    }

    [[nodiscard]] auto created_at() const
        -> std::chrono::steady_clock::time_point override {
        return created_at_;
    }

    [[nodiscard]] auto last_activity() const
        -> std::chrono::steady_clock::time_point override {
        return last_activity_;
    }

    auto update_activity() -> void override {
        if constexpr (traits::has_activity_tracking) {
            last_activity_ = std::chrono::steady_clock::now();
        }
    }

    [[nodiscard]] auto idle_duration() const -> std::chrono::milliseconds override {
        if constexpr (traits::has_activity_tracking) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - last_activity_);
        } else {
            return std::chrono::milliseconds{0};
        }
    }

    // =========================================================================
    // Direct Access to Wrapped Session
    // =========================================================================

    /**
     * @brief Gets the underlying session (non-owning)
     * @return Raw pointer to the session
     */
    [[nodiscard]] auto get_session() noexcept -> SessionType* {
        return session_.get();
    }

    /**
     * @brief Gets the underlying session (non-owning, const)
     * @return Raw const pointer to the session
     */
    [[nodiscard]] auto get_session() const noexcept -> const SessionType* {
        return session_.get();
    }

    /**
     * @brief Gets the underlying session shared_ptr
     * @return Shared pointer to the session
     */
    [[nodiscard]] auto get_session_ptr() const noexcept -> session_ptr {
        return session_;
    }

private:
    session_ptr session_;
    std::chrono::steady_clock::time_point created_at_;
    std::chrono::steady_clock::time_point last_activity_;

    // =========================================================================
    // SFINAE Helpers for Method Detection
    // =========================================================================

    // id() method detection
    template <typename T, typename = void>
    struct has_id_method : std::false_type {};

    template <typename T>
    struct has_id_method<T, std::void_t<decltype(std::declval<const T&>().id())>>
        : std::true_type {};

    // server_id() method detection
    template <typename T, typename = void>
    struct has_server_id_method : std::false_type {};

    template <typename T>
    struct has_server_id_method<T,
                                 std::void_t<decltype(std::declval<const T&>().server_id())>>
        : std::true_type {};

    // is_connected() method detection
    template <typename T, typename = void>
    struct has_is_connected_method : std::false_type {};

    template <typename T>
    struct has_is_connected_method<
        T, std::void_t<decltype(std::declval<const T&>().is_connected())>>
        : std::true_type {};

    // is_stopped() method detection
    template <typename T, typename = void>
    struct has_is_stopped_method : std::false_type {};

    template <typename T>
    struct has_is_stopped_method<
        T, std::void_t<decltype(std::declval<const T&>().is_stopped())>>
        : std::true_type {};

    // send() method detection
    template <typename T, typename = void>
    struct has_send_method : std::false_type {};

    template <typename T>
    struct has_send_method<
        T, std::void_t<decltype(std::declval<T&>().send(std::declval<std::vector<uint8_t>&&>()))>>
        : std::true_type {};

    // send_packet() method detection
    template <typename T, typename = void>
    struct has_send_packet_method : std::false_type {};

    template <typename T>
    struct has_send_packet_method<
        T,
        std::void_t<decltype(std::declval<T&>().send_packet(
            std::declval<std::vector<uint8_t>&&>()))>>
        : std::true_type {};

    // close() method detection
    template <typename T, typename = void>
    struct has_close_method : std::false_type {};

    template <typename T>
    struct has_close_method<T, std::void_t<decltype(std::declval<T&>().close())>>
        : std::true_type {};

    // stop_session() method detection
    template <typename T, typename = void>
    struct has_stop_session_method : std::false_type {};

    template <typename T>
    struct has_stop_session_method<T,
                                    std::void_t<decltype(std::declval<T&>().stop_session())>>
        : std::true_type {};
};

/**
 * @brief Factory function to create a session_model
 * @tparam SessionType The concrete session type
 * @param session Shared pointer to the session
 * @return Unique pointer to the type-erased session_concept
 */
template <typename SessionType>
[[nodiscard]] auto make_session_model(std::shared_ptr<SessionType> session)
    -> std::unique_ptr<session_concept> {
    // Explicitly construct unique_ptr<session_concept> from raw pointer
    // This ensures proper polymorphic conversion
    return std::unique_ptr<session_concept>(
        new session_model<SessionType>(std::move(session)));
}

} // namespace kcenon::network::core
