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
 * @file thread_pool_adapters.h
 * @brief Bidirectional adapters between network_system's thread_pool_interface
 *        and common_system's IExecutor/IThreadPool interfaces
 *
 * This header provides adapter classes that enable interoperability between
 * network_system's local thread pool interface and common_system's unified
 * executor interfaces.
 *
 * @author kcenon
 * @date 2025-12-27
 */

#include <kcenon/network/config/feature_flags.h>
#include "kcenon/network/integration/thread_integration.h"

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/executor_interface.h>
#include <kcenon/common/patterns/result.h>
#endif

namespace kcenon::network::integration {

#if KCENON_WITH_COMMON_SYSTEM

/**
 * @class function_job
 * @brief Helper class that wraps std::function<void()> as IJob
 *
 * This adapter allows standard function objects to be used with
 * common_system's job-based execution model.
 */
class function_job final : public ::kcenon::common::interfaces::IJob {
public:
    /**
     * @brief Construct a function job
     * @param func The function to execute
     * @param name Optional name for debugging/logging
     */
    explicit function_job(std::function<void()> func, std::string name = "function_job")
        : func_(std::move(func)), name_(std::move(name)) {}

    /**
     * @brief Execute the wrapped function
     * @return VoidResult indicating success or failure
     */
    ::kcenon::common::VoidResult execute() override {
        try {
            if (func_) {
                func_();
            }
            return ::kcenon::common::ok();
        } catch (const std::exception& ex) {
            return ::kcenon::common::VoidResult(::kcenon::common::error_info{
                ::kcenon::common::error_codes::INTERNAL_ERROR,
                ex.what(),
                "network_system::function_job"});
        } catch (...) {
            return ::kcenon::common::VoidResult(::kcenon::common::error_info{
                ::kcenon::common::error_codes::INTERNAL_ERROR,
                "Unknown exception in function_job",
                "network_system::function_job"});
        }
    }

    /**
     * @brief Get the job name
     * @return Job name for logging/debugging
     */
    std::string get_name() const override { return name_; }

private:
    std::function<void()> func_;
    std::string name_;
};

/**
 * @class network_to_common_thread_adapter
 * @brief Adapts network_system's thread_pool_interface to common IExecutor
 *
 * This adapter enables network_system's thread pools to be used where
 * common_system's IExecutor interface is expected. It wraps a
 * thread_pool_interface and exposes it as an IExecutor.
 *
 * Use case: When other systems (messaging_system, database_system) require
 * an IExecutor but you want to use network_system's thread pool.
 *
 * @code
 * auto network_pool = thread_integration_manager::instance().get_thread_pool();
 * auto executor = std::make_shared<network_to_common_thread_adapter>(network_pool);
 * messaging_system.set_executor(executor);
 * @endcode
 */
class network_to_common_thread_adapter
    : public ::kcenon::common::interfaces::IExecutor {
public:
    /**
     * @brief Construct adapter wrapping a thread_pool_interface
     * @param pool The network_system thread pool to adapt
     * @throws std::invalid_argument if pool is nullptr
     */
    explicit network_to_common_thread_adapter(
        std::shared_ptr<thread_pool_interface> pool)
        : pool_(std::move(pool)) {
        if (!pool_) {
            throw std::invalid_argument(
                "network_to_common_thread_adapter requires non-null pool");
        }
    }

    /**
     * @brief Execute a job using the wrapped thread pool
     * @param job The job to execute
     * @return Result containing future or error
     */
    ::kcenon::common::Result<std::future<void>> execute(
        std::unique_ptr<::kcenon::common::interfaces::IJob>&& job) override {
        if (!pool_ || !pool_->is_running()) {
            return ::kcenon::common::Result<std::future<void>>::err(
                ::kcenon::common::error_codes::INVALID_ARGUMENT,
                "Thread pool not running",
                "network_to_common_thread_adapter");
        }

        try {
            // Wrap the job in a shared_ptr for safe capture in lambda
            auto shared_job = std::shared_ptr<::kcenon::common::interfaces::IJob>(
                std::move(job));

            auto future = pool_->submit([shared_job]() mutable {
                auto result = shared_job->execute();
                if (result.is_err()) {
                    throw std::runtime_error(result.error().message);
                }
            });

            return ::kcenon::common::Result<std::future<void>>::ok(std::move(future));
        } catch (const std::exception& e) {
            return ::kcenon::common::Result<std::future<void>>::err(
                ::kcenon::common::error_codes::INTERNAL_ERROR,
                e.what(),
                "network_to_common_thread_adapter");
        }
    }

    /**
     * @brief Execute a job with delay using the wrapped thread pool
     * @param job The job to execute
     * @param delay Delay before execution
     * @return Result containing future or error
     */
    ::kcenon::common::Result<std::future<void>> execute_delayed(
        std::unique_ptr<::kcenon::common::interfaces::IJob>&& job,
        std::chrono::milliseconds delay) override {
        if (!pool_ || !pool_->is_running()) {
            return ::kcenon::common::Result<std::future<void>>::err(
                ::kcenon::common::error_codes::INVALID_ARGUMENT,
                "Thread pool not running",
                "network_to_common_thread_adapter");
        }

        try {
            auto shared_job = std::shared_ptr<::kcenon::common::interfaces::IJob>(
                std::move(job));

            auto future = pool_->submit_delayed(
                [shared_job]() mutable {
                    auto result = shared_job->execute();
                    if (result.is_err()) {
                        throw std::runtime_error(result.error().message);
                    }
                },
                delay);

            return ::kcenon::common::Result<std::future<void>>::ok(std::move(future));
        } catch (const std::exception& e) {
            return ::kcenon::common::Result<std::future<void>>::err(
                ::kcenon::common::error_codes::INTERNAL_ERROR,
                e.what(),
                "network_to_common_thread_adapter");
        }
    }

    /**
     * @brief Get the number of worker threads
     * @return Number of workers in the underlying pool
     */
    size_t worker_count() const override {
        return pool_ ? pool_->worker_count() : 0;
    }

    /**
     * @brief Check if the executor is running
     * @return true if the underlying pool is running
     */
    bool is_running() const override {
        return pool_ ? pool_->is_running() : false;
    }

    /**
     * @brief Get the number of pending tasks
     * @return Number of tasks waiting in the queue
     */
    size_t pending_tasks() const override {
        return pool_ ? pool_->pending_tasks() : 0;
    }

    /**
     * @brief Shutdown the executor
     * @param wait_for_completion Wait for pending tasks to complete
     *
     * @note The underlying thread_pool_interface doesn't expose shutdown,
     *       so this is a no-op. Users should manage pool lifecycle directly.
     */
    void shutdown([[maybe_unused]] bool wait_for_completion = true) override {
        // thread_pool_interface doesn't expose shutdown
        // Lifecycle management should be done on the underlying pool directly
    }

private:
    std::shared_ptr<thread_pool_interface> pool_;
};

/**
 * @class common_to_network_thread_adapter
 * @brief Adapts common IExecutor to network_system's thread_pool_interface
 *
 * This adapter enables external IExecutor implementations to be injected
 * into network_system where thread_pool_interface is expected.
 *
 * Use case: When you want network_system to use a thread pool from
 * common_system or another system that implements IExecutor.
 *
 * @code
 * auto thread_pool = container.resolve<common::interfaces::IThreadPool>();
 * auto adapted = std::make_shared<common_to_network_thread_adapter>(thread_pool);
 * thread_integration_manager::instance().set_thread_pool(adapted);
 * @endcode
 *
 * @note This is an alias-compatible replacement for common_thread_pool_adapter
 *       from common_system_adapter.h, with consistent naming convention.
 */
class common_to_network_thread_adapter : public thread_pool_interface {
public:
    /**
     * @brief Construct adapter wrapping an IExecutor
     * @param executor The common_system executor to adapt
     * @throws std::invalid_argument if executor is nullptr
     */
    explicit common_to_network_thread_adapter(
        std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor)
        : executor_(std::move(executor)) {
        if (!executor_) {
            throw std::invalid_argument(
                "common_to_network_thread_adapter requires non-null executor");
        }
    }

