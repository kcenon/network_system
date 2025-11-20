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

#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/internal/send_coroutine.h"
#include "kcenon/network/integration/logger_integration.h"
#include <string_view>
#include <type_traits>
#include <optional>

// Use nested namespace definition (C++17)
namespace network_system::core
{

	using tcp = asio::ip::tcp;

	// Use string_view for better efficiency (C++17)
	messaging_client::messaging_client(std::string_view client_id)
		: client_id_(client_id)
	{
		// Optionally configure pipeline or modes here:
		pipeline_ = internal::make_default_pipeline();
		compress_mode_ = false; // set true if you want to compress
		encrypt_mode_ = false;	// set true if you want to encrypt
	}

	messaging_client::~messaging_client() noexcept
	{
		try
		{
			// Early exit if already stopped (optimization)
			if (!is_running_.load(std::memory_order_acquire))
			{
				return;
			}

			// Ensure client is properly stopped before destruction
			// Ignore the return value in destructor to avoid throwing
			auto result = stop_client();
			if (result.is_err())
			{
				// Log error but don't throw (destructors must not throw)
				NETWORK_LOG_WARN("[messaging_client::~messaging_client] "
					"Failed to stop client cleanly: " + result.error().message +
					" (Client ID: " + client_id_ + ")");
			}
			else
			{
				NETWORK_LOG_DEBUG("[messaging_client::~messaging_client] "
					"Client stopped successfully (Client ID: " + client_id_ + ")");
			}
		}
		catch (const std::exception& e)
		{
			// Destructor must not throw - swallow all exceptions but log them
			NETWORK_LOG_ERROR("[messaging_client::~messaging_client] "
				"Exception during cleanup: " + std::string(e.what()) +
				" (Client ID: " + client_id_ + ")");
		}
		catch (...)
		{
			// Destructor must not throw - swallow unknown exceptions
			NETWORK_LOG_ERROR("[messaging_client::~messaging_client] "
				"Unknown exception during cleanup (Client ID: " + client_id_ + ")");
		}
	}

	// Use string_view for more efficient string handling (C++17)
	auto messaging_client::start_client(std::string_view host,
										unsigned short port) -> VoidResult
	{
		if (is_running_.load())
		{
			return error_void(
				error_codes::common_errors::already_exists,
				"Client is already running",
				"messaging_client::start_client",
				"Client ID: " + client_id_
			);
		}

		if (host.empty())
		{
			return error_void(
				error_codes::common_errors::invalid_argument,
				"Host cannot be empty",
				"messaging_client::start_client",
				"Client ID: " + client_id_
			);
		}

		try
		{
			is_running_.store(true);
			is_connected_.store(false);
			stop_initiated_.store(false);

			{
				std::lock_guard<std::mutex> lock(socket_mutex_);
				socket_.reset();
			}

			io_context_ = std::make_unique<asio::io_context>();
			// Create work guard to keep io_context running
			work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
				asio::make_work_guard(*io_context_)
			);
			// For wait_for_stop()
			stop_future_ = std::future<void>();
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_) {
				// Fallback: create basic thread pool
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(2);
			}

			// Submit io_context run task to thread pool
			io_context_future_ = thread_pool_->submit(
				[this]()
				{
					try
					{
						NETWORK_LOG_INFO("[messaging_client] Starting io_context on thread pool");
						io_context_->run();
						NETWORK_LOG_INFO("[messaging_client] io_context stopped");
					}
					catch (const std::exception& e)
					{
						NETWORK_LOG_ERROR("[messaging_client] Exception in io_context: " +
						                 std::string(e.what()));
					}
				});

			do_connect(host, port);

			NETWORK_LOG_INFO("[messaging_client] started. ID=" + client_id_
					+ " target=" + std::string(host) + ":" + std::to_string(port));

