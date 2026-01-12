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

#include "i_network_component.h"

#include <cstdint>
#include <functional>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_udp_client
	 * \brief Interface for UDP client components.
	 *
	 * This interface extends i_network_component with UDP-specific
	 * operations such as connectionless datagram transmission and
	 * receiving datagrams with sender endpoint information.
	 *
	 * ### Key Characteristics
	 * - Connectionless: Each send operation is independent
	 * - Unreliable: No built-in acknowledgment or ordering
	 * - Endpoint-aware: Receive callbacks include sender information
	 *
	 * ### Callback Types
	 * - receive_callback_t: Called when data is received (includes sender endpoint)
	 * - error_callback_t: Called when an error occurs
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_client
	 */
	class i_udp_client : public i_network_component
	{
	public:
		//! \brief Endpoint information for UDP datagrams
		struct endpoint_info
		{
			std::string address;  //!< IP address string
			uint16_t port;        //!< Port number
		};

		//! \brief Callback type for received data (includes sender endpoint)
		using receive_callback_t = std::function<void(
			const std::vector<uint8_t>&,
			const endpoint_info&)>;

		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		//! \brief Callback type for send completion
		using send_callback_t = std::function<void(std::error_code, std::size_t)>;

		/*!
		 * \brief Starts the UDP client targeting the specified endpoint.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Creates a UDP socket
		 * - Resolves the target endpoint
		 * - Begins listening for incoming datagrams
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if socket creation fails
		 * - Returns error if name resolution fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(std::string_view host, uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the UDP client.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Thread Safety
		 * Thread-safe. Pending operations are cancelled.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Sends a datagram to the configured target endpoint.
		 * \param data The data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if not running
		 * - Returns error if send fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Multiple sends may be queued.
		 */
		[[nodiscard]] virtual auto send(
			std::vector<uint8_t>&& data,
			send_callback_t handler = nullptr) -> VoidResult = 0;

		/*!
		 * \brief Changes the target endpoint for future sends.
		 * \param host The new target hostname or IP address.
		 * \param port The new target port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if name resolution fails
		 *
		 * ### Thread Safety
		 * Thread-safe.
		 */
		[[nodiscard]] virtual auto set_target(std::string_view host, uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Sets the callback for received datagrams.
		 * \param callback The callback function.
		 *
		 * ### Note
		 * The callback receives both the data and the sender's endpoint
		 * information, allowing responses to be sent to the correct peer.
		 */
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
