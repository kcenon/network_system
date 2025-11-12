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
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include "network_system/internal/pipeline.h"
#include "network_system/internal/secure_tcp_socket.h"
#include "network_system/utils/result_types.h"
#include "network_system/integration/thread_integration.h"

namespace network_system::core
{

	/*!
	 * \class secure_messaging_client
	 * \brief A secure client for establishing TLS/SSL encrypted TCP connections to a server.
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
	 * - Provides \c start_client() to connect and \c stop_client() to disconnect.
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
		: public std::enable_shared_from_this<secure_messaging_client>
	{
	public:
		/*!
		 * \brief Constructs a secure messaging client.
		 * \param client_id A descriptive identifier for this client instance.
		 * \param verify_cert Whether to verify server certificate (default: true)
		 */
		explicit secure_messaging_client(const std::string& client_id,
										 bool verify_cert = true);

		/*!
		 * \brief Destructor. Calls \c stop_client() if still connected.
		 */
		~secure_messaging_client() noexcept;

		/*!
		 * \brief Initiates an encrypted connection to the specified server.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return Result<void> - Success if connected, or error with code:
		 *         - error_codes::network_system::connection_failed
		 *         - error_codes::network_system::connection_timeout
		 *         - error_codes::common_errors::internal_error
		 *
		 * This method:
		 * 1. Creates io_context and secure socket
		 * 2. Establishes TCP connection
		 * 3. Performs SSL handshake
		 * 4. Starts background thread for I/O
		 */
		auto start_client(const std::string& host, unsigned short port)
			-> VoidResult;

		/*!
		 * \brief Stops the client and disconnects from the server.
		 * \return Result<void> - Success if stopped, or error
		 */
		auto stop_client() -> VoidResult;

		/*!
		 * \brief Sends encrypted data to the server.
		 * \param data The data to send (moved for efficiency).
		 * \return Result<void> - Success if sent, or error
		 *
		 * \note Data is encrypted before transmission.
		 * \note The original vector will be empty after this call.
		 */
		auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Checks if the client is currently connected.
		 * \return true if connected and SSL handshake completed, false otherwise.
		 */
		[[nodiscard]] auto is_connected() const noexcept -> bool
		{
			return is_connected_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback Function called when data is received from the server.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for connection established.
		 * \param callback Function called when connection and SSL handshake succeed.
		 */
		auto set_connected_callback(std::function<void()> callback) -> void;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback Function called when disconnected from server.
		 */
		auto set_disconnected_callback(std::function<void()> callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback Function called when an error occurs.
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

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
		std::string client_id_; /*!< Identifier for this client instance. */

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

		internal::pipeline
			pipeline_;		/*!< Pipeline for compress/encrypt transformations. */

		mutable std::mutex mode_mutex_; /*!< Protects pipeline mode flags. */
		bool compress_mode_{false};		/*!< If true, compress data before sending. */
		bool encrypt_mode_{false};		/*!< If true, encrypt data before sending. */

		std::atomic<bool> is_connected_{false}; /*!< Connection state flag. */
		bool verify_cert_{true};		/*!< Whether to verify server certificate. */

		/*!
		 * \brief Callbacks for client events
		 */
		std::function<void(const std::vector<uint8_t>&)> receive_callback_;
		std::function<void()> connected_callback_;
		std::function<void()> disconnected_callback_;
		std::function<void(std::error_code)> error_callback_;

		/*!
		 * \brief Mutex protecting callback access
		 */
		mutable std::mutex callback_mutex_;
	};

} // namespace network_system::core
