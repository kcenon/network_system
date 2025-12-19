// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file tcp_receive_bench.cpp
 * @brief TCP receive dispatch benchmarks (std::span vs std::vector)
 *
 * This benchmark quantifies the overhead difference between:
 * - span-based receive dispatch (no per-iteration allocation)
 * - legacy vector-based receive dispatch (per-iteration allocation + copy)
 *
 * Part of issue #315: TCP receive std::span callback migration
 */

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace {

/**
 * @brief Trivial callback that touches bytes to prevent dead-code elimination
 */
inline uint64_t consume_bytes(std::span<const uint8_t> data) {
    uint64_t sum = 0;
    for (size_t i = 0; i < data.size(); i += 64) {
        sum += data[i];
    }
    return sum;
}

/**
 * @brief Benchmark span-based receive dispatch (zero allocation)
 *
 * This simulates the optimized path where tcp_socket delivers received bytes
 * as std::span<const uint8_t> without allocating or copying into a temporary vector.
 */
static void BM_TcpReceive_Dispatch_Span(benchmark::State& state) {
    const size_t payload_size = static_cast<size_t>(state.range(0));

    // Pre-allocate buffer (simulates kernel/ASIO buffer)
    std::vector<uint8_t> receive_buffer(payload_size, 0x42);

    for (auto _ : state) {
        // Create span view (no allocation, just pointer + size)
        std::span<const uint8_t> span_view(receive_buffer.data(), receive_buffer.size());

        // Invoke callback with span
        auto result = consume_bytes(span_view);
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload_size));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_TcpReceive_Dispatch_Span)
    ->Arg(64)      // Small payload
    ->Arg(256)     // Typical small message
    ->Arg(1024)    // 1KB
    ->Arg(4096)    // 4KB (common page size)
    ->Arg(8192)    // 8KB
    ->Arg(16384)   // 16KB
    ->Arg(65536);  // 64KB (large payload)

/**
 * @brief Benchmark vector-based receive dispatch (per-iteration allocation)
 *
 * This simulates the legacy path where tcp_socket allocates and copies
 * received bytes into a std::vector<uint8_t> on each read operation.
 */
static void BM_TcpReceive_Dispatch_VectorFallback(benchmark::State& state) {
    const size_t payload_size = static_cast<size_t>(state.range(0));

    // Pre-allocate source buffer (simulates kernel/ASIO buffer)
    std::vector<uint8_t> receive_buffer(payload_size, 0x42);

    for (auto _ : state) {
        // Allocate vector and copy data (legacy path overhead)
        std::vector<uint8_t> copied_data(receive_buffer.begin(), receive_buffer.end());

        // Invoke callback with vector (converted to span for fair comparison)
        std::span<const uint8_t> span_view(copied_data.data(), copied_data.size());
        auto result = consume_bytes(span_view);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(copied_data.data());
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload_size));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_TcpReceive_Dispatch_VectorFallback)
    ->Arg(64)      // Small payload
    ->Arg(256)     // Typical small message
    ->Arg(1024)    // 1KB
    ->Arg(4096)    // 4KB (common page size)
    ->Arg(8192)    // 8KB
    ->Arg(16384)   // 16KB
    ->Arg(65536);  // 64KB (large payload)

/**
 * @brief Benchmark span dispatch with multiple callback invocations
 *
 * Simulates scenarios where received data is processed by multiple handlers
 * (e.g., logging, parsing, forwarding) without copying.
 */
static void BM_TcpReceive_Dispatch_Span_MultiCallback(benchmark::State& state) {
    const size_t payload_size = static_cast<size_t>(state.range(0));
    constexpr int callback_count = 3;

    std::vector<uint8_t> receive_buffer(payload_size, 0x42);

    for (auto _ : state) {
        std::span<const uint8_t> span_view(receive_buffer.data(), receive_buffer.size());

        // Multiple callbacks share the same span (zero-copy)
        uint64_t total = 0;
        for (int i = 0; i < callback_count; ++i) {
            total += consume_bytes(span_view);
        }
        benchmark::DoNotOptimize(total);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload_size) * callback_count);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * callback_count);
}
BENCHMARK(BM_TcpReceive_Dispatch_Span_MultiCallback)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384);

/**
 * @brief Benchmark vector dispatch with multiple callback invocations
 *
 * Simulates the legacy path where each handler might need its own copy.
 */
static void BM_TcpReceive_Dispatch_Vector_MultiCallback(benchmark::State& state) {
    const size_t payload_size = static_cast<size_t>(state.range(0));
    constexpr int callback_count = 3;

    std::vector<uint8_t> receive_buffer(payload_size, 0x42);

    for (auto _ : state) {
        // Each callback gets its own copy (worst-case legacy behavior)
        uint64_t total = 0;
        for (int i = 0; i < callback_count; ++i) {
            std::vector<uint8_t> copied_data(receive_buffer.begin(), receive_buffer.end());
            std::span<const uint8_t> span_view(copied_data.data(), copied_data.size());
            total += consume_bytes(span_view);
            benchmark::DoNotOptimize(copied_data.data());
        }
        benchmark::DoNotOptimize(total);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload_size) * callback_count);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * callback_count);
}
BENCHMARK(BM_TcpReceive_Dispatch_Vector_MultiCallback)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384);

/**
 * @brief Benchmark subspan operations (slicing without allocation)
 *
 * Demonstrates efficiency of span when parsing protocol headers/payloads.
 */
static void BM_TcpReceive_Subspan_Operations(benchmark::State& state) {
    const size_t payload_size = static_cast<size_t>(state.range(0));
    constexpr size_t header_size = 16;

    std::vector<uint8_t> receive_buffer(payload_size, 0x42);

    for (auto _ : state) {
        std::span<const uint8_t> full_span(receive_buffer.data(), receive_buffer.size());

        // Parse header (first 16 bytes)
        auto header = full_span.subspan(0, header_size);
        auto header_result = consume_bytes(header);

        // Parse payload (remaining bytes)
        auto payload = full_span.subspan(header_size);
        auto payload_result = consume_bytes(payload);

        benchmark::DoNotOptimize(header_result);
        benchmark::DoNotOptimize(payload_result);
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload_size));
}
BENCHMARK(BM_TcpReceive_Subspan_Operations)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096);

/**
 * @brief Benchmark vector slicing operations (requires allocation)
 *
 * Shows overhead when legacy code needs to slice data into separate vectors.
 */
static void BM_TcpReceive_VectorSlice_Operations(benchmark::State& state) {
    const size_t payload_size = static_cast<size_t>(state.range(0));
    constexpr size_t header_size = 16;

    std::vector<uint8_t> receive_buffer(payload_size, 0x42);

    for (auto _ : state) {
        // Slice into separate vectors (legacy pattern)
        std::vector<uint8_t> header(receive_buffer.begin(),
                                    receive_buffer.begin() + header_size);
        std::vector<uint8_t> payload(receive_buffer.begin() + header_size,
                                     receive_buffer.end());

        auto header_result = consume_bytes(std::span<const uint8_t>(header));
        auto payload_result = consume_bytes(std::span<const uint8_t>(payload));

        benchmark::DoNotOptimize(header_result);
        benchmark::DoNotOptimize(payload_result);
        benchmark::DoNotOptimize(header.data());
        benchmark::DoNotOptimize(payload.data());
    }

    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(payload_size));
}
BENCHMARK(BM_TcpReceive_VectorSlice_Operations)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096);

}  // namespace
