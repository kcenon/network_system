/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

/**
 * @file unified_compat.h
 * @brief Backward-compatible type aliases for unified messaging templates.
 *
 * This header provides type aliases that map the old class names to the new
 * unified template instantiations. This allows existing code to continue
 * working while migrating to the new API.
 *
 * ## Provided Aliases
 *
 * | Old Name | New Type |
 * |----------|----------|
 * | `messaging_client_compat` | `unified_messaging_client<tcp_protocol, no_tls>` |
 * | `secure_messaging_client_compat` | `unified_messaging_client<tcp_protocol, tls_enabled>` |
 * | `messaging_server_compat` | `unified_messaging_server<tcp_protocol, no_tls>` |
 * | `secure_messaging_server_compat` | `unified_messaging_server<tcp_protocol, tls_enabled>` |
 *
 * ## Migration Guide
 *
 * ### Step 1: Include the new header
 * @code
 * // Old code:
 * #include <kcenon/network/core/messaging_client.h>
 *
 * // New code:
 * #include <kcenon/network/core/unified_messaging_client.h>
 * @endcode
 *
 * ### Step 2: Use the new type aliases (optional)
 * @code
 * // Old code:
 * auto client = std::make_shared<messaging_client>("client1");
 *
 * // New code (option 1 - use tcp_client alias):
 * auto client = std::make_shared<tcp_client>("client1");
 *
 * // New code (option 2 - use full template):
 * auto client = std::make_shared<unified_messaging_client<tcp_protocol>>("client1");
 * @endcode
 *
 * ### Step 3: For secure clients
 * @code
 * // Old code:
 * auto client = std::make_shared<secure_messaging_client>("client1");
 *
 * // New code:
 * tls_enabled tls_config{
 *     .cert_path = "cert.pem",
 *     .key_path = "key.pem"
 * };
 * auto client = std::make_shared<secure_tcp_client>("client1", tls_config);
 * @endcode
 *
 * ## Breaking Changes
 *
 * The unified templates are API-compatible with the old classes for most use cases.
 * Notable differences:
 *
 * 1. **TLS Configuration**: Secure variants now require explicit TLS configuration
 *    passed to the constructor instead of being configured after construction.
 *
 * 2. **Template Parameters**: The new API uses template parameters for protocol
 *    and TLS policy selection, enabling compile-time optimization.
 *
 * 3. **Namespace**: Type aliases are in `kcenon::network::core` namespace.
 *
 * @see unified_messaging_client
 * @see unified_messaging_server
 */

#include "kcenon/network/core/unified_messaging_client.h"
#include "kcenon/network/core/unified_messaging_server.h"

namespace kcenon::network::compat {

// ============================================================================
// Deprecation Macros
// ============================================================================

/**
 * @def NETWORK_DEPRECATED
 * @brief Marks a declaration as deprecated with a custom message.
 *
 * Uses [[deprecated]] attribute for C++14+ compilers.
 */
#if defined(__cplusplus) && __cplusplus >= 201402L
#    define NETWORK_DEPRECATED(msg) [[deprecated(msg)]]
#elif defined(__GNUC__) || defined(__clang__)
#    define NETWORK_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#    define NETWORK_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#    define NETWORK_DEPRECATED(msg)
#endif

// ============================================================================
// Backward-Compatible Type Aliases (TCP)
// ============================================================================

/**
 * @brief Backward-compatible alias for plain TCP messaging client.
 *
 * @deprecated Use `tcp_client` or `unified_messaging_client<tcp_protocol>` instead.
 *
 * @code
 * // Migration:
 * // Old: std::make_shared<messaging_client_compat>("id")
 * // New: std::make_shared<tcp_client>("id")
 * @endcode
 */
using messaging_client_compat NETWORK_DEPRECATED(
    "Use tcp_client or unified_messaging_client<tcp_protocol> instead") =
    core::unified_messaging_client<protocol::tcp_protocol, policy::no_tls>;

/**
 * @brief Backward-compatible alias for plain TCP messaging server.
 *
 * @deprecated Use `tcp_server` or `unified_messaging_server<tcp_protocol>` instead.
 */
using messaging_server_compat NETWORK_DEPRECATED(
    "Use tcp_server or unified_messaging_server<tcp_protocol> instead") =
    core::unified_messaging_server<protocol::tcp_protocol, policy::no_tls>;

#ifdef BUILD_TLS_SUPPORT
/**
 * @brief Backward-compatible alias for secure TCP messaging client.
 *
 * @deprecated Use `secure_tcp_client` or
 *             `unified_messaging_client<tcp_protocol, tls_enabled>` instead.
 */
using secure_messaging_client_compat NETWORK_DEPRECATED(
    "Use secure_tcp_client or unified_messaging_client<tcp_protocol, tls_enabled> instead") =
    core::unified_messaging_client<protocol::tcp_protocol, policy::tls_enabled>;

/**
 * @brief Backward-compatible alias for secure TCP messaging server.
 *
 * @deprecated Use `secure_tcp_server` or
 *             `unified_messaging_server<tcp_protocol, tls_enabled>` instead.
 */
using secure_messaging_server_compat NETWORK_DEPRECATED(
    "Use secure_tcp_server or unified_messaging_server<tcp_protocol, tls_enabled> instead") =
    core::unified_messaging_server<protocol::tcp_protocol, policy::tls_enabled>;
#endif  // BUILD_TLS_SUPPORT

// ============================================================================
// Non-Deprecated Convenience Aliases
// ============================================================================

/**
 * @brief Plain TCP client (non-deprecated convenience alias).
 *
 * This alias is provided for users who want to use the compatibility header
 * but don't want deprecation warnings.
 */
using plain_tcp_client = core::tcp_client;

/**
 * @brief Plain TCP server (non-deprecated convenience alias).
 */
using plain_tcp_server = core::tcp_server;

#ifdef BUILD_TLS_SUPPORT
/**
 * @brief Secure TCP client (non-deprecated convenience alias).
 */
using tls_tcp_client = core::secure_tcp_client;

/**
 * @brief Secure TCP server (non-deprecated convenience alias).
 */
using tls_tcp_server = core::secure_tcp_server;
#endif  // BUILD_TLS_SUPPORT

}  // namespace kcenon::network::compat

// ============================================================================
// Global Namespace Aliases (for ease of migration)
// ============================================================================

namespace kcenon::network::core {

// Re-export protocol and policy types for convenience
using namespace kcenon::network::protocol;
using namespace kcenon::network::policy;

}  // namespace kcenon::network::core
