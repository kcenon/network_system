// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "i_network_component.h"
#include "i_session.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network-core/utils/result_types.h"

namespace kcenon::network_core::interfaces
{

	/*!
	 * \interface i_server
	 * \brief Base interface for server-side network components.
	 *
	 * This interface extends i_network_component with server-specific
	 * operations such as listening for connections, managing sessions,
	 * and broadcasting data.
	 *
	 * ### Callback Types
	 * - connection_callback_t: Called when a new client connects
	 * - disconnection_callback_t: Called when a client disconnects
	 * - receive_callback_t: Called when data is received from a client
	 * - error_callback_t: Called when an error occurs
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_tcp_server
	 * \see i_udp_server
	 * \see i_websocket_server
	 * \see i_quic_server
	 */
	class i_server : public i_network_component
	{
	public:
		//! \brief Callback type for new connections
		using connection_callback_t = std::function<void(std::shared_ptr<i_session>)>;
		//! \brief Callback type for disconnections (session_id)
		using disconnection_callback_t = std::function<void(std::string_view)>;
		//! \brief Callback type for received data (session_id, data)
		using receive_callback_t = std::function<void(std::string_view, const std::vector<uint8_t>&)>;
		//! \brief Callback type for errors (session_id, error)
		using error_callback_t = std::function<void(std::string_view, std::error_code)>;

		/*!
		 * \brief Starts the server and begins listening for connections.
		 * \param port The port number to listen on.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if port binding fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the server and closes all connections.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Stops accepting new connections
		 * - Closes all active sessions
		 * - Triggers disconnection callbacks for each session
		 *
		 * ### Thread Safety
		 * Thread-safe. Pending operations are cancelled.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Gets the number of active connections.
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
		 * \brief Sets the callback for received data.
		 * \param callback The callback function.
		 */
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network_core::interfaces
