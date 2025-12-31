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

#include "kcenon/network/core/messaging_ws_server_base.h"
#include "kcenon/network/internal/websocket_protocol.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/integration/thread_integration.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <asio.hpp>

namespace kcenon::network::internal
{
	class websocket_socket;
}

namespace kcenon::network::core
{
	class ws_session_manager;

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
		 * \param message The text message to send.
		 * \param handler Optional callback invoked with send result.
		 * \return VoidResult indicating validation success.
		 */
		auto send_text(std::string&& message,
					   std::function<void(std::error_code, std::size_t)> handler = nullptr) -> VoidResult;

		/*!
		 * \brief Sends a binary message to the client.
		 * \param data The binary data to send.
		 * \param handler Optional callback invoked with send result.
		 * \return VoidResult indicating success.
		 */
		auto send_binary(std::vector<uint8_t>&& data,
						 std::function<void(std::error_code, std::size_t)> handler = nullptr) -> VoidResult;

		/*!
		 * \brief Closes the connection gracefully.
		 * \param code The close status code (default: normal).
		 * \param reason Optional human-readable reason.
		 * \return VoidResult indicating success.
		 */
		auto close(internal::ws_close_code code = internal::ws_close_code::normal,
				   const std::string& reason = "") -> VoidResult;

		/*!
		 * \brief Gets the connection ID.
		 * \return The unique connection identifier.
		 */
		auto connection_id() const -> const std::string&;

		/*!
		 * \brief Gets the remote endpoint address.
		 * \return String representation of remote address (e.g., "192.168.1.1:54321").
		 */
		auto remote_endpoint() const -> std::string;

		// Allow server to create connections
		friend class messaging_ws_server;

	public:
		class impl;
		std::shared_ptr<impl> pimpl_;

		// Constructor - used by server
		ws_connection(std::shared_ptr<impl> impl);
	};

	/*!
	 * \class messaging_ws_server
	 * \brief High-level WebSocket server with connection management.
	 *
	 * This class inherits from messaging_ws_server_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
	 *
	 * It handles:
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
		: public messaging_ws_server_base<messaging_ws_server>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_ws_server_base<messaging_ws_server>;

		/*!
		 * \brief Constructs a WebSocket server.
		 * \param server_id A unique identifier for this server instance.
		 */
		explicit messaging_ws_server(std::string_view server_id);

		/*!
		 * \brief Destructor.
		 * Automatically stops the server if still running (handled by base class).
		 */
		~messaging_ws_server() noexcept override = default;

		/*!
		 * \brief Starts the server with full configuration.
		 * \param config The server configuration.
		 * \return VoidResult indicating success or error.
		 */
		auto start_server(const ws_server_config& config) -> VoidResult;

		// start_server(port, path), stop_server(), wait_for_stop(),
		// is_running(), server_id() are provided by base class

		/*!
		 * \brief Broadcasts a text message to all connected clients.
		 * \param message The text message to broadcast.
		 */
		auto broadcast_text(const std::string& message) -> void;

		/*!
		 * \brief Broadcasts a binary message to all connected clients.
		 * \param data The binary data to broadcast.
		 */
		auto broadcast_binary(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Gets a connection by ID.
		 * \param connection_id The connection identifier.
		 * \return Shared pointer to connection, or nullptr if not found.
		 */
		auto get_connection(const std::string& connection_id) -> std::shared_ptr<ws_connection>;

		/*!
		 * \brief Gets all connection IDs.
		 * \return Vector of connection identifiers.
		 */
		auto get_all_connections() -> std::vector<std::string>;

		/*!
		 * \brief Gets the current connection count.
		 * \return Number of active connections.
		 */
		auto connection_count() const -> size_t;

	protected:
		/*!
		 * \brief WebSocket-specific implementation of server start.
		 * \param port The port number to listen on.
		 * \param path The WebSocket path.
		 * \return Result<void> - Success if server started, or error with code.
		 *
		 * Called by base class start_server() after common validation.
		 */
		auto do_start(uint16_t port, std::string_view path) -> VoidResult;

		/*!
		 * \brief WebSocket-specific implementation of server stop.
		 * \return Result<void> - Success if server stopped, or error with code.
		 *
		 * Called by base class stop_server() after common cleanup.
		 */
		auto do_stop() -> VoidResult;

	private:
		/*!
		 * \brief Starts accepting new connections.
		 */
		auto do_accept() -> void;

		/*!
		 * \brief Handles a new connection.
		 */
		auto handle_new_connection(std::shared_ptr<asio::ip::tcp::socket> socket) -> void;

		/*!
		 * \brief Handles received WebSocket messages.
		 */
		auto on_message(std::shared_ptr<ws_connection> conn, const internal::ws_message& msg) -> void;

		/*!
		 * \brief Handles connection close.
		 */
		auto on_close(const std::string& conn_id, internal::ws_close_code code, const std::string& reason) -> void;

		/*!
		 * \brief Handles errors.
		 */
		auto on_error(const std::string& conn_id, std::error_code ec) -> void;

	private:
		ws_server_config config_;                        /*!< Server configuration. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_; /*!< Work guard. */
		std::unique_ptr<asio::ip::tcp::acceptor> acceptor_; /*!< TCP acceptor. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::shared_ptr<ws_session_manager> session_mgr_; /*!< Session manager. */
	};

} // namespace kcenon::network::core
