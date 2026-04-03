// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file http_facade.h
 * @brief Simplified facade for creating HTTP clients and servers.
 *
 * @see tcp_facade.h
 */

#pragma once

/**
 * @file http_facade.h
 * @brief Simplified facade for creating HTTP clients and servers
 *
 * Provides a template-free API for HTTP/1.1 client/server creation,
 * hiding protocol implementation details behind configuration structs.
 */

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace kcenon::network::interfaces
{
	class i_protocol_client;
	class i_protocol_server;
} // namespace kcenon::network::interfaces

namespace kcenon::network::facade
{

/**
 * @class http_facade
 * @brief Simplified facade for creating HTTP clients and servers.
 *
 * This facade provides a simple, unified API for creating HTTP/1.1 protocol
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
 * // Create HTTP client
 * http_facade facade;
 * auto client = facade.create_client({
 *     .client_id = "my-http-client",
 *     .timeout = std::chrono::seconds(10)
 * });
 *
 * // Create HTTP server
 * auto server = facade.create_server({
 *     .port = 8080,
 *     .server_id = "my-http-server"
 * });
 * @endcode
 *
 * @see interfaces::i_protocol_client
 * @see interfaces::i_protocol_server
 */
class http_facade
{
public:
	/**
	 * @struct client_config
	 * @brief Configuration for creating an HTTP client.
	 */
	struct client_config
	{
		/// Client identifier (auto-generated if not provided)
		std::string client_id;

		/// Request timeout
		std::chrono::milliseconds timeout = std::chrono::seconds(30);

		/// Whether to use HTTPS
		bool use_ssl = false;

		/// HTTP path (default: "/")
		std::string path = "/";
	};

	/**
	 * @struct server_config
	 * @brief Configuration for creating an HTTP server.
	 */
	struct server_config
	{
		/// Port number to listen on
		uint16_t port = 0;

		/// Server identifier (optional, auto-generated if not provided)
		std::string server_id;
	};

	/**
	 * @brief Creates an HTTP client with the specified configuration.
	 * @param config Client configuration.
	 * @return Shared pointer to i_protocol_client interface.
	 * @throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates an HTTP client adapter wrapping http_client
	 * - Client ID is auto-generated if not provided
	 * - Default timeout is 30 seconds if not specified
	 * - Returns unified i_protocol_client interface for protocol-agnostic usage
	 *
	 * ### Protocol-Specific Notes
	 * - start() sets the base URL (host:port) for subsequent requests
	 * - send() performs an HTTP POST request with binary data
	 * - Received response body is delivered via receive callback
	 *
	 * ### Error Conditions
	 * - Throws if timeout is zero or negative
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> std::shared_ptr<interfaces::i_protocol_client>;

	/**
	 * @brief Creates an HTTP server with the specified configuration.
	 * @param config Server configuration.
	 * @return Shared pointer to i_protocol_server interface.
	 * @throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates an HTTP server adapter wrapping http_server
	 * - Server ID is auto-generated if not provided
	 * - Server must be started manually using start() method
	 * - Returns unified i_protocol_server interface for protocol-agnostic usage
	 *
	 * ### Protocol-Specific Notes
	 * - Receive callback delivers HTTP request body as binary data
	 * - Session send() queues response data for the current request
	 *
	 * ### Error Conditions
	 * - Throws if port is 0 or > 65535
	 */
	[[nodiscard]] auto create_server(const server_config& config) const
		-> std::shared_ptr<interfaces::i_protocol_server>;

private:
	/// @brief Generates a unique client ID
	[[nodiscard]] static auto generate_client_id() -> std::string;

	/// @brief Generates a unique server ID
	[[nodiscard]] static auto generate_server_id() -> std::string;

	/// @brief Validates client configuration
	static auto validate_client_config(const client_config& config) -> void;

	/// @brief Validates server configuration
	static auto validate_server_config(const server_config& config) -> void;
};

} // namespace kcenon::network::facade
