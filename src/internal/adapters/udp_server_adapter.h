// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/interfaces/i_protocol_server.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kcenon::network::core {
	class messaging_udp_server;
}

namespace kcenon::network::internal::adapters {

/**
 * @class udp_endpoint_session
 * @brief Session wrapper for UDP endpoint connections
 *
 * Since UDP is connectionless, this class provides a session abstraction
 * for each unique remote endpoint that communicates with the server.
 */
class udp_endpoint_session : public interfaces::i_session {
public:
	udp_endpoint_session(
		std::string_view session_id,
		std::string_view address,
		uint16_t port,
		std::weak_ptr<core::messaging_udp_server> server);

	[[nodiscard]] auto id() const -> std::string_view override;
	[[nodiscard]] auto is_connected() const -> bool override;
	[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
	auto close() -> void override;

private:
	std::string session_id_;
	std::string address_;
	uint16_t port_;
	std::weak_ptr<core::messaging_udp_server> server_;
};

/**
 * @class udp_server_adapter
 * @brief Adapter that wraps messaging_udp_server to implement i_protocol_server
 *
 * This adapter bridges the UDP server implementation with the unified
 * i_protocol_server interface. It tracks unique endpoints as virtual
 * "connections" to provide session semantics for connectionless UDP.
 *
 * ### Connection Semantics
 * Since UDP is connectionless:
 * - Each unique remote endpoint is treated as a virtual "session"
 * - Sessions are created on first datagram from a new endpoint
 * - Sessions are removed after inactivity timeout (optional)
 * - connection_count() returns count of known endpoints
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_server
 * @see messaging_udp_server
 */
class udp_server_adapter : public interfaces::i_protocol_server {
public:
	/**
	 * @brief Constructs an adapter with a unique server ID
	 * @param server_id Unique identifier for this server
	 */
	explicit udp_server_adapter(std::string_view server_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~udp_server_adapter() override;

	// Non-copyable, non-movable
	udp_server_adapter(const udp_server_adapter&) = delete;
	udp_server_adapter& operator=(const udp_server_adapter&) = delete;
	udp_server_adapter(udp_server_adapter&&) = delete;
	udp_server_adapter& operator=(udp_server_adapter&&) = delete;

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
	 * @brief Sets up internal callbacks to bridge UDP callbacks to i_protocol_server callbacks
	 */
	auto setup_internal_callbacks() -> void;

	/**
	 * @brief Creates a unique session ID from endpoint info
	 */
	static auto make_session_id(const std::string& address, uint16_t port) -> std::string;

	/**
	 * @brief Gets or creates a session for the given endpoint
	 */
	auto get_or_create_session(const std::string& address, uint16_t port)
		-> std::shared_ptr<interfaces::i_session>;

	std::string server_id_;
	std::shared_ptr<core::messaging_udp_server> server_;

	mutable std::mutex callbacks_mutex_;
	connection_callback_t connection_callback_;
	disconnection_callback_t disconnection_callback_;
	receive_callback_t receive_callback_;
	error_callback_t error_callback_;

	mutable std::mutex sessions_mutex_;
	std::unordered_map<std::string, std::shared_ptr<udp_endpoint_session>> sessions_;
};

} // namespace kcenon::network::internal::adapters
