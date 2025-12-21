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

#include <memory>
#include <string>
#include <string_view>
#include <atomic>
#include <functional>
#include <mutex>

#include <asio.hpp>
#include <openssl/ssl.h>

#include "kcenon/network/utils/result_types.h"
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
	 * // Set callback to handle received datagrams
	 * client->set_receive_callback(
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
	class secure_messaging_udp_client : public std::enable_shared_from_this<secure_messaging_udp_client>
	{
	public:
		/*!
		 * \brief Constructs a secure_messaging_udp_client with an identifier.
		 * \param client_id A string identifier for this client instance.
		 * \param verify_cert Whether to verify server certificate (default: true).
		 */
		secure_messaging_udp_client(std::string_view client_id, bool verify_cert = true);

		/*!
		 * \brief Destructor. Automatically calls stop_client() if still running.
		 */
		~secure_messaging_udp_client() noexcept;

		// Non-copyable
		secure_messaging_udp_client(const secure_messaging_udp_client&) = delete;
		secure_messaging_udp_client& operator=(const secure_messaging_udp_client&) = delete;

		/*!
		 * \brief Starts the client and establishes DTLS connection.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started and handshake completed.
		 *
		 * Creates a UDP socket, resolves the target endpoint, and performs
		 * DTLS handshake. Spawns a background thread for I/O operations.
		 */
		auto start_client(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief Stops the client and releases resources.
		 * \return Result<void> - Success if client stopped.
		 */
		auto stop_client() -> VoidResult;

		/*!
		 * \brief Blocks the calling thread until the client is stopped.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Sends an encrypted datagram to the server.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \param handler Optional completion handler.
		 * \return Result<void> - Success if send initiated.
		 */
		auto send_packet(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler = nullptr) -> VoidResult;

		/*!
		 * \brief Sets a callback to handle received decrypted datagrams.
		 * \param callback Function with signature
		 *        void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&,
			                   const asio::ip::udp::endpoint&)> callback) -> void;

		/*!
		 * \brief Sets a callback to handle errors.
		 * \param callback Function with signature void(std::error_code)
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

		/*!
		 * \brief Sets a callback for connection established event.
		 * \param callback Function called when DTLS handshake completes.
		 */
		auto set_connected_callback(std::function<void()> callback) -> void;

		/*!
		 * \brief Sets a callback for disconnection event.
		 * \param callback Function called when client disconnects.
		 */
		auto set_disconnected_callback(std::function<void()> callback) -> void;

		/*!
		 * \brief Returns whether the client is currently connected.
		 * \return true if connected and DTLS handshake completed.
		 */
		auto is_connected() const -> bool { return is_connected_.load(); }

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string provided at construction.
		 */
		auto client_id() const -> const std::string& { return client_id_; }

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
		std::string client_id_;                          /*!< Client identifier. */
		std::atomic<bool> is_connected_{false};          /*!< Connection state flag. */
		bool verify_cert_{true};                         /*!< Certificate verification flag. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::dtls_socket> socket_;  /*!< DTLS socket wrapper. */

		SSL_CTX* ssl_ctx_{nullptr};                      /*!< OpenSSL SSL context. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::mutex endpoint_mutex_;                      /*!< Protects target endpoint. */
		asio::ip::udp::endpoint target_endpoint_;        /*!< Target endpoint. */

		std::mutex callback_mutex_;                      /*!< Protects callbacks. */
		std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)>
			receive_callback_;
		std::function<void(std::error_code)> error_callback_;
		std::function<void()> connected_callback_;
		std::function<void()> disconnected_callback_;
	};

} // namespace kcenon::network::core
