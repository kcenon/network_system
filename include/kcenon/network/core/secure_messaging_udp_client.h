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

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>
#include <openssl/ssl.h>

#include "kcenon/network/core/messaging_client_base.h"
#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::internal
{
	class dtls_socket;
}

namespace kcenon::network::core
{
	/*!
	 * \class secure_messaging_udp_client
	 * \brief A secure UDP client using DTLS (Datagram TLS) for encrypted communication.
	 *
	 * This class inherits from messaging_client_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Socket access is protected by appropriate mutexes.
	 * - Atomic flags prevent race conditions.
	 *
	 * ### Key Characteristics
	 * - Uses DTLS 1.2/1.3 for encryption over UDP transport.
	 * - Performs DTLS handshake after socket setup.
	 * - Provides confidentiality and integrity for UDP datagrams.
	 * - Suitable for secure real-time communication.
	 *
	 * ### Usage Example
	 * \code
	 * auto client = std::make_shared<secure_messaging_udp_client>("SecureUDPClient");
	 *
	 * // Set callback to handle received datagrams (UDP-specific with endpoint)
	 * client->set_udp_receive_callback(
	 *     [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
	 *         std::cout << "Received " << data.size() << " encrypted bytes\n";
	 *     });
	 *
	 * // Start client with DTLS connection
	 * auto result = client->start_client("localhost", 5555);
	 * if (!result) {
	 *     std::cerr << "Failed to start: " << result.error().message << "\n";
	 *     return -1;
	 * }
	 *
	 * // Send encrypted datagram
	 * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	 * client->send_packet(std::move(data));
	 *
	 * // Stop client
	 * client->stop_client();
	 * \endcode
	 */
	class secure_messaging_udp_client
		: public messaging_client_base<secure_messaging_udp_client>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_client_base<secure_messaging_udp_client>;

		//! \brief UDP-specific receive callback type with sender endpoint
		using udp_receive_callback_t = std::function<void(const std::vector<uint8_t>&,
		                                                   const asio::ip::udp::endpoint&)>;

		/*!
		 * \brief Constructs a secure_messaging_udp_client with an identifier.
		 * \param client_id A string identifier for this client instance.
		 * \param verify_cert Whether to verify server certificate (default: true).
		 */
		secure_messaging_udp_client(std::string_view client_id, bool verify_cert = true);

		/*!
		 * \brief Destructor. Automatically calls stop_client() if still running
		 *        (handled by base class).
		 */
		~secure_messaging_udp_client() noexcept override = default;

		// Non-copyable (handled by base class)

		/*!
		 * \brief Sends an encrypted datagram to the server.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \param handler Optional completion handler.
		 * \return Result<void> - Success if send initiated.
		 */
		auto send_packet_with_handler(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> VoidResult;

		/*!
		 * \brief Sets a UDP-specific callback to handle received decrypted datagrams.
		 * \param callback Function with signature
		 *        void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		 *
		 * \note This is UDP-specific and includes sender endpoint information.
		 *       Use this instead of the base class set_receive_callback().
		 */
		auto set_udp_receive_callback(udp_receive_callback_t callback) -> void;

	protected:
		/*!
		 * \brief DTLS-specific implementation of client start.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started and handshake completed.
		 *
		 * Called by base class start_client() after common validation.
		 */
		auto do_start(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief DTLS-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped.
		 *
		 * Called by base class stop_client() after common cleanup.
		 */
		auto do_stop() -> VoidResult;

		/*!
		 * \brief DTLS-specific implementation of data send.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \return Result<void> - Success if send initiated.
		 *
		 * Called by base class send_packet() after common validation.
		 */
		auto do_send(std::vector<uint8_t>&& data) -> VoidResult;

	private:
		/*!
		 * \brief Initializes the SSL context for DTLS.
		 * \return Result<void> - Success if context initialized.
		 */
		auto init_ssl_context() -> VoidResult;

		/*!
		 * \brief Performs the DTLS handshake.
		 * \return Result<void> - Success if handshake completed.
		 */
		auto do_handshake() -> VoidResult;

	private:
		// DTLS protocol-specific members (base class provides client_id_,
		// is_running_, is_connected_, stop_initiated_, stop_promise_, stop_future_,
		// and standard callbacks)

		bool verify_cert_{true};                         /*!< Certificate verification flag. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::dtls_socket> socket_;  /*!< DTLS socket wrapper. */

		SSL_CTX* ssl_ctx_{nullptr};                      /*!< OpenSSL SSL context. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::mutex endpoint_mutex_;                      /*!< Protects target endpoint. */
		asio::ip::udp::endpoint target_endpoint_;        /*!< Target endpoint. */

		//! \brief UDP-specific receive callback (includes sender endpoint)
		std::mutex udp_callback_mutex_;
		udp_receive_callback_t udp_receive_callback_;
	};

} // namespace kcenon::network::core
