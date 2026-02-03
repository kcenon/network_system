/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "kcenon/network/core/callback_indices.h"
#include "kcenon/network/interfaces/i_websocket_server.h"
#include "internal/websocket/websocket_protocol.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/callback_manager.h"
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
	 * WebSocket connections. It implements the i_websocket_session interface
	 * for composition-based usage.
	 *
	 * Thread Safety:
	 * - All public methods are thread-safe
	 * - Can be safely shared across threads
	 *
	 * ### Interface Compliance
	 * This class implements interfaces::i_websocket_session.
	 */
	class ws_connection : public interfaces::i_websocket_session
	{
	public:
		// Allow server to create connections
		friend class messaging_ws_server;

		// Constructor - used by server
		explicit ws_connection(std::shared_ptr<class ws_connection_impl> impl);

		//! \brief Destructor
		~ws_connection() override = default;

		// ========================================================================
		// i_websocket_session interface implementation
		// ========================================================================

		/*!
		 * \brief Gets the unique identifier for this session.
		 * \return A string view of the session ID.
		 *
		 * Implements i_session::id().
		 */
		[[nodiscard]] auto id() const -> std::string_view override;

		/*!
		 * \brief Checks if the session is currently connected.
		 * \return true if connected, false otherwise.
		 *
		 * Implements i_session::is_connected().
		 */
		[[nodiscard]] auto is_connected() const -> bool override;

		/*!
		 * \brief Sends data to the client.
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_session::send(). Sends as binary.
		 */
		[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;

		/*!
		 * \brief Closes the session.
		 *
		 * Implements i_session::close().
		 */
		auto close() -> void override;

		/*!
		 * \brief Sends a text message to the client.
		 * \param message The text message to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_session::send_text().
		 */
		[[nodiscard]] auto send_text(std::string&& message) -> VoidResult override;

		/*!
		 * \brief Sends a binary message to the client.
		 * \param data The binary data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_session::send_binary().
		 */
		[[nodiscard]] auto send_binary(std::vector<uint8_t>&& data) -> VoidResult override;

		/*!
		 * \brief Closes the WebSocket connection gracefully.
		 * \param code The close status code.
		 * \param reason Optional human-readable reason.
		 *
		 * Implements i_websocket_session::close().
		 */
		auto close(uint16_t code, std::string_view reason = "") -> void override;

		/*!
		 * \brief Gets the requested path from the handshake.
		 * \return The WebSocket path (e.g., "/ws").
		 *
		 * Implements i_websocket_session::path().
		 */
		[[nodiscard]] auto path() const -> std::string_view override;

		/*!
		 * \brief Gets the remote endpoint address.
		 * \return String representation of remote address (e.g., "192.168.1.1:54321").
		 */
		auto remote_endpoint() const -> std::string;

	private:
		//! \brief Internal: Get the implementation pointer (for server use)
		auto get_impl() const -> std::shared_ptr<ws_connection_impl> { return pimpl_; }

		std::shared_ptr<ws_connection_impl> pimpl_;
	};

	//! \brief Forward declaration for implementation
	class ws_connection_impl;

	/*!
	 * \class messaging_ws_server
	 * \brief High-level WebSocket server with connection management.
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 * It also implements the i_websocket_server interface for composition-based usage.
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
	 * ### Interface Compliance
	 * This class implements interfaces::i_websocket_server for composition-based usage.
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
		, public interfaces::i_websocket_server
	{
	public:
		//! \brief Callback type for new connections
		using connection_callback_t = std::function<void(std::shared_ptr<ws_connection>)>;
		//! \brief Callback type for disconnections
		using disconnection_callback_t = std::function<void(const std::string&, internal::ws_close_code, const std::string&)>;
		//! \brief Callback type for WebSocket messages
		using message_callback_t = std::function<void(std::shared_ptr<ws_connection>, const internal::ws_message&)>;
		//! \brief Callback type for text messages
		using text_message_callback_t = std::function<void(std::shared_ptr<ws_connection>, const std::string&)>;
		//! \brief Callback type for binary messages
		using binary_message_callback_t = std::function<void(std::shared_ptr<ws_connection>, const std::vector<uint8_t>&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(const std::string&, std::error_code)>;

		/*!
		 * \brief Constructs a WebSocket server.
		 * \param server_id A unique identifier for this server instance.
		 */
		explicit messaging_ws_server(std::string_view server_id);

		/*!
		 * \brief Destructor.
		 * Automatically stops the server if still running.
		 */
		~messaging_ws_server() noexcept override;

		// Non-copyable, non-movable
		messaging_ws_server(const messaging_ws_server&) = delete;
		messaging_ws_server& operator=(const messaging_ws_server&) = delete;
		messaging_ws_server(messaging_ws_server&&) = delete;
		messaging_ws_server& operator=(messaging_ws_server&&) = delete;

		// ========================================================================
		// Lifecycle Management
		// ========================================================================

		/*!
		 * \brief Starts the server with full configuration.
		 * \param config The server configuration.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto start_server(const ws_server_config& config) -> VoidResult;

		/*!
		 * \brief Starts the server on the specified port.
		 * \param port The port number to listen on.
		 * \param path The WebSocket path (default: "/").
		 * \return Result<void> - Success if server started, or error with code.
		 */
		[[nodiscard]] auto start_server(uint16_t port, std::string_view path = "/") -> VoidResult;

		/*!
		 * \brief Stops the server and releases all resources.
		 * \return Result<void> - Success if server stopped, or error with code.
		 */
		[[nodiscard]] auto stop_server() -> VoidResult;

		/*!
		 * \brief Returns the server identifier.
		 * \return The server_id string.
		 */
		[[nodiscard]] auto server_id() const -> const std::string&;

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

		// ========================================================================
		// i_network_component interface implementation
		// ========================================================================

		/*!
		 * \brief Checks if the server is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override;

		/*!
		 * \brief Blocks until stop() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override;

		// ========================================================================
		// i_websocket_server interface implementation
		// ========================================================================

		/*!
		 * \brief Starts the WebSocket server on the specified port.
		 * \param port The port number to listen on.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_server::start(). Delegates to start_server().
		 */
		[[nodiscard]] auto start(uint16_t port) -> VoidResult override;

		/*!
		 * \brief Stops the WebSocket server.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_server::stop(). Delegates to stop_server().
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Gets the number of active WebSocket connections.
		 * \return The count of currently connected clients.
		 *
		 * Implements i_websocket_server::connection_count().
		 */
		[[nodiscard]] auto connection_count() const -> size_t override;

		/*!
		 * \brief Sets the callback for new connections (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_server::set_connection_callback().
		 */
		auto set_connection_callback(interfaces::i_websocket_server::connection_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for disconnections (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_server::set_disconnection_callback().
		 */
		auto set_disconnection_callback(interfaces::i_websocket_server::disconnection_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for text messages (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_server::set_text_callback().
		 */
		auto set_text_callback(interfaces::i_websocket_server::text_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for binary messages (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_server::set_binary_callback().
		 */
		auto set_binary_callback(interfaces::i_websocket_server::binary_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for errors (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_server::set_error_callback().
		 */
		auto set_error_callback(interfaces::i_websocket_server::error_callback_t callback) -> void override;

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief WebSocket-specific implementation of server start.
		 * \param port The port number to listen on.
		 * \param path The WebSocket path.
		 * \return Result<void> - Success if server started, or error with code.
		 */
		auto do_start_impl(uint16_t port, std::string_view path) -> VoidResult;

		/*!
		 * \brief WebSocket-specific implementation of server stop.
		 * \return Result<void> - Success if server stopped, or error with code.
		 */
		auto do_stop_impl() -> VoidResult;

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

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Invokes the connection callback.
		 * \param conn The new connection.
		 */
		auto invoke_connection_callback(std::shared_ptr<ws_connection> conn) -> void;

		/*!
		 * \brief Invokes the disconnection callback.
		 * \param conn_id The connection ID.
		 * \param code The close code.
		 * \param reason The close reason.
		 */
		auto invoke_disconnection_callback(const std::string& conn_id,
		                                   internal::ws_close_code code,
		                                   const std::string& reason) -> void;

		/*!
		 * \brief Invokes the message callback.
		 * \param conn The connection that received the message.
		 * \param msg The received message.
		 */
		auto invoke_message_callback(std::shared_ptr<ws_connection> conn,
		                             const internal::ws_message& msg) -> void;

		/*!
		 * \brief Invokes the error callback.
		 * \param conn_id The connection ID.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(const std::string& conn_id, std::error_code ec) -> void;

		//! \brief Callback index type alias for clarity
		using callback_index = ws_server_callback;

		//! \brief Callback manager type for this server
		using callbacks_t = utils::callback_manager<
			connection_callback_t,
			disconnection_callback_t,
			message_callback_t,
			text_message_callback_t,
			binary_message_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string server_id_;                          /*!< Server identifier. */
		utils::lifecycle_manager lifecycle_;             /*!< Lifecycle state manager. */
		callbacks_t callbacks_;                          /*!< Callback manager. */

		ws_server_config config_;                        /*!< Server configuration. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_; /*!< Work guard. */
		std::unique_ptr<asio::ip::tcp::acceptor> acceptor_; /*!< TCP acceptor. */
		mutable std::mutex acceptor_mutex_;              /*!< Mutex protecting acceptor_ access. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::shared_ptr<ws_session_manager> session_mgr_; /*!< Session manager. */
	};

// =====================================================================
// Unified Pattern Type Aliases
// =====================================================================
// These aliases provide a consistent API pattern across all protocols,
// making WebSocket servers accessible via the unified template naming.
// See: unified_messaging_server.h for TCP, unified_udp_messaging_server.h for UDP.

/*!
 * \brief Type alias for WebSocket server (plain).
 *
 * Provides consistent naming with the unified template pattern.
 * WebSocket uses HTTP upgrade over TCP, optionally secured via TLS (WSS).
 *
 * \code
 * auto ws_server = std::make_shared<ws_server>("server1");
 * ws_server->start_server(8080, "/ws");
 * \endcode
 */
using ws_server = messaging_ws_server;

/*!
 * \brief Type alias for secure WebSocket server (WSS).
 *
 * WebSocket Secure (WSS) uses TLS encryption over the TCP connection.
 * Note: TLS configuration should be handled at the server setup level.
 *
 * \code
 * auto wss_server = std::make_shared<secure_ws_server>("server1");
 * wss_server->start_server(8443, "/ws");
 * \endcode
 *
 * \note Currently uses the same implementation as messaging_ws_server.
 *       TLS should be configured via server configuration options.
 */
using secure_ws_server = messaging_ws_server;

} // namespace kcenon::network::core
