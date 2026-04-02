// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

	auto set_receive_callback(receive_callback_t callback) -> void override;
	auto set_connected_callback(connected_callback_t callback) -> void override;
	auto set_disconnected_callback(disconnected_callback_t callback) -> void override;
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
