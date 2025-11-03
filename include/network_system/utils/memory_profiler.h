#pragma once

/**
 * @file memory_profiler.h
 * @brief Lightweight, cross-platform process memory profiler (optional)
 *
 * Build-time opt-in: define NETWORK_ENABLE_MEMORY_PROFILER to enable.
 *
 * Thread Pool Integration:
 * - Uses thread_pool_manager's utility pool for periodic sampling
 * - No dedicated thread creation, leverages thread pool resources
 */

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <vector>
#include <string>
#include <future>

namespace kcenon::thread
{
	class thread_pool;
}

namespace network_system::utils {

struct memory_snapshot {
    std::chrono::system_clock::time_point timestamp{};
    std::uint64_t resident_bytes{0};   // RSS
    std::uint64_t virtual_bytes{0};    // VSZ
};

/**
 * @brief Lightweight memory profiler using thread pool for sampling
 *
 * Periodically samples process memory usage (RSS and virtual memory)
 * using thread_pool_manager's utility pool instead of dedicated threads.
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Safe to call start/stop from multiple threads
 * - Snapshot history is protected by mutex
 *
 * Usage Example:
 * @code
 * auto& profiler = memory_profiler::instance();
 * profiler.start(std::chrono::seconds(1));  // Sample every second
 *
 * // ... do work ...
 *
 * auto history = profiler.get_history();
 * profiler.stop();
 * @endcode
 */
class memory_profiler {
public:
    // Singleton accessor
    static memory_profiler& instance();

    /**
     * @brief Start periodic sampling using thread pool
     *
     * Submits recurring sampling task to utility pool. No-op if already running.
     *
     * @param interval Sampling interval (default: 1000ms)
     * @throws std::runtime_error if thread pool is not initialized
     */
    void start(std::chrono::milliseconds interval = std::chrono::milliseconds{1000});

    /**
     * @brief Stop periodic sampling
     *
     * Stops the sampling task. Thread pool handles task cleanup automatically.
     */
    void stop();

    /**
     * @brief Take one snapshot immediately
     *
     * @return Current memory snapshot
     */
    memory_snapshot snapshot();

    /**
     * @brief Get recent snapshots (thread-safe copy)
     *
     * @param max_count Maximum number of snapshots to return (default: 256)
     * @return Vector of recent snapshots
     */
    std::vector<memory_snapshot> get_history(std::size_t max_count = 256) const;

    /**
     * @brief Clear stored snapshots
     */
    void clear_history();

    /**
     * @brief Export as TSV string (timestamp, rss, vms)
     *
     * @return TSV-formatted string of snapshot history
     */
    std::string to_tsv() const;

private:
    memory_profiler() = default;
    ~memory_profiler();

    memory_profiler(const memory_profiler&) = delete;
    memory_profiler& operator=(const memory_profiler&) = delete;

    /**
     * @brief Sampling loop executed in thread pool
     *
     * Recursively submits itself to utility pool until stopped.
     *
     * @param interval Sampling interval
     */
    void sampler_loop(std::chrono::milliseconds interval);

    /**
     * @brief Query OS-specific process memory statistics
     *
     * @param[out] rss Resident set size (bytes)
     * @param[out] vms Virtual memory size (bytes)
     * @return true if query succeeded
     */
    static bool query_process_memory(std::uint64_t& rss, std::uint64_t& vms);

private:
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_{};
    std::vector<memory_snapshot> history_{};
    std::size_t max_history_{4096};

    // Future for tracking sampling task (optional, for future cancellation support)
    std::shared_future<void> sampling_task_;
};

} // namespace network_system::utils

