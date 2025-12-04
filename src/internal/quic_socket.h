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

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <array>
#include <system_error>
#include <mutex>
#include <atomic>
#include <map>
#include <deque>

#include "common_defs.h"
#include "kcenon/network/protocols/quic/crypto.h"
#include "kcenon/network/protocols/quic/frame.h"
#include "kcenon/network/protocols/quic/packet.h"
#include "kcenon/network/utils/result_types.h"

namespace network_system::internal
{
	/*!
	 * \enum quic_role
	 * \brief Role of the QUIC endpoint (client or server)
	 */
	enum class quic_role : uint8_t
	{
		client = 0,
		server = 1
	};

	/*!
	 * \enum quic_connection_state
	 * \brief QUIC connection state machine states
	 */
	enum class quic_connection_state : uint8_t
	{
		idle = 0,              //!< Not yet started
		handshake_start = 1,   //!< Initiating handshake
		handshake = 2,         //!< Handshake in progress
		connected = 3,         //!< Connection established
		closing = 4,           //!< Closing connection
		draining = 5,          //!< Draining period before close
		closed = 6             //!< Connection closed
	};

	/*!
	 * \class quic_socket
	 * \brief A QUIC socket that wraps UDP and integrates QUIC packet protection
	 *
	 * ### Key Features
	 * - Wraps an existing \c asio::ip::udp::socket for UDP I/O
	 * - Integrates QUIC packet protection (encryption/decryption)
	 * - Handles packet parsing and building
	 * - Manages connection state at socket level
	 * - Supports stream-based data transfer
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe through internal locking
	 * - Callbacks are invoked on ASIO worker threads
	 * - Ensure callback implementations are thread-safe
	 *
	 * ### QUIC Protocol
	 * - Implements RFC 9000 connection semantics
	 * - Uses RFC 9001 TLS 1.3 for key negotiation
	 * - Supports multiple concurrent streams
	 */
	class quic_socket : public std::enable_shared_from_this<quic_socket>
	{
	public:
		/*!
		 * \brief Callback for receiving stream data
		 * \param stream_id The stream identifier
		 * \param data The received data
		 * \param fin True if this is the final data on the stream
		 */
		using stream_data_callback = std::function<void(
			uint64_t stream_id,
			std::span<const uint8_t> data,
			bool fin)>;

		/*!
		 * \brief Callback when connection is established
		 */
		using connected_callback = std::function<void()>;

		/*!
		 * \brief Callback for error handling
		 * \param ec The error code describing the error
		 */
		using error_callback = std::function<void(std::error_code)>;

		/*!
		 * \brief Callback when connection is closed
		 * \param error_code The QUIC error code
		 * \param reason Human-readable reason string
		 */
		using close_callback = std::function<void(uint64_t error_code, const std::string& reason)>;

		/*!
		 * \brief Constructs a QUIC socket
		 * \param socket An open UDP socket
		 * \param role Client or server role
		 */
		quic_socket(asio::ip::udp::socket socket, quic_role role);

		/*!
		 * \brief Destructor
		 */
		~quic_socket();

		// Non-copyable
		quic_socket(const quic_socket&) = delete;
		quic_socket& operator=(const quic_socket&) = delete;

		// Movable
		quic_socket(quic_socket&& other) noexcept;
		quic_socket& operator=(quic_socket&& other) noexcept;

		// =====================================================================
		// Callback Registration
		// =====================================================================

		/*!
		 * \brief Set callback for stream data reception
		 * \param cb Callback function
		 */
		auto set_stream_data_callback(stream_data_callback cb) -> void;

		/*!
		 * \brief Set callback for connection establishment
		 * \param cb Callback function
		 */
		auto set_connected_callback(connected_callback cb) -> void;

		/*!
		 * \brief Set callback for errors
		 * \param cb Callback function
		 */
		auto set_error_callback(error_callback cb) -> void;

		/*!
		 * \brief Set callback for connection close
		 * \param cb Callback function
		 */
		auto set_close_callback(close_callback cb) -> void;

		// =====================================================================
		// Connection Management
		// =====================================================================

		/*!
		 * \brief Connect to a remote server (client only)
		 * \param endpoint Remote endpoint to connect to
		 * \param server_name Server name for SNI (TLS)
		 * \return Success or error
		 */
		[[nodiscard]] auto connect(const asio::ip::udp::endpoint& endpoint,
		                           const std::string& server_name = "") -> VoidResult;

