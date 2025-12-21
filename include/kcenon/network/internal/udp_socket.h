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

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <array>
#include <system_error>
#include <mutex>
#include <atomic>

#include "kcenon/network/internal/common_defs.h"

namespace kcenon::network::internal
{
	/*!
	 * \class udp_socket
	 * \brief A lightweight wrapper around \c asio::ip::udp::socket,
	 *        enabling asynchronous datagram operations.
	 *
	 * ### Key Features
	 * - Maintains a \c socket_ (from ASIO) for UDP communication.
	 * - Exposes \c set_receive_callback() to handle inbound datagrams
	 *   along with sender endpoint information.
	 * - Exposes \c set_error_callback() for error handling.
	 * - \c start_receive() begins an ongoing loop of \c async_receive_from().
	 * - \c async_send_to() performs an asynchronous send to a specified endpoint.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe. Callback registration is protected
	 *   by callback_mutex_.
	 * - ASIO operations are serialized through the io_context, ensuring
	 *   read_buffer_ is only accessed by one async operation at a time.
	 * - The provided callbacks will be invoked on an ASIO worker thread;
	 *   ensure that your callback logic is thread-safe if it shares data.
	 *
	 * ### UDP Characteristics
	 * - Connectionless: Each datagram is independent.
	 * - No guaranteed delivery: Packets may be lost, duplicated, or reordered.
	 * - Message boundaries preserved: Each receive corresponds to one send.
	 */
	class udp_socket : public std::enable_shared_from_this<udp_socket>
	{
	public:
		/*!
		 * \brief Constructs a \c udp_socket by taking ownership of a moved \p socket.
		 * \param socket An \c asio::ip::udp::socket that must be open or at least valid.
		 *
		 * After construction, you can call \c start_receive() to begin receiving datagrams.
		 * For sending, call \c async_send_to().
		 */
		udp_socket(asio::ip::udp::socket socket);

		/*!
		 * \brief Default destructor (no special cleanup needed).
		 */
		~udp_socket() = default;

		/*!
		 * \brief Sets a callback to receive inbound datagrams.
		 * \param callback A function with signature
		 *        \c void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&),
		 *        called whenever a datagram is successfully received.
		 *        The first parameter is the data, the second is the sender's endpoint.
		 *
		 * If no callback is set, received data is effectively discarded.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&,
			                   const asio::ip::udp::endpoint&)> callback) -> void;

		/*!
		 * \brief Sets a callback to handle socket errors (e.g., receive/send failures).
		 * \param callback A function with signature \c void(std::error_code),
		 *        invoked when any asynchronous operation fails.
		 *
		 * If no callback is set, errors are not explicitly handled here (beyond stopping receives).
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

		/*!
		 * \brief Begins the continuous asynchronous receive loop.
		 *
		 * Once called, the class repeatedly calls \c async_receive_from().
		 * If an error occurs, \c error_callback_ is triggered, stopping further receives.
		 */
		auto start_receive() -> void;

		/*!
		 * \brief Stops the receive loop to prevent further async operations.
		 */
		auto stop_receive() -> void;

		/*!
		 * \brief Initiates an asynchronous send of the given \p data to \p endpoint.
		 * \param data The buffer to send over UDP (moved for efficiency).
		 * \param endpoint The target \c asio::ip::udp::endpoint to send to.
		 * \param handler A completion handler with signature
		 *        \c void(std::error_code, std::size_t) invoked upon completion.
		 *
		 * The handler receives:
		 * - \c ec : the \c std::error_code from the send operation,
		 * - \c bytes_transferred : how many bytes were actually sent.
		 *
		 * ### Example
		 * \code
		 * auto sock = std::make_shared<kcenon::network::internal::udp_socket>(...);
		 * std::vector<uint8_t> buf = {0x01, 0x02, 0x03};
		 * asio::ip::udp::endpoint target(asio::ip::address::from_string("127.0.0.1"), 8080);
		 * sock->async_send_to(std::move(buf), target,
		 *     [](std::error_code ec, std::size_t len) {
		 *         if(ec) {
		 *             // handle error
		 *         } else {
		 *             // handle success
		 *         }
		 *     });
		 * \endcode
		 *
		 * \note Data is moved (not copied) to avoid memory allocation.
		 * \note The original vector will be empty after this call.
		 */
		auto async_send_to(
			std::vector<uint8_t>&& data,
			const asio::ip::udp::endpoint& endpoint,
			std::function<void(std::error_code, std::size_t)> handler) -> void;

		/*!
		 * \brief Provides direct access to the underlying \c asio::ip::udp::socket
		 *        in case advanced operations are needed.
		 * \return A reference to the wrapped \c asio::ip::udp::socket.
		 */
		auto socket() -> asio::ip::udp::socket& { return socket_; }

	private:
		/*!
		 * \brief Internal function to handle the receive logic with \c async_receive_from().
		 *
		 * Upon success, it calls \c receive_callback_ if set with both data and sender endpoint,
		 * then schedules another receive. On error, it calls \c error_callback_ if available.
		 */
		auto do_receive() -> void;

	private:
		asio::ip::udp::socket socket_; /*!< The underlying ASIO UDP socket. */

		std::array<uint8_t, 65536> read_buffer_; /*!< Buffer for receiving datagrams (max UDP size). */
		asio::ip::udp::endpoint sender_endpoint_; /*!< Stores the sender's endpoint during receive. */

		std::mutex callback_mutex_; /*!< Protects callback registration and access. */
		std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>
			receive_callback_; /*!< Inbound datagram callback. */
		std::function<void(std::error_code)>
			error_callback_; /*!< Error callback. */

		std::atomic<bool> is_receiving_{false}; /*!< Flag to prevent receive after stop. */
	};
} // namespace kcenon::network::internal
