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

#include "network_tcp/internal/tcp_socket.h"

#include <atomic>
#include <iostream> // for debugging/logging
#include <type_traits>

// Use nested namespace definition (C++17)
namespace kcenon::network::internal
{

	tcp_socket::tcp_socket(asio::ip::tcp::socket socket)
		: socket_(std::move(socket)), config_{}
	{
		// constructor body empty
	}

	tcp_socket::tcp_socket(asio::ip::tcp::socket socket, const socket_config& config)
		: socket_(std::move(socket)), config_(config)
	{
		// constructor body empty
	}

	auto tcp_socket::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		auto new_cb = std::make_shared<receive_callback_t>(std::move(callback));
		std::lock_guard<std::mutex> lock(callback_mutex_);
		std::atomic_store(&receive_callback_, new_cb);
	}

	auto tcp_socket::set_receive_callback_view(
		std::function<void(std::span<const uint8_t>)> callback) -> void
	{
		auto new_cb = std::make_shared<receive_callback_view_t>(std::move(callback));
		std::lock_guard<std::mutex> lock(callback_mutex_);
		std::atomic_store(&receive_callback_view_, new_cb);
	}

	auto tcp_socket::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		auto new_cb = std::make_shared<error_callback_t>(std::move(callback));
		std::lock_guard<std::mutex> lock(callback_mutex_);
		std::atomic_store(&error_callback_, new_cb);
	}

	auto tcp_socket::start_read() -> void
	{
		// Prevent duplicate read operations - only start if not already reading
		bool expected = false;
		if (!is_reading_.compare_exchange_strong(expected, true))
		{
			// Already reading, don't start another async operation
			return;
		}
		do_read();
	}

	auto tcp_socket::stop_read() -> void
	{
		// Stop further read operations
		is_reading_.store(false);
	}

	auto tcp_socket::close() -> void
	{
		// Atomically mark socket as closed before any other operation.
		// Use exchange to ensure only one thread proceeds with closing.
		bool expected = false;
		if (!is_closed_.compare_exchange_strong(expected, true))
		{
			// Already closed by another thread
			return;
		}

		// Stop the read loop
		is_reading_.store(false);

		// Close the socket under mutex protection.
		// This ensures mutual exclusion with async_read_some() and async_write()
		// operations, preventing the race condition where socket internal state
		// (descriptor_data_) is accessed concurrently from multiple threads.
		//
		// Note: We use a synchronous close with mutex protection rather than
		// posting to executor because io_context may be running on multiple
		// threads, and asio::post does not guarantee serialization across threads.
		std::lock_guard<std::mutex> lock(socket_mutex_);
		std::error_code ec;
		if (socket_.is_open())
		{
			// Cancel pending async operations first
			socket_.cancel(ec);

			// Shutdown causes pending reads to complete with error
			socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);

			// Now safe to close the socket
			socket_.close(ec);
		}
		// Ignore close errors - socket may already be closed
	}

	auto tcp_socket::is_closed() const -> bool
	{
		return is_closed_.load();
	}

	auto tcp_socket::do_read() -> void
	{
		// Check if reading has been stopped before initiating new async operation
		if (!is_reading_.load())
		{
			return;
		}

		// Early check for closed state to avoid unnecessary work
		if (is_closed_.load())
		{
			is_reading_.store(false);
			return;
		}

		auto self = shared_from_this();

		// Post the async read to the socket's executor.
		asio::post(socket_.get_executor(), [self]() {
			// Re-check state after being scheduled on the executor
			if (!self->is_reading_.load() || self->is_closed_.load())
			{
				self->is_reading_.store(false);
				return;
			}

			// Lock the socket mutex to prevent concurrent access with close().
			// This is critical because io_context may run on multiple threads,
			// and close() may be called from any thread at any time.
			// The lock ensures mutual exclusion between socket operations.
			std::unique_lock<std::mutex> lock(self->socket_mutex_);

			// Re-check state under lock - close() may have acquired the lock first
			if (!self->is_reading_.load() || self->is_closed_.load() || !self->socket_.is_open())
			{
				self->is_reading_.store(false);
				return;
			}

			// Now safe to initiate async operation under lock protection.
			// Release the lock before async_read_some since the callback may
			// be invoked from the same thread and try to acquire the lock again.
			// Instead, we rely on is_closed_ flag for callback safety.
			lock.unlock();

			self->socket_.async_read_some(
				asio::buffer(self->read_buffer_),
				[self](std::error_code ec, std::size_t length)
				{
					// Check if reading has been stopped or socket closed at callback time
					// This prevents accessing invalid socket state after close()
					if (!self->is_reading_.load() || self->is_closed_.load())
					{
						return;
					}

					if (ec)
					{
						// On error, invoke the error callback
						// Lock-free callback access via atomic_load
						auto error_cb = std::atomic_load(&self->error_callback_);
						if (error_cb && *error_cb)
						{
							(*error_cb)(ec);
						}
						return;
					}

					// On success, if length > 0, dispatch to the appropriate callback
					if (length > 0)
					{
						// Lock-free callback access via atomic_load
						// Prefer view callback (zero-copy) over vector callback
						auto view_cb = std::atomic_load(&self->receive_callback_view_);
						if (view_cb && *view_cb)
						{
							// Zero-copy path: create span view directly into read_buffer_
							// No std::vector allocation or copy required
							std::span<const uint8_t> data_view(self->read_buffer_.data(), length);
							(*view_cb)(data_view);
						}
						else
						{
							// Legacy path: allocate and copy into vector for compatibility
							auto recv_cb = std::atomic_load(&self->receive_callback_);
							if (recv_cb && *recv_cb)
							{
								std::vector<uint8_t> chunk(self->read_buffer_.begin(),
														   self->read_buffer_.begin() + length);
								(*recv_cb)(chunk);
							}
						}
					}

					// Continue reading only if still active and socket is not closed
					// Use atomic is_closed_ flag to prevent data race with close()
					if (self->is_reading_.load() && !self->is_closed_.load())
					{
						self->do_read();
					}
				});
		});
	}