		/*!
		 * \brief Accept an incoming connection (server only)
		 * \param cert_file Path to certificate file (PEM format)
		 * \param key_file Path to private key file (PEM format)
		 * \return Success or error
		 */
		[[nodiscard]] auto accept(const std::string& cert_file,
		                          const std::string& key_file) -> VoidResult;

		/*!
		 * \brief Close the connection gracefully
		 * \param error_code Application error code (0 for no error)
		 * \param reason Human-readable reason string
		 * \return Success or error
		 */
		[[nodiscard]] auto close(uint64_t error_code = 0,
		                         const std::string& reason = "") -> VoidResult;

		// =====================================================================
		// I/O Operations
		// =====================================================================

		/*!
		 * \brief Start the receive loop
		 *
		 * Once called, the socket continuously receives UDP datagrams,
		 * processes QUIC packets, and invokes appropriate callbacks.
		 */
		auto start_receive() -> void;

		/*!
		 * \brief Stop the receive loop
		 */
		auto stop_receive() -> void;

		/*!
		 * \brief Send data on a stream
		 * \param stream_id Target stream ID
		 * \param data Data to send (moved for efficiency)
		 * \param fin True if this is the final data on the stream
		 * \return Success or error
		 */
		[[nodiscard]] auto send_stream_data(uint64_t stream_id,
		                                    std::vector<uint8_t>&& data,
		                                    bool fin = false) -> VoidResult;

		// =====================================================================
		// Stream Management
		// =====================================================================

		/*!
		 * \brief Create a new stream
		 * \param unidirectional True for unidirectional stream
		 * \return Stream ID or error
		 */
		[[nodiscard]] auto create_stream(bool unidirectional = false) -> Result<uint64_t>;

		/*!
		 * \brief Close a stream
		 * \param stream_id Stream to close
		 * \return Success or error
		 */
		[[nodiscard]] auto close_stream(uint64_t stream_id) -> VoidResult;

		// =====================================================================
		// State Queries
		// =====================================================================

		/*!
		 * \brief Check if the connection is established
		 * \return True if connected
		 */
		[[nodiscard]] auto is_connected() const noexcept -> bool;

		/*!
		 * \brief Check if the TLS handshake is complete
		 * \return True if handshake is done
		 */
		[[nodiscard]] auto is_handshake_complete() const noexcept -> bool;

		/*!
		 * \brief Get the current connection state
		 * \return Connection state
		 */
		[[nodiscard]] auto state() const noexcept -> quic_connection_state;

		/*!
		 * \brief Get the role (client or server)
		 * \return Role
		 */
		[[nodiscard]] auto role() const noexcept -> quic_role;

		/*!
		 * \brief Get the remote endpoint
		 * \return Remote endpoint
		 */
		[[nodiscard]] auto remote_endpoint() const -> asio::ip::udp::endpoint;

		/*!
		 * \brief Get the local connection ID
		 * \return Local connection ID
		 */
		[[nodiscard]] auto local_connection_id() const
			-> const protocols::quic::connection_id&;

		/*!
		 * \brief Get the remote connection ID
		 * \return Remote connection ID
		 */
		[[nodiscard]] auto remote_connection_id() const
			-> const protocols::quic::connection_id&;

		// =====================================================================
		// Socket Access
		// =====================================================================

		/*!
		 * \brief Access the underlying UDP socket
		 * \return Reference to the UDP socket
		 */
		auto socket() -> asio::ip::udp::socket& { return udp_socket_; }

		/*!
		 * \brief Access the underlying UDP socket (const)
		 * \return Const reference to the UDP socket
		 */
		auto socket() const -> const asio::ip::udp::socket& { return udp_socket_; }

	private:
		// =====================================================================
		// Internal Methods
		// =====================================================================

		/*!
		 * \brief Internal receive loop implementation
		 */
		auto do_receive() -> void;

		/*!
		 * \brief Handle received packet data
		 * \param data Raw packet bytes
		 */
		auto handle_packet(std::span<const uint8_t> data) -> void;

		/*!
		 * \brief Process a parsed frame
		 * \param f Frame to process
		 */
		auto process_frame(const protocols::quic::frame& f) -> void;

		/*!
		 * \brief Process CRYPTO frame data
		 * \param f Crypto frame
		 */
		auto process_crypto_frame(const protocols::quic::crypto_frame& f) -> void;

