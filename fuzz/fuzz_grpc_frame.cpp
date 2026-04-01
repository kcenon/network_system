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
