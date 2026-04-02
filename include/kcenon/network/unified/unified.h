// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file unified.h
 * @brief Convenience header for the unified network interface API
 *
 * This header provides a single include point for the consolidated
 * network interface architecture (3 core interfaces).
 *
 * ### The Three Core Interfaces
 *
 * 1. **i_transport** - Data transport abstraction
 *    - send(), is_connected(), remote_endpoint()
 *    - Base interface for all data transfer operations
 *
 * 2. **i_connection** - Active connection (client-side or accepted)
 *    - Extends i_transport with connect(), close(), set_callbacks()
 *    - Used by both client-initiated and server-accepted connections
 *
 * 3. **i_listener** - Passive listener (server-side)
 *    - start(), stop(), set_accept_callback()
 *    - Accepts incoming connections and manages them
 *
 * ### Supporting Types
 *
 * - **endpoint_info** - Network endpoint (host:port or URL)
 * - **connection_callbacks** - Callback functions for connections
 * - **listener_callbacks** - Callback functions for listeners
 * - **connection_options** - Configuration options
 *
 * ### Usage Example
 * @code
 * #include <kcenon/network/unified/unified.h>
 *
 * using namespace kcenon::network::unified;
 *
 * // Client-side
 * void client_example(std::unique_ptr<i_connection> conn) {
 *     conn->set_callbacks({
 *         .on_connected = []() { },
 *         .on_data = [](std::span<const std::byte> data) { }
 *     });
 *     conn->connect(endpoint_info("localhost", 8080));
 *     conn->send(some_data);
 * }
 *
 * // Server-side
 * void server_example(std::unique_ptr<i_listener> listener) {
 *     listener->set_callbacks({
 *         .on_accept = [](std::string_view id) { },
 *         .on_data = [](std::string_view id, std::span<const std::byte> data) { }
 *     });
 *     listener->start(8080);
 * }
 * @endcode
 *
 * ### Migration from Legacy Interfaces
 *
 * | Legacy Interface | Unified Interface |
 * |------------------|-------------------|
 * | i_client | i_connection |
 * | i_tcp_client | i_connection (via protocol::tcp) |
 * | i_udp_client | i_connection (via protocol::udp) |
 * | i_websocket_client | i_connection (via protocol::websocket) |
 * | i_quic_client | i_connection (via protocol::quic) |
 * | i_server | i_listener |
 * | i_session | i_connection (accepted) |
 *
 * @see i_transport
 * @see i_connection
 * @see i_listener
 */

// Core types (from detail directory)
#include "kcenon/network/detail/unified/types.h"

// Core interfaces (from detail directory)
#include "kcenon/network/detail/unified/i_transport.h"
#include "kcenon/network/detail/unified/i_connection.h"
#include "kcenon/network/detail/unified/i_listener.h"
