// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_thread_pool_bridge.cpp
 * @brief Unit tests for ThreadPoolBridge class
 */

#include <gtest/gtest.h>

#include "internal/integration/thread_pool_bridge.h"
#include <kcenon/network/integration/thread_integration.h>
#include <kcenon/network/detail/config/feature_flags.h>

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

TEST_F(ThreadPoolBridgeTest, CreateWithNullPoolReturnsError) {
    auto result = ThreadPoolBridge::create(nullptr, ThreadPoolBridge::BackendType::Custom);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ThreadPoolBridgeTest, CreateWithValidPool) {
    auto result = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ThreadPoolBridgeTest, InitializeSuccess) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";
    config.properties["pool_name"] = "test";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, InitializeWithStoppedPoolFails) {
    pool_->set_running(false);
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_err());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, InitializeDisabledBridgeFails) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";
    config.properties["enabled"] = "false";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_err());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, InitializeAlreadyInitializedFails) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";

    auto result1 = bridge->initialize(config);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge->initialize(config);
    ASSERT_TRUE(result2.is_err());
}

TEST_F(ThreadPoolBridgeTest, ShutdownSuccess) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";

    auto init_result = bridge->initialize(config);
    ASSERT_TRUE(init_result.is_ok());

    auto shutdown_result = bridge->shutdown();
    ASSERT_TRUE(shutdown_result.is_ok());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ThreadPoolBridgeTest, ShutdownIdempotent) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";

    bridge->initialize(config);
    auto result1 = bridge->shutdown();
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge->shutdown();
    ASSERT_TRUE(result2.is_ok());
}

TEST_F(ThreadPoolBridgeTest, GetMetricsBeforeInitialization) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    auto metrics = bridge->get_metrics();
    EXPECT_FALSE(metrics.is_healthy);
}

TEST_F(ThreadPoolBridgeTest, GetMetricsAfterInitialization) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

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
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    BridgeConfig config;
    config.integration_name = "test_pool";
    bridge->initialize(config);
    bridge->shutdown();

    auto metrics = bridge->get_metrics();
    EXPECT_FALSE(metrics.is_healthy);
}

TEST_F(ThreadPoolBridgeTest, GetThreadPool) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

    auto retrieved_pool = bridge->get_thread_pool();
    EXPECT_EQ(retrieved_pool, pool_);
}

TEST_F(ThreadPoolBridgeTest, GetBackendType) {
    auto bridge = ThreadPoolBridge::create(
        pool_, ThreadPoolBridge::BackendType::ThreadSystem).value();

    EXPECT_EQ(bridge->get_backend_type(), ThreadPoolBridge::BackendType::ThreadSystem);
}

TEST_F(ThreadPoolBridgeTest, FromThreadSystemFactoryMethod) {
    auto bridge_result = ThreadPoolBridge::from_thread_system("test_pool");
    ASSERT_TRUE(bridge_result.is_ok());

    auto bridge = bridge_result.value();
    EXPECT_EQ(bridge->get_backend_type(), ThreadPoolBridge::BackendType::ThreadSystem);

    auto pool = bridge->get_thread_pool();
    ASSERT_NE(pool, nullptr);
}

TEST_F(ThreadPoolBridgeTest, ThreadPoolFunctionality) {
    auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

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

TEST_F(ThreadPoolBridgeTest, FromCommonSystemFactoryMethodWithNullExecutorReturnsError) {
    auto result = ThreadPoolBridge::from_common_system(nullptr);
    EXPECT_TRUE(result.is_err());
}

// Note: Full test for from_common_system requires a mock IExecutor
// which is complex to implement. The basic null-check is tested above.

#endif // KCENON_WITH_COMMON_SYSTEM

TEST_F(ThreadPoolBridgeTest, DestructorCallsShutdownIfInitialized) {
    {
        auto bridge = ThreadPoolBridge::create(pool_, ThreadPoolBridge::BackendType::Custom).value();

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
