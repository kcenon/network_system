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
 * @file observability_bridge.h
 * @brief Observability (logger and monitoring) integration bridge for network_system
 *
 * This file provides the ObservabilityBridge class that consolidates logger
 * and monitoring integrations into a single, unified bridge.
 *
 * Design Goals:
 * - Unified interface for observability integration
 * - Support for both common_system and standalone backends
 * - Factory methods for common configurations
 * - Lifecycle management via INetworkBridge interface
 *
 * Usage Example:
 * @code
 * // Using common_system backend
 * auto logger = container.resolve<ILogger>();
 * auto monitor = container.resolve<IMonitor>();
 * auto bridge = ObservabilityBridge::from_common_system(logger, monitor);
 * bridge->initialize(config);
 *
 * // Using standalone backend
 * auto logger = std::make_shared<basic_logger>();
 * auto monitor = std::make_shared<basic_monitoring>();
 * auto bridge = std::make_shared<ObservabilityBridge>(logger, monitor);
 * bridge->initialize(config);
 * @endcode
 *
 * Related Issues:
 * - #583: Implement ObservabilityBridge for logger and monitor integration
 * - #579: Consolidate integration adapters into NetworkSystemBridge
 *
 * @author network_system team
 * @date 2026-02-01
 */

#include "internal/integration/bridge_interface.h"
#include "internal/integration/logger_integration.h"
#include "internal/integration/monitoring_integration.h"
#include <kcenon/network/config/feature_flags.h>
#include <memory>
#include <mutex>
#include <atomic>

#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#endif

namespace kcenon::network::integration {

/**
 * @class ObservabilityBridge
 * @brief Bridge for observability (logger + monitoring) integration implementing INetworkBridge
 *
 * This class consolidates logger and monitoring integrations into a single,
 * unified bridge. It provides factory methods for creating bridges from
 * different backend types.
 *
 * Backend Types:
 * - CommonSystem: Uses common_system's ILogger and IMonitor (when KCENON_WITH_COMMON_SYSTEM)
 * - Standalone: Uses network_system's logger_interface and monitoring_interface
 *
 * Lifecycle:
 * 1. Create using factory method (from_common_system) or direct constructor
 * 2. Call initialize() with configuration
 * 3. Use get_logger() and get_monitor() to access observability interfaces
 * 4. Call shutdown() before destruction
 *
 * Thread Safety:
 * - initialize() and shutdown() are not thread-safe (single-threaded usage)
 * - get_metrics() is thread-safe for concurrent queries
 * - get_logger() and get_monitor() are thread-safe after initialization
 */
class ObservabilityBridge : public INetworkBridge {
public:
    /**
     * @enum BackendType
     * @brief Type of observability backend
     */
    enum class BackendType {
        CommonSystem,  ///< Uses common_system's ILogger and IMonitor
        Standalone     ///< Uses network_system's logger_interface and monitoring_interface
    };

    /**
     * @brief Construct bridge with logger and monitoring interfaces
     * @param logger Logger implementation
     * @param monitor Monitoring implementation
     * @param backend_type Type of backend (default: Standalone)
     * @throws std::invalid_argument if logger or monitor is nullptr
     *
     * Example:
     * @code
     * auto logger = std::make_shared<basic_logger>();
     * auto monitor = std::make_shared<basic_monitoring>();
     * auto bridge = std::make_shared<ObservabilityBridge>(logger, monitor);
     * @endcode
     */
    explicit ObservabilityBridge(
        std::shared_ptr<logger_interface> logger,
        std::shared_ptr<monitoring_interface> monitor,
        BackendType backend_type = BackendType::Standalone);

    /**
     * @brief Destructor
     *
     * Automatically calls shutdown() if initialized
     */
    ~ObservabilityBridge() override;

    // INetworkBridge interface implementation

