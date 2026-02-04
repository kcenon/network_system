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
#include <optional>
#include <string>
#include <vector>

namespace kcenon::network::core {
	class messaging_quic_client;
}

namespace kcenon::network::internal::adapters {

/**
 * @class quic_client_adapter
 * @brief Adapter that wraps messaging_quic_client to implement i_protocol_client
 *
 * This adapter bridges the QUIC client implementation with the unified
 * i_protocol_client interface. It allows quic_facade to return i_protocol_client
 * without modifying the existing messaging_quic_client class.
 *
 * ### Design Rationale
 * The adapter pattern is used because:
 * 1. QUIC's i_quic_client has additional methods (multi-stream, 0-RTT, ALPN)
 * 2. QUIC always uses TLS 1.3, so SSL/TLS configuration is implicit
 * 3. For QUIC-specific features, users should use messaging_quic_client directly
 *
 * ### ALPN Configuration
 * ALPN protocols can be configured via set_alpn_protocols() before start().
 * Common values: "h3" (HTTP/3), "hq-29" (HTTP/QUIC draft-29)
 *
 * ### Message Handling
 * - send() sends data on the default stream (stream 0)
 * - For multi-stream operations, use messaging_quic_client directly
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_client
 * @see messaging_quic_client
 */
class quic_client_adapter : public interfaces::i_protocol_client {
public:
	/**
	 * @brief Constructs an adapter with a unique client ID
	 * @param client_id Unique identifier for this client
	 */
	explicit quic_client_adapter(std::string_view client_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~quic_client_adapter() override;

	// Non-copyable, non-movable
	quic_client_adapter(const quic_client_adapter&) = delete;
	quic_client_adapter& operator=(const quic_client_adapter&) = delete;
	quic_client_adapter(quic_client_adapter&&) = delete;
	quic_client_adapter& operator=(quic_client_adapter&&) = delete;

	// =========================================================================
	// QUIC-specific Configuration
	// =========================================================================

	/**
	 * @brief Sets the ALPN protocols for negotiation
	 * @param protocols List of protocol identifiers (e.g., {"h3", "hq-29"})
	 *
	 * Must be called before start() to take effect.
	 */
	auto set_alpn_protocols(const std::vector<std::string>& protocols) -> void;

	/**
	 * @brief Sets CA certificate path for server verification
	 * @param path Path to CA certificate file (PEM format)
	 */
	auto set_ca_cert_path(std::string_view path) -> void;

	/**
	 * @brief Sets client certificate path for mutual TLS
	 * @param cert_path Path to client certificate file (PEM format)
	 * @param key_path Path to client private key file (PEM format)
	 */
	auto set_client_cert(std::string_view cert_path, std::string_view key_path) -> void;

	/**
	 * @brief Sets whether to verify server certificate
	 * @param verify True to verify (default), false to skip verification
	 */
	auto set_verify_server(bool verify) -> void;

	/**
	 * @brief Sets max idle timeout in milliseconds
	 * @param timeout_ms Maximum idle timeout (default: 30000)
	 */
	auto set_max_idle_timeout(uint64_t timeout_ms) -> void;

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
	 * @brief Sets up internal callbacks to bridge QUIC callbacks to i_protocol_client callbacks
	 */
	auto setup_internal_callbacks() -> void;

	std::string client_id_;
	std::shared_ptr<core::messaging_quic_client> client_;

	// QUIC-specific configuration
	std::vector<std::string> alpn_protocols_;
	std::optional<std::string> ca_cert_path_;
	std::optional<std::string> client_cert_path_;
	std::optional<std::string> client_key_path_;
	bool verify_server_{true};
	uint64_t max_idle_timeout_ms_{30000};

	mutable std::mutex callbacks_mutex_;
	std::shared_ptr<interfaces::connection_observer> observer_;
	receive_callback_t receive_callback_;
	connected_callback_t connected_callback_;
	disconnected_callback_t disconnected_callback_;
	error_callback_t error_callback_;
};

} // namespace kcenon::network::internal::adapters
