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

#include "kcenon/network/session/messaging_session.h"

#include <span>

#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/metrics/network_metrics.h"

// Use nested namespace definition (C++17)
namespace kcenon::network::session
{

	// Use string_view in constructor for efficiency (C++17)
	messaging_session::messaging_session(asio::ip::tcp::socket socket,
										 std::string_view server_id)
		: server_id_(server_id)
	{
		// Create the tcp_socket wrapper
		socket_ = std::make_shared<internal::tcp_socket>(std::move(socket));
	}

	messaging_session::~messaging_session() noexcept
	{
		try
		{
			// Call stop_session to clean up resources
			stop_session();
		}
		catch (...)
		{
			// Destructor must not throw - swallow all exceptions
		}
	}

	auto messaging_session::start_session() -> void
	{
		// NOTE: No logging in session methods to prevent heap corruption during
		// static destruction. These methods may run in async handlers when
		// GlobalLoggerRegistry is already destroyed.

		if (is_stopped_.load())
		{
			return;
		}

		// Set up callbacks with weak_ptr to avoid circular reference
		// Use span-based callback for zero-copy receive path (no per-read allocation)
		auto weak_self = weak_from_this();
		socket_->set_receive_callback_view(
			[weak_self](std::span<const uint8_t> data)
			{
				if (auto self = weak_self.lock())
				{
					self->on_receive(data);
				}
			});
		socket_->set_error_callback([weak_self](std::error_code ec)
			{
				if (auto self = weak_self.lock())
				{
					self->on_error(ec);
				}
			});

		// Begin reading
		socket_->start_read();
	}

	auto messaging_session::stop_session() -> void
	{
		if (is_stopped_.exchange(true))
		{
			return;
		}
		// Stop reading first to prevent new async operations
		if (socket_)
		{
			socket_->stop_read();
		}
		// Close socket safely
		if (socket_)
		{
			std::error_code ec;
			socket_->socket().close(ec);
			// Ignore close errors silently - no logging during potential static destruction
		}

		// Invoke disconnection callback if set
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (disconnection_callback_)
			{
				try
				{
					disconnection_callback_(server_id_);
				}
				catch (...)
				{
					// Silently ignore exceptions - no logging during potential static destruction
				}
			}
		}
	}

	auto messaging_session::send_packet(std::vector<uint8_t>&& data) -> void
	{
		if (is_stopped_.load())
		{
			return;
		}

		// Report bytes sent metric
		metrics::metric_reporter::report_bytes_sent(data.size());

		// Send data directly without pipeline transformation
		// NOTE: No logging in async callbacks to prevent heap corruption during static destruction
		socket_->async_send(std::move(data),
			[](std::error_code, std::size_t) {
				// Silently ignore errors - no logging during potential static destruction
			});
	}

	auto messaging_session::on_receive(std::span<const uint8_t> data) -> void
	{
		// NOTE: No logging in async handlers to prevent heap corruption during
		// static destruction.

		if (is_stopped_.load())
		{
			return;
		}

		// Report bytes received metric
		metrics::metric_reporter::report_bytes_received(data.size());

		// Check queue size before adding
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			size_t queue_size = pending_messages_.size();

			// Apply backpressure if queue is getting full
			// If queue is severely overflowing, disconnect the session
			if (queue_size >= max_pending_messages_ * 2)
			{
				stop_session();
				return;
			}

			// Copy span data into vector only when adding to queue (single copy point)
			// This is the only allocation in the receive path
			pending_messages_.emplace_back(data.begin(), data.end());
		}

		// Process messages from queue
		process_next_message();
	}

	auto messaging_session::on_error(std::error_code ec) -> void
	{
		// NOTE: No logging in async handlers to prevent heap corruption during
		// static destruction.

		// Invoke error callback if set
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (error_callback_)
			{
				try
				{
					error_callback_(ec);
				}
				catch (...)
				{
					// Silently ignore exceptions - no logging during potential static destruction
				}
			}
		}

		stop_session();
	}

	auto messaging_session::process_next_message() -> void
	{
		// NOTE: No logging in message processing to prevent heap corruption during
		// static destruction.

		std::vector<uint8_t> message;

		// Dequeue one message
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			if (pending_messages_.empty())
			{
				return;
			}

			message = std::move(pending_messages_.front());
			pending_messages_.pop_front();
		}

		// Invoke receive callback if set
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				try
				{
					receive_callback_(message);
				}
				catch (...)
				{
					// Silently ignore exceptions - no logging during potential static destruction
				}
			}
		}

		// If there are more messages, process them asynchronously
		// to avoid blocking the receive thread
		std::lock_guard<std::mutex> lock(queue_mutex_);
		if (!pending_messages_.empty())
		{
			// In a production system, you might want to post this to a thread pool
			// or use a work queue to process messages asynchronously
			// For now, we just process one message per call
		}
	}

	auto messaging_session::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		receive_callback_ = std::move(callback);
	}

	auto messaging_session::set_disconnection_callback(
		std::function<void(const std::string&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		disconnection_callback_ = std::move(callback);
	}

	auto messaging_session::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::move(callback);
	}

} // namespace kcenon::network::session
