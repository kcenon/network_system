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
 * @file test_thread_pool_adapters.cpp
 * @brief Unit tests for thread pool adapter classes
 */

#include <kcenon/network/detail/config/feature_flags.h>

#include <gtest/gtest.h>

#include "internal/integration/thread_pool_adapters.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#if KCENON_WITH_COMMON_SYSTEM

namespace kcenon::network::integration {
namespace {

// Mock thread pool for testing network_to_common_thread_adapter
class mock_thread_pool : public thread_pool_interface {
public:
    mock_thread_pool() : running_(true), worker_count_(4) {}

    std::future<void> submit(std::function<void()> task) override {
        std::promise<void> promise;
        auto future = promise.get_future();

        std::thread([task = std::move(task), promise = std::move(promise)]() mutable {
            try {
                task();
                promise.set_value();
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        }).detach();

        return future;
    }

    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay) override {
        std::promise<void> promise;
        auto future = promise.get_future();

        std::thread([task = std::move(task), delay, promise = std::move(promise)]() mutable {
            std::this_thread::sleep_for(delay);
            try {
                task();
                promise.set_value();
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        }).detach();

        return future;
    }

    size_t worker_count() const override { return worker_count_; }
    bool is_running() const override { return running_; }
    size_t pending_tasks() const override { return pending_.load(); }

    void set_running(bool running) { running_ = running; }
    void set_worker_count(size_t count) { worker_count_ = count; }

private:
    std::atomic<bool> running_;
    std::atomic<size_t> pending_{0};
    size_t worker_count_;
};

// Mock executor for testing common_to_network_thread_adapter
class mock_executor : public ::kcenon::common::interfaces::IExecutor {
public:
    mock_executor() : running_(true), worker_count_(4) {}

    ::kcenon::common::Result<std::future<void>> execute(
        std::unique_ptr<::kcenon::common::interfaces::IJob>&& job) override {
        if (!running_) {
            return ::kcenon::common::Result<std::future<void>>::err(
                ::kcenon::common::error_codes::INVALID_ARGUMENT,
                "Executor not running");
        }

        std::promise<void> promise;
        auto future = promise.get_future();

        std::thread([job = std::move(job), promise = std::move(promise)]() mutable {
            auto result = job->execute();
            if (result.is_err()) {
                promise.set_exception(
                    std::make_exception_ptr(std::runtime_error(result.error().message)));
            } else {
                promise.set_value();
            }
        }).detach();

        return ::kcenon::common::Result<std::future<void>>::ok(std::move(future));
    }

    ::kcenon::common::Result<std::future<void>> execute_delayed(
        std::unique_ptr<::kcenon::common::interfaces::IJob>&& job,
        std::chrono::milliseconds delay) override {
        if (!running_) {
            return ::kcenon::common::Result<std::future<void>>::err(
                ::kcenon::common::error_codes::INVALID_ARGUMENT,
                "Executor not running");
        }

        std::promise<void> promise;
        auto future = promise.get_future();

        std::thread([job = std::move(job), delay, promise = std::move(promise)]() mutable {
            std::this_thread::sleep_for(delay);
            auto result = job->execute();
            if (result.is_err()) {
                promise.set_exception(
                    std::make_exception_ptr(std::runtime_error(result.error().message)));
            } else {
                promise.set_value();
            }
        }).detach();

        return ::kcenon::common::Result<std::future<void>>::ok(std::move(future));
    }

    size_t worker_count() const override { return worker_count_; }
    bool is_running() const override { return running_; }
    size_t pending_tasks() const override { return pending_.load(); }
    void shutdown([[maybe_unused]] bool wait_for_completion = true) override {
        running_ = false;
    }

