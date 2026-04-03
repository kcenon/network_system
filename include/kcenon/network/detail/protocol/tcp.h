// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file tcp.h
 * @brief TCP protocol connection and listener factory functions.
 *
 * @see tcp_facade.h
 */

#pragma once

/**
 * @file tcp.h
 * @brief TCP protocol factory functions for the unified transport layer
 *
 * Provides create_connection() and create_listener() factory functions
 * for TCP transport using the unified i_connection/i_listener interfaces.
 */

#include "kcenon/network/detail/unified/i_connection.h"
#include "kcenon/network/detail/unified/i_listener.h"
#include "kcenon/network/detail/unified/types.h"

#include <memory>
#include <string>
#include <string_view>

namespace kcenon::network::protocol::tcp {

/**
 * @brief Creates a TCP connection (not yet connected)
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance
 *
 * The returned connection is not connected. Call connect() to establish
 * the connection to a remote endpoint.
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::tcp::create_connection("my-client");
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) { }
 * });
 * conn->connect({"localhost", 8080});
 * @endcode
 */
[[nodiscard]] auto create_connection(std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and connects a TCP connection in one call
 * @param endpoint The remote endpoint to connect to
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (connecting or connected)
 *
 * This is a convenience function that creates a connection and immediately
 * initiates the connection to the specified endpoint.
 *
 * ### Usage Example
 * @code
 * auto conn = protocol::tcp::connect({"localhost", 8080});
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Connected!\n"; }
 * });
 * // Connection is already in progress
 * @endcode
 */
[[nodiscard]] auto connect(const unified::endpoint_info& endpoint,
                           std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and connects a TCP connection using URL format
 * @param url The URL to connect to (format: "tcp://host:port" or "host:port")
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (connecting or connected)
 */
[[nodiscard]] auto connect(std::string_view url, std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates a TCP listener (not yet listening)
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance
 *
 * The returned listener is not listening. Call start() to begin
 * accepting connections.
 *
 * ### Usage Example
 * @code
 * auto listener = protocol::tcp::create_listener("my-server");
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New connection: " << conn_id << "\n";
 *     }
 * });
 * listener->start(8080);
 * @endcode
 */
[[nodiscard]] auto create_listener(std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a TCP listener in one call
 * @param bind_address The local address to bind to
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * This is a convenience function that creates a listener and immediately
 * starts listening on the specified address.
 *
 * ### Usage Example
 * @code
 * auto listener = protocol::tcp::listen({"0.0.0.0", 8080});
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) { }
 * });
 * // Listener is already accepting connections
 * @endcode
 */
[[nodiscard]] auto listen(const unified::endpoint_info& bind_address,
                          std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a TCP listener on a specific port
 * @param port The port number to listen on
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * Convenience overload that binds to all interfaces (0.0.0.0).
 */
[[nodiscard]] auto listen(uint16_t port, std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

}  // namespace kcenon::network::protocol::tcp
