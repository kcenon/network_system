/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#pragma once

/**
 * @file legacy_aliases.h
 * @brief Deprecated type aliases for legacy interface compatibility
 *
 * This header provides backward-compatible type aliases that map legacy
 * protocol-specific interfaces to the new unified interface API.
 *
 * ## Interface Consolidation Summary
 *
 * The network_system library has consolidated 12+ protocol-specific interfaces
 * into 3 core abstractions:
 *
 * | Old Interface | New Interface | Factory Function |
 * |---------------|---------------|------------------|
 * | `i_client` | `unified::i_connection` | `protocol::*::connect()` |
 * | `i_tcp_client` | `unified::i_connection` | `protocol::tcp::connect()` |
 * | `i_udp_client` | `unified::i_connection` | `protocol::udp::connect()` |
 * | `i_websocket_client` | `unified::i_connection` | `protocol::websocket::connect()` |
 * | `i_quic_client` | `unified::i_connection` | `protocol::quic::connect()` |
 * | `i_server` | `unified::i_listener` | `protocol::*::listen()` |
 * | `i_tcp_server` | `unified::i_listener` | `protocol::tcp::listen()` |
 * | `i_udp_server` | `unified::i_listener` | `protocol::udp::listen()` |
 * | `i_websocket_server` | `unified::i_listener` | `protocol::websocket::listen()` |
 * | `i_quic_server` | `unified::i_listener` | `protocol::quic::listen()` |
 * | `i_session` | `unified::i_connection` | (accepted connections) |
 *
 * ## Migration Path
 *
 * 1. Replace interface type with `i_connection` or `i_listener`
 * 2. Use protocol factory functions instead of direct construction
 * 3. Update callback signatures to match unified interface
 *
 * ## Example Migration
 *
 * ### Before (Legacy API)
 * @code
 * #include <kcenon/network/interfaces/i_tcp_client.h>
 *
 * void legacy_example(interfaces::i_tcp_client* client) {
 *     client->set_receive_callback([](const std::vector<uint8_t>& data) {
 *         // Handle data
 *     });
 *     client->start("localhost", 8080);
 * }
 * @endcode
 *
 * ### After (Unified API)
 * @code
 * #include <kcenon/network/protocol/protocol.h>
 *
 * void unified_example() {
 *     auto conn = protocol::tcp::connect({"localhost", 8080});
 *     conn->set_callbacks({
 *         .on_data = [](std::span<const std::byte> data) {
 *             // Handle data
 *         }
 *     });
 * }
 * @endcode
 *
 * @see unified::i_connection
 * @see unified::i_listener
 * @see unified::i_transport
 * @see docs/guides/MIGRATION_UNIFIED_API.md for full migration guide
 */

#include "kcenon/network/unified/unified.h"