    /**
     * @brief Initialize the bridge with configuration
     * @param config Configuration parameters
     * @return ok() on success, error_info on failure
     *
     * Configuration Properties:
     * - "enabled": "true" or "false" (default: "true")
     * - "log_level": Minimum log level (informational)
     * - "enable_monitoring": Enable/disable monitoring (default: "true")
     *
     * Error Conditions:
     * - Already initialized
     * - Logger or monitor is null
     * - Invalid configuration
     *
     * Example:
     * @code
     * BridgeConfig config;
     * config.integration_name = "common_system";
     * config.properties["log_level"] = "info";
     * config.properties["enable_monitoring"] = "true";
     *
     * auto result = bridge->initialize(config);
     * if (result.is_err()) {
     *     std::cerr << "Init failed: " << result.error().message << std::endl;
     * }
     * @endcode
     */
    VoidResult initialize(const BridgeConfig& config) override;

    /**
     * @brief Shutdown the bridge
     * @return ok() on success, error_info on failure
     *
     * Shuts down the bridge and flushes any buffered logs.
     * Logger and monitor lifecycle is managed externally.
     *
     * This method is idempotent - multiple calls are safe.
     */
    VoidResult shutdown() override;

    /**
     * @brief Check if the bridge is initialized
     * @return true if initialized and ready, false otherwise
     */
    bool is_initialized() const override;

    /**
     * @brief Get current metrics
     * @return Bridge metrics including observability statistics
     *
     * Custom Metrics:
     * - "backend_type": Backend type (0=CommonSystem, 1=Standalone)
     * - "monitoring_enabled": 1.0 if monitoring is enabled, 0.0 otherwise
     * - "logger_available": 1.0 if logger is available, 0.0 otherwise
     * - "monitor_available": 1.0 if monitor is available, 0.0 otherwise
     *
     * Thread Safety: Safe to call concurrently
     */
    BridgeMetrics get_metrics() const override;

    // ObservabilityBridge-specific methods

    /**
     * @brief Get the logger interface
     * @return Shared pointer to logger, or nullptr if not initialized
     *
     * Thread Safety: Safe to call after initialization
     *
     * Example:
     * @code
     * if (auto logger = bridge->get_logger()) {
     *     logger->log(log_level::info, "Application started");
     * }
     * @endcode
     */
    std::shared_ptr<logger_interface> get_logger() const;

    /**
     * @brief Get the monitoring interface
     * @return Shared pointer to monitoring interface, or nullptr if not initialized
     *
     * Thread Safety: Safe to call after initialization
     *
     * Example:
     * @code
     * if (auto monitor = bridge->get_monitor()) {
     *     monitor->report_counter("requests_total", 1);
     * }
     * @endcode
     */
    std::shared_ptr<monitoring_interface> get_monitor() const;

    /**
     * @brief Get the backend type
     * @return Type of backend this bridge uses
     */
    BackendType get_backend_type() const;

    // Factory methods

#if KCENON_WITH_COMMON_SYSTEM
    /**
     * @brief Create bridge from common_system logger and monitor
     * @param logger Common system logger to adapt
     * @param monitor Common system monitor to adapt
     * @return Shared pointer to ObservabilityBridge
     * @throws std::invalid_argument if logger or monitor is nullptr
     *
     * Creates a bridge that adapts common_system's ILogger and IMonitor to
     * network_system's logger_interface and monitoring_interface.
     *
     * Example:
     * @code
     * auto logger = container.resolve<ILogger>();
     * auto monitor = container.resolve<IMonitor>();
     * auto bridge = ObservabilityBridge::from_common_system(logger, monitor);
     * BridgeConfig config;
     * config.integration_name = "common_system";
     * bridge->initialize(config);
     * @endcode
     */
    static std::shared_ptr<ObservabilityBridge> from_common_system(
        std::shared_ptr<::kcenon::common::interfaces::ILogger> logger,
        std::shared_ptr<::kcenon::common::interfaces::IMonitor> monitor);
#endif

private:
    std::shared_ptr<logger_interface> logger_;
    std::shared_ptr<monitoring_interface> monitor_;
    BackendType backend_type_;
    std::atomic<bool> initialized_{false};
    mutable std::mutex metrics_mutex_;
    mutable BridgeMetrics cached_metrics_;
    bool monitoring_enabled_{true};
};

} // namespace kcenon::network::integration

// Backward compatibility namespace alias
namespace network_system {
    namespace integration = kcenon::network::integration;
}
