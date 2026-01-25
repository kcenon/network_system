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

#include "network_tcp/internal/secure_tcp_socket.h"

#include <atomic>
#include <span>
#include <type_traits>

namespace kcenon::network::internal
{

	secure_tcp_socket::secure_tcp_socket(asio::ip::tcp::socket socket,
										 asio::ssl::context& ssl_context)
		: ssl_stream_(std::move(socket), ssl_context)
	{
	}

	auto secure_tcp_socket::async_handshake(
		asio::ssl::stream_base::handshake_type type,
		std::function<void(std::error_code)> handler) -> void
	{
		auto self = shared_from_this();
		ssl_stream_.async_handshake(
			type,
			[handler = std::move(handler), self](std::error_code ec)
			{
				if constexpr (std::is_invocable_v<decltype(handler), std::error_code>)
				{
					if (handler)
					{
						handler(ec);
					}
				}
			});
	}

	auto secure_tcp_socket::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		auto new_cb = std::make_shared<receive_callback_t>(std::move(callback));
		std::lock_guard<std::mutex> lock(callback_mutex_);
		std::atomic_store(&receive_callback_, new_cb);
	}

	auto secure_tcp_socket::set_receive_callback_view(
		std::function<void(std::span<const uint8_t>)> callback) -> void
	{
		auto new_cb = std::make_shared<receive_callback_view_t>(std::move(callback));
		std::lock_guard<std::mutex> lock(callback_mutex_);
		std::atomic_store(&receive_callback_view_, new_cb);
	}

	auto secure_tcp_socket::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		auto new_cb = std::make_shared<error_callback_t>(std::move(callback));
		std::lock_guard<std::mutex> lock(callback_mutex_);
		std::atomic_store(&error_callback_, new_cb);
	}

	auto secure_tcp_socket::start_read() -> void
	{
		// Set reading flag and kick off the initial read loop
		is_reading_.store(true);
		do_read();
	}

	auto secure_tcp_socket::stop_read() -> void
	{
		// Stop further read operations
		is_reading_.store(false);
	}

	auto secure_tcp_socket::close() -> void
	{
		// Atomically mark socket as closed before actual close
		// This prevents data races with concurrent async operations
		bool was_closed = is_closed_.exchange(true);
		if (was_closed)
		{
			// Already closed, avoid double-close
			return;
		}
		is_reading_.store(false);

		// Use dispatch to run on the same strand/thread if possible
		// The close operation cancels pending operations first, then closes
		try
		{
			auto self = shared_from_this();
			asio::dispatch(ssl_stream_.lowest_layer().get_executor(), [self]() {
				std::error_code ec;
				auto& lowest = self->ssl_stream_.lowest_layer();
				if (lowest.is_open())
				{
					// Cancel pending operations first
					lowest.cancel(ec);
					// Then close the socket
					lowest.close(ec);
				}
				// Ignore close errors - socket may already be closed
			});
		}
		catch (...)
		{
			// If dispatch fails, fall back to direct close
			std::error_code ec;
			ssl_stream_.lowest_layer().cancel(ec);
			ssl_stream_.lowest_layer().close(ec);
		}
	}

	auto secure_tcp_socket::is_closed() const -> bool
	{
		return is_closed_.load();
	}

	auto secure_tcp_socket::do_read() -> void
	{
		// Check if reading has been stopped before initiating new async operation
		if (!is_reading_.load())
		{
			return;
		}

		// Check if socket has been closed before starting async operation
		if (is_closed_.load())
		{
			is_reading_.store(false);
			return;
		}

		auto self = shared_from_this();

		// Dispatch the read operation to the socket's executor to serialize with close()
		asio::dispatch(ssl_stream_.lowest_layer().get_executor(), [this, self]() {
			// Re-check after dispatch
			if (!is_reading_.load() || is_closed_.load())
			{
				return;
			}

			if (!ssl_stream_.lowest_layer().is_open())
			{
				is_reading_.store(false);
				return;
			}

			try
			{
				ssl_stream_.async_read_some(
					asio::buffer(read_buffer_),
					[this, self](std::error_code ec, std::size_t length)
					{
						// Check if reading has been stopped or socket closed at callback time
						if (!is_reading_.load() || is_closed_.load())
						{
							return;
						}

						if (ec)
						{
							is_reading_.store(false);

							// Don't invoke error callback for operation_aborted
							if (ec == asio::error::operation_aborted)
							{
								return;
							}

							auto error_cb = std::atomic_load(&error_callback_);
							if (error_cb && *error_cb)
							{
								(*error_cb)(ec);
							}
							return;
						}

						if (length > 0)
						{
							auto view_cb = std::atomic_load(&receive_callback_view_);
							if (view_cb && *view_cb)
							{
								std::span<const uint8_t> data_view(read_buffer_.data(), length);
								(*view_cb)(data_view);
							}
							else
							{
								auto recv_cb = std::atomic_load(&receive_callback_);
								if (recv_cb && *recv_cb)
								{
									std::vector<uint8_t> chunk(read_buffer_.begin(),
															   read_buffer_.begin() + length);
									(*recv_cb)(chunk);
								}
							}
						}

						if (is_reading_.load() && !is_closed_.load())
						{
							do_read();
						}
					});
			}
			catch (const std::exception&)
			{
				is_reading_.store(false);
			}
		});
	}

	auto secure_tcp_socket::async_send(
		std::vector<uint8_t>&& data,
		std::function<void(std::error_code, std::size_t)> handler) -> void
	{
		// Check if socket has been closed before starting async operation
		if (is_closed_.load())
		{
			if (handler)
			{
				handler(asio::error::not_connected, 0);
			}
			return;
		}

		auto self = shared_from_this();
		// Move data into shared_ptr for lifetime management
		auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(data));

		// Dispatch write operation to socket's executor to serialize with close()
		asio::dispatch(ssl_stream_.lowest_layer().get_executor(),
			[this, self, buffer, handler = std::move(handler)]() mutable {
				// Re-check after dispatch
				if (is_closed_.load() || !ssl_stream_.lowest_layer().is_open())
				{
					if (handler)
					{
						handler(asio::error::not_connected, 0);
					}
					return;
				}

				try
				{
					asio::async_write(
						ssl_stream_, asio::buffer(*buffer),
						[handler = std::move(handler), self, buffer](std::error_code ec, std::size_t bytes_transferred)
						{
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
					if (handler)
					{
						handler(asio::error::not_connected, 0);
					}
				}
			});
	}

} // namespace kcenon::network::internal
