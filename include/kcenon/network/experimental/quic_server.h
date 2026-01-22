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

// Experimental API marker - users must opt-in to use this header
#include "kcenon/network/experimental/experimental_api.h"
NETWORK_REQUIRE_EXPERIMENTAL

#include <kcenon/network/config/feature_flags.h>

#include "kcenon/network/core/callback_indices.h"
#include "kcenon/network/experimental/quic_client.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/interfaces/i_quic_server.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/protocols/quic/connection_id.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/result_types.h"

#include <array>
#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <asio.hpp>

// Optional monitoring support via common_system
#if KCENON_WITH_COMMON_SYSTEM
	#include <kcenon/common/interfaces/monitoring_interface.h>
#endif // KCENON_WITH_COMMON_SYSTEM

namespace kcenon::network::internal
{
	class quic_socket;
} // namespace kcenon::network::internal

namespace kcenon::network::session
{
	class quic_session;
} // namespace kcenon::network::session

namespace kcenon::network::core
{

	/*!
	 * \struct quic_server_config
	 * \brief Configuration options for QUIC server
	 */
	struct quic_server_config
	{
		//! Path to server certificate file (PEM format, required)
		std::string cert_file;

		//! Path to server private key file (PEM format, required)
		std::string key_file;

		//! Path to CA certificate file for client verification (optional)
		std::optional<std::string> ca_cert_file;

		//! Whether to require client certificate (mutual TLS)
		bool require_client_cert{false};

		//! ALPN protocols to negotiate
		std::vector<std::string> alpn_protocols;

		//! Maximum idle timeout in milliseconds (default: 30 seconds)
		uint64_t max_idle_timeout_ms{30000};

		//! Initial maximum data that can be sent (default: 1 MB)
		uint64_t initial_max_data{1048576};

		//! Initial maximum data per stream (default: 64 KB)
		uint64_t initial_max_stream_data{65536};

		//! Initial maximum bidirectional streams (default: 100)
		uint64_t initial_max_streams_bidi{100};

		//! Initial maximum unidirectional streams (default: 100)
		uint64_t initial_max_streams_uni{100};

		//! Maximum number of concurrent connections (default: 10000)
		size_t max_connections{10000};

		//! Enable retry token for DoS protection (default: true)
		bool enable_retry{true};

		//! Key for retry token validation (auto-generated if empty)
		std::vector<uint8_t> retry_key;
	};

