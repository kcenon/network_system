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
#include "kcenon/network/interfaces/i_websocket_server.h"
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

		// ========================================================================
		// Legacy API (maintained for backward compatibility)
		// ========================================================================

		/*!
		 * \brief Sends a text message to the client (legacy version).
		 * \param message The text message to send.
		 * \param handler Optional callback invoked with send result.
		 * \return VoidResult indicating validation success.
		 *
		 * \deprecated Use send_text(std::string&&) for interface compliance.
		 */
		auto send_text(std::string&& message,
					   std::function<void(std::error_code, std::size_t)> handler) -> VoidResult;

		/*!
		 * \brief Sends a binary message to the client (legacy version).
		 * \param data The binary data to send.
		 * \param handler Optional callback invoked with send result.
		 * \return VoidResult indicating success.
		 *
		 * \deprecated Use send_binary(std::vector<uint8_t>&&) for interface compliance.
		 */
		auto send_binary(std::vector<uint8_t>&& data,
						 std::function<void(std::error_code, std::size_t)> handler) -> VoidResult;

		/*!
		 * \brief Closes the connection gracefully (legacy version).
		 * \param code The close status code (default: normal).
		 * \param reason Optional human-readable reason.
		 * \return VoidResult indicating success.
		 *
		 * \deprecated Use close(uint16_t, std::string_view) for interface compliance.
		 */
		auto close(internal::ws_close_code code,
				   const std::string& reason) -> VoidResult;

		/*!
		 * \brief Gets the connection ID.
		 * \return The unique connection identifier.
		 *
		 * \deprecated Use id() for interface compliance.
		 */
		auto connection_id() const -> const std::string&;

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
	 * This class inherits from messaging_ws_server_base using the CRTP pattern
	 * and implements the i_websocket_server interface for composition-based usage.
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
		: public messaging_ws_server_base<messaging_ws_server>
		, public interfaces::i_websocket_server
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

		//! \brief Bring base class start_server overloads into scope
		using messaging_ws_server_base::start_server;

		// stop_server(), wait_for_stop(), is_running(), server_id()
		// are provided by base class

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
		// i_websocket_server interface implementation
		// ========================================================================

		/*!
		 * \brief Checks if the server is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override {
			return messaging_ws_server_base::is_running();
		}

		/*!
		 * \brief Blocks until stop() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override {
			messaging_ws_server_base::wait_for_stop();
		}

		/*!
		 * \brief Starts the WebSocket server on the specified port.
		 * \param port The port number to listen on.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_server::start(). Delegates to start_server().
		 */
		[[nodiscard]] auto start(uint16_t port) -> VoidResult override {
			return start_server(port);
		}

		/*!
		 * \brief Stops the WebSocket server.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_server::stop(). Delegates to stop_server().
		 */
		[[nodiscard]] auto stop() -> VoidResult override {
			return stop_server();
		}

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

		// ========================================================================
		// Legacy API (maintained for backward compatibility)
		// ========================================================================

		/*!
		 * \brief Sets the callback for new connections (legacy version).
		 * \param callback The callback function.
		 *
		 * This overload maintains backward compatibility.
		 */
		using messaging_ws_server_base::set_connection_callback;

		/*!
		 * \brief Sets the callback for disconnections (legacy version).
		 * \param callback The callback function.
		 *
		 * This overload maintains backward compatibility.
		 */
		using messaging_ws_server_base::set_disconnection_callback;

		/*!
		 * \brief Sets the callback for text messages (legacy version).
		 * \param callback The callback function.
		 *
		 * This overload maintains backward compatibility.
		 */
		using messaging_ws_server_base::set_text_message_callback;

		/*!
		 * \brief Sets the callback for binary messages (legacy version).
		 * \param callback The callback function.
		 *
		 * This overload maintains backward compatibility.
		 */
		using messaging_ws_server_base::set_binary_message_callback;

		/*!
		 * \brief Sets the callback for errors (legacy version).
		 * \param callback The callback function.
		 *
		 * This overload maintains backward compatibility.
		 */
		using messaging_ws_server_base::set_error_callback;

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
		mutable std::mutex acceptor_mutex_;              /*!< Mutex protecting acceptor_ access. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::shared_ptr<ws_session_manager> session_mgr_; /*!< Session manager. */
	};

} // namespace kcenon::network::core
