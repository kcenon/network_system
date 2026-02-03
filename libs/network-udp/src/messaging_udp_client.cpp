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

#include "network_udp/core/messaging_udp_client.h"
#include "internal/core/network_context.h"
#include "network_udp/internal/udp_socket.h"
#include "internal/integration/logger_integration.h"

namespace kcenon::network::core
{
	using udp = asio::ip::udp;

	messaging_udp_client::messaging_udp_client(std::string_view client_id)
		: client_id_(client_id)
	{
	}

	messaging_udp_client::~messaging_udp_client() noexcept
	{
		if (lifecycle_.is_running())
		{
			(void)stop_client();
		}
	}

	// ========================================================================
	// Lifecycle Management
	// ========================================================================

	auto messaging_udp_client::start_client(std::string_view host, uint16_t port) -> VoidResult
	{
		if (lifecycle_.is_running())
		{
			return error_void(
				error_codes::common_errors::already_exists,
				"UDP client is already running",
				"messaging_udp_client::start_client");
		}

		if (host.empty())
		{
			return error_void(
				error_codes::common_errors::invalid_argument,
				"Host cannot be empty",
				"messaging_udp_client::start_client");
		}

		// Mark as running before starting
		lifecycle_.set_running();

		auto result = do_start_impl(host, port);
		if (result.is_err())
		{
			lifecycle_.mark_stopped();
		}

		return result;
	}

	auto messaging_udp_client::stop_client() -> VoidResult
	{
		if (!lifecycle_.prepare_stop())
		{
			return ok();  // Not running or already stopping
		}

		auto result = do_stop_impl();

		lifecycle_.mark_stopped();

		return result;
	}

	auto messaging_udp_client::client_id() const -> const std::string&
	{
		return client_id_;
	}

	// ========================================================================
	// i_network_component interface implementation
	// ========================================================================

	auto messaging_udp_client::is_running() const -> bool
	{
		return lifecycle_.is_running();
	}

	auto messaging_udp_client::wait_for_stop() -> void
	{
		lifecycle_.wait_for_stop();
	}

	// ========================================================================
	// i_udp_client interface implementation
	// ========================================================================

	auto messaging_udp_client::start(std::string_view host, uint16_t port) -> VoidResult
	{
		return start_client(host, port);
	}

	auto messaging_udp_client::stop() -> VoidResult
	{
		return stop_client();
	}

	auto messaging_udp_client::send(
		std::vector<uint8_t>&& data,
		interfaces::i_udp_client::send_callback_t handler) -> VoidResult
	{
		if (!lifecycle_.is_running())
		{
			return error_void(
				error_codes::common_errors::internal_error,
				"UDP client is not running",
				"messaging_udp_client::send",
				""
			);
		}

		std::lock_guard<std::mutex> socket_lock(socket_mutex_);
		if (!socket_)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				"Socket not available",
				"messaging_udp_client::send",
				""
			);
		}

		std::lock_guard<std::mutex> endpoint_lock(endpoint_mutex_);
		socket_->async_send_to(std::move(data), target_endpoint_, std::move(handler));

		return ok();
	}

	auto messaging_udp_client::set_target(std::string_view host, uint16_t port) -> VoidResult
	{
		if (!lifecycle_.is_running())
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

	auto messaging_udp_client::set_receive_callback(
		interfaces::i_udp_client::receive_callback_t callback) -> void
	{
		if (!callback)
		{
			// Clear the callback
			callbacks_.set<to_index(callback_index::receive)>(nullptr);
			return;
		}

		// Adapt the interface callback to the internal callback type
		// Convert asio::ip::udp::endpoint to endpoint_info
		callbacks_.set<to_index(callback_index::receive)>(
			[callback = std::move(callback)](
				const std::vector<uint8_t>& data,
				const asio::ip::udp::endpoint& endpoint)
			{
				interfaces::i_udp_client::endpoint_info info;
				info.address = endpoint.address().to_string();
				info.port = endpoint.port();
				callback(data, info);
			});
	}

	auto messaging_udp_client::set_receive_callback(receive_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::receive)>(std::move(callback));
	}

	auto messaging_udp_client::set_error_callback(error_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::error)>(std::move(callback));
	}

	// ========================================================================
	// Internal Implementation Methods
	// ========================================================================

	auto messaging_udp_client::do_start_impl(std::string_view host, uint16_t port) -> VoidResult
	{
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
					"messaging_udp_client::do_start_impl",
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

			// Set callbacks to socket
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
			return error_void(
				error_codes::common_errors::internal_error,
				std::string("Failed to create UDP socket: ") + e.what(),
				"messaging_udp_client::do_start_impl",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				std::string("Failed to start UDP client: ") + e.what(),
				"messaging_udp_client::do_start_impl",
				"Host: " + std::string(host) + ":" + std::to_string(port)
			);
		}
	}

	auto messaging_udp_client::do_stop_impl() -> VoidResult
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
				"messaging_udp_client::do_stop_impl",
				""
			);
		}
	}

	// ========================================================================
	// Internal Callback Helpers
	// ========================================================================

	auto messaging_udp_client::invoke_receive_callback(
		const std::vector<uint8_t>& data,
		const asio::ip::udp::endpoint& endpoint) -> void
	{
		callbacks_.invoke<to_index(callback_index::receive)>(data, endpoint);
	}

	auto messaging_udp_client::invoke_error_callback(std::error_code ec) -> void
	{
		callbacks_.invoke<to_index(callback_index::error)>(ec);
	}

	auto messaging_udp_client::get_receive_callback() const -> receive_callback_t
	{
		return callbacks_.get<to_index(callback_index::receive)>();
	}

	auto messaging_udp_client::get_error_callback() const -> error_callback_t
	{
		return callbacks_.get<to_index(callback_index::error)>();
	}

} // namespace kcenon::network::core