	/*!
	 * \class messaging_quic_server
	 * \brief A QUIC server that manages incoming client connections
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 * It also implements the i_quic_server interface for composition-based usage.
	 *
	 * ### Overview
	 * Implements a QUIC (RFC 9000) server with an API consistent with the
	 * existing TCP-based messaging_server, while exposing QUIC-specific
	 * features like multiple concurrent streams and 0-RTT.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Session map is protected by shared_mutex for concurrent read access.
	 * - Atomic flags prevent race conditions.
	 * - Callbacks are invoked on I/O threads; implementations should be safe.
	 *
	 * ### Key Features
	 * - Uses \c asio::io_context for UDP I/O operations.
	 * - Manages multiple QUIC sessions concurrently.
	 * - Supports broadcast/multicast to connected clients.
	 * - Provides session lifecycle callbacks.
	 *
	 * ### Interface Compliance
	 * This class implements interfaces::i_quic_server for composition-based usage.
	 *
	 * ### Comparison with messaging_server (TCP)
	 * | Feature                  | messaging_server (TCP) | messaging_quic_server |
	 * |-------------------------|------------------------|----------------------|
	 * | start_server()           | ✓                      | ✓                    |
	 * | stop_server()            | ✓                      | ✓                    |
	 * | broadcast()              | ✗                      | ✓                    |
	 * | multicast()              | ✗                      | ✓                    |
	 * | TLS configuration        | ✗                      | ✓ (required)         |
	 * | Session management       | Basic                  | Advanced             |
	 */
	class messaging_quic_server
		: public std::enable_shared_from_this<messaging_quic_server>
		, public interfaces::i_quic_server
	{
	public:
		//! \brief Callback type for new connections
		using connection_callback_t = std::function<void(std::shared_ptr<session::quic_session>)>;
		//! \brief Callback type for disconnections
		using disconnection_callback_t = std::function<void(std::shared_ptr<session::quic_session>)>;
		//! \brief Callback type for received data (session, data)
		using receive_callback_t = std::function<void(std::shared_ptr<session::quic_session>,
		                                               const std::vector<uint8_t>&)>;
		//! \brief Callback type for stream data (session, stream_id, data, fin)
		using stream_receive_callback_t = std::function<void(std::shared_ptr<session::quic_session>,
		                                                      uint64_t,
		                                                      const std::vector<uint8_t>&,
		                                                      bool)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a QUIC server with a given identifier.
		 * \param server_id A string identifier for logging/debugging.
		 */
		explicit messaging_quic_server(std::string_view server_id);

		/*!
		 * \brief Destructor; automatically calls \c stop_server() if running.
		 */
		~messaging_quic_server() noexcept override;

		// Non-copyable, non-movable
		messaging_quic_server(const messaging_quic_server&) = delete;
		messaging_quic_server& operator=(const messaging_quic_server&) = delete;
		messaging_quic_server(messaging_quic_server&&) = delete;
		messaging_quic_server& operator=(messaging_quic_server&&) = delete;

		// =====================================================================
		// Server Lifecycle
		// =====================================================================

		/*!
		 * \brief Start the server with default configuration.
		 * \param port UDP port to listen on.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto start_server(unsigned short port) -> VoidResult;

		/*!
		 * \brief Start the server with TLS configuration.
		 * \param port UDP port to listen on.
		 * \param config Server configuration with TLS settings.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto start_server(unsigned short port,
		                                const quic_server_config& config)
		    -> VoidResult;

		/*!
		 * \brief Stops the server and releases all resources.
		 * \return Result<void> - Success if server stopped, or error with code.
		 */
		[[nodiscard]] auto stop_server() -> VoidResult;

		/*!
		 * \brief Returns the server identifier.
		 * \return The server_id string.
		 */
		[[nodiscard]] auto server_id() const -> const std::string&;

		// =====================================================================
		// Session Management
		// =====================================================================

		/*!
		 * \brief Get all active sessions.
		 * \return Vector of session pointers.
		 */
		[[nodiscard]] auto sessions() const
		    -> std::vector<std::shared_ptr<session::quic_session>>;

		/*!
		 * \brief Get a session by its ID.
		 * \param session_id Session identifier.
		 * \return Session pointer or nullptr if not found.
		 */
		[[nodiscard]] auto get_session(const std::string& session_id)
		    -> std::shared_ptr<session::quic_session>;

		/*!
		 * \brief Get the number of active sessions.
		 * \return Session count.
		 */
		[[nodiscard]] auto session_count() const -> size_t;

		/*!
		 * \brief Disconnect a specific session.
		 * \param session_id Session to disconnect.
		 * \param error_code Application error code (0 for no error).
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto disconnect_session(const std::string& session_id,
		                                      uint64_t error_code = 0)
		    -> VoidResult;

		/*!
		 * \brief Disconnect all active sessions.
		 * \param error_code Application error code (0 for no error).
		 */
		auto disconnect_all(uint64_t error_code = 0) -> void;

		// =====================================================================
		// Broadcasting
		// =====================================================================

		/*!
		 * \brief Send data to all connected clients.
		 * \param data Data to broadcast (moved for efficiency).
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto broadcast(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Send data to specific sessions.
		 * \param session_ids List of session IDs to send to.
		 * \param data Data to send (moved for efficiency).
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto multicast(const std::vector<std::string>& session_ids,
		                             std::vector<uint8_t>&& data) -> VoidResult;

		// =====================================================================
		// i_network_component interface implementation
		// =====================================================================

		/*!
		 * \brief Checks if the server is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override;

		/*!
		 * \brief Blocks until stop() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override;

		// =====================================================================
		// i_quic_server interface implementation
		// =====================================================================

		/*!
		 * \brief Starts the QUIC server on the specified port.
		 * \param port The port number to listen on.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_server::start(). Delegates to start_server().
		 */
		[[nodiscard]] auto start(uint16_t port) -> VoidResult override;

		/*!
		 * \brief Stops the QUIC server.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_server::stop(). Delegates to stop_server().
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Gets the number of active QUIC connections (interface version).
		 * \return The count of currently connected clients.
		 *
		 * Implements i_quic_server::connection_count().
		 */
		[[nodiscard]] auto connection_count() const -> size_t override;

		/*!
		 * \brief Sets the callback for new connections (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_server::set_connection_callback().
		 */
		auto set_connection_callback(interfaces::i_quic_server::connection_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for disconnections (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_server::set_disconnection_callback().
		 */
		auto set_disconnection_callback(interfaces::i_quic_server::disconnection_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for received data on default stream (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_server::set_receive_callback().
		 */
		auto set_receive_callback(interfaces::i_quic_server::receive_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for stream data (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_server::set_stream_callback().
		 */
		auto set_stream_callback(interfaces::i_quic_server::stream_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for errors (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_server::set_error_callback().
		 */
		auto set_error_callback(interfaces::i_quic_server::error_callback_t callback) -> void override;

		// =====================================================================
		// Legacy API (maintained for backward compatibility)
		// =====================================================================

		/*!
		 * \brief Sets the callback for new connections (legacy version).
		 * \param callback Function called when a client connects.
		 */
		auto set_connection_callback(connection_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for disconnections (legacy version).
		 * \param callback Function called when a client disconnects.
		 */
		auto set_disconnection_callback(disconnection_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for received data (legacy version).
		 * \param callback Function called when data is received.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for stream data reception (legacy version).
		 * \param callback Function called with session, stream ID, data, and FIN flag.
		 *
		 * \note This is kept for backward compatibility. New code should use
		 *       set_stream_callback() from the i_quic_server interface.
		 */
		auto set_stream_receive_callback(stream_receive_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for errors (legacy version).
		 * \param callback Function called when an error occurs.
		 */
		auto set_error_callback(error_callback_t callback) -> void;

#if KCENON_WITH_COMMON_SYSTEM
		/*!
		 * \brief Set a monitoring interface for metrics collection.
		 * \param monitor Pointer to IMonitor implementation (not owned).
		 */
		auto set_monitor(kcenon::common::interfaces::IMonitor* monitor) -> void;

		/*!
		 * \brief Get the current monitor.
		 * \return Pointer to monitor or nullptr if not set.
		 */
		auto get_monitor() const -> kcenon::common::interfaces::IMonitor*;
#endif // KCENON_WITH_COMMON_SYSTEM

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief QUIC-specific implementation of server start.
		 * \param port The UDP port to listen on.
		 * \return VoidResult - Success if server started, or error with code.
		 */
		auto do_start_impl(unsigned short port) -> VoidResult;

		/*!
		 * \brief QUIC-specific implementation of server stop.
		 * \return VoidResult - Success if server stopped, or error with code.
		 */
		auto do_stop_impl() -> VoidResult;

		auto start_receive() -> void;

		auto handle_packet(std::span<const uint8_t> data,
		                   const asio::ip::udp::endpoint& from) -> void;

		auto find_or_create_session(
		    const protocols::quic::connection_id& dcid,
		    const asio::ip::udp::endpoint& endpoint)
		    -> std::shared_ptr<session::quic_session>;

		auto generate_session_id() -> std::string;

		auto on_session_close(const std::string& session_id) -> void;

		auto start_cleanup_timer() -> void;

		auto cleanup_dead_sessions() -> void;

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Invokes the connection callback.
		 * \param session The new session.
		 */
		auto invoke_connection_callback(std::shared_ptr<session::quic_session> session) -> void;

		/*!
		 * \brief Invokes the disconnection callback.
		 * \param session The disconnected session.
		 */
		auto invoke_disconnection_callback(std::shared_ptr<session::quic_session> session) -> void;

		/*!
		 * \brief Invokes the receive callback.
		 * \param session The session that received data.
		 * \param data The received data.
		 */
		auto invoke_receive_callback(std::shared_ptr<session::quic_session> session,
		                             const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Invokes the stream receive callback.
		 * \param session The session that received data.
		 * \param stream_id The stream ID.
		 * \param data The received data.
		 * \param fin Whether this is the final data on the stream.
		 */
		auto invoke_stream_receive_callback(std::shared_ptr<session::quic_session> session,
		                                    uint64_t stream_id,
		                                    const std::vector<uint8_t>& data,
		                                    bool fin) -> void;

		/*!
		 * \brief Invokes the error callback.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

		//! \brief Callback index type alias for clarity
		using callback_index = quic_server_callback;

		//! \brief Callback manager type for this server
		using callbacks_t = utils::callback_manager<
			connection_callback_t,
			disconnection_callback_t,
			receive_callback_t,
			stream_receive_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string server_id_;                          /*!< Server identifier. */
		utils::lifecycle_manager lifecycle_;             /*!< Lifecycle state manager. */
		callbacks_t callbacks_;                          /*!< Callback manager. */

		std::unique_ptr<asio::io_context> io_context_;
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
		    work_guard_;
		std::unique_ptr<asio::ip::udp::socket> udp_socket_;
		std::shared_ptr<integration::thread_pool_interface> thread_pool_;
		std::future<void> io_context_future_;

		quic_server_config config_;

		// Session management
		mutable std::shared_mutex sessions_mutex_;
		std::map<std::string, std::shared_ptr<session::quic_session>> sessions_;

		// Receive buffer
		std::array<uint8_t, 65536> recv_buffer_;
		asio::ip::udp::endpoint recv_endpoint_;

		// Cleanup timer
		std::unique_ptr<asio::steady_timer> cleanup_timer_;

		// Session ID counter
		std::atomic<uint64_t> session_counter_{0};

		// Interface callback storage (separate from legacy callbacks)
		interfaces::i_quic_server::error_callback_t interface_error_cb_;

#if KCENON_WITH_COMMON_SYSTEM
		kcenon::common::interfaces::IMonitor* monitor_ = nullptr;
		std::atomic<uint64_t> messages_received_{0};
		std::atomic<uint64_t> messages_sent_{0};
		std::atomic<uint64_t> connection_errors_{0};
#endif // KCENON_WITH_COMMON_SYSTEM
	};

// =====================================================================
// Unified Pattern Type Aliases
// =====================================================================
// These aliases provide a consistent API pattern across all protocols,
// making QUIC servers accessible via the unified template naming.
// See: unified_messaging_server.h for TCP, unified_udp_messaging_server.h for UDP.

/*!
 * \brief Type alias for QUIC server.
 *
 * QUIC (RFC 9000) provides reliable, multiplexed, secure transport.
 * Note: QUIC always uses TLS 1.3 encryption - there is no "plain" QUIC variant.
 *
 * \code
 * auto server = std::make_shared<quic_server>("server1");
 * quic_server_config config{.cert_file = "cert.pem", .key_file = "key.pem"};
 * server->start_server(4433, config);
 * \endcode
 */
using quic_server = messaging_quic_server;

/*!
 * \brief Type alias for secure QUIC server (same as quic_server).
 *
 * QUIC inherently uses TLS 1.3 for all connections, so this alias
 * is provided for API consistency with other protocol patterns.
 * Both quic_server and secure_quic_server refer to the same implementation.
 *
 * \code
 * // Both are equivalent:
 * auto server1 = std::make_shared<quic_server>("server1");
 * auto server2 = std::make_shared<secure_quic_server>("server2");
 * \endcode
 */
using secure_quic_server = messaging_quic_server;

} // namespace kcenon::network::core
