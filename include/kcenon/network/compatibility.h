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
 * @file compatibility.h
 * @brief Compatibility layer for messaging_system migration
 *
 * This header provides backward compatibility aliases and wrappers
 * to allow existing messaging_system code to work with network_system
 * without modification.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include "kcenon/network/core/messaging_server.h"
#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/session/messaging_session.h"
#ifdef BUILD_MESSAGING_BRIDGE
#include "kcenon/network/integration/messaging_bridge.h"
#endif
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/integration/container_integration.h"
#include "kcenon/network/config/network_config.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/utils/health_monitor.h"

// Legacy namespace aliases for backward compatibility
namespace network_module {
    // Core types
    using messaging_server = ::kcenon::network::core::messaging_server;
    using messaging_client = ::kcenon::network::core::messaging_client;

    // Session types
    using messaging_session = ::kcenon::network::session::messaging_session;

#ifdef BUILD_MESSAGING_BRIDGE
    // Integration types
    using messaging_bridge = ::kcenon::network::integration::messaging_bridge;
#endif

    // Thread integration
    using thread_pool_interface = ::kcenon::network::integration::thread_pool_interface;
    using basic_thread_pool = ::kcenon::network::integration::basic_thread_pool;
    using thread_integration_manager = ::kcenon::network::integration::thread_integration_manager;

    // Container integration
    using container_interface = ::kcenon::network::integration::container_interface;
    using basic_container = ::kcenon::network::integration::basic_container;
    using container_manager = ::kcenon::network::integration::container_manager;

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    using container_system_adapter = ::kcenon::network::integration::container_system_adapter;
#endif

    /**
     * @brief Legacy factory function for creating servers
     * @param server_id Server identifier
     * @return Shared pointer to messaging server
     */
    inline std::shared_ptr<messaging_server> create_server(const std::string& server_id) {
        return std::make_shared<messaging_server>(server_id);
    }

    /**
     * @brief Legacy factory function for creating clients
     * @param client_id Client identifier
     * @return Shared pointer to messaging client
     */
    inline std::shared_ptr<messaging_client> create_client(const std::string& client_id) {
        return std::make_shared<messaging_client>(client_id);
    }

#ifdef BUILD_MESSAGING_BRIDGE
    /**
     * @brief Legacy factory function for creating bridges
     * @return Shared pointer to messaging bridge
     */
    inline std::shared_ptr<messaging_bridge> create_bridge() {
        return std::make_shared<messaging_bridge>();
    }
#endif
}

// Additional compatibility namespace
namespace messaging {
    // Import all types from network_module for double compatibility
    using namespace network_module;
}

// Legacy network_system namespace for backward compatibility
// DEPRECATED: Use kcenon::network directly in new code
namespace network_system {
    // Import all kcenon::network types
    using namespace kcenon::network;

    // Explicit namespace aliases for sub-namespaces
    namespace core = kcenon::network::core;
    namespace session = kcenon::network::session;
    namespace integration = kcenon::network::integration;
    namespace config = kcenon::network::config;
    namespace internal = kcenon::network::internal;
    namespace utils = kcenon::network::utils;
}

// Feature detection macros
#ifdef BUILD_WITH_CONTAINER_SYSTEM
    #define HAS_CONTAINER_INTEGRATION 1
#else
    #define HAS_CONTAINER_INTEGRATION 0
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
    #define HAS_THREAD_INTEGRATION 1
#else
    #define HAS_THREAD_INTEGRATION 0
#endif

// Deprecated macro for marking old code
#ifdef __GNUC__
    #define NETWORK_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
    #define NETWORK_DEPRECATED __declspec(deprecated)
#else
    #define NETWORK_DEPRECATED
#endif

/**
 * @namespace kcenon::network::compat
 * @brief Compatibility utilities namespace
 */
namespace kcenon::network::compat {

    /**
     * @brief Check if container integration is available
     * @return true if container system is integrated
     */
    inline constexpr bool has_container_support() {
        return HAS_CONTAINER_INTEGRATION;
    }

    /**
     * @brief Check if thread integration is available
     * @return true if thread system is integrated
     */
    inline constexpr bool has_thread_support() {
        return HAS_THREAD_INTEGRATION;
    }

    /**
     * @brief Initialize network system with default settings
     */
    inline void initialize() {
        // Initialize thread pool if not already set
        auto& thread_mgr = integration::thread_integration_manager::instance();
        if (!thread_mgr.get_thread_pool()) {
            thread_mgr.set_thread_pool(
                std::make_shared<integration::basic_thread_pool>()
            );
        }

        // Initialize container manager if not already set
        auto& container_mgr = integration::container_manager::instance();
        if (!container_mgr.get_default_container()) {
            container_mgr.set_default_container(
                std::make_shared<integration::basic_container>()
            );
        }
    }

    /**
     * @brief Shutdown network system cleanly
     */
    inline void shutdown() {
        // Clean shutdown of thread pool
        auto& thread_mgr = integration::thread_integration_manager::instance();
        if (auto pool = thread_mgr.get_thread_pool()) {
            if (auto basic = std::dynamic_pointer_cast<integration::basic_thread_pool>(pool)) {
                basic->stop(true);  // Wait for pending tasks
            }
        }
    }
}