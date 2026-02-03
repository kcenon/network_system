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
#include <string>

#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"

namespace kcenon::network::facade
{

/*!
 * \class udp_facade
 * \brief Simplified facade for creating UDP clients and servers.
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
 * \code
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
 * \endcode
 *
 * \see interfaces::i_protocol_client
 * \see interfaces::i_protocol_server
 */
class udp_facade
{
public:
	/*!
	 * \struct client_config
	 * \brief Configuration for creating a UDP client.
	 */
	struct client_config
	{
		//! Target hostname or IP address
		std::string host;

		//! Target port number
		uint16_t port = 0;

		//! Client identifier (optional, auto-generated if not provided)
		std::string client_id;
	};

	/*!
	 * \struct server_config
	 * \brief Configuration for creating a UDP server.
	 */
	struct server_config
	{
		//! Port number to listen on
		uint16_t port = 0;

		//! Server identifier (optional, auto-generated if not provided)
		std::string server_id;
	};

	/*!
	 * \brief Creates a UDP client with the specified configuration.
	 * \param config Client configuration.
	 * \return Shared pointer to IProtocolClient interface.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates a UDP client adapter wrapping messaging_udp_client
	 * - Client ID is auto-generated if not provided
	 * - Automatically connects to the specified target endpoint
	 *
	 * ### Error Conditions
	 * - Throws if host is empty
	 * - Throws if port is 0 or > 65535
	 */
	[[nodiscard]] auto create_client(const client_config& config) const
		-> std::shared_ptr<interfaces::i_protocol_client>;

	/*!
	 * \brief Creates a UDP server with the specified configuration.
	 * \param config Server configuration.
	 * \return Shared pointer to IProtocolServer interface.
	 * \throws std::invalid_argument if configuration is invalid.
	 *
	 * ### Behavior
	 * - Creates a UDP server adapter wrapping messaging_udp_server
	 * - Server ID is auto-generated if not provided
	 * - Automatically starts listening on the specified port
	 *
	 * ### Error Conditions
	 * - Throws if port is 0 or > 65535
	 */
	[[nodiscard]] auto create_server(const server_config& config) const
		-> std::shared_ptr<interfaces::i_protocol_server>;

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
