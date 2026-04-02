// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "network_udp/internal/udp_socket.h"

#include <type_traits>

namespace kcenon::network::internal
{

	udp_socket::udp_socket(asio::ip::udp::socket socket)
		: socket_(std::move(socket))
	{
		// constructor body empty
	}

	auto udp_socket::set_receive_callback(
		std::function<void(const std::vector<uint8_t>&,
		                   const asio::ip::udp::endpoint&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		receive_callback_ = std::move(callback);
	}

	auto udp_socket::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::move(callback);
	}

	auto udp_socket::start_receive() -> void
	{
		// Set receiving flag and kick off the initial receive loop
		is_receiving_.store(true);
		do_receive();
	}

	auto udp_socket::stop_receive() -> void
	{
		// Stop further receive operations
		is_receiving_.store(false);
	}

	auto udp_socket::close() -> void
	{
		// Set closed flag first to prevent new operations
		is_closed_.store(true);
		stop_receive();

		// Close the socket
		std::error_code ec;
		socket_.close(ec);
		// Errors during close are intentionally ignored
	}

	auto udp_socket::is_closed() const -> bool
	{
		return is_closed_.load();
	}

	auto udp_socket::do_receive() -> void
	{
		// Check if receiving has been stopped before initiating new async operation
		if (!is_receiving_.load())
		{
			return;
		}

		auto self = shared_from_this();
		socket_.async_receive_from(
			asio::buffer(read_buffer_),
			sender_endpoint_,
			[this, self](std::error_code ec, std::size_t length)
			{
				// Check if receiving has been stopped at callback time
				if (!is_receiving_.load())
				{
					return;
				}

				if (ec)
				{
					// On error, invoke the error callback
					if constexpr (std::is_invocable_v<decltype(error_callback_),
					                                   std::error_code>)
					{
						std::lock_guard<std::mutex> lock(callback_mutex_);
						if (error_callback_)
						{
							error_callback_(ec);
						}
					}
					return;
				}

				// On success, if length > 0, build a vector and call receive_callback_
				if (length > 0)
				{
					if constexpr (std::is_invocable_v<decltype(receive_callback_),
					                                   const std::vector<uint8_t>&,
					                                   const asio::ip::udp::endpoint&>)
					{
						std::vector<uint8_t> datagram(read_buffer_.begin(),
						                              read_buffer_.begin() + length);
						std::lock_guard<std::mutex> lock(callback_mutex_);
						if (receive_callback_)
						{
							receive_callback_(datagram, sender_endpoint_);
						}
					}
				}

				// Continue receiving only if still active
				if (is_receiving_.load())
				{
					do_receive();
				}
			});
	}

	auto udp_socket::async_send_to(
		std::vector<uint8_t>&& data,
		const asio::ip::udp::endpoint& endpoint,
		std::function<void(std::error_code, std::size_t)> handler) -> void
	{
		auto self = shared_from_this();
		// Move data into shared_ptr for lifetime management
		auto buffer = std::make_shared<std::vector<uint8_t>>(std::move(data));
		socket_.async_send_to(
			asio::buffer(*buffer),
			endpoint,
			[handler = std::move(handler), self, buffer](std::error_code ec,
			                                              std::size_t bytes_transferred)
			{
				if constexpr (std::is_invocable_v<decltype(handler),
				                                   std::error_code, std::size_t>)
				{
					if (handler)
					{
						handler(ec, bytes_transferred);
					}
				}
			});
	}

} // namespace kcenon::network::internal
