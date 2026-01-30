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
		// Early validation - check if socket is usable before starting read loop.
		// This prevents calling async operations on a socket that was moved-from
		// after being closed, which would cause SEGV due to null descriptor_data_.
		if (is_closed_.load() || !socket_.is_open() || socket_.native_handle() == -1)
		{
			return;
		}

		// Prevent duplicate read operations - only start if not already reading
		bool expected = false;
		if (!is_reading_.compare_exchange_strong(expected, true))
		{
			// Already reading, don't start another async operation
			return;
		}

		// Post do_read() to the socket's executor to ensure ASIO internal state
		// is properly initialized. When start_read() is called from a thread
		// different from the io_context thread (e.g., main thread in tests),
		// directly calling async_read_some() can cause SEGV because ASIO's
		// per-thread state and reactor registration may not be properly set up.
		// Using asio::post() ensures the async operation starts on the correct
		// executor context where ASIO can properly manage its internal state.
		auto self = shared_from_this();
		asio::post(socket_.get_executor(),
				   [self]()
				   {
					   // Re-check state after post - socket may have been closed
					   if (self->is_reading_.load() && !self->is_closed_.load())
					   {
						   self->do_read();
					   }
				   });
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

		// Post the close operation to the socket's executor to ensure thread-safety.
		// Since async_read_some() and async_write() now run on the executor,
		// we need close() to also run on the executor to serialize operations.
		// This prevents data races where close() modifies socket state while
		// an async operation is being initiated.
		auto self = shared_from_this();
		asio::post(socket_.get_executor(),
				   [self]()
				   {
					   std::error_code ec;
					   if (self->socket_.is_open())
					   {
						   // Cancel pending async operations first
						   self->socket_.cancel(ec);

						   // Shutdown causes pending reads to complete with error
						   self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);

						   // Now safe to close the socket
						   self->socket_.close(ec);
					   }
					   // Ignore close errors - socket may already be closed
				   });
	}

	auto tcp_socket::is_closed() const -> bool
	{
		return is_closed_.load();
	}

	auto tcp_socket::do_read() -> void
	{
		// This function is always called from the socket's executor thread
		// (either via asio::post() from start_read(), or from the async callback).
		// No mutex needed since we're guaranteed to be on the correct thread.

		// Check if reading has been stopped before initiating new async operation
		if (!is_reading_.load())
		{
			return;
		}

		// Check for closed state to avoid operations on closed socket.
		// Also check native_handle() to catch sockets that have been closed
		// but not yet reflected in is_closed_ flag (race with close()).
		if (is_closed_.load() || !socket_.is_open() || socket_.native_handle() == -1)
		{
			is_reading_.store(false);
			return;
		}

		auto self = shared_from_this();

		// Safe to initiate async operation on the executor thread.
		// ASIO's internal state (including descriptor_data_) is properly
		// accessible when we're on the executor's thread context.
		//
		// Wrap in try-catch to handle potential allocation failures in ASIO's
		// async operation setup. This can occur with sanitizers when the
		// recycling allocator's thread context is not properly initialized.
		try
		{
			socket_.async_read_some(
				asio::buffer(read_buffer_),
				[self](std::error_code ec, std::size_t length)
				{
					// Check if reading has been stopped or socket closed at callback time.
					// This prevents processing data after socket is closed.
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

						// Notify observers of error
						self->notify_observers_error(ec);
						return;
					}

					// On success, if length > 0, dispatch to the appropriate callback
					if (length > 0)
					{
						// Create span view for zero-copy access
						std::span<const uint8_t> data_view(self->read_buffer_.data(), length);

						// Lock-free callback access via atomic_load
						// Prefer view callback (zero-copy) over vector callback
						auto view_cb = std::atomic_load(&self->receive_callback_view_);
						if (view_cb && *view_cb)
						{
							// Zero-copy path: create span view directly into read_buffer_
							// No std::vector allocation or copy required
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

						// Notify observers with zero-copy span view
						self->notify_observers_receive(data_view);
					}

					// Continue reading only if still active and socket is not closed
					// Use atomic is_closed_ flag to prevent data race with close()
					if (self->is_reading_.load() && !self->is_closed_.load())
					{
						self->do_read();
					}
				});
		}
		catch (const std::bad_alloc&)
		{
			// Allocation failed - stop reading and report error
			is_reading_.store(false);
			auto error_cb = std::atomic_load(&error_callback_);
			if (error_cb && *error_cb)
			{
				(*error_cb)(std::make_error_code(std::errc::not_enough_memory));
			}
		}
		catch (const std::exception&)
		{
			// Other exception - stop reading and report generic error
			is_reading_.store(false);
			auto error_cb = std::atomic_load(&error_callback_);
			if (error_cb && *error_cb)
			{
				(*error_cb)(std::make_error_code(std::errc::operation_canceled));
			}
		}
	}

