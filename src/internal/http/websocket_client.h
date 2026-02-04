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

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <asio.hpp>

#include "internal/interfaces/i_websocket_client.h"
#include "internal/core/callback_indices.h"
#include "internal/websocket/websocket_protocol.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::internal
{
	class tcp_socket;
	class websocket_socket;
}

namespace kcenon::network::core
{
	/*!
	 * \struct ws_client_config
	 * \brief Configuration for WebSocket client.
	 *
	 * Provides comprehensive configuration options for WebSocket client behavior
	 * including connection parameters, timeouts, and protocol options.
	 */
	struct ws_client_config
	{
		std::string host;                                ///< Server hostname or IP
		uint16_t port = 80;                              ///< Server port
		std::string path = "/";                          ///< WebSocket path
		std::map<std::string, std::string> headers;      ///< Additional HTTP headers
		std::chrono::milliseconds connect_timeout{10000}; ///< Connection timeout
		std::chrono::milliseconds ping_interval{30000};   ///< Ping interval
		bool auto_pong = true;                            ///< Auto-respond to pings
		bool auto_reconnect = false;                      ///< Auto-reconnect on disconnect
		size_t max_message_size = 10 * 1024 * 1024;      ///< Max message size (10MB)
	};

	/*!
	 * \class messaging_ws_client
	 * \brief High-level WebSocket client with automatic connection management.
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 * It also implements the i_websocket_client interface for composition-based usage.
	 *
	 * It handles:
	 * - Asynchronous connection and handshake
	 * - Message sending and receiving (text and binary)
	 * - Ping/pong keepalive
	 * - Graceful disconnection
	 * - Event-driven callbacks
	 *
	 * Thread Safety:
	 * - All public methods are thread-safe
	 * - Callbacks are invoked from the internal I/O thread
	 * - Message sending can be called from any thread
	 *
	 * ### Interface Compliance
	 * This class implements interfaces::i_websocket_client for composition-based usage.
	 *
	 * Usage Example:
	 * \code
	 * auto client = std::make_shared<messaging_ws_client>("my_client");
	 *
	 * client->set_connected_callback([]() {
	 *     std::cout << "Connected!" << std::endl;
	 * });
	 *
	 * client->set_text_message_callback([](const std::string& msg) {
	 *     std::cout << "Received: " << msg << std::endl;
	 * });
	 *
	 * client->start_client("example.com", 80, "/ws");
	 * client->send_text("Hello, WebSocket!");
	 * \endcode
	 */
	class messaging_ws_client
		: public std::enable_shared_from_this<messaging_ws_client>
		, public interfaces::i_websocket_client
	{
	public:
		//! \brief Callback type for WebSocket messages
		using message_callback_t = std::function<void(const internal::ws_message&)>;
		//! \brief Callback type for text messages
		using text_message_callback_t = std::function<void(const std::string&)>;
		//! \brief Callback type for binary messages
		using binary_message_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection with close code
		using disconnected_callback_t = std::function<void(internal::ws_close_code, const std::string&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a WebSocket client.
		 * \param client_id A unique identifier for this client instance.
		 */
		explicit messaging_ws_client(std::string_view client_id);

		/*!
		 * \brief Destructor.
		 * Automatically stops the client if still running.
		 */
		~messaging_ws_client() noexcept override;

		// Non-copyable, non-movable
		messaging_ws_client(const messaging_ws_client&) = delete;
		messaging_ws_client& operator=(const messaging_ws_client&) = delete;
		messaging_ws_client(messaging_ws_client&&) = delete;
		messaging_ws_client& operator=(messaging_ws_client&&) = delete;

		// ========================================================================
		// Lifecycle Management
		// ========================================================================

		/*!
		 * \brief Starts the client with full configuration.
		 * \param config The client configuration.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto start_client(const ws_client_config& config) -> VoidResult;

		/*!
		 * \brief Starts the client by connecting to the WebSocket server.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number.
		 * \param path The WebSocket path (default: "/").
		 * \return Result<void> - Success if client started, or error with code.
		 */
		[[nodiscard]] auto start_client(std::string_view host, uint16_t port,
		                                std::string_view path = "/") -> VoidResult;

		/*!
		 * \brief Stops the client and releases all resources.
		 * \return Result<void> - Success if client stopped, or error with code.
		 */
		[[nodiscard]] auto stop_client() -> VoidResult;

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string.
		 */
		[[nodiscard]] auto client_id() const -> const std::string&;

		/*!
		 * \brief Sends a ping frame.
		 * \param payload Optional payload data (max 125 bytes).
		 * \return VoidResult indicating success.
		 */
		[[nodiscard]] auto send_ping(std::vector<uint8_t>&& payload = {}) -> VoidResult;

		// ========================================================================
		// i_network_component interface implementation
		// ========================================================================

		/*!
		 * \brief Checks if the client is currently running.
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
		// i_websocket_client interface implementation
		// ========================================================================

		/*!
		 * \brief Starts the WebSocket client connecting to the specified endpoint.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \param path The WebSocket path (default: "/").
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_client::start(). Delegates to start_client().
		 */
		[[nodiscard]] auto start(
			std::string_view host,
			uint16_t port,
			std::string_view path = "/") -> VoidResult override;

		/*!
		 * \brief Stops the WebSocket client.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_client::stop(). Delegates to stop_client().
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Checks if the WebSocket connection is established.
		 * \return true if connected, false otherwise.
		 *
		 * Implements i_websocket_client::is_connected().
		 */
		[[nodiscard]] auto is_connected() const -> bool override;

		/*!
		 * \brief Sends a text message (interface version).
		 * \param message The text message to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_client::send_text().
		 */
		[[nodiscard]] auto send_text(
			std::string&& message,
			interfaces::i_websocket_client::send_callback_t handler = nullptr) -> VoidResult override;

		/*!
		 * \brief Sends a binary message (interface version).
		 * \param data The binary data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_client::send_binary().
		 */
		[[nodiscard]] auto send_binary(
			std::vector<uint8_t>&& data,
			interfaces::i_websocket_client::send_callback_t handler = nullptr) -> VoidResult override;

		/*!
		 * \brief Sends a ping frame (interface version).
		 * \param payload Optional payload data (max 125 bytes).
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_client::ping(). Delegates to send_ping().
		 */
		[[nodiscard]] auto ping(std::vector<uint8_t>&& payload = {}) -> VoidResult override;

		/*!
		 * \brief Closes the WebSocket connection gracefully (interface version).
		 * \param code The close status code.
		 * \param reason Optional human-readable reason.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_websocket_client::close().
		 */
		[[nodiscard]] auto close(
			uint16_t code = 1000,
			std::string_view reason = "") -> VoidResult override;

		/*!
		 * \brief Sets the callback for text messages (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_client::set_text_callback().
		 */
		auto set_text_callback(interfaces::i_websocket_client::text_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for binary messages (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_client::set_binary_callback().
		 */
		auto set_binary_callback(interfaces::i_websocket_client::binary_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for connection established (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_client::set_connected_callback().
		 */
		auto set_connected_callback(interfaces::i_websocket_client::connected_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for disconnection (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_client::set_disconnected_callback().
		 */
		auto set_disconnected_callback(interfaces::i_websocket_client::disconnected_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for errors (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_websocket_client::set_error_callback().
		 */
		auto set_error_callback(interfaces::i_websocket_client::error_callback_t callback) -> void override;

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief WebSocket-specific implementation of client start.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \param path The WebSocket path.
		 * \return Result<void> - Success if client started, or error with code.
		 */
		auto do_start_impl(std::string_view host, uint16_t port, std::string_view path) -> VoidResult;

		/*!
		 * \brief WebSocket-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped, or error with code.
		 */
		auto do_stop_impl() -> VoidResult;

		/*!
		 * \brief Initiates async connection to the server.
		 */
		auto do_connect() -> void;

		/*!
		 * \brief Handles received WebSocket messages.
		 */
		auto on_message(const internal::ws_message& msg) -> void;

		/*!
		 * \brief Handles ping frames.
		 */
		auto on_ping(const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles connection close.
		 */
		auto on_close(internal::ws_close_code code, const std::string& reason) -> void;

		/*!
		 * \brief Handles errors.
		 */
		auto on_error(std::error_code ec) -> void;

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Invokes the message callback.
		 * \param msg The received message.
		 */
		auto invoke_message_callback(const internal::ws_message& msg) -> void;

		/*!
		 * \brief Invokes the connected callback.
		 */
		auto invoke_connected_callback() -> void;

		/*!
		 * \brief Invokes the disconnected callback.
		 * \param code The close code.
		 * \param reason The close reason.
		 */
		auto invoke_disconnected_callback(internal::ws_close_code code,
		                                  const std::string& reason) -> void;

		/*!
		 * \brief Invokes the error callback.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

		//! \brief Callback index type alias for clarity
		using callback_index = ws_client_callback;

		//! \brief Callback manager type for this client
		using callbacks_t = utils::callback_manager<
			message_callback_t,
			text_message_callback_t,
			binary_message_callback_t,
			connected_callback_t,
			disconnected_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string client_id_;                          /*!< Client identifier. */
		utils::lifecycle_manager lifecycle_;             /*!< Lifecycle state manager. */
		callbacks_t callbacks_;                          /*!< Callback manager. */
		std::atomic<bool> is_connected_{false};          /*!< True if connected to remote. */

		ws_client_config config_;                        /*!< Client configuration. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_; /*!< Work guard. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::shared_ptr<internal::websocket_socket> ws_socket_; /*!< WebSocket wrapper. */
		std::mutex ws_socket_mutex_;                     /*!< Protects ws_socket_. */
	};

// =====================================================================
// Unified Pattern Type Aliases
// =====================================================================
// These aliases provide a consistent API pattern across all protocols,
// making WebSocket clients accessible via the unified template naming.
// See: unified_messaging_client.h for TCP, unified_udp_messaging_client.h for UDP.

/*!
 * \brief Type alias for WebSocket client (plain).
 *
 * Provides consistent naming with the unified template pattern.
 * WebSocket uses HTTP upgrade over TCP, optionally secured via TLS (WSS).
 *
 * \code
 * auto ws_client = std::make_shared<ws_client>("client1");
 * ws_client->start_client("localhost", 80, "/ws");
 * \endcode
 */
using ws_client = messaging_ws_client;

/*!
 * \brief Type alias for secure WebSocket client (WSS).
 *
 * WebSocket Secure (WSS) uses TLS encryption over the TCP connection.
 * Note: The actual TLS configuration is handled at the connection level.
 *
 * \code
 * auto wss_client = std::make_shared<secure_ws_client>("client1");
 * wss_client->start_client("localhost", 443, "/ws");
 * \endcode
 *
 * \note Currently uses the same implementation as messaging_ws_client.
 *       TLS is negotiated via the wss:// scheme or port configuration.
 */
using secure_ws_client = messaging_ws_client;

} // namespace kcenon::network::core
