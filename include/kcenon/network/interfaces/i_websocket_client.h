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
 * @file i_websocket_client.h
 * @brief WebSocket client interface (deprecated - use facade API)
 *
 * @deprecated This header exposes internal implementation details.
 * Use the facade API instead:
 *
 * @code
 * #include <kcenon/network/facade/websocket_facade.h>
 *
 * auto client = kcenon::network::facade::websocket_facade{}.create_client({
 *     .host = "example.com",
 *     .port = 443,
 *     .path = "/ws",
 *     .use_ssl = true
 * });
 * @endcode
 *
 * This header will be moved to internal in a future release.
 */

#include "i_network_component.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/detail/utils/result_types.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_websocket_client
	 * \brief Interface for WebSocket client components.
	 *
	 * This interface extends i_network_component with WebSocket-specific
	 * operations such as text/binary message sending, path-based connections,
	 * and close frame handling.
	 *
	 * ### Key Features
	 * - Text and binary message support
	 * - Path-based connection (e.g., "/ws", "/api/stream")
	 * - Ping/pong keepalive support
	 * - Graceful close with status codes
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_client
	 */
	class i_websocket_client : public i_network_component
	{
	public:
		//! \brief Callback type for text messages
		using text_callback_t = std::function<void(const std::string&)>;

		//! \brief Callback type for binary messages
		using binary_callback_t = std::function<void(const std::vector<uint8_t>&)>;

		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;

		//! \brief Callback type for disconnection (with close code and reason)
		using disconnected_callback_t = std::function<void(uint16_t code, std::string_view reason)>;

		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		//! \brief Callback type for send completion
		using send_callback_t = std::function<void(std::error_code, std::size_t)>;

		/*!
		 * \brief Starts the WebSocket client connecting to the specified endpoint.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \param path The WebSocket path (default: "/").
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Establishes TCP connection
		 * - Performs WebSocket handshake
		 * - Begins message receive loop
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if connection fails
		 * - Returns error if handshake fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(
			std::string_view host,
			uint16_t port,
			std::string_view path = "/") -> VoidResult = 0;

		/*!
		 * \brief Stops the WebSocket client.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * Performs graceful close if connected.
		 *
		 * ### Thread Safety
		 * Thread-safe.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Checks if the WebSocket connection is established.
		 * \return true if connected, false otherwise.
		 */
		[[nodiscard]] virtual auto is_connected() const -> bool = 0;

		/*!
		 * \brief Sends a text message.
		 * \param message The text message to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if not connected
		 */
		[[nodiscard]] virtual auto send_text(
			std::string&& message,
			send_callback_t handler = nullptr) -> VoidResult = 0;

		/*!
		 * \brief Sends a binary message.
		 * \param data The binary data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if not connected
		 */
		[[nodiscard]] virtual auto send_binary(
			std::vector<uint8_t>&& data,
			send_callback_t handler = nullptr) -> VoidResult = 0;

		/*!
		 * \brief Sends a ping frame.
		 * \param payload Optional payload data (max 125 bytes).
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto ping(std::vector<uint8_t>&& payload = {}) -> VoidResult = 0;

		/*!
		 * \brief Closes the WebSocket connection gracefully.
		 * \param code The close status code.
		 * \param reason Optional human-readable reason.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto close(
			uint16_t code = 1000,
			std::string_view reason = "") -> VoidResult = 0;

		/*!
		 * \brief Sets the callback for text messages.
		 * \param callback The callback function.
		 */
		virtual auto set_text_callback(text_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for binary messages.
		 * \param callback The callback function.
		 */
		virtual auto set_binary_callback(binary_callback_t callback) -> void = 0;

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
