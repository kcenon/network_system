// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file service_registration.h
 * @brief Service container registration for network_system services.
 *
 * This header provides functions to register network_system services
 * with the unified service container from common_system.
 *
 * @see TICKET-103 for integration requirements.
 */

#pragma once

#include <kcenon/common/config/feature_flags.h>

#include <memory>

#if KCENON_WITH_COMMON_SYSTEM

#include <kcenon/common/di/service_container.h>

#include "../core/network_context.h"

namespace kcenon::network::di {

/**
 * @brief Configuration for network context service registration
 */
struct network_registration_config {
    /// Number of worker threads for network operations (0 = auto-detect)
    size_t thread_count = 0;

    /// Whether to initialize the context immediately upon registration
    bool initialize_on_register = true;

    /// Service lifetime (singleton for network context)
    common::di::service_lifetime lifetime = common::di::service_lifetime::singleton;
};

/**
 * @brief Register network context services with the service container.
 *
 * Registers network_context for access via DI container.
 * By default, registers as a singleton and initializes immediately.
 *
 * @param container The service container to register with
 * @param config Optional configuration for the network context
 * @return VoidResult indicating success or registration error
 *
 * @code
 * auto& container = common::di::service_container::global();
 *
 * // Register with default configuration
 * auto result = register_network_services(container);
 *
 * // Or with custom configuration
 * network_registration_config config;
 * config.thread_count = 8;
 * config.initialize_on_register = true;
 * auto result = register_network_services(container, config);
 *
 * // Then resolve network context anywhere in the application
 * auto ctx = container.resolve<kcenon::network::core::network_context>().value();
 * ctx->get_thread_pool()->submit([]{ /* work */ });
 * @endcode
 */
inline common::VoidResult register_network_services(
    common::di::IServiceContainer& container,
    const network_registration_config& config = {}) {

    // Check if already registered
    if (container.is_registered<kcenon::network::core::network_context>()) {
        return common::make_error<std::monostate>(
            common::di::di_error_codes::already_registered,
            "network_context is already registered",
            "kcenon::network::di"
        );
    }

    // Register network_context factory
    // Note: network_context is a singleton, so we return a reference wrapper
    return container.register_factory<kcenon::network::core::network_context>(
        [config](common::di::IServiceContainer&) -> std::shared_ptr<kcenon::network::core::network_context> {
            // Get the singleton instance
            auto& ctx = kcenon::network::core::network_context::instance();

            // Initialize if requested and not already initialized
            if (config.initialize_on_register && !ctx.is_initialized()) {
                ctx.initialize(config.thread_count);
            }

            // Return a non-owning shared_ptr (aliasing constructor)
            // This allows the context to be managed via DI without changing ownership
            return std::shared_ptr<kcenon::network::core::network_context>(
                std::shared_ptr<void>{}, &ctx);
        },
        config.lifetime
    );
}

/**
 * @brief Unregister network services from the container.
 *
 * Note: This does not shutdown the network_context singleton.
 * Call network_context::instance().shutdown() separately if needed.
 *
 * @param container The service container to unregister from
 * @return VoidResult indicating success or error
 */
inline common::VoidResult unregister_network_services(
    common::di::IServiceContainer& container) {

    return container.unregister<kcenon::network::core::network_context>();
}

/**
 * @brief Shutdown and unregister network services.
 *
 * This function shuts down the network context and unregisters it
 * from the container.
 *
 * @param container The service container to unregister from
 * @return VoidResult indicating success or error
 */
inline common::VoidResult shutdown_network_services(
    common::di::IServiceContainer& container) {

    // Shutdown the singleton
    kcenon::network::core::network_context::instance().shutdown();

    // Unregister from container
    return unregister_network_services(container);
}

/**
 * @brief Get network_context directly from the singleton.
 *
 * Convenience function for accessing the network_context when
 * the DI container is not available.
 *
 * @return Reference to the network_context singleton
 */
inline kcenon::network::core::network_context& get_network_context() {
    return kcenon::network::core::network_context::instance();
}

/**
 * @brief Register all network_system services with the container.
 *
 * Convenience function to register all available network_system services.
 *
 * @param container The service container to register with
 * @param config Optional configuration for network context
 * @return VoidResult indicating success or registration error
 */
inline common::VoidResult register_all_network_services(
    common::di::IServiceContainer& container,
    const network_registration_config& config = {}) {

    // Register network_context
    auto result = register_network_services(container, config);
    if (result.is_err()) {
        return result;
    }

    return common::VoidResult::ok({});
}

} // namespace kcenon::network::di

#endif // KCENON_WITH_COMMON_SYSTEM
