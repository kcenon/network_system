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
 * @file fuzz_http2_frame.cpp
 * @brief Fuzz target for HTTP/2 frame parsing
 *
 * Tests frame_header::parse() and frame::parse() with random input
 * to detect memory safety issues in HTTP/2 frame deserialization.
 */

#include <cstddef>
#include <cstdint>
#include <span>

#include "internal/protocols/http2/frame.h"

using namespace kcenon::network::protocols::http2;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::span<const uint8_t> input(data, size);

    // Test frame_header::parse with raw input
    auto header_result = frame_header::parse(input);
    if (header_result.is_ok())
    {
        auto& hdr = header_result.value();
        // Exercise accessors
        (void)hdr.length;
        (void)hdr.type;
        (void)hdr.flags;
        (void)hdr.stream_id;

        // Re-serialize to verify round-trip safety
        auto serialized = hdr.serialize();
        (void)serialized;
    }

    // Test full frame::parse with raw input
    auto frame_result = frame::parse(input);
    if (frame_result.is_ok())
    {
        auto& f = frame_result.value();
        // Exercise accessors
        (void)f->header();
        (void)f->payload();

        // Re-serialize to verify round-trip safety
        auto serialized = f->serialize();
        (void)serialized;
    }

    return 0;
}
