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

#include "network_system/core/messaging_udp_client.h"
#include "network_system/internal/udp_socket.h"
#include "network_system/integration/logger_integration.h"
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"

#include <stdexcept>

namespace network_system::core
{
	using udp = asio::ip::udp;

	messaging_udp_client::messaging_udp_client(std::string_view client_id)
		: client_id_(client_id)
	{
	}

	messaging_udp_client::~messaging_udp_client() noexcept
	{
		try
		{
			(void)stop_client();
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto messaging_udp_client::start_client(std::string_view host, uint16_t port) -> VoidResult
	{
		if (is_running_.load())
		{
			return error_void(
				error_codes::common::already_exists,
				"UDP client is already running",
				"messaging_udp_client::start_client",
				"Client ID: " + client_id_
			);
		}

		if (host.empty())
		{
			return error_void(
				error_codes::common::invalid_argument,
				"Host cannot be empty",
				"messaging_udp_client::start_client",
				""
			);
		}

		try
		{
			// Create io_context
			io_context_ = std::make_unique<asio::io_context>();

			// Resolve target endpoint
			udp::resolver resolver(*io_context_);
			auto endpoints = resolver.resolve(udp::v4(), std::string(host), std::to_string(port));

			if (endpoints.empty())
			{
				return error_void(
					error_codes::common::internal_error,
					"Failed to resolve host",
					"messaging_udp_client::start_client",
					"Host: " + std::string(host)
				);
			}

			{
				std::lock_guard<std::mutex> lock(endpoint_mutex_);
				target_endpoint_ = *endpoints.begin();
			}

			// Create UDP socket (bind to any port)
			asio::ip::udp::socket raw_socket(*io_context_, udp::endpoint(udp::v4(), 0));

			// Wrap in our udp_socket
			socket_ = std::make_shared<internal::udp_socket>(std::move(raw_socket));

			// Start receiving
			socket_->start_receive();

			// Get thread pool from manager
			auto& mgr = integration::thread_pool_manager::instance();
			if (!mgr.is_initialized())
			{
				throw std::runtime_error("[messaging_udp_client] thread_pool_manager not initialized");
			}

			auto pool = mgr.create_io_pool("udp_client_" + client_id_);
			if (!pool)
			{
				throw std::runtime_error("[messaging_udp_client] Failed to create I/O pool");
			}

			// Create io_context executor
			io_executor_ = std::make_unique<integration::io_context_executor>(
				pool,
				*io_context_,
				"udp_client_" + client_id_
			);

			is_running_.store(true);

			// Start io_context execution in thread pool
			io_executor_->start();

			NETWORK_LOG_INFO("UDP client started targeting " + std::string(host) + ":" + std::to_string(port) + " with thread pool");

			return ok();
		}
		catch (const std::system_error& e)
		{
			// Cleanup on failure
			is_running_.store(false);
			io_executor_.reset();
			socket_.reset();
			io_context_.reset();

			return error_void(
				error_codes::common::internal_error,
				std::string("Failed to create UDP socket: ") + e.what(),
				"messaging_udp_client::start_client",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			// Cleanup on failure
			is_running_.store(false);
			io_executor_.reset();
			socket_.reset();
			io_context_.reset();

			return error_void(
				error_codes::common::internal_error,
				std::string("Failed to start UDP client: ") + e.what(),
				"messaging_udp_client::start_client",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
	}

	auto messaging_udp_client::stop_client() -> VoidResult
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

			// Stop executor (RAII will handle thread pool cleanup)
			io_executor_.reset();

			// Clean up
			socket_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("UDP client stopped");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common::internal_error,
				std::string("Failed to stop UDP client: ") + e.what(),
				"messaging_udp_client::stop_client",
				""
			);
		}
	}

	auto messaging_udp_client::wait_for_stop() -> void
	{
		// Simple busy wait - in production, use condition variable
		while (is_running_.load())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	auto messaging_udp_client::send_packet(
		std::vector<uint8_t>&& data,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		if (!is_running_.load())
		{
			return error_void(
				error_codes::common::internal_error,
				"UDP client is not running",
				"messaging_udp_client::send_packet",
				""
			);
		}

		std::lock_guard<std::mutex> socket_lock(socket_mutex_);
		if (!socket_)
		{
			return error_void(
				error_codes::common::internal_error,
				"Socket not available",
				"messaging_udp_client::send_packet",
				""
			);
		}

		std::lock_guard<std::mutex> endpoint_lock(endpoint_mutex_);
		socket_->async_send_to(std::move(data), target_endpoint_, std::move(handler));

		return ok();
	}

	auto messaging_udp_client::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&,
		                   const asio::ip::udp::endpoint&)> callback) -> void
	{
		if (socket_)
		{
			socket_->set_receive_callback(std::move(callback));
		}
	}

	auto messaging_udp_client::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		if (socket_)
		{
			socket_->set_error_callback(std::move(callback));
		}
	}

	auto messaging_udp_client::set_target(std::string_view host, uint16_t port) -> VoidResult
	{
		if (!is_running_.load())
		{
			return error_void(
				error_codes::common::internal_error,
				"UDP client is not running",
				"messaging_udp_client::set_target",
				""
			);
		}

		try
		{
			// Resolve new target endpoint
			udp::resolver resolver(*io_context_);
			auto endpoints = resolver.resolve(udp::v4(), std::string(host), std::to_string(port));

			if (endpoints.empty())
			{
				return error_void(
					error_codes::common::internal_error,
					"Failed to resolve host",
					"messaging_udp_client::set_target",
					"Host: " + std::string(host)
				);
			}

			std::lock_guard<std::mutex> lock(endpoint_mutex_);
			target_endpoint_ = *endpoints.begin();

			NETWORK_LOG_INFO("Target updated to " + std::string(host) + ":" + std::to_string(port));

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common::internal_error,
				std::string("Failed to set target: ") + e.what(),
				"messaging_udp_client::set_target",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
	}

} // namespace network_system::core
