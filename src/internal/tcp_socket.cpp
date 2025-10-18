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

#include "tcp_socket.h"

#include <iostream> // for debugging/logging
#include <type_traits>

// Use nested namespace definition (C++17)
namespace network_system::internal
{

	tcp_socket::tcp_socket(asio::ip::tcp::socket socket)
		: socket_(std::move(socket))
	{
		// constructor body empty
	}

	auto tcp_socket::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		receive_callback_ = std::move(callback);
	}

	auto tcp_socket::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::move(callback);
	}

	auto tcp_socket::start_read() -> void
	{
		// Set reading flag and kick off the initial read loop
		is_reading_.store(true);
		do_read();
	}

	auto tcp_socket::stop_read() -> void
	{
		// Stop further read operations
		is_reading_.store(false);
	}

	auto tcp_socket::do_read() -> void
	{
		// Check if reading has been stopped before initiating new async operation
		if (!is_reading_.load())
		{
			return;
		}

		auto self = shared_from_this();
		socket_.async_read_some(
			asio::buffer(read_buffer_),
			[this, self](std::error_code ec, std::size_t length)
			{
				// Check if reading has been stopped at callback time
				if (!is_reading_.load())
				{
					return;
				}

				if (ec)
				{
					// On error, invoke the error callback using if constexpr to check invocability
					if constexpr (std::is_invocable_v<decltype(error_callback_), std::error_code>)
					{
						std::lock_guard<std::mutex> lock(callback_mutex_);
						if (error_callback_) {
							error_callback_(ec);
						}
					}
					return;
				}
				// On success, if length > 0, build a vector and call receive_callback_
				if (length > 0)
				{
					if constexpr (std::is_invocable_v<decltype(receive_callback_), const std::vector<uint8_t>&>)
					{
						std::vector<uint8_t> chunk(read_buffer_.begin(),
											   read_buffer_.begin() + length);
						std::lock_guard<std::mutex> lock(callback_mutex_);
						if (receive_callback_) {
							receive_callback_(chunk);
						}
					}
				}
				// Continue reading only if still active
				if (is_reading_.load())
				{
					do_read();
				}
			});
	}

auto tcp_socket::async_send(
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler) -> void
{
    auto self = shared_from_this();
    // Move data into shared_ptr for lifetime management
    auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(data));
    asio::async_write(
        socket_, asio::buffer(*buffer),
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

} // namespace network_system::internal
