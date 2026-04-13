// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file udp_facade.h
 * @brief Simplified facade for creating UDP clients and servers.
 *
 */

#pragma once

/**
 * @file udp_facade.h
 * @brief Simplified facade for creating UDP clients and servers
 *
 * Provides a template-free API for UDP client/server creation,
 * hiding protocol tag and implementation details from the user.
 */

#include <cstdint>
#include <memory>
#include <string>

#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"
#include "kcenon/network/types/result.h"

namespace kcenon::network::facade
{

/**
 * @class udp_facade
 * @brief Simplified facade for creating UDP clients and servers.
 *
 * This facade provides a simple, unified API for creating UDP protocol
 * clients and servers, hiding the complexity of template parameters,
 * protocol tags, and implementation details from the user.
 *
 * ### Design Goals
 * - **Simplicity**: No template parameters or protocol tags
 * - **Consistency**: Same API pattern across all protocol facades
 * - **Type Safety**: Returns standard IProtocolClient/IProtocolServer interfaces
 * - **Zero Cost**: No performance overhead compared to direct instantiation
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called concurrently.
 *
 * ### Usage Example
 * @code
 * using namespace kcenon::network::facade;
 *
 * // Create UDP client
 * udp_facade facade;
 * auto client = facade.create_client({
 *     .host = "127.0.0.1",
 *     .port = 5555,
 *     .client_id = "my-udp-client"
 * });
 *
 * // Create UDP server
 * auto server = facade.create_server({
 *     .port = 5555,
 *     .server_id = "my-udp-server"
 * });
 * @endcode
 *
 * @see interfaces::i_protocol_client
 * @see interfaces::i_protocol_server
 */
class udp_facade
{
public:
	/**
	 * @struct client_config
	 * @brief Configuration for creating a UDP client.
	 */
	struct client_config
	{
		/// Target hostname or IP address
		std::string host;

		/// Target port number
		uint16_t port = 0;

		/// Client identifier (optional, auto-generated if not provided)
		std::string client_id;
	};

	/**
	 * @struct server_config
	 * @brief Configuration for creating a UDP server.
	 */
	struct server_config
	{
		/// Port number to listen on
		uint16_t port = 0;

		/// Server identifier (optional, auto-generated if not provided)
		std::string server_id;
	};

	/**
	 * @brief Creates a UDP client with the specified configuration.
	 * @param config Client configuration.
	 * @return Result containing shared pointer to IProtocolClient, or error.
	 *
	 * ### Behavior
	 * - Creates a UDP client adapter wrapping messaging_udp_client
	 * - Client ID is auto-generated if not provided
	 * - Automatically connects to the specified target endpoint
	 *
	 * ### Error Conditions
	 * - Returns error if host is empty
	 * - Returns error if port is 0 or > 65535
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> Result<std::shared_ptr<interfaces::i_protocol_client>>;

	/**
	 * @brief Creates a UDP server with the specified configuration.
	 * @param config Server configuration.
	 * @return Result containing shared pointer to IProtocolServer, or error.
	 *
	 * ### Behavior
	 * - Creates a UDP server adapter wrapping messaging_udp_server
	 * - Server ID is auto-generated if not provided
	 * - Automatically starts listening on the specified port
	 *
	 * ### Error Conditions
	 * - Returns error if port is 0 or > 65535
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
