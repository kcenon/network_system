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

