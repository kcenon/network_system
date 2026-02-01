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
#include <string>

namespace kcenon::network::core
{
	class http_client;
	class http_server;
} // namespace kcenon::network::core

namespace kcenon::network::facade
{

/*!
 * \class http_facade
 * \brief Simplified facade for creating HTTP clients and servers.
 *
 * This facade provides a simple, unified API for creating HTTP/1.1 protocol
 * clients and servers, hiding the complexity of underlying implementation
 * from the user.
 *
 * ### Design Goals
 * - **Simplicity**: No template parameters or protocol tags
 * - **Consistency**: Same API pattern across all protocol facades
 * - **Type Safety**: Returns standard HTTP client/server types
 * - **Zero Cost**: No performance overhead compared to direct instantiation
 *
 * ### Thread Safety
 * All methods are thread-safe and can be called concurrently.
 *
 * ### Usage Example
 * \code
 * using namespace kcenon::network::facade;
 *
 * // Create HTTP client
 * http_facade facade;
 * auto client = facade.create_client({
 *     .timeout = std::chrono::seconds(10)
 * });
 *
 * // Create HTTP server
 * auto server = facade.create_server({
 *     .port = 8080,
 *     .server_id = "my-http-server"
 * });
 * \endcode
 *
 * \see core::http_client
 * \see core::http_server
 */
class http_facade
{
public:
	/*!
	 * \struct client_config
	 * \brief Configuration for creating an HTTP client.
	 */
	struct client_config
	{
		//! Request timeout
		std::chrono::milliseconds timeout = std::chrono::seconds(30);
	};

	/*!
	 * \struct server_config
	 * \brief Configuration for creating an HTTP server.
	 */
	struct server_config
	{
		//! Port number to listen on
		uint16_t port = 0;

		//! Server identifier (optional, auto-generated if not provided)
		std::string server_id;
	};

	/*!
	 * \brief Creates an HTTP client with the specified configuration.
	 * \param config Client configuration.
	 * \return Shared pointer to http_client.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates an http_client instance with the specified timeout
	 * - Default timeout is 30 seconds if not specified
	 *
	 * ### Error Conditions
	 * - Throws if timeout is zero or negative
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> std::shared_ptr<core::http_client>;

	/*!
	 * \brief Creates an HTTP server with the specified configuration.
	 * \param config Server configuration.
	 * \return Shared pointer to http_server.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates an http_server instance
	 * - Server ID is auto-generated if not provided
	 * - Server must be started manually using start() method
	 *
	 * ### Error Conditions
	 * - Throws if port is 0 or > 65535
	 */
	[[nodiscard]] auto create_server(const server_config& config) const
		-> std::shared_ptr<core::http_server>;

private:
	//! \brief Generates a unique server ID
	[[nodiscard]] static auto generate_server_id() -> std::string;

	//! \brief Validates client configuration
	static auto validate_client_config(const client_config& config) -> void;

	//! \brief Validates server configuration
	static auto validate_server_config(const server_config& config) -> void;
};

} // namespace kcenon::network::facade
