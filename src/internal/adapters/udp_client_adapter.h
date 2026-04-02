// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/network/interfaces/i_protocol_client.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace kcenon::network::core {
	class messaging_udp_client;
}

namespace kcenon::network::internal::adapters {

/**
 * @class udp_client_adapter
 * @brief Adapter that wraps messaging_udp_client to implement i_protocol_client
 *
 * This adapter bridges the UDP client implementation with the unified
 * i_protocol_client interface. It allows udp_facade to return i_protocol_client
 * without modifying the existing messaging_udp_client class.
 *
 * ### Design Rationale
 * The adapter pattern is used because:
 * 1. UDP's i_udp_client has different method signatures than i_protocol_client
 * 2. UDP is connectionless but i_protocol_client assumes connection semantics
 * 3. UDP receive callbacks include endpoint info, which i_protocol_client lacks
 *
 * ### Connection Semantics
 * Since UDP is connectionless:
 * - is_connected() returns true when target endpoint is set
 * - connect/disconnect callbacks are invoked on start/stop
 * - The "connection" represents the ability to send to the target
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * @see i_protocol_client
 * @see messaging_udp_client
 */
class udp_client_adapter : public interfaces::i_protocol_client {
public:
	/**
	 * @brief Constructs an adapter with a unique client ID
	 * @param client_id Unique identifier for this client
	 */
	explicit udp_client_adapter(std::string_view client_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~udp_client_adapter() override;

	// Non-copyable, non-movable
	udp_client_adapter(const udp_client_adapter&) = delete;
	udp_client_adapter& operator=(const udp_client_adapter&) = delete;
	udp_client_adapter(udp_client_adapter&&) = delete;
	udp_client_adapter& operator=(udp_client_adapter&&) = delete;

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
	 * @brief Sets up internal callbacks to bridge UDP callbacks to i_protocol_client callbacks
	 */
	auto setup_internal_callbacks() -> void;

	std::string client_id_;
	std::shared_ptr<core::messaging_udp_client> client_;
	std::atomic<bool> is_connected_{false};

	mutable std::mutex callbacks_mutex_;
	std::shared_ptr<interfaces::connection_observer> observer_;
	receive_callback_t receive_callback_;
	connected_callback_t connected_callback_;
	disconnected_callback_t disconnected_callback_;
	error_callback_t error_callback_;
};

} // namespace kcenon::network::internal::adapters
