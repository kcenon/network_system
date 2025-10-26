/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
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

#pragma once

#include <cstddef>

namespace network_system::integration_tests {

/**
 * @struct MemoryMetrics
 * @brief Memory usage metrics for performance testing
 */
struct MemoryMetrics {
    size_t rss_bytes{0};    // Resident Set Size (physical memory)
    size_t heap_bytes{0};   // Heap allocation
    size_t vm_bytes{0};     // Virtual memory size

    /**
     * @brief Check if metrics are valid (non-zero RSS)
     */
    [[nodiscard]] auto is_valid() const -> bool {
        return rss_bytes > 0;
    }

    /**
     * @brief Convert RSS to megabytes
     */
    [[nodiscard]] auto rss_mb() const -> double {
        return static_cast<double>(rss_bytes) / (1024.0 * 1024.0);
    }

    /**
     * @brief Convert heap to megabytes
     */
    [[nodiscard]] auto heap_mb() const -> double {
        return static_cast<double>(heap_bytes) / (1024.0 * 1024.0);
    }

    /**
     * @brief Convert VM to megabytes
     */
    [[nodiscard]] auto vm_mb() const -> double {
        return static_cast<double>(vm_bytes) / (1024.0 * 1024.0);
    }
};

/**
 * @class MemoryProfiler
 * @brief Cross-platform memory profiling utility
 *
 * Provides platform-specific memory measurement capabilities for
 * performance testing. Measures RSS (physical memory), heap usage,
 * and virtual memory size.
 *
 * Usage:
 * @code
 * MemoryProfiler profiler;
 * auto before = profiler.snapshot();
 * // ... perform operations ...
 * auto after = profiler.snapshot();
 * auto delta = profiler.delta(before, after);
 * std::cout << "Memory growth: " << delta.rss_mb() << " MB\n";
 * @endcode
 */
class MemoryProfiler {
public:
    /**
     * @brief Take a snapshot of current memory usage
     * @return MemoryMetrics containing current memory state
     */
    [[nodiscard]] auto snapshot() const -> MemoryMetrics;

    /**
     * @brief Calculate memory delta between two snapshots
     * @param before Earlier snapshot
     * @param after Later snapshot
     * @return MemoryMetrics showing the difference
     */
    [[nodiscard]] static auto delta(const MemoryMetrics& before,
                                     const MemoryMetrics& after) -> MemoryMetrics {
        MemoryMetrics result;
        result.rss_bytes = (after.rss_bytes > before.rss_bytes)
                          ? (after.rss_bytes - before.rss_bytes) : 0;
        result.heap_bytes = (after.heap_bytes > before.heap_bytes)
                           ? (after.heap_bytes - before.heap_bytes) : 0;
        result.vm_bytes = (after.vm_bytes > before.vm_bytes)
                         ? (after.vm_bytes - before.vm_bytes) : 0;
        return result;
    }

private:
#if defined(__linux__)
    /**
     * @brief Linux-specific implementation using /proc/self/status
     */
    [[nodiscard]] auto snapshot_linux() const -> MemoryMetrics;
#elif defined(__APPLE__)
    /**
     * @brief macOS-specific implementation using task_info()
     */
    [[nodiscard]] auto snapshot_macos() const -> MemoryMetrics;
#elif defined(_WIN32)
    /**
     * @brief Windows-specific implementation using GetProcessMemoryInfo()
     */
    [[nodiscard]] auto snapshot_windows() const -> MemoryMetrics;
#endif
};

} // namespace network_system::integration_tests
