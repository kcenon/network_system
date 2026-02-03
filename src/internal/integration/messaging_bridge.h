/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#pragma once

/**
 * @file messaging_bridge.h
 * @brief Bridge for messaging_system compatibility implementing INetworkBridge
 *
 * This bridge provides backward compatibility with the existing messaging_system
 * while using the new independent network_system implementation. Updated to
 * implement INetworkBridge for unified lifecycle management.
 *
 * @author kcenon
 * @date 2025-09-19
 */

#include <kcenon/network/config/feature_flags.h>
#include "internal/integration/bridge_interface.h"
#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/core/messaging_server.h"

#if KCENON_WITH_CONTAINER_SYSTEM
#include "container.h"
#endif

#if KCENON_WITH_THREAD_SYSTEM
// Suppress deprecation warnings from thread_system headers
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
// Include format for thread_system headers that use std::format
#  include <format>
#include <kcenon/thread/core/thread_pool.h>
#  pragma clang diagnostic pop
#endif

#include "kcenon/network/integration/thread_integration.h"

#include <memory>
#include <string>
#include <functional>
#include <chrono>

namespace kcenon::network::integration {

/**
 * @class messaging_bridge
 * @brief Bridge class for messaging_system compatibility implementing INetworkBridge
 *
 * This class provides a compatibility layer that allows existing messaging_system
 * code to work with the new independent network_system without modification.
 *
 * Lifecycle:
 * 1. Create using constructor or factory methods
 * 2. Call initialize() with configuration
 * 3. Use create_server() and create_client() to create messaging components
 * 4. Call shutdown() before destruction
 *
 * Thread Safety:
 * - initialize() and shutdown() are not thread-safe (single-threaded usage)
 * - get_metrics() is thread-safe for concurrent queries
 * - create_server() and create_client() are thread-safe after initialization
 */
class messaging_bridge : public INetworkBridge {
public:
    /**
     * @brief Default constructor
     */
    messaging_bridge();

    /**
     * @brief Destructor
     *
     * Automatically calls shutdown() if initialized
     */
    ~messaging_bridge() override;

    // INetworkBridge interface implementation

    /**
     * @brief Initialize the bridge with configuration
     * @param config Configuration parameters
     * @return ok() on success, error_info on failure
     *
     * Configuration Properties:
     * - "enabled": "true" or "false" (default: "true")
     * - "server_id": Default server identifier (optional)
     * - "client_id": Default client identifier (optional)
     *
     * Error Conditions:
     * - Already initialized
     * - Invalid configuration
     *
     * Example:
     * @code
     * BridgeConfig config;
     * config.integration_name = "messaging_system";
     * config.properties["enabled"] = "true";
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
     * Shuts down the bridge and releases resources.
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
     * @return Bridge metrics including messaging statistics
     *
     * Custom Metrics:
     * - "messages_sent": Total messages sent
     * - "messages_received": Total messages received
     * - "bytes_sent": Total bytes sent
     * - "bytes_received": Total bytes received
     * - "connections_active": Number of active connections
     * - "avg_latency_ms": Average message latency in milliseconds
     *
     * Thread Safety: Safe to call concurrently
     */
    BridgeMetrics get_metrics() const override;

    // messaging_bridge-specific methods (maintained for backward compatibility)

    /**
     * @brief Create a messaging server with messaging_system compatible API
     * @param server_id Unique identifier for the server
     * @return Shared pointer to the created server
     */
    std::shared_ptr<kcenon::network::core::messaging_server> create_server(
        const std::string& server_id
    );

    /**
     * @brief Create a messaging client with messaging_system compatible API
     * @param client_id Unique identifier for the client
     * @return Shared pointer to the created client
     */
    std::shared_ptr<kcenon::network::core::messaging_client> create_client(
        const std::string& client_id
    );

#if KCENON_WITH_CONTAINER_SYSTEM
    /**
     * @brief Set container for message serialization/deserialization
     * @param container Shared pointer to value container
     */
    void set_container(
        std::shared_ptr<container_module::value_container> container
    );

    /**
     * @brief Set container message handler
     * @param handler Function to handle container messages
     */
    void set_container_message_handler(
        std::function<void(const container_module::value_container&)> handler
    );
#endif

#if KCENON_WITH_THREAD_SYSTEM
    /**
     * @brief Set thread pool for asynchronous operations
     * @param pool Shared pointer to thread pool
     */
    void set_thread_pool(
        std::shared_ptr<kcenon::thread::thread_pool> pool
    );
#endif

    /**
     * @brief Set thread pool using the integration interface
     * @param pool Thread pool interface implementation
     */
    void set_thread_pool_interface(
        std::shared_ptr<thread_pool_interface> pool
    );

    /**
     * @brief Get the thread pool interface
     * @return Current thread pool interface
     */
    std::shared_ptr<thread_pool_interface> get_thread_pool_interface() const;

    /**
     * @brief Performance metrics structure (deprecated, use get_metrics() from INetworkBridge)
     * @deprecated Use BridgeMetrics from INetworkBridge::get_metrics() instead
     */
    struct performance_metrics {
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t connections_active = 0;
        std::chrono::milliseconds avg_latency{0};
        std::chrono::steady_clock::time_point start_time;
    };

    /**
     * @brief Get current performance metrics (deprecated)
     * @return Current performance metrics
     * @deprecated Use get_metrics() from INetworkBridge instead
     */
    performance_metrics get_performance_metrics() const;

    /**
     * @brief Reset performance metrics
     */
    void reset_metrics();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

// Namespace alias for backward compatibility
namespace bridge = kcenon::network::integration;

} // namespace kcenon::network::integration

// Additional backward compatibility aliases
namespace network_module {
    using messaging_bridge = kcenon::network::integration::messaging_bridge;
    using messaging_server = kcenon::network::core::messaging_server;
    using messaging_client = kcenon::network::core::messaging_client;
    using messaging_session = kcenon::network::session::messaging_session;
}