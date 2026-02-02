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
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>

#include <asio.hpp>

#include "internal/utils/common_defs.h"
#include "internal/utils/openssl_compat.h"

namespace kcenon::network::internal
{
	/*!
	 * \class dtls_socket
	 * \brief A wrapper around ASIO UDP socket with OpenSSL DTLS encryption.
	 *
	 * ### Key Features
	 * - Provides DTLS (Datagram TLS) encryption over UDP transport.
	 * - Uses OpenSSL's DTLS implementation with memory BIOs for async I/O.
	 * - Exposes \c set_receive_callback() to handle decrypted inbound datagrams
	 *   and \c set_error_callback() for error handling.
	 * - \c start_receive() begins an ongoing loop of receiving encrypted datagrams.
	 * - \c async_send() encrypts and sends data to the configured peer.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe. Callback registration is protected
	 *   by callback_mutex_.
	 * - ASIO operations are serialized through the io_context.
	 * - OpenSSL operations are protected by ssl_mutex_.
	 * - The provided callbacks will be invoked on an ASIO worker thread;
	 *   ensure that your callback logic is thread-safe if it shares data.
	 *
	 * ### DTLS Characteristics
	 * - Provides confidentiality and integrity for UDP datagrams.
	 * - Handles packet loss and reordering during handshake.
	 * - Message boundaries are preserved (each receive is one datagram).
	 * - Suitable for real-time applications requiring encryption.
	 */
	class dtls_socket : public std::enable_shared_from_this<dtls_socket>
	{
	public:
		/*!
		 * \brief Handshake type enumeration.
		 */
		enum class handshake_type
		{
			client, /*!< Client-side handshake */
			server  /*!< Server-side handshake */
		};

		/*!
		 * \brief Constructs a \c dtls_socket with an existing UDP socket.
		 * \param socket An \c asio::ip::udp::socket that must be open.
		 * \param ssl_ctx The OpenSSL SSL_CTX configured for DTLS.
		 *
		 * The socket should be connected (for client) or bound (for server)
		 * before calling handshake methods.
		 */
		dtls_socket(asio::ip::udp::socket socket, SSL_CTX* ssl_ctx);

		/*!
		 * \brief Destructor. Cleans up OpenSSL resources.
		 */
		~dtls_socket();

		// Non-copyable, non-movable
		dtls_socket(const dtls_socket&) = delete;
		dtls_socket& operator=(const dtls_socket&) = delete;
		dtls_socket(dtls_socket&&) = delete;
		dtls_socket& operator=(dtls_socket&&) = delete;

		/*!
		 * \brief Performs asynchronous DTLS handshake.
		 * \param type Handshake type (client or server)
		 * \param handler Completion handler with signature void(std::error_code)
		 *
		 * Must be called before start_receive() or async_send().
		 * The handshake involves multiple round-trips over UDP.
		 */
		auto async_handshake(
			handshake_type type,
			std::function<void(std::error_code)> handler) -> void;

		/*!
		 * \brief Sets a callback to receive decrypted inbound datagrams.
		 * \param callback A function with signature
		 *        \c void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&),
		 *        called whenever a datagram is successfully received and decrypted.
		 *
		 * If no callback is set, received data is effectively discarded.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&,
			                   const asio::ip::udp::endpoint&)> callback) -> void;

		/*!
		 * \brief Sets a callback to handle socket errors.
		 * \param callback A function with signature \c void(std::error_code),
		 *        invoked when any asynchronous operation fails.
		 *
		 * If no callback is set, errors are not explicitly handled here.
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback)
			-> void;

		/*!
		 * \brief Begins the continuous asynchronous receive loop.
		 *
		 * Once called, the class repeatedly receives encrypted datagrams,
		 * decrypts them, and invokes the receive callback.
		 * If an error occurs, the error callback is triggered.
		 */
		auto start_receive() -> void;

		/*!
		 * \brief Stops the receive loop.
		 */
		auto stop_receive() -> void;

