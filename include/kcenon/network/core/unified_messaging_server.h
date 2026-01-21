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
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/config/feature_flags.h"
#include "kcenon/network/policy/tls_policy.h"
#include "kcenon/network/protocol/protocol_tags.h"
#include "kcenon/network/core/callback_indices.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/utils/lifecycle_manager.h"

#ifdef BUILD_TLS_SUPPORT
#include <asio/ssl.hpp>
#endif

// Optional monitoring support via common_system
#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/monitoring_interface.h>
#endif

namespace kcenon::network::session
{
class messaging_session;
#ifdef BUILD_TLS_SUPPORT
class secure_session;
#endif
} // namespace kcenon::network::session

namespace kcenon::network::core
{

/*!
 * \class unified_messaging_server
 * \brief Unified TCP server template parameterized by protocol and TLS policy.
 *
 * This template consolidates plain and secure TCP server variants into a single
 * implementation. The TLS policy determines at compile-time whether secure
 * communication is used.
 *
 * ### Template Parameters
 * - \c Protocol: Protocol tag type (e.g., tcp_protocol, udp_protocol)
 * - \c TlsPolicy: TLS policy type (no_tls or tls_enabled)
 *
 * ### Thread Safety
 * - All public methods are thread-safe.
 * - Internal state is protected by atomics and mutex.
 * - Background thread runs io_context.run() independently.
 * - Multiple sessions can be active concurrently without blocking each other.
 * - Sessions vector is protected by sessions_mutex_ for thread-safe cleanup.
 *
 * ### Usage Example
 * \code
 * // Plain TCP server
 * auto plain_server = std::make_shared<unified_messaging_server<tcp_protocol>>("server1");
 * plain_server->start_server(8080);
 *
 * // Secure TCP server
 * tls_enabled tls_config{
 *     .cert_path = "server.crt",
 *     .key_path = "server.key"
 * };
 * auto secure_server = std::make_shared<unified_messaging_server<tcp_protocol, tls_enabled>>(
 *     "server2", tls_config);
 * secure_server->start_server(8443);
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
class unified_messaging_server
	: public std::enable_shared_from_this<unified_messaging_server<Protocol, TlsPolicy>>
{
public:
	//! \brief Indicates whether TLS is enabled for this server
	static constexpr bool is_secure = policy::is_tls_enabled_v<TlsPolicy>;

	//! \brief Session type depends on TLS policy
#ifdef BUILD_TLS_SUPPORT
	using session_type = std::conditional_t<
		is_secure,
		session::secure_session,
		session::messaging_session>;
#else
	using session_type = session::messaging_session;
#endif

	//! \brief Session pointer type
	using session_ptr = std::shared_ptr<session_type>;

	//! \brief Callback type for new connection
	using connection_callback_t = std::function<void(session_ptr)>;
	//! \brief Callback type for disconnection
	using disconnection_callback_t = std::function<void(const std::string&)>;
	//! \brief Callback type for received data
	using receive_callback_t = std::function<void(session_ptr, const std::vector<uint8_t>&)>;
	//! \brief Callback type for errors
	using error_callback_t = std::function<void(session_ptr, std::error_code)>;

	/*!
	 * \brief Constructs a plain server with a given identifier.
	 * \param server_id A string identifier for this server instance.
	 *
	 * \note This constructor is only available when TlsPolicy is no_tls.
	 */
	explicit unified_messaging_server(std::string_view server_id)
		requires (!policy::is_tls_enabled_v<TlsPolicy>);

	/*!
	 * \brief Constructs a secure server with TLS configuration.
	 * \param server_id A string identifier for this server instance.
	 * \param tls_config TLS configuration (cert paths, verification settings).
	 *
	 * \note This constructor is only available when TlsPolicy is tls_enabled.
	 */
	unified_messaging_server(std::string_view server_id, const TlsPolicy& tls_config)
		requires policy::is_tls_enabled_v<TlsPolicy>;

	/*!
	 * \brief Destructor; automatically calls stop_server() if still running.
	 */
	~unified_messaging_server() noexcept;

	// Non-copyable, non-movable
	unified_messaging_server(const unified_messaging_server&) = delete;
	unified_messaging_server& operator=(const unified_messaging_server&) = delete;
	unified_messaging_server(unified_messaging_server&&) = delete;
	unified_messaging_server& operator=(unified_messaging_server&&) = delete;

	// =====================================================================
	// Lifecycle Management
	// =====================================================================

	/*!
	 * \brief Starts the server on the specified port.
	 * \param port The port to listen on.
	 * \return Result<void> - Success if server started, or error with code:
	 *         - error_codes::network_system::server_already_running if already running
	 *         - error_codes::network_system::bind_failed if port binding failed
	 *         - error_codes::common_errors::internal_error for other failures
	 */
	[[nodiscard]] auto start_server(uint16_t port) -> VoidResult;

	/*!
	 * \brief Stops the server and closes all connections.
	 * \return Result<void> - Success if server stopped, or error with code:
	 *         - error_codes::network_system::server_not_started if not running
	 *         - error_codes::common_errors::internal_error for other failures
	 */
	[[nodiscard]] auto stop_server() -> VoidResult;

