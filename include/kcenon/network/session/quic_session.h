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
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/core/messaging_quic_client.h"
#include "kcenon/network/utils/result_types.h"

namespace network_system::internal
{
	class quic_socket;
} // namespace network_system::internal

namespace network_system::session
{

	/*!
	 * \class quic_session
	 * \brief Represents a single QUIC client session on the server side.
	 *
	 * ### Overview
	 * This class wraps a QUIC connection from a connected client, providing
	 * methods to send/receive data, manage streams, and handle session lifecycle.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Session state is protected by atomic operations.
	 * - Socket access is protected by internal mutex.
	 * - Callbacks are invoked on I/O threads.
	 *
	 * ### Key Features
	 * - Manages a single client's QUIC connection
	 * - Supports multiple concurrent streams (QUIC feature)
	 * - Provides send/receive callbacks for data handling
	 * - Thread-safe session management
	 */
	class quic_session : public std::enable_shared_from_this<quic_session>
	{
	public:
		/*!
		 * \brief Constructs a QUIC session with an existing socket.
		 * \param socket The QUIC socket for this session.
		 * \param session_id Unique identifier for this session.
		 */
		quic_session(std::shared_ptr<internal::quic_socket> socket,
		             std::string_view session_id);

		/*!
		 * \brief Destructor; closes the session if still active.
		 */
		~quic_session() noexcept;

		// Disable copy
		quic_session(const quic_session&) = delete;
		quic_session& operator=(const quic_session&) = delete;

		// =====================================================================
		// Session Information
		// =====================================================================

		/*!
		 * \brief Get the unique session identifier.
		 * \return Session ID string.
		 */
		[[nodiscard]] auto session_id() const -> const std::string&;

		/*!
		 * \brief Get the remote endpoint address.
		 * \return Remote UDP endpoint.
		 */
		[[nodiscard]] auto remote_endpoint() const -> asio::ip::udp::endpoint;

		/*!
		 * \brief Check if the session is currently active.
		 * \return true if active, false if closed.
		 */
		[[nodiscard]] auto is_active() const noexcept -> bool;

		// =====================================================================
		// Data Transfer (Default Stream)
		// =====================================================================

		/*!
		 * \brief Send data on the default stream.
		 * \param data Data to send (moved for efficiency).
		 * \return VoidResult indicating success or error.
		 *
		 * ### Errors
		 * - \c connection_closed if session is not active
		 * - \c send_failed for transmission failures
		 */
		[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Send string data on the default stream.
		 * \param data String to send.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto send(std::string_view data) -> VoidResult;

		// =====================================================================
		// Multi-Stream Support (QUIC Specific)
		// =====================================================================

		/*!
		 * \brief Send data on a specific stream.
		 * \param stream_id Target stream ID.
		 * \param data Data to send (moved for efficiency).
		 * \param fin True if this is the final data on the stream.
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto send_on_stream(uint64_t stream_id,
		                                  std::vector<uint8_t>&& data,
		                                  bool fin = false) -> VoidResult;

		/*!
		 * \brief Create a new bidirectional stream to the client.
		 * \return Stream ID or error.
		 */
		[[nodiscard]] auto create_stream() -> Result<uint64_t>;

		// =====================================================================
		// Session Management
		// =====================================================================

		/*!
		 * \brief Close the session gracefully.
		 * \param error_code Application error code (0 for no error).
		 * \return VoidResult indicating success or error.
		 */
		[[nodiscard]] auto close(uint64_t error_code = 0) -> VoidResult;

		// =====================================================================
		// Statistics
		// =====================================================================

		/*!
		 * \brief Get connection statistics.
		 * \return Current connection stats.
		 */
		[[nodiscard]] auto stats() const -> core::quic_connection_stats;

		// =====================================================================
		// Callbacks
		// =====================================================================

		/*!
		 * \brief Set callback for received data on the default stream.
		 * \param callback Function called when data is received.
		 */
		auto set_receive_callback(
		    std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Set callback for stream data reception.
		 * \param callback Function with stream_id, data, and fin flag.
		 */
		auto set_stream_receive_callback(
		    std::function<void(uint64_t, const std::vector<uint8_t>&, bool)>
		        callback) -> void;

		/*!
		 * \brief Set callback when session closes.
		 * \param callback Function called on session close.
		 */
		auto set_close_callback(std::function<void()> callback) -> void;

		// =====================================================================
		// Internal Methods (for server use)
		// =====================================================================

		/*!
		 * \brief Start receiving data (called by server).
		 */
		auto start_session() -> void;

		/*!
		 * \brief Handle incoming packet (called by server).
		 * \param data Raw packet data.
		 */
		auto handle_packet(std::span<const uint8_t> data) -> void;

		/*!
		 * \brief Check if this session matches a connection ID.
		 * \param conn_id Connection ID to match.
		 * \return true if matches.
		 */
		[[nodiscard]] auto matches_connection_id(
		    const protocols::quic::connection_id& conn_id) const -> bool;

	private:
		// =====================================================================
		// Internal Callbacks
		// =====================================================================

		auto on_stream_data(uint64_t stream_id,
		                    std::span<const uint8_t> data,
		                    bool fin) -> void;

		auto on_error(std::error_code ec) -> void;

		auto on_close(uint64_t error_code, const std::string& reason) -> void;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string session_id_;

		mutable std::mutex socket_mutex_;
		std::shared_ptr<internal::quic_socket> socket_;

		std::atomic<bool> is_active_{true};

		uint64_t default_stream_id_{0};

		// Callbacks
		std::function<void(const std::vector<uint8_t>&)> receive_callback_;
		std::function<void(uint64_t, const std::vector<uint8_t>&, bool)>
		    stream_receive_callback_;
		std::function<void()> close_callback_;

		mutable std::mutex callback_mutex_;
	};

} // namespace network_system::session