auto tcp_socket::async_send(
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler) -> void
{
    // Early check for closed state before any socket access
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

    // Track pending bytes before initiating async operation
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

        // Notify observers of backpressure
        notify_observers_backpressure(true);
    }

    // Post the async_write to the socket's executor to ensure ASIO internal state
    // is properly initialized. When async_send() is called from a thread
    // different from the io_context thread (e.g., main thread in tests),
    // directly calling async_write() can cause SEGV because ASIO's per-thread
    // state and reactor registration may not be properly set up.
    // Using asio::post() ensures the async operation starts on the correct
    // executor context where ASIO can properly manage its internal state.
    auto handler_ptr = std::make_shared<std::function<void(std::error_code, std::size_t)>>(
        std::move(handler));

    asio::post(socket_.get_executor(),
        [self, buffer, data_size, handler_ptr]()
        {
            // Re-check state after post - socket may have been closed
            if (self->is_closed_.load() || !self->socket_.is_open() ||
                self->socket_.native_handle() == -1)
            {
                // Restore pending bytes count
                self->pending_bytes_.fetch_sub(data_size);
                self->metrics_.current_pending_bytes.store(self->pending_bytes_.load());

                if (*handler_ptr)
                {
                    (*handler_ptr)(asio::error::not_connected, 0);
                }
                return;
            }

            // Now safe to initiate async operation on the executor thread.
            // No mutex needed here since we're on the executor's thread.
            //
            // Wrap in try-catch to handle potential allocation failures in ASIO's
            // async operation setup. This can occur with sanitizers when the
            // recycling allocator's thread context is not properly initialized.
            try
            {
                asio::async_write(
                    self->socket_, asio::buffer(*buffer),
                    [handler_ptr, self, buffer, data_size](std::error_code ec, std::size_t bytes_transferred)
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

                            // Notify observers of backpressure release
                            self->notify_observers_backpressure(false);
                        }

                        if (*handler_ptr)
                        {
                            (*handler_ptr)(ec, bytes_transferred);
                        }
                    });
            }
            catch (const std::bad_alloc&)
            {
                // Allocation failed - restore pending bytes and report error
                self->pending_bytes_.fetch_sub(data_size);
                self->metrics_.current_pending_bytes.store(self->pending_bytes_.load());

                if (*handler_ptr)
                {
                    (*handler_ptr)(std::make_error_code(std::errc::not_enough_memory), 0);
                }
            }
            catch (const std::exception&)
            {
                // Other exception - restore pending bytes and report generic error
                self->pending_bytes_.fetch_sub(data_size);
                self->metrics_.current_pending_bytes.store(self->pending_bytes_.load());

                if (*handler_ptr)
                {
                    (*handler_ptr)(std::make_error_code(std::errc::operation_canceled), 0);
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

auto tcp_socket::attach_observer(std::shared_ptr<network_core::interfaces::socket_observer> observer) -> void
{
	if (!observer)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(callback_mutex_);

	// Remove expired observers before adding new one
	observers_.erase(
		std::remove_if(observers_.begin(), observers_.end(),
			[](const std::weak_ptr<network_core::interfaces::socket_observer>& weak_obs) {
				return weak_obs.expired();
			}),
		observers_.end());

	// Check if observer is already attached
	auto it = std::find_if(observers_.begin(), observers_.end(),
		[&observer](const std::weak_ptr<network_core::interfaces::socket_observer>& weak_obs) {
			auto existing = weak_obs.lock();
			return existing && existing == observer;
		});

	if (it == observers_.end())
	{
		observers_.push_back(observer);
	}
}

auto tcp_socket::detach_observer(std::shared_ptr<network_core::interfaces::socket_observer> observer) -> void
{
	if (!observer)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(callback_mutex_);

	observers_.erase(
		std::remove_if(observers_.begin(), observers_.end(),
			[&observer](const std::weak_ptr<network_core::interfaces::socket_observer>& weak_obs) {
				auto existing = weak_obs.lock();
				return !existing || existing == observer;
			}),
		observers_.end());
}

auto tcp_socket::notify_observers_receive(std::span<const uint8_t> data) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);

	for (auto it = observers_.begin(); it != observers_.end();)
	{
		if (auto observer = it->lock())
		{
			observer->on_receive(data);
			++it;
		}
		else
		{
			// Remove expired observer
			it = observers_.erase(it);
		}
	}
}

auto tcp_socket::notify_observers_error(std::error_code ec) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);

	for (auto it = observers_.begin(); it != observers_.end();)
	{
		if (auto observer = it->lock())
		{
			observer->on_error(ec);
			++it;
		}
		else
		{
			// Remove expired observer
			it = observers_.erase(it);
		}
	}
}

auto tcp_socket::notify_observers_backpressure(bool apply) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);

	for (auto it = observers_.begin(); it != observers_.end();)
	{
		if (auto observer = it->lock())
		{
			observer->on_backpressure(apply);
			++it;
		}
		else
		{
			// Remove expired observer
			it = observers_.erase(it);
		}
	}
}

} // namespace kcenon::network::internal
