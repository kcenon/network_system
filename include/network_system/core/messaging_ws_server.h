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

#include "network_system/internal/websocket_protocol.h"
#include "network_system/utils/result_types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace network_system::core
{
	/*!
	 * \struct ws_server_config
	 * \brief Configuration for WebSocket server.
	 *
	 * Provides comprehensive configuration options for WebSocket server behavior
	 * including connection limits, timeouts, and protocol options.
	 */
	struct ws_server_config
	{
		uint16_t port = 8080;                            ///< Server port
		std::string path = "/";                          ///< WebSocket path
		size_t max_connections = 1000;                   ///< Max concurrent connections
		std::chrono::milliseconds ping_interval{30000};  ///< Ping interval
		bool auto_pong = true;                           ///< Auto-respond to pings
		size_t max_message_size = 10 * 1024 * 1024;     ///< Max message size (10MB)
	};

	// Forward declaration
	class messaging_ws_server;

	/*!
	 * \class ws_connection
	 * \brief Represents a WebSocket connection to a client.
	 *
	 * This class provides an interface for interacting with individual
	 * WebSocket connections. It allows sending messages, closing connections,
	 * and querying connection information.
	 *
	 * Thread Safety:
	 * - All public methods are thread-safe
	 * - Can be safely shared across threads
	 */
	class ws_connection
	{
	public:
		/*!
		 * \brief Sends a text message to the client.
		 *
		 * Uses move semantics for zero-copy operation. The message must be
		 * valid UTF-8. The handler is called when the message is sent or
		 * if an error occurs.
		 *
		 * \param message The text message to send
		 * \param handler Optional callback invoked with send result
		 * \return VoidResult indicating validation success
		 */
		auto send_text(std::string&& message,
					   std::function<void(std::error_code, std::size_t)> handler =
						   nullptr) -> VoidResult;

		/*!
		 * \brief Sends a binary message to the client.
		 *
		 * Uses move semantics for zero-copy operation. The handler is called
		 * when the message is sent or if an error occurs.
		 *
		 * \param data The binary data to send
		 * \param handler Optional callback invoked with send result
		 * \return VoidResult indicating success
		 */
		auto send_binary(std::vector<uint8_t>&& data,
						 std::function<void(std::error_code, std::size_t)> handler =
							 nullptr) -> VoidResult;

		/*!
		 * \brief Closes the connection gracefully.
		 *
		 * Sends a close frame with the specified code and reason.
		 *
		 * \param code The close status code (default: normal)
		 * \param reason Optional human-readable reason
		 * \return VoidResult indicating success
		 */
		auto close(internal::ws_close_code code = internal::ws_close_code::normal,
				   const std::string& reason = "") -> VoidResult;

		/*!
		 * \brief Gets the connection ID.
		 *
		 * \return The unique connection identifier
		 */
		auto connection_id() const -> const std::string&;

		/*!
		 * \brief Gets the remote endpoint address.
		 *
		 * \return String representation of remote address (e.g., "192.168.1.1:54321")
		 */
		auto remote_endpoint() const -> std::string;

		// Allow server to create connections
		friend class messaging_ws_server;

	private:
		class impl;
		std::shared_ptr<impl> pimpl_;

		// Private constructor - only server can create
		ws_connection(std::shared_ptr<impl> impl);
	};

	/*!
	 * \class messaging_ws_server
	 * \brief High-level WebSocket server with connection management.
	 *
	 * This class provides a simple, easy-to-use interface for WebSocket server
	 * applications. It handles:
	 * - Accepting incoming connections
	 * - Connection management (tracking, limits)
	 * - Message broadcasting
	 * - Per-connection message handling
	 * - Graceful shutdown
	 * - Event-driven callbacks
	 *
	 * Thread Safety:
	 * - All public methods are thread-safe
	 * - Callbacks are invoked from the internal I/O thread
	 * - Broadcast operations are thread-safe
	 *
	 * Usage Example:
	 * \code
	 * auto server = std::make_shared<messaging_ws_server>("my_server");
	 *
	 * server->set_connection_callback([](auto conn) {
	 *     std::cout << "Client connected: " << conn->connection_id() << std::endl;
	 * });
	 *
	 * server->set_text_message_callback([](auto conn, const std::string& msg) {
	 *     std::cout << "Received: " << msg << std::endl;
	 *     conn->send_text("Echo: " + msg);
	 * });
	 *
	 * server->start_server(8080, "/ws");
	 * \endcode
	 */
	class messaging_ws_server
		: public std::enable_shared_from_this<messaging_ws_server>
	{
	public:
		/*!
		 * \brief Constructs a WebSocket server.
		 *
		 * \param server_id A unique identifier for this server instance
		 */
		explicit messaging_ws_server(const std::string& server_id);

		/*!
		 * \brief Destructor.
		 *
		 * Automatically stops the server if still running.
		 */
		~messaging_ws_server();

		/*!
		 * \brief Starts the server with full configuration.
		 *
		 * Binds to the specified port and begins accepting connections.
		 * Returns immediately; server runs in background thread.
		 *
		 * \param config The server configuration
		 * \return VoidResult indicating success or error
		 */
		auto start_server(const ws_server_config& config) -> VoidResult;

		/*!
		 * \brief Starts the server with simple parameters.
		 *
		 * Convenience method that uses default configuration with specified
		 * port and path.
		 *
		 * \param port Server port number
		 * \param path WebSocket path (default: "/")
		 * \return VoidResult indicating success or error
		 */
		auto start_server(uint16_t port, std::string_view path = "/") -> VoidResult;

		/*!
		 * \brief Stops the server.
		 *
		 * Closes all connections and stops accepting new ones. Returns
		 * immediately; use wait_for_stop() to block until fully stopped.
		 *
		 * \return VoidResult indicating success or error
		 */
		auto stop_server() -> VoidResult;

		/*!
		 * \brief Waits for the server to stop completely.
		 *
		 * Blocks the calling thread until the server has fully stopped.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Checks if the server is running.
		 *
		 * \return True if the server is accepting connections
		 */
		auto is_running() const -> bool;

		/*!
		 * \brief Broadcasts a text message to all connected clients.
		 *
		 * \param message The text message to broadcast
		 */
		auto broadcast_text(const std::string& message) -> void;

		/*!
		 * \brief Broadcasts a binary message to all connected clients.
		 *
		 * \param data The binary data to broadcast
		 */
		auto broadcast_binary(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Gets a connection by ID.
		 *
		 * \param connection_id The connection identifier
		 * \return Shared pointer to connection, or nullptr if not found
		 */
		auto get_connection(const std::string& connection_id)
			-> std::shared_ptr<ws_connection>;

		/*!
		 * \brief Gets all connection IDs.
		 *
		 * \return Vector of connection identifiers
		 */
		auto get_all_connections() -> std::vector<std::string>;

		/*!
		 * \brief Gets the current connection count.
		 *
		 * \return Number of active connections
		 */
		auto connection_count() const -> size_t;

		/*!
		 * \brief Sets the callback for new connections.
		 *
		 * \param callback Function called when a client connects
		 */
		auto set_connection_callback(
			std::function<void(std::shared_ptr<ws_connection>)> callback) -> void;

		/*!
		 * \brief Sets the callback for disconnections.
		 *
		 * \param callback Function called when a client disconnects
		 */
		auto set_disconnection_callback(
			std::function<void(const std::string&, internal::ws_close_code,
							   const std::string&)>
				callback) -> void;

		/*!
		 * \brief Sets the callback for all message types.
		 *
		 * \param callback Function called when a message is received
		 */
		auto set_message_callback(
			std::function<void(std::shared_ptr<ws_connection>,
							   const internal::ws_message&)>
				callback) -> void;

		/*!
		 * \brief Sets the callback for text messages only.
		 *
		 * \param callback Function called when a text message is received
		 */
		auto set_text_message_callback(
			std::function<void(std::shared_ptr<ws_connection>, const std::string&)>
				callback) -> void;

		/*!
		 * \brief Sets the callback for binary messages only.
		 *
		 * \param callback Function called when a binary message is received
		 */
		auto set_binary_message_callback(
			std::function<void(std::shared_ptr<ws_connection>,
							   const std::vector<uint8_t>&)>
				callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 *
		 * \param callback Function called when an error occurs
		 */
		auto set_error_callback(
			std::function<void(const std::string&, std::error_code)> callback)
			-> void;

		/*!
		 * \brief Gets the server ID.
		 *
		 * \return The server identifier
		 */
		auto server_id() const -> const std::string&;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};

} // namespace network_system::core
