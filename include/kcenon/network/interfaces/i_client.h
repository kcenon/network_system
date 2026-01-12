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
	 * \interface i_client
	 * \brief Base interface for client-side network components.
	 *
	 * This interface extends i_network_component with client-specific
	 * operations such as connecting to a server, sending data, and
	 * handling connection state.
	 *
	 * ### Callback Types
	 * - receive_callback_t: Called when data is received
	 * - connected_callback_t: Called when connection is established
	 * - disconnected_callback_t: Called when connection is lost
	 * - error_callback_t: Called when an error occurs
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_tcp_client
	 * \see i_udp_client
	 * \see i_websocket_client
	 * \see i_quic_client
	 */
	class i_client : public i_network_component
	{
	public:
		//! \brief Callback type for received data
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Starts the client and connects to the specified server.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if connection fails
		 * - Returns error if host resolution fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(std::string_view host, uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the client and closes the connection.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if not running
		 *
		 * ### Thread Safety
		 * Thread-safe. Cancels pending operations and triggers
		 * disconnected callback.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Sends data to the connected server.
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if not connected
		 * - Returns error if send operation fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Multiple sends may be queued.
		 */
		[[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;

		/*!
		 * \brief Checks if the client is connected to the server.
		 * \return true if connected, false otherwise.
		 *
		 * ### Note
		 * A client can be running but not connected (e.g., during
		 * connection establishment or after disconnection).
		 */
		[[nodiscard]] virtual auto is_connected() const -> bool = 0;

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback The callback function.
		 *
		 * ### Thread Safety
		 * Thread-safe. The callback may be invoked from I/O threads.
		 */
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for connection established.
		 * \param callback The callback function.
		 */
		virtual auto set_connected_callback(connected_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback The callback function.
		 */
		virtual auto set_disconnected_callback(disconnected_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
