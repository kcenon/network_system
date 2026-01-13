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

#include "kcenon/network/core/messaging_udp_server.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/internal/udp_socket.h"
#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::core
{
	using udp = asio::ip::udp;

	messaging_udp_server::messaging_udp_server(std::string_view server_id)
		: messaging_udp_server_base<messaging_udp_server>(server_id)
	{
	}

	// Destructor is defaulted in header - base class handles stop_server() call

	// UDP-specific implementation of server start
	// Called by base class start_server() after common validation
	auto messaging_udp_server::do_start(uint16_t port) -> VoidResult
	{
		try
		{
			// Create io_context
			io_context_ = std::make_unique<asio::io_context>();

			// Create and bind UDP socket
			asio::ip::udp::socket raw_socket(*io_context_, udp::endpoint(udp::v4(), port));

			// Wrap in our udp_socket
			socket_ = std::make_shared<internal::udp_socket>(std::move(raw_socket));

			// Set callbacks from base class to socket
			auto receive_cb = get_receive_callback();
			if (receive_cb)
			{
				socket_->set_receive_callback(std::move(receive_cb));
			}

			auto error_cb = get_error_callback();
			if (error_cb)
			{
				socket_->set_error_callback(std::move(error_cb));
			}

			// Start receiving
			socket_->start_receive();

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_) {
				// Fallback: create a temporary thread pool if network_context is not initialized
				NETWORK_LOG_WARN("[messaging_udp_server] network_context not initialized, creating temporary thread pool");
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
			}

			// Start io_context on thread pool
			io_context_future_ = thread_pool_->submit(
				[this]()
				{
					try
					{
						NETWORK_LOG_DEBUG("[messaging_udp_server] io_context started");
						io_context_->run();
						NETWORK_LOG_DEBUG("[messaging_udp_server] io_context stopped");
					}
					catch (const std::exception& e)
					{
						NETWORK_LOG_ERROR(std::string("Worker thread exception: ") + e.what());
					}
				});

			NETWORK_LOG_INFO("UDP server started on port " + std::to_string(port));

			return ok();
		}
		catch (const std::system_error& e)
		{
			return error_void(
				error_codes::network_system::bind_failed,
				std::string("Failed to bind UDP socket: ") + e.what(),
				"messaging_udp_server::do_start",
				"Port: " + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				std::string("Failed to start UDP server: ") + e.what(),
				"messaging_udp_server::do_start",
				"Port: " + std::to_string(port)
			);
		}
	}

	// UDP-specific implementation of server stop
	// Called by base class stop_server() after common cleanup
	auto messaging_udp_server::do_stop() -> VoidResult
	{
		try
		{
			// Stop receiving
			if (socket_)
			{
				socket_->stop_receive();
			}

			// Stop io_context
			if (io_context_)
			{
				io_context_->stop();
			}

			// Join worker thread
			if (io_context_future_.valid())
			{
				try {
					io_context_future_.wait();
				} catch (const std::exception& e) {
					NETWORK_LOG_ERROR("[messaging_udp_server] Exception while waiting for io_context: " +
									  std::string(e.what()));
				}
			}

			// Clean up
			socket_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("UDP server stopped");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				std::string("Failed to stop UDP server: ") + e.what(),
				"messaging_udp_server::do_stop",
				""
			);
		}
	}

	// wait_for_stop() is provided by base class

	// set_receive_callback and set_error_callback are provided by base class

	auto messaging_udp_server::async_send_to(
		std::vector<uint8_t>&& data,
		const asio::ip::udp::endpoint& endpoint,
		std::function<void(std::error_code, std::size_t)> handler) -> void
	{
		if (socket_)
		{
			socket_->async_send_to(std::move(data), endpoint, std::move(handler));
		}
		else if (handler)
		{
			// Socket not available, report error
			handler(asio::error::not_connected, 0);
		}
	}

	// ========================================================================
	// i_udp_server interface implementation
	// ========================================================================

	auto messaging_udp_server::send_to(
		const endpoint_info& endpoint,
		std::vector<uint8_t>&& data,
		send_callback_t handler) -> VoidResult
	{
		if (!messaging_udp_server_base::is_running())
		{
			return error_void(
				error_codes::network_system::server_not_started,
				"UDP server is not running",
				"messaging_udp_server::send_to",
				""
			);
		}

		if (!socket_)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				"Socket not available",
				"messaging_udp_server::send_to",
				""
			);
		}

		try
		{
			// Convert endpoint_info to asio::ip::udp::endpoint
			asio::ip::udp::endpoint asio_endpoint(
				asio::ip::make_address(endpoint.address),
				endpoint.port
			);

			socket_->async_send_to(std::move(data), asio_endpoint, std::move(handler));
			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				std::string("Failed to send datagram: ") + e.what(),
				"messaging_udp_server::send_to",
				"Target: " + endpoint.address + ":" + std::to_string(endpoint.port)
			);
		}
	}

	auto messaging_udp_server::set_receive_callback(
		interfaces::i_udp_server::receive_callback_t callback) -> void
	{
		if (!callback)
		{
			// Clear the callback
			messaging_udp_server_base::set_receive_callback(nullptr);
			return;
		}

		// Adapt the interface callback to the base class callback type
		// Convert asio::ip::udp::endpoint to endpoint_info
		messaging_udp_server_base::set_receive_callback(
			[callback = std::move(callback)](
				const std::vector<uint8_t>& data,
				const asio::ip::udp::endpoint& endpoint)
			{
				interfaces::i_udp_server::endpoint_info info;
				info.address = endpoint.address().to_string();
				info.port = endpoint.port();
				callback(data, info);
			});
	}

	auto messaging_udp_server::set_error_callback(
		interfaces::i_udp_server::error_callback_t callback) -> void
	{
		// The error callback types are compatible (both use std::error_code)
		messaging_udp_server_base::set_error_callback(std::move(callback));
	}

} // namespace kcenon::network::core
