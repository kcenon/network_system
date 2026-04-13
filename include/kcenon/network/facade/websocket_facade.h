// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file websocket_facade.h
 * @brief Simplified facade for creating WebSocket clients and servers.
 *
 */

#pragma once

/**
 * @file websocket_facade.h
 * @brief Simplified facade for creating WebSocket clients and servers
 *
 * Provides a template-free API for WebSocket client/server creation
 * with optional TLS support via configuration structs.
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "kcenon/network/types/result.h"

namespace kcenon::network::interfaces
{
	class i_protocol_client;
	class i_protocol_server;
} // namespace kcenon::network::interfaces

namespace kcenon::network::facade
{

/**
 * @class websocket_facade
 * @brief Simplified facade for creating WebSocket clients and servers.
 *
 * This facade provides a simple, unified API for creating WebSocket protocol
 * clients and servers, hiding the complexity of underlying implementation
 * from the user.
 *
 * ### Design Goals
 * - **Simplicity**: No template parameters or protocol tags
 * - **Consistency**: Same API pattern across all protocol facades
 * - **Type Safety**: Returns unified protocol interfaces (i_protocol_client/i_protocol_server)
 * - **Zero Cost**: No performance overhead compared to direct instantiation
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called concurrently.
 *
 * ### Usage Example
 * @code
 * using namespace kcenon::network::facade;
 *
 * // Create WebSocket client
 * websocket_facade facade;
 * auto client = facade.create_client({
 *     .client_id = "my-ws-client",
 *     .ping_interval = std::chrono::seconds(30)
 * });
 *
 * // Create WebSocket server
 * auto server = facade.create_server({
 *     .port = 8080,
 *     .path = "/ws",
 *     .server_id = "my-ws-server"
 * });
 * @endcode
 *
 * @see interfaces::i_protocol_client
 * @see interfaces::i_protocol_server
 */
class websocket_facade
{
public:
	/**
	 * @struct client_config
	 * @brief Configuration for creating a WebSocket client.
	 */
	struct client_config
	{
		/// Client identifier (auto-generated if not provided)
		std::string client_id;

		/// Ping interval (default: 30 seconds)
		std::chrono::milliseconds ping_interval = std::chrono::seconds(30);
	};

	/**
	 * @struct server_config
	 * @brief Configuration for creating a WebSocket server.
	 */
	struct server_config
	{
		/// Port number to listen on
		uint16_t port = 0;

		/// WebSocket path (default: "/")
		std::string path = "/";

		/// Server identifier (auto-generated if not provided)
		std::string server_id;
	};

	/**
	 * @brief Creates a WebSocket client with the specified configuration.
	 * @param config Client configuration.
	 * @return Result containing shared pointer to i_protocol_client, or error.
	 *
	 * ### Behavior
	 * - Creates a WebSocket client adapter wrapping messaging_ws_client
	 * - Client ID is auto-generated if not provided
	 * - Default ping interval is 30 seconds
	 * - Returns unified i_protocol_client interface for protocol-agnostic usage
	 *
	 * ### Protocol-Specific Notes
	 * - send() sends data as binary WebSocket frames
	 * - For text messages or WebSocket-specific features, cast to i_websocket_client
	 *
	 * ### Error Conditions
	 * - Returns error if ping_interval is zero or negative
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> Result<std::shared_ptr<interfaces::i_protocol_client>>;

	/**
	 * @brief Creates a WebSocket server with the specified configuration.
	 * @param config Server configuration.
	 * @return Result containing shared pointer to i_protocol_server, or error.
	 *
	 * ### Behavior
	 * - Creates a WebSocket server adapter wrapping messaging_ws_server
	 * - Server ID is auto-generated if not provided
	 * - Server must be started manually using start() method
	 * - Returns unified i_protocol_server interface for protocol-agnostic usage
	 *
	 * ### Protocol-Specific Notes
	 * - Receive callback delivers binary data via the unified interface
	 * - For text messages or WebSocket-specific features, cast to i_websocket_server
	 *
	 * ### Error Conditions
	 * - Returns error if port is 0 or > 65535
	 * - Returns error if path is empty or does not start with '/'
	 */
	[[nodiscard]] auto create_server(const server_config& config) const
		-> Result<std::shared_ptr<interfaces::i_protocol_server>>;

private:
	/// @brief Generates a unique client ID
	[[nodiscard]] static auto generate_client_id() -> std::string;

	/// @brief Generates a unique server ID
	[[nodiscard]] static auto generate_server_id() -> std::string;

	/// @brief Validates client configuration
	static auto validate_client_config(const client_config& config) -> VoidResult;

	/// @brief Validates server configuration
	static auto validate_server_config(const server_config& config) -> VoidResult;
};

} // namespace kcenon::network::facade
