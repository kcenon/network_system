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

#include "network_system/internal/websocket_frame.h"

#include <algorithm>
#include <random>

namespace network_system::internal
{
	namespace
	{
		// Constants from RFC 6455
		constexpr uint8_t FIN_BIT = 0x80;
		constexpr uint8_t RSV1_BIT = 0x40;
		constexpr uint8_t RSV2_BIT = 0x20;
		constexpr uint8_t RSV3_BIT = 0x10;
		constexpr uint8_t OPCODE_MASK = 0x0F;
		constexpr uint8_t MASK_BIT = 0x80;
		constexpr uint8_t PAYLOAD_LEN_MASK = 0x7F;

		constexpr uint8_t PAYLOAD_LEN_16BIT = 126;
		constexpr uint8_t PAYLOAD_LEN_64BIT = 127;

		// Minimum header sizes
		constexpr size_t MIN_HEADER_SIZE = 2;
		constexpr size_t MASKING_KEY_SIZE = 4;

		/*!
		 * \brief Converts a uint16_t to network byte order (big-endian).
		 */
		auto to_network_u16(uint16_t value) -> std::array<uint8_t, 2>
		{
			return {static_cast<uint8_t>((value >> 8) & 0xFF),
					static_cast<uint8_t>(value & 0xFF)};
		}

		/*!
		 * \brief Converts a uint64_t to network byte order (big-endian).
		 */
		auto to_network_u64(uint64_t value) -> std::array<uint8_t, 8>
		{
			return {static_cast<uint8_t>((value >> 56) & 0xFF),
					static_cast<uint8_t>((value >> 48) & 0xFF),
					static_cast<uint8_t>((value >> 40) & 0xFF),
					static_cast<uint8_t>((value >> 32) & 0xFF),
					static_cast<uint8_t>((value >> 24) & 0xFF),
					static_cast<uint8_t>((value >> 16) & 0xFF),
					static_cast<uint8_t>((value >> 8) & 0xFF),
					static_cast<uint8_t>(value & 0xFF)};
		}

		/*!
		 * \brief Converts network byte order (big-endian) to uint16_t.
		 */
		auto from_network_u16(const uint8_t* data) -> uint16_t
		{
			return static_cast<uint16_t>((data[0] << 8) | data[1]);
		}

		/*!
		 * \brief Converts network byte order (big-endian) to uint64_t.
		 */
		auto from_network_u64(const uint8_t* data) -> uint64_t
		{
			return (static_cast<uint64_t>(data[0]) << 56) |
				   (static_cast<uint64_t>(data[1]) << 48) |
				   (static_cast<uint64_t>(data[2]) << 40) |
				   (static_cast<uint64_t>(data[3]) << 32) |
				   (static_cast<uint64_t>(data[4]) << 24) |
				   (static_cast<uint64_t>(data[5]) << 16) |
				   (static_cast<uint64_t>(data[6]) << 8) | (static_cast<uint64_t>(data[7]));
		}

	} // anonymous namespace

	auto websocket_frame::calculate_header_size(uint64_t payload_len, bool mask) -> size_t
	{
		size_t header_size = MIN_HEADER_SIZE;

		if (payload_len >= 65536)
		{
			header_size += 8; // 64-bit length field
		}
		else if (payload_len >= 126)
		{
			header_size += 2; // 16-bit length field
		}

		if (mask)
		{
			header_size += MASKING_KEY_SIZE;
		}

		return header_size;
	}

