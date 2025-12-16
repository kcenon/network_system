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
 * @file thread_integration.h
 * @brief Thread system integration interface for network_system
 *
 * This interface provides integration with thread_system for asynchronous
 * task scheduling and thread pool management.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include <functional>
#include <memory>
#include <future>
#include <chrono>
#include <vector>

namespace kcenon::network::integration {

/**
 * @class thread_pool_interface
 * @brief Abstract interface for thread pool integration
 *
 * This interface allows network_system to work with any thread pool
 * implementation, including the future thread_system module.
 */
class thread_pool_interface {
public:
    virtual ~thread_pool_interface() = default;

    /**
     * @brief Submit a task to the thread pool
     * @param task The task to execute
     * @return Future for the task result
     */
    virtual std::future<void> submit(std::function<void()> task) = 0;

    /**
     * @brief Submit a task with delay
     * @param task The task to execute
     * @param delay Delay before execution
     * @return Future for the task result
     */
    virtual std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) = 0;

    /**
     * @brief Get the number of worker threads
     * @return Number of worker threads
     */
    virtual size_t worker_count() const = 0;

    /**
     * @brief Check if the thread pool is running
     * @return true if running, false otherwise
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Get pending task count
     * @return Number of tasks waiting to be executed
     */
    virtual size_t pending_tasks() const = 0;
};

/**
 * @class basic_thread_pool
 * @brief Basic thread pool implementation for standalone use
 *
 * This provides a simple thread pool implementation for when
 * thread_system is not available.
 */
class basic_thread_pool : public thread_pool_interface {
public:
    /**
     * @brief Construct with specified number of threads
     * @param num_threads Number of worker threads (0 = hardware concurrency)
     */
    explicit basic_thread_pool(size_t num_threads = 0);

    ~basic_thread_pool();

    // thread_pool_interface implementation
    std::future<void> submit(std::function<void()> task) override;
    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) override;
    size_t worker_count() const override;
    bool is_running() const override;
    size_t pending_tasks() const override;

    /**
     * @brief Stop the thread pool
     * @param wait_for_tasks Whether to wait for pending tasks
     */
    void stop(bool wait_for_tasks = true);

    /**
     * @brief Get completed tasks count
     * @return Number of completed tasks
     */
    size_t completed_tasks() const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

/**
 * @class thread_integration_manager
 * @brief Manager for thread system integration
 *
 * This class manages the integration between network_system and
 * thread pool implementations.
 */
class thread_integration_manager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static thread_integration_manager& instance();

    /**
     * @brief Set the thread pool implementation
     * @param pool Thread pool to use
     */
    void set_thread_pool(std::shared_ptr<thread_pool_interface> pool);

    /**
     * @brief Get the current thread pool
     * @return Current thread pool (creates basic pool if none set)
     */
    std::shared_ptr<thread_pool_interface> get_thread_pool();

    /**
     * @brief Submit a task to the thread pool
     * @param task The task to execute
     * @return Future for the task result
     */
    std::future<void> submit_task(std::function<void()> task);

    /**
     * @brief Submit a task with delay
     * @param task The task to execute
     * @param delay Delay before execution
     * @return Future for the task result
     */
    std::future<void> submit_delayed_task(
        std::function<void()> task,
        std::chrono::milliseconds delay
    );

    /**
     * @brief Get thread pool metrics
     */
    struct metrics {
        size_t worker_threads = 0;
        size_t pending_tasks = 0;
        size_t completed_tasks = 0;
        bool is_running = false;
    };

    /**
     * @brief Get current metrics
     * @return Current thread pool metrics
     */
    metrics get_metrics() const;

private:
    thread_integration_manager();
    ~thread_integration_manager();

    class impl;
    // Use shared_ptr with no-op deleter (Intentional Leak pattern) to prevent
    // heap corruption during static destruction phase when thread pool tasks
    // may still reference the impl's members.
    std::shared_ptr<impl> pimpl_;
};

} // namespace kcenon::network::integration

// Backward compatibility
namespace network_system {
    namespace integration = kcenon::network::integration;
}