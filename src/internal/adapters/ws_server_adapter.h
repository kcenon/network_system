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

#include "kcenon/network/interfaces/i_protocol_server.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kcenon::network::core {
	class messaging_ws_server;
	class ws_connection;
}

namespace kcenon::network::internal::adapters {

/**
 * @class ws_session_wrapper
 * @brief Session wrapper for WebSocket connections implementing i_session
 *
 * This class wraps the ws_connection to provide a stable i_session interface
 * that the i_protocol_server callbacks expect.
 *
 * Note: ws_connection already implements i_websocket_session which extends i_session,
 * but we create this wrapper to ensure consistent lifetime management and
 * to decouple from WebSocket-specific types.
 */
class ws_session_wrapper : public interfaces::i_session {
public:
	explicit ws_session_wrapper(std::shared_ptr<core::ws_connection> connection);

	[[nodiscard]] auto id() const -> std::string_view override;
	[[nodiscard]] auto is_connected() const -> bool override;
	[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
	auto close() -> void override;

private:
	std::shared_ptr<core::ws_connection> connection_;
	std::string id_cache_;
};

/**
 * @class ws_server_adapter
 * @brief Adapter that wraps messaging_ws_server to implement i_protocol_server
 *
 * This adapter bridges the WebSocket server implementation with the unified
 * i_protocol_server interface. It allows websocket_facade to return i_protocol_server
 * without modifying the existing messaging_ws_server class.
 *
 * ### Design Rationale
 * The adapter pattern is used because:
 * 1. WebSocket's i_websocket_server has different callback signatures
 * 2. WebSocket has text/binary message distinction not present in i_protocol_server
 * 3. WebSocket disconnection includes close code and reason
 *
 * ### Path Handling
 * Since i_protocol_server::start() only takes port:
 * - The path is configured via set_path() before calling start()
 * - Default path is "/" if not configured
 *
 * ### Session Management
 * - Each ws_connection is wrapped in ws_session_wrapper
 * - Sessions are tracked for the lifetime of the connection
 * - connection_callback receives wrapped sessions
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_server
 * @see messaging_ws_server
 */
class ws_server_adapter : public interfaces::i_protocol_server {
public:
	/**
	 * @brief Constructs an adapter with a unique server ID
	 * @param server_id Unique identifier for this server
	 */
	explicit ws_server_adapter(std::string_view server_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~ws_server_adapter() override;

	// Non-copyable, non-movable
	ws_server_adapter(const ws_server_adapter&) = delete;
	ws_server_adapter& operator=(const ws_server_adapter&) = delete;
	ws_server_adapter(ws_server_adapter&&) = delete;
	ws_server_adapter& operator=(ws_server_adapter&&) = delete;

	// =========================================================================
	// Path Configuration
	// =========================================================================

	/**
	 * @brief Sets the WebSocket path for accepting connections
	 * @param path The WebSocket path (e.g., "/ws", "/chat")
	 *
	 * Must be called before start() if a non-default path is needed.
	 * Default path is "/" if not set.
	 */
	auto set_path(std::string_view path) -> void;

	// =========================================================================
	// i_network_component interface implementation
	// =========================================================================

	[[nodiscard]] auto is_running() const -> bool override;
	auto wait_for_stop() -> void override;

	// =========================================================================
	// i_protocol_server interface implementation
	// =========================================================================

	[[nodiscard]] auto start(uint16_t port) -> VoidResult override;
	[[nodiscard]] auto stop() -> VoidResult override;
	[[nodiscard]] auto connection_count() const -> size_t override;

	auto set_connection_callback(connection_callback_t callback) -> void override;
	auto set_disconnection_callback(disconnection_callback_t callback) -> void override;
	auto set_receive_callback(receive_callback_t callback) -> void override;
	auto set_error_callback(error_callback_t callback) -> void override;

private:
	/**
	 * @brief Sets up internal callbacks to bridge WebSocket callbacks to i_protocol_server callbacks
	 */
	auto setup_internal_callbacks() -> void;

	/**
	 * @brief Gets or creates a session wrapper for the given connection
	 */
	auto get_or_create_session(std::shared_ptr<core::ws_connection> connection)
		-> std::shared_ptr<interfaces::i_session>;

	std::string server_id_;
	std::string path_{"/"}; // Default WebSocket path
	std::shared_ptr<core::messaging_ws_server> server_;

	mutable std::mutex callbacks_mutex_;
	connection_callback_t connection_callback_;
	disconnection_callback_t disconnection_callback_;
	receive_callback_t receive_callback_;
	error_callback_t error_callback_;

	mutable std::mutex sessions_mutex_;
	std::unordered_map<std::string, std::shared_ptr<ws_session_wrapper>> sessions_;
};

} // namespace kcenon::network::internal::adapters
