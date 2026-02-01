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
#include "connection_observer.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_protocol_client
	 * \brief Unified interface for all protocol client implementations.
	 *
	 * This interface establishes a common contract for all protocol clients
	 * (TCP, UDP, HTTP, WebSocket, QUIC, etc.) in the network system. It provides
	 * a consistent API for client lifecycle management, connection handling,
	 * and data transmission across different protocol implementations.
	 *
	 * ### Design Rationale
	 * This interface was created to:
	 * - Eliminate code duplication across protocol-specific client implementations
	 * - Provide a single, uniform API for all client types
	 * - Enable protocol-agnostic facade and adapter patterns
	 * - Simplify testing through consistent interface contracts
	 *
	 * ### Protocol Coverage
	 * Implementations include:
	 * - TCP clients (messaging_client, secure_messaging_client)
	 * - UDP clients (messaging_udp_client, reliable_udp_client)
	 * - HTTP clients (http_client)
	 * - WebSocket clients (messaging_ws_client)
	 * - QUIC clients (messaging_quic_client)
	 *
	 * ### Observer Pattern (Recommended)
	 * Use set_observer() with a connection_observer implementation for
	 * unified event handling across all protocol types.
	 *
	 * ### Legacy Callback Support
	 * Individual callback setters are provided for backward compatibility
	 * but are deprecated in favor of the observer pattern.
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks and observer methods may be invoked from I/O threads
	 *
	 * ### Usage Example
	 * \code
	 * // Create client instance (protocol-specific)
	 * std::shared_ptr<i_protocol_client> client = std::make_shared<messaging_client>();
	 *
	 * // Set observer for events
	 * auto observer = std::make_shared<my_connection_observer>();
	 * client->set_observer(observer);
	 *
	 * // Start and connect
	 * if (auto result = client->start("127.0.0.1", 8080); !result) {
	 *     // Handle connection error
	 * }
	 *
	 * // Send data
	 * std::vector<uint8_t> data = { 0x01, 0x02, 0x03 };
	 * client->send(std::move(data));
	 *
	 * // Stop when done
	 * client->stop();
	 * \endcode
	 *
	 * \see i_protocol_server
	 * \see connection_observer
	 * \see i_network_component
	 */
	class i_protocol_client : public i_network_component
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
		 * ### Behavior
		 * - Creates the underlying protocol-specific connection
		 * - Resolves hostname to IP address if needed
		 * - Establishes connection with the server
		 * - Begins receiving data (for connection-oriented protocols)
		 * - Invokes connected callback/observer on success
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if host resolution fails
		 * - Returns error if connection establishment fails
		 * - Returns error if underlying protocol initialization fails
		 *
		 * ### Protocol-Specific Notes
		 * - TCP/WebSocket/QUIC: Establishes stateful connection
		 * - UDP: Sets default target endpoint, does not "connect" in traditional sense
		 * - HTTP: May establish connection pool or persistent connection
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 * Subsequent calls while running will return an error.
		 */
		[[nodiscard]] virtual auto start(std::string_view host, uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the client and closes the connection.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Cancels all pending operations
		 * - Closes the connection gracefully if possible
		 * - Invokes disconnected callback/observer
		 * - Releases protocol-specific resources
		 *
		 * ### Error Conditions
		 * - Returns error if not running
		 *
		 * ### Protocol-Specific Notes
		 * - TCP: Sends FIN, waits for graceful close
		 * - WebSocket: Sends close frame with status code
		 * - UDP: Stops receiving, no "close" required
		 * - QUIC: Sends CONNECTION_CLOSE frame
		 *
		 * ### Thread Safety
		 * Thread-safe. Safe to call from any thread including callbacks.
		 * Pending operations will be cancelled and cleaned up.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Sends data to the connected server.
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Queues data for transmission
		 * - May block if send queue is full (protocol-dependent)
		 * - Data is moved, not copied
		 *
		 * ### Error Conditions
		 * - Returns error if not connected
		 * - Returns error if send buffer is full
		 * - Returns error if underlying protocol send fails
		 *
		 * ### Protocol-Specific Notes
		 * - TCP: Stream-based, may fragment data
		 * - UDP: Datagram-based, preserves message boundaries
		 * - WebSocket: Frame-based, preserves message boundaries
		 * - HTTP: Typically request/response, may queue
		 *
		 * ### Thread Safety
		 * Thread-safe. Multiple sends may be queued concurrently.
		 * Send operations are serialized internally.
		 */
		[[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;

		/*!
		 * \brief Checks if the client is connected to the server.
		 * \return true if connected, false otherwise.
		 *
		 * ### Connection States
		 * - Not started: is_running() == false, is_connected() == false
		 * - Starting/Connecting: is_running() == true, is_connected() == false
		 * - Connected: is_running() == true, is_connected() == true
		 * - Disconnected: is_running() == true, is_connected() == false
		 * - Stopped: is_running() == false, is_connected() == false
		 *
		 * ### Protocol-Specific Notes
		 * - TCP/WebSocket/QUIC: Returns true only when connection is established
		 * - UDP: Returns true when target endpoint is set (no actual "connection")
		 * - HTTP: Returns true if connection pool has active connections
		 *
		 * ### Thread Safety
		 * Thread-safe. Uses atomic operations for state checking.
		 */
		[[nodiscard]] virtual auto is_connected() const -> bool = 0;

		/*!
		 * \brief Sets the connection observer for unified event handling.
		 * \param observer The observer instance (shared ownership).
		 *
		 * The observer receives all connection events through a single interface:
		 * - on_connected(): Connection established
		 * - on_disconnected(): Connection lost or closed
		 * - on_receive(data): Data received from server
		 * - on_error(error_code): Error occurred
		 *
		 * ### Recommended Pattern
		 * This is the preferred method for event handling as it:
		 * - Centralizes all event handling in one place
		 * - Provides consistent interface across all protocols
		 * - Enables easier testing through mock observers
		 *
		 * ### Usage
		 * \code
		 * class my_observer : public connection_observer {
		 *     void on_connected() override { /* handle connect */ }
		 *     void on_receive(const std::vector<uint8_t>& data) override { /* handle data */ }
		 *     // ... implement other methods
		 * };
		 *
		 * client->set_observer(std::make_shared<my_observer>());
		 * \endcode
		 *
		 * ### Thread Safety
		 * Thread-safe. Observer methods may be invoked from I/O threads.
		 * Observer must be thread-safe if shared across multiple clients.
		 *
		 * \see connection_observer
		 * \see callback_adapter
		 */
		virtual auto set_observer(std::shared_ptr<connection_observer> observer) -> void = 0;

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback The callback function.
		 *
		 * ### Thread Safety
		 * Thread-safe. The callback may be invoked from I/O threads.
		 * Callback must be thread-safe if it accesses shared state.
		 *
		 * \deprecated Use set_observer() with connection_observer instead.
		 *             Use callback_adapter for gradual migration from legacy code.
		 */
		[[deprecated("Use set_observer() with connection_observer instead")]]
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for connection established.
		 * \param callback The callback function.
		 *
		 * \deprecated Use set_observer() with connection_observer instead.
		 *             Use callback_adapter for gradual migration from legacy code.
		 */
		[[deprecated("Use set_observer() with connection_observer instead")]]
		virtual auto set_connected_callback(connected_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback The callback function.
		 *
		 * \deprecated Use set_observer() with connection_observer instead.
		 *             Use callback_adapter for gradual migration from legacy code.
		 */
		[[deprecated("Use set_observer() with connection_observer instead")]]
		virtual auto set_disconnected_callback(disconnected_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 *
		 * \deprecated Use set_observer() with connection_observer instead.
		 *             Use callback_adapter for gradual migration from legacy code.
		 */
		[[deprecated("Use set_observer() with connection_observer instead")]]
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
