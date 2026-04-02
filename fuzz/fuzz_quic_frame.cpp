// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
