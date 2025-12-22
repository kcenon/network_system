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
 * This implementation delegates to thread_system::thread_pool when KCENON_WITH_THREAD_SYSTEM
 * is defined, providing unified thread pool management across the network_system.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include <kcenon/network/config/feature_flags.h>

#include "kcenon/network/integration/thread_integration.h"
#include <mutex>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <thread>
#include <queue>
#include <condition_variable>

#if KCENON_WITH_THREAD_SYSTEM
#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/thread_worker.h>
#endif

namespace kcenon::network::integration {

#if KCENON_WITH_THREAD_SYSTEM

// basic_thread_pool implementation using thread_system::thread_pool
class basic_thread_pool::impl {
public:
    impl(size_t num_threads) : completed_tasks_(0) {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 2; // Fallback
        }

        // Intentional Leak pattern: Use no-op deleter to prevent destruction
        // during static destruction phase. This avoids heap corruption when
        // thread_pool's destructor accesses statically destroyed objects.
        // Memory impact: ~few KB (reclaimed by OS on process termination)
        auto* pool = new kcenon::thread::thread_pool("network_basic_pool");
        pool_ = std::shared_ptr<kcenon::thread::thread_pool>(
            pool,
            [](kcenon::thread::thread_pool*) { /* no-op deleter - intentional leak */ }
        );

        // Add workers to the pool
        for (size_t i = 0; i < num_threads; ++i) {
            pool_->enqueue(std::make_unique<kcenon::thread::thread_worker>());
        }

        pool_->start();
    }

    ~impl() {
        stop(true);
    }

    std::future<void> submit(std::function<void()> task) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        if (!pool_ || !pool_->is_running()) {
            promise->set_exception(
                std::make_exception_ptr(
                    std::runtime_error("Thread pool is not running")
                )
            );
            return future;
        }

        // Note: Capture completed_tasks_ by pointer to avoid capturing 'this',
        // which can cause heap corruption during static destruction.
        auto* completed_ptr = &completed_tasks_;
        bool ok = pool_->submit_task([task = std::move(task), promise, completed_ptr]() mutable {
            try {
                if (task) task();
                promise->set_value();
                completed_ptr->fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        if (!ok) {
            promise->set_exception(
                std::make_exception_ptr(
                    std::runtime_error("Failed to submit task to thread pool")
                )
            );
        }

        return future;
    }

    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) {
        // Use thread_system's submit_delayed if available (via IExecutor interface)
#if defined(THREAD_HAS_COMMON_EXECUTOR)
        return pool_->submit_delayed(std::move(task), delay);
#else
        // Fallback: submit a task that sleeps then executes
        // Note: This is not ideal but maintains API compatibility
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        if (!pool_ || !pool_->is_running()) {
            promise->set_exception(
                std::make_exception_ptr(
                    std::runtime_error("Thread pool is not running")
                )
            );
            return future;
        }

        // Note: Capture completed_tasks_ by pointer to avoid capturing 'this',
        // which can cause heap corruption during static destruction.
        auto* completed_ptr = &completed_tasks_;
        bool ok = pool_->submit_task([task = std::move(task), delay, promise, completed_ptr]() mutable {
            try {
                std::this_thread::sleep_for(delay);
                if (task) task();
                promise->set_value();
                completed_ptr->fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        if (!ok) {
            promise->set_exception(
                std::make_exception_ptr(
                    std::runtime_error("Failed to submit delayed task to thread pool")
                )
            );
        }

        return future;
#endif
    }

    size_t worker_count() const {
        return pool_ ? pool_->get_thread_count() : 0;
    }

    bool is_running() const {
        return pool_ && pool_->is_running();
    }

    size_t pending_tasks() const {
        return pool_ ? pool_->get_pending_task_count() : 0;
    }

    void stop(bool wait_for_tasks) {
        if (pool_) {
            pool_->stop(!wait_for_tasks);
        }
    }

    size_t get_completed_tasks() const {
        return completed_tasks_.load();
    }

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
    std::atomic<size_t> completed_tasks_;
};

#else // !KCENON_WITH_THREAD_SYSTEM

// Fallback implementation when thread_system is not available
// This provides a minimal thread pool using std::thread
class basic_thread_pool::impl {
    struct DelayedTask {
        std::chrono::steady_clock::time_point execute_at;
        std::function<void()> task;

        bool operator>(const DelayedTask& other) const {
            return execute_at > other.execute_at;
        }
    };

public:
    impl(size_t num_threads)
        : running_(true), completed_tasks_(0) {

        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 2; // Fallback
        }

        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }

        scheduler_thread_ = std::thread([this] { scheduler_loop(); });
    }

    ~impl() {
        stop(true);
    }

    std::future<void> submit(std::function<void()> task) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!running_) {
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
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(scheduler_mutex_);
            if (!running_) {
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Thread pool is not running")
                    )
                );
                return future;
            }

