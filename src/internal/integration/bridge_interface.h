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
 * @file bridge_interface.h
 * @brief Unified interface for external system integration bridges
 *
 * This file defines the core interface for all integration bridges in network_system.
 * A bridge provides a consistent way to integrate with external systems while
 * maintaining lifecycle management, configuration, and metrics reporting.
 *
 * Design Goals:
 * - Unified interface for all external system integrations
 * - Consistent lifecycle management (initialize, shutdown)
 * - Configuration support for runtime adaptation
 * - Health and metrics reporting
 * - Type-safe error handling via Result<T>
 *
 * Usage Example:
 * @code
 * class ThreadPoolBridge : public INetworkBridge {
 * public:
 *     Result<void> initialize(const BridgeConfig& config) override {
 *         // Setup thread pool with config parameters
 *         return ok();
 *     }
 *
 *     Result<void> shutdown() override {
 *         // Gracefully shutdown thread pool
 *         return ok();
 *     }
 *
 *     // ... other INetworkBridge methods
 * };
 * @endcode
 *
 * Related Issues:
 * - #579: Consolidate integration adapters into NetworkSystemBridge
 * - #577: EPIC for Facade pattern refactoring
 *
 * @author network_system team
 * @date 2026-02-01
 */

#include <kcenon/network/utils/result_types.h>
#include <chrono>
#include <map>
#include <string>

namespace kcenon::network::integration {

/**
 * @struct BridgeConfig
 * @brief Configuration for bridge initialization
 *
 * This structure provides a flexible key-value configuration mechanism
 * for bridges. Each bridge type interprets the properties map according
 * to its specific needs.
 *
 * Example:
 * @code
 * BridgeConfig config;
 * config.integration_name = "thread_system";
 * config.properties["pool_name"] = "network_pool";
 * config.properties["worker_count"] = "8";
 * @endcode
 */
struct BridgeConfig {
    /**
     * @brief Name identifying the external system being integrated
     *
     * Examples: "thread_system", "common_system", "messaging_system"
     */
    std::string integration_name;

    /**
     * @brief Key-value properties for bridge-specific configuration
     *
     * Property keys and values are bridge-specific. Common properties:
     * - "worker_count": Number of worker threads
     * - "pool_name": Thread pool identifier
     * - "enable_logging": Enable/disable logging
     * - "log_level": Minimum log level
     */
    std::map<std::string, std::string> properties;
};

/**
 * @struct BridgeMetrics
 * @brief Metrics and health information for a bridge
 *
 * This structure provides standardized metrics reporting across all bridges.
 * Each bridge can extend with custom metrics via the custom_metrics map.
 *
 * Example:
 * @code
 * BridgeMetrics metrics;
 * metrics.is_healthy = true;
 * metrics.last_activity = std::chrono::steady_clock::now();
 * metrics.custom_metrics["pending_tasks"] = 42.0;
 * metrics.custom_metrics["worker_threads"] = 8.0;
 * @endcode
 */
struct BridgeMetrics {
    /**
     * @brief Overall health status of the bridge
     *
     * false indicates the bridge has encountered errors or is in degraded state
     */
    bool is_healthy{true};

    /**
     * @brief Timestamp of last activity or health check
     *
     * Updated when the bridge performs operations or reports health
     */
    std::chrono::steady_clock::time_point last_activity;

