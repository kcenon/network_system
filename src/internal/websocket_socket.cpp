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

#include "network_system/internal/websocket_socket.h"

#include "network_system/internal/tcp_socket.h"
#include "network_system/internal/websocket_handshake.h"

#include <algorithm>

namespace network_system::internal
{
	// ========================================================================
	// websocket_socket implementation
	// ========================================================================

	websocket_socket::websocket_socket(std::shared_ptr<tcp_socket> socket,
									   bool is_client)
		: tcp_socket_(std::move(socket))
		, protocol_(is_client)
	{
		// Set up WebSocket protocol callbacks
		protocol_.set_message_callback(
			[this](const ws_message& msg) { handle_protocol_message(msg); });

		protocol_.set_ping_callback([this](const std::vector<uint8_t>& payload)
									{ handle_protocol_ping(payload); });

		protocol_.set_pong_callback([this](const std::vector<uint8_t>& payload)
									{ handle_protocol_pong(payload); });

		protocol_.set_close_callback([this](ws_close_code code, const std::string& reason)
									 { handle_protocol_close(code, reason); });

		// Set up TCP socket callbacks to feed data into protocol layer
		tcp_socket_->set_receive_callback(
			[this](const std::vector<uint8_t>& data) { on_tcp_receive(data); });

		tcp_socket_->set_error_callback(
			[this](std::error_code ec) { on_tcp_error(ec); });
	}

	websocket_socket::~websocket_socket()
	{
		// Cleanup resources if needed
	}

	auto websocket_socket::async_handshake(const std::string& host,
										   const std::string& path, uint16_t port,
										   std::function<void(std::error_code)> handler)
		-> void
	{
		// Generate client handshake request
		auto handshake_request =
			websocket_handshake::create_client_handshake(host, path, port);

		// Store the expected key for validation
		auto key_start = handshake_request.find("Sec-WebSocket-Key: ");
		std::string client_key;
		if (key_start != std::string::npos)
		{
			key_start += 19; // Length of "Sec-WebSocket-Key: "
			auto key_end = handshake_request.find("\r\n", key_start);
			client_key = handshake_request.substr(key_start, key_end - key_start);
		}

		// Send handshake request
		std::vector<uint8_t> request_data(handshake_request.begin(),
										  handshake_request.end());

		tcp_socket_->async_send(
			std::move(request_data),
			[this, client_key, handler](std::error_code ec, std::size_t)
			{
				if (ec)
				{
					handler(ec);
					return;
				}

				// Set up temporary callback to receive handshake response
				tcp_socket_->set_receive_callback(
					[this, client_key, handler](const std::vector<uint8_t>& data)
					{
						// Convert response to string
						std::string response(data.begin(), data.end());

						// Validate server response
						auto result = websocket_handshake::validate_server_response(
							response, client_key);

						if (!result.success)
						{
							handler(std::make_error_code(std::errc::protocol_error));
							return;
						}

						// Handshake successful, update state
						state_ = ws_state::open;

						// Restore normal receive callback
						tcp_socket_->set_receive_callback(
							[this](const std::vector<uint8_t>& data)
							{ on_tcp_receive(data); });

						handler(std::error_code{});
					});

				// Start reading response
				tcp_socket_->start_read();
			});
	}

	auto websocket_socket::async_accept(std::function<void(std::error_code)> handler)
		-> void
	{
		// Set up temporary callback to receive handshake request
		tcp_socket_->set_receive_callback(
			[this, handler](const std::vector<uint8_t>& data)
			{
				// Convert request to string
				std::string request(data.begin(), data.end());

				// Parse client request
				auto result = websocket_handshake::parse_client_request(request);

				if (!result.success)
				{
					handler(std::make_error_code(std::errc::protocol_error));
					return;
				}

				// Extract client key
				auto it = result.headers.find("Sec-WebSocket-Key");
				if (it == result.headers.end())
				{
					handler(std::make_error_code(std::errc::protocol_error));
					return;
				}

				std::string client_key = it->second;

				// Generate server response
				auto response = websocket_handshake::create_server_response(client_key);
				std::vector<uint8_t> response_data(response.begin(), response.end());

				// Send response
				tcp_socket_->async_send(
					std::move(response_data),
					[this, handler](std::error_code ec, std::size_t)
					{
						if (ec)
						{
							handler(ec);
							return;
						}

						// Handshake successful, update state
						state_ = ws_state::open;

						// Restore normal receive callback
						tcp_socket_->set_receive_callback(
							[this](const std::vector<uint8_t>& data)
							{ on_tcp_receive(data); });

						handler(std::error_code{});
					});
			});

		// Start reading request
		tcp_socket_->start_read();
	}

