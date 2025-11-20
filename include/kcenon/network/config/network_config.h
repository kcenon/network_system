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
 * @brief Configuration structures for network_system initialization
 *
 * @author kcenon
 * @date 2025-01-13
 */

#pragma once

#include <chrono>
#include <string>
#include <cstddef>
#include "kcenon/network/integration/logger_integration.h"

namespace network_system::config {

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
 * @brief Complete configuration for network_system
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

} // namespace network_system::config
