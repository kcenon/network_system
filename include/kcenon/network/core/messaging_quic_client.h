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

#include "kcenon/network/core/messaging_quic_client_base.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/interfaces/i_quic_client.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/utils/result_types.h"

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
#include <vector>

#include <asio.hpp>

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
	 * This class inherits from messaging_quic_client_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
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
	 *   for lifecycle control (inherited from base).
	 * - Full Result<T> error handling for all fallible operations.
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
		: public messaging_quic_client_base<messaging_quic_client>
		, public interfaces::i_quic_client
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_quic_client_base<messaging_quic_client>;

		/*!
		 * \brief Constructs a QUIC client with a given identifier.
		 * \param client_id A string identifier for logging/debugging.
		 */
		explicit messaging_quic_client(std::string_view client_id);

		/*!
		 * \brief Destructor; automatically calls \c stop_client() if running.
		 * (Handled by base class)
		 */
		~messaging_quic_client() noexcept override = default;

		// Disable copy (inherited from base)
		messaging_quic_client(const messaging_quic_client&) = delete;
		messaging_quic_client& operator=(const messaging_quic_client&) = delete;

		// =====================================================================
		// Connection Management (Extended)
		// =====================================================================

		// stop_client(), wait_for_stop(), is_running(),
		// is_connected(), client_id() are provided by base class

		//! \brief Bring base class start_client into scope
		using messaging_quic_client_base<messaging_quic_client>::start_client;

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
		// i_quic_client interface implementation
		// =====================================================================

		/*!
		 * \brief Checks if the client is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override {
			return messaging_quic_client_base::is_running();
		}

		/*!
		 * \brief Blocks until stop() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override {
			messaging_quic_client_base::wait_for_stop();
		}

		/*!
		 * \brief Starts the QUIC client connecting to the specified server.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::start(). Delegates to start_client().
		 */
		[[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override {
			return start_client(host, port);
		}

		/*!
		 * \brief Stops the QUIC client.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::stop(). Delegates to stop_client().
		 */
		[[nodiscard]] auto stop() -> VoidResult override {
			return stop_client();
		}

		/*!
		 * \brief Checks if the client is connected (interface version).
		 * \return true if connected, false otherwise.
		 *
		 * Implements i_quic_client::is_connected().
		 */
		[[nodiscard]] auto is_connected() const -> bool override {
			return messaging_quic_client_base::is_connected();
		}

		/*!
		 * \brief Checks if TLS handshake is complete (interface version).
		 * \return true if handshake is done, false otherwise.
		 *
		 * Implements i_quic_client::is_handshake_complete().
		 */
		[[nodiscard]] auto is_handshake_complete() const -> bool override {
			return is_handshake_complete_impl();
		}

		/*!
		 * \brief Sends data on the default stream (interface version).
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_quic_client::send().
		 */
		[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override {
			return send_packet(std::move(data));
		}

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
		[[nodiscard]] auto is_early_data_accepted() const -> bool override {
			return is_early_data_accepted_impl();
		}

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

		//! \brief Legacy callback setters from base class
		using messaging_quic_client_base::set_receive_callback;
		using messaging_quic_client_base::set_stream_receive_callback;
		using messaging_quic_client_base::set_connected_callback;
		using messaging_quic_client_base::set_disconnected_callback;
		using messaging_quic_client_base::set_error_callback;

	protected:
		// =====================================================================
		// CRTP Implementation Methods
		// =====================================================================

		/*!
		 * \brief QUIC-specific implementation of client start.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult - Success if client started, or error with code.
		 *
		 * Called by base class start_client() after common validation.
		 */
		auto do_start(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief QUIC-specific implementation of client stop.
		 * \return VoidResult - Success if client stopped, or error with code.
		 *
		 * Called by base class stop_client() after common cleanup.
		 */
		auto do_stop() -> VoidResult;

	private:
		// =====================================================================
		// Internal Methods
		// =====================================================================

		/*!
		 * \brief Internal implementation for is_handshake_complete.
		 */
		[[nodiscard]] auto is_handshake_complete_impl() const noexcept -> bool;

		/*!
		 * \brief Internal implementation for is_early_data_accepted.
		 */
		[[nodiscard]] auto is_early_data_accepted_impl() const noexcept -> bool;

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
		// Member Variables
		// =====================================================================

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

} // namespace kcenon::network::core
