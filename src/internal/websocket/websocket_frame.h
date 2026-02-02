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

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace kcenon::network::internal
{
	/*!
	 * \enum ws_opcode
	 * \brief WebSocket frame operation codes as defined in RFC 6455.
	 *
	 * These opcodes indicate the type of data contained in a WebSocket frame.
	 * The values are from the WebSocket protocol specification.
	 */
	enum class ws_opcode : uint8_t
	{
		continuation = 0x0,  ///< Continuation frame
		text = 0x1,          ///< Text frame (UTF-8 encoded)
		binary = 0x2,        ///< Binary frame
		close = 0x8,         ///< Connection close frame
		ping = 0x9,          ///< Ping frame
		pong = 0xA           ///< Pong frame
	};

	/*!
	 * \struct ws_frame_header
	 * \brief Represents a decoded WebSocket frame header.
	 *
	 * This structure contains all the header fields from a WebSocket frame
	 * according to RFC 6455 section 5.2.
	 */
	struct ws_frame_header
	{
		bool fin;                        ///< Final fragment flag
		bool rsv1;                       ///< Reserved bit 1
		bool rsv2;                       ///< Reserved bit 2
		bool rsv3;                       ///< Reserved bit 3
		ws_opcode opcode;                ///< Operation code
		bool mask;                       ///< Mask flag
		uint64_t payload_len;            ///< Payload length
		std::array<uint8_t, 4> masking_key; ///< Masking key (if mask == true)
	};

	/*!
	 * \class websocket_frame
	 * \brief Provides WebSocket frame encoding and decoding functionality.
	 *
	 * This class implements RFC 6455 compliant frame encoding and decoding,
	 * including support for masking, fragmentation, and all frame types.
	 */
	class websocket_frame
	{
	public:
		/*!
		 * \brief Encodes data into a WebSocket frame.
		 *
		 * Creates a properly formatted WebSocket frame with the specified
		 * opcode and payload. Uses move semantics for zero-copy operation.
		 *
		 * \param opcode The operation code for this frame
		 * \param payload The payload data (will be moved)
		 * \param fin Final fragment flag (default: true)
		 * \param mask Whether to mask the payload (default: false)
		 * \return Encoded frame as a byte vector
		 */
		static auto encode_frame(ws_opcode opcode, std::vector<uint8_t>&& payload,
								 bool fin = true, bool mask = false)
			-> std::vector<uint8_t>;

		/*!
		 * \brief Decodes a WebSocket frame header from raw data.
		 *
		 * Parses the first bytes of a WebSocket frame to extract header
		 * information. Does not validate or extract the payload.
		 *
		 * \param data The raw frame data
		 * \return Decoded header if valid, std::nullopt if invalid or incomplete
		 */
		static auto decode_header(const std::vector<uint8_t>& data)
			-> std::optional<ws_frame_header>;

		/*!
		 * \brief Decodes the payload from a WebSocket frame.
		 *
		 * Extracts and unmasks (if necessary) the payload data from a frame
		 * using the provided header information.
		 *
		 * \param header The decoded frame header
		 * \param data The complete frame data
		 * \return Decoded (and unmasked if needed) payload
		 */
		static auto decode_payload(const ws_frame_header& header,
								   const std::vector<uint8_t>& data)
			-> std::vector<uint8_t>;

		/*!
		 * \brief Applies or removes XOR masking on data.
		 *
		 * WebSocket masking is symmetric (XOR operation), so this function
		 * can be used both to apply and remove masking.
		 *
		 * \param data The data to mask/unmask (modified in place)
		 * \param mask The 4-byte masking key
		 */
		static auto apply_mask(std::vector<uint8_t>& data,
							   const std::array<uint8_t, 4>& mask) -> void;

		/*!
		 * \brief Generates a random 4-byte masking key.
		 *
		 * Creates a cryptographically random masking key as required by
		 * RFC 6455 for client-to-server frames.
		 *
		 * \return A 4-byte random masking key
		 */
		static auto generate_mask() -> std::array<uint8_t, 4>;

		/*!
		 * \brief Calculates the size of the frame header.
		 *
		 * Determines how many bytes the header will occupy based on
		 * payload length and masking flag.
		 *
		 * \param payload_len The length of the payload
		 * \param mask Whether the frame will be masked
		 * \return The header size in bytes
		 */
		static auto calculate_header_size(uint64_t payload_len, bool mask) -> size_t;
	};

} // namespace kcenon::network::internal
