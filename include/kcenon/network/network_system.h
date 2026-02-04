// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

/**
 * @file network_system.h
 * @brief Main header for the Network System
 *
 * This header provides access to all core Network System functionality
 * including messaging clients, servers, and session management.
 *
 * @author kcenon
 * @date 2025-09-19
 */

// Core networking components
#include "internal/core/messaging_client.h"
#include "internal/core/messaging_server.h"

// Unified templates with protocol/TLS policy parameters
#include "internal/core/unified_messaging_client.h"
#include "internal/core/unified_messaging_server.h"

// Protocol tags and TLS policies for unified templates
#include "kcenon/network/protocol/protocol.h"
#include "kcenon/network/policy/tls_policy.h"

// Session management
#include "kcenon/network/session/session.h"

// Integration interfaces
#include "internal/integration/messaging_bridge.h"
#include "kcenon/network/integration/thread_integration.h"
#include "internal/integration/container_integration.h"

// Configuration system
#include "kcenon/network/config/config.h"
#include "internal/core/network_context.h"

// Result type for error handling
#include "kcenon/network/types/result.h"

// C++20 Concepts for compile-time type validation
#include "kcenon/network/concepts/concepts.h"

/**
 * @namespace kcenon::network
 * @brief Main namespace for all Network System components
 */
namespace kcenon::network {

/**
 * @brief Initialize the network system with default production configuration
 *
 * This is the simplest way to initialize the network system. It uses
 * production-optimized defaults with internally managed resources.
 * Equivalent to calling initialize(network_config::production()).
 *
 * @return VoidResult - ok() on success, error on failure
 *
 * Possible errors:
 * - already_exists: Network system already initialized
 * - internal_error: System initialization failed
 *
 * @see initialize(const config::network_config&) For custom settings
 * @see initialize(const config::network_system_config&) For external dependencies
 */
VoidResult initialize();

/**
 * @brief Initialize the network system with custom settings
 *
 * Use this overload to customize thread pool, logger, and monitoring settings
 * while letting the network system manage these resources internally.
 *
 * Example:
 * @code
 * // Use predefined configuration
 * auto result = initialize(network_config::development());
 *
 * // Or customize specific settings
 * network_config cfg;
 * cfg.thread_pool.worker_count = 8;
 * cfg.logger.min_level = log_level::debug;
 * auto result = initialize(cfg);
 * @endcode
 *
 * @param config Configuration settings for internal resource creation
 * @return VoidResult - ok() on success, error on failure
 *
 * Possible errors:
 * - already_exists: Network system already initialized
 * - invalid_argument: Invalid configuration values
 * - internal_error: System initialization failed
 *
 * @see network_config For available settings and factory methods
 * @see initialize(const config::network_system_config&) For external dependencies
 */
VoidResult initialize(const config::network_config& config);

/**
 * @brief Initialize the network system with external dependencies
 *
 * Use this overload when integrating the network system with existing
 * application infrastructure. This allows sharing thread pools, loggers,
 * and monitoring systems with other components in your application.
 *
 * Example:
 * @code
 * // Share existing infrastructure
 * network_system_config cfg;
 * cfg.executor = my_app_thread_pool;
 * cfg.logger = my_app_logger;
 * cfg.runtime = network_config::production();
 * auto result = initialize(cfg);
 * @endcode
 *
 * @param config Configuration with external dependencies and runtime settings
 * @return VoidResult - ok() on success, error on failure
 *
 * Possible errors:
 * - already_exists: Network system already initialized
 * - invalid_argument: Invalid configuration or dependencies
 * - internal_error: System initialization failed
 *
 * @note Unset dependencies (nullptr) will be created internally using
 *       the runtime configuration settings.
 *
 * @see network_system_config For dependency injection options
 * @see initialize(const config::network_config&) For standalone usage
 */
VoidResult initialize(const config::network_system_config& config);

/**
 * @brief Shutdown the network system
 * @return VoidResult - ok() on success, error on failure
 *
 * Possible errors:
 * - not_initialized: Network system not initialized
 * - internal_error: Shutdown failed
 */
VoidResult shutdown();

/**
 * @brief Check if network system is initialized
 * @return true if initialized, false otherwise
 */
bool is_initialized();

} // namespace kcenon::network
