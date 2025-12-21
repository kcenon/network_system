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

#include "kcenon/network/internal/websocket_frame.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace kcenon::network::internal
{
	/*!
	 * \enum ws_message_type
	 * \brief Type of WebSocket message.
	 *
	 * WebSocket supports two types of data messages: text (UTF-8) and binary.
	 */
	enum class ws_message_type
	{
		text,   ///< Text message (UTF-8 encoded)
		binary  ///< Binary message
	};

	/*!
	 * \struct ws_message
	 * \brief Represents a complete WebSocket message.
	 *
	 * A message may consist of one or more frames that have been reassembled.
	 * Text messages are UTF-8 encoded.
	 */
	struct ws_message
	{
		ws_message_type type;      ///< Message type
		std::vector<uint8_t> data; ///< Message payload

		/*!
		 * \brief Converts message data to string (for text messages).
		 * \return String representation of the data
		 */
		auto as_text() const -> std::string;

		/*!
		 * \brief Returns message data as binary (reference).
		 * \return Reference to the binary data
		 */
		auto as_binary() const -> const std::vector<uint8_t>&;
	};

	/*!
	 * \enum ws_close_code
	 * \brief WebSocket close status codes (RFC 6455 Section 7.4).
	 *
	 * These codes indicate the reason for closing the WebSocket connection.
	 */
	enum class ws_close_code : uint16_t
	{
		normal = 1000,            ///< Normal closure
		going_away = 1001,        ///< Endpoint is going away
		protocol_error = 1002,    ///< Protocol error
		unsupported_data = 1003,  ///< Unsupported data type
		invalid_frame = 1007,     ///< Invalid frame payload data
		policy_violation = 1008,  ///< Policy violation
		message_too_big = 1009,   ///< Message too large
		internal_error = 1011     ///< Internal server error
	};

	/*!
	 * \class websocket_protocol
	 * \brief WebSocket protocol handler for message processing.
	 *
	 * This class handles the WebSocket protocol state machine including:
	 * - Frame processing and message reassembly
	 * - Fragmentation handling
	 * - Control frame processing (ping/pong/close)
	 * - Message callbacks
	 */
	class websocket_protocol
	{
	public:
		/*!
		 * \brief Constructs a WebSocket protocol handler.
		 *
		 * \param is_client True if this is a client endpoint (applies masking)
		 */
		explicit websocket_protocol(bool is_client);

		/*!
		 * \brief Processes incoming WebSocket data.
		 *
		 * Parses frames, handles fragmentation, and invokes appropriate callbacks.
		 * This method should be called when data is received from the network.
		 *
		 * ### Zero-Copy Performance
		 * Accepts data as a non-owning span view, avoiding unnecessary copies.
		 * The data is copied once into the internal buffer for frame processing.
		 *
		 * ### Lifetime Contract
		 * - The span must remain valid for the duration of this call.
		 * - After the call returns, the span may be invalidated.
		 *
		 * \param data The incoming data buffer as a view
		 */
		auto process_data(std::span<const uint8_t> data) -> void;

		/*!
		 * \brief Creates a text message frame.
		 *
		 * Encodes the text as a WebSocket text frame with UTF-8 validation.
		 * Uses move semantics for zero-copy operation.
		 *
		 * \param text The text message to send
		 * \return Encoded WebSocket frame ready for transmission
		 */
		auto create_text_message(std::string&& text) -> std::vector<uint8_t>;

		/*!
		 * \brief Creates a binary message frame.
		 *
		 * Encodes the binary data as a WebSocket binary frame.
		 * Uses move semantics for zero-copy operation.
		 *
		 * \param data The binary data to send
		 * \return Encoded WebSocket frame ready for transmission
		 */
		auto create_binary_message(std::vector<uint8_t>&& data)
			-> std::vector<uint8_t>;

		/*!
		 * \brief Creates a ping control frame.
		 *
		 * Ping frames are used to check connection liveness.
		 * The peer should respond with a pong frame.
		 *
		 * \param payload Optional payload data (typically empty)
		 * \return Encoded ping frame
		 */
		auto create_ping(std::vector<uint8_t>&& payload = {})
			-> std::vector<uint8_t>;

		/*!
		 * \brief Creates a pong control frame.
		 *
		 * Pong frames are sent in response to ping frames.
		 *
		 * \param payload Payload data (should match the ping payload)
		 * \return Encoded pong frame
		 */
		auto create_pong(std::vector<uint8_t>&& payload = {})
			-> std::vector<uint8_t>;

		/*!
		 * \brief Creates a close control frame.
		 *
		 * Initiates the WebSocket closing handshake.
		 *
		 * \param code The close status code
		 * \param reason Optional human-readable reason (UTF-8)
		 * \return Encoded close frame
		 */
		auto create_close(ws_close_code code, std::string&& reason = "")
			-> std::vector<uint8_t>;

		/*!
		 * \brief Sets the callback for data messages.
		 *
		 * This callback is invoked when a complete text or binary message
		 * has been received and reassembled.
		 *
		 * \param callback The callback function
		 */
		auto set_message_callback(std::function<void(const ws_message&)> callback)
			-> void;

		/*!
		 * \brief Sets the callback for ping frames.
		 *
		 * This callback is invoked when a ping frame is received.
		 * The application may choose to send a pong response.
		 *
		 * \param callback The callback function
		 */
		auto set_ping_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for pong frames.
		 *
		 * This callback is invoked when a pong frame is received
		 * (typically in response to a ping).
		 *
		 * \param callback The callback function
		 */
		auto set_pong_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for close frames.
		 *
		 * This callback is invoked when a close frame is received.
		 * The application should respond with a close frame if it hasn't already.
		 *
		 * \param callback The callback function
		 */
		auto set_close_callback(
			std::function<void(ws_close_code, const std::string&)> callback) -> void;

	private:
		bool is_client_;                            ///< True if client (applies masking)
		std::vector<uint8_t> buffer_;               ///< Incoming data buffer
		std::vector<uint8_t> fragmented_message_;   ///< Reassembly buffer for fragmented messages
		ws_opcode fragmented_type_;                 ///< Type of fragmented message in progress

		// Callbacks
		std::function<void(const ws_message&)> message_callback_;
		std::function<void(const std::vector<uint8_t>&)> ping_callback_;
		std::function<void(const std::vector<uint8_t>&)> pong_callback_;
		std::function<void(ws_close_code, const std::string&)> close_callback_;

		/*!
		 * \brief Processes incoming frames from the buffer.
		 *
		 * Attempts to parse and handle frames from the accumulated buffer.
		 * May invoke callbacks for complete messages.
		 */
		auto process_frames() -> void;

		/*!
		 * \brief Handles a data frame (text or binary).
		 *
		 * Manages fragmentation and reassembly.
		 *
		 * \param header The frame header
		 * \param payload The frame payload
		 */
		auto handle_data_frame(const ws_frame_header& header,
							   const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles a control frame (ping, pong, close).
		 *
		 * Control frames must not be fragmented.
		 *
		 * \param header The frame header
		 * \param payload The frame payload
		 */
		auto handle_control_frame(const ws_frame_header& header,
								  const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles a ping frame.
		 *
		 * Invokes the ping callback if set.
		 *
		 * \param payload The ping payload
		 */
		auto handle_ping(const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles a pong frame.
		 *
		 * Invokes the pong callback if set.
		 *
		 * \param payload The pong payload
		 */
		auto handle_pong(const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Handles a close frame.
		 *
		 * Parses the close code and reason, then invokes the close callback.
		 *
		 * \param payload The close frame payload
		 */
		auto handle_close(const std::vector<uint8_t>& payload) -> void;

		/*!
		 * \brief Validates UTF-8 encoding.
		 *
		 * Checks if the data is valid UTF-8 (required for text messages).
		 *
		 * \param data The data to validate
		 * \return True if valid UTF-8, false otherwise
		 */
		static auto is_valid_utf8(const std::vector<uint8_t>& data) -> bool;
	};

} // namespace kcenon::network::internal
