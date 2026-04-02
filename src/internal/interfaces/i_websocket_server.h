// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/interfaces/i_network_component.h"
#include "kcenon/network/interfaces/i_session.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/detail/utils/result_types.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_websocket_session
	 * \brief Interface for a WebSocket session on the server side.
	 *
	 * This interface extends i_session with WebSocket-specific operations
	 * such as sending text/binary messages and closing with status codes.
	 */
	class i_websocket_session : public i_session
	{
	public:
		/*!
		 * \brief Sends a text message to the client.
		 * \param message The text message to send.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto send_text(std::string&& message) -> VoidResult = 0;

		/*!
		 * \brief Sends a binary message to the client.
		 * \param data The binary data to send.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto send_binary(std::vector<uint8_t>&& data) -> VoidResult = 0;

		/*!
		 * \brief Closes the WebSocket connection gracefully.
		 * \param code The close status code.
		 * \param reason Optional human-readable reason.
		 */
		virtual auto close(uint16_t code, std::string_view reason = "") -> void = 0;

		/*!
		 * \brief Gets the requested path from the handshake.
		 * \return The WebSocket path (e.g., "/ws").
		 */
		[[nodiscard]] virtual auto path() const -> std::string_view = 0;
	};

	/*!
	 * \interface i_websocket_server
	 * \brief Interface for WebSocket server components.
	 *
	 * This interface extends i_network_component with WebSocket server-specific
	 * operations such as handling text/binary messages and session management.
	 *
	 * ### Key Features
	 * - Text and binary message support
	 * - Session-based client management
	 * - Close frame handling with status codes
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_server
	 */
	class i_websocket_server : public i_network_component
	{
	public:
		//! \brief Callback type for new connections
		using connection_callback_t = std::function<void(std::shared_ptr<i_websocket_session>)>;

		//! \brief Callback type for disconnections (with close code and reason)
		using disconnection_callback_t = std::function<void(
			std::string_view session_id,
			uint16_t code,
			std::string_view reason)>;

		//! \brief Callback type for text messages (session_id, message)
		using text_callback_t = std::function<void(std::string_view, const std::string&)>;

		//! \brief Callback type for binary messages (session_id, data)
		using binary_callback_t = std::function<void(std::string_view, const std::vector<uint8_t>&)>;

		//! \brief Callback type for errors (session_id, error)
		using error_callback_t = std::function<void(std::string_view, std::error_code)>;

		/*!
		 * \brief Starts the WebSocket server on the specified port.
		 * \param port The port number to listen on.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Starts TCP listener
		 * - Handles WebSocket handshakes for incoming connections
		 * - Begins accepting WebSocket connections
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the WebSocket server.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Stops accepting new connections
		 * - Closes all active sessions
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Gets the number of active WebSocket connections.
		 * \return The count of currently connected clients.
		 */
		[[nodiscard]] virtual auto connection_count() const -> size_t = 0;

		/*!
		 * \brief Sets the callback for new connections.
		 * \param callback The callback function.
		 */
		virtual auto set_connection_callback(connection_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for disconnections.
		 * \param callback The callback function.
		 */
		virtual auto set_disconnection_callback(disconnection_callback_t callback) -> void = 0;

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
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
