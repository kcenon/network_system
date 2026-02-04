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
#include "internal/experimental/experimental_api.h"
NETWORK_REQUIRE_EXPERIMENTAL

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <asio.hpp>

#include "internal/core/callback_indices.h"
#include "internal/core/network_context.h"
#include "internal/interfaces/i_quic_client.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/result_types.h"

// Forward declaration
namespace kcenon::network::internal
{
	class quic_socket;
	enum class quic_role : uint8_t;
} // namespace kcenon::network::internal

namespace kcenon::network::core
{

	/*!
	 * \struct quic_client_config
	 * \brief Configuration options for QUIC client
	 */
	struct quic_client_config
	{
		//! Path to CA certificate file for server verification (PEM format)
		std::optional<std::string> ca_cert_file;

		//! Path to client certificate file for mutual TLS (PEM format)
		std::optional<std::string> client_cert_file;

		//! Path to client private key file for mutual TLS (PEM format)
		std::optional<std::string> client_key_file;

		//! Whether to verify server certificate (default: true)
		bool verify_server{true};

		//! ALPN protocols to negotiate (e.g., {"h3", "hq-29"})
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

		//! Enable 0-RTT early data (default: false)
		bool enable_early_data{false};

		//! Session ticket for 0-RTT resumption
		std::optional<std::vector<uint8_t>> session_ticket;

		//! Maximum early data size in bytes (default: 16KB, 0 to disable)
		uint32_t max_early_data_size{16384};
	};

	/*!
	 * \struct quic_connection_stats
	 * \brief Statistics for a QUIC connection
	 */
	struct quic_connection_stats
	{
		uint64_t bytes_sent{0};              //!< Total bytes sent
		uint64_t bytes_received{0};          //!< Total bytes received
		uint64_t packets_sent{0};            //!< Total packets sent
		uint64_t packets_received{0};        //!< Total packets received
		uint64_t packets_lost{0};            //!< Total packets lost
		std::chrono::microseconds smoothed_rtt{0}; //!< Smoothed RTT
		std::chrono::microseconds min_rtt{0};      //!< Minimum RTT observed
		size_t cwnd{0};                      //!< Congestion window size
	};

