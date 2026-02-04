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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kcenon::network::core {
	class http_server;
}

namespace kcenon::network::internal::adapters {

/**
 * @class http_request_session
 * @brief Session wrapper for HTTP request connections implementing i_session
 *
 * Since HTTP is stateless, each request is treated as a virtual session.
 * This class wraps request metadata to provide a stable i_session interface.
 *
 * Note: In HTTP, a "session" typically represents a single request/response
 * cycle. The session ID is derived from the client's endpoint information.
 */
class http_request_session : public interfaces::i_session {
public:
	http_request_session(
		std::string_view session_id,
		std::string_view client_address,
		uint16_t client_port,
		std::weak_ptr<core::http_server> server);

	[[nodiscard]] auto id() const -> std::string_view override;
	[[nodiscard]] auto is_connected() const -> bool override;
	[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
	auto close() -> void override;

	/**
	 * @brief Stores the response data to be sent back to client
	 * @param data Response body data
	 */
	auto set_response_data(std::vector<uint8_t> data) -> void;

	/**
	 * @brief Gets the stored response data
	 * @return Response body data
	 */
	[[nodiscard]] auto get_response_data() const -> const std::vector<uint8_t>&;

private:
	std::string session_id_;
	std::string client_address_;
	uint16_t client_port_;
	std::weak_ptr<core::http_server> server_;
	std::vector<uint8_t> response_data_;
	std::atomic<bool> is_connected_{true};
	mutable std::mutex data_mutex_;
};

/**
 * @class http_server_adapter
 * @brief Adapter that wraps http_server to implement i_protocol_server
 *
 * This adapter bridges the HTTP server implementation with the unified
 * i_protocol_server interface. It allows http_facade to return i_protocol_server
 * without modifying the existing http_server class.
 *
 * ### Design Rationale
 * The adapter pattern is used because:
 * 1. HTTP server uses routing (GET, POST handlers) vs callback-only i_protocol_server
 * 2. HTTP responses must be sent within the request handler context
 * 3. HTTP has request/response semantics, not streaming
 *
 * ### Request Handling
 * The adapter registers a catch-all route that:
 * 1. Invokes connection_callback when a request arrives
 * 2. Invokes receive_callback with the request body
 * 3. Waits for send() on the session to get response data
 * 4. Returns the response to the HTTP client
 *
 * ### Session Management
 * - Each HTTP request is treated as a temporary session
 * - Session ID is derived from client endpoint + request timestamp
 * - Sessions are removed after response is sent
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_server
 * @see http_server
 */
class http_server_adapter : public interfaces::i_protocol_server {
public:
	/**
	 * @brief Constructs an adapter with a unique server ID
	 * @param server_id Unique identifier for this server
	 */
	explicit http_server_adapter(std::string_view server_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~http_server_adapter() override;

	// Non-copyable, non-movable
	http_server_adapter(const http_server_adapter&) = delete;
	http_server_adapter& operator=(const http_server_adapter&) = delete;
	http_server_adapter(http_server_adapter&&) = delete;
	http_server_adapter& operator=(http_server_adapter&&) = delete;

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
	 * @brief Sets up internal HTTP routes to bridge to i_protocol_server callbacks
	 */
	auto setup_internal_routes() -> void;

	/**
	 * @brief Creates a unique session ID from request info
	 */
	static auto make_session_id(const std::string& address, uint16_t port, uint64_t request_id) -> std::string;

	/**
	 * @brief Gets or creates a session for the given request
	 */
	auto get_or_create_session(const std::string& address, uint16_t port)
		-> std::shared_ptr<http_request_session>;

	/**
	 * @brief Removes a session after request processing
	 */
	auto remove_session(const std::string& session_id) -> void;

	std::string server_id_;
	std::shared_ptr<core::http_server> server_;

	mutable std::mutex callbacks_mutex_;
	connection_callback_t connection_callback_;
	disconnection_callback_t disconnection_callback_;
	receive_callback_t receive_callback_;
	error_callback_t error_callback_;

	mutable std::mutex sessions_mutex_;
	std::unordered_map<std::string, std::shared_ptr<http_request_session>> sessions_;
	std::atomic<uint64_t> request_counter_{0};
	std::atomic<bool> is_running_{false};
};

} // namespace kcenon::network::internal::adapters
