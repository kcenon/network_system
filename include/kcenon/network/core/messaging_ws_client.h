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

#include "kcenon/network/core/messaging_ws_client_base.h"
#include "kcenon/network/internal/websocket_protocol.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/integration/thread_integration.h"

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
	 * This class inherits from messaging_ws_client_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
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
		: public messaging_ws_client_base<messaging_ws_client>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_ws_client_base<messaging_ws_client>;

		/*!
		 * \brief Constructs a WebSocket client.
		 * \param client_id A unique identifier for this client instance.
		 */
		explicit messaging_ws_client(std::string_view client_id);

		/*!
		 * \brief Destructor.
		 * Automatically stops the client if still running (handled by base class).
		 */
		~messaging_ws_client() noexcept override = default;

		/*!
		 * \brief Starts the client with full configuration.
		 * \param config The client configuration.
		 * \return VoidResult indicating success or error.
		 */
		auto start_client(const ws_client_config& config) -> VoidResult;

		// start_client(host, port, path), stop_client(), wait_for_stop(),
		// is_running(), is_connected(), client_id() are provided by base class

		/*!
		 * \brief Sends a text message.
		 * \param message The text message to send.
		 * \param handler Optional callback invoked with send result.
		 * \return VoidResult indicating validation success.
		 */
		auto send_text(std::string&& message,
					   std::function<void(std::error_code, std::size_t)> handler = nullptr) -> VoidResult;

		/*!
		 * \brief Sends a binary message.
		 * \param data The binary data to send.
		 * \param handler Optional callback invoked with send result.
		 * \return VoidResult indicating success.
		 */
		auto send_binary(std::vector<uint8_t>&& data,
						 std::function<void(std::error_code, std::size_t)> handler = nullptr) -> VoidResult;

		/*!
		 * \brief Sends a ping frame.
		 * \param payload Optional payload data (max 125 bytes).
		 * \return VoidResult indicating success.
		 */
		auto send_ping(std::vector<uint8_t>&& payload = {}) -> VoidResult;

		/*!
		 * \brief Closes the connection gracefully.
		 * \param code The close status code (default: normal).
		 * \param reason Optional human-readable reason.
		 * \return VoidResult indicating success.
		 */
		auto close(internal::ws_close_code code = internal::ws_close_code::normal,
				   const std::string& reason = "") -> VoidResult;

	protected:
		/*!
		 * \brief WebSocket-specific implementation of client start.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \param path The WebSocket path.
		 * \return Result<void> - Success if client started, or error with code.
		 *
		 * Called by base class start_client() after common validation.
		 */
		auto do_start(std::string_view host, uint16_t port, std::string_view path) -> VoidResult;

		/*!
		 * \brief WebSocket-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped, or error with code.
		 *
		 * Called by base class stop_client() after common cleanup.
		 */
		auto do_stop() -> VoidResult;

	private:
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

	private:
		ws_client_config config_;                        /*!< Client configuration. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_; /*!< Work guard. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::shared_ptr<internal::websocket_socket> ws_socket_; /*!< WebSocket wrapper. */
		std::mutex ws_socket_mutex_;                     /*!< Protects ws_socket_. */
	};

} // namespace kcenon::network::core