auto tcp_socket::async_send(
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler) -> void
{
    // Early check for closed state
    if (is_closed_.load())
    {
        if (handler)
        {
            handler(asio::error::not_connected, 0);
        }
        return;
    }

    auto self = shared_from_this();
    std::size_t data_size = data.size();

    // Move data into shared_ptr for lifetime management
    auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(data));

    // Track pending bytes before posting to executor
    std::size_t new_pending = pending_bytes_.fetch_add(data_size) + data_size;
    metrics_.current_pending_bytes.store(new_pending);

    // Update peak pending bytes
    std::size_t peak = metrics_.peak_pending_bytes.load();
    while (new_pending > peak &&
           !metrics_.peak_pending_bytes.compare_exchange_weak(peak, new_pending))
    {
        // Loop until we successfully update or find a higher value
    }

    // Check high water mark for backpressure
    if (config_.high_water_mark > 0 && !backpressure_active_.load() &&
        new_pending >= config_.high_water_mark)
    {
        backpressure_active_.store(true);
        metrics_.backpressure_events.fetch_add(1);

        // Invoke backpressure callback
        auto bp_cb = std::atomic_load(&backpressure_callback_);
        if (bp_cb && *bp_cb)
        {
            (*bp_cb)(true);
        }
    }

    // Post the async write to the socket's executor.
    // This ensures all socket operations (read, write, close) run on the same
    // thread/strand, preventing race conditions between close() and async_write().
    asio::post(socket_.get_executor(), [self, buffer, data_size, handler = std::move(handler)]() mutable {
        // Re-check state after being scheduled on the executor
        if (self->is_closed_.load() || !self->socket_.is_open())
        {
            // Restore pending bytes count
            self->pending_bytes_.fetch_sub(data_size);
            self->metrics_.current_pending_bytes.store(self->pending_bytes_.load());

            if (handler)
            {
                handler(asio::error::not_connected, 0);
            }
            return;
        }

        // Now safe to initiate async operation
        // Wrap in try-catch to handle race condition where socket may be closed
        // between our check and the actual async operation
        try
        {
            asio::async_write(
                self->socket_, asio::buffer(*buffer),
                [handler = std::move(handler), self, buffer, data_size](std::error_code ec, std::size_t bytes_transferred)
                {
                    // Update pending bytes on completion
                    std::size_t remaining = self->pending_bytes_.fetch_sub(data_size) - data_size;
                    self->metrics_.current_pending_bytes.store(remaining);

                    if (!ec)
                    {
                        self->metrics_.total_bytes_sent.fetch_add(bytes_transferred);
                        self->metrics_.send_count.fetch_add(1);
                    }

                    // Check low water mark to release backpressure
                    if (self->config_.low_water_mark > 0 && self->backpressure_active_.load() &&
                        remaining <= self->config_.low_water_mark)
                    {
                        self->backpressure_active_.store(false);

                        // Invoke backpressure callback
                        auto bp_cb = std::atomic_load(&self->backpressure_callback_);
                        if (bp_cb && *bp_cb)
                        {
                            (*bp_cb)(false);
                        }
                    }

                    if constexpr (std::is_invocable_v<decltype(handler), std::error_code, std::size_t>)
                    {
                        if (handler)
                        {
                            handler(ec, bytes_transferred);
                        }
                    }
                });
        }
        catch (const std::exception&)
        {
            // Socket was closed between our check and async_write() call.
            // Restore pending bytes count and invoke handler with error.
            self->pending_bytes_.fetch_sub(data_size);
            self->metrics_.current_pending_bytes.store(self->pending_bytes_.load());

            if (handler)
            {
                handler(asio::error::not_connected, 0);
            }
        }
    });
}

auto tcp_socket::set_backpressure_callback(backpressure_callback callback) -> void
{
    auto new_cb = std::make_shared<backpressure_callback>(std::move(callback));
    std::lock_guard<std::mutex> lock(callback_mutex_);
    std::atomic_store(&backpressure_callback_, new_cb);
}

auto tcp_socket::try_send(
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler) -> bool
{
    std::size_t data_size = data.size();
    std::size_t current = pending_bytes_.load();

    // Check if max_pending_bytes limit would be exceeded
    if (config_.max_pending_bytes > 0 &&
        current + data_size > config_.max_pending_bytes)
    {
        metrics_.rejected_sends.fetch_add(1);
        return false;
    }

    // Proceed with async_send
    async_send(std::move(data), std::move(handler));
    return true;
}

auto tcp_socket::pending_bytes() const -> std::size_t
{
    return pending_bytes_.load();
}

auto tcp_socket::is_backpressure_active() const -> bool
{
    return backpressure_active_.load();
}

auto tcp_socket::metrics() const -> const socket_metrics&
{
    return metrics_;
}

auto tcp_socket::reset_metrics() -> void
{
    metrics_.reset();
}

auto tcp_socket::config() const -> const socket_config&
{
    return config_;
}

} // namespace kcenon::network::internal
