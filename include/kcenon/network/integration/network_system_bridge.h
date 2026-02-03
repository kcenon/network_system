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
 * @file network_system_bridge.h
 * @brief Unified facade for all network_system integration bridges
 *
 * This file provides the NetworkSystemBridge class that consolidates all
 * external system integrations (thread pools, logging, monitoring, messaging)
 * into a single, unified facade.
 *
 * Design Goals:
 * - Single entry point for all integration bridges
 * - Simplified configuration with sensible defaults
 * - Factory methods for common integration scenarios
 * - Lifecycle management for all bridges
 * - Thread-safe access to bridge components
 *
 * Usage Example:
 * @code
 * // Create bridge with default configuration
 * auto bridge = NetworkSystemBridge::create_default();
 * bridge->initialize();
 *
 * // Access specific bridges
 * auto thread_pool = bridge->get_thread_pool_bridge();
 * auto logger = bridge->get_logger_bridge();
 *
 * // Use integration interfaces
 * if (auto pool = bridge->get_thread_pool()) {
 *     pool->submit([]{ // task implementation });
 * }
 * @endcode
 *
 * Related Issues:
 * - #579: Consolidate integration adapters into NetworkSystemBridge
 * - #577: EPIC for Facade pattern refactoring
 *
 * @author network_system team
 * @date 2026-02-01
 */

#include "internal/integration/bridge_interface.h"
#include "internal/integration/thread_pool_bridge.h"
#include "kcenon/network/integration/thread_integration.h"
#include "internal/integration/logger_integration.h"
#include "internal/integration/monitoring_integration.h"
#include <kcenon/network/config/feature_flags.h>

#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/executor_interface.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#endif

#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <optional>

namespace kcenon::network::integration {

/**
 * @struct NetworkSystemBridgeConfig
 * @brief Configuration for NetworkSystemBridge
 *
 * This structure provides configuration options for all integrated bridges.
 * Each bridge has its own configuration section.
 */
struct NetworkSystemBridgeConfig {
    /**
     * @brief Global integration name
     */
    std::string integration_name = "network_system";

    /**
     * @brief Enable thread pool integration
     */
    bool enable_thread_pool = true;

    /**
     * @brief Enable logger integration
     */
    bool enable_logger = false;

    /**
     * @brief Enable monitoring integration
     */
    bool enable_monitoring = false;

    /**
     * @brief Thread pool configuration properties
     *
     * Common properties:
     * - "pool_name": Thread pool identifier (default: "network_pool")
     * - "worker_count": Number of worker threads (informational)
     */
    std::map<std::string, std::string> thread_pool_properties;

    /**
     * @brief Logger configuration properties
     *
     * Common properties:
     * - "log_level": Minimum log level ("trace", "debug", "info", "warn", "error", "fatal")
     * - "output_file": Log file path (optional)
     */
    std::map<std::string, std::string> logger_properties;

    /**
     * @brief Monitoring configuration properties
     *
     * Common properties:
     * - "enable_metrics": Enable metrics collection ("true" or "false")
     * - "metrics_interval_ms": Metrics collection interval in milliseconds
     */
    std::map<std::string, std::string> monitoring_properties;
};

/**
 * @class NetworkSystemBridge
 * @brief Unified facade for all network_system integration bridges
 *
 * This class provides a single entry point for managing all external system
 * integrations in network_system. It consolidates thread pool, logger, and
 * monitoring integrations into a unified interface.
 *
 * Key Features:
 * - Single initialization point for all integrations
 * - Factory methods for common scenarios
 * - Thread-safe access to bridge components
 * - Centralized lifecycle management
 * - Metrics aggregation from all bridges
 *
 * Lifecycle:
 * 1. Create using factory method or direct constructor
 * 2. Call initialize() with configuration
 * 3. Access bridge components via getters
 * 4. Call shutdown() before destruction
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - initialize() and shutdown() can be called from multiple threads
 * - Getters are thread-safe after initialization
 */
class NetworkSystemBridge {
public:
    /**
     * @brief Default constructor
     *
     * Creates a bridge with default settings. All integrations are disabled
     * until explicitly configured and initialized.
     */
    NetworkSystemBridge();

    /**
     * @brief Construct bridge with custom thread pool
     * @param thread_pool Thread pool bridge to use
     *
     * Creates a bridge with a specific thread pool bridge. Other integrations
     * can be added via set_* methods.
     */
    explicit NetworkSystemBridge(std::shared_ptr<ThreadPoolBridge> thread_pool);

    /**
     * @brief Destructor
     *
     * Automatically calls shutdown() if initialized
     */
    ~NetworkSystemBridge();

    // Non-copyable, movable
    NetworkSystemBridge(const NetworkSystemBridge&) = delete;
    NetworkSystemBridge& operator=(const NetworkSystemBridge&) = delete;
    NetworkSystemBridge(NetworkSystemBridge&&) noexcept;
    NetworkSystemBridge& operator=(NetworkSystemBridge&&) noexcept;

    /**
     * @brief Initialize all configured bridges
     * @param config Configuration for all bridges
     * @return ok() on success, error_info on failure
     *
     * Initializes all enabled bridges according to the configuration.
     * If any bridge fails to initialize, all previously initialized bridges
     * are shut down and an error is returned.
     *
     * Error Conditions:
     * - Already initialized
     * - Bridge initialization failure
     * - Invalid configuration
     *
     * Example:
     * @code
     * NetworkSystemBridgeConfig config;
     * config.enable_thread_pool = true;
     * config.thread_pool_properties["pool_name"] = "network_pool";
     *
     * auto result = bridge->initialize(config);
     * if (result.is_err()) {
     *     std::cerr << "Init failed: " << result.error().message << std::endl;
     * }
     * @endcode
     */
    VoidResult initialize(const NetworkSystemBridgeConfig& config);

    /**
     * @brief Shutdown all bridges
     * @return ok() on success, error_info on failure
     *
     * Shuts down all initialized bridges in reverse initialization order.
     * This method is idempotent - multiple calls are safe.
     */
    VoidResult shutdown();

    /**
     * @brief Check if the bridge is initialized
     * @return true if initialized and ready, false otherwise
     */
    bool is_initialized() const;

    /**
     * @brief Get aggregated metrics from all bridges
     * @return Aggregated bridge metrics
     *
     * Returns combined metrics from all initialized bridges.
     * Custom metrics from each bridge are prefixed with the bridge name.
     */
    BridgeMetrics get_metrics() const;

    // Bridge Component Access

    /**
     * @brief Get thread pool bridge
     * @return Shared pointer to thread pool bridge, or nullptr if not configured
     */
    std::shared_ptr<ThreadPoolBridge> get_thread_pool_bridge() const;

    /**
     * @brief Get thread pool interface
     * @return Shared pointer to thread pool, or nullptr if not configured
     *
     * Convenience method equivalent to:
     * @code
     * auto bridge = get_thread_pool_bridge();
     * return bridge ? bridge->get_thread_pool() : nullptr;
     * @endcode
     */
    std::shared_ptr<thread_pool_interface> get_thread_pool() const;

    /**
     * @brief Get logger interface
     * @return Shared pointer to logger, or nullptr if not configured
     */
    std::shared_ptr<logger_interface> get_logger() const;

    /**
     * @brief Get monitoring interface
     * @return Shared pointer to monitoring, or nullptr if not configured
     */
    std::shared_ptr<monitoring_interface> get_monitoring() const;

    // Configuration Methods

    /**
     * @brief Set custom thread pool bridge
     * @param bridge Thread pool bridge to use
     * @return ok() on success, error_info on failure
     *
     * Sets a custom thread pool bridge. Must be called before initialize().
     *
     * Error Conditions:
     * - Already initialized
     * - Null bridge pointer
     */
    VoidResult set_thread_pool_bridge(std::shared_ptr<ThreadPoolBridge> bridge);

    /**
     * @brief Set custom logger
     * @param logger Logger implementation to use
     * @return ok() on success, error_info on failure
     *
     * Sets a custom logger. Must be called before initialize().
     */
    VoidResult set_logger(std::shared_ptr<logger_interface> logger);

    /**
     * @brief Set custom monitoring
     * @param monitoring Monitoring implementation to use
     * @return ok() on success, error_info on failure
     *
     * Sets a custom monitoring implementation. Must be called before initialize().
     */
    VoidResult set_monitoring(std::shared_ptr<monitoring_interface> monitoring);

    // Factory Methods

    /**
     * @brief Create bridge with default configuration
     * @return Shared pointer to NetworkSystemBridge
     *
     * Creates a bridge with:
     * - Thread pool from thread_system (if available)
     * - Default logger (disabled)
     * - Default monitoring (disabled)
     *
     * Example:
     * @code
     * auto bridge = NetworkSystemBridge::create_default();
     * NetworkSystemBridgeConfig config;
     * bridge->initialize(config);
     * @endcode
     */
    static std::shared_ptr<NetworkSystemBridge> create_default();

    /**
     * @brief Create bridge with thread_system integration
     * @param pool_name Thread pool name (default: "network_pool")
     * @return Shared pointer to NetworkSystemBridge
     *
     * Creates a bridge configured for thread_system integration.
     *
     * Example:
     * @code
     * auto bridge = NetworkSystemBridge::with_thread_system("network_pool");
     * NetworkSystemBridgeConfig config;
     * config.enable_thread_pool = true;
     * bridge->initialize(config);
     * @endcode
     */
    static std::shared_ptr<NetworkSystemBridge> with_thread_system(
        const std::string& pool_name = "network_pool");

#if KCENON_WITH_COMMON_SYSTEM
    /**
     * @brief Create bridge with common_system integration
     * @param executor Executor from common_system
     * @param logger Logger from common_system (optional)
     * @param monitor Monitor from common_system (optional)
     * @return Shared pointer to NetworkSystemBridge
     *
     * Creates a bridge configured for common_system integration.
     *
     * Example:
     * @code
     * auto executor = container.resolve<IExecutor>();
     * auto logger = container.resolve<ILogger>();
     * auto bridge = NetworkSystemBridge::with_common_system(executor, logger);
     * NetworkSystemBridgeConfig config;
     * config.enable_thread_pool = true;
     * config.enable_logger = true;
     * bridge->initialize(config);
     * @endcode
     */
    static std::shared_ptr<NetworkSystemBridge> with_common_system(
        std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor,
        std::shared_ptr<::kcenon::common::interfaces::ILogger> logger = nullptr,
        std::shared_ptr<::kcenon::common::interfaces::IMonitor> monitor = nullptr);
#endif

    /**
     * @brief Create bridge with custom components
     * @param thread_pool Custom thread pool (optional)
     * @param logger Custom logger (optional)
     * @param monitoring Custom monitoring (optional)
     * @return Shared pointer to NetworkSystemBridge
     *
     * Creates a bridge with custom components.
     */
    static std::shared_ptr<NetworkSystemBridge> with_custom(
        std::shared_ptr<thread_pool_interface> thread_pool = nullptr,
        std::shared_ptr<logger_interface> logger = nullptr,
        std::shared_ptr<monitoring_interface> monitoring = nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace kcenon::network::integration

// Backward compatibility namespace alias
namespace network_system {
    namespace integration = kcenon::network::integration;
}
