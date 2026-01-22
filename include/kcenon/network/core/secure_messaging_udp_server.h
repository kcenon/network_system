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

/**
 * @file secure_messaging_udp_server.h
 * @brief Legacy DTLS server class.
 *
 * @deprecated This header is deprecated. Use unified_udp_messaging_server.h instead.
 *
 * Migration guide:
 * @code
 * // Old code:
 * #include <kcenon/network/core/secure_messaging_udp_server.h>
 * auto server = std::make_shared<secure_messaging_udp_server>("server1");
 * server->set_certificate_chain_file("server.crt");
 * server->set_private_key_file("server.key");
 *
 * // New code:
 * #include <kcenon/network/core/unified_udp_messaging_server.h>
 * policy::tls_enabled tls_config{.cert_path = "server.crt", .key_path = "server.key"};
 * auto server = std::make_shared<secure_udp_server>("server1", tls_config);
 * @endcode
 *
 * @see unified_udp_messaging_server.h for the new template-based API
 */

#include <array>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
	 * \class secure_messaging_udp_server
	 * \brief A secure UDP server using DTLS (Datagram TLS) for encrypted communication.
	 *
	 * @deprecated Use unified_udp_messaging_server<tls_enabled> or secure_udp_server instead.
	 *
	 * This class uses the same lifecycle management pattern as other messaging
	 * classes (is_running_, stop_promise_, etc.) but with UDP-specific callbacks
	 * that include endpoint information.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Session management is protected by appropriate mutexes.
	 * - Atomic flags prevent race conditions.
	 *
	 * ### Key Characteristics
	 * - Uses DTLS 1.2/1.3 for encryption over UDP transport.
	 * - Manages multiple client sessions with individual DTLS contexts.
	 * - Provides confidentiality and integrity for UDP datagrams.
	 * - Suitable for secure real-time server applications.
	 *
	 * ### Usage Example
	 * \code
	 * auto server = std::make_shared<secure_messaging_udp_server>("SecureUDPServer");
	 *
	 * // Configure certificates
	 * server->set_certificate_chain_file("server.crt");
	 * server->set_private_key_file("server.key");
	 *
	 * // Set callback to handle received datagrams
	 * server->set_receive_callback(
	 *     [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
	 *         std::cout << "Received " << data.size() << " encrypted bytes\n";
	 *     });
	 *
	 * // Start server
	 * auto result = server->start_server(5555);
	 * if (!result) {
	 *     std::cerr << "Failed to start: " << result.error().message << "\n";
	 *     return -1;
	 * }
	 *
	 * // Send response to client
	 * server->async_send_to(response_data, client_endpoint, handler);
	 *
	 * // Stop server
	 * server->stop_server();
	 * \endcode
	 */
	class secure_messaging_udp_server : public std::enable_shared_from_this<secure_messaging_udp_server>
	{
	public:
		//! \brief UDP-specific callback types with endpoint information
		using udp_receive_callback_t = std::function<void(const std::vector<uint8_t>&,
		                                                   const asio::ip::udp::endpoint&)>;
		using udp_client_callback_t = std::function<void(const asio::ip::udp::endpoint&)>;

		/*!
		 * \brief Constructs a secure_messaging_udp_server with an identifier.
		 * \param server_id A descriptive identifier for this server instance.
		 */
		explicit secure_messaging_udp_server(std::string_view server_id);

		/*!
		 * \brief Destructor. Automatically calls stop_server() if still running.
		 */
		~secure_messaging_udp_server() noexcept;

		// Non-copyable
		secure_messaging_udp_server(const secure_messaging_udp_server&) = delete;
		secure_messaging_udp_server& operator=(const secure_messaging_udp_server&) = delete;

		/*!
		 * \brief Sets the certificate chain file for TLS.
		 * \param file_path Path to the certificate chain file (PEM format).
		 * \return Result<void> - Success if file loaded.
		 *
		 * Must be called before start_server().
		 */
		auto set_certificate_chain_file(const std::string& file_path) -> VoidResult;

		/*!
		 * \brief Sets the private key file for TLS.
		 * \param file_path Path to the private key file (PEM format).
		 * \return Result<void> - Success if file loaded.
		 *
		 * Must be called before start_server().
		 */
		auto set_private_key_file(const std::string& file_path) -> VoidResult;

		/*!
		 * \brief Starts the server and begins listening for DTLS connections.
		 * \param port The UDP port to bind to.
		 * \return Result<void> - Success if server started.
		 *
		 * Spawns a background thread for I/O operations.
		 * DTLS handshakes are performed automatically for new clients.
		 */
		auto start_server(uint16_t port) -> VoidResult;

		/*!
		 * \brief Stops the server and releases all resources.
		 * \return Result<void> - Success if server stopped.
		 */
		auto stop_server() -> VoidResult;

		/*!
		 * \brief Blocks the calling thread until the server is stopped.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Sends an encrypted datagram to a specific client.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \param endpoint The target client endpoint.
		 * \param handler Optional completion handler.
		 */
		auto async_send_to(
			std::vector<uint8_t>&& data,
			const asio::ip::udp::endpoint& endpoint,
			std::function<void(std::error_code, std::size_t)> handler = nullptr) -> void;

		/*!
		 * \brief Sets a UDP-specific callback to handle received decrypted datagrams.
		 * \param callback Function with signature
		 *        void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		 */
		auto set_receive_callback(udp_receive_callback_t callback) -> void;

		/*!
		 * \brief Sets a callback to handle errors.
		 * \param callback Function with signature void(std::error_code)
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

		/*!
		 * \brief Sets a UDP-specific callback for new client connection.
		 * \param callback Function called when a new client completes DTLS handshake.
		 */
		auto set_client_connected_callback(udp_client_callback_t callback) -> void;

		/*!
		 * \brief Sets a UDP-specific callback for client disconnection.
		 * \param callback Function called when a client session ends.
		 */
		auto set_client_disconnected_callback(udp_client_callback_t callback) -> void;

		/*!
		 * \brief Returns whether the server is currently running.
		 * \return true if server is running.
		 */
		[[nodiscard]] auto is_running() const noexcept -> bool
		{
			return is_running_.load(std::memory_order_acquire);
		}

		/*!
		 * \brief Returns the server identifier.
		 * \return The server_id string provided at construction.
		 */
		[[nodiscard]] auto server_id() const -> const std::string& { return server_id_; }

	private:
		/*!
		 * \brief DTLS session for a client.
		 */
		struct dtls_session
		{
			std::shared_ptr<internal::dtls_socket> socket;
			bool handshake_complete{false};
		};

		/*!
		 * \brief Initializes the SSL context for DTLS server.
		 * \return Result<void> - Success if context initialized.
		 */
		auto init_ssl_context() -> VoidResult;

		/*!
		 * \brief Handles incoming datagrams and routes them to appropriate sessions.
		 */
		auto do_receive() -> void;

		/*!
		 * \brief Processes received data for an existing session.
		 */
		auto process_session_data(const std::vector<uint8_t>& data,
		                          const asio::ip::udp::endpoint& sender) -> void;

		/*!
		 * \brief Creates a new DTLS session for a client.
		 */
		auto create_session(const asio::ip::udp::endpoint& client_endpoint)
			-> std::shared_ptr<dtls_session>;

		/*!
		 * \brief Hash function for endpoint (for unordered_map).
		 */
		struct endpoint_hash
		{
			std::size_t operator()(const asio::ip::udp::endpoint& ep) const
			{
				auto addr_hash = std::hash<std::string>{}(ep.address().to_string());
				auto port_hash = std::hash<unsigned short>{}(ep.port());
				return addr_hash ^ (port_hash << 1);
			}
		};

	private:
		// Lifecycle management (consistent with other messaging classes)
		std::string server_id_;                          /*!< Server identifier. */
		std::atomic<bool> is_running_{false};            /*!< Running state flag. */
		std::optional<std::promise<void>> stop_promise_; /*!< Used to signal wait_for_stop(). */
		std::future<void> stop_future_;                  /*!< Future that wait_for_stop() waits on. */

		// DTLS protocol-specific members
		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::unique_ptr<asio::ip::udp::socket> socket_;  /*!< Main UDP socket. */

		SSL_CTX* ssl_ctx_{nullptr};                      /*!< OpenSSL SSL context. */
		std::string cert_file_;                          /*!< Certificate file path. */
		std::string key_file_;                           /*!< Private key file path. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::array<uint8_t, 65536> read_buffer_;         /*!< Buffer for receiving datagrams. */
		asio::ip::udp::endpoint sender_endpoint_;        /*!< Current sender endpoint. */

		std::mutex sessions_mutex_;                      /*!< Protects sessions map. */
		std::unordered_map<asio::ip::udp::endpoint, std::shared_ptr<dtls_session>, endpoint_hash>
			sessions_;                                   /*!< Active DTLS sessions. */

		// UDP-specific callbacks (different signature from TCP servers)
		mutable std::mutex callback_mutex_;              /*!< Protects callbacks. */
		udp_receive_callback_t receive_callback_;
		std::function<void(std::error_code)> error_callback_;
		udp_client_callback_t client_connected_callback_;
		udp_client_callback_t client_disconnected_callback_;
	};

} // namespace kcenon::network::core