    void set_running(bool running) { running_ = running; }
    void set_worker_count(size_t count) { worker_count_ = count; }

private:
    std::atomic<bool> running_;
    std::atomic<size_t> pending_{0};
    size_t worker_count_;
};

// ============================================================================
// function_job tests
// ============================================================================

class FunctionJobTest : public ::testing::Test {};

TEST_F(FunctionJobTest, ExecuteSuccess) {
    bool executed = false;
    function_job job([&executed]() { executed = true; });

    auto result = job.execute();

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(executed);
}

TEST_F(FunctionJobTest, ExecuteWithException) {
    function_job job([]() {
        throw std::runtime_error("test error");
    });

    auto result = job.execute();

    EXPECT_TRUE(result.is_err());
    EXPECT_NE(result.error().message.find("test error"), std::string::npos);
}

TEST_F(FunctionJobTest, GetName) {
    function_job job([]() {}, "test_job");
    EXPECT_EQ(job.get_name(), "test_job");
}

TEST_F(FunctionJobTest, GetDefaultName) {
    function_job job([]() {});
    EXPECT_EQ(job.get_name(), "function_job");
}

// ============================================================================
// network_to_common_thread_adapter tests
// ============================================================================

class NetworkToCommonAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = std::make_shared<mock_thread_pool>();
        adapter_ = std::make_shared<network_to_common_thread_adapter>(pool_);
    }

    std::shared_ptr<mock_thread_pool> pool_;
    std::shared_ptr<network_to_common_thread_adapter> adapter_;
};

TEST_F(NetworkToCommonAdapterTest, ConstructWithNullThrows) {
    EXPECT_THROW(
        network_to_common_thread_adapter(nullptr),
        std::invalid_argument);
}

TEST_F(NetworkToCommonAdapterTest, IsRunning) {
    EXPECT_TRUE(adapter_->is_running());

    pool_->set_running(false);
    EXPECT_FALSE(adapter_->is_running());
}

TEST_F(NetworkToCommonAdapterTest, WorkerCount) {
    EXPECT_EQ(adapter_->worker_count(), 4u);

    pool_->set_worker_count(8);
    EXPECT_EQ(adapter_->worker_count(), 8u);
}

TEST_F(NetworkToCommonAdapterTest, ExecuteSuccess) {
    std::atomic<bool> executed{false};

    auto job = std::make_unique<function_job>([&executed]() {
        executed = true;
    });

    auto result = adapter_->execute(std::move(job));

    EXPECT_TRUE(result.is_ok());
    result.value().wait();
    EXPECT_TRUE(executed);
}

TEST_F(NetworkToCommonAdapterTest, ExecuteMultipleTasks) {
    constexpr int num_tasks = 10;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        auto job = std::make_unique<function_job>([&counter]() {
            counter++;
        });
        auto result = adapter_->execute(std::move(job));
        ASSERT_TRUE(result.is_ok());
        futures.push_back(std::move(result.value()));
    }

    for (auto& f : futures) {
        f.wait();
    }

    EXPECT_EQ(counter.load(), num_tasks);
}

TEST_F(NetworkToCommonAdapterTest, ExecuteWhenNotRunning) {
    pool_->set_running(false);

    auto job = std::make_unique<function_job>([]() {});
    auto result = adapter_->execute(std::move(job));

    EXPECT_TRUE(result.is_err());
    EXPECT_NE(result.error().message.find("not running"), std::string::npos);
}

TEST_F(NetworkToCommonAdapterTest, ExecuteDelayedSuccess) {
    std::atomic<bool> executed{false};
    auto start = std::chrono::steady_clock::now();

    auto job = std::make_unique<function_job>([&executed]() {
        executed = true;
    });

    auto result = adapter_->execute_delayed(
        std::move(job), std::chrono::milliseconds(50));

    EXPECT_TRUE(result.is_ok());
    result.value().wait();

    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(executed);
    EXPECT_GE(elapsed, std::chrono::milliseconds(45));
}

