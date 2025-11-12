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
 * @file thread_integration.cpp
 * @brief Implementation of thread system integration
 *
 * @author kcenon
 * @date 2025-09-20

 */

#include "network_system/integration/thread_integration.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>

namespace network_system::integration {

// basic_thread_pool implementation
class basic_thread_pool::impl {
public:
    impl(size_t num_threads)
        : running_(true), stopped_(false), completed_tasks_(0) {

        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 2; // Fallback
        }

        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~impl() {
        stop(true);
    }

    std::future<void> submit(std::function<void()> task) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Check if pool has been explicitly stopped
            // Use memory_order_acquire to ensure we see the stopped state set by stop()
            if (stopped_.load(std::memory_order_acquire)) {
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Thread pool is stopped")
                    )
                );
                return future;
            }

            if (!running_.load(std::memory_order_acquire)) {
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Thread pool is not running")
                    )
                );
                return future;
            }

            tasks_.emplace([task, promise]() {
                try {
                    task();
                    promise->set_value();
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }

        condition_.notify_one();
        return future;
    }

    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) {
        return submit([task, delay]() {
            std::this_thread::sleep_for(delay);
            task();
        });
    }

    size_t worker_count() const {
        return workers_.size();
    }

    bool is_running() const {
        // Use memory_order_acquire to ensure we see the latest state
        // set by stop() with release semantics
        return running_.load(std::memory_order_acquire);
    }

    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    void stop(bool wait_for_tasks) {
        // Use compare_exchange_strong to atomically check and set state
        // This prevents TOCTOU (Time-Of-Check-Time-Of-Use) race conditions
        // where multiple threads might call stop() simultaneously
        bool expected = false;
        if (!stopped_.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire)) {
            // Pool is already stopped or being stopped by another thread
            return;
        }

        // At this point, we've atomically transitioned to stopped state
        // and only this thread will execute the shutdown sequence

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!wait_for_tasks) {
                while (!tasks_.empty()) {
                    tasks_.pop();
                }
            }
            // Use memory_order_release to ensure all previous writes are visible
            // to threads that check running_ with acquire semantics
            running_.store(false, std::memory_order_release);
        }

        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers_.clear();
    }

    size_t get_completed_tasks() const {
        return completed_tasks_.load();
    }

private:
    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this] {
                    // Use memory_order_acquire to see latest state from stop()
                    return !running_.load(std::memory_order_acquire) || !tasks_.empty();
                });

                // Use memory_order_acquire for consistent state checks
                if (!running_.load(std::memory_order_acquire) && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }

            if (task) {
                task();
                // Use memory_order_relaxed for simple counter increment
                completed_tasks_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> running_;
    std::atomic<bool> stopped_;  // Explicit stopped flag for TOCTOU prevention
    std::atomic<size_t> completed_tasks_;
};

basic_thread_pool::basic_thread_pool(size_t num_threads)
    : pimpl_(std::make_unique<impl>(num_threads)) {
}

basic_thread_pool::~basic_thread_pool() = default;

std::future<void> basic_thread_pool::submit(std::function<void()> task) {
    return pimpl_->submit(task);
}

std::future<void> basic_thread_pool::submit_delayed(
    std::function<void()> task,
    std::chrono::milliseconds delay
) {
    return pimpl_->submit_delayed(task, delay);
}

size_t basic_thread_pool::worker_count() const {
    return pimpl_->worker_count();
}

bool basic_thread_pool::is_running() const {
    return pimpl_->is_running();
}

size_t basic_thread_pool::pending_tasks() const {
    return pimpl_->pending_tasks();
}

void basic_thread_pool::stop(bool wait_for_tasks) {
    pimpl_->stop(wait_for_tasks);
}

size_t basic_thread_pool::completed_tasks() const {
    return pimpl_->get_completed_tasks();
}

// thread_integration_manager implementation
class thread_integration_manager::impl {
public:
    impl() = default;

    void set_thread_pool(std::shared_ptr<thread_pool_interface> pool) {
        std::unique_lock<std::mutex> lock(mutex_);
        thread_pool_ = pool;
    }

    std::shared_ptr<thread_pool_interface> get_thread_pool() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!thread_pool_) {
            thread_pool_ = std::make_shared<basic_thread_pool>();
        }
        return thread_pool_;
    }

    std::future<void> submit_task(std::function<void()> task) {
        return get_thread_pool()->submit(task);
    }

    std::future<void> submit_delayed_task(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) {
        return get_thread_pool()->submit_delayed(task, delay);
    }

    thread_integration_manager::metrics get_metrics() const {
        std::unique_lock<std::mutex> lock(mutex_);

        metrics m;
        if (thread_pool_) {
            m.worker_threads = thread_pool_->worker_count();
            m.pending_tasks = thread_pool_->pending_tasks();
            m.is_running = thread_pool_->is_running();

            if (auto* basic = dynamic_cast<basic_thread_pool*>(thread_pool_.get())) {
                m.completed_tasks = basic->completed_tasks();
            }
        }

        return m;
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<thread_pool_interface> thread_pool_;
};

thread_integration_manager& thread_integration_manager::instance() {
    static thread_integration_manager instance;
    return instance;
}

thread_integration_manager::thread_integration_manager()
    : pimpl_(std::make_unique<impl>()) {
}

thread_integration_manager::~thread_integration_manager() = default;

void thread_integration_manager::set_thread_pool(
    std::shared_ptr<thread_pool_interface> pool
) {
    pimpl_->set_thread_pool(pool);
}

std::shared_ptr<thread_pool_interface> thread_integration_manager::get_thread_pool() {
    return pimpl_->get_thread_pool();
}

std::future<void> thread_integration_manager::submit_task(
    std::function<void()> task
) {
    return pimpl_->submit_task(task);
}

std::future<void> thread_integration_manager::submit_delayed_task(
    std::function<void()> task,
    std::chrono::milliseconds delay
) {
    return pimpl_->submit_delayed_task(task, delay);
}

thread_integration_manager::metrics thread_integration_manager::get_metrics() const {
    return pimpl_->get_metrics();
}

} // namespace network_system::integration