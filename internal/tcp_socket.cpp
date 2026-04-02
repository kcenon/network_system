// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/internal/tcp_socket.h"

#include <iostream> // for debugging/logging
#include <type_traits>

// Use nested namespace definition (C++17)
namespace kcenon::network::internal
{

	tcp_socket::tcp_socket(asio::ip::tcp::socket socket)
		: socket_(std::move(socket))
	{
		// constructor body empty
	}

	auto tcp_socket::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		receive_callback_ = std::move(callback);
	}

	auto tcp_socket::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		error_callback_ = std::move(callback);
	}

	auto tcp_socket::start_read() -> void
	{
		// Kick off the initial read loop
		do_read();
	}

	auto tcp_socket::do_read() -> void
	{
		auto self = shared_from_this();
		socket_.async_read_some(
			asio::buffer(read_buffer_),
			[this, self](std::error_code ec, std::size_t length)
			{
				if (ec)
				{
					// On error, invoke the error callback using if constexpr to check invocability
					if constexpr (std::is_invocable_v<decltype(error_callback_), std::error_code>)
					{
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
						if (receive_callback_) {
							std::vector<uint8_t> chunk(read_buffer_.begin(),
												   read_buffer_.begin() + length);
							receive_callback_(chunk);
						}
					}
				}
				// Continue reading
				do_read();
			});
	}

	auto tcp_socket::async_send(
		const std::vector<uint8_t>& data,
		std::function<void(std::error_code, std::size_t)> handler) -> void
	{
		auto self = shared_from_this();
		asio::async_write(
			socket_, asio::buffer(data),
			[handler = std::move(handler), self](std::error_code ec, std::size_t bytes_transferred)
			{
				// Using if constexpr to check if handler is invocable
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