	auto websocket_socket::start_read() -> void { tcp_socket_->start_read(); }

	auto websocket_socket::async_send_text(
		std::string&& message,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		if (state_ != ws_state::open)
		{
			return error_void(error_codes::network_system::connection_closed,
							  "WebSocket connection not open");
		}

		// Encode text message into WebSocket frame
		auto frame_data = protocol_.create_text_message(std::move(message));

		if (frame_data.empty())
		{
			return error_void(error_codes::common::invalid_argument,
							  "Invalid UTF-8 in text message");
		}

		// Send via underlying tcp_socket
		tcp_socket_->async_send(std::move(frame_data), std::move(handler));

		return ok();
	}

	auto websocket_socket::async_send_binary(
		std::vector<uint8_t>&& data,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		if (state_ != ws_state::open)
		{
			return error_void(error_codes::network_system::connection_closed,
							  "WebSocket connection not open");
		}

		// Encode binary message into WebSocket frame
		auto frame_data = protocol_.create_binary_message(std::move(data));

		// Send via underlying tcp_socket
		tcp_socket_->async_send(std::move(frame_data), std::move(handler));

		return ok();
	}

	auto websocket_socket::async_send_ping(
		std::vector<uint8_t> payload, std::function<void(std::error_code)> handler)
		-> void
	{
		// Encode ping frame
		auto frame_data = protocol_.create_ping(std::move(payload));

		// Send via underlying tcp_socket
		tcp_socket_->async_send(std::move(frame_data),
								[handler](std::error_code ec, std::size_t)
								{
									if (handler)
									{
										handler(ec);
									}
								});
	}

	auto websocket_socket::async_close(ws_close_code code, const std::string& reason,
									   std::function<void(std::error_code)> handler)
		-> void
	{
		// Update state to closing
		state_ = ws_state::closing;

		// Encode close frame
		auto frame_data = protocol_.create_close(code, std::string(reason));

		// Send via underlying tcp_socket
		tcp_socket_->async_send(std::move(frame_data),
								[this, handler](std::error_code ec, std::size_t)
								{
									if (ec)
									{
										state_ = ws_state::closed;
										if (handler)
										{
											handler(ec);
										}
										return;
									}

									// Close handshake initiated successfully
									if (handler)
									{
										handler(std::error_code{});
									}
								});
	}

	auto websocket_socket::state() const -> ws_state { return state_.load(); }

	auto websocket_socket::is_open() const -> bool
	{
		return state_.load() == ws_state::open;
	}

	auto websocket_socket::set_message_callback(
		std::function<void(const ws_message&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		message_callback_ = std::move(callback);
	}

	auto websocket_socket::set_ping_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		ping_callback_ = std::move(callback);
	}

	auto websocket_socket::set_pong_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		pong_callback_ = std::move(callback);
	}

	auto websocket_socket::set_close_callback(
		std::function<void(ws_close_code, const std::string&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		close_callback_ = std::move(callback);
	}

	auto websocket_socket::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::move(callback);
	}

	auto websocket_socket::on_tcp_receive(const std::vector<uint8_t>& data) -> void
	{
		// Feed raw TCP data into WebSocket protocol decoder
		protocol_.process_data(data);
	}

	auto websocket_socket::on_tcp_error(std::error_code ec) -> void
	{
		// Update state to closed
		state_ = ws_state::closed;

		// Propagate error to application
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (error_callback_)
		{
			error_callback_(ec);
		}
	}

	auto websocket_socket::handle_protocol_message(const ws_message& msg) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (message_callback_)
		{
			message_callback_(msg);
		}
	}

	auto websocket_socket::handle_protocol_ping(const std::vector<uint8_t>& payload)
		-> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (ping_callback_)
		{
			ping_callback_(payload);
		}
	}

	auto websocket_socket::handle_protocol_pong(const std::vector<uint8_t>& payload)
		-> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (pong_callback_)
		{
			pong_callback_(payload);
		}
	}

	auto websocket_socket::handle_protocol_close(ws_close_code code,
												 const std::string& reason) -> void
	{
		// Update state to closed
		state_ = ws_state::closed;

		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (close_callback_)
		{
			close_callback_(code, reason);
		}
	}

} // namespace network_system::internal
