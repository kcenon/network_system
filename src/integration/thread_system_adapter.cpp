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

/**
 * @file thread_system_adapter.cpp
 * @brief Implementation of thread_system_pool_adapter
 *
 * This adapter bridges thread_system::thread_pool to the network_system's
 * thread_pool_interface. It now delegates delayed task scheduling directly to
 * thread_pool::submit_delayed (when THREAD_HAS_COMMON_EXECUTOR is available),
 * eliminating the need for a separate scheduler thread.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include <kcenon/network/config/feature_flags.h>

#include "internal/integration/thread_system_adapter.h"

#if KCENON_WITH_THREAD_SYSTEM

// Suppress deprecation warnings from thread_system headers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <stdexcept>
#include <thread>  // For std::thread::hardware_concurrency and std::this_thread::sleep_for (fallback)

#include <kcenon/thread/core/thread_worker.h>

namespace kcenon::network::integration {

thread_system_pool_adapter::thread_system_pool_adapter(
    std::shared_ptr<kcenon::thread::thread_pool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("thread_system_pool_adapter: pool is null");
    }
    // No scheduler thread needed - delayed tasks are handled by thread_pool::submit_delayed
}

thread_system_pool_adapter::~thread_system_pool_adapter() {
    // No scheduler thread to stop - cleanup handled by thread_pool
}

std::future<void> thread_system_pool_adapter::submit(std::function<void()> task) {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // Use submit which returns a future and throws on failure
    try {
        pool_->submit([task = std::move(task), promise]() mutable {
            try {
                if (task) task();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    } catch (const std::exception& e) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error(
                std::string("thread_system_pool_adapter: submit failed: ") + e.what()
            )));
    }

    return future;
}

std::future<void> thread_system_pool_adapter::submit_delayed(
    std::function<void()> task,
    std::chrono::milliseconds delay
) {
#if defined(THREAD_HAS_COMMON_EXECUTOR)
    // Delegate directly to thread_pool::submit_delayed when IExecutor is available
    // This eliminates the need for a separate scheduler thread
    return pool_->submit_delayed(std::move(task), delay);
#else
    // Fallback: submit a task that sleeps then executes
    // Note: This blocks a worker thread during the delay period
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    if (!pool_->is_running()) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("thread_system_pool_adapter: pool is not running")));
        return future;
    }

    // Use submit which returns a future and throws on failure
    try {
        pool_->submit([task = std::move(task), delay, promise]() mutable {
            try {
                std::this_thread::sleep_for(delay);
                if (task) task();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
    } catch (const std::exception& e) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error(
                std::string("thread_system_pool_adapter: delayed submit failed: ") + e.what()
            )));
    }

    return future;
#endif
}

size_t thread_system_pool_adapter::worker_count() const {
    return pool_->get_active_worker_count();
}

bool thread_system_pool_adapter::is_running() const {
    return pool_->is_running();
}

size_t thread_system_pool_adapter::pending_tasks() const {
    return pool_->get_pending_task_count();
}

std::shared_ptr<thread_system_pool_adapter> thread_system_pool_adapter::create_default(
    const std::string& pool_name
) {
    kcenon::thread::thread_context ctx; // default resolves logger/monitoring if registered
    auto pool = std::make_shared<kcenon::thread::thread_pool>(pool_name, ctx);

    // Add default workers based on hardware concurrency
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) {
        num_threads = 2; // Fallback
    }

    for (size_t i = 0; i < num_threads; ++i) {
        pool->enqueue(std::make_unique<kcenon::thread::thread_worker>());
    }

    (void)pool->start(); // best-effort start; ignore error to keep adapter usable
    return std::make_shared<thread_system_pool_adapter>(std::move(pool));
}

std::shared_ptr<thread_system_pool_adapter> thread_system_pool_adapter::from_service_or_default(
    const std::string& pool_name
) {
    try {
        auto& sc = kcenon::thread::service_container::global();
        if (auto existing = sc.resolve<kcenon::thread::thread_pool>()) {
            return std::make_shared<thread_system_pool_adapter>(std::move(existing));
        }
    } catch (...) {
        // ignore and fallback
    }
    return create_default(pool_name);
}

bool bind_thread_system_pool_into_manager(const std::string& pool_name) {
    try {
        auto adapter = thread_system_pool_adapter::from_service_or_default(pool_name);
        if (!adapter) return false;
        thread_integration_manager::instance().set_thread_pool(adapter);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace kcenon::network::integration

#pragma clang diagnostic pop

#endif // KCENON_WITH_THREAD_SYSTEM

