/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "memory_profiler.h"

#if defined(__linux__)
#include <fstream>
#include <sstream>
#include <string>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace network_system::integration_tests {

auto MemoryProfiler::snapshot() const -> MemoryMetrics {
#if defined(__linux__)
    return snapshot_linux();
#elif defined(__APPLE__)
    return snapshot_macos();
#elif defined(_WIN32)
    return snapshot_windows();
#else
    // Unsupported platform - return empty metrics
    return MemoryMetrics{};
#endif
}

#if defined(__linux__)
auto MemoryProfiler::snapshot_linux() const -> MemoryMetrics {
    MemoryMetrics metrics;

    std::ifstream status_file("/proc/self/status");
    if (!status_file.is_open()) {
        return metrics;
    }

    std::string line;
    while (std::getline(status_file, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "VmRSS:") {
            // Resident Set Size (physical memory in KB)
            size_t value_kb;
            iss >> value_kb;
            metrics.rss_bytes = value_kb * 1024;
        } else if (key == "VmSize:") {
            // Virtual memory size (in KB)
            size_t value_kb;
            iss >> value_kb;
            metrics.vm_bytes = value_kb * 1024;
        } else if (key == "VmData:") {
            // Heap (data segment) size (in KB)
            size_t value_kb;
            iss >> value_kb;
            metrics.heap_bytes = value_kb * 1024;
        }
    }

    return metrics;
}
#endif

#if defined(__APPLE__)
auto MemoryProfiler::snapshot_macos() const -> MemoryMetrics {
    MemoryMetrics metrics;

    task_basic_info_64_data_t info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_64_COUNT;

    kern_return_t result = task_info(
        mach_task_self(),
        TASK_BASIC_INFO_64,
        reinterpret_cast<task_info_t>(&info),
        &count
    );

    if (result == KERN_SUCCESS) {
        metrics.rss_bytes = info.resident_size;
        metrics.vm_bytes = info.virtual_size;
        // Note: macOS doesn't provide separate heap metrics easily
        metrics.heap_bytes = 0;
    }

    return metrics;
}
#endif

#if defined(_WIN32)
auto MemoryProfiler::snapshot_windows() const -> MemoryMetrics {
    MemoryMetrics metrics;

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
            sizeof(pmc))) {
        metrics.rss_bytes = pmc.WorkingSetSize;
        metrics.vm_bytes = pmc.PrivateUsage;
        // Windows doesn't separate heap easily
        metrics.heap_bytes = 0;
    }

    return metrics;
}
#endif

} // namespace network_system::integration_tests