			return ok();
		}
		catch (const std::exception& e)
		{
			is_running_.store(false);
			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to start client: " + std::string(e.what()),
				"messaging_client::start_client",
				"Client ID: " + client_id_ + ", Host: " + std::string(host)
			);
		}
	}

	auto messaging_client::stop_client() -> VoidResult
	{
		// Use compare_exchange to ensure only one thread executes cleanup
		bool expected = false;
		if (!stop_initiated_.compare_exchange_strong(expected, true))
		{
			// Another thread is already stopping or has stopped
			NETWORK_LOG_DEBUG("[messaging_client] stop_client already called, returning success");
			return ok();
		}

		if (!is_running_.load())
		{
			return ok(); // Already stopped, not an error
		}

		try
		{
			is_running_.store(false);
			is_connected_.store(false);

		// Swap out socket with mutex protection and close outside lock
		std::shared_ptr<internal::tcp_socket> local_socket;
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			local_socket = std::move(socket_);
		}
		if (local_socket)
		{
			// Stop reading first to prevent new async operations
			local_socket->stop_read();
			asio::error_code ec;
			local_socket->socket().close(ec);
		}

		// Release work guard to allow io_context to finish
		if (work_guard_)
		{
			work_guard_.reset();
		}
		// Stop io_context first to cancel all pending operations
		if (io_context_)
		{
			io_context_->stop();
		}
		// Wait for io_context task to complete
		if (io_context_future_.valid())
		{
			io_context_future_.wait();
		}
		thread_pool_.reset();
		io_context_.reset();
		// Signal stop
		if (stop_promise_.has_value())
		{
			try
			{
				stop_promise_->set_value();
			}
			catch (const std::future_error& e)
			{
				// Promise already satisfied, ignore
				NETWORK_LOG_DEBUG("[messaging_client] Promise already satisfied");
			}
			stop_promise_.reset();
		}
		stop_future_ = std::future<void>();

		NETWORK_LOG_INFO("[messaging_client] stopped.");
		return ok();
	}
	catch (const std::exception& e)
	{
		stop_initiated_.store(false);
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to stop client: " + std::string(e.what()),
			"messaging_client::stop_client",
			"Client ID: " + client_id_
		);
	}
	}

	auto messaging_client::wait_for_stop() -> void
	{
		if (stop_future_.valid())
		{
			stop_future_.wait();
		}
	}

	auto messaging_client::do_connect(std::string_view host,
									  unsigned short port) -> void
	{
		// Use resolver to get endpoints
		// Create resolver on heap to keep it alive during async operation
		auto resolver = std::make_shared<tcp::resolver>(*io_context_);
		auto self = shared_from_this();

		NETWORK_LOG_INFO("[messaging_client] Starting async resolve for " + std::string(host) + ":" + std::to_string(port));

		resolver->async_resolve(
			std::string(host), std::to_string(port),
			[this, self, resolver](std::error_code ec,
						   tcp::resolver::results_type results)
			{
				NETWORK_LOG_INFO("[messaging_client] Resolve callback invoked");
				if (ec)
				{
					NETWORK_LOG_ERROR("[messaging_client] Resolve error: " + ec.message());
					return;
				}
				NETWORK_LOG_INFO("[messaging_client] Resolve successful, starting async connect");
				// Attempt to connect to one of the resolved endpoints
				// Create socket on heap to avoid dangling reference
				auto raw_socket = std::make_shared<tcp::socket>(*io_context_);
				asio::async_connect(
					*raw_socket, results,
					[this, self, raw_socket](std::error_code connect_ec,
											 const tcp::endpoint& endpoint)
					{
						NETWORK_LOG_INFO("[messaging_client] Connect callback invoked");
						if (connect_ec)
						{
							NETWORK_LOG_ERROR("[messaging_client] Connect error: " + connect_ec.message());
							return;
						}
						NETWORK_LOG_INFO("[messaging_client] Connect successful, calling on_connect");
						// On success, wrap it in our tcp_socket with mutex protection
						{
							std::lock_guard<std::mutex> lock(socket_mutex_);
							socket_ = std::make_shared<internal::tcp_socket>(
								std::move(*raw_socket));
						}
						on_connect(connect_ec);
					});
			});
	}

	auto messaging_client::on_connect(std::error_code ec) -> void
	{
		if (ec)
		{
			NETWORK_LOG_ERROR("[messaging_client] on_connect error: " + ec.message());
			return;
		}
		NETWORK_LOG_INFO("[messaging_client] Connected successfully.");
		is_connected_.store(true);

		// Invoke connected callback if set
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connected_callback_)
			{
				try
				{
					connected_callback_();
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[messaging_client] Exception in connected callback: "
					                  + std::string(e.what()));
				}
			}
		}

	// set callbacks and start read loop with mutex protection
	auto self = shared_from_this();
	auto local_socket = get_socket();

		if (local_socket)
		{
			local_socket->set_receive_callback(
				[this, self](const std::vector<uint8_t>& chunk)
				{ on_receive(chunk); });
			local_socket->set_error_callback([this, self](std::error_code err)
										{ on_error(err); });
			local_socket->start_read();
		}
	}

	auto messaging_client::send_packet(std::vector<uint8_t>&& data) -> VoidResult
	{
		if (!is_running_.load())
		{
			return error_void(
				error_codes::network_system::connection_closed,
				"Client is not running",
				"messaging_client::send_packet",
				"Client ID: " + client_id_
			);
		}

		if (data.empty())
		{
			return error_void(
				error_codes::common_errors::invalid_argument,
				"Data cannot be empty",
				"messaging_client::send_packet",
				"Client ID: " + client_id_
			);
		}

	// Get a local copy of socket with mutex protection
	auto local_socket = get_socket();

		if (!is_connected_.load() || !local_socket)
		{
			return error_void(
				error_codes::network_system::connection_closed,
				"Client is not connected",
				"messaging_client::send_packet",
				"Client ID: " + client_id_
			);
		}

// Using if constexpr for compile-time branching (C++17)
if constexpr (std::is_same_v<decltype(local_socket->socket().get_executor()), asio::io_context::executor_type>)
{
#ifdef USE_STD_COROUTINE
		// Coroutine approach
		asio::co_spawn(local_socket->socket().get_executor(),
					   async_send_with_pipeline_co(local_socket, std::move(data),
												   pipeline_, compress_mode_,
												   encrypt_mode_),
					   [](std::error_code ec)
					   {
						   if (ec)
						   {
							   NETWORK_LOG_ERROR("[messaging_client] Send error: " + ec.message());
						   }
					   });
#else
		// Fallback approach
		auto fut = async_send_with_pipeline_no_co(
			local_socket, std::move(data), pipeline_, compress_mode_, encrypt_mode_);
		// Use structured binding with try/catch for better error handling (C++17)
		try {
			auto result_ec = fut.get();
			if (result_ec)
			{
				NETWORK_LOG_ERROR("[messaging_client] Send error: " + result_ec.message());
			}
		} catch (const std::exception& e) {
			NETWORK_LOG_ERROR("[messaging_client] Exception while waiting for send: " + std::string(e.what()));
		}
#endif
}
		return ok();
	}

	auto messaging_client::on_receive(const std::vector<uint8_t>& data) -> void
	{
		if (!is_connected_.load())
		{
			return;
		}
		NETWORK_LOG_DEBUG("[messaging_client] Received " + std::to_string(data.size())
				+ " bytes");

		// Invoke receive callback if set
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				try
				{
					receive_callback_(data);
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[messaging_client] Exception in receive callback: "
					                  + std::string(e.what()));
				}
			}
		}

		// Decompress/Decrypt if needed?
		// For demonstration, ignoring. In real usage:
		//   auto uncompressed = pipeline_.decompress(...);
		//   auto decrypted = pipeline_.decrypt(...);
		//   parse or handle...
	}

