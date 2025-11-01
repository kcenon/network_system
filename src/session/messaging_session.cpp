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

#include "network_system/session/messaging_session.h"

#include <type_traits>

#include "network_system/integration/logger_integration.h"
#include "network_system/internal/send_coroutine.h"

// Use nested namespace definition (C++17)
namespace network_system::session
{

	// Use string_view in constructor for efficiency (C++17)
	messaging_session::messaging_session(asio::ip::tcp::socket socket,
										 std::string_view server_id)
		: server_id_(server_id)
	{
		// Create the tcp_socket wrapper
		socket_ = std::make_shared<internal::tcp_socket>(std::move(socket));

		// Initialize the pipeline (stub)
		pipeline_ = internal::make_default_pipeline();

		// Default modes - could use inline initialization in header with C++17
		compress_mode_ = false;
		encrypt_mode_ = false;
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
		if (is_stopped_.load())
		{
			return;
		}

		// Set up callbacks with weak_ptr to avoid circular reference
		std::weak_ptr<messaging_session> weak_self = shared_from_this();

		socket_->set_receive_callback(
			[this, weak_self](const std::vector<uint8_t>& data)
			{
				// Lock weak_ptr to ensure session is still alive
				if (auto self = weak_self.lock())
				{
					on_receive(data);
				}
			});

		socket_->set_error_callback(
			[this, weak_self](std::error_code ec)
			{
				// Lock weak_ptr to ensure session is still alive
				if (auto self = weak_self.lock())
				{
					on_error(ec);
				}
			});

		// Begin reading
		socket_->start_read();

		NETWORK_LOG_INFO("[messaging_session] Started session on server: " + server_id_);
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
			if (ec)
			{
				NETWORK_LOG_ERROR("[messaging_session] Error closing socket: " + ec.message());
			}
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
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[messaging_session] Exception in disconnection callback: "
					                  + std::string(e.what()));
				}
			}
		}

		NETWORK_LOG_INFO("[messaging_session] Stopped.");
	}

	auto messaging_session::send_packet(std::vector<uint8_t>&& data) -> void
	{
		NETWORK_LOG_INFO("[messaging_session] send_packet called with " + std::to_string(data.size()) + " bytes");

		if (is_stopped_.load())
		{
			NETWORK_LOG_ERROR("[messaging_session] send_packet: session is stopped, not sending");
			return;
		}

		// Capture mode flags atomically
		bool compress_mode;
		bool encrypt_mode;
		{
			std::lock_guard<std::mutex> lock(mode_mutex_);
			compress_mode = compress_mode_;
			encrypt_mode = encrypt_mode_;
		}

		NETWORK_LOG_INFO("[messaging_session] send_packet: compress=" + std::to_string(compress_mode) +
		                ", encrypt=" + std::to_string(encrypt_mode));

// Using if constexpr for compile-time branching (C++17)
if constexpr (std::is_same_v<decltype(socket_->socket().get_executor()), asio::io_context::executor_type>)
{
#ifdef USE_STD_COROUTINE
		// Coroutine-based approach
		asio::co_spawn(socket_->socket().get_executor(),
					   internal::async_send_with_pipeline_co(socket_, std::move(data),
												   pipeline_, compress_mode,
												   encrypt_mode),
					   [](std::error_code ec)
					   {
						   if (ec)
						   {
							   NETWORK_LOG_ERROR("[messaging_session] Send error: " + ec.message());
						   }
					   });
#else
		// Fallback approach
		auto fut = internal::async_send_with_pipeline_no_co(
			socket_, std::move(data), pipeline_, compress_mode, encrypt_mode);

		// Use structured binding with try/catch for better error handling (C++17)
		try {
			auto result_ec = fut.get();
			if (result_ec)
			{
				NETWORK_LOG_ERROR("[messaging_session] Send error: " + result_ec.message());
			}
		} catch (const std::exception& e) {
			NETWORK_LOG_ERROR("[messaging_session] Exception while waiting for send: " + std::string(e.what()));
		}
#endif
}
	}

	auto messaging_session::send_packet_sync(std::vector<uint8_t>&& data) -> std::error_code
	{
		NETWORK_LOG_INFO("[messaging_session] send_packet_sync called with " + std::to_string(data.size()) + " bytes");

		if (is_stopped_.load())
		{
			NETWORK_LOG_ERROR("[messaging_session] send_packet_sync: session is stopped, not sending");
			return std::make_error_code(std::errc::not_connected);
		}

		if (!socket_)
		{
			NETWORK_LOG_ERROR("[messaging_session] send_packet_sync: socket is null");
			return std::make_error_code(std::errc::not_connected);
		}

		// Perform synchronous write
		std::error_code ec;
		try
		{
			asio::write(socket_->socket(), asio::buffer(data), ec);
			if (ec)
			{
				NETWORK_LOG_ERROR("[messaging_session] send_packet_sync failed: " + ec.message());
			}
			else
			{
				NETWORK_LOG_INFO("[messaging_session] send_packet_sync completed: " + std::to_string(data.size()) + " bytes");
			}
		}
		catch (const std::exception& e)
		{
			NETWORK_LOG_ERROR("[messaging_session] send_packet_sync exception: " + std::string(e.what()));
			return std::make_error_code(std::errc::io_error);
		}

		return ec;
	}

	auto messaging_session::on_receive(const std::vector<uint8_t>& data) -> void
	{
		if (is_stopped_.load())
		{
			return;
		}

		NETWORK_LOG_DEBUG("[messaging_session] Received " + std::to_string(data.size())
				+ " bytes.");

		// Check queue size before adding
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			size_t queue_size = pending_messages_.size();

			// Apply backpressure if queue is getting full
			if (queue_size >= max_pending_messages_)
			{
				NETWORK_LOG_WARN("[messaging_session] Queue size (" +
					std::to_string(queue_size) + ") reached limit (" +
					std::to_string(max_pending_messages_) + "). Applying backpressure.");

				// If queue is severely overflowing, disconnect the session
				if (queue_size >= max_pending_messages_ * 2)
				{
					NETWORK_LOG_ERROR("[messaging_session] Queue overflow (" +
						std::to_string(queue_size) + "). Disconnecting abusive client.");
					stop_session();
					return;
				}
			}

			// Add message to queue
			pending_messages_.push_back(data);
		}

		// Process messages from queue
		process_next_message();
	}

	auto messaging_session::on_error(std::error_code ec) -> void
	{
		NETWORK_LOG_ERROR("[messaging_session] Socket error: " + ec.message());

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
					NETWORK_LOG_ERROR("[messaging_session] Exception in error callback: "
					                  + std::string(e.what()));
				}
			}
		}

		stop_session();
	}

	auto messaging_session::process_next_message() -> void
	{
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

		// Process the message
		// For now, just log it. In a real implementation, you would:
		// 1. Decompress + decrypt using pipeline_ if needed
		// 2. Parse message format (length-prefixed, framed, etc.)
		// 3. Dispatch to application-level handler
		NETWORK_LOG_DEBUG("[messaging_session] Processing message of " +
			std::to_string(message.size()) + " bytes. Queue remaining: " +
			std::to_string(pending_messages_.size()));

		// Invoke receive callback if set
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				try
				{
					receive_callback_(message);
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[messaging_session] Exception in receive callback: "
					                  + std::string(e.what()));
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

} // namespace network_system::session
