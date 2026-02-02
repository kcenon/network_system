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
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include "internal/tcp/secure_tcp_socket.h"

namespace kcenon::network::session
{

	/*!
	 * \class secure_session
	 * \brief Manages a single connected secure (TLS/SSL) client session on the server side,
	 *        providing asynchronous encrypted read/write operations.
	 *
	 * ### Responsibilities
	 * - Owns a \c secure_tcp_socket for non-blocking encrypted I/O.
	 * - Performs SSL handshake before data transmission.
	 * - Optionally applies compression/encryption via \c pipeline_ before
	 * sending, and can do the reverse upon receiving data (if needed).
	 * - Provides callbacks (on_receive, on_error) for data handling and
	 * error detection.
	 *
	 * ### Lifecycle
	 * - Constructed with an accepted \c asio::ip::tcp::socket and SSL context.
	 * - \c start_session() performs SSL handshake and begins reading.
	 * - \c stop_session() closes the underlying socket, stopping further I/O.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Session state (is_stopped_) is protected by atomic operations.
	 * - Pipeline mode flags are protected by mode_mutex_.
	 * - Socket operations are serialized through ASIO's io_context.
	 */
	class secure_session
		: public std::enable_shared_from_this<secure_session>
	{
	public:
		/*!
		 * \brief Constructs a secure session with a given \p socket, SSL context, and \p server_id.
		 * \param socket    The \c asio::ip::tcp::socket (already connected).
		 * \param ssl_context SSL context for encryption settings.
		 * \param server_id An identifier for this server instance or context.
		 */
		secure_session(asio::ip::tcp::socket socket,
					   asio::ssl::context& ssl_context,
					   std::string_view server_id);

		/*!
		 * \brief Destructor; calls \c stop_session() if not already stopped.
		 */
		~secure_session() noexcept;

		/*!
		 * \brief Starts the session: performs SSL handshake, sets up read/error callbacks,
		 *        and begins reading data.
		 */
		auto start_session() -> void;

		/*!
		 * \brief Stops the session by closing the socket and marking the
		 * session as inactive.
		 */
		auto stop_session() -> void;

		/*!
		 * \brief Checks if the session has been stopped.
		 * \return true if the session is stopped, false otherwise.
		 */
		[[nodiscard]] auto is_stopped() const noexcept -> bool {
			return is_stopped_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Sends data to the connected client with encryption.
		 * \param data The raw bytes to transmit (moved for efficiency).
		 *
		 * ### Notes
		 * - If \c compress_mode_ or \c encrypt_mode_ is true, the data will be
		 * processed by the pipeline's compress/encrypt functions before
		 * writing.
		 * - Data is moved (not copied) to avoid memory allocation overhead.
		 * - After calling this function, the original vector will be empty.
		 */
		auto send_packet(std::vector<uint8_t>&& data) -> void;

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback Function called when data is received.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback Function called when the session stops.
		 */
		auto set_disconnection_callback(
			std::function<void(const std::string&)> callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback Function called when an error occurs.
		 */
		auto set_error_callback(
			std::function<void(std::error_code)> callback) -> void;

		/*!
		 * \brief Gets the server identifier.
		 * \return The server_id string.
		 */
		[[nodiscard]] auto server_id() const -> const std::string& {
			return server_id_;
		}

	private:
		/*!
		 * \brief Callback for when encrypted data arrives from the client.
		 * \param data A vector containing a chunk of received bytes.
		 *
		 * Override or extend the logic here to parse messages, handle commands,
		 * etc. If decompression/decryption is needed, apply \c pipeline_
		 * accordingly.
		 */
		auto on_receive(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Callback for handling socket errors from \c secure_tcp_socket.
		 * \param ec The \c std::error_code describing the error.
		 *
		 * By default, logs the error and calls \c stop_session().
		 */
		auto on_error(std::error_code ec) -> void;

		/*!
		 * \brief Processes pending messages from the queue.
		 *
		 * This method dequeues and processes messages one at a time.
		 * Implement actual message handling logic here.
		 */
		auto process_next_message() -> void;

	private:
		std::string server_id_; /*!< Identifier for the server side. */

		std::shared_ptr<internal::secure_tcp_socket>
			socket_;			/*!< The wrapped secure TCP socket for this session. */

		std::atomic<bool> is_stopped_{
			false
		}; /*!< Indicates whether this session is stopped. */

		/*!
		 * \brief Queue of pending received messages awaiting processing.
		 */
		std::deque<std::vector<uint8_t>> pending_messages_;

		/*!
		 * \brief Mutex protecting access to pending_messages_ queue.
		 */
		mutable std::mutex queue_mutex_;

		/*!
		 * \brief Maximum number of pending messages before applying backpressure.
		 *
		 * When this limit is reached, a warning is logged.
		 * If doubled, the session is disconnected.
		 */
		static constexpr size_t max_pending_messages_ = 1000;

		/*!
		 * \brief Callbacks for session events
		 */
		std::function<void(const std::vector<uint8_t>&)> receive_callback_;
		std::function<void(const std::string&)> disconnection_callback_;
		std::function<void(std::error_code)> error_callback_;

		/*!
		 * \brief Mutex protecting callback access
		 */
		mutable std::mutex callback_mutex_;
	};

} // namespace kcenon::network::session
