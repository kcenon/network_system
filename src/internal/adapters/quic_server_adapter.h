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
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::network::core {
	class messaging_quic_server;
}

namespace kcenon::network::session {
	class quic_session;
}

namespace kcenon::network::internal::adapters {

/**
 * @class quic_session_wrapper
 * @brief Wrapper for QUIC session to implement i_session interface
 *
 * This wrapper provides the i_session interface for QUIC sessions,
 * enabling unified session management through i_protocol_server.
 */
class quic_session_wrapper : public interfaces::i_session {
public:
	/**
	 * @brief Constructs a session wrapper
	 * @param session_id Unique session identifier
	 * @param session The underlying QUIC session
	 */
	quic_session_wrapper(
		std::string_view session_id,
		std::shared_ptr<session::quic_session> session);

	// i_session interface
	[[nodiscard]] auto id() const -> std::string_view override;
	[[nodiscard]] auto is_connected() const -> bool override;
	[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
	auto close() -> void override;

private:
	std::string session_id_;
	std::shared_ptr<session::quic_session> session_;
	std::atomic<bool> is_connected_{true};
};

/**
 * @class quic_server_adapter
 * @brief Adapter that wraps messaging_quic_server to implement i_protocol_server
 *
 * This adapter bridges the QUIC server implementation with the unified
 * i_protocol_server interface. It allows quic_facade to return i_protocol_server
 * without modifying the existing messaging_quic_server class.
 *
 * ### Design Rationale
 * The adapter pattern is used because:
 * 1. QUIC's i_quic_server has additional methods (multi-stream, broadcast)
 * 2. QUIC always uses TLS 1.3, requiring certificate configuration
 * 3. For QUIC-specific features, users should use messaging_quic_server directly
 *
 * ### TLS Configuration
 * QUIC requires TLS certificates. Configure via:
 * - set_cert_path() for server certificate
 * - set_key_path() for private key
 * These MUST be called before start().
 *
 * ### Message Handling
 * - Receives data from clients on default stream
 * - For multi-stream operations, use messaging_quic_server directly
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_server
 * @see messaging_quic_server
 */
class quic_server_adapter : public interfaces::i_protocol_server {
public:
	/**
	 * @brief Constructs an adapter with a unique server ID
	 * @param server_id Unique identifier for this server
	 */
	explicit quic_server_adapter(std::string_view server_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~quic_server_adapter() override;

	// Non-copyable, non-movable
	quic_server_adapter(const quic_server_adapter&) = delete;
	quic_server_adapter& operator=(const quic_server_adapter&) = delete;
	quic_server_adapter(quic_server_adapter&&) = delete;
	quic_server_adapter& operator=(quic_server_adapter&&) = delete;

	// =========================================================================
	// QUIC-specific Configuration (REQUIRED before start)
	// =========================================================================

	/**
	 * @brief Sets the server certificate path
	 * @param path Path to server certificate file (PEM format)
	 *
	 * REQUIRED: Must be called before start().
	 */
	auto set_cert_path(std::string_view path) -> void;

	/**
	 * @brief Sets the server private key path
	 * @param path Path to private key file (PEM format)
	 *
	 * REQUIRED: Must be called before start().
	 */
	auto set_key_path(std::string_view path) -> void;

	/**
	 * @brief Sets the ALPN protocols for negotiation
	 * @param protocols List of protocol identifiers (e.g., {"h3", "hq-29"})
	 */
	auto set_alpn_protocols(const std::vector<std::string>& protocols) -> void;

	/**
	 * @brief Sets CA certificate path for client verification
	 * @param path Path to CA certificate file (PEM format)
	 */
	auto set_ca_cert_path(std::string_view path) -> void;

	/**
	 * @brief Sets whether to require client certificate (mutual TLS)
	 * @param require True to require client cert, false otherwise
	 */
	auto set_require_client_cert(bool require) -> void;

	/**
	 * @brief Sets max idle timeout in milliseconds
	 * @param timeout_ms Maximum idle timeout (default: 30000)
	 */
	auto set_max_idle_timeout(uint64_t timeout_ms) -> void;

	/**
	 * @brief Sets maximum number of concurrent connections
	 * @param max Maximum connections (default: 10000)
	 */
	auto set_max_connections(size_t max) -> void;

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
	 * @brief Sets up internal callbacks to bridge QUIC callbacks to i_protocol_server callbacks
	 */
	auto setup_internal_callbacks() -> void;

	/**
	 * @brief Creates or retrieves a session wrapper for a QUIC session
	 */
	auto get_or_create_wrapper(std::shared_ptr<session::quic_session> session)
		-> std::shared_ptr<quic_session_wrapper>;

	/**
	 * @brief Removes a session wrapper
	 */
	auto remove_wrapper(const std::string& session_id) -> void;

	std::string server_id_;
	std::shared_ptr<core::messaging_quic_server> server_;
	std::atomic<bool> is_running_{false};

	// QUIC-specific configuration
	std::string cert_path_;
	std::string key_path_;
	std::vector<std::string> alpn_protocols_;
	std::optional<std::string> ca_cert_path_;
	bool require_client_cert_{false};
	uint64_t max_idle_timeout_ms_{30000};
	size_t max_connections_{10000};

	// Session management
	mutable std::mutex sessions_mutex_;
	std::unordered_map<std::string, std::shared_ptr<quic_session_wrapper>> sessions_;

	// Callbacks
	mutable std::mutex callbacks_mutex_;
	connection_callback_t connection_callback_;
	disconnection_callback_t disconnection_callback_;
	receive_callback_t receive_callback_;
	error_callback_t error_callback_;
};

} // namespace kcenon::network::internal::adapters
