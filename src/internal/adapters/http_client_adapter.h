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

#include "kcenon/network/interfaces/i_protocol_client.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

namespace kcenon::network::core {
	class http_client;
}

namespace kcenon::network::internal::adapters {

/**
 * @class http_client_adapter
 * @brief Adapter that wraps http_client to implement i_protocol_client
 *
 * This adapter bridges the HTTP client implementation with the unified
 * i_protocol_client interface. It allows http_facade to return i_protocol_client
 * without modifying the existing http_client class.
 *
 * ### Design Rationale
 * The adapter pattern is used because:
 * 1. HTTP uses request/response paradigm vs streaming i_protocol_client interface
 * 2. HTTP is stateless while i_protocol_client assumes connection state
 * 3. HTTP operations (GET, POST, etc.) don't map directly to send()
 *
 * ### Connection Semantics
 * Since HTTP is stateless:
 * - start(host, port) stores the base URL for subsequent requests
 * - is_connected() returns true when base URL is configured
 * - send() performs an HTTP POST request with binary data
 * - Received response body is delivered via receive callback
 *
 * ### Path Handling
 * - The path is configured via set_path() before sending
 * - Default path is "/" if not configured
 * - Each send() uses the configured path
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_client
 * @see http_client
 */
class http_client_adapter : public interfaces::i_protocol_client {
public:
	/**
	 * @brief Constructs an adapter with timeout configuration
	 * @param client_id Unique identifier for this client
	 * @param timeout Request timeout (default: 30 seconds)
	 */
	explicit http_client_adapter(
		std::string_view client_id,
		std::chrono::milliseconds timeout = std::chrono::seconds(30));

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~http_client_adapter() override;

	// Non-copyable, non-movable
	http_client_adapter(const http_client_adapter&) = delete;
	http_client_adapter& operator=(const http_client_adapter&) = delete;
	http_client_adapter(http_client_adapter&&) = delete;
	http_client_adapter& operator=(http_client_adapter&&) = delete;

	// =========================================================================
	// Path Configuration
	// =========================================================================

	/**
	 * @brief Sets the HTTP path for requests
	 * @param path The HTTP path (e.g., "/api/data", "/submit")
	 *
	 * Can be changed between send() calls to target different endpoints.
	 * Default path is "/" if not set.
	 */
	auto set_path(std::string_view path) -> void;

	/**
	 * @brief Sets whether to use HTTPS
	 * @param use_ssl true for HTTPS, false for HTTP
	 *
	 * Must be called before start() or between connections.
	 * Default is false (HTTP).
	 */
	auto set_use_ssl(bool use_ssl) -> void;

	// =========================================================================
	// i_network_component interface implementation
	// =========================================================================

	[[nodiscard]] auto is_running() const -> bool override;
	auto wait_for_stop() -> void override;

	// =========================================================================
	// i_protocol_client interface implementation
	// =========================================================================

	[[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override;
	[[nodiscard]] auto stop() -> VoidResult override;
	[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
	[[nodiscard]] auto is_connected() const -> bool override;

	auto set_observer(std::shared_ptr<interfaces::connection_observer> observer) -> void override;

	[[deprecated("Use set_observer() with connection_observer instead")]]
	auto set_receive_callback(receive_callback_t callback) -> void override;

	[[deprecated("Use set_observer() with connection_observer instead")]]
	auto set_connected_callback(connected_callback_t callback) -> void override;

	[[deprecated("Use set_observer() with connection_observer instead")]]
	auto set_disconnected_callback(disconnected_callback_t callback) -> void override;

	[[deprecated("Use set_observer() with connection_observer instead")]]
	auto set_error_callback(error_callback_t callback) -> void override;

private:
	/**
	 * @brief Builds the full URL from stored components
	 */
	[[nodiscard]] auto build_url() const -> std::string;

	/**
	 * @brief Notifies observers/callbacks of received data
	 */
	auto notify_receive(const std::vector<uint8_t>& data) -> void;

	/**
	 * @brief Notifies observers/callbacks of errors
	 */
	auto notify_error(std::error_code ec) -> void;

	std::string client_id_;
	std::chrono::milliseconds timeout_;
	std::shared_ptr<core::http_client> client_;

	// URL components
	std::string host_;
	uint16_t port_{0};
	std::string path_{"/"};
	bool use_ssl_{false};

	std::atomic<bool> is_running_{false};

	mutable std::mutex callbacks_mutex_;
	std::shared_ptr<interfaces::connection_observer> observer_;
	receive_callback_t receive_callback_;
	connected_callback_t connected_callback_;
	disconnected_callback_t disconnected_callback_;
	error_callback_t error_callback_;
};

} // namespace kcenon::network::internal::adapters
