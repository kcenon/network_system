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

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include "network_system/internal/common_defs.h"

namespace network_system::internal
{
	/*!
	 * \class secure_tcp_socket
	 * \brief A lightweight wrapper around \c asio::ssl::stream<asio::ip::tcp::socket>,
	 *        enabling asynchronous TLS/SSL encrypted read and write operations.
	 *
	 * ### Key Features
	 * - Maintains a \c ssl_stream_ (from ASIO SSL) for secure TCP communication.
	 * - Performs SSL handshake before data transmission.
	 * - Exposes \c set_receive_callback() to handle inbound encrypted data
	 *   and \c set_error_callback() for error handling.
	 * - \c start_read() begins an ongoing loop of \c async_read_some().
	 * - \c async_send() performs an \c async_write of a given data buffer with encryption.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe. Callback registration is protected
	 *   by callback_mutex_.
	 * - ASIO operations are serialized through the io_context, ensuring
	 *   read_buffer_ is only accessed by one async operation at a time.
	 * - The provided callbacks will be invoked on an ASIO worker thread;
	 *   ensure that your callback logic is thread-safe if it shares data.
	 */
	class secure_tcp_socket : public std::enable_shared_from_this<secure_tcp_socket>
	{
	public:
		using ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>;

		/*!
		 * \brief Constructs a \c secure_tcp_socket by taking ownership of a moved \p
		 * socket and SSL context.
		 * \param socket An \c asio::ip::tcp::socket that must be open/connected
		 *               or at least valid.
		 * \param ssl_context SSL context for encryption settings
		 *
		 * After construction, you must call \c async_handshake() before using
		 * the socket for data transmission.
		 */
		secure_tcp_socket(asio::ip::tcp::socket socket,
						  asio::ssl::context& ssl_context);

		/*!
		 * \brief Default destructor (no special cleanup needed).
		 */
		~secure_tcp_socket() = default;

		/*!
		 * \brief Performs asynchronous SSL handshake.
		 * \param type Handshake type (client or server)
		 * \param handler Completion handler with signature void(std::error_code)
		 *
		 * Must be called before start_read() or async_send().
		 */
		auto async_handshake(
			asio::ssl::stream_base::handshake_type type,
			std::function<void(std::error_code)> handler) -> void;

		/*!
		 * \brief Sets a callback to receive inbound data chunks.
		 * \param callback A function with signature \c void(const
		 * std::vector<uint8_t>&), called whenever a chunk of data is
		 * successfully read and decrypted.
		 *
		 * If no callback is set, received data is effectively discarded.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets a callback to handle socket errors (e.g., read/write
		 * failures).
		 * \param callback A function with signature \c void(std::error_code),
		 *        invoked when any asynchronous operation fails.
		 *
		 * If no callback is set, errors are not explicitly handled here (beyond
		 * stopping reads).
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback)
			-> void;

		/*!
		 * \brief Begins the continuous asynchronous read loop.
		 *
		 * Once called, the class repeatedly calls \c async_read_some().
		 * If an error occurs, \c on_error() is triggered, stopping further
		 * reads.
		 */
		auto start_read() -> void;

		/*!
		 * \brief Initiates an asynchronous write of the given \p data buffer with encryption.
		 * \param data The buffer to send over TLS (moved for efficiency).
		 * \param handler A completion handler with signature \c
		 * void(std::error_code, std::size_t) that is invoked upon success or
		 * failure.
		 *
		 * The handler receives:
		 * - \c ec : the \c std::error_code from the write operation,
		 * - \c bytes_transferred : how many bytes were actually written.
		 *
		 * \note Data is moved (not copied) to avoid memory allocation overhead.
		 * \note The original vector will be empty after this call.
		 */
		auto async_send(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> void;

		/*!
		 * \brief Provides direct access to the underlying SSL stream.
		 * \return A reference to the wrapped \c asio::ssl::stream.
		 */
		auto stream() -> ssl_socket& { return ssl_stream_; }

		/*!
		 * \brief Provides access to the underlying TCP socket.
		 * \return A reference to the lowest layer socket.
		 */
		auto socket() -> ssl_socket::lowest_layer_type&
		{
			return ssl_stream_.lowest_layer();
		}

		/*!
		 * \brief Stops the read loop to prevent further async operations
		 */
		auto stop_read() -> void;

	private:
		/*!
		 * \brief Internal function to handle the read logic with \c
		 * async_read_some().
		 *
		 * Upon success, it calls \c receive_callback_ if set, then schedules
		 * another read. On error, it calls \c error_callback_ if available.
		 */
		auto do_read() -> void;

	private:
		ssl_socket ssl_stream_; /*!< The underlying ASIO SSL stream. */

		std::array<uint8_t, 4096>
			read_buffer_; /*!< Buffer for receiving data in \c do_read(). */

		std::mutex callback_mutex_; /*!< Protects callback registration and access. */
		std::function<void(const std::vector<uint8_t>&)>
			receive_callback_; /*!< Inbound data callback. */
		std::function<void(std::error_code)>
			error_callback_;   /*!< Error callback. */

		std::atomic<bool> is_reading_{false}; /*!< Flag to prevent read after stop. */
	};
} // namespace network_system::internal
