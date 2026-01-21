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
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>
#include <openssl/ssl.h>

#include "kcenon/network/core/callback_indices.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/result_types.h"

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
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
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
		: public std::enable_shared_from_this<secure_messaging_udp_client>
	{
	public:
		//! \brief Callback type for received data
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

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
		 * \brief Destructor. Automatically calls stop_client() if still running.
		 */
		~secure_messaging_udp_client() noexcept;

		// Non-copyable, non-movable
		secure_messaging_udp_client(const secure_messaging_udp_client&) = delete;
		secure_messaging_udp_client& operator=(const secure_messaging_udp_client&) = delete;
		secure_messaging_udp_client(secure_messaging_udp_client&&) = delete;
		secure_messaging_udp_client& operator=(secure_messaging_udp_client&&) = delete;

		// =====================================================================
		// Lifecycle Management
		// =====================================================================

		/*!
		 * \brief Starts the client and connects to the specified host and port.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number to connect.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::common_errors::already_exists if already running
		 *         - error_codes::common_errors::invalid_argument if empty host
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		[[nodiscard]] auto start_client(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief Stops the client and disconnects.
		 * \return Result<void> - Success if client stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
		 */
		[[nodiscard]] auto stop_client() -> VoidResult;

		/*!
		 * \brief Blocks until stop_client() is called.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Checks if the client is currently running.
		 * \return true if running, false otherwise.
		 */
		[[nodiscard]] auto is_running() const noexcept -> bool;

		/*!
		 * \brief Checks if the client is connected to the server.
		 * \return true if connected, false otherwise.
		 */
		[[nodiscard]] auto is_connected() const noexcept -> bool;

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string.
		 */
		[[nodiscard]] auto client_id() const -> const std::string&;

		// =====================================================================
		// Data Transfer
		// =====================================================================

		/*!
		 * \brief Sends an encrypted datagram to the server.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \return Result<void> - Success if send initiated, or error.
		 */
		[[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Sends an encrypted datagram with a completion handler.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \param handler Optional completion handler.
		 * \return Result<void> - Success if send initiated.
		 */
		auto send_packet_with_handler(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> VoidResult;

		// =====================================================================
		// Callback Setters
		// =====================================================================

		/*!
		 * \brief Sets a UDP-specific callback to handle received decrypted datagrams.
		 * \param callback Function with signature
		 *        void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		 *
		 * \note This is UDP-specific and includes sender endpoint information.
		 *       Use this instead of set_receive_callback().
		 */
		auto set_udp_receive_callback(udp_receive_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback Function called when data is received.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for connection established.
		 * \param callback Function called when connection is established.
		 */
		auto set_connected_callback(connected_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback Function called when disconnected.
		 */
		auto set_disconnected_callback(disconnected_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback Function called when an error occurs.
		 */
		auto set_error_callback(error_callback_t callback) -> void;

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief DTLS-specific implementation of client start.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started and handshake completed.
		 */
		auto do_start_impl(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief DTLS-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped.
		 */
		auto do_stop_impl() -> VoidResult;

		/*!
		 * \brief DTLS-specific implementation of data send.
		 * \param data The plaintext data to encrypt and send (moved for efficiency).
		 * \return Result<void> - Success if send initiated.
		 */
		auto do_send_impl(std::vector<uint8_t>&& data) -> VoidResult;

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

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Sets the connected state.
		 * \param connected The new connection state.
		 */
		auto set_connected(bool connected) -> void;

		/*!
		 * \brief Invokes the receive callback.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Invokes the connected callback.
		 */
		auto invoke_connected_callback() -> void;

		/*!
		 * \brief Invokes the disconnected callback.
		 */
		auto invoke_disconnected_callback() -> void;

		/*!
		 * \brief Invokes the error callback.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

	private:
		//! \brief Callback index type alias for clarity
		using callback_index = secure_udp_client_callback;

		//! \brief Callback manager type for this client
		using callbacks_t = utils::callback_manager<
			receive_callback_t,
			connected_callback_t,
			disconnected_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string client_id_;              /*!< Client identifier. */
		utils::lifecycle_manager lifecycle_; /*!< Lifecycle state manager. */
		callbacks_t callbacks_;              /*!< Callback manager. */
		std::atomic<bool> is_connected_{false}; /*!< Connection state. */

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
