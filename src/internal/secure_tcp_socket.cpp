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

#include "kcenon/network/internal/secure_tcp_socket.h"

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
		is_closed_.store(true);
		is_reading_.store(false);

		std::error_code ec;
		ssl_stream_.lowest_layer().close(ec);
		// Ignore close errors - socket may already be closed
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

		// Check if socket has been closed or is no longer open before starting async operation
		// This prevents data races and UBSAN errors from accessing null descriptor_state
		// Both checks are needed: is_closed_ for explicit close() calls, is_open() for ASIO state
		if (is_closed_.load() || !ssl_stream_.lowest_layer().is_open())
		{
			is_reading_.store(false);
			return;
		}

		auto self = shared_from_this();
		ssl_stream_.async_read_some(
			asio::buffer(read_buffer_),
			[this, self](std::error_code ec, std::size_t length)
			{
				// Check if reading has been stopped or socket closed at callback time
				// This prevents accessing invalid socket state after close()
				if (!is_reading_.load() || is_closed_.load())
				{
					return;
				}

				if (ec)
				{
					// On error, invoke the error callback
					// Lock-free callback access via atomic_load
					auto error_cb = std::atomic_load(&error_callback_);
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
					auto view_cb = std::atomic_load(&receive_callback_view_);
					if (view_cb && *view_cb)
					{
						// Zero-copy path: create span view directly into read_buffer_
						// No std::vector allocation or copy required
						std::span<const uint8_t> data_view(read_buffer_.data(), length);
						(*view_cb)(data_view);
					}
					else
					{
						// Legacy path: allocate and copy into vector for compatibility
						auto recv_cb = std::atomic_load(&receive_callback_);
						if (recv_cb && *recv_cb)
						{
							std::vector<uint8_t> chunk(read_buffer_.begin(),
													   read_buffer_.begin() + length);
							(*recv_cb)(chunk);
						}
					}
				}

				// Continue reading only if still active and socket is not closed
				// Use atomic is_closed_ flag to prevent data race with close()
				if (is_reading_.load() && !is_closed_.load())
				{
					do_read();
				}
			});
	}

	auto secure_tcp_socket::async_send(
		std::vector<uint8_t>&& data,
		std::function<void(std::error_code, std::size_t)> handler) -> void
	{
		// Check if socket has been closed before starting async operation
		// Use atomic is_closed_ flag to prevent data race with close()
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

} // namespace kcenon::network::internal
