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

/**
 * @file fuzz_quic_varint.cpp
 * @brief Fuzz target for QUIC variable-length integer decoding
 *
 * Tests varint::decode() with random input to detect memory safety
 * issues in RFC 9000 Section 16 variable-length integer parsing.
 */

#include <cstddef>
#include <cstdint>
#include <span>

#include "internal/protocols/quic/varint.h"

using namespace kcenon::network::protocols::quic;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::span<const uint8_t> input(data, size);

    // Test varint::decode with raw input
    auto result = varint::decode(input);
    if (result.is_ok())
    {
        auto [value, bytes_consumed] = result.value();

        // Verify encode round-trip does not crash
        auto encoded = varint::encode(value);
        (void)encoded;

        // Test encode_with_length with decoded value
        for (size_t len : {1UL, 2UL, 4UL, 8UL})
        {
            auto ewl_result = varint::encode_with_length(value, len);
            (void)ewl_result;
        }

        // Test length computations
        (void)varint::encoded_length(value);
        (void)varint::is_valid(value);

        // If there is remaining data after first varint, try decoding more
        if (bytes_consumed < size)
        {
            auto remaining = input.subspan(bytes_consumed);
            auto next = varint::decode(remaining);
            (void)next;
        }
    }

    return 0;
}
