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

/**
 * @file i_udp_server.h
 * @brief UDP server interface (deprecated - use facade API)
 *
 * @deprecated This header exposes internal implementation details.
 * Use the facade API instead:
 *
 * @code
 * #include <kcenon/network/facade/udp_facade.h>
 *
 * auto server = kcenon::network::facade::udp_facade{}.create_server({
 *     .port = 5555,
 *     .server_id = "my-server"
 * });
 * @endcode
 *
 * This header will be moved to internal in a future release.
 */

#include "i_network_component.h"
#include "i_udp_client.h"

#include <cstdint>
#include <functional>
#include <system_error>
#include <vector>

#include "kcenon/network/types/result.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_udp_server
	 * \brief Interface for UDP server components.
	 *
	 * This interface extends i_network_component with UDP server-specific
	 * operations such as receiving datagrams from multiple clients and
	 * sending responses to specific endpoints.
	 *
	 * ### Key Characteristics
	 * - Connectionless: No client session management required
	 * - Endpoint-aware: Each received datagram includes sender information
	 * - Bidirectional: Can send responses to any endpoint
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_server
	 */
	class i_udp_server : public i_network_component
	{
	public:
		//! \brief Reuse endpoint_info from i_udp_client
		using endpoint_info = i_udp_client::endpoint_info;

		//! \brief Callback type for received data (includes sender endpoint)
		using receive_callback_t = std::function<void(
			const std::vector<uint8_t>&,
			const endpoint_info&)>;

		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		//! \brief Callback type for send completion
		using send_callback_t = std::function<void(std::error_code, std::size_t)>;

		/*!
		 * \brief Starts the UDP server on the specified port.
		 * \param port The port number to bind to.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Creates a UDP socket and binds to the specified port
		 * - Begins listening for incoming datagrams
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if socket creation fails
		 * - Returns error if port binding fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the UDP server.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Thread Safety
		 * Thread-safe. Pending operations are cancelled.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Sends a datagram to the specified endpoint.
		 * \param endpoint The target endpoint.
		 * \param data The data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Thread Safety
		 * Thread-safe. Multiple sends may be queued.
		 */
		[[nodiscard]] virtual auto send_to(
			const endpoint_info& endpoint,
			std::vector<uint8_t>&& data,
			send_callback_t handler = nullptr) -> VoidResult = 0;

		/*!
		 * \brief Sets the callback for received datagrams.
		 * \param callback The callback function.
		 *
		 * ### Note
		 * The callback receives both the data and the sender's endpoint
		 * information, allowing responses to be sent using send_to().
		 */
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
