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

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace network_system::internal
{
	// Forward declaration
	class tcp_socket;

	/*!
	 * \enum ws_state
	 * \brief WebSocket connection state (RFC 6455 Section 7).
	 *
	 * Tracks the lifecycle of a WebSocket connection from initial connection
	 * through the closing handshake.
	 */
	enum class ws_state
	{
		connecting, ///< Handshake in progress
		open,       ///< Connection established and ready
		closing,    ///< Close handshake initiated
		closed      ///< Connection closed
	};

	/*!
	 * \class websocket_socket
	 * \brief WebSocket framing layer built on top of tcp_socket.
	 *
	 * This class wraps an existing tcp_socket and provides WebSocket protocol
	 * framing. The underlying tcp_socket handles all transport I/O operations,
	 * while websocket_socket handles:
	 * - WebSocket handshake (client and server)
	 * - Frame encoding/decoding via websocket_protocol
	 * - Message fragmentation and reassembly
	 * - Connection state management
	 * - Ping/pong keepalive
	 * - Close handshake
	 *
	 * Design rationale:
	 * - Reuses tcp_socket infrastructure (no I/O duplication)
	 * - Maintains consistency with existing network_system patterns
	 * - Separates concerns: tcp_socket = transport, websocket_socket = framing
	 */
	class websocket_socket : public std::enable_shared_from_this<websocket_socket>
	{
	public:
		/*!
		 * \brief Constructs a websocket_socket wrapping an existing tcp_socket.
		 *
		 * \param socket The underlying TCP socket (must be connected)
		 * \param is_client True if this is a client endpoint (applies masking)
		 */
		websocket_socket(std::shared_ptr<tcp_socket> socket, bool is_client);

		/*!
		 * \brief Destructor.
		 *
		 * Stops reading and cleans up resources.
		 */
		~websocket_socket();

		/*!
		 * \brief Performs WebSocket client handshake (RFC 6455 Section 4.1).
		 *
		 * Sends an HTTP/1.1 Upgrade request and validates the server response.
		 * The handler is called with success/failure status.
		 *
		 * \param host The server hostname (for Host header)
		 * \param path The request path (e.g., "/chat")
		 * \param port The server port (for Host header)
		 * \param handler Callback invoked with handshake result
		 */
		auto async_handshake(const std::string& host, const std::string& path,
							 uint16_t port,
							 std::function<void(std::error_code)> handler) -> void;

		/*!
		 * \brief Accepts WebSocket server handshake (RFC 6455 Section 4.2).
		 *
		 * Reads the client's HTTP/1.1 Upgrade request and sends the acceptance
		 * response. The handler is called with success/failure status.
		 *
		 * \param handler Callback invoked with handshake result
		 */
		auto async_accept(std::function<void(std::error_code)> handler) -> void;

		/*!
		 * \brief Starts reading WebSocket frames from the underlying socket.
		 *
		 * Must be called after successful handshake to begin receiving messages.
		 */
		auto start_read() -> void;

		/*!
		 * \brief Sends a text message (UTF-8 encoded).
		 *
		 * Uses move semantics for zero-copy operation. The message is validated
		 * for UTF-8 encoding and encoded as a WebSocket text frame.
		 *
		 * \param message The text message to send
		 * \param handler Callback invoked with send result
		 * \return VoidResult indicating validation success
		 */
		auto async_send_text(std::string&& message,
							 std::function<void(std::error_code, std::size_t)> handler)
			-> VoidResult;

		/*!
		 * \brief Sends a binary message.
		 *
		 * Uses move semantics for zero-copy operation. The data is encoded
		 * as a WebSocket binary frame.
		 *
		 * \param data The binary data to send
		 * \param handler Callback invoked with send result
		 * \return VoidResult indicating success
		 */
		auto async_send_binary(std::vector<uint8_t>&& data,
							   std::function<void(std::error_code, std::size_t)> handler)
			-> VoidResult;

		/*!
		 * \brief Sends a ping control frame.
		 *
		 * Ping frames are used to check connection liveness.
		 * The peer should respond with a pong frame.
		 *
		 * \param payload Optional payload data (max 125 bytes)
		 * \param handler Callback invoked with send result
		 */
		auto async_send_ping(std::vector<uint8_t> payload,
							 std::function<void(std::error_code)> handler) -> void;

		/*!
		 * \brief Initiates the WebSocket closing handshake.
		 *
		 * Sends a close frame with the specified status code and reason.
		 * The connection state transitions to closing.
		 *
		 * \param code The close status code (RFC 6455 Section 7.4)
		 * \param reason Optional human-readable reason (UTF-8, max ~123 bytes)
		 * \param handler Callback invoked when close frame is sent
		 */
		auto async_close(ws_close_code code, const std::string& reason,
						 std::function<void(std::error_code)> handler) -> void;

		/*!
		 * \brief Gets the current connection state.
		 *
		 * \return The current ws_state
		 */
		auto state() const -> ws_state;

		/*!
		 * \brief Checks if the connection is open and ready.
		 *
		 * \return True if state is ws_state::open
		 */
		auto is_open() const -> bool;

		/*!
		 * \brief Sets the callback for received messages.
		 *
		 * This callback is invoked when a complete text or binary message
		 * has been received and reassembled.
		 *
		 * \param callback The callback function
		 */
		auto set_message_callback(std::function<void(const ws_message&)> callback)
			-> void;

		/*!
		 * \brief Sets the callback for received ping frames.
		 *
		 * This callback is invoked when a ping frame is received.
		 * The application may choose to send a pong response.
		 *
		 * \param callback The callback function
		 */
		auto set_ping_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for received pong frames.
		 *
		 * This callback is invoked when a pong frame is received
		 * (typically in response to a ping).
		 *
		 * \param callback The callback function
		 */
		auto set_pong_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for received close frames.
		 *
		 * This callback is invoked when a close frame is received.
		 * The application should respond with a close frame if it hasn't already.
		 *
		 * \param callback The callback function
		 */
		auto set_close_callback(
			std::function<void(ws_close_code, const std::string&)> callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 *
		 * This callback is invoked when an error occurs on the underlying socket.
		 *
		 * \param callback The callback function
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

	private:
		// Underlying TCP socket (reuses existing infrastructure)
		std::shared_ptr<tcp_socket> tcp_socket_;

		// WebSocket protocol state machine
		websocket_protocol protocol_;
		std::atomic<ws_state> state_{ws_state::connecting};

		// Callbacks
		std::mutex callback_mutex_;
		std::function<void(const ws_message&)> message_callback_;
		std::function<void(const std::vector<uint8_t>&)> ping_callback_;
		std::function<void(const std::vector<uint8_t>&)> pong_callback_;
		std::function<void(ws_close_code, const std::string&)> close_callback_;
		std::function<void(std::error_code)> error_callback_;

		/*!
		 * \brief Called by tcp_socket when data is received.
		 *
		 * Feeds the data into the WebSocket protocol decoder.
		 *
		 * \param data The received raw bytes
		 */
		auto on_tcp_receive(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Called by tcp_socket when an error occurs.
		 *
		 * Propagates the error to the application.
		 *
		 * \param ec The error code
		 */
		auto on_tcp_error(std::error_code ec) -> void;

		/*!
		 * \brief Handles a decoded message from the protocol layer.
		 *
		 * Invokes the message callback.
		 *
		 * \param msg The decoded message
		 */
		auto handle_protocol_message(const ws_message& msg) -> void;

		/*!
		 * \brief Handles a ping frame from the protocol layer.
		 *
		 * Invokes the ping callback (application may send pong).
		 *
		 * \param payload The ping payload
		 */
		auto handle_protocol_ping(const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles a pong frame from the protocol layer.
		 *
		 * Invokes the pong callback.
		 *
		 * \param payload The pong payload
		 */
		auto handle_protocol_pong(const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles a close frame from the protocol layer.
		 *
		 * Updates connection state and invokes the close callback.
		 *
		 * \param code The close status code
		 * \param reason The close reason string
		 */
		auto handle_protocol_close(ws_close_code code, const std::string& reason)
			-> void;
	};

} // namespace network_system::internal
