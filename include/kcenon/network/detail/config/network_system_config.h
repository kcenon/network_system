// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file network_system_config.h
 * @brief Configuration for network_system with external dependencies
 *
 * This header provides configuration structures for initializing the network
 * system with externally provided components. Use this configuration when
 * integrating the network system into an existing application infrastructure
 * where you want to share thread pools, loggers, and monitoring systems
 * across multiple components.
 *
 * For standalone usage where the network system should manage its own
 * resources internally, see network_config.h instead.
 *
 * @see network_config.h For standalone configuration with internal resources
 * @see network_system.h For initialization functions
 *
 * @author kcenon
 * @date 2025-01-13
 */

#pragma once

#include <memory>

#include "kcenon/network/detail/config/network_config.h"

namespace kcenon::common::interfaces {
class IExecutor;
class ILogger;
class IMonitor;
} // namespace kcenon::common::interfaces

namespace kcenon::network::config {

/**
 * @struct network_system_config
 * @brief Configuration for network_system with external dependencies
 *
 * Use this configuration when integrating the network system with existing
 * application infrastructure. This allows sharing thread pools, loggers,
 * and monitoring components with other parts of your application.
 *
 * Example usage:
 * @code
 * // Create shared infrastructure
 * auto app_executor = std::make_shared<MyExecutor>();
 * auto app_logger = std::make_shared<MyLogger>();
 *
 * // Configure network system to use shared resources
 * network_system_config cfg;
 * cfg.executor = app_executor;
 * cfg.logger = app_logger;
 * cfg.runtime = network_config::production();
 *
 * auto result = kcenon::network::initialize(cfg);
 * @endcode
 *
 * @note Components not provided (nullptr) will be created internally
 *       based on the runtime configuration settings.
 *
 * @see network_config For standalone usage with internal resources
 * @see initialize(const network_system_config&) For initialization
 */
struct network_system_config {
    /// Runtime configuration settings (thread pool, logger, monitoring config)
    network_config runtime{network_config::production()};

    /// External executor/thread pool (nullptr = create internal)
    std::shared_ptr<kcenon::common::interfaces::IExecutor> executor;

    /// External logger instance (nullptr = create internal)
    std::shared_ptr<kcenon::common::interfaces::ILogger> logger;

    /// External monitoring instance (nullptr = create internal if enabled)
    std::shared_ptr<kcenon::common::interfaces::IMonitor> monitor;
};

} // namespace kcenon::network::config