namespace kcenon::network::compat {

// ============================================================================
// Deprecation Macros
// ============================================================================

/**
 * @def NETWORK_DEPRECATED_INTERFACE
 * @brief Marks an interface alias as deprecated with migration guidance.
 */
#if defined(__cplusplus) && __cplusplus >= 201402L
#    define NETWORK_DEPRECATED_INTERFACE(msg) [[deprecated(msg)]]
#elif defined(__GNUC__) || defined(__clang__)
#    define NETWORK_DEPRECATED_INTERFACE(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#    define NETWORK_DEPRECATED_INTERFACE(msg) __declspec(deprecated(msg))
#else
#    define NETWORK_DEPRECATED_INTERFACE(msg)
#endif

// ============================================================================
// Client Interface Aliases (-> i_connection)
// ============================================================================

/**
 * @brief Deprecated alias for generic client interface
 *
 * @deprecated Use `unified::i_connection` with protocol factory functions instead.
 *
 * Migration:
 * @code
 * // Old: std::unique_ptr<interfaces::i_client> client;
 * // New: std::unique_ptr<unified::i_connection> conn = protocol::tcp::connect(...);
 * @endcode
 */
using i_client_compat NETWORK_DEPRECATED_INTERFACE(
    "Use unified::i_connection with protocol factories instead. "
    "See docs/guides/MIGRATION_UNIFIED_API.md") = unified::i_connection;

/**
 * @brief Deprecated alias for TCP client interface
 *
 * @deprecated Use `protocol::tcp::connect()` to create `i_connection`.
 */
using i_tcp_client_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::tcp::connect() to create unified::i_connection instead") =
    unified::i_connection;

/**
 * @brief Deprecated alias for UDP client interface
 *
 * @deprecated Use `protocol::udp::connect()` to create `i_connection`.
 */
using i_udp_client_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::udp::connect() to create unified::i_connection instead") =
    unified::i_connection;

/**
 * @brief Deprecated alias for WebSocket client interface
 *
 * @deprecated Use `protocol::websocket::connect()` to create `i_connection`.
 */
using i_websocket_client_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::websocket::connect() to create unified::i_connection instead") =
    unified::i_connection;

/**
 * @brief Deprecated alias for QUIC client interface
 *
 * @deprecated Use `protocol::quic::connect()` to create `i_connection`.
 */
using i_quic_client_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::quic::connect() to create unified::i_connection instead") =
    unified::i_connection;

// ============================================================================
// Server Interface Aliases (-> i_listener)
// ============================================================================

/**
 * @brief Deprecated alias for generic server interface
 *
 * @deprecated Use `unified::i_listener` with protocol factory functions instead.
 *
 * Migration:
 * @code
 * // Old: std::unique_ptr<interfaces::i_server> server;
 * // New: std::unique_ptr<unified::i_listener> listener = protocol::tcp::listen(...);
 * @endcode
 */
using i_server_compat NETWORK_DEPRECATED_INTERFACE(
    "Use unified::i_listener with protocol factories instead. "
    "See docs/guides/MIGRATION_UNIFIED_API.md") = unified::i_listener;

/**
 * @brief Deprecated alias for TCP server interface
 *
 * @deprecated Use `protocol::tcp::listen()` to create `i_listener`.
 */
using i_tcp_server_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::tcp::listen() to create unified::i_listener instead") =
    unified::i_listener;

/**
 * @brief Deprecated alias for UDP server interface
 *
 * @deprecated Use `protocol::udp::listen()` to create `i_listener`.
 */
using i_udp_server_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::udp::listen() to create unified::i_listener instead") =
    unified::i_listener;

/**
 * @brief Deprecated alias for WebSocket server interface
 *
 * @deprecated Use `protocol::websocket::listen()` to create `i_listener`.
 */
using i_websocket_server_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::websocket::listen() to create unified::i_listener instead") =
    unified::i_listener;

/**
 * @brief Deprecated alias for QUIC server interface
 *
 * @deprecated Use `protocol::quic::listen()` to create `i_listener`.
 */
using i_quic_server_compat NETWORK_DEPRECATED_INTERFACE(
    "Use protocol::quic::listen() to create unified::i_listener instead") =
    unified::i_listener;

// ============================================================================
// Session Interface Alias (-> i_connection)
// ============================================================================

/**
 * @brief Deprecated alias for session interface (accepted connections)
 *
 * @deprecated Sessions are now represented as `i_connection` instances.
 *
 * In the unified API, accepted connections from a listener are the same
 * type as client-initiated connections. Both implement `i_connection`.
 *
 * Migration:
 * @code
 * // Old: void handle_session(std::shared_ptr<interfaces::i_session> session);
 * // New: void handle_connection(std::unique_ptr<unified::i_connection> conn);
 * @endcode
 */
using i_session_compat NETWORK_DEPRECATED_INTERFACE(
    "Sessions are now unified::i_connection. Accepted connections share "
    "the same interface as client connections") = unified::i_connection;

// ============================================================================
// Non-Deprecated Convenience Type Aliases
// ============================================================================

/**
 * @brief Convenience alias for unified connection interface
 *
 * Use this alias when you need a protocol-agnostic connection type.
 */
using connection = unified::i_connection;

/**
 * @brief Convenience alias for unified listener interface
 *
 * Use this alias when you need a protocol-agnostic listener type.
 */
using listener = unified::i_listener;

/**
 * @brief Convenience alias for unified transport interface
 *
 * Use this alias when you only need data transport operations
 * (send/receive) without connection management.
 */
using transport = unified::i_transport;

// ============================================================================
// Pointer Type Aliases
// ============================================================================

/**
 * @brief Unique pointer type for connection
 */
using connection_ptr = std::unique_ptr<unified::i_connection>;

/**
 * @brief Unique pointer type for listener
 */
using listener_ptr = std::unique_ptr<unified::i_listener>;

/**
 * @brief Unique pointer type for transport
 */
using transport_ptr = std::unique_ptr<unified::i_transport>;

}  // namespace kcenon::network::compat

// ============================================================================
// Namespace Shortcuts for Easy Migration
// ============================================================================

namespace kcenon::network {

/**
 * @brief Import unified types into the main network namespace
 *
 * This allows using `network::i_connection` instead of
 * `network::unified::i_connection`.
 */
using unified::i_connection;
using unified::i_listener;
using unified::i_transport;
using unified::endpoint_info;
using unified::connection_callbacks;
using unified::listener_callbacks;
using unified::connection_options;

}  // namespace kcenon::network
