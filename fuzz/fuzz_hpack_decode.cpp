// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file fuzz_hpack_decode.cpp
 * @brief Fuzz target for HPACK header compression decoding
 *
 * Tests hpack_decoder::decode() with random input to detect memory
 * safety issues in HTTP/2 header compression parsing (RFC 7541).
 */

#include <cstddef>
#include <cstdint>
#include <span>

#include "internal/protocols/http2/hpack.h"

using namespace kcenon::network::protocols::http2;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::span<const uint8_t> input(data, size);

    // Test HPACK decoding with default table size
    {
        hpack_decoder decoder(4096);
        auto result = decoder.decode(input);
        if (result.is_ok())
        {
            for (const auto& header : result.value())
            {
                (void)header.name;
                (void)header.value;
                (void)header.size();
            }
        }
    }

    // Test HPACK decoding with small table size (stress eviction)
    {
        hpack_decoder decoder(256);
        auto result = decoder.decode(input);
        (void)result;
    }

    // Test static table lookups with first byte as index
    if (size > 0)
    {
        size_t index = static_cast<size_t>(data[0]) % 70;
        auto entry = static_table::get(index);
        (void)entry;
    }

    return 0;
}
