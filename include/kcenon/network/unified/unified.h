/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
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
