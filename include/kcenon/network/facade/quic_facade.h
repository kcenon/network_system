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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::network::core
{
	class messaging_quic_client;
	class messaging_quic_server;
} // namespace kcenon::network::core

namespace kcenon::network::facade
{

/*!
 * \class quic_facade
 * \brief Simplified facade for creating QUIC clients and servers.
 *
 * This facade provides a simple, unified API for creating QUIC protocol
 * clients and servers, hiding the complexity of experimental API opt-in,
 * configuration details, and implementation specifics from the user.
 *
 * ### Design Goals
 * - **Simplicity**: No template parameters or protocol tags
 * - **Consistency**: Same API pattern across all protocol facades
 * - **Type Safety**: Returns standard QUIC client/server types
 * - **Zero Cost**: No performance overhead compared to direct instantiation
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called concurrently.
 *
 * ### Usage Example
 * \code
 * using namespace kcenon::network::facade;
 *
 * // Create QUIC client
 * quic_facade facade;
 * auto client = facade.create_client({
 *     .host = "127.0.0.1",
 *     .port = 4433,
 *     .client_id = "my-quic-client",
 *     .alpn = "h3"
 * });
 *
 * // Create QUIC server
 * auto server = facade.create_server({
 *     .port = 4433,
 *     .server_id = "my-quic-server",
 *     .cert_path = "/path/to/cert.pem",
 *     .key_path = "/path/to/key.pem"
 * });
 * \endcode
 *
 * \see core::messaging_quic_client
 * \see core::messaging_quic_server
 */
class quic_facade
{
public:
	/*!
	 * \struct client_config
	 * \brief Configuration for creating a QUIC client.
	 */
	struct client_config
	{
		//! Server hostname or IP address
		std::string host;

		//! Server port number
		uint16_t port = 0;

		//! Client identifier (optional, auto-generated if not provided)
		std::string client_id;

		//! Path to CA certificate file for server verification (PEM format)
		std::optional<std::string> ca_cert_path;

		//! Path to client certificate file for mutual TLS (PEM format)
		std::optional<std::string> client_cert_path;

		//! Path to client private key file for mutual TLS (PEM format)
		std::optional<std::string> client_key_path;

		//! Whether to verify server certificate (default: true)
		bool verify_server = true;

		//! ALPN protocol identifier (e.g., "h3", "hq-29")
		std::string alpn;

		//! Maximum idle timeout in milliseconds (default: 30 seconds)
		uint64_t max_idle_timeout_ms = 30000;

		//! Enable 0-RTT early data (default: false)
		bool enable_0rtt = false;
	};

	/*!
	 * \struct server_config
	 * \brief Configuration for creating a QUIC server.
	 */
	struct server_config
	{
		//! Port number to listen on
		uint16_t port = 0;

		//! Server identifier (optional, auto-generated if not provided)
		std::string server_id;

		//! Path to server certificate file (PEM format, required)
		std::string cert_path;

		//! Path to server private key file (PEM format, required)
		std::string key_path;

		//! Path to CA certificate file for client verification (optional)
		std::optional<std::string> ca_cert_path;

		//! Whether to require client certificate (mutual TLS)
		bool require_client_cert = false;

		//! ALPN protocol identifier (e.g., "h3", "hq-29")
		std::string alpn;

		//! Maximum idle timeout in milliseconds (default: 30 seconds)
		uint64_t max_idle_timeout_ms = 30000;

		//! Maximum number of concurrent connections (default: 10000)
		size_t max_connections = 10000;
	};

	/*!
	 * \brief Creates a QUIC client with the specified configuration.
	 * \param config Client configuration.
	 * \return Shared pointer to messaging_quic_client.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates a messaging_quic_client instance
	 * - Client ID is auto-generated if not provided
	 * - QUIC always uses TLS 1.3 encryption
	 * - Client must be started manually using start_client() method
	 *
	 * ### Error Conditions
	 * - Throws if host is empty
	 * - Throws if port is 0 or > 65535
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> std::shared_ptr<core::messaging_quic_client>;

	/*!
	 * \brief Creates a QUIC server with the specified configuration.
	 * \param config Server configuration.
	 * \return Shared pointer to messaging_quic_server.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates a messaging_quic_server instance
	 * - Server ID is auto-generated if not provided
	 * - QUIC always uses TLS 1.3 encryption
	 * - Server must be started manually using start_server() method
	 *
	 * ### Error Conditions
	 * - Throws if port is 0 or > 65535
	 * - Throws if cert_path is empty
	 * - Throws if key_path is empty
	 */
	[[nodiscard]] auto create_server(const server_config& config) const
		-> std::shared_ptr<core::messaging_quic_server>;

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
