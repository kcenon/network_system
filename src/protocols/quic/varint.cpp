/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "internal/protocols/quic/varint.h"

namespace kcenon::network::protocols::quic
{

auto varint::encode(uint64_t value) -> std::vector<uint8_t>
{
    if (value <= max_1byte)
    {
        // 6-bit: prefix 0b00
        return {static_cast<uint8_t>(value)};
    }

    if (value <= max_2byte)
    {
        // 14-bit: prefix 0b01
        return {
            static_cast<uint8_t>(prefix_2byte | ((value >> 8) & value_mask)),
            static_cast<uint8_t>(value & 0xFF)
        };
    }

    if (value <= max_4byte)
    {
        // 30-bit: prefix 0b10
        return {
            static_cast<uint8_t>(prefix_4byte | ((value >> 24) & value_mask)),
            static_cast<uint8_t>((value >> 16) & 0xFF),
            static_cast<uint8_t>((value >> 8) & 0xFF),
            static_cast<uint8_t>(value & 0xFF)
        };
    }

    // 62-bit: prefix 0b11
    // Clamp to max if exceeding varint_max
    if (value > varint_max)
    {
        value = varint_max;
    }

    return {
        static_cast<uint8_t>(prefix_8byte | ((value >> 56) & value_mask)),
        static_cast<uint8_t>((value >> 48) & 0xFF),
        static_cast<uint8_t>((value >> 40) & 0xFF),
        static_cast<uint8_t>((value >> 32) & 0xFF),
        static_cast<uint8_t>((value >> 24) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF)
    };
}

auto varint::encode_with_length(uint64_t value, size_t min_length)
    -> Result<std::vector<uint8_t>>
{
    // Validate min_length
    if (min_length != 1 && min_length != 2 && min_length != 4 && min_length != 8)
    {
        return error<std::vector<uint8_t>>(
            error_codes::common_errors::invalid_argument,
            "Invalid minimum length",
            "quic::varint",
            "min_length must be 1, 2, 4, or 8"
        );
    }

    // Check if value fits in the requested length
    uint64_t max_for_length = 0;
    switch (min_length)
    {
        case 1: max_for_length = max_1byte; break;
        case 2: max_for_length = max_2byte; break;
        case 4: max_for_length = max_4byte; break;
        case 8: max_for_length = max_8byte; break;
        default: break;
    }

    // Find the actual minimum length needed
    size_t actual_length = encoded_length(value);

    // Use the larger of the two
    size_t use_length = (min_length > actual_length) ? min_length : actual_length;

    // If the requested min_length is smaller than needed, the value won't fit
    // But we use the larger length, so this is fine

    // Check if value exceeds maximum encodable
    if (value > varint_max)
    {
        return error<std::vector<uint8_t>>(
            error_codes::common_errors::invalid_argument,
            "Value exceeds maximum",
            "quic::varint",
            "Value exceeds 2^62 - 1"
        );
    }

    // Encode with the determined length
    std::vector<uint8_t> result;
    switch (use_length)
    {
        case 1:
            result = {static_cast<uint8_t>(value)};
            break;

        case 2:
            result = {
                static_cast<uint8_t>(prefix_2byte | ((value >> 8) & value_mask)),
                static_cast<uint8_t>(value & 0xFF)
            };
            break;

        case 4:
            result = {
                static_cast<uint8_t>(prefix_4byte | ((value >> 24) & value_mask)),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
            break;

        case 8:
            result = {
                static_cast<uint8_t>(prefix_8byte | ((value >> 56) & value_mask)),
                static_cast<uint8_t>((value >> 48) & 0xFF),
                static_cast<uint8_t>((value >> 40) & 0xFF),
                static_cast<uint8_t>((value >> 32) & 0xFF),
                static_cast<uint8_t>((value >> 24) & 0xFF),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
            break;

        default:
            break;
    }

    return ok(std::move(result));
}

auto varint::decode(std::span<const uint8_t> data)
    -> Result<std::pair<uint64_t, size_t>>
{
    if (data.empty())
    {
        return error<std::pair<uint64_t, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Empty buffer",
            "quic::varint",
            "Cannot decode from empty buffer"
        );
    }

    const uint8_t first_byte = data[0];
    const size_t length = length_from_prefix(first_byte);

    if (data.size() < length)
    {
        return error<std::pair<uint64_t, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data",
            "quic::varint",
            "Buffer too small for encoded length"
        );
    }

    uint64_t value = first_byte & value_mask;

    for (size_t i = 1; i < length; ++i)
    {
        value = (value << 8) | data[i];
    }

    return ok(std::make_pair(value, length));
}

} // namespace kcenon::network::protocols::quic
