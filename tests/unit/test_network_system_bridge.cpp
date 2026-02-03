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
 * @file test_network_system_bridge.cpp
 * @brief Unit tests for NetworkSystemBridge class
 */

#include <gtest/gtest.h>

#include <kcenon/network/integration/network_system_bridge.h>
#include "internal/integration/thread_pool_bridge.h"
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

// Test fixture for NetworkSystemBridge
class NetworkSystemBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_pool_ = std::make_shared<mock_thread_pool>();
        thread_pool_bridge_ = std::make_shared<ThreadPoolBridge>(
            mock_pool_,
            ThreadPoolBridge::BackendType::Custom);
    }

    void TearDown() override {
        if (bridge_ && bridge_->is_initialized()) {
            bridge_->shutdown();
        }
    }

    std::shared_ptr<mock_thread_pool> mock_pool_;
    std::shared_ptr<ThreadPoolBridge> thread_pool_bridge_;
    std::shared_ptr<NetworkSystemBridge> bridge_;
};

// Basic Tests

TEST_F(NetworkSystemBridgeTest, DefaultConstruction) {
    bridge_ = std::make_shared<NetworkSystemBridge>();
    EXPECT_FALSE(bridge_->is_initialized());
    EXPECT_EQ(bridge_->get_thread_pool_bridge(), nullptr);
    EXPECT_EQ(bridge_->get_thread_pool(), nullptr);
}

TEST_F(NetworkSystemBridgeTest, ConstructionWithThreadPool) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);
    EXPECT_FALSE(bridge_->is_initialized());
    EXPECT_NE(bridge_->get_thread_pool_bridge(), nullptr);
}

// Initialization Tests

TEST_F(NetworkSystemBridgeTest, InitializeWithoutThreadPool) {
    bridge_ = std::make_shared<NetworkSystemBridge>();

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = false;

    auto result = bridge_->initialize(config);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge_->is_initialized());
}

TEST_F(NetworkSystemBridgeTest, InitializeWithThreadPool) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;
    config.thread_pool_properties["pool_name"] = "test_pool";

    auto result = bridge_->initialize(config);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge_->is_initialized());
    EXPECT_NE(bridge_->get_thread_pool(), nullptr);
}

TEST_F(NetworkSystemBridgeTest, InitializeAlreadyInitialized) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    auto result1 = bridge_->initialize(config);
    EXPECT_TRUE(result1.is_ok());

    auto result2 = bridge_->initialize(config);
    EXPECT_TRUE(result2.is_err());
    EXPECT_EQ(result2.error().code, error_codes::common_errors::already_exists);
}

TEST_F(NetworkSystemBridgeTest, InitializeWithEnabledButNoThreadPool) {
    bridge_ = std::make_shared<NetworkSystemBridge>();

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    auto result = bridge_->initialize(config);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge_->is_initialized());
}

// Shutdown Tests

TEST_F(NetworkSystemBridgeTest, ShutdownWithoutInitialize) {
    bridge_ = std::make_shared<NetworkSystemBridge>();
    auto result = bridge_->shutdown();
    EXPECT_TRUE(result.is_ok());
}

TEST_F(NetworkSystemBridgeTest, ShutdownAfterInitialize) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    bridge_->initialize(config);
    EXPECT_TRUE(bridge_->is_initialized());

    auto result = bridge_->shutdown();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(bridge_->is_initialized());
}

TEST_F(NetworkSystemBridgeTest, ShutdownIdempotent) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    bridge_->initialize(config);

    auto result1 = bridge_->shutdown();
    EXPECT_TRUE(result1.is_ok());

    auto result2 = bridge_->shutdown();
    EXPECT_TRUE(result2.is_ok());
}

// Metrics Tests

TEST_F(NetworkSystemBridgeTest, GetMetricsBeforeInitialize) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);
    auto metrics = bridge_->get_metrics();
    EXPECT_TRUE(metrics.is_healthy);
}

