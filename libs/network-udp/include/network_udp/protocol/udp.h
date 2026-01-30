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

namespace kcenon::network::protocol::udp {

/**
 * @brief Creates a UDP connection (not yet started)
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance
 *
 * The returned connection is not started. Call connect() to set the
 * target endpoint and start the UDP client.
 *
 * ### UDP Semantics
 * Unlike TCP, UDP is connectionless. The "connection" object manages
 * a UDP socket that sends datagrams to a configured target endpoint.
 * - connect() sets the target and starts the client
 * - is_connected() returns true while the client is running
 * - send() sends datagrams to the target endpoint
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::udp::create_connection("my-udp-client");
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Started!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received datagram
 *     }
 * });
 * conn->connect({"localhost", 5555});
 * @endcode
 */
[[nodiscard]] auto create_connection(std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and starts a UDP connection in one call
 * @param endpoint The target endpoint to send datagrams to
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (running)
 *
 * This is a convenience function that creates a UDP connection and
 * immediately starts it with the specified target endpoint.
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::udp::connect({"localhost", 5555});
 * conn->set_callbacks({
 *     .on_data = [](std::span<const std::byte> data) { }
 * });
 * // UDP client is already running
 * @endcode
 */
[[nodiscard]] auto connect(const unified::endpoint_info& endpoint,
                           std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and starts a UDP connection using URL format
 * @param url The URL to connect to (format: "udp://host:port" or "host:port")
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (running)
 */
[[nodiscard]] auto connect(std::string_view url, std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates a UDP listener (not yet listening)
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance
 *
 * The returned listener is not listening. Call start() to begin
 * receiving datagrams.
 *
 * ### UDP Semantics
 * Unlike TCP, UDP servers don't accept connections. Instead, they
 * receive datagrams from any sender. The listener tracks unique
 * sender endpoints as virtual "connections" for convenience.
 * - on_accept is called when a new sender endpoint is seen
 * - connection_id is formatted as "address:port"
 * - send_to() sends datagrams back to specific endpoints
 *
 * ### Usage Example
 * @code
 * auto listener = protocol::udp::create_listener("my-udp-server");
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New endpoint: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Handle received datagram from conn_id
 *     }
 * });
 * listener->start(5555);
 * @endcode
 */
[[nodiscard]] auto create_listener(std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a UDP listener in one call
 * @param bind_address The local address to bind to
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * This is a convenience function that creates a listener and immediately
 * starts listening on the specified address.
 *
 * ### Usage Example
 * @code
 * auto listener = protocol::udp::listen({"0.0.0.0", 5555});
 * listener->set_callbacks({
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) { }
 * });
 * // Listener is already receiving datagrams
 * @endcode
 */
[[nodiscard]] auto listen(const unified::endpoint_info& bind_address,
                          std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a UDP listener on a specific port
 * @param port The port number to listen on
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * Convenience overload that binds to all interfaces (0.0.0.0).
 */
[[nodiscard]] auto listen(uint16_t port, std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

}  // namespace kcenon::network::protocol::udp
