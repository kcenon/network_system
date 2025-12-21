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

#include "kcenon/network/internal/websocket_protocol.h"
#include "kcenon/network/utils/result_types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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
	 * This class provides a simple, easy-to-use interface for WebSocket client
	 * applications. It handles:
	 * - Asynchronous connection and handshake
	 * - Message sending and receiving (text and binary)
	 * - Ping/pong keepalive
	 * - Graceful disconnection
	 * - Optional auto-reconnect
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
		: public std::enable_shared_from_this<messaging_ws_client>
	{
	public:
		/*!
		 * \brief Constructs a WebSocket client.
		 *
		 * \param client_id A unique identifier for this client instance
		 */
		explicit messaging_ws_client(const std::string& client_id);

		/*!
		 * \brief Destructor.
		 *
		 * Automatically stops the client if still running.
		 */
		~messaging_ws_client();

		/*!
		 * \brief Starts the client with full configuration.
		 *
		 * Initiates connection to the WebSocket server with the provided
		 * configuration. Returns immediately; connection happens asynchronously.
		 * Use set_connected_callback() to be notified when connected.
		 *
		 * \param config The client configuration
		 * \return VoidResult indicating success or error
		 */
		auto start_client(const ws_client_config& config) -> VoidResult;

		/*!
		 * \brief Starts the client with simple parameters.
		 *
		 * Convenience method that uses default configuration with specified
		 * host, port, and path.
		 *
		 * \param host Server hostname or IP address
		 * \param port Server port number
		 * \param path WebSocket path (default: "/")
		 * \return VoidResult indicating success or error
		 */
		auto start_client(std::string_view host, uint16_t port,
						  std::string_view path = "/") -> VoidResult;

		/*!
		 * \brief Stops the client.
		 *
		 * Initiates graceful shutdown by sending a close frame and stopping
		 * the I/O thread. Returns immediately; use wait_for_stop() to block
		 * until fully stopped.
		 *
		 * \return VoidResult indicating success or error
		 */
		auto stop_client() -> VoidResult;

		/*!
		 * \brief Waits for the client to stop completely.
		 *
		 * Blocks the calling thread until the client has fully stopped.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Checks if the client is running.
		 *
		 * \return True if the I/O thread is active
		 */
		auto is_running() const -> bool;

		/*!
		 * \brief Checks if the WebSocket connection is established.
		 *
		 * \return True if connected and handshake completed
		 */
		auto is_connected() const -> bool;

		/*!
		 * \brief Sends a text message.
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
		 * \brief Sends a binary message.
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
		 * \brief Sends a ping frame.
		 *
		 * Sends a ping to check connection liveness. The peer should respond
		 * with a pong frame.
		 *
		 * \param payload Optional payload data (max 125 bytes)
		 * \return VoidResult indicating success
		 */
		auto send_ping(std::vector<uint8_t>&& payload = {}) -> VoidResult;

		/*!
		 * \brief Closes the connection gracefully.
		 *
		 * Sends a close frame with the specified code and reason, then waits
		 * for the server's close response.
		 *
		 * \param code The close status code (default: normal)
		 * \param reason Optional human-readable reason
		 * \return VoidResult indicating success
		 */
		auto close(internal::ws_close_code code = internal::ws_close_code::normal,
				   const std::string& reason = "") -> VoidResult;

		/*!
		 * \brief Sets the callback for all message types.
		 *
		 * \param callback Function called when a message is received
		 */
		auto set_message_callback(
			std::function<void(const internal::ws_message&)> callback) -> void;

		/*!
		 * \brief Sets the callback for text messages only.
		 *
		 * \param callback Function called when a text message is received
		 */
		auto set_text_message_callback(
			std::function<void(const std::string&)> callback) -> void;

		/*!
		 * \brief Sets the callback for binary messages only.
		 *
		 * \param callback Function called when a binary message is received
		 */
		auto set_binary_message_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for connection established.
		 *
		 * \param callback Function called when connection is established
		 */
		auto set_connected_callback(std::function<void()> callback) -> void;

		/*!
		 * \brief Sets the callback for disconnection.
		 *
		 * \param callback Function called when connection is closed
		 */
		auto set_disconnected_callback(
			std::function<void(internal::ws_close_code, const std::string&)> callback)
			-> void;

		/*!
		 * \brief Sets the callback for errors.
		 *
		 * \param callback Function called when an error occurs
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback)
			-> void;

		/*!
		 * \brief Gets the client ID.
		 *
		 * \return The client identifier
		 */
		auto client_id() const -> const std::string&;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};

} // namespace kcenon::network::core
