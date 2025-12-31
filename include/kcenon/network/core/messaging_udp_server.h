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
#include <vector>
#include <functional>
#include <future>
#include <string>

#include <asio.hpp>

#include "kcenon/network/core/messaging_udp_server_base.h"
#include "kcenon/network/utils/result_types.h"
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
	 * This class inherits from messaging_udp_server_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Internal state (is_running_) is protected by atomics.
	 * - Background thread runs io_context.run() independently.
	 * - Callbacks are invoked on ASIO worker thread.
	 *
	 * ### Key Characteristics
	 * - Connectionless: No persistent sessions, each datagram is independent.
	 * - Endpoint-based routing: Each received datagram includes sender endpoint.
	 * - No session management: Unlike TCP server, UDP server doesn't maintain sessions.
	 * - Stateless: Application layer must handle state if needed.
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
		: public messaging_udp_server_base<messaging_udp_server>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_udp_server_base<messaging_udp_server>;

		/*!
		 * \brief Constructs a messaging_udp_server with an identifier.
		 * \param server_id A descriptive identifier for this server instance.
		 */
		explicit messaging_udp_server(std::string_view server_id);

		/*!
		 * \brief Destructor. If the server is still running, stop_server() is invoked
		 *        (handled by base class).
		 */
		~messaging_udp_server() noexcept override = default;

		/*!
		 * \brief Sends a datagram to a specific endpoint.
		 * \param data The data to send (moved for efficiency).
		 * \param endpoint The target endpoint to send to.
		 * \param handler Completion handler with signature void(std::error_code, std::size_t).
		 *
		 * This allows the server to send responses back to clients.
		 */
		auto async_send_to(
			std::vector<uint8_t>&& data,
			const asio::ip::udp::endpoint& endpoint,
			std::function<void(std::error_code, std::size_t)> handler) -> void;

	protected:
		/*!
		 * \brief UDP-specific implementation of server start.
		 * \param port The UDP port to bind and listen on.
		 * \return Result<void> - Success if server started, or error with code.
		 *
		 * Called by base class start_server() after common validation.
		 * Creates io_context, binds socket, and starts worker thread.
		 */
		auto do_start(uint16_t port) -> VoidResult;

		/*!
		 * \brief UDP-specific implementation of server stop.
		 * \return Result<void> - Success if server stopped, or error with code.
		 *
		 * Called by base class stop_server() after common cleanup.
		 * Stops receiving, closes socket, and releases resources.
		 */
		auto do_stop() -> VoidResult;

	private:
		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::udp_socket> socket_;   /*!< UDP socket wrapper. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */
	};

} // namespace kcenon::network::core