	/*!
	 * \class messaging_quic_client
	 * \brief A QUIC client that provides reliable, multiplexed communication
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 * It also implements the i_quic_client interface for composition-based usage.
	 *
	 * ### Overview
	 * Implements a QUIC (RFC 9000) client with an API consistent with the
	 * existing TCP-based messaging_client, while exposing QUIC-specific
	 * features like multiple concurrent streams and 0-RTT.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Socket access is protected by internal locking.
	 * - Atomic flags prevent race conditions.
	 * - Callbacks are invoked on I/O threads; implementations should be safe.
	 *
	 * ### Key Features
	 * - Uses \c asio::io_context in a dedicated thread for I/O events.
	 * - Supports multiple concurrent streams (QUIC-specific).
	 * - Provides \c start_client(), \c stop_client(), and \c wait_for_stop()
	 *   for lifecycle control.
	 * - Full Result<T> error handling for all fallible operations.
	 *
	 * ### Interface Compliance
	 * This class implements interfaces::i_quic_client for composition-based usage.
	 *
	 * ### Comparison with messaging_client (TCP)
	 * | Feature              | messaging_client (TCP) | messaging_quic_client |
	 * |---------------------|------------------------|----------------------|
	 * | start_client()       | ✓                      | ✓                    |
	 * | stop_client()        | ✓                      | ✓                    |
	 * | send_packet()        | ✓                      | ✓                    |
	 * | set_receive_callback() | ✓                    | ✓                    |
	 * | create_stream()      | ✗                      | ✓ (QUIC specific)    |
	 * | send_on_stream()     | ✗                      | ✓ (QUIC specific)    |
	 * | 0-RTT               | ✗                      | ✓ (QUIC specific)    |
	 */
	class messaging_quic_client
		: public std::enable_shared_from_this<messaging_quic_client>
		, public interfaces::i_quic_client
	{
	public:
		//! \brief Callback type for received data
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for stream data (stream_id, data, fin)
		using stream_receive_callback_t = std::function<void(uint64_t, const std::vector<uint8_t>&, bool)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a QUIC client with a given identifier.
		 * \param client_id A string identifier for logging/debugging.
		 */
		explicit messaging_quic_client(std::string_view client_id);

		/*!
		 * \brief Destructor; automatically calls \c stop_client() if running.
		 */
		~messaging_quic_client() noexcept override;

		// Non-copyable, non-movable
		messaging_quic_client(const messaging_quic_client&) = delete;
		messaging_quic_client& operator=(const messaging_quic_client&) = delete;
		messaging_quic_client(messaging_quic_client&&) = delete;
		messaging_quic_client& operator=(messaging_quic_client&&) = delete;

		// =====================================================================
		// Lifecycle Management
		// =====================================================================

		/*!
		 * \brief Starts the client with default configuration.
		 * \param host Server hostname or IP address.
		 * \param port Server port number.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto start_client(std::string_view host,
		                                unsigned short port) -> VoidResult;

		/*!
		 * \brief Starts the client with TLS configuration.
		 * \param host Server hostname or IP address.
		 * \param port Server port number.
		 * \param config QUIC client configuration.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto start_client(std::string_view host,
		                                unsigned short port,
		                                const quic_client_config& config) -> VoidResult;

		/*!
		 * \brief Stops the client and releases all resources.
		 * \return Result<void> - Success if client stopped, or error with code.
		 */
		[[nodiscard]] auto stop_client() -> VoidResult;

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string.
		 */
		[[nodiscard]] auto client_id() const -> const std::string&;

		// =====================================================================
		// Data Transfer (Default Stream)
		// =====================================================================

		/*!
		 * \brief Send data on the default stream (stream 0).
		 * \param data Data to send (moved for efficiency).
		 * \return VoidResult indicating success or error.
		 *
		 * ### Errors
		 * - \c connection_closed if not connected
		 * - \c invalid_argument if data is empty
		 * - \c send_failed for other failures
		 *
		 * \code
		 * std::vector<uint8_t> data = {1, 2, 3, 4};
		 * auto result = client->send_packet(std::move(data));
		 * \endcode
		 */
		[[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Send string data on the default stream.
		 * \param data String to send.
		 * \return VoidResult indicating success or error.
		 *
		 * \code
		 * auto result = client->send_packet("Hello QUIC!");
		 * \endcode
		 */
		[[nodiscard]] auto send_packet(std::string_view data) -> VoidResult;

		/*!
		 * \brief Get connection statistics.
		 * \return Current connection stats.
		 */
		[[nodiscard]] auto stats() const -> quic_connection_stats;

		// =====================================================================
		// i_network_component interface implementation
		// =====================================================================

		/*!
		 * \brief Checks if the client is currently running.
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
		// i_quic_client interface implementation
		// =====================================================================

		/*!
		 * \brief Starts the QUIC client connecting to the specified server.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::start(). Delegates to start_client().
		 */
		[[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override;

		/*!
		 * \brief Stops the QUIC client.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::stop(). Delegates to stop_client().
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Checks if the client is connected (interface version).
		 * \return true if connected, false otherwise.
		 *
		 * Implements i_quic_client::is_connected().
		 */
		[[nodiscard]] auto is_connected() const -> bool override;

		/*!
		 * \brief Checks if TLS handshake is complete (interface version).
		 * \return true if handshake is done, false otherwise.
		 *
		 * Implements i_quic_client::is_handshake_complete().
		 */
		[[nodiscard]] auto is_handshake_complete() const -> bool override;

		/*!
		 * \brief Sends data on the default stream (interface version).
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::send().
		 */
		[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;

		/*!
		 * \brief Creates a new bidirectional stream (interface version).
		 * \return Stream ID or error.
		 *
		 * Implements i_quic_client::create_stream().
		 */
		[[nodiscard]] auto create_stream() -> Result<uint64_t> override;

		/*!
		 * \brief Creates a new unidirectional stream (interface version).
		 * \return Stream ID or error.
		 *
		 * Implements i_quic_client::create_unidirectional_stream().
		 */
		[[nodiscard]] auto create_unidirectional_stream() -> Result<uint64_t> override;

		/*!
		 * \brief Sends data on a specific stream (interface version).
		 * \param stream_id The target stream ID.
		 * \param data The data to send.
		 * \param fin True if this is the final data on the stream.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::send_on_stream().
		 */
		[[nodiscard]] auto send_on_stream(
			uint64_t stream_id,
			std::vector<uint8_t>&& data,
			bool fin = false) -> VoidResult override;

		/*!
		 * \brief Closes a stream (interface version).
		 * \param stream_id The stream to close.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::close_stream().
		 */
		[[nodiscard]] auto close_stream(uint64_t stream_id) -> VoidResult override;

		/*!
		 * \brief Sets the ALPN protocols for negotiation (interface version).
		 * \param protocols List of protocol identifiers.
		 *
		 * Implements i_quic_client::set_alpn_protocols().
		 */
		auto set_alpn_protocols(const std::vector<std::string>& protocols) -> void override;

		/*!
		 * \brief Gets the negotiated ALPN protocol (interface version).
		 * \return Protocol string if negotiated, empty optional otherwise.
		 *
		 * Implements i_quic_client::alpn_protocol().
		 */
		[[nodiscard]] auto alpn_protocol() const -> std::optional<std::string> override;

		/*!
		 * \brief Checks if early data was accepted (interface version).
		 * \return true if accepted, false otherwise.
		 *
		 * Implements i_quic_client::is_early_data_accepted().
		 */
		[[nodiscard]] auto is_early_data_accepted() const -> bool override;

		/*!
		 * \brief Sets the callback for received data on default stream (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_receive_callback().
		 */
		auto set_receive_callback(interfaces::i_quic_client::receive_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for stream data (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_stream_callback().
		 */
		auto set_stream_callback(interfaces::i_quic_client::stream_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for connection established (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_connected_callback().
		 */
		auto set_connected_callback(interfaces::i_quic_client::connected_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for disconnection (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_disconnected_callback().
		 */
		auto set_disconnected_callback(interfaces::i_quic_client::disconnected_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for errors (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_error_callback().
		 */
		auto set_error_callback(interfaces::i_quic_client::error_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for session tickets (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_session_ticket_callback().
		 */
		auto set_session_ticket_callback(interfaces::i_quic_client::session_ticket_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for early data production (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_early_data_callback().
		 */
		auto set_early_data_callback(interfaces::i_quic_client::early_data_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for early data acceptance notification (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_quic_client::set_early_data_accepted_callback().
		 */
		auto set_early_data_accepted_callback(interfaces::i_quic_client::early_data_accepted_callback_t callback) -> void override;

		// =====================================================================
		// Legacy API (maintained for backward compatibility)
		// =====================================================================

		/*!
		 * \brief Sets the callback for stream data reception (all streams, legacy version).
		 * \param callback Function called with stream ID, data, and FIN flag.
		 *
		 * \note This is kept for backward compatibility. New code should use
		 *       set_stream_callback() from the i_quic_client interface.
		 */
		auto set_stream_receive_callback(stream_receive_callback_t callback) -> void;

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief QUIC-specific implementation of client start.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult - Success if client started, or error with code.
		 */
		auto do_start_impl(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief QUIC-specific implementation of client stop.
		 * \return VoidResult - Success if client stopped, or error with code.
		 */
		auto do_stop_impl() -> VoidResult;

		/*!
		 * \brief Internal connection implementation.
		 */
		auto do_connect(std::string_view host, unsigned short port) -> void;

		/*!
		 * \brief Callback invoked when connection is established.
		 */
		auto on_connect() -> void;

		/*!
		 * \brief Callback for receiving stream data.
		 */
		auto on_stream_data(uint64_t stream_id,
		                    std::span<const uint8_t> data,
		                    bool fin) -> void;

		/*!
		 * \brief Callback for handling errors.
		 */
		auto on_error(std::error_code ec) -> void;

		/*!
		 * \brief Callback for connection close.
		 */
		auto on_close(uint64_t error_code, const std::string& reason) -> void;

		/*!
		 * \brief Get the internal socket with mutex protection.
		 */
		auto get_socket() const -> std::shared_ptr<internal::quic_socket>;

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Invokes the receive callback.
		 * \param data The received data.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Invokes the stream receive callback.
		 * \param stream_id The stream ID.
		 * \param data The received data.
		 * \param fin Whether this is the final data on the stream.
		 */
		auto invoke_stream_receive_callback(uint64_t stream_id,
		                                    const std::vector<uint8_t>& data,
		                                    bool fin) -> void;

		/*!
		 * \brief Invokes the connected callback.
		 */
		auto invoke_connected_callback() -> void;

		/*!
		 * \brief Invokes the disconnected callback.
		 */
		auto invoke_disconnected_callback() -> void;

		/*!
		 * \brief Invokes the error callback.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

		//! \brief Callback index type alias for clarity
		using callback_index = quic_client_callback;

		//! \brief Callback manager type for this client
		using callbacks_t = utils::callback_manager<
			receive_callback_t,
			stream_receive_callback_t,
			connected_callback_t,
			disconnected_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string client_id_;                          /*!< Client identifier. */
		utils::lifecycle_manager lifecycle_;             /*!< Lifecycle state manager. */
		callbacks_t callbacks_;                          /*!< Callback manager. */
		std::atomic<bool> is_connected_{false};          /*!< True if connected to remote. */

		std::unique_ptr<asio::io_context> io_context_;  //!< I/O context
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_; //!< Keeps io_context running
		std::shared_ptr<integration::thread_pool_interface>
			thread_pool_;                        //!< Thread pool for async ops
		std::future<void> io_context_future_;    //!< Future for io_context run

		mutable std::mutex socket_mutex_;  //!< Protects socket_ from data races
		std::shared_ptr<internal::quic_socket> socket_; //!< The QUIC socket

		quic_client_config config_;  //!< Client configuration
		uint64_t default_stream_id_{0}; //!< Default stream for send_packet()

		std::atomic<bool> handshake_complete_{false}; //!< TLS handshake status

		// 0-RTT callbacks
		interfaces::i_quic_client::session_ticket_callback_t session_ticket_cb_; //!< Session ticket callback
		interfaces::i_quic_client::early_data_callback_t early_data_cb_; //!< Early data production callback
		interfaces::i_quic_client::early_data_accepted_callback_t early_data_accepted_cb_; //!< Early data acceptance callback
		std::atomic<bool> early_data_accepted_{false}; //!< Early data acceptance status
	};

// =====================================================================
// Unified Pattern Type Aliases
// =====================================================================
// These aliases provide a consistent API pattern across all protocols,
// making QUIC clients accessible via the unified template naming.
// See: unified_messaging_client.h for TCP, unified_udp_messaging_client.h for UDP.

/*!
 * \brief Type alias for QUIC client.
 *
 * QUIC (RFC 9000) provides reliable, multiplexed, secure transport.
 * Note: QUIC always uses TLS 1.3 encryption - there is no "plain" QUIC variant.
 *
 * \code
 * auto client = std::make_shared<quic_client>("client1");
 * client->start_client("localhost", 4433);
 * \endcode
 */
using quic_client = messaging_quic_client;

/*!
 * \brief Type alias for secure QUIC client (same as quic_client).
 *
 * QUIC inherently uses TLS 1.3 for all connections, so this alias
 * is provided for API consistency with other protocol patterns.
 * Both quic_client and secure_quic_client refer to the same implementation.
 *
 * \code
 * // Both are equivalent:
 * auto client1 = std::make_shared<quic_client>("client1");
 * auto client2 = std::make_shared<secure_quic_client>("client2");
 * \endcode
 */
using secure_quic_client = messaging_quic_client;

} // namespace kcenon::network::core
