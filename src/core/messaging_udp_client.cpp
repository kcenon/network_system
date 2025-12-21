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

#include "kcenon/network/core/messaging_udp_client.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/internal/udp_socket.h"
#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::core
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
				error_codes::common_errors::already_exists,
				"UDP client is already running",
				"messaging_udp_client::start_client",
				"Client ID: " + client_id_
			);
		}

		if (host.empty())
		{
			return error_void(
				error_codes::common_errors::invalid_argument,
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
					error_codes::common_errors::internal_error,
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

			is_running_.store(true);

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_) {
				// Fallback: create a temporary thread pool if network_context is not initialized
				NETWORK_LOG_WARN("[messaging_udp_client] network_context not initialized, creating temporary thread pool");
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
			}

			// Start io_context on thread pool
			io_context_future_ = thread_pool_->submit(
				[this]()
				{
					try
					{
						NETWORK_LOG_DEBUG("[messaging_udp_client] io_context started");
						io_context_->run();
						NETWORK_LOG_DEBUG("[messaging_udp_client] io_context stopped");
					}
					catch (const std::exception& e)
					{
						NETWORK_LOG_ERROR(std::string("Worker thread exception: ") + e.what());
					}
				});

			NETWORK_LOG_INFO("UDP client started targeting " + std::string(host) + ":" + std::to_string(port));

			return ok();
		}
		catch (const std::system_error& e)
		{
			is_running_.store(false);
			return error_void(
				error_codes::common_errors::internal_error,
				std::string("Failed to create UDP socket: ") + e.what(),
				"messaging_udp_client::start_client",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			is_running_.store(false);
			return error_void(
				error_codes::common_errors::internal_error,
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

			// Wait for io_context task to complete
			if (io_context_future_.valid())
			{
				try {
					io_context_future_.wait();
				} catch (const std::exception& e) {
					NETWORK_LOG_ERROR("[messaging_udp_client] Exception while waiting for io_context: " +
									  std::string(e.what()));
				}
			}

			// Clean up
			socket_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("UDP client stopped");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
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
				error_codes::common_errors::internal_error,
				"UDP client is not running",
				"messaging_udp_client::send_packet",
				""
			);
		}

		std::lock_guard<std::mutex> socket_lock(socket_mutex_);
		if (!socket_)
		{
			return error_void(
				error_codes::common_errors::internal_error,
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
				error_codes::common_errors::internal_error,
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
					error_codes::common_errors::internal_error,
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
				error_codes::common_errors::internal_error,
				std::string("Failed to set target: ") + e.what(),
				"messaging_udp_client::set_target",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
	}

} // namespace kcenon::network::core
