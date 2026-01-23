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

#include "kcenon/network/unified/i_connection.h"
#include "kcenon/network/unified/i_listener.h"
#include "kcenon/network/unified/types.h"

#include <memory>
#include <string>
#include <string_view>

namespace kcenon::network::protocol::websocket {

/**
 * @brief Creates a WebSocket connection (not yet started)
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance
 *
 * The returned connection is not started. Call connect() with a URL
 * to establish the WebSocket connection.
 *
 * ### WebSocket Semantics
 * WebSocket is a full-duplex protocol over TCP. The connection
 * starts with an HTTP upgrade handshake.
 * - connect() accepts URLs like "ws://host:port/path" or "wss://host:port/path"
 * - is_connected() returns true after successful handshake
 * - send() sends binary frames (use underlying client for text)
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::websocket::create_connection("my-ws-client");
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "WebSocket connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received WebSocket message
 *     }
 * });
 * conn->connect("ws://localhost:8080/ws");
 * @endcode
 */
[[nodiscard]] auto create_connection(std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and starts a WebSocket connection in one call
 * @param url The WebSocket URL to connect to (ws:// or wss://)
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (connecting)
 *
 * This is a convenience function that creates a WebSocket connection
 * and immediately initiates the connection with the specified URL.
 *
 * ### URL Format
 * - `ws://host:port/path` - Plain WebSocket
 * - `wss://host:port/path` - Secure WebSocket (TLS)
 * - Port defaults to 80 for ws:// and 443 for wss:// if not specified
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::websocket::connect("ws://localhost:8080/ws");
 * conn->set_callbacks({
 *     .on_data = [](std::span<const std::byte> data) { }
 * });
 * // Connection is initiating...
 * @endcode
 */
[[nodiscard]] auto connect(std::string_view url, std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and starts a WebSocket connection using endpoint info
 * @param endpoint The endpoint with host and port
 * @param path The WebSocket path (default: "/")
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (connecting)
 *
 * This overload is useful when host and port are known separately.
 * The connection will use plain WebSocket (ws://).
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::websocket::connect({"localhost", 8080}, "/ws");
 * @endcode
 */
[[nodiscard]] auto connect(const unified::endpoint_info& endpoint,
                           std::string_view path = "/",
                           std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates a WebSocket listener (not yet listening)
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance
 *
 * The returned listener is not listening. Call start() to begin
 * accepting WebSocket connections.
 *
 * ### WebSocket Server Semantics
 * WebSocket servers accept HTTP upgrade requests and establish
 * full-duplex connections with clients.
 * - on_accept is called when a WebSocket handshake completes
 * - connection_id uniquely identifies each connected client
 * - send_to() sends data to specific clients
 * - broadcast() sends data to all connected clients
 *
 * ### Usage Example
 * @code
 * auto listener = protocol::websocket::create_listener("my-ws-server");
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New WebSocket client: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Handle received message from conn_id
 *     }
 * });
 * listener->start(8080);
 * @endcode
 */
[[nodiscard]] auto create_listener(std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a WebSocket listener in one call
 * @param bind_address The local address to bind to
 * @param path The WebSocket path to handle (default: "/")
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * This is a convenience function that creates a listener and immediately
 * starts listening on the specified address.
 *
 * ### Usage Example
 * @code
 * auto listener = protocol::websocket::listen({"0.0.0.0", 8080}, "/ws");
 * listener->set_callbacks({
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) { }
 * });
 * // Listener is already accepting connections
 * @endcode
 */
[[nodiscard]] auto listen(const unified::endpoint_info& bind_address,
                          std::string_view path = "/",
                          std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a WebSocket listener on a specific port
 * @param port The port number to listen on
 * @param path The WebSocket path to handle (default: "/")
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * Convenience overload that binds to all interfaces (0.0.0.0).
 */
[[nodiscard]] auto listen(uint16_t port,
                          std::string_view path = "/",
                          std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

}  // namespace kcenon::network::protocol::websocket
