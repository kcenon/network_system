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

#include "network_system/utils/result_types.h"

namespace network_system::internal
{
	class udp_socket;
}

namespace network_system::integration
{
	class io_context_executor;
}

namespace network_system::core
{
	/*!
	 * \class messaging_udp_client
	 * \brief A UDP client that sends datagrams to a target endpoint and can receive responses.
	 *
	 * ### Thread Pool Integration
	 * - Uses thread_pool_manager's I/O pool for ASIO io_context execution
	 * - No dedicated thread creation, leverages thread pool resources
	 * - Automatic resource cleanup via io_context_executor RAII
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Socket access is protected by socket_mutex_.
	 * - Atomic flags (is_running_) prevent race conditions.
	 * - send_packet() can be called from any thread safely.
	 *
	 * ### Key Characteristics
	 * - Connectionless: No persistent connection, each send is independent.
	 * - Target endpoint: Configured at start, can be changed via set_target().
	 * - Bidirectional: Can both send and receive datagrams.
	 * - Stateless: No built-in acknowledgment or reliability.
	 *
	 * ### Usage Example
	 * \code
	 * // Initialize thread pool manager first
	 * auto& mgr = integration::thread_pool_manager::instance();
	 * mgr.initialize();
	 *
	 * auto client = std::make_shared<messaging_udp_client>("UDPClient");
	 *
	 * // Set callback to handle received datagrams
	 * client->set_receive_callback(
	 *     [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
	 *         std::cout << "Received " << data.size() << " bytes from "
	 *                   << sender.address().to_string() << ":" << sender.port() << "\n";
	 *     });
	 *
	 * // Start client targeting localhost:5555
	 * auto result = client->start_client("localhost", 5555);
	 * if (!result) {
	 *     std::cerr << "Failed to start client: " << result.error().message << "\n";
	 *     return -1;
	 * }
	 *
	 * // Send a datagram
	 * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	 * client->send_packet(std::move(data),
	 *     [](std::error_code ec, std::size_t bytes) {
	 *         if (!ec) {
	 *             std::cout << "Sent " << bytes << " bytes\n";
	 *         }
	 *     });
	 *
	 * // Stop client
	 * client->stop_client();
	 * \endcode
	 */
	class messaging_udp_client : public std::enable_shared_from_this<messaging_udp_client>
	{
	public:
		/*!
		 * \brief Constructs a messaging_udp_client with an identifier.
		 * \param client_id A string identifier for this client instance.
		 */
		messaging_udp_client(std::string_view client_id);

		/*!
		 * \brief Destructor. Automatically calls stop_client() if the client is still running.
		 */
		~messaging_udp_client() noexcept;

		/*!
		 * \brief Starts the client by resolving target host and port, creating socket,
		 *        and running io_context on thread pool.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::common_errors::already_exists if already running
		 *         - error_codes::common_errors::invalid_argument if empty host
		 *         - error_codes::common_errors::internal_error for other failures
		 *
		 * Creates an io_context, resolves the target endpoint, creates a UDP socket,
		 * and runs io_context on thread pool's I/O executor.
		 *
		 * \throws std::runtime_error if thread_pool_manager is not initialized
		 */
		auto start_client(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief Stops the client and releases resources.
		 * \return Result<void> - Success if client stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
		 *
		 * Stops receiving datagrams, closes the socket, and joins the background thread.
		 */
		auto stop_client() -> VoidResult;

		/*!
		 * \brief Blocks the calling thread until the client is stopped.
		 */
		auto wait_for_stop() -> void;

		/*!
		 * \brief Sends a datagram to the configured target endpoint.
		 * \param data The data to send (moved for efficiency).
		 * \param handler Completion handler with signature void(std::error_code, std::size_t).
		 * \return Result<void> - Success if send initiated, or error if not running.
		 */
		auto send_packet(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> VoidResult;

		/*!
		 * \brief Sets a callback to handle received datagrams.
		 * \param callback Function with signature
		 *        void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		 *        called whenever a datagram is received.
		 */
		auto set_receive_callback(
			std::function<void(const std::vector<uint8_t>&,
			                   const asio::ip::udp::endpoint&)> callback) -> void;

		/*!
		 * \brief Sets a callback to handle errors.
		 * \param callback Function with signature void(std::error_code)
		 *        called when an error occurs during receive operations.
		 */
		auto set_error_callback(std::function<void(std::error_code)> callback) -> void;

		/*!
		 * \brief Changes the target endpoint for future sends.
		 * \param host The new target hostname or IP address.
		 * \param port The new target port number.
		 * \return Result<void> - Success if target updated, or error if resolution failed.
		 */
		auto set_target(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief Returns whether the client is currently running.
		 * \return true if client is running, false otherwise.
		 */
		auto is_running() const -> bool { return is_running_.load(); }

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string provided at construction.
		 */
		auto client_id() const -> const std::string& { return client_id_; }

	private:
		std::string client_id_;                          /*!< Client identifier. */
		std::atomic<bool> is_running_{false};            /*!< Running state flag. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::udp_socket> socket_;   /*!< UDP socket wrapper. */

		// Thread pool integration: io_context_executor replaces manual std::thread
		std::unique_ptr<integration::io_context_executor> io_executor_; /*!< IO context executor from thread pool */

		std::mutex endpoint_mutex_;                      /*!< Protects target_endpoint_. */
		asio::ip::udp::endpoint target_endpoint_;        /*!< Target endpoint for sends. */

		std::mutex socket_mutex_;                        /*!< Protects socket access. */
	};

} // namespace network_system::core
