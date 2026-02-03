/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"

// Forward declarations
namespace kcenon::network::core
{
class connection_pool;
} // namespace kcenon::network::core

namespace kcenon::network::facade
{

/*!
 * \class tcp_facade
 * \brief Simplified facade for creating TCP clients and servers.
 *
 * This facade provides a simple, unified API for creating TCP protocol
 * clients and servers, hiding the complexity of template parameters,
 * protocol tags, and TLS policies from the user.
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
 * \code
 * using namespace kcenon::network::facade;
 *
 * // Create plain TCP client
 * tcp_facade facade;
 * auto client = facade.create_client({
 *     .host = "127.0.0.1",
 *     .port = 8080,
 *     .client_id = "my-client"
 * });
 *
 * // Create secure TCP client
 * auto secure_client = facade.create_client({
 *     .host = "example.com",
 *     .port = 8443,
 *     .use_ssl = true,
 *     .client_id = "secure-client"
 * });
 *
 * // Create TCP server
 * auto server = facade.create_server({
 *     .port = 8080
 * });
 *
 * // Create secure TCP server
 * auto secure_server = facade.create_server({
 *     .port = 8443,
 *     .use_ssl = true,
 *     .cert_path = "/path/to/cert.pem",
 *     .key_path = "/path/to/key.pem"
 * });
 * \endcode
 *
 * \see interfaces::i_protocol_client
 * \see interfaces::i_protocol_server
 */
class tcp_facade
{
public:
	/*!
	 * \struct client_config
	 * \brief Configuration for creating a TCP client.
	 */
	struct client_config
	{
		//! Server hostname or IP address
		std::string host;

		//! Server port number
		uint16_t port = 0;

		//! Client identifier (optional, auto-generated if not provided)
		std::string client_id;

		//! Connection timeout
		std::chrono::milliseconds timeout = std::chrono::seconds(30);

		//! Whether to use SSL/TLS
		bool use_ssl = false;

		//! Path to CA certificate file (for SSL verification)
		std::optional<std::string> ca_cert_path;

		//! Whether to verify SSL certificate (default: true)
		bool verify_certificate = true;
	};

	/*!
	 * \struct server_config
	 * \brief Configuration for creating a TCP server.
	 */
	struct server_config
	{
		//! Port number to listen on
		uint16_t port = 0;

		//! Server identifier (optional, auto-generated if not provided)
		std::string server_id;

		//! Whether to use SSL/TLS
		bool use_ssl = false;

		//! Path to server certificate file (required if use_ssl=true)
		std::optional<std::string> cert_path;

		//! Path to server private key file (required if use_ssl=true)
		std::optional<std::string> key_path;

		//! SSL/TLS protocol version (default: TLS 1.2+)
		std::optional<std::string> tls_version;
	};

	/*!
	 * \brief Creates a TCP client with the specified configuration.
	 * \param config Client configuration.
	 * \return Shared pointer to IProtocolClient interface.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates appropriate client type based on use_ssl flag
	 * - Plain TCP client if use_ssl=false
	 * - Secure TCP client if use_ssl=true
	 * - Client ID is auto-generated if not provided
	 *
	 * ### Error Conditions
	 * - Throws if host is empty
	 * - Throws if port is 0 or > 65535
	 * - Throws if use_ssl=true but SSL support not compiled in
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> std::shared_ptr<interfaces::i_protocol_client>;

	/*!
	 * \brief Creates a TCP server with the specified configuration.
	 * \param config Server configuration.
	 * \return Shared pointer to IProtocolServer interface.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates appropriate server type based on use_ssl flag
	 * - Plain TCP server if use_ssl=false
	 * - Secure TCP server if use_ssl=true
	 * - Server ID is auto-generated if not provided
	 *
	 * ### Error Conditions
	 * - Throws if port is 0 or > 65535
	 * - Throws if use_ssl=true but cert_path or key_path not provided
	 * - Throws if use_ssl=true but SSL support not compiled in
	 */
	[[nodiscard]] auto create_server(const server_config& config) const
		-> std::shared_ptr<interfaces::i_protocol_server>;

	/*!
	 * \struct pool_config
	 * \brief Configuration for creating a TCP connection pool.
	 */
	struct pool_config
	{
		//! Server hostname or IP address
		std::string host;

		//! Server port number
		uint16_t port = 0;

		//! Number of connections to maintain in the pool
		size_t pool_size = 10;
	};

	/*!
	 * \brief Creates a TCP connection pool with the specified configuration.
	 * \param config Pool configuration.
	 * \return Shared pointer to connection_pool.
	 *
	 * ### Behavior
	 * - Creates a connection pool that manages multiple reusable connections
	 * - Pool is not initialized; call initialize() before use
	 * - Thread-safe for concurrent acquire/release operations
	 *
	 * ### Usage Example
	 * \code
	 * tcp_facade facade;
	 * auto pool = facade.create_connection_pool({
	 *     .host = "127.0.0.1",
	 *     .port = 5555,
	 *     .pool_size = 10
	 * });
	 * auto result = pool->initialize();
	 * if (result.is_ok()) {
	 *     auto client = pool->acquire();
	 *     client->send_packet(data);
	 *     pool->release(std::move(client));
	 * }
	 * \endcode
	 */
	[[nodiscard]] auto create_connection_pool(const pool_config& config) const
		-> std::shared_ptr<core::connection_pool>;

private:
	//! \brief Generates a unique client ID
	[[nodiscard]] static auto generate_client_id() -> std::string;

	//! \brief Generates a unique server ID
	[[nodiscard]] static auto generate_server_id() -> std::string;

	//! \brief Validates client configuration
	static auto validate_client_config(const client_config& config) -> void;

	//! \brief Validates server configuration
	static auto validate_server_config(const server_config& config) -> void;
};

} // namespace kcenon::network::facade
