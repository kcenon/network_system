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
#include <asio/ssl.hpp>

#include "kcenon/network/core/messaging_client_base.h"
#include "kcenon/network/internal/secure_tcp_socket.h"
#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::core
{

	/*!
	 * \class secure_messaging_client
	 * \brief A secure client for establishing TLS/SSL encrypted TCP connections to a server.
	 *
	 * This class inherits from messaging_client_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Connection state (is_connected_) is protected by atomic operations.
	 * - Socket operations are serialized through ASIO's io_context.
	 *
	 * ### Key Responsibilities
	 * - Establishes encrypted connection to a remote server using TLS/SSL.
	 * - Performs SSL handshake after TCP connection.
	 * - Manages \c asio::io_context and runs it in a background thread.
	 * - Provides \c start_client() to connect and \c stop_client() to disconnect
	 *   (inherited from messaging_client_base).
	 * - Sends encrypted data via \c send_packet().
	 * - Optionally verifies server certificate.
	 *
	 * ### Usage Example
	 * \code
	 * auto client = std::make_shared<secure_messaging_client>("ClientID");
	 * auto result = client->start_client("server.example.com", 5555);
	 * if (!result) {
	 *     std::cerr << "Connection failed\n";
	 *     return;
	 * }
	 *
	 * std::vector<uint8_t> data = {1, 2, 3};
	 * client->send_packet(std::move(data));
	 *
	 * client->stop_client();
	 * \endcode
	 */
	class secure_messaging_client
		: public messaging_client_base<secure_messaging_client>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_client_base<secure_messaging_client>;

		/*!
		 * \brief Constructs a secure messaging client.
		 * \param client_id A descriptive identifier for this client instance.
		 * \param verify_cert Whether to verify server certificate (default: true)
		 */
		explicit secure_messaging_client(std::string_view client_id,
										 bool verify_cert = true);

		/*!
		 * \brief Destructor. Calls \c stop_client() if still connected
		 *        (handled by base class).
		 */
		~secure_messaging_client() noexcept override = default;

	protected:
		/*!
		 * \brief Secure TCP-specific implementation of client start.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return Result<void> - Success if connected, or error with code:
		 *         - error_codes::network_system::connection_failed
		 *         - error_codes::network_system::connection_timeout
		 *         - error_codes::common_errors::internal_error
		 *
		 * Called by base class start_client() after common validation.
		 * This method:
		 * 1. Creates io_context and secure socket
		 * 2. Establishes TCP connection
		 * 3. Performs SSL handshake
		 * 4. Starts background thread for I/O
		 */
		auto do_start(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief Secure TCP-specific implementation of client stop.
		 * \return Result<void> - Success if stopped, or error
		 *
		 * Called by base class stop_client() after common cleanup.
		 */
		auto do_stop() -> VoidResult;

		/*!
		 * \brief Secure TCP-specific implementation of data send.
		 * \param data The data to send (moved for efficiency).
		 * \return Result<void> - Success if sent, or error
		 *
		 * Called by base class send_packet() after common validation.
		 * Data is encrypted before transmission.
		 */
		auto do_send(std::vector<uint8_t>&& data) -> VoidResult;

	private:
		/*!
		 * \brief Callback for when encrypted data arrives from the server.
		 * \param data A vector containing a chunk of received bytes.
		 */
		auto on_receive(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Callback for handling socket errors.
		 * \param ec The \c std::error_code describing the error.
		 */
		auto on_error(std::error_code ec) -> void;

	private:
		// Secure TCP protocol-specific members (base class provides client_id_,
		// is_running_, is_connected_, stop_initiated_, stop_promise_, stop_future_,
		// and callbacks)

		std::unique_ptr<asio::io_context>
			io_context_;	/*!< The I/O context for async ops. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_;	/*!< Keeps io_context running. */

		std::shared_ptr<integration::thread_pool_interface>
			thread_pool_;	/*!< Thread pool for async operations. */
		std::future<void>
			io_context_future_; /*!< Future for io_context run task. */

		std::unique_ptr<asio::ssl::context>
			ssl_context_;	/*!< SSL context for encryption. */

		std::shared_ptr<internal::secure_tcp_socket>
			socket_;		/*!< The secure TCP socket for this connection. */

		bool verify_cert_{true};		/*!< Whether to verify server certificate. */
	};

} // namespace kcenon::network::core
