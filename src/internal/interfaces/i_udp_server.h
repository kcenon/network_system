// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/interfaces/i_network_component.h"
#include "i_udp_client.h"

#include <cstdint>
#include <functional>
#include <system_error>
#include <vector>

#include "kcenon/network/detail/utils/result_types.h"

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
