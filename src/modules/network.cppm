// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file network.cppm
 * @brief Primary C++20 module for network_system.
 *
 * This is the main module interface for the network_system library.
 * It aggregates all module partitions to provide a single import point.
 *
 * Usage:
 * @code
 * import kcenon.network;
 *
 * using namespace kcenon::network::core;
 *
 * // TCP Client example
 * auto client = std::make_unique<messaging_client>("my_client");
 * client->set_connection_callback([]() {
 *     std::cout << "Connected!" << std::endl;
 * });
 * client->start_client("localhost", 8080);
 *
 * // UDP Server example
 * auto server = std::make_unique<messaging_udp_server>("udp_server");
 * server->start_server(9090);
 *
 * // Secure TCP example
 * ssl_config config;
 * config.certificate_path = "/path/to/cert.pem";
 * auto secure = std::make_unique<secure_messaging_server>("secure_server", config);
 * secure->start_server(443);
 * @endcode
 *
 * Module Structure:
 * - kcenon.network:core - Core network interfaces, connection management
 * - kcenon.network:tcp - TCP client/server implementations
 * - kcenon.network:udp - UDP implementations
 * - kcenon.network:ssl - SSL/TLS support (requires OpenSSL)
 *
 * Dependencies:
 * - kcenon.common (Tier 0) - Result<T>, error handling, interfaces
 * - kcenon.thread (Tier 1) - Thread pool integration
 */

export module kcenon.network;

// Import and re-export dependent modules
export import kcenon.common;
export import kcenon.thread;

// Tier 1: Core network infrastructure
export import :core;

// Tier 2: Protocol implementations
export import :tcp;
export import :udp;

// Tier 3: Secure protocols (optional - requires OpenSSL)
export import :ssl;

// =============================================================================
// Module-Level API
// =============================================================================

export namespace kcenon::network {

/**
 * @brief Version information for network_system module.
 */
struct module_version {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;
    static constexpr int tweak = 0;
    static constexpr const char* string = "0.1.0.0";
    static constexpr const char* module_name = "kcenon.network";
};

/**
 * @brief Initialize the network system with default configuration
 * @return VoidResult - ok() on success, error on failure
 *
 * Possible errors:
 * - already_exists: Network system already initialized
 * - internal_error: System initialization failed
 */
kcenon::common::VoidResult initialize();

/**
 * @brief Shutdown the network system
 * @return VoidResult - ok() on success, error on failure
 *
 * Possible errors:
 * - not_initialized: Network system not initialized
 * - internal_error: Shutdown failed
 */
kcenon::common::VoidResult shutdown();

/**
 * @brief Check if network system is initialized
 * @return true if initialized, false otherwise
 */
bool is_initialized() noexcept;

/**
 * @brief Get the network system version string
 * @return Version string in "major.minor.patch.tweak" format
 */
constexpr const char* version() noexcept {
    return module_version::string;
}

} // namespace kcenon::network
