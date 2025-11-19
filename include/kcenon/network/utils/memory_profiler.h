// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
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

#pragma once

/**
 * @file memory_profiler.h
 * @brief Lightweight, cross-platform process memory profiler (optional)
 *
 * Build-time opt-in: define NETWORK_ENABLE_MEMORY_PROFILER to enable.
 */

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>
#include <mutex>
#include <vector>
#include <string>

namespace network_system::utils {

struct memory_snapshot {
    std::chrono::system_clock::time_point timestamp{};
    std::uint64_t resident_bytes{0};   // RSS
    std::uint64_t virtual_bytes{0};    // VSZ
};

class memory_profiler {
public:
    // Singleton accessor
    static memory_profiler& instance();

    // Start periodic sampling (no-op if already running)
    void start(std::chrono::milliseconds interval = std::chrono::milliseconds{1000});

    // Stop periodic sampling
    void stop();

    // Take one snapshot immediately
    memory_snapshot snapshot();

    // Get recent snapshots (copy)
    std::vector<memory_snapshot> get_history(std::size_t max_count = 256) const;

    // Clear stored snapshots
    void clear_history();

    // Export as TSV string (timestamp, rss, vms)
    std::string to_tsv() const;

private:
    memory_profiler() = default;
    ~memory_profiler();

    memory_profiler(const memory_profiler&) = delete;
    memory_profiler& operator=(const memory_profiler&) = delete;

    void sampler_loop(std::chrono::milliseconds interval);
    static bool query_process_memory(std::uint64_t& rss, std::uint64_t& vms);

private:
    std::atomic<bool> running_{false};
    std::thread worker_{};
    mutable std::mutex mutex_{};
    std::vector<memory_snapshot> history_{};
    std::size_t max_history_{4096};
};

} // namespace network_system::utils

