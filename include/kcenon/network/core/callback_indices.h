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

#include <cstddef>
#include <type_traits>

namespace kcenon::network {

/**
 * \brief Callback indices for messaging_client and secure_messaging_client.
 *
 * These clients use callbacks for: receive, connected, disconnected, error.
 */
enum class tcp_client_callback : std::size_t {
	receive = 0,
	connected = 1,
	disconnected = 2,
	error = 3
};

/**
 * \brief Callback indices for messaging_server and secure_messaging_server.
 *
 * These servers use callbacks for: connection, disconnection, receive, error.
 */
enum class tcp_server_callback : std::size_t {
	connection = 0,
	disconnection = 1,
	receive = 2,
	error = 3
};

/**
 * \brief Callback indices for messaging_udp_client.
 *
 * UDP client uses callbacks for: receive, error.
 */
enum class udp_client_callback : std::size_t {
	receive = 0,
	error = 1
};

/**
 * \brief Callback indices for secure_messaging_udp_client.
 *
 * Secure UDP client uses callbacks for: receive, connected, disconnected, error.
 */
enum class secure_udp_client_callback : std::size_t {
	receive = 0,
	connected = 1,
	disconnected = 2,
	error = 3
};

/**
 * \brief Callback indices for messaging_udp_server.
 *
 * UDP server uses callbacks for: receive, error.
 */
enum class udp_server_callback : std::size_t {
	receive = 0,
	error = 1
};

/**
 * \brief Callback indices for unified_udp_messaging_client.
 *
 * Unified UDP client uses callbacks for: receive, connected, disconnected, error.
 * The connected/disconnected callbacks are used for DTLS handshake completion.
 * For plain UDP, connected is called immediately after start.
 */
enum class unified_udp_client_callback : std::size_t {
	receive = 0,
	connected = 1,
	disconnected = 2,
	error = 3
};

/**
 * \brief Callback indices for unified_udp_messaging_server.
 *
 * Unified UDP server uses callbacks for: receive, client_connected,
 * client_disconnected, error.
 * The client_connected/client_disconnected callbacks are used for DTLS sessions.
 * For plain UDP, only receive and error are meaningful.
 */
enum class unified_udp_server_callback : std::size_t {
	receive = 0,
	client_connected = 1,
	client_disconnected = 2,
	error = 3
};

/**
 * \brief Callback indices for messaging_ws_client.
 *
 * WebSocket client uses callbacks for: message, text_message, binary_message,
 * connected, disconnected, error.
 */
enum class ws_client_callback : std::size_t {
	message = 0,
	text_message = 1,
	binary_message = 2,
	connected = 3,
	disconnected = 4,
	error = 5
};

/**
 * \brief Callback indices for messaging_ws_server.
 *
 * WebSocket server uses callbacks for: connection, disconnection, message,
 * text_message, binary_message, error.
 */
enum class ws_server_callback : std::size_t {
	connection = 0,
	disconnection = 1,
	message = 2,
	text_message = 3,
	binary_message = 4,
	error = 5
};

/**
 * \brief Callback indices for messaging_quic_client.
 *
 * QUIC client uses callbacks for: receive, stream_receive, connected,
 * disconnected, error.
 */
enum class quic_client_callback : std::size_t {
	receive = 0,
	stream_receive = 1,
	connected = 2,
	disconnected = 3,
	error = 4
};

/**
 * \brief Callback indices for messaging_quic_server.
 *
 * QUIC server uses callbacks for: connection, disconnection, receive,
 * stream_receive, error.
 */
enum class quic_server_callback : std::size_t {
	connection = 0,
	disconnection = 1,
	receive = 2,
	stream_receive = 3,
	error = 4
};

/**
 * \brief Helper to convert enum to std::size_t for callback_manager access.
 * \tparam E The enum type.
 * \param e The enum value.
 * \return The underlying std::size_t value.
 */
template <typename E>
constexpr auto to_index(E e) noexcept -> std::size_t
{
	static_assert(std::is_enum_v<E>, "to_index requires an enum type");
	return static_cast<std::size_t>(e);
}

} // namespace kcenon::network
