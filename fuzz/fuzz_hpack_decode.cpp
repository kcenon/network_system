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
