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
	class messaging_server;
}

namespace kcenon::network::session {
	class messaging_session;
}

namespace kcenon::network::internal::adapters {

/**
 * @class tcp_server_adapter
 * @brief Adapter that wraps messaging_server to implement i_protocol_server
 *
 * This adapter bridges the legacy messaging_server implementation with the
 * unified i_protocol_server interface. It allows tcp_facade to return
 * i_protocol_server without modifying the existing messaging_server class.
 *
 * ### Design Rationale
 * The adapter pattern is used instead of direct inheritance because:
 * 1. messaging_server has legacy callback signatures that differ from i_protocol_server
 * 2. Avoiding breaking changes to existing code that uses messaging_server directly
 * 3. Clean separation between legacy API and new unified API
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Usage Example
 * @code
 * // Used internally by tcp_facade
 * auto adapter = std::make_shared<tcp_server_adapter>("my-server");
 * auto result = adapter->start(8080);
 * if (result) {
 *     // Server is running
 * }
 * @endcode
 *
 * @see i_protocol_server
 * @see messaging_server
 */
class tcp_server_adapter : public interfaces::i_protocol_server {
public:
	/**
	 * @brief Constructs an adapter with a unique server ID
	 * @param server_id Unique identifier for this server
	 */
	explicit tcp_server_adapter(std::string_view server_id);

	/**
	 * @brief Destructor ensures proper cleanup
	 */
	~tcp_server_adapter() override;

	// Non-copyable, non-movable
	tcp_server_adapter(const tcp_server_adapter&) = delete;
	tcp_server_adapter& operator=(const tcp_server_adapter&) = delete;
	tcp_server_adapter(tcp_server_adapter&&) = delete;
	tcp_server_adapter& operator=(tcp_server_adapter&&) = delete;

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
	 * @brief Sets up internal callbacks to bridge legacy callbacks to i_protocol_server callbacks
	 */
	auto setup_internal_callbacks() -> void;

	/**
	 * @brief Tracks a session and returns it as i_session
	 * @param session The messaging_session to track
	 * @return Shared pointer to i_session
	 */
	auto track_session(std::shared_ptr<session::messaging_session> session)
		-> std::shared_ptr<interfaces::i_session>;

	std::string server_id_;
	std::shared_ptr<core::messaging_server> server_;

	mutable std::mutex callbacks_mutex_;
	connection_callback_t connection_callback_;
	disconnection_callback_t disconnection_callback_;
	receive_callback_t receive_callback_;
	error_callback_t error_callback_;

	// Session tracking for lookup by ID
	mutable std::mutex sessions_mutex_;
	std::unordered_map<std::string, std::shared_ptr<session::messaging_session>> sessions_;
};

} // namespace kcenon::network::internal::adapters