		/*!
		 * \brief Initiates an asynchronous encrypted send.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \param handler A completion handler with signature
		 *        \c void(std::error_code, std::size_t) invoked upon completion.
		 *
		 * The data is encrypted using DTLS before transmission.
		 * \note Data is moved (not copied) to avoid memory allocation overhead.
		 */
		auto async_send(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> void;

		/*!
		 * \brief Initiates an asynchronous encrypted send to a specific endpoint.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \param endpoint The target endpoint.
		 * \param handler A completion handler invoked upon completion.
		 *
		 * Useful for server responding to different clients.
		 */
		auto async_send_to(
			std::vector<uint8_t>&& data,
			const asio::ip::udp::endpoint& endpoint,
			std::function<void(std::error_code, std::size_t)> handler) -> void;

		/*!
		 * \brief Sets the peer endpoint for connected mode.
		 * \param endpoint The peer's UDP endpoint.
		 */
		auto set_peer_endpoint(const asio::ip::udp::endpoint& endpoint) -> void;

		/*!
		 * \brief Returns the peer endpoint.
		 * \return The configured peer endpoint.
		 */
		auto peer_endpoint() const -> asio::ip::udp::endpoint;

		/*!
		 * \brief Provides direct access to the underlying UDP socket.
		 * \return A reference to the wrapped \c asio::ip::udp::socket.
		 */
		auto socket() -> asio::ip::udp::socket& { return socket_; }

		/*!
		 * \brief Checks if the DTLS handshake is complete.
		 * \return true if handshake completed successfully.
		 */
		auto is_handshake_complete() const -> bool
		{
			return handshake_complete_.load();
		}

	private:
		/*!
		 * \brief Internal function to handle the receive logic.
		 */
		auto do_receive() -> void;

		/*!
		 * \brief Processes received encrypted data through DTLS.
		 * \param data The encrypted datagram.
		 * \param sender The sender's endpoint.
		 */
		auto process_received_data(const std::vector<uint8_t>& data,
		                           const asio::ip::udp::endpoint& sender) -> void;

		/*!
		 * \brief Flushes pending DTLS output to the network.
		 */
		auto flush_bio_output() -> void;

		/*!
		 * \brief Continues the handshake process.
		 */
		auto continue_handshake() -> void;

		/*!
		 * \brief Creates an OpenSSL error code from the current error state.
		 * \return std::error_code representing the SSL error.
		 */
		auto make_ssl_error() const -> std::error_code;

	private:
		asio::ip::udp::socket socket_;              /*!< The underlying UDP socket. */
		asio::ip::udp::endpoint peer_endpoint_;     /*!< Peer endpoint for connected mode. */
		asio::ip::udp::endpoint sender_endpoint_;   /*!< Sender endpoint for receives. */

		SSL_CTX* ssl_ctx_;                          /*!< OpenSSL context (not owned). */
		SSL* ssl_;                                  /*!< OpenSSL SSL object. */
		BIO* rbio_;                                 /*!< Read BIO (memory). */
		BIO* wbio_;                                 /*!< Write BIO (memory). */

		std::array<uint8_t, 65536> read_buffer_;    /*!< Buffer for receiving datagrams. */

		std::mutex ssl_mutex_;                      /*!< Protects SSL operations. */
		std::mutex callback_mutex_;                 /*!< Protects callback registration. */
		std::mutex endpoint_mutex_;                 /*!< Protects endpoint access. */

		std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>
			receive_callback_;                      /*!< Inbound data callback. */
		std::function<void(std::error_code)>
			error_callback_;                        /*!< Error callback. */
		std::function<void(std::error_code)>
			handshake_callback_;                    /*!< Handshake completion callback. */

		std::atomic<bool> is_receiving_{false};     /*!< Receive loop active flag. */
		std::atomic<bool> handshake_complete_{false}; /*!< Handshake completed flag. */
		std::atomic<bool> handshake_in_progress_{false}; /*!< Handshake in progress flag. */
		handshake_type handshake_type_{handshake_type::client}; /*!< Handshake mode. */
	};

} // namespace kcenon::network::internal
