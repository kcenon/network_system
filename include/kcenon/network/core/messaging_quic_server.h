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

#include <kcenon/common/config/feature_flags.h>

#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/core/messaging_quic_client.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/protocols/quic/connection_id.h"
#include "kcenon/network/utils/result_types.h"

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
	{
	public:
		/*!
		 * \brief Constructs a QUIC server with a given identifier.
		 * \param server_id A string identifier for logging/debugging.
		 */
		explicit messaging_quic_server(std::string_view server_id);

		/*!
		 * \brief Destructor; automatically calls \c stop_server() if running.
		 */
		~messaging_quic_server() noexcept;

		// Disable copy
		messaging_quic_server(const messaging_quic_server&) = delete;
		messaging_quic_server& operator=(const messaging_quic_server&) = delete;

		// =====================================================================
		// Server Lifecycle
		// =====================================================================

		/*!
		 * \brief Start the server on the specified port.
		 * \param port UDP port to listen on.
		 * \return VoidResult indicating success or error.
		 *
		 * ### Errors
		 * - \c server_already_running if already running
		 * - \c bind_failed if port binding failed
		 * - \c internal_error for other failures
		 *
		 * \code
		 * auto result = server->start_server(443);
		 * if (!result) {
		 *     std::cerr << "Failed: " << result.error().message << "\n";
		 *     return 1;
		 * }
		 * \endcode
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
		 * \brief Stop the server and close all connections.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto stop_server() -> VoidResult;

		/*!
		 * \brief Block until the server stops.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Check if the server is running.
		 * \return true if running.
		 */
		[[nodiscard]] auto is_running() const noexcept -> bool;

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
		// Callbacks
		// =====================================================================

		/*!
		 * \brief Set callback when a new client connects.
		 * \param callback Function called with the new session.
		 */
		auto set_connection_callback(
		    std::function<void(std::shared_ptr<session::quic_session>)> callback)
		    -> void;

		/*!
		 * \brief Set callback when a client disconnects.
		 * \param callback Function called with the disconnected session.
		 */
		auto set_disconnection_callback(
		    std::function<void(std::shared_ptr<session::quic_session>)> callback)
		    -> void;

		/*!
		 * \brief Set callback for received data from any session.
		 * \param callback Function called with session and received data.
		 */
		auto set_receive_callback(
		    std::function<void(std::shared_ptr<session::quic_session>,
		                       const std::vector<uint8_t>&)>
		        callback) -> void;

		/*!
		 * \brief Set callback for stream data from any session.
		 * \param callback Function with session, stream_id, data, and fin flag.
		 */
		auto set_stream_receive_callback(
		    std::function<void(std::shared_ptr<session::quic_session>,
		                       uint64_t stream_id,
		                       const std::vector<uint8_t>&,
		                       bool fin)>
		        callback) -> void;

		/*!
		 * \brief Set callback for server errors.
		 * \param callback Function called when errors occur.
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback)
		    -> void;

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
		// Internal Methods
		// =====================================================================

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
		// Member Variables
		// =====================================================================

		std::string server_id_;

		std::unique_ptr<asio::io_context> io_context_;
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
		    work_guard_;
		std::unique_ptr<asio::ip::udp::socket> udp_socket_;
		std::shared_ptr<integration::thread_pool_interface> thread_pool_;
		std::future<void> io_context_future_;

		std::optional<std::promise<void>> stop_promise_;
		std::future<void> stop_future_;

		std::atomic<bool> is_running_{false};

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

		// Callbacks
		std::function<void(std::shared_ptr<session::quic_session>)>
		    connection_callback_;
		std::function<void(std::shared_ptr<session::quic_session>)>
		    disconnection_callback_;
		std::function<void(std::shared_ptr<session::quic_session>,
		                   const std::vector<uint8_t>&)>
		    receive_callback_;
		std::function<void(std::shared_ptr<session::quic_session>,
		                   uint64_t,
		                   const std::vector<uint8_t>&,
		                   bool)>
		    stream_receive_callback_;
		std::function<void(std::error_code)> error_callback_;

		mutable std::mutex callback_mutex_;

#if KCENON_WITH_COMMON_SYSTEM
		kcenon::common::interfaces::IMonitor* monitor_ = nullptr;
		std::atomic<uint64_t> messages_received_{0};
		std::atomic<uint64_t> messages_sent_{0};
		std::atomic<uint64_t> connection_errors_{0};
#endif // KCENON_WITH_COMMON_SYSTEM
	};

} // namespace kcenon::network::core
