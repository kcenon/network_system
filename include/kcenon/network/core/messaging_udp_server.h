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
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/interfaces/i_udp_server.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/utils/lifecycle_manager.h"
#include "kcenon/network/utils/callback_manager.h"
#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::internal
{
	class udp_socket;
}

namespace kcenon::network::core
{
	/*!
	 * \class messaging_udp_server
	 * \brief A UDP server that receives datagrams and routes them based on sender endpoint.
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 * It also implements the i_udp_server interface for composition-based usage.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Internal state is protected by atomics and mutex.
	 * - Background thread runs io_context.run() independently.
	 * - Callbacks are invoked on ASIO worker thread.
	 *
	 * ### Key Characteristics
	 * - Connectionless: No persistent sessions, each datagram is independent.
	 * - Endpoint-based routing: Each received datagram includes sender endpoint.
	 * - No session management: Unlike TCP server, UDP server doesn't maintain sessions.
	 * - Stateless: Application layer must handle state if needed.
	 *
	 * ### Interface Compliance
	 * This class implements interfaces::i_udp_server for composition-based usage.
	 *
	 * ### Usage Example
	 * \code
	 * auto server = std::make_shared<messaging_udp_server>("UDPServer");
	 *
	 * // Set callback to handle received datagrams
	 * server->set_receive_callback(
	 *     [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
	 *         std::cout << "Received " << data.size() << " bytes from "
	 *                   << sender.address().to_string() << ":" << sender.port() << "\n";
	 *     });
	 *
	 * // Start server on port 5555
	 * auto result = server->start_server(5555);
	 * if (!result) {
	 *     std::cerr << "Failed to start server: " << result.error().message << "\n";
	 *     return -1;
	 * }
	 *
	 * // Send response back to client
	 * std::vector<uint8_t> response = {0x01, 0x02, 0x03};
	 * server->async_send_to(std::move(response), sender_endpoint,
	 *     [](std::error_code ec, std::size_t bytes) {
	 *         if (!ec) {
	 *             std::cout << "Sent " << bytes << " bytes\n";
	 *         }
	 *     });
	 *
	 * // Stop server
	 * server->stop_server();
	 * \endcode
	 */
	class messaging_udp_server
		: public std::enable_shared_from_this<messaging_udp_server>
		, public interfaces::i_udp_server
	{
	public:
		//! \brief Callback type for received datagrams with sender endpoint
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&,
		                                              const asio::ip::udp::endpoint&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a messaging_udp_server with an identifier.
		 * \param server_id A descriptive identifier for this server instance.
		 */
		explicit messaging_udp_server(std::string_view server_id);

		/*!
		 * \brief Destructor. If the server is still running, stop_server() is invoked.
		 */
		~messaging_udp_server() noexcept override;

		// Non-copyable, non-movable
		messaging_udp_server(const messaging_udp_server&) = delete;
		messaging_udp_server& operator=(const messaging_udp_server&) = delete;
		messaging_udp_server(messaging_udp_server&&) = delete;
		messaging_udp_server& operator=(messaging_udp_server&&) = delete;

		// ========================================================================
		// Lifecycle Management
		// ========================================================================

		/*!
		 * \brief Starts the server on the specified port.
		 * \param port The UDP port to bind and listen on.
		 * \return Result<void> - Success if server started, or error with code:
		 *         - error_codes::network_system::server_already_running if already running
		 *         - error_codes::network_system::bind_failed if port binding failed
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		[[nodiscard]] auto start_server(uint16_t port) -> VoidResult;

		/*!
		 * \brief Stops the server and releases all resources.
		 * \return Result<void> - Success if server stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
		 */
		[[nodiscard]] auto stop_server() -> VoidResult;

		/*!
		 * \brief Returns the server identifier.
		 * \return The server_id string.
		 */
		[[nodiscard]] auto server_id() const -> const std::string&;

		// ========================================================================
		// i_network_component interface implementation
		// ========================================================================

		/*!
		 * \brief Checks if the server is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override;

		/*!
		 * \brief Blocks until stop_server() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override;

		// ========================================================================
		// i_udp_server interface implementation
		// ========================================================================

		/*!
		 * \brief Starts the UDP server on the specified port.
		 * \param port The port number to bind to.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_server::start(). Delegates to start_server().
		 */
		[[nodiscard]] auto start(uint16_t port) -> VoidResult override;

		/*!
		 * \brief Stops the UDP server.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_server::stop(). Delegates to stop_server().
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Sends a datagram to the specified endpoint.
		 * \param endpoint The target endpoint.
		 * \param data The data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_server::send_to().
		 */
		[[nodiscard]] auto send_to(
			const interfaces::i_udp_server::endpoint_info& endpoint,
			std::vector<uint8_t>&& data,
			interfaces::i_udp_server::send_callback_t handler = nullptr) -> VoidResult override;

		/*!
		 * \brief Sets the callback for received datagrams (interface version).
		 * \param callback The callback function with endpoint_info.
		 *
		 * Implements i_udp_server::set_receive_callback().
		 */
		auto set_receive_callback(interfaces::i_udp_server::receive_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for received datagrams (legacy version).
		 * \param callback The callback function with asio endpoint.
		 *
		 * This overload maintains backward compatibility with code using
		 * asio::ip::udp::endpoint directly.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 *
		 * Implements i_udp_server::set_error_callback().
		 * Also compatible with legacy error_callback_t (same signature).
		 */
		auto set_error_callback(error_callback_t callback) -> void override;

		// ========================================================================
		// Legacy API (maintained for backward compatibility)
		// ========================================================================

		/*!
		 * \brief Sends a datagram to a specific endpoint.
		 * \param data The data to send (moved for efficiency).
		 * \param endpoint The target endpoint to send to.
		 * \param handler Completion handler with signature void(std::error_code, std::size_t).
		 *
		 * This allows the server to send responses back to clients.
		 *
		 * \deprecated Use send_to() instead for interface compliance.
		 */
		auto async_send_to(
			std::vector<uint8_t>&& data,
			const asio::ip::udp::endpoint& endpoint,
			std::function<void(std::error_code, std::size_t)> handler) -> void;

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief UDP-specific implementation of server start.
		 * \param port The UDP port to bind and listen on.
		 * \return Result<void> - Success if server started, or error with code.
		 *
		 * Creates io_context, binds socket, and starts worker thread.
		 */
		auto do_start_impl(uint16_t port) -> VoidResult;

		/*!
		 * \brief UDP-specific implementation of server stop.
		 * \return Result<void> - Success if server stopped, or error with code.
		 *
		 * Stops receiving, closes socket, and releases resources.
		 */
		auto do_stop_impl() -> VoidResult;

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Invokes the receive callback with the given data and endpoint.
		 * \param data The received data.
		 * \param endpoint The sender's endpoint.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data,
		                             const asio::ip::udp::endpoint& endpoint) -> void;

		/*!
		 * \brief Invokes the error callback with the given error code.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

		/*!
		 * \brief Gets a copy of the receive callback.
		 * \return Copy of the receive callback (may be empty).
		 */
		[[nodiscard]] auto get_receive_callback() const -> receive_callback_t;

		/*!
		 * \brief Gets a copy of the error callback.
		 * \return Copy of the error callback (may be empty).
		 */
		[[nodiscard]] auto get_error_callback() const -> error_callback_t;

		// =====================================================================
		// Callback indices for callback_manager
		// =====================================================================
		static constexpr std::size_t kReceiveCallbackIndex = 0;
		static constexpr std::size_t kErrorCallbackIndex = 1;

		//! \brief Callback manager type for this server
		using callbacks_t = utils::callback_manager<
			receive_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string server_id_;                          /*!< Server identifier. */
		utils::lifecycle_manager lifecycle_;             /*!< Lifecycle state manager. */
		callbacks_t callbacks_;                          /*!< Callback manager. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::udp_socket> socket_;   /*!< UDP socket wrapper. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */
	};

} // namespace kcenon::network::core
