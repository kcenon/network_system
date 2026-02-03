/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/policy/tls_policy.h"
#include "kcenon/network/protocol/protocol_tags.h"
#include "internal/core/callback_indices.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/utils/lifecycle_manager.h"

#ifdef BUILD_TLS_SUPPORT
#include <asio/ssl.hpp>
#endif

namespace kcenon::network::internal
{
	class tcp_socket;
#ifdef BUILD_TLS_SUPPORT
	class secure_tcp_socket;
#endif
}

namespace kcenon::network::core
{

/*!
 * \class unified_messaging_client
 * \brief Unified TCP client template parameterized by protocol and TLS policy.
 *
 * This template consolidates plain and secure TCP client variants into a single
 * implementation. The TLS policy determines at compile-time whether secure
 * communication is used.
 *
 * ### Template Parameters
 * - \c Protocol: Protocol tag type (e.g., tcp_protocol, udp_protocol)
 * - \c TlsPolicy: TLS policy type (no_tls or tls_enabled)
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Socket access is protected by socket_mutex_.
 * - Atomic flags prevent race conditions.
 * - send_packet() can be called from any thread safely.
 * - Connection state changes are serialized through ASIO's io_context.
 *
 * ### Usage Example
 * \code
 * // Plain TCP client
 * auto plain_client = std::make_shared<unified_messaging_client<tcp_protocol>>("client1");
 * plain_client->start_client("localhost", 8080);
 *
 * // Secure TCP client
 * auto secure_client = std::make_shared<unified_messaging_client<tcp_protocol, tls_enabled>>("client2");
 * secure_client->start_client("localhost", 8443);
 * \endcode
 *
 * \tparam Protocol The protocol tag type (must satisfy protocol::Protocol concept)
 * \tparam TlsPolicy The TLS policy type (must satisfy policy::TlsPolicy concept)
 *
 * \note This template currently supports tcp_protocol only. Support for other
 *       protocols (udp, websocket, quic) will be added in future iterations.
 */
template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy = policy::no_tls>
	requires std::same_as<Protocol, protocol::tcp_protocol>
class unified_messaging_client
	: public std::enable_shared_from_this<unified_messaging_client<Protocol, TlsPolicy>>
{
public:
	//! \brief Indicates whether TLS is enabled for this client
	static constexpr bool is_secure = policy::is_tls_enabled_v<TlsPolicy>;

	//! \brief Callback type for received data
	using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
	//! \brief Callback type for connection established
	using connected_callback_t = std::function<void()>;
	//! \brief Callback type for disconnection
	using disconnected_callback_t = std::function<void()>;
	//! \brief Callback type for errors
	using error_callback_t = std::function<void(std::error_code)>;

	/*!
	 * \brief Constructs a client with a given identifier.
	 * \param client_id A string identifier for this client instance.
	 */
	explicit unified_messaging_client(std::string_view client_id);

	/*!
	 * \brief Constructs a secure client with TLS configuration.
	 * \param client_id A string identifier for this client instance.
	 * \param tls_config TLS configuration (cert paths, verification settings).
	 *
	 * \note This constructor is only available when TlsPolicy is tls_enabled.
	 */
	unified_messaging_client(std::string_view client_id, const TlsPolicy& tls_config)
		requires policy::is_tls_enabled_v<TlsPolicy>;

	/*!
	 * \brief Destructor; automatically calls stop_client() if still running.
	 */
	~unified_messaging_client() noexcept;

	// Non-copyable, non-movable
	unified_messaging_client(const unified_messaging_client&) = delete;
	unified_messaging_client& operator=(const unified_messaging_client&) = delete;
	unified_messaging_client(unified_messaging_client&&) = delete;
	unified_messaging_client& operator=(unified_messaging_client&&) = delete;

	// =====================================================================
	// Lifecycle Management
	// =====================================================================

	/*!
	 * \brief Starts the client and connects to the specified host and port.
	 * \param host The remote hostname or IP address.
	 * \param port The remote port number to connect.
	 * \return Result<void> - Success if client started, or error with code:
	 *         - error_codes::network_system::client_already_running if already running
	 *         - error_codes::common_errors::internal_error for other failures
	 */
	[[nodiscard]] auto start_client(std::string_view host, uint16_t port) -> VoidResult;

	/*!
	 * \brief Stops the client and disconnects from the server.
	 * \return Result<void> - Success if client stopped, or error with code:
	 *         - error_codes::network_system::client_not_started if not running
	 *         - error_codes::common_errors::internal_error for failures
	 */
	[[nodiscard]] auto stop_client() -> VoidResult;

	/*!
	 * \brief Blocks until stop_client() is called.
	 */
	auto wait_for_stop() -> void;

	/*!
	 * \brief Checks if the client is currently running.
	 * \return true if running, false otherwise.
	 */
	[[nodiscard]] auto is_running() const noexcept -> bool;

	/*!
	 * \brief Checks if the client is connected to the server.
	 * \return true if connected, false otherwise.
	 */
	[[nodiscard]] auto is_connected() const noexcept -> bool;

	/*!
	 * \brief Returns the client identifier.
	 * \return The client_id string.
	 */
	[[nodiscard]] auto client_id() const -> const std::string&;

	// =====================================================================
	// Data Transfer
	// =====================================================================

	/*!
	 * \brief Sends data to the connected server.
	 * \param data The buffer to send (moved for efficiency).
	 * \return Result<void> - Success if data queued for send, or error with code:
	 *         - error_codes::network_system::connection_closed if not connected
	 *         - error_codes::network_system::send_failed for other failures
	 */
	[[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

	// =====================================================================
	// Callback Setters
	// =====================================================================

	/*!
	 * \brief Sets the callback for received data.
	 * \param callback Function called when data is received.
	 */
	auto set_receive_callback(receive_callback_t callback) -> void;

	/*!
	 * \brief Sets the callback for connection established.
	 * \param callback Function called when connection is established.
	 */
	auto set_connected_callback(connected_callback_t callback) -> void;

	/*!
	 * \brief Sets the callback for disconnection.
	 * \param callback Function called when disconnected.
	 */
	auto set_disconnected_callback(disconnected_callback_t callback) -> void;

	/*!
	 * \brief Sets the callback for errors.
	 * \param callback Function called when an error occurs.
	 */
	auto set_error_callback(error_callback_t callback) -> void;

private:
	// =====================================================================
	// Internal Socket Type Selection
	// =====================================================================

#ifdef BUILD_TLS_SUPPORT
	using socket_type = std::conditional_t<
		is_secure,
		internal::secure_tcp_socket,
		internal::tcp_socket
	>;
#else
	using socket_type = internal::tcp_socket;
#endif

	// =====================================================================
	// Internal Implementation Methods
	// =====================================================================

	auto do_start_impl(std::string_view host, uint16_t port) -> VoidResult;
	auto do_stop_impl() -> VoidResult;
	auto do_send_impl(std::vector<uint8_t>&& data) -> VoidResult;

	// =====================================================================
	// Internal Callback Helpers
	// =====================================================================

	auto set_connected(bool connected) -> void;
	auto invoke_receive_callback(const std::vector<uint8_t>& data) -> void;
	auto invoke_connected_callback() -> void;
	auto invoke_disconnected_callback() -> void;
	auto invoke_error_callback(std::error_code ec) -> void;

	// =====================================================================
	// Internal Connection Handlers
	// =====================================================================

	auto do_connect(std::string_view host, uint16_t port) -> void;
	auto on_connect(std::error_code ec) -> void;
	auto on_receive(std::span<const uint8_t> data) -> void;
	auto on_error(std::error_code ec) -> void;
	auto on_connection_failed(std::error_code ec) -> void;

#ifdef BUILD_TLS_SUPPORT
	auto on_handshake_complete(std::error_code ec) -> void
		requires is_secure;
#endif

private:
	//! \brief Callback index type alias for clarity
	using callback_index = tcp_client_callback;

	//! \brief Callback manager type for this client
	using callbacks_t = utils::callback_manager<
		receive_callback_t,
		connected_callback_t,
		disconnected_callback_t,
		error_callback_t
	>;

	// =====================================================================
	// Member Variables
	// =====================================================================

	std::string client_id_;                      /*!< Client identifier. */
	utils::lifecycle_manager lifecycle_;         /*!< Lifecycle state manager. */
	callbacks_t callbacks_;                      /*!< Callback manager. */
	std::atomic<bool> is_connected_{false};      /*!< Connection state. */
	std::atomic<bool> stop_initiated_{false};    /*!< Stop operation flag. */

	std::shared_ptr<asio::io_context> io_context_; /*!< I/O context for async operations. */
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
		work_guard_;                             /*!< Keeps io_context running. */
	std::future<void> io_context_future_;        /*!< Future for io_context run task. */

#ifdef BUILD_TLS_SUPPORT
	std::unique_ptr<asio::ssl::context> ssl_context_; /*!< SSL context (secure only). */
	[[no_unique_address]] TlsPolicy tls_config_; /*!< TLS configuration. */
#endif

	auto get_socket() const -> std::shared_ptr<socket_type>;

	mutable std::mutex socket_mutex_;            /*!< Protects socket_ from data races. */
	std::shared_ptr<socket_type> socket_;        /*!< The socket wrapper once connected. */

	mutable std::mutex pending_mutex_;           /*!< Protects pending connection state. */
	std::shared_ptr<asio::ip::tcp::resolver> pending_resolver_;
	std::shared_ptr<asio::ip::tcp::socket> pending_socket_;
};

// =====================================================================
// Type Aliases for Convenience
// =====================================================================

/*!
 * \brief Type alias for plain TCP client.
 *
 * Equivalent to: messaging_client<tcp_protocol, no_tls>
 */
using tcp_client = unified_messaging_client<protocol::tcp_protocol, policy::no_tls>;

#ifdef BUILD_TLS_SUPPORT
/*!
 * \brief Type alias for secure TCP client with TLS.
 *
 * Equivalent to: messaging_client<tcp_protocol, tls_enabled>
 */
using secure_tcp_client = unified_messaging_client<protocol::tcp_protocol, policy::tls_enabled>;
#endif

} // namespace kcenon::network::core

// Include template implementation
#include "unified_messaging_client.inl"
