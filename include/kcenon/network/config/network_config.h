/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/**
 * @file network_config.h
 * @brief Configuration structures for standalone network_system initialization
 *
 * This header provides configuration structures for initializing the network
 * system with internally managed resources. Use these configurations when you
 * want the network system to create and manage its own thread pool, logger,
 * and monitoring components.
 *
 * For integration with existing infrastructure where you want to share
 * resources (thread pools, loggers, etc.) with other components, see
 * network_system_config.h instead.
 *
 * @see network_system_config.h For integration with external dependencies
 * @see network_system.h For initialization functions
 *
 * @author kcenon
 * @date 2025-01-13
 */

#pragma once

#include <chrono>
#include <string>
#include <cstddef>
#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::config {

/**
 * @struct thread_pool_config
 * @brief Configuration for thread pool
 */
struct thread_pool_config {
    /// Number of worker threads (0 = auto-detect via hardware_concurrency)
    size_t worker_count = 0;

    /// Maximum queue capacity
    size_t queue_capacity = 10000;

    /// Thread pool name
    std::string pool_name = "network_pool";
};

/**
 * @struct logger_config
 * @brief Configuration for logging system
 */
struct logger_config {
    /// Minimum log level to record
    integration::log_level min_level = integration::log_level::info;

    /// Enable asynchronous logging
    bool async_logging = true;

    /// Buffer size for async logging
    size_t buffer_size = 8192;

    /// Log file path (empty = console only)
    std::string log_file_path;
};

/**
 * @struct monitoring_config
 * @brief Configuration for monitoring system
 */
struct monitoring_config {
    /// Enable monitoring
    bool enabled = true;

    /// Metrics collection interval
    std::chrono::seconds metrics_interval{5};

    /// Service name for monitoring
    std::string service_name = "network_system";
};

/**
 * @struct network_config
 * @brief Configuration for standalone network_system initialization
 *
 * Use this configuration when you want the network system to manage its own
 * internal resources (thread pool, logger, monitoring). The network system
 * will create these components based on the provided settings.
 *
 * Example usage:
 * @code
 * // Use predefined configurations
 * auto result = kcenon::network::initialize(network_config::production());
 *
 * // Or customize settings
 * network_config cfg;
 * cfg.thread_pool.worker_count = 8;
 * cfg.logger.min_level = integration::log_level::debug;
 * auto result = kcenon::network::initialize(cfg);
 * @endcode
 *
 * @note For sharing existing thread pools, loggers, or other infrastructure
 *       with the network system, use network_system_config instead.
 *
 * @see network_system_config For integration with external dependencies
 * @see initialize() For initialization without configuration
 */
struct network_config {
    /// Thread pool configuration
    thread_pool_config thread_pool;

    /// Logger configuration
    logger_config logger;

    /// Monitoring configuration
    monitoring_config monitoring;

    /**
     * @brief Create development configuration
     * @return Configuration optimized for development
     */
    static network_config development() {
        network_config cfg;
        cfg.logger.min_level = integration::log_level::debug;
        cfg.logger.async_logging = false; // Synchronous for debugging
        cfg.monitoring.enabled = true;
        cfg.thread_pool.worker_count = 2; // Minimal for dev
        return cfg;
    }

    /**
     * @brief Create production configuration
     * @return Configuration optimized for production
     */
    static network_config production() {
        network_config cfg;
        cfg.logger.min_level = integration::log_level::info;
        cfg.logger.async_logging = true;
        cfg.monitoring.enabled = true;
        cfg.thread_pool.worker_count = 0; // Auto-detect
        return cfg;
    }

    /**
     * @brief Create testing configuration
     * @return Configuration optimized for testing
     */
    static network_config testing() {
        network_config cfg;
        cfg.logger.min_level = integration::log_level::warn;
        cfg.logger.async_logging = false; // Synchronous for test reliability
        cfg.monitoring.enabled = false;
        cfg.thread_pool.worker_count = 1; // Single thread for determinism
        return cfg;
    }
};

} // namespace kcenon::network::config