TEST_F(NetworkSystemBridgeTest, GetMetricsAfterInitialize) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    bridge_->initialize(config);

    auto metrics = bridge_->get_metrics();
    EXPECT_TRUE(metrics.is_healthy);
    EXPECT_GT(metrics.custom_metrics.count("thread_pool.worker_threads"), 0);
}

// Configuration Tests

TEST_F(NetworkSystemBridgeTest, SetThreadPoolBridgeBeforeInitialize) {
    bridge_ = std::make_shared<NetworkSystemBridge>();

    auto result = bridge_->set_thread_pool_bridge(thread_pool_bridge_);
    EXPECT_TRUE(result.is_ok());
    EXPECT_NE(bridge_->get_thread_pool_bridge(), nullptr);
}

TEST_F(NetworkSystemBridgeTest, SetThreadPoolBridgeAfterInitialize) {
    bridge_ = std::make_shared<NetworkSystemBridge>();

    NetworkSystemBridgeConfig config;
    bridge_->initialize(config);

    auto result = bridge_->set_thread_pool_bridge(thread_pool_bridge_);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, error_codes::common_errors::already_exists);
}

TEST_F(NetworkSystemBridgeTest, SetNullThreadPoolBridge) {
    bridge_ = std::make_shared<NetworkSystemBridge>();

    auto result = bridge_->set_thread_pool_bridge(nullptr);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, error_codes::common_errors::invalid_argument);
}

// Factory Method Tests

TEST_F(NetworkSystemBridgeTest, CreateDefault) {
    auto bridge = NetworkSystemBridge::create_default();
    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(NetworkSystemBridgeTest, WithCustom) {
    auto bridge = NetworkSystemBridge::with_custom(mock_pool_);
    EXPECT_NE(bridge, nullptr);
    EXPECT_NE(bridge->get_thread_pool_bridge(), nullptr);
}

TEST_F(NetworkSystemBridgeTest, WithCustomAllComponents) {
    auto bridge = NetworkSystemBridge::with_custom(mock_pool_, nullptr, nullptr);
    EXPECT_NE(bridge, nullptr);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    auto result = bridge->initialize(config);
    EXPECT_TRUE(result.is_ok());
}

// Move Semantics Tests

TEST_F(NetworkSystemBridgeTest, MoveConstruction) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;
    bridge_->initialize(config);

    NetworkSystemBridge moved_bridge(std::move(*bridge_));
    EXPECT_TRUE(moved_bridge.is_initialized());
}

TEST_F(NetworkSystemBridgeTest, MoveAssignment) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;
    bridge_->initialize(config);

    NetworkSystemBridge moved_bridge;
    moved_bridge = std::move(*bridge_);
    EXPECT_TRUE(moved_bridge.is_initialized());
}

// Integration Tests

TEST_F(NetworkSystemBridgeTest, FullLifecycle) {
    bridge_ = std::make_shared<NetworkSystemBridge>(thread_pool_bridge_);

    NetworkSystemBridgeConfig config;
    config.integration_name = "test_integration";
    config.enable_thread_pool = true;
    config.thread_pool_properties["pool_name"] = "test_pool";

    auto init_result = bridge_->initialize(config);
    EXPECT_TRUE(init_result.is_ok());
    EXPECT_TRUE(bridge_->is_initialized());

    auto pool = bridge_->get_thread_pool();
    EXPECT_NE(pool, nullptr);

    std::atomic<bool> task_executed{false};
    auto future = pool->submit([&task_executed]() {
        task_executed.store(true);
    });

    future.wait();
    EXPECT_TRUE(task_executed.load());

    auto metrics = bridge_->get_metrics();
    EXPECT_TRUE(metrics.is_healthy);

    auto shutdown_result = bridge_->shutdown();
    EXPECT_TRUE(shutdown_result.is_ok());
    EXPECT_FALSE(bridge_->is_initialized());
}

} // namespace
} // namespace kcenon::network::integration