TEST_F(NetworkToCommonAdapterTest, ExecuteDelayedWhenNotRunning) {
    pool_->set_running(false);

    auto job = std::make_unique<function_job>([]() {});
    auto result = adapter_->execute_delayed(
        std::move(job), std::chrono::milliseconds(10));

    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// common_to_network_thread_adapter tests
// ============================================================================

class CommonToNetworkAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        executor_ = std::make_shared<mock_executor>();
        adapter_ = std::make_shared<common_to_network_thread_adapter>(executor_);
    }

    std::shared_ptr<mock_executor> executor_;
    std::shared_ptr<common_to_network_thread_adapter> adapter_;
};

TEST_F(CommonToNetworkAdapterTest, ConstructWithNullThrows) {
    EXPECT_THROW(
        common_to_network_thread_adapter(nullptr),
        std::invalid_argument);
}

TEST_F(CommonToNetworkAdapterTest, IsRunning) {
    EXPECT_TRUE(adapter_->is_running());

    executor_->set_running(false);
    EXPECT_FALSE(adapter_->is_running());
}

TEST_F(CommonToNetworkAdapterTest, WorkerCount) {
    EXPECT_EQ(adapter_->worker_count(), 4u);

    executor_->set_worker_count(8);
    EXPECT_EQ(adapter_->worker_count(), 8u);
}

TEST_F(CommonToNetworkAdapterTest, SubmitSuccess) {
    std::atomic<bool> executed{false};

    auto future = adapter_->submit([&executed]() {
        executed = true;
    });

    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(CommonToNetworkAdapterTest, SubmitMultipleTasks) {
    constexpr int num_tasks = 10;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(adapter_->submit([&counter]() {
            counter++;
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }

    EXPECT_EQ(counter.load(), num_tasks);
}

TEST_F(CommonToNetworkAdapterTest, SubmitWhenNotRunning) {
    executor_->set_running(false);

    auto future = adapter_->submit([]() {});

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(CommonToNetworkAdapterTest, SubmitDelayedSuccess) {
    std::atomic<bool> executed{false};
    auto start = std::chrono::steady_clock::now();

    auto future = adapter_->submit_delayed(
        [&executed]() { executed = true; },
        std::chrono::milliseconds(50));

    future.wait();

    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_TRUE(executed);
    EXPECT_GE(elapsed, std::chrono::milliseconds(45));
}

TEST_F(CommonToNetworkAdapterTest, SubmitDelayedWhenNotRunning) {
    executor_->set_running(false);

    auto future = adapter_->submit_delayed(
        []() {},
        std::chrono::milliseconds(10));

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(CommonToNetworkAdapterTest, Shutdown) {
    EXPECT_TRUE(adapter_->is_running());
    adapter_->shutdown(true);
    EXPECT_FALSE(adapter_->is_running());
}

// ============================================================================
// Integration tests between adapters
// ============================================================================

class AdapterIntegrationTest : public ::testing::Test {};

TEST_F(AdapterIntegrationTest, RoundTripAdaptation) {
    // Create a mock pool
    auto pool = std::make_shared<mock_thread_pool>();

    // Adapt to IExecutor
    auto executor = std::make_shared<network_to_common_thread_adapter>(pool);

    // Adapt back to thread_pool_interface
    auto adapted_pool = std::make_shared<common_to_network_thread_adapter>(executor);

    // Use the adapted pool
    std::atomic<bool> executed{false};
    auto future = adapted_pool->submit([&executed]() {
        executed = true;
    });

    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(AdapterIntegrationTest, RoundTripWorkerCount) {
    auto pool = std::make_shared<mock_thread_pool>();
    pool->set_worker_count(16);

    auto executor = std::make_shared<network_to_common_thread_adapter>(pool);
    auto adapted_pool = std::make_shared<common_to_network_thread_adapter>(executor);

    EXPECT_EQ(adapted_pool->worker_count(), 16u);
}

} // namespace
} // namespace kcenon::network::integration

#else // !KCENON_WITH_COMMON_SYSTEM

// Placeholder test when common_system is not available
TEST(ThreadPoolAdaptersTest, NotAvailable) {
    GTEST_SKIP() << "common_system not available, skipping adapter tests";
}

#endif // KCENON_WITH_COMMON_SYSTEM
