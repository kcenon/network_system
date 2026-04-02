// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
