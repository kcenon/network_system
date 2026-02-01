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
 * @file test_thread_pool_bridge.cpp
 * @brief Unit tests for ThreadPoolBridge class
 */

#include <gtest/gtest.h>

#include <kcenon/network/integration/thread_pool_bridge.h>
#include <kcenon/network/integration/thread_integration.h>
#include <kcenon/network/config/feature_flags.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace kcenon::network::integration {
namespace {

// Mock thread pool for testing
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

class ThreadPoolBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool_ = std::make_shared<mock_thread_pool>();
    }

    void TearDown() override {
        pool_.reset();
    }

    std::shared_ptr<mock_thread_pool> pool_;
};

TEST_F(ThreadPoolBridgeTest, ConstructorWithNullPoolThrows) {
    EXPECT_THROW(
        ThreadPoolBridge(nullptr, ThreadPoolBridge::BackendType::Custom),
        std::invalid_argument);
}

TEST_F(ThreadPoolBridgeTest, ConstructorWithValidPool) {
    EXPECT_NO_THROW(ThreadPoolBridge(pool_, ThreadPoolBridge::BackendType::Custom));
}

TEST_F(ThreadPoolBridgeTest, InitializeSuccess) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";
    config.properties["pool_name"] = "test";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, InitializeWithStoppedPoolFails) {
    pool_->set_running(false);
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_err());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, InitializeDisabledBridgeFails) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";
    config.properties["enabled"] = "false";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_err());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, InitializeAlreadyInitializedFails) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";

    auto result1 = bridge->initialize(config);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge->initialize(config);
    ASSERT_TRUE(result2.is_err());
}

TEST_F(ThreadPoolBridgeTest, ShutdownSuccess) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";

    auto init_result = bridge->initialize(config);
    ASSERT_TRUE(init_result.is_ok());

    auto shutdown_result = bridge->shutdown();
    ASSERT_TRUE(shutdown_result.is_ok());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, ShutdownIdempotent) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";

    bridge->initialize(config);
    auto result1 = bridge->shutdown();
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge->shutdown();
    ASSERT_TRUE(result2.is_ok());
}

TEST_F(ThreadPoolBridgeTest, GetMetricsBeforeInitialization) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    auto metrics = bridge->get_metrics();
    EXPECT_FALSE(metrics.is_healthy);
}

TEST_F(ThreadPoolBridgeTest, GetMetricsAfterInitialization) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";
    bridge->initialize(config);

    auto metrics = bridge->get_metrics();
    EXPECT_TRUE(metrics.is_healthy);
    EXPECT_EQ(metrics.custom_metrics["worker_threads"], 4.0);
    EXPECT_EQ(metrics.custom_metrics["backend_type"],
              static_cast<double>(ThreadPoolBridge::BackendType::Custom));
}

TEST_F(ThreadPoolBridgeTest, GetMetricsAfterShutdown) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";
    bridge->initialize(config);
    bridge->shutdown();

    auto metrics = bridge->get_metrics();
    EXPECT_FALSE(metrics.is_healthy);
}

TEST_F(ThreadPoolBridgeTest, GetThreadPool) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    auto retrieved_pool = bridge->get_thread_pool();
    EXPECT_EQ(retrieved_pool, pool_);
}

TEST_F(ThreadPoolBridgeTest, GetBackendType) {
    auto bridge = std::make_shared<ThreadPoolBridge>(
        pool_, ThreadPoolBridge::BackendType::ThreadSystem);

    EXPECT_EQ(bridge->get_backend_type(), ThreadPoolBridge::BackendType::ThreadSystem);
}

TEST_F(ThreadPoolBridgeTest, FromThreadSystemFactoryMethod) {
    auto bridge = ThreadPoolBridge::from_thread_system("test_pool");
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(bridge->get_backend_type(), ThreadPoolBridge::BackendType::ThreadSystem);

    auto pool = bridge->get_thread_pool();
    ASSERT_NE(pool, nullptr);
}

TEST_F(ThreadPoolBridgeTest, ThreadPoolFunctionality) {
    auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

    BridgeConfig config;
    config.integration_name = "test_pool";
    bridge->initialize(config);

    auto pool = bridge->get_thread_pool();
    ASSERT_NE(pool, nullptr);

    std::atomic<int> counter{0};
    auto future = pool->submit([&counter]() {
        counter.fetch_add(1);
    });

    future.wait();
    EXPECT_EQ(counter.load(), 1);
}

#if KCENON_WITH_COMMON_SYSTEM

TEST_F(ThreadPoolBridgeTest, FromCommonSystemFactoryMethodWithNullExecutorThrows) {
    EXPECT_THROW(
        ThreadPoolBridge::from_common_system(nullptr),
        std::invalid_argument);
}

// Note: Full test for from_common_system requires a mock IExecutor
// which is complex to implement. The basic null-check is tested above.

#endif // KCENON_WITH_COMMON_SYSTEM

TEST_F(ThreadPoolBridgeTest, DestructorCallsShutdownIfInitialized) {
    {
        auto bridge = std::make_shared<ThreadPoolBridge>(pool_, ThreadPoolBridge::BackendType::Custom);

        BridgeConfig config;
        config.integration_name = "test_pool";
        bridge->initialize(config);

        // Destructor should call shutdown automatically
    }
    // If shutdown wasn't called, this would be a problem
    // We can't directly test this, but we ensure no crash occurs
}

} // namespace
} // namespace kcenon::network::integration