    /**
     * @brief Bridge-specific custom metrics
     *
     * Each bridge can report custom metrics here. Common metric names:
     * - "pending_tasks": Number of queued tasks (thread pools)
     * - "worker_threads": Number of worker threads
     * - "messages_sent": Total messages sent (messaging bridges)
     * - "connections_active": Active connections
     * - "error_count": Number of errors encountered
     */
    std::map<std::string, double> custom_metrics;
};

/**
 * @class INetworkBridge
 * @brief Abstract interface for external system integration bridges
 *
 * This interface defines the contract for all integration bridges in network_system.
 * Each bridge provides a consistent way to:
 * - Initialize with configuration
 * - Manage lifecycle (shutdown)
 * - Report health and metrics
 * - Check initialization status
 *
 * Lifecycle:
 * 1. Construct bridge instance
 * 2. Call initialize() with configuration
 * 3. Use bridge functionality
 * 4. Call shutdown() before destruction
 *
 * Thread Safety:
 * - Implementations should be thread-safe for concurrent metric queries
 * - initialize() and shutdown() need not be thread-safe (call from one thread)
 *
 * Error Handling:
 * - All operations return Result<T> for type-safe error propagation
 * - Failed initialization should prevent bridge usage
 * - Shutdown should always succeed or log errors internally
 */
class INetworkBridge {
public:
    virtual ~INetworkBridge() = default;

    /**
     * @brief Initialize the bridge with configuration
     * @param config Configuration parameters for the bridge
     * @return ok() on success, error_info on failure
     *
     * This method must be called before using the bridge. Initialization
     * sets up the external system integration according to the provided
     * configuration.
     *
     * Error Conditions:
     * - Invalid configuration parameters
     * - External system unavailable
     * - Resource allocation failure
     * - Already initialized
     *
     * Example:
     * @code
     * BridgeConfig config;
     * config.integration_name = "thread_system";
     * config.properties["pool_name"] = "network_pool";
     *
     * auto result = bridge->initialize(config);
     * if (result.is_err()) {
     *     std::cerr << "Failed to initialize: " << result.error().message << std::endl;
     *     return;
     * }
     * @endcode
     */
    virtual VoidResult initialize(const BridgeConfig& config) = 0;

    /**
     * @brief Shutdown the bridge and release resources
     * @return ok() on success, error_info on failure
     *
     * This method should be called before destroying the bridge. It
     * gracefully shuts down the external system integration and releases
     * any held resources.
     *
     * Shutdown should be idempotent - calling shutdown() multiple times
     * should not cause errors.
     *
     * Error Conditions:
     * - Resource cleanup failure (should log but not throw)
     * - External system communication error
     *
     * Example:
     * @code
     * auto result = bridge->shutdown();
     * if (result.is_err()) {
     *     std::cerr << "Shutdown error: " << result.error().message << std::endl;
     * }
     * @endcode
     */
    virtual VoidResult shutdown() = 0;

    /**
     * @brief Check if the bridge is initialized and ready for use
     * @return true if initialized, false otherwise
     *
     * This method provides a quick way to check if the bridge has been
     * successfully initialized and is ready for operation.
     *
     * Returns false if:
     * - initialize() has not been called
     * - initialize() failed
     * - shutdown() has been called
     *
     * Example:
     * @code
     * if (!bridge->is_initialized()) {
     *     std::cerr << "Bridge not initialized" << std::endl;
     *     return;
     * }
     * // Use bridge functionality
     * @endcode
     */
    virtual bool is_initialized() const = 0;

    /**
     * @brief Get current metrics and health information
     * @return Current bridge metrics
     *
     * This method returns health and performance metrics for the bridge.
     * It should be lightweight and suitable for frequent polling.
     *
     * Thread Safety: Must be safe to call concurrently from multiple threads
     *
     * Example:
     * @code
     * auto metrics = bridge->get_metrics();
     * if (!metrics.is_healthy) {
     *     std::cerr << "Bridge unhealthy" << std::endl;
     * }
     *
     * if (metrics.custom_metrics.count("pending_tasks") > 0) {
     *     std::cout << "Pending tasks: " << metrics.custom_metrics["pending_tasks"] << std::endl;
     * }
     * @endcode
     */
    virtual BridgeMetrics get_metrics() const = 0;
};

} // namespace kcenon::network::integration

// Backward compatibility namespace alias
namespace network_system {
    namespace integration = kcenon::network::integration;
}
