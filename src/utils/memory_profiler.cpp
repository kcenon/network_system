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

#include "kcenon/network/utils/memory_profiler.h"

#include <sstream>
#include <iomanip>

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
    if (running_.exchange(true)) return;
    worker_ = std::thread(&memory_profiler::sampler_loop, this, interval);
#endif
}

void memory_profiler::stop() {
#ifdef NETWORK_ENABLE_MEMORY_PROFILER
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
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
    while (running_.load()) {
        snapshot();
        std::this_thread::sleep_for(interval);
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

