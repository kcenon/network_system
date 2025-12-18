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

#include "kcenon/network/internal/websocket_protocol.h"

#include <algorithm>
#include <span>

namespace network_system::internal
{
	// ========================================================================
	// ws_message implementation
	// ========================================================================

	auto ws_message::as_text() const -> std::string
	{
		return std::string(data.begin(), data.end());
	}

	auto ws_message::as_binary() const -> const std::vector<uint8_t>&
	{
		return data;
	}

	// ========================================================================
	// websocket_protocol implementation
	// ========================================================================

	websocket_protocol::websocket_protocol(bool is_client)
		: is_client_(is_client)
		, fragmented_type_(ws_opcode::continuation)
	{
	}

	auto websocket_protocol::process_data(std::span<const uint8_t> data) -> void
	{
		// Append incoming data to buffer (single copy from span view)
		buffer_.insert(buffer_.end(), data.begin(), data.end());

		// Process frames from buffer
		process_frames();
	}

	auto websocket_protocol::process_frames() -> void
	{
		while (true)
		{
			// Try to decode frame header
			auto header_opt = websocket_frame::decode_header(buffer_);
			if (!header_opt.has_value())
			{
				// Not enough data for header, wait for more
				return;
			}

			const auto& header = header_opt.value();

			// Calculate total frame size
			const size_t header_size =
				websocket_frame::calculate_header_size(header.payload_len, header.mask);
			const size_t frame_size = header_size + header.payload_len;

			// Check if we have the complete frame
			if (buffer_.size() < frame_size)
			{
				// Incomplete frame, wait for more data
				return;
			}

			// Decode payload
			auto payload = websocket_frame::decode_payload(header, buffer_);

			// Remove processed frame from buffer
			buffer_.erase(buffer_.begin(), buffer_.begin() + frame_size);

			// Dispatch frame based on opcode
			if (header.opcode == ws_opcode::text || header.opcode == ws_opcode::binary ||
				header.opcode == ws_opcode::continuation)
			{
				handle_data_frame(header, payload);
			}
			else
			{
				handle_control_frame(header, payload);
			}
		}
	}

	auto websocket_protocol::handle_data_frame(const ws_frame_header& header,
												const std::vector<uint8_t>& payload)
		-> void
	{
		// Handle fragmentation
		if (header.opcode == ws_opcode::continuation)
		{
			// Continuation frame
			if (fragmented_message_.empty())
			{
				// Protocol error: continuation without initial frame
				return;
			}

			// Append to fragmented message
			fragmented_message_.insert(fragmented_message_.end(), payload.begin(),
									   payload.end());

			if (header.fin)
			{
				// Final fragment - complete message
				ws_message msg;
				msg.type = (fragmented_type_ == ws_opcode::text) ? ws_message_type::text
																  : ws_message_type::binary;
				msg.data = std::move(fragmented_message_);

				// Validate UTF-8 for text messages
				if (msg.type == ws_message_type::text && !is_valid_utf8(msg.data))
				{
					// Invalid UTF-8, protocol error
					fragmented_message_.clear();
					return;
				}

				// Clear fragmentation state
				fragmented_message_.clear();
				fragmented_type_ = ws_opcode::continuation;

				// Invoke callback
				if (message_callback_)
				{
					message_callback_(msg);
				}
			}
		}
		else
		{
			// Initial frame (text or binary)
			if (!header.fin)
			{
				// Start of fragmented message
				fragmented_type_ = header.opcode;
				fragmented_message_ = payload;
			}
			else
			{
				// Complete message in single frame
				ws_message msg;
				msg.type = (header.opcode == ws_opcode::text) ? ws_message_type::text
															   : ws_message_type::binary;
				msg.data = payload;

				// Validate UTF-8 for text messages
				if (msg.type == ws_message_type::text && !is_valid_utf8(msg.data))
				{
					// Invalid UTF-8, protocol error
					return;
				}

				// Invoke callback
				if (message_callback_)
				{
					message_callback_(msg);
				}
			}
		}
	}

	auto websocket_protocol::handle_control_frame(const ws_frame_header& header,
												   const std::vector<uint8_t>& payload)
		-> void
	{
		// Control frames must not be fragmented
		if (!header.fin)
		{
			// Protocol error
			return;
		}

		// Control frames must have payload <= 125 bytes
		if (header.payload_len > 125)
		{
			// Protocol error
			return;
		}

		switch (header.opcode)
		{
		case ws_opcode::ping:
			handle_ping(payload);
			break;
		case ws_opcode::pong:
			handle_pong(payload);
			break;
		case ws_opcode::close:
			handle_close(payload);
			break;
		default:
			// Unknown control frame
			break;
		}
	}

	auto websocket_protocol::handle_ping(const std::vector<uint8_t>& payload) -> void
	{
		if (ping_callback_)
		{
			ping_callback_(payload);
		}
	}

	auto websocket_protocol::handle_pong(const std::vector<uint8_t>& payload) -> void
	{
		if (pong_callback_)
		{
			pong_callback_(payload);
		}
	}

