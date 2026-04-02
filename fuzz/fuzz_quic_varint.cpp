// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
