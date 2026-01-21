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
 * @file secure_messaging_client.h
 * @brief Legacy secure TCP client class.
 *
 * @deprecated This header is deprecated. Use unified_messaging_client.h instead.
 *
 * Migration guide:
 * @code
 * // Old code:
 * #include <kcenon/network/core/secure_messaging_client.h>
 * auto client = std::make_shared<secure_messaging_client>("client1");
 *
 * // New code:
 * #include <kcenon/network/core/unified_messaging_client.h>
 * tls_enabled tls_config{.cert_path = "cert.pem", .key_path = "key.pem"};
 * auto client = std::make_shared<secure_tcp_client>("client1", tls_config);
 * // Or: auto client = std::make_shared<unified_messaging_client<tcp_protocol, tls_enabled>>("client1", tls_config);
 * @endcode
 *
 * @see unified_messaging_client.h for the new template-based API
 * @see unified_compat.h for backward-compatible type aliases
 */

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include "kcenon/network/core/callback_indices.h"
#include "kcenon/network/internal/secure_tcp_socket.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::core
{

	/*!
	 * \class secure_messaging_client
	 * \brief A secure client for establishing TLS/SSL encrypted TCP connections to a server.
	 *
	 * @deprecated Use unified_messaging_client<tcp_protocol, tls_enabled> or secure_tcp_client instead.
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
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
		//! \brief Callback type for received data
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a secure messaging client.
		 * \param client_id A descriptive identifier for this client instance.
		 * \param verify_cert Whether to verify server certificate (default: true)
		 */
		explicit secure_messaging_client(std::string_view client_id,
										 bool verify_cert = true);

		/*!
		 * \brief Destructor. Calls \c stop_client() if still connected.
		 */
		~secure_messaging_client() noexcept;

		// Non-copyable, non-movable
		secure_messaging_client(const secure_messaging_client&) = delete;
		secure_messaging_client& operator=(const secure_messaging_client&) = delete;
		secure_messaging_client(secure_messaging_client&&) = delete;
		secure_messaging_client& operator=(secure_messaging_client&&) = delete;

		// =====================================================================
		// Lifecycle Management
		// =====================================================================

		/*!
		 * \brief Starts the client and connects to the specified host and port.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number to connect.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::network_system::client_already_running if already running
		 *         - error_codes::common_errors::invalid_argument if empty host
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		[[nodiscard]] auto start_client(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief Stops the client and disconnects from the server.
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
		 * \brief Sends data to the connected server.
		 * \param data The buffer to send (moved for efficiency).
		 * \return Result<void> - Success if data queued for send, or error with code:
		 *         - error_codes::network_system::connection_closed if not connected
		 *         - error_codes::network_system::send_failed for other failures
		 */
		[[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

		// =====================================================================
		// Callback Setters
		// =====================================================================

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
		 * \brief Secure TCP-specific implementation of client start.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return Result<void> - Success if connected, or error with code:
		 *         - error_codes::network_system::connection_failed
		 *         - error_codes::network_system::connection_timeout
		 *         - error_codes::common_errors::internal_error
		 */
		auto do_start_impl(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief Secure TCP-specific implementation of client stop.
		 * \return Result<void> - Success if stopped, or error
		 */
		auto do_stop_impl() -> VoidResult;

		/*!
		 * \brief Secure TCP-specific implementation of data send.
		 * \param data The data to send (moved for efficiency).
		 * \return Result<void> - Success if sent, or error
		 */
		auto do_send_impl(std::vector<uint8_t>&& data) -> VoidResult;

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

		// =====================================================================
		// Internal Socket Handlers
		// =====================================================================

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
		//! \brief Callback index type alias for clarity
		using callback_index = tcp_client_callback;

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