auto messaging_client::on_error(std::error_code ec) -> void
{
	NETWORK_LOG_ERROR("[messaging_client] Socket error: " + ec.message());

	// Invoke error callback if set
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (error_callback_)
		{
			try
			{
				error_callback_(ec);
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR("[messaging_client] Exception in error callback: "
				                  + std::string(e.what()));
			}
		}
	}

	// Invoke disconnected callback if was connected
	if (is_connected_.load())
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (disconnected_callback_)
		{
			try
			{
				disconnected_callback_();
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR("[messaging_client] Exception in disconnected callback: "
				                  + std::string(e.what()));
			}
		}
	}

	// Mark connection as lost but don't call stop_client from callback thread
	// to avoid race conditions with destructor or explicit stop_client calls
	is_connected_.store(false);
	is_running_.store(false);
}

auto messaging_client::get_socket() const -> std::shared_ptr<internal::tcp_socket>
{
	std::lock_guard<std::mutex> lock(socket_mutex_);
	return socket_;
}

auto messaging_client::set_receive_callback(
	std::function<void(const std::vector<uint8_t>&)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	receive_callback_ = std::move(callback);
}

auto messaging_client::set_connected_callback(std::function<void()> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	connected_callback_ = std::move(callback);
}

auto messaging_client::set_disconnected_callback(std::function<void()> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	disconnected_callback_ = std::move(callback);
}

auto messaging_client::set_error_callback(std::function<void(std::error_code)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	error_callback_ = std::move(callback);
}

} // namespace network_system::core