	auto websocket_frame::encode_frame(ws_opcode opcode, std::vector<uint8_t>&& payload,
									   bool fin, bool mask) -> std::vector<uint8_t>
	{
		const uint64_t payload_len = payload.size();
		const size_t header_size = calculate_header_size(payload_len, mask);

		std::vector<uint8_t> frame;
		frame.reserve(header_size + payload_len);

		// Byte 0: FIN, RSV1-3, Opcode
		uint8_t byte0 = static_cast<uint8_t>(opcode);
		if (fin)
		{
			byte0 |= FIN_BIT;
		}
		frame.push_back(byte0);

		// Byte 1: Mask, Payload length
		uint8_t byte1 = 0;
		if (mask)
		{
			byte1 |= MASK_BIT;
		}

		if (payload_len < 126)
		{
			byte1 |= static_cast<uint8_t>(payload_len);
			frame.push_back(byte1);
		}
		else if (payload_len < 65536)
		{
			byte1 |= PAYLOAD_LEN_16BIT;
			frame.push_back(byte1);

			// Extended payload length (16-bit)
			auto len_bytes = to_network_u16(static_cast<uint16_t>(payload_len));
			frame.insert(frame.end(), len_bytes.begin(), len_bytes.end());
		}
		else
		{
			byte1 |= PAYLOAD_LEN_64BIT;
			frame.push_back(byte1);

			// Extended payload length (64-bit)
			auto len_bytes = to_network_u64(payload_len);
			frame.insert(frame.end(), len_bytes.begin(), len_bytes.end());
		}

		// Masking key (if mask is enabled)
		std::array<uint8_t, 4> masking_key{};
		if (mask)
		{
			masking_key = generate_mask();
			frame.insert(frame.end(), masking_key.begin(), masking_key.end());

			// Apply masking to payload
			apply_mask(payload, masking_key);
		}

		// Append payload
		frame.insert(frame.end(), payload.begin(), payload.end());

		return frame;
	}

	auto websocket_frame::decode_header(const std::vector<uint8_t>& data)
		-> std::optional<ws_frame_header>
	{
		if (data.size() < MIN_HEADER_SIZE)
		{
			return std::nullopt;
		}

		ws_frame_header header{};

		// Parse byte 0: FIN, RSV1-3, Opcode
		const uint8_t byte0 = data[0];
		header.fin = (byte0 & FIN_BIT) != 0;
		header.rsv1 = (byte0 & RSV1_BIT) != 0;
		header.rsv2 = (byte0 & RSV2_BIT) != 0;
		header.rsv3 = (byte0 & RSV3_BIT) != 0;
		header.opcode = static_cast<ws_opcode>(byte0 & OPCODE_MASK);

		// Parse byte 1: Mask, Payload length
		const uint8_t byte1 = data[1];
		header.mask = (byte1 & MASK_BIT) != 0;
		uint8_t payload_len = byte1 & PAYLOAD_LEN_MASK;

		size_t offset = MIN_HEADER_SIZE;

		// Extended payload length
		if (payload_len == PAYLOAD_LEN_16BIT)
		{
			if (data.size() < offset + 2)
			{
				return std::nullopt;
			}
			header.payload_len = from_network_u16(&data[offset]);
			offset += 2;
		}
		else if (payload_len == PAYLOAD_LEN_64BIT)
		{
			if (data.size() < offset + 8)
			{
				return std::nullopt;
			}
			header.payload_len = from_network_u64(&data[offset]);
			offset += 8;
		}
		else
		{
			header.payload_len = payload_len;
		}

		// Masking key
		if (header.mask)
		{
			if (data.size() < offset + MASKING_KEY_SIZE)
			{
				return std::nullopt;
			}
			std::copy_n(data.begin() + offset, MASKING_KEY_SIZE, header.masking_key.begin());
		}

		return header;
	}

	auto websocket_frame::decode_payload(const ws_frame_header& header,
										 const std::vector<uint8_t>& data)
		-> std::vector<uint8_t>
	{
		const size_t header_size = calculate_header_size(header.payload_len, header.mask);

		if (data.size() < header_size + header.payload_len)
		{
			return {};
		}

		std::vector<uint8_t> payload(data.begin() + header_size,
									 data.begin() + header_size + header.payload_len);

		// Unmask if necessary
		if (header.mask)
		{
			apply_mask(payload, header.masking_key);
		}

		return payload;
	}

	auto websocket_frame::apply_mask(std::vector<uint8_t>& data,
									 const std::array<uint8_t, 4>& mask) -> void
	{
		for (size_t i = 0; i < data.size(); ++i)
		{
			data[i] ^= mask[i % 4];
		}
	}

	auto websocket_frame::generate_mask() -> std::array<uint8_t, 4>
	{
		static thread_local std::random_device rd;
		static thread_local std::mt19937 gen(rd());
		static thread_local std::uniform_int_distribution<uint32_t> dis;

		const uint32_t random_value = dis(gen);
		return {static_cast<uint8_t>((random_value >> 24) & 0xFF),
				static_cast<uint8_t>((random_value >> 16) & 0xFF),
				static_cast<uint8_t>((random_value >> 8) & 0xFF),
				static_cast<uint8_t>(random_value & 0xFF)};
	}

} // namespace network_system::internal
