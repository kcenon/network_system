// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file fuzz_grpc_frame.cpp
 * @brief Fuzz target for gRPC message and timeout parsing
 *
 * Tests grpc_message::parse() and parse_timeout() with random input
 * to detect memory safety issues in gRPC protocol parsing.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "kcenon/network/detail/protocols/grpc/frame.h"

using namespace kcenon::network::protocols::grpc;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::span<const uint8_t> input(data, size);

    // Test grpc_message::parse with raw input
    auto msg_result = grpc_message::parse(input);
    if (msg_result.is_ok())
    {
        auto& msg = msg_result.value();

        // Exercise accessors
        (void)msg.compressed;
        (void)msg.empty();
        (void)msg.size();
        (void)msg.serialized_size();

        // Re-serialize to verify round-trip safety
        auto serialized = msg.serialize();
        (void)serialized;
    }

    // Test parse_timeout with input interpreted as string
    if (size > 0 && size <= 256)
    {
        std::string_view timeout_str(reinterpret_cast<const char*>(data), size);
        auto timeout_ms = parse_timeout(timeout_str);
        (void)timeout_ms;

        // Test format_timeout round-trip
        if (timeout_ms > 0)
        {
            auto formatted = format_timeout(timeout_ms);
            (void)formatted;
        }
    }

    return 0;
}
