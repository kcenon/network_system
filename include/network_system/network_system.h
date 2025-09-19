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
 * @version 2.0.0
 */

// Core networking components
#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"

// Session management
#include "network_system/session/messaging_session.h"

// Integration interfaces (conditionally compiled)
#ifdef BUILD_WITH_MESSAGING_BRIDGE
#include "network_system/integration/messaging_bridge.h"
#endif

#ifdef BUILD_WITH_CONTAINER_SYSTEM
#include "network_system/integration/container_integration.h"
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
#include "network_system/integration/thread_integration.h"
#endif

/**
 * @namespace network_system
 * @brief Main namespace for all Network System components
 */
namespace network_system {

/**
 * @brief Get version information
 * @return Version string in format "MAJOR.MINOR.PATCH"
 */
const char* get_version();

/**
 * @brief Initialize the network system
 * @return true if initialization successful, false otherwise
 */
bool initialize();

/**
 * @brief Shutdown the network system
 */
void shutdown();

} // namespace network_system