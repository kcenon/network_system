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
 * @file thread_system_adapter.h
 * @brief Adapter that bridges thread_system::thread_pool to thread_pool_interface
 *
 * This optional adapter lets NetworkSystem use thread_system's thread_pool via the
 * existing thread_integration API, strengthening DI/scheduling consistency.
 * Enabled when BUILD_WITH_THREAD_SYSTEM is defined.
 */

#include "kcenon/network/integration/thread_integration.h"
#include <memory>
#include <future>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>

#if defined(BUILD_WITH_THREAD_SYSTEM)
// Suppress deprecation warnings from thread_system headers
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
// Include format for thread_system headers that use std::format
#  include <format>
#  include <kcenon/thread/core/thread_pool.h>
#  include <kcenon/thread/interfaces/thread_context.h>
#  include <kcenon/thread/interfaces/service_container.h>
#  pragma clang diagnostic pop
#endif

namespace kcenon::network::integration {

#if defined(BUILD_WITH_THREAD_SYSTEM)

class thread_system_pool_adapter : public thread_pool_interface {
public:
    explicit thread_system_pool_adapter(std::shared_ptr<kcenon::thread::thread_pool> pool);
    ~thread_system_pool_adapter();

    // Non-copyable, non-movable
    thread_system_pool_adapter(const thread_system_pool_adapter&) = delete;
    thread_system_pool_adapter& operator=(const thread_system_pool_adapter&) = delete;
    thread_system_pool_adapter(thread_system_pool_adapter&&) = delete;
    thread_system_pool_adapter& operator=(thread_system_pool_adapter&&) = delete;

    // thread_pool_interface
    std::future<void> submit(std::function<void()> task) override;
    std::future<void> submit_delayed(std::function<void()> task,
                                     std::chrono::milliseconds delay) override;
    size_t worker_count() const override;
    bool is_running() const override;
    size_t pending_tasks() const override;

    // Convenience helpers
    static std::shared_ptr<thread_system_pool_adapter> create_default(
        const std::string& pool_name = "network_pool");

    // Try resolving from thread_system's service_container; fallback to default
    static std::shared_ptr<thread_system_pool_adapter> from_service_or_default(
        const std::string& pool_name = "network_pool");

private:
    /**
     * @brief Internal structure for delayed tasks
     */
    struct DelayedTask {
        std::chrono::steady_clock::time_point execute_at;
        std::function<void()> task;

        bool operator>(const DelayedTask& other) const {
            return execute_at > other.execute_at;
        }
    };

    /**
     * @brief Scheduler loop that processes delayed tasks
     */
    void scheduler_loop();

    /**
     * @brief Stop the scheduler thread
     */
    void stop_scheduler();

    std::shared_ptr<kcenon::thread::thread_pool> pool_;

    // Delayed task scheduler members
    std::thread scheduler_thread_;
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<DelayedTask>> delayed_tasks_;
    mutable std::mutex scheduler_mutex_;
    std::condition_variable scheduler_condition_;
    std::atomic<bool> scheduler_running_{false};
};

// Bind a thread_system adapter into the global integration manager.
// Returns true on success.
bool bind_thread_system_pool_into_manager(const std::string& pool_name = "network_pool");

#else // BUILD_WITH_THREAD_SYSTEM

// Placeholder when thread_system is not available, to keep includes harmless
struct thread_system_pool_adapter_unavailable final {
    thread_system_pool_adapter_unavailable() = delete;
};

#endif // BUILD_WITH_THREAD_SYSTEM

} // namespace kcenon::network::integration

