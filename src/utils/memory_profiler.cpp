#include "network_system/utils/memory_profiler.h"
#include "network_system/integration/thread_pool_manager.h"

#include <sstream>
#include <iomanip>
#include <stdexcept>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <cstdio>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace network_system::utils {

memory_profiler& memory_profiler::instance() {
    static memory_profiler s;
    return s;
}

memory_profiler::~memory_profiler() { stop(); }

void memory_profiler::start(std::chrono::milliseconds interval) {
#ifndef NETWORK_ENABLE_MEMORY_PROFILER
    (void)interval;
    return; // profiling disabled
#else
    if (running_.exchange(true)) {
        return; // already running
    }

    try {
        // Get utility pool from thread pool manager
        auto& mgr = integration::thread_pool_manager::instance();
        if (!mgr.is_initialized()) {
            running_ = false;
            throw std::runtime_error("[memory_profiler] thread_pool_manager not initialized");
        }

        auto utility_pool = mgr.get_utility_pool();
        if (!utility_pool) {
            running_ = false;
            throw std::runtime_error("[memory_profiler] Failed to get utility pool");
        }

        // Submit initial sampling task to utility pool
        auto future = utility_pool->submit([this, interval]() {
            sampler_loop(interval);
        });

        // Store future for potential cancellation support
        sampling_task_ = future.share();
    }
    catch (const std::exception& e) {
        running_ = false;
        throw std::runtime_error(std::string("[memory_profiler] Start failed: ") + e.what());
    }
#endif
}

void memory_profiler::stop() {
#ifdef NETWORK_ENABLE_MEMORY_PROFILER
    if (!running_.exchange(false)) {
        return; // not running
    }

    // Wait for sampling task to complete (with timeout)
    if (sampling_task_.valid()) {
        auto status = sampling_task_.wait_for(std::chrono::seconds(2));
        if (status == std::future_status::timeout) {
            // Task didn't complete in time, but that's okay
            // Thread pool will clean up eventually
        }
    }
#endif
}

memory_snapshot memory_profiler::snapshot() {
    memory_snapshot snap;
    snap.timestamp = std::chrono::system_clock::now();
    std::uint64_t rss=0, vms=0;
    query_process_memory(rss, vms);
    snap.resident_bytes = rss;
    snap.virtual_bytes = vms;

    std::lock_guard<std::mutex> lock(mutex_);
    history_.push_back(snap);
    if (history_.size() > max_history_) {
        history_.erase(history_.begin(), history_.begin() + (history_.size() - max_history_));
    }
    return snap;
}

std::vector<memory_snapshot> memory_profiler::get_history(std::size_t max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (max_count >= history_.size()) return history_;
    return std::vector<memory_snapshot>(history_.end() - max_count, history_.end());
}

void memory_profiler::clear_history() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

std::string memory_profiler::to_tsv() const {
    std::ostringstream oss;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : history_) {
        auto t = std::chrono::system_clock::to_time_t(s.timestamp);
        oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
            << '\t' << s.resident_bytes
            << '\t' << s.virtual_bytes
            << '\n';
    }
    return oss.str();
}

void memory_profiler::sampler_loop(std::chrono::milliseconds interval) {
    // Take snapshot
    snapshot();

    // Check if should continue
    if (!running_.load()) {
        return; // stopped
    }

    // Sleep before next sample
    std::this_thread::sleep_for(interval);

    // Check again after sleep
    if (!running_.load()) {
        return; // stopped during sleep
    }

    // Recursively submit next sampling task to utility pool
    try {
        auto& mgr = integration::thread_pool_manager::instance();
        auto utility_pool = mgr.get_utility_pool();
        if (utility_pool) {
            auto future = utility_pool->submit([this, interval]() {
                sampler_loop(interval);
            });
            sampling_task_ = future.share();
        }
    }
    catch (const std::exception&) {
        // Failed to submit next task, stop sampling
        running_ = false;
    }
}

bool memory_profiler::query_process_memory(std::uint64_t& rss, std::uint64_t& vms) {
#if defined(__APPLE__)
    task_basic_info_data_t info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count) != KERN_SUCCESS) {
        return false;
    }
    rss = static_cast<std::uint64_t>(info.resident_size);
    vms = static_cast<std::uint64_t>(info.virtual_size);
    return true;
#elif defined(__linux__)
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return false;
    long pages_res=0, pages_total=0;
    int rc = std::fscanf(f, "%ld %ld", &pages_total, &pages_res);
    std::fclose(f);
    if (rc != 2) return false;
    long page_size = sysconf(_SC_PAGESIZE);
    vms = static_cast<std::uint64_t>(pages_total) * static_cast<std::uint64_t>(page_size);
    rss = static_cast<std::uint64_t>(pages_res) * static_cast<std::uint64_t>(page_size);
    return true;
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        rss = static_cast<std::uint64_t>(pmc.WorkingSetSize);
        vms = static_cast<std::uint64_t>(pmc.PrivateUsage);
        return true;
    }
    return false;
#else
    // Unknown platform
    rss = vms = 0;
    return false;
#endif
}

} // namespace network_system::utils

