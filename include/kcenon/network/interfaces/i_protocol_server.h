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
#include "i_session.h"

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
	 * \interface i_protocol_server
	 * \brief Unified interface for all protocol server implementations.
	 *
	 * This interface establishes a common contract for all protocol servers
	 * (TCP, UDP, HTTP, WebSocket, QUIC, etc.) in the network system. It provides
	 * a consistent API for server lifecycle management, connection handling,
	 * session management, and data broadcasting across different protocol implementations.
	 *
	 * ### Design Rationale
	 * This interface was created to:
	 * - Eliminate code duplication across protocol-specific server implementations
	 * - Provide a single, uniform API for all server types
	 * - Enable protocol-agnostic facade and adapter patterns
	 * - Simplify testing through consistent interface contracts
	 * - Support unified session management across all protocols
	 *
	 * ### Protocol Coverage
	 * Implementations include:
	 * - TCP servers (messaging_server, secure_messaging_server)
	 * - UDP servers (messaging_udp_server, secure_messaging_udp_server)
	 * - HTTP servers (http_server)
	 * - WebSocket servers (messaging_ws_server)
	 * - QUIC servers (messaging_quic_server)
	 *
	 * ### Session Management
	 * All server implementations manage client connections as sessions.
	 * Sessions are represented by the i_session interface, which provides:
	 * - Unique session identifiers
	 * - Per-session data transmission
	 * - Session metadata and state
	 *
	 * ### Callback Pattern
	 * Servers use callbacks to notify application code of events:
	 * - connection_callback_t: New client connected
	 * - disconnection_callback_t: Client disconnected
	 * - receive_callback_t: Data received from client
	 * - error_callback_t: Error occurred
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 * - Session objects are thread-safe
	 *
	 * ### Usage Example
	 * \code
	 * // Create server instance (protocol-specific)
	 * std::shared_ptr<i_protocol_server> server = std::make_shared<messaging_server>();
	 *
	 * // Set callbacks for events
	 * server->set_connection_callback([](std::shared_ptr<i_session> session) {
	 *     std::cout << "Client connected: " << session->id() << std::endl;
	 * });
	 *
	 * server->set_receive_callback([](std::string_view session_id, const std::vector<uint8_t>& data) {
	 *     std::cout << "Received " << data.size() << " bytes from " << session_id << std::endl;
	 * });
	 *
	 * // Start server on port
	 * if (auto result = server->start(8080); !result) {
	 *     // Handle start error
	 * }
	 *
	 * // Server is now accepting connections...
	 * std::cout << "Active connections: " << server->connection_count() << std::endl;
	 *
	 * // Stop when done
	 * server->stop();
	 * \endcode
	 *
	 * \see i_protocol_client
	 * \see i_session
	 * \see i_network_component
	 */
	class i_protocol_server : public i_network_component
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
		 * ### Behavior
		 * - Binds to the specified port on all interfaces (0.0.0.0/::)
		 * - Begins accepting incoming connections
		 * - Initializes protocol-specific resources
		 * - Invokes connection_callback for each new client
		 *
		 * ### Error Conditions
		 * - Returns error if already running
		 * - Returns error if port is already in use (EADDRINUSE)
		 * - Returns error if port binding fails (permission, invalid port)
		 * - Returns error if underlying protocol initialization fails
		 *
		 * ### Protocol-Specific Notes
		 * - TCP/WebSocket/QUIC: Creates listening socket
		 * - UDP: Creates datagram socket, begins receiving
		 * - HTTP: May create connection pool or thread pool
		 *
		 * ### Port Range
		 * Valid ports: 1-65535
		 * - Privileged ports (< 1024) may require elevated permissions
		 * - Port 0 requests OS to assign an available port
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 * Subsequent calls while running will return an error.
		 */
		[[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the server and closes all connections.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Stops accepting new connections
		 * - Closes all active client sessions gracefully
		 * - Invokes disconnection_callback for each session
		 * - Cancels all pending operations
		 * - Releases protocol-specific resources
		 *
		 * ### Error Conditions
		 * - Returns error if not running
		 *
		 * ### Graceful Shutdown
		 * The stop operation attempts to:
		 * 1. Stop accepting new connections immediately
		 * 2. Close existing connections gracefully (protocol-dependent)
		 * 3. Wait for pending operations to complete (with timeout)
		 * 4. Force-close remaining connections if timeout expires
		 *
		 * ### Protocol-Specific Notes
		 * - TCP: Sends FIN to all clients, waits for ACK
		 * - WebSocket: Sends close frame to all clients
		 * - UDP: Stops receiving, no client "close" needed
		 * - QUIC: Sends CONNECTION_CLOSE to all clients
		 *
		 * ### Thread Safety
		 * Thread-safe. Safe to call from any thread including callbacks.
		 * The method blocks until shutdown is complete.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Gets the number of active client connections.
		 * \return The count of currently connected clients.
		 *
		 * ### Definition of "Active"
		 * A connection is considered active if:
		 * - Connection is established (handshake complete if applicable)
		 * - Session has not been closed
		 * - No fatal errors have occurred on the session
		 *
		 * ### Protocol-Specific Notes
		 * - TCP/WebSocket/QUIC: Count of established connections
		 * - UDP: Count of unique peer endpoints seen recently
		 * - HTTP: Count of active persistent connections or requests
		 *
		 * ### Thread Safety
		 * Thread-safe. Uses atomic operations for accurate count.
		 *
		 * ### Performance
		 * O(1) operation. Does not iterate through connections.
		 */
		[[nodiscard]] virtual auto connection_count() const -> size_t = 0;

		/*!
		 * \brief Sets the callback for new connections.
		 * \param callback The callback function.
		 *
		 * ### Callback Parameters
		 * - session: Shared pointer to the new session object
		 *
		 * ### Callback Behavior
		 * Called when:
		 * - TCP: After accept() and initial handshake complete
		 * - WebSocket: After HTTP upgrade handshake succeeds
		 * - UDP: After first datagram received from new peer
		 * - QUIC: After connection establishment handshake
		 * - HTTP: After request line and headers parsed (HTTP/1.1)
		 *
		 * ### Session Object
		 * The session object allows:
		 * - Sending data to the specific client
		 * - Retrieving session metadata (ID, remote endpoint)
		 * - Closing the session individually
		 *
		 * ### Thread Safety
		 * The callback may be invoked from I/O threads.
		 * If the callback accesses shared state, it must be thread-safe.
		 *
		 * ### Example
		 * \code
		 * server->set_connection_callback([](std::shared_ptr<i_session> session) {
		 *     std::cout << "New client: " << session->id() << std::endl;
		 *     // Send welcome message
		 *     session->send({0x01, 0x02, 0x03});
		 * });
		 * \endcode
		 */
		virtual auto set_connection_callback(connection_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for disconnections.
		 * \param callback The callback function.
		 *
		 * ### Callback Parameters
		 * - session_id: String identifier of the disconnected session
		 *
		 * ### Callback Behavior
		 * Called when:
		 * - Client closes connection gracefully
		 * - Connection lost due to network error
		 * - Server closes the session
		 * - Timeout or keepalive failure
		 *
		 * ### Clean-up Responsibility
		 * The callback is the appropriate place to:
		 * - Remove session from application state
		 * - Log disconnection events
		 * - Release session-specific resources
		 *
		 * ### Thread Safety
		 * The callback may be invoked from I/O threads.
		 * Session cleanup code must be thread-safe.
		 *
		 * ### Example
		 * \code
		 * server->set_disconnection_callback([](std::string_view session_id) {
		 *     std::cout << "Client disconnected: " << session_id << std::endl;
		 *     // Remove from session map
		 *     sessions.erase(std::string(session_id));
		 * });
		 * \endcode
		 */
		virtual auto set_disconnection_callback(disconnection_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback The callback function.
		 *
		 * ### Callback Parameters
		 * - session_id: String identifier of the sending session
		 * - data: Vector of bytes received
		 *
		 * ### Data Ownership
		 * The data vector is passed by const reference.
		 * If the callback needs to retain the data, it must copy it.
		 * The data is valid only for the duration of the callback.
		 *
		 * ### Protocol-Specific Notes
		 * - TCP: May deliver partial messages (stream-based)
		 * - UDP: Delivers complete datagrams (message boundaries preserved)
		 * - WebSocket: Delivers complete messages (frame assembled)
		 * - QUIC: Delivers complete messages (stream data)
		 *
		 * ### Thread Safety
		 * The callback may be invoked from I/O threads.
		 * Multiple callbacks may execute concurrently for different sessions.
		 *
		 * ### Example
		 * \code
		 * server->set_receive_callback([](std::string_view session_id,
		 *                                  const std::vector<uint8_t>& data) {
		 *     std::cout << "Received " << data.size() << " bytes from "
		 *               << session_id << std::endl;
		 *     // Process data...
		 * });
		 * \endcode
		 */
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 *
		 * ### Callback Parameters
		 * - session_id: String identifier of the affected session (may be empty for server-level errors)
		 * - error: std::error_code describing the error
		 *
		 * ### Error Categories
		 * - Connection errors: Reset, timeout, refused
		 * - Protocol errors: Invalid handshake, malformed data
		 * - System errors: Out of memory, file descriptor limits
		 * - Application errors: Send queue full, rate limit exceeded
		 *
		 * ### Error Handling Strategy
		 * The callback should:
		 * - Log the error for diagnostics
		 * - Decide whether to close the session
		 * - Update error metrics/statistics
		 * - Notify monitoring systems if critical
		 *
		 * ### Automatic Disconnection
		 * Some errors trigger automatic session closure:
		 * - Connection reset (ECONNRESET)
		 * - Protocol violations
		 * - Fatal security errors
		 *
		 * For these, disconnection_callback will also be invoked.
		 *
		 * ### Thread Safety
		 * The callback may be invoked from I/O threads.
		 * Error handling code must be thread-safe.
		 *
		 * ### Example
		 * \code
		 * server->set_error_callback([](std::string_view session_id,
		 *                                std::error_code error) {
		 *     std::cerr << "Error on session " << session_id << ": "
		 *               << error.message() << std::endl;
		 * });
		 * \endcode
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
