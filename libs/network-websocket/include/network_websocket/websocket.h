/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
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

/**
 * @file websocket.h
 * @brief Main header for network-websocket library
 *
 * This is the main public header for the network-websocket library.
 * Include this file to get access to all WebSocket functionality.
 *
 * The network-websocket library provides:
 * - WebSocket frame encoding/decoding (RFC 6455)
 * - HTTP/1.1 upgrade handshake
 * - WebSocket protocol state machine
 * - WebSocket socket wrapper on top of TCP
 *
 * Dependencies:
 * - network-core: For interfaces and result types
 * - network-tcp: For underlying TCP transport
 * - OpenSSL: For SHA-1 hashing in handshake
 *
 * Example usage:
 * @code
 * #include <network_websocket/websocket.h>
 *
 * // Create a WebSocket socket wrapping an existing TCP socket
 * auto ws = std::make_shared<websocket_socket>(tcp_socket, true);
 *
 * // Perform client handshake
 * ws->async_handshake("example.com", "/chat", 80, [](std::error_code ec) {
 *     if (!ec) {
 *         // Handshake successful, start reading
 *         ws->start_read();
 *     }
 * });
 *
 * // Set message callback
 * ws->set_message_callback([](const ws_message& msg) {
 *     if (msg.type == ws_message_type::text) {
 *         std::cout << "Received: " << msg.as_text() << std::endl;
 *     }
 * });
 *
 * // Send a text message
 * ws->async_send_text("Hello, WebSocket!", [](std::error_code ec, std::size_t bytes) {
 *     if (!ec) {
 *         std::cout << "Sent " << bytes << " bytes" << std::endl;
 *     }
 * });
 * @endcode
 */

// Internal implementation headers
#include "network_websocket/internal/websocket_frame.h"
#include "network_websocket/internal/websocket_handshake.h"
#include "network_websocket/internal/websocket_protocol.h"
#include "network_websocket/internal/websocket_socket.h"

// Interface headers from main project
#include "kcenon/network/interfaces/i_websocket_client.h"
#include "kcenon/network/interfaces/i_websocket_server.h"

namespace network_websocket
{
	/**
	 * @brief Library version information
	 */
	constexpr const char* VERSION = "0.1.0";
	constexpr int VERSION_MAJOR = 0;
	constexpr int VERSION_MINOR = 1;
	constexpr int VERSION_PATCH = 0;

	// Re-export commonly used types for convenience
	using kcenon::network::interfaces::i_websocket_session;

} // namespace network_websocket
