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
 * @file thread_pool_bridge.h
 * @brief Thread pool integration bridge for network_system
 *
 * This file provides the ThreadPoolBridge class that consolidates thread_system
 * and common_system thread pool integrations into a single, unified bridge.
 *
 * Design Goals:
 * - Unified interface for thread pool integration
 * - Support for both thread_system and common_system backends
 * - Factory methods for common configurations
 * - Lifecycle management via INetworkBridge interface
 *
 * Usage Example:
 * @code
 * // Using thread_system backend
 * auto bridge = ThreadPoolBridge::from_thread_system("network_pool");
 * bridge->initialize(config);
 *
 * // Using common_system backend
 * auto executor = container.resolve<IExecutor>();
 * auto bridge = ThreadPoolBridge::from_common_system(executor);
 * bridge->initialize(config);
 * @endcode
 *
 * Related Issues:
 * - #582: Implement ThreadPoolBridge for thread_system integration
 * - #579: Consolidate integration adapters into NetworkSystemBridge
 *
 * @author network_system team
 * @date 2026-02-01
 */

#include <kcenon/network/integration/bridge_interface.h>
#include <kcenon/network/integration/thread_integration.h>
#include <kcenon/network/config/feature_flags.h>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/executor_interface.h>
#endif

namespace kcenon::network::integration {

/**
 * @class ThreadPoolBridge
 * @brief Bridge for thread pool integration implementing INetworkBridge
 *
 * This class consolidates thread_system and common_system thread pool
 * integrations into a single, unified bridge. It provides factory methods
 * for creating bridges from different backend types.
 *
 * Backend Types:
 * - ThreadSystem: Uses thread_system's thread pool directly
 * - CommonSystem: Adapts common_system's IExecutor
 * - Custom: Uses user-provided thread_pool_interface
 *
 * Lifecycle:
 * 1. Create using factory method (from_thread_system, from_common_system)
 *    or direct constructor
 * 2. Call initialize() with configuration
 * 3. Use get_thread_pool() to access the underlying pool
 * 4. Call shutdown() before destruction
 *
 * Thread Safety:
 * - initialize() and shutdown() are not thread-safe (single-threaded usage)
 * - get_metrics() is thread-safe for concurrent queries
 * - get_thread_pool() is thread-safe after initialization
 */
class ThreadPoolBridge : public INetworkBridge {
public:
    /**
     * @enum BackendType
     * @brief Type of thread pool backend
     */
    enum class BackendType {
        ThreadSystem,  ///< Uses thread_system's thread pool
        CommonSystem,  ///< Uses common_system's IExecutor
        Custom         ///< Uses custom thread_pool_interface
    };

    /**
     * @brief Construct bridge with custom thread pool
     * @param pool Thread pool implementation
     * @param backend_type Type of backend (default: Custom)
     * @throws std::invalid_argument if pool is nullptr
     *
     * Example:
     * @code
     * auto pool = std::make_shared<basic_thread_pool>(8);
     * auto bridge = std::make_shared<ThreadPoolBridge>(pool);
     * @endcode
     */
    explicit ThreadPoolBridge(
        std::shared_ptr<thread_pool_interface> pool,
        BackendType backend_type = BackendType::Custom);

    /**
     * @brief Destructor
     *
     * Automatically calls shutdown() if initialized
     */
    ~ThreadPoolBridge() override;

    // INetworkBridge interface implementation

    /**
     * @brief Initialize the bridge with configuration
     * @param config Configuration parameters
     * @return ok() on success, error_info on failure
     *
     * Configuration Properties:
     * - "enabled": "true" or "false" (default: "true")
     * - "worker_count": Number of worker threads (informational)
     * - "pool_name": Thread pool identifier (informational)
     *
     * Error Conditions:
     * - Already initialized
     * - Underlying thread pool not running
     * - Invalid configuration
     *
     * Example:
     * @code
     * BridgeConfig config;
     * config.integration_name = "thread_system";
     * config.properties["pool_name"] = "network_pool";
     * config.properties["worker_count"] = "8";
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
     * Shuts down the bridge but does not shut down the underlying thread pool.
     * Thread pool lifecycle is managed externally.
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
     * @return Bridge metrics including thread pool statistics
     *
     * Custom Metrics:
     * - "worker_threads": Number of worker threads
     * - "pending_tasks": Number of queued tasks
     * - "backend_type": Backend type (0=ThreadSystem, 1=CommonSystem, 2=Custom)
     *
     * Thread Safety: Safe to call concurrently
     */
    BridgeMetrics get_metrics() const override;

    // ThreadPoolBridge-specific methods

    /**
     * @brief Get the underlying thread pool
     * @return Shared pointer to thread pool, or nullptr if not initialized
     *
     * Thread Safety: Safe to call after initialization
     *
     * Example:
     * @code
     * if (auto pool = bridge->get_thread_pool()) {
     *     pool->submit([]{ // task implementation here });
     * }
     * @endcode
     */
    std::shared_ptr<thread_pool_interface> get_thread_pool() const;

    /**
     * @brief Get the backend type
     * @return Type of backend this bridge uses
     */
    BackendType get_backend_type() const;

    // Factory methods

    /**
     * @brief Create bridge from thread_system
     * @param pool_name Thread pool name (default: "network_pool")
     * @return Shared pointer to ThreadPoolBridge
     *
     * Creates a bridge using thread_system's thread pool via
     * thread_integration_manager.
     *
     * Example:
     * @code
     * auto bridge = ThreadPoolBridge::from_thread_system("network_pool");
     * BridgeConfig config;
     * config.integration_name = "thread_system";
     * bridge->initialize(config);
     * @endcode
     */
    static std::shared_ptr<ThreadPoolBridge> from_thread_system(
        const std::string& pool_name = "network_pool");

#if KCENON_WITH_COMMON_SYSTEM
    /**
     * @brief Create bridge from common_system executor
     * @param executor Common system executor to adapt
     * @return Shared pointer to ThreadPoolBridge
     * @throws std::invalid_argument if executor is nullptr
     *
     * Creates a bridge that adapts common_system's IExecutor to
     * thread_pool_interface via common_to_network_thread_adapter.
     *
     * Example:
     * @code
     * auto executor = container.resolve<IExecutor>();
     * auto bridge = ThreadPoolBridge::from_common_system(executor);
     * BridgeConfig config;
     * config.integration_name = "common_system";
     * bridge->initialize(config);
     * @endcode
     */
    static std::shared_ptr<ThreadPoolBridge> from_common_system(
        std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor);
#endif

private:
    std::shared_ptr<thread_pool_interface> pool_;
    BackendType backend_type_;
    std::atomic<bool> initialized_{false};
    mutable std::mutex metrics_mutex_;
    mutable BridgeMetrics cached_metrics_;
};

} // namespace kcenon::network::integration

// Backward compatibility namespace alias
namespace network_system {
    namespace integration = kcenon::network::integration;
}