            auto execute_time = std::chrono::steady_clock::now() + delay;
            delayed_tasks_.push({execute_time, [this, task, promise]() {
                // Submit to main pool when due
                submit([task, promise]() {
                    try {
                        task();
                        promise->set_value();
                    } catch (...) {
                        promise->set_exception(std::current_exception());
                    }
                });
            }});
        }

        scheduler_condition_.notify_one();
        return future;
    }

    size_t worker_count() const {
        return workers_.size();
    }

    bool is_running() const {
        return running_.load();
    }

    size_t pending_tasks() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    void stop(bool wait_for_tasks) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!wait_for_tasks) {
                while (!tasks_.empty()) {
                    tasks_.pop();
                }
            }
            running_ = false;
        }

        {
             std::unique_lock<std::mutex> lock(scheduler_mutex_);
        }

        condition_.notify_all();
        scheduler_condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();

        if (scheduler_thread_.joinable()) {
            scheduler_thread_.join();
        }
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
                    return !running_ || !tasks_.empty();
                });

                if (!running_ && tasks_.empty()) {
                    return;
                }

                if (!tasks_.empty()) {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
            }

            if (task) {
                task();
                completed_tasks_++;
            }
        }
    }

    void scheduler_loop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(scheduler_mutex_);

            if (delayed_tasks_.empty()) {
                scheduler_condition_.wait(lock, [this] {
                    return !running_ || !delayed_tasks_.empty();
                });
            } else {
                auto now = std::chrono::steady_clock::now();
                auto& next_task = delayed_tasks_.top();

                if (now >= next_task.execute_at) {
                    auto task = next_task.task;
                    delayed_tasks_.pop();
                    lock.unlock();
                    task(); // Execute (which submits to main pool)
                    continue;
                } else {
                    scheduler_condition_.wait_until(lock, next_task.execute_at, [this, &next_task] {
                         return !running_ || delayed_tasks_.empty() || delayed_tasks_.top().execute_at < next_task.execute_at;
                    });
                }
            }
        }
    }

    std::vector<std::thread> workers_;
    std::thread scheduler_thread_;

    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;

    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<DelayedTask>> delayed_tasks_;
    std::mutex scheduler_mutex_;
    std::condition_variable scheduler_condition_;

    std::atomic<bool> running_;
    std::atomic<size_t> completed_tasks_;
};

#endif // KCENON_WITH_THREAD_SYSTEM

basic_thread_pool::basic_thread_pool(size_t num_threads)
    // Intentional Leak pattern: Use no-op deleter to prevent destruction
    // during static destruction phase. This avoids heap corruption when
    // worker threads may still access the impl's members (queue, mutex, etc.)
    // Memory impact: ~few KB (reclaimed by OS on process termination)
    : pimpl_(new impl(num_threads), [](impl*) { /* no-op deleter - intentional leak */ }) {
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
            // When thread_system is available, basic_thread_pool now internally
            // uses thread_system::thread_pool, so creating basic_thread_pool
            // automatically gets the benefits of thread_system.
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
    // Intentional Leak pattern: Use no-op deleter to prevent destruction
    // during static destruction phase. This avoids heap corruption when
    // thread pool tasks may still reference the impl's members.
    // Memory impact: ~few KB (reclaimed by OS on process termination)
    : pimpl_(new impl(), [](impl*) { /* no-op deleter - intentional leak */ }) {
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

} // namespace kcenon::network::integration