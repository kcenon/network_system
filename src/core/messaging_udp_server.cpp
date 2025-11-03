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

#include "network_system/core/messaging_udp_server.h"
#include "network_system/internal/udp_socket.h"
#include "network_system/integration/logger_integration.h"
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"

namespace network_system::core
{
	using udp = asio::ip::udp;

	messaging_udp_server::messaging_udp_server(const std::string& server_id)
		: server_id_(server_id)
	{
	}

	messaging_udp_server::~messaging_udp_server() noexcept
	{
		try
		{
			(void)stop_server();
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto messaging_udp_server::start_server(uint16_t port) -> VoidResult
	{
		if (is_running_.load())
		{
			return error_void(
				error_codes::network_system::server_already_running,
				"UDP server is already running",
				"messaging_udp_server::start_server",
				"Server ID: " + server_id_
			);
		}

		try
		{
			// Get thread pool from manager
			auto& mgr = integration::thread_pool_manager::instance();
			if (!mgr.is_initialized())
			{
				return error_void(
					error_codes::common::internal_error,
					"thread_pool_manager not initialized",
					"messaging_udp_server::start_server",
					"Server ID: " + server_id_
				);
			}

			auto pool = mgr.create_io_pool("messaging_udp_server");
			if (!pool)
			{
				return error_void(
					error_codes::common::internal_error,
					"Failed to create I/O pool",
					"messaging_udp_server::start_server",
					"Server ID: " + server_id_
				);
			}

			// Create io_context
			io_context_ = std::make_unique<asio::io_context>();

			// Create and bind UDP socket
			asio::ip::udp::socket raw_socket(*io_context_, udp::endpoint(udp::v4(), port));

			// Wrap in our udp_socket
			socket_ = std::make_shared<internal::udp_socket>(std::move(raw_socket));

			// Start receiving
			socket_->start_receive();

			// Create io_context executor
			io_executor_ = std::make_unique<integration::io_context_executor>(
				pool,
				*io_context_,
				"messaging_udp_server"
			);

			// Start io_context execution in thread pool
			io_executor_->start();

			is_running_.store(true);

			NETWORK_LOG_INFO("UDP server started on port " + std::to_string(port));

			return ok();
		}
		catch (const std::system_error& e)
		{
			is_running_.store(false);
			io_executor_.reset();
			return error_void(
				error_codes::network_system::bind_failed,
				std::string("Failed to bind UDP socket: ") + e.what(),
				"messaging_udp_server::start_server",
				"Port: " + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			is_running_.store(false);
			io_executor_.reset();
			return error_void(
				error_codes::common::internal_error,
				std::string("Failed to start UDP server: ") + e.what(),
				"messaging_udp_server::start_server",
				"Port: " + std::to_string(port)
			);
		}
	}

	auto messaging_udp_server::stop_server() -> VoidResult
	{
		if (!is_running_.load())
		{
			return ok();
		}

		try
		{
			is_running_.store(false);

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

			// Release io_context executor (automatically stops and joins)
			io_executor_.reset();

			// Clean up
			socket_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("UDP server stopped");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common::internal_error,
				std::string("Failed to stop UDP server: ") + e.what(),
				"messaging_udp_server::stop_server",
				""
			);
		}
	}

	auto messaging_udp_server::wait_for_stop() -> void
	{
		// Simple busy wait - in production, use condition variable
		while (is_running_.load())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	auto messaging_udp_server::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&,
		                   const asio::ip::udp::endpoint&)> callback) -> void
	{
		if (socket_)
		{
			socket_->set_receive_callback(std::move(callback));
		}
	}

	auto messaging_udp_server::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		if (socket_)
		{
			socket_->set_error_callback(std::move(callback));
		}
	}

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

} // namespace network_system::core