    /**
     * @brief Submit a task to the thread pool
     * @param task The task to execute
     * @return Future for the task result
     */
    std::future<void> submit(std::function<void()> task) override {
        if (!executor_ || !executor_->is_running()) {
            return make_error_future("Executor not running");
        }

        auto result = executor_->execute(
            std::make_unique<function_job>(std::move(task)));

        if (result.is_err()) {
            return make_error_future(result.error().message);
        }

        return std::move(result.value());
    }

    /**
     * @brief Submit a task with delay
     * @param task The task to execute
     * @param delay Delay before execution
     * @return Future for the task result
     */
    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay) override {
        if (!executor_ || !executor_->is_running()) {
            return make_error_future("Executor not running");
        }

        auto result = executor_->execute_delayed(
            std::make_unique<function_job>(std::move(task)), delay);

        if (result.is_err()) {
            return make_error_future(result.error().message);
        }

        return std::move(result.value());
    }

    /**
     * @brief Get the number of worker threads
     * @return Number of available workers
     */
    size_t worker_count() const override {
        return executor_ ? executor_->worker_count() : 0;
    }

    /**
     * @brief Check if the thread pool is running
     * @return true if running, false otherwise
     */
    bool is_running() const override {
        return executor_ ? executor_->is_running() : false;
    }

    /**
     * @brief Get pending task count
     * @return Number of tasks waiting to be executed
     */
    size_t pending_tasks() const override {
        return executor_ ? executor_->pending_tasks() : 0;
    }

    /**
     * @brief Shutdown the underlying executor
     * @param wait_for_completion Wait for pending tasks
     */
    void shutdown(bool wait_for_completion = true) {
        if (executor_) {
            executor_->shutdown(wait_for_completion);
        }
    }

private:
    static std::future<void> make_error_future(const std::string& message) {
        std::promise<void> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error(message)));
        return promise.get_future();
    }

    std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor_;
};

#else // !KCENON_WITH_COMMON_SYSTEM

// Placeholder types when common_system is not available
struct function_job_unavailable final {
    function_job_unavailable() = delete;
};

struct network_to_common_thread_adapter_unavailable final {
    network_to_common_thread_adapter_unavailable() = delete;
};

struct common_to_network_thread_adapter_unavailable final {
    common_to_network_thread_adapter_unavailable() = delete;
};

#endif // KCENON_WITH_COMMON_SYSTEM

} // namespace kcenon::network::integration