	auto websocket_protocol::handle_close(const std::vector<uint8_t>& payload) -> void
	{
		ws_close_code code = ws_close_code::normal;
		std::string reason;

		if (payload.size() >= 2)
		{
			// Extract close code (network byte order)
			code = static_cast<ws_close_code>((payload[0] << 8) | payload[1]);

			// Extract reason (if present)
			if (payload.size() > 2)
			{
				reason = std::string(payload.begin() + 2, payload.end());

				// Validate UTF-8 in reason
				std::vector<uint8_t> reason_bytes(reason.begin(), reason.end());
				if (!is_valid_utf8(reason_bytes))
				{
					// Invalid UTF-8 in close reason
					code = ws_close_code::protocol_error;
					reason.clear();
				}
			}
		}

		if (close_callback_)
		{
			close_callback_(code, reason);
		}
	}

	auto websocket_protocol::create_text_message(std::string&& text)
		-> std::vector<uint8_t>
	{
		// Convert string to bytes
		std::vector<uint8_t> payload(text.begin(), text.end());

		// Validate UTF-8
		if (!is_valid_utf8(payload))
		{
			// Return empty frame on invalid UTF-8
			return {};
		}

		// Encode as WebSocket frame
		return websocket_frame::encode_frame(ws_opcode::text, std::move(payload), true,
											 is_client_);
	}

	auto websocket_protocol::create_binary_message(std::vector<uint8_t>&& data)
		-> std::vector<uint8_t>
	{
		return websocket_frame::encode_frame(ws_opcode::binary, std::move(data), true,
											 is_client_);
	}

	auto websocket_protocol::create_ping(std::vector<uint8_t>&& payload)
		-> std::vector<uint8_t>
	{
		// Ping payload must be <= 125 bytes
		if (payload.size() > 125)
		{
			payload.resize(125);
		}

		return websocket_frame::encode_frame(ws_opcode::ping, std::move(payload), true,
											 is_client_);
	}

	auto websocket_protocol::create_pong(std::vector<uint8_t>&& payload)
		-> std::vector<uint8_t>
	{
		// Pong payload must be <= 125 bytes
		if (payload.size() > 125)
		{
			payload.resize(125);
		}

		return websocket_frame::encode_frame(ws_opcode::pong, std::move(payload), true,
											 is_client_);
	}

	auto websocket_protocol::create_close(ws_close_code code, std::string&& reason)
		-> std::vector<uint8_t>
	{
		std::vector<uint8_t> payload;

		// Encode close code (network byte order)
		payload.push_back(static_cast<uint8_t>((static_cast<uint16_t>(code) >> 8) & 0xFF));
		payload.push_back(static_cast<uint8_t>(static_cast<uint16_t>(code) & 0xFF));

		// Append reason (if provided)
		if (!reason.empty())
		{
			// Validate UTF-8 in reason
			std::vector<uint8_t> reason_bytes(reason.begin(), reason.end());
			if (is_valid_utf8(reason_bytes))
			{
				payload.insert(payload.end(), reason.begin(), reason.end());
			}
		}

		// Ensure payload <= 125 bytes
		if (payload.size() > 125)
		{
			payload.resize(125);
		}

		return websocket_frame::encode_frame(ws_opcode::close, std::move(payload), true,
											 is_client_);
	}

	auto websocket_protocol::set_message_callback(
		std::function<void(const ws_message&)> callback) -> void
	{
		message_callback_ = std::move(callback);
	}

	auto websocket_protocol::set_ping_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		ping_callback_ = std::move(callback);
	}

	auto websocket_protocol::set_pong_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		pong_callback_ = std::move(callback);
	}

	auto websocket_protocol::set_close_callback(
		std::function<void(ws_close_code, const std::string&)> callback) -> void
	{
		close_callback_ = std::move(callback);
	}

	auto websocket_protocol::is_valid_utf8(const std::vector<uint8_t>& data) -> bool
	{
		size_t i = 0;
		while (i < data.size())
		{
			uint8_t byte = data[i];

			// Determine the number of bytes in this UTF-8 character
			int bytes_to_follow = 0;

			if ((byte & 0x80) == 0x00)
			{
				// 1-byte character (0xxxxxxx)
				bytes_to_follow = 0;
			}
			else if ((byte & 0xE0) == 0xC0)
			{
				// 2-byte character (110xxxxx)
				bytes_to_follow = 1;
			}
			else if ((byte & 0xF0) == 0xE0)
			{
				// 3-byte character (1110xxxx)
				bytes_to_follow = 2;
			}
			else if ((byte & 0xF8) == 0xF0)
			{
				// 4-byte character (11110xxx)
				bytes_to_follow = 3;
			}
			else
			{
				// Invalid UTF-8 start byte
				return false;
			}

			// Check if we have enough bytes
			if (i + bytes_to_follow >= data.size())
			{
				return false;
			}

			// Validate continuation bytes
			for (int j = 1; j <= bytes_to_follow; ++j)
			{
				if ((data[i + j] & 0xC0) != 0x80)
				{
					// Invalid continuation byte (should be 10xxxxxx)
					return false;
				}
			}

			i += bytes_to_follow + 1;
		}

		return true;
	}

} // namespace network_system::internal
