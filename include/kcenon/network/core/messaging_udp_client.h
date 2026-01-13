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
#include <functional>
#include <mutex>
#include <future>

#include <asio.hpp>

#include "kcenon/network/core/messaging_udp_client_base.h"
#include "kcenon/network/interfaces/i_udp_client.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::internal
{
	class udp_socket;
}

namespace kcenon::network::core
{
	/*!
	 * \class messaging_udp_client
	 * \brief A UDP client that sends datagrams to a target endpoint and can receive responses.
	 *
	 * This class inherits from messaging_udp_client_base using the CRTP pattern
	 * and implements the i_udp_client interface for composition-based usage.
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
	 * ### Interface Compliance
	 * This class implements interfaces::i_udp_client for composition-based usage.
	 *
	 * ### Usage Example
	 * \code
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
	class messaging_udp_client
		: public messaging_udp_client_base<messaging_udp_client>
		, public interfaces::i_udp_client
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_udp_client_base<messaging_udp_client>;

		/*!
		 * \brief Constructs a messaging_udp_client with an identifier.
		 * \param client_id A string identifier for this client instance.
		 */
		explicit messaging_udp_client(std::string_view client_id);

		/*!
		 * \brief Destructor. Automatically calls stop_client() if the client is still running
		 *        (handled by base class).
		 */
		~messaging_udp_client() noexcept override = default;

		// ========================================================================
		// i_udp_client interface implementation
		// ========================================================================

		/*!
		 * \brief Checks if the client is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override {
			return messaging_udp_client_base::is_running();
		}

		/*!
		 * \brief Blocks until stop() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override {
			messaging_udp_client_base::wait_for_stop();
		}

		/*!
		 * \brief Starts the UDP client targeting the specified endpoint.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::start(). Delegates to start_client().
		 */
		[[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override {
			return start_client(host, port);
		}

		/*!
		 * \brief Stops the UDP client.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::stop(). Delegates to stop_client().
		 */
		[[nodiscard]] auto stop() -> VoidResult override {
			return stop_client();
		}

		/*!
		 * \brief Sends a datagram to the configured target endpoint.
		 * \param data The data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::send().
		 */
		[[nodiscard]] auto send(
			std::vector<uint8_t>&& data,
			send_callback_t handler = nullptr) -> VoidResult override;

		/*!
		 * \brief Changes the target endpoint for future sends.
		 * \param host The new target hostname or IP address.
		 * \param port The new target port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::set_target().
		 */
		[[nodiscard]] auto set_target(std::string_view host, uint16_t port) -> VoidResult override;

		/*!
		 * \brief Sets the callback for received datagrams (interface version).
		 * \param callback The callback function with endpoint_info.
		 *
		 * Implements i_udp_client::set_receive_callback().
		 */
		auto set_receive_callback(interfaces::i_udp_client::receive_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for received datagrams (legacy version).
		 * \param callback The callback function with asio endpoint.
		 *
		 * This overload maintains backward compatibility with code using
		 * asio::ip::udp::endpoint directly.
		 */
		using messaging_udp_client_base::set_receive_callback;

		/*!
		 * \brief Sets the callback for errors (interface version).
		 * \param callback The callback function.
		 *
		 * Implements i_udp_client::set_error_callback().
		 */
		auto set_error_callback(interfaces::i_udp_client::error_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for errors (legacy version).
		 * \param callback The callback function.
		 *
		 * This overload maintains backward compatibility.
		 */
		using messaging_udp_client_base::set_error_callback;

		// ========================================================================
		// Legacy API (maintained for backward compatibility)
		// ========================================================================

		/*!
		 * \brief Sends a datagram to the configured target endpoint.
		 * \param data The data to send (moved for efficiency).
		 * \param handler Completion handler with signature void(std::error_code, std::size_t).
		 * \return Result<void> - Success if send initiated, or error if not running.
		 *
		 * \deprecated Use send() instead for interface compliance.
		 */
		auto send_packet(
			std::vector<uint8_t>&& data,
			std::function<void(std::error_code, std::size_t)> handler) -> VoidResult;

	protected:
		/*!
		 * \brief UDP-specific implementation of client start.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started, or error with code.
		 *
		 * Called by base class start_client() after common validation.
		 * Creates io_context, resolves target, creates socket, and starts worker thread.
		 */
		auto do_start(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief UDP-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped, or error with code.
		 *
		 * Called by base class stop_client() after common cleanup.
		 * Stops receiving, closes socket, and releases resources.
		 */
		auto do_stop() -> VoidResult;

	private:
		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::udp_socket> socket_;   /*!< UDP socket wrapper. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::mutex endpoint_mutex_;                      /*!< Protects target_endpoint_. */
		asio::ip::udp::endpoint target_endpoint_;        /*!< Target endpoint for sends. */

		std::mutex socket_mutex_;                        /*!< Protects socket access. */
	};

} // namespace kcenon::network::core