	/*!
	 * \brief Blocks until stop_server() is called.
	 */
	auto wait_for_stop() -> void;

	/*!
	 * \brief Checks if the server is currently running.
	 * \return true if running, false otherwise.
	 */
	[[nodiscard]] auto is_running() const noexcept -> bool;

	/*!
	 * \brief Returns the server identifier.
	 * \return The server_id string.
	 */
	[[nodiscard]] auto server_id() const -> const std::string&;

	// =====================================================================
	// Callback Setters
	// =====================================================================

	/*!
	 * \brief Sets the callback for new client connections.
	 * \param callback Function called when a client connects.
	 */
	auto set_connection_callback(connection_callback_t callback) -> void;

	/*!
	 * \brief Sets the callback for client disconnections.
	 * \param callback Function called when a client disconnects.
	 */
	auto set_disconnection_callback(disconnection_callback_t callback) -> void;

	/*!
	 * \brief Sets the callback for received messages.
	 * \param callback Function called when data is received from a client.
	 */
	auto set_receive_callback(receive_callback_t callback) -> void;

	/*!
	 * \brief Sets the callback for session errors.
	 * \param callback Function called when an error occurs on a session.
	 */
	auto set_error_callback(error_callback_t callback) -> void;

#if KCENON_WITH_COMMON_SYSTEM
	/*!
	 * \brief Set a monitoring interface for metrics collection
	 * \param monitor Pointer to IMonitor implementation (not owned)
	 */
	auto set_monitor(kcenon::common::interfaces::IMonitor* monitor) -> void;

	/*!
	 * \brief Get the current monitor
	 * \return Pointer to monitor or nullptr if not set
	 */
	auto get_monitor() const -> kcenon::common::interfaces::IMonitor*;
#endif

private:
	// =====================================================================
	// Internal Implementation Methods
	// =====================================================================

	auto do_start_impl(uint16_t port) -> VoidResult;
	auto do_stop_impl() -> VoidResult;

	// =====================================================================
	// Internal Callback Helpers
	// =====================================================================

	[[nodiscard]] auto get_connection_callback() const -> connection_callback_t;
	[[nodiscard]] auto get_disconnection_callback() const -> disconnection_callback_t;
	[[nodiscard]] auto get_receive_callback() const -> receive_callback_t;
	[[nodiscard]] auto get_error_callback() const -> error_callback_t;

	auto invoke_connection_callback(session_ptr session) -> void;

	// =====================================================================
	// Internal Connection Handlers
	// =====================================================================

	auto do_accept() -> void;
	auto on_accept(std::error_code ec, asio::ip::tcp::socket socket) -> void;
	auto cleanup_dead_sessions() -> void;
	auto start_cleanup_timer() -> void;

#ifdef BUILD_TLS_SUPPORT
	auto on_handshake_complete(session_ptr session, std::error_code ec) -> void
		requires is_secure;
#endif

private:
	//! \brief Callback index type alias for clarity
	using callback_index = tcp_server_callback;

	//! \brief Callback manager type for this server
	using callbacks_t = utils::callback_manager<
		connection_callback_t,
		disconnection_callback_t,
		receive_callback_t,
		error_callback_t>;

	// =====================================================================
	// Member Variables
	// =====================================================================

	std::string server_id_;                      /*!< Server identifier. */
	utils::lifecycle_manager lifecycle_;         /*!< Lifecycle state manager. */
	callbacks_t callbacks_;                      /*!< Callback manager. */
	std::atomic<bool> stop_initiated_{false};    /*!< Stop operation flag. */

	std::shared_ptr<asio::io_context> io_context_;
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
		work_guard_;
	std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
	std::future<void> io_context_future_;

#ifdef BUILD_TLS_SUPPORT
	std::unique_ptr<asio::ssl::context> ssl_context_;
	[[no_unique_address]] TlsPolicy tls_config_;
#endif

	std::vector<session_ptr> sessions_;
	mutable std::mutex sessions_mutex_;
	mutable std::mutex acceptor_mutex_;
	std::unique_ptr<asio::steady_timer> cleanup_timer_;

#if KCENON_WITH_COMMON_SYSTEM
	kcenon::common::interfaces::IMonitor* monitor_ = nullptr;
	std::atomic<uint64_t> messages_received_{0};
	std::atomic<uint64_t> messages_sent_{0};
	std::atomic<uint64_t> connection_errors_{0};
#endif
};

// =====================================================================
// Type Aliases for Convenience
// =====================================================================

/*!
 * \brief Type alias for plain TCP server.
 *
 * Equivalent to: unified_messaging_server<tcp_protocol, no_tls>
 */
using tcp_server = unified_messaging_server<protocol::tcp_protocol, policy::no_tls>;

#ifdef BUILD_TLS_SUPPORT
/*!
 * \brief Type alias for secure TCP server with TLS.
 *
 * Equivalent to: unified_messaging_server<tcp_protocol, tls_enabled>
 */
using secure_tcp_server = unified_messaging_server<protocol::tcp_protocol, policy::tls_enabled>;
#endif

} // namespace kcenon::network::core

// Include template implementation
#include "unified_messaging_server.inl"
