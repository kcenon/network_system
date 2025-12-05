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

#include "kcenon/network/integration/thread_system_adapter.h"

#if defined(BUILD_WITH_THREAD_SYSTEM)

// Suppress deprecation warnings from thread_system headers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <thread>
#include <stdexcept>

namespace kcenon::network::integration {

thread_system_pool_adapter::thread_system_pool_adapter(
    std::shared_ptr<kcenon::thread::thread_pool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("thread_system_pool_adapter: pool is null");
    }

    // Start the scheduler thread for delayed tasks
    scheduler_running_ = true;
    scheduler_thread_ = std::thread([this] { scheduler_loop(); });
}

thread_system_pool_adapter::~thread_system_pool_adapter() {
    stop_scheduler();
}

void thread_system_pool_adapter::stop_scheduler() {
    {
        std::unique_lock<std::mutex> lock(scheduler_mutex_);
        if (!scheduler_running_) {
            return;
        }
        scheduler_running_ = false;
    }

    scheduler_condition_.notify_all();

    if (scheduler_thread_.joinable()) {
        scheduler_thread_.join();
    }
}

void thread_system_pool_adapter::scheduler_loop() {
    while (scheduler_running_) {
        std::unique_lock<std::mutex> lock(scheduler_mutex_);

        if (delayed_tasks_.empty()) {
            scheduler_condition_.wait(lock, [this] {
                return !scheduler_running_ || !delayed_tasks_.empty();
            });
        }

        if (!scheduler_running_) {
            break;
        }

        if (!delayed_tasks_.empty()) {
            auto now = std::chrono::steady_clock::now();
            const auto& next_task = delayed_tasks_.top();

            if (now >= next_task.execute_at) {
                // Task is ready to execute
                auto task = std::move(const_cast<DelayedTask&>(next_task).task);
                delayed_tasks_.pop();
                lock.unlock();

                // Execute the task (which submits to thread pool)
                if (task) {
                    task();
                }
            } else {
                // Wait until the next task is due or a new task is added
                auto wait_time = next_task.execute_at;
                scheduler_condition_.wait_until(lock, wait_time, [this, wait_time] {
                    return !scheduler_running_ ||
                           delayed_tasks_.empty() ||
                           delayed_tasks_.top().execute_at < wait_time;
                });
            }
        }
    }
}

std::future<void> thread_system_pool_adapter::submit(std::function<void()> task) {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    bool ok = pool_->submit_task([task = std::move(task), promise]() mutable {
        try {
            if (task) task();
            promise->set_value();
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });

    if (!ok) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("thread_system_pool_adapter: submit failed")));
    }
    return future;
}

std::future<void> thread_system_pool_adapter::submit_delayed(
    std::function<void()> task,
    std::chrono::milliseconds delay
) {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    {
        std::unique_lock<std::mutex> lock(scheduler_mutex_);
        if (!scheduler_running_) {
            promise->set_exception(std::make_exception_ptr(
                std::runtime_error("thread_system_pool_adapter: scheduler is not running")));
            return future;
        }

        auto execute_time = std::chrono::steady_clock::now() + delay;

        // Capture pool_ by value (shared_ptr copy) to ensure it stays alive
        auto pool = pool_;
        delayed_tasks_.push({execute_time, [pool, task = std::move(task), promise]() mutable {
            // Submit to thread pool when the delay expires
            bool ok = pool->submit_task([task = std::move(task), promise]() mutable {
                try {
                    if (task) task();
                    promise->set_value();
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });

            if (!ok) {
                promise->set_exception(std::make_exception_ptr(
                    std::runtime_error("thread_system_pool_adapter: delayed submit failed")));
            }
        }});
    }

    scheduler_condition_.notify_one();
    return future;
}

size_t thread_system_pool_adapter::worker_count() const {
    return pool_->get_thread_count();
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

#endif // BUILD_WITH_THREAD_SYSTEM

