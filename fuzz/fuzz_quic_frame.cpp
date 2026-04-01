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
 * @file fuzz_quic_frame.cpp
 * @brief Fuzz target for QUIC frame parsing
 *
 * Tests frame_parser::parse() and frame_parser::parse_all() with
 * random input to detect memory safety issues in QUIC frame
 * deserialization (RFC 9000 Section 12/19).
 */

#include <cstddef>
#include <cstdint>
#include <span>

#include "internal/protocols/quic/frame.h"

using namespace kcenon::network::protocols::quic;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::span<const uint8_t> input(data, size);

    // Test single frame parsing
    auto single_result = frame_parser::parse(input);
    if (single_result.is_ok())
    {
        auto& [parsed_frame, bytes_consumed] = single_result.value();
        (void)get_frame_type(parsed_frame);
    }

    // Test peek_type (lightweight type check without full parsing)
    auto peek_result = frame_parser::peek_type(input);
    (void)peek_result;

    // Test parsing all frames from buffer
    auto all_result = frame_parser::parse_all(input);
    if (all_result.is_ok())
    {
        for (const auto& f : all_result.value())
        {
            (void)get_frame_type(f);
        }
    }

    return 0;
}
