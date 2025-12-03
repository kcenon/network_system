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

#pragma once

#include "kcenon/network/utils/result_types.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace network_system::protocols::quic
{
    /*!
     * \brief Maximum value that can be encoded in QUIC variable-length integer
     *
     * This is 2^62 - 1, the maximum 62-bit unsigned integer.
     */
    constexpr uint64_t varint_max = 4611686018427387903ULL;

    /*!
     * \class varint
     * \brief QUIC variable-length integer encoding/decoding (RFC 9000 Section 16)
     *
     * QUIC uses a variable-length integer encoding with 2-bit length prefix:
     *
     * | 2-Bit Value | Length  | Usable Bits | Range                    |
     * |-------------|---------|-------------|--------------------------|
     * | 0b00        | 1 byte  | 6           | 0-63                     |
     * | 0b01        | 2 bytes | 14          | 0-16383                  |
     * | 0b10        | 4 bytes | 30          | 0-1073741823             |
     * | 0b11        | 8 bytes | 62          | 0-4611686018427387903    |
     *
     * The two most significant bits of the first byte indicate the length.
     */
    class varint
    {
    public:
        /*!
         * \brief Encode a value to variable-length format
         * \param value Value to encode (must be <= varint_max)
         * \return Encoded byte vector
         * \note Values exceeding varint_max will be clamped
         */
        [[nodiscard]] static auto encode(uint64_t value) -> std::vector<uint8_t>;

        /*!
         * \brief Encode with minimum length requirement
         * \param value Value to encode
         * \param min_length Minimum encoded length (1, 2, 4, or 8)
         * \return Result containing encoded bytes or error if value doesn't fit
         *         or min_length is invalid
         */
        [[nodiscard]] static auto encode_with_length(uint64_t value, size_t min_length)
            -> Result<std::vector<uint8_t>>;

        /*!
         * \brief Decode variable-length integer from buffer
         * \param data Input buffer
         * \return Result containing (decoded_value, bytes_consumed) or error
         */
        [[nodiscard]] static auto decode(std::span<const uint8_t> data)
            -> Result<std::pair<uint64_t, size_t>>;

        /*!
         * \brief Get the number of bytes needed to encode a value
         * \param value Value to check
         * \return Encoded length in bytes (1, 2, 4, or 8)
         */
        [[nodiscard]] static constexpr auto encoded_length(uint64_t value) noexcept -> size_t
        {
            if (value <= 63)
            {
                return 1;
            }
            if (value <= 16383)
            {
                return 2;
            }
            if (value <= 1073741823)
            {
                return 4;
            }
            return 8;
        }

        /*!
         * \brief Get encoded length from the first byte's prefix
         * \param first_byte First byte of encoded varint
         * \return Encoded length in bytes (1, 2, 4, or 8)
         */
        [[nodiscard]] static constexpr auto length_from_prefix(uint8_t first_byte) noexcept -> size_t
        {
            return size_t{1} << (first_byte >> 6);
        }

        /*!
         * \brief Check if a value can be encoded as a varint
         * \param value Value to check
         * \return true if value <= varint_max
         */
        [[nodiscard]] static constexpr auto is_valid(uint64_t value) noexcept -> bool
        {
            return value <= varint_max;
        }

        /*!
         * \brief Maximum value for each encoded length
         */
        static constexpr uint64_t max_1byte = 63;
        static constexpr uint64_t max_2byte = 16383;
        static constexpr uint64_t max_4byte = 1073741823;
        static constexpr uint64_t max_8byte = varint_max;

    private:
        static constexpr uint8_t prefix_1byte = 0x00;
        static constexpr uint8_t prefix_2byte = 0x40;
        static constexpr uint8_t prefix_4byte = 0x80;
        static constexpr uint8_t prefix_8byte = 0xC0;
        static constexpr uint8_t prefix_mask = 0xC0;
        static constexpr uint8_t value_mask = 0x3F;
    };

} // namespace network_system::protocols::quic