		/*!
		 * \brief Process STREAM frame data
		 * \param f Stream frame
		 */
		auto process_stream_frame(const protocols::quic::stream_frame& f) -> void;

		/*!
		 * \brief Process ACK frame
		 * \param f ACK frame
		 */
		auto process_ack_frame(const protocols::quic::ack_frame& f) -> void;

		/*!
		 * \brief Process CONNECTION_CLOSE frame
		 * \param f Connection close frame
		 */
		auto process_connection_close_frame(
			const protocols::quic::connection_close_frame& f) -> void;

		/*!
		 * \brief Process HANDSHAKE_DONE frame
		 */
		auto process_handshake_done_frame() -> void;

		/*!
		 * \brief Send pending outgoing packets
		 */
		auto send_pending_packets() -> void;

		/*!
		 * \brief Build and send a packet with frames
		 * \param level Encryption level
		 * \param frames Frames to include
		 * \return Success or error
		 */
		[[nodiscard]] auto send_packet(
			protocols::quic::encryption_level level,
			std::vector<protocols::quic::frame>&& frames) -> VoidResult;

		/*!
		 * \brief Queue crypto data for sending
		 * \param data Crypto data to send
		 */
		auto queue_crypto_data(std::vector<uint8_t>&& data) -> void;

		/*!
		 * \brief Determine encryption level from packet header
		 * \param header Packet header
		 * \return Encryption level
		 */
		[[nodiscard]] auto determine_encryption_level(
			const protocols::quic::packet_header& header) const noexcept
			-> protocols::quic::encryption_level;

		/*!
		 * \brief Generate a new connection ID
		 * \return Generated connection ID
		 */
		[[nodiscard]] auto generate_connection_id()
			-> protocols::quic::connection_id;

		/*!
		 * \brief Retransmission timeout handler
		 */
		auto on_retransmit_timeout() -> void;

		/*!
		 * \brief Transition to a new connection state
		 * \param new_state New state
		 */
		auto transition_state(quic_connection_state new_state) -> void;

		// =====================================================================
		// Member Variables
		// =====================================================================

		//! Underlying UDP socket
		asio::ip::udp::socket udp_socket_;

		//! Remote endpoint
		asio::ip::udp::endpoint remote_endpoint_;

		//! Receive buffer (max UDP datagram size)
		std::array<uint8_t, 65536> recv_buffer_;

		//! Socket role (client/server)
		quic_role role_;

		//! Connection state
		std::atomic<quic_connection_state> state_{quic_connection_state::idle};

		//! QUIC crypto handler
		protocols::quic::quic_crypto crypto_;

		//! Local connection ID
		protocols::quic::connection_id local_conn_id_;

		//! Remote connection ID
		protocols::quic::connection_id remote_conn_id_;

		//! Packet number for each encryption level
		std::array<uint64_t, 4> next_packet_number_{0, 0, 0, 0};

		//! Largest received packet number for each level
		std::array<uint64_t, 4> largest_received_pn_{0, 0, 0, 0};

		//! Next stream ID to allocate
		uint64_t next_stream_id_{0};

		// =====================================================================
		// Pending Data Queues
		// =====================================================================

		//! Pending crypto data to send per encryption level
		std::array<std::deque<std::vector<uint8_t>>, 4> pending_crypto_data_;

		//! Pending stream data to send (stream_id -> data queue)
		std::map<uint64_t, std::deque<std::pair<std::vector<uint8_t>, bool>>> pending_stream_data_;

		// =====================================================================
		// Callbacks
		// =====================================================================

		//! Mutex for callback protection
		mutable std::mutex callback_mutex_;

		//! Stream data callback
		stream_data_callback stream_data_cb_;

		//! Connected callback
		connected_callback connected_cb_;

		//! Error callback
		error_callback error_cb_;

		//! Close callback
		close_callback close_cb_;

		// =====================================================================
		// State Flags
		// =====================================================================

		//! Is receive loop running
		std::atomic<bool> is_receiving_{false};

		//! Is handshake complete
		std::atomic<bool> handshake_complete_{false};

		// =====================================================================
		// Timers
		// =====================================================================

		//! Retransmission timer
		asio::steady_timer retransmit_timer_;

		//! Idle timeout timer
		asio::steady_timer idle_timer_;

		//! Mutex for state protection
		mutable std::mutex state_mutex_;
	};

} // namespace network_system::internal
