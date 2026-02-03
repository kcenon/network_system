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
 * @file test_messaging_bridge_interface.cpp
 * @brief Unit tests for messaging_bridge INetworkBridge interface compliance
 *
 * Tests verify that messaging_bridge correctly implements the INetworkBridge
 * interface for unified lifecycle management.
 */

#include <gtest/gtest.h>

#include "internal/integration/messaging_bridge.h"
#include "internal/integration/bridge_interface.h"
#include <kcenon/network/config/feature_flags.h>

#include <chrono>
#include <thread>

namespace kcenon::network::integration {
namespace {

class MessagingBridgeInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        bridge_ = std::make_shared<messaging_bridge>();
    }

    void TearDown() override {
        if (bridge_ && bridge_->is_initialized()) {
            bridge_->shutdown();
        }
        bridge_.reset();
    }

    std::shared_ptr<messaging_bridge> bridge_;
};

// ============================================================
// INetworkBridge Interface Compliance Tests
// ============================================================

TEST_F(MessagingBridgeInterfaceTest, InitializeSuccess) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    config.properties["enabled"] = "true";

    auto result = bridge_->initialize(config);
    ASSERT_TRUE(result.is_ok()) << "Initialize should succeed with valid config";
    EXPECT_TRUE(bridge_->is_initialized()) << "Bridge should be initialized";
}

TEST_F(MessagingBridgeInterfaceTest, InitializeDisabledBridgeFails) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    config.properties["enabled"] = "false";

    auto result = bridge_->initialize(config);
    ASSERT_TRUE(result.is_err()) << "Initialize should fail when disabled";
    EXPECT_FALSE(bridge_->is_initialized()) << "Bridge should not be initialized";
}

TEST_F(MessagingBridgeInterfaceTest, InitializeAlreadyInitializedFails) {
    BridgeConfig config;
    config.integration_name = "messaging_system";

    auto result1 = bridge_->initialize(config);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge_->initialize(config);
    ASSERT_TRUE(result2.is_err()) << "Second initialize should fail";
    EXPECT_EQ(result2.error().code, error_codes::common_errors::already_exists)
        << "Error code should be already_exists";
}

TEST_F(MessagingBridgeInterfaceTest, ShutdownSuccess) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    auto result = bridge_->shutdown();
    ASSERT_TRUE(result.is_ok()) << "Shutdown should succeed";
    EXPECT_FALSE(bridge_->is_initialized()) << "Bridge should not be initialized after shutdown";
}

TEST_F(MessagingBridgeInterfaceTest, ShutdownIdempotent) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    auto result1 = bridge_->shutdown();
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge_->shutdown();
    ASSERT_TRUE(result2.is_ok()) << "Second shutdown should succeed (idempotent)";
}

TEST_F(MessagingBridgeInterfaceTest, ShutdownWithoutInitializeSucceeds) {
    auto result = bridge_->shutdown();
    ASSERT_TRUE(result.is_ok()) << "Shutdown without initialize should succeed (idempotent)";
}

TEST_F(MessagingBridgeInterfaceTest, IsInitializedBeforeInitialize) {
    EXPECT_FALSE(bridge_->is_initialized()) << "Bridge should not be initialized initially";
}

TEST_F(MessagingBridgeInterfaceTest, IsInitializedAfterInitialize) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    EXPECT_TRUE(bridge_->is_initialized()) << "Bridge should be initialized after initialize()";
}

TEST_F(MessagingBridgeInterfaceTest, IsInitializedAfterShutdown) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);
    bridge_->shutdown();

    EXPECT_FALSE(bridge_->is_initialized()) << "Bridge should not be initialized after shutdown";
}

TEST_F(MessagingBridgeInterfaceTest, GetMetricsBeforeInitialize) {
    auto metrics = bridge_->get_metrics();

    // Metrics should be accessible even before initialization
    EXPECT_FALSE(metrics.is_healthy) << "Uninitialized bridge should be unhealthy";
}

TEST_F(MessagingBridgeInterfaceTest, GetMetricsAfterInitialize) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    auto metrics = bridge_->get_metrics();

    EXPECT_TRUE(metrics.is_healthy) << "Initialized bridge should be healthy";
    EXPECT_GT(metrics.last_activity.time_since_epoch().count(), 0)
        << "Last activity should be set";
}

TEST_F(MessagingBridgeInterfaceTest, GetMetricsContainsCustomMetrics) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    auto metrics = bridge_->get_metrics();

    // Verify custom metrics are present
    EXPECT_TRUE(metrics.custom_metrics.count("messages_sent") > 0)
        << "Should have messages_sent metric";
    EXPECT_TRUE(metrics.custom_metrics.count("messages_received") > 0)
        << "Should have messages_received metric";
    EXPECT_TRUE(metrics.custom_metrics.count("bytes_sent") > 0)
        << "Should have bytes_sent metric";
    EXPECT_TRUE(metrics.custom_metrics.count("bytes_received") > 0)
        << "Should have bytes_received metric";
    EXPECT_TRUE(metrics.custom_metrics.count("connections_active") > 0)
        << "Should have connections_active metric";
    EXPECT_TRUE(metrics.custom_metrics.count("avg_latency_ms") > 0)
        << "Should have avg_latency_ms metric";

    // Verify initial values are zero
    EXPECT_EQ(metrics.custom_metrics["messages_sent"], 0.0);
    EXPECT_EQ(metrics.custom_metrics["messages_received"], 0.0);
    EXPECT_EQ(metrics.custom_metrics["bytes_sent"], 0.0);
    EXPECT_EQ(metrics.custom_metrics["bytes_received"], 0.0);
    EXPECT_EQ(metrics.custom_metrics["connections_active"], 0.0);
    EXPECT_EQ(metrics.custom_metrics["avg_latency_ms"], 0.0);
}

TEST_F(MessagingBridgeInterfaceTest, GetMetricsThreadSafe) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    // Call get_metrics from multiple threads concurrently
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count]() {
            try {
                auto metrics = bridge_->get_metrics();
                if (metrics.is_healthy) {
                    ++success_count;
                }
            } catch (...) {
                // Should not throw
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10) << "All get_metrics calls should succeed";
}

// ============================================================
// Backward Compatibility Tests
// ============================================================

TEST_F(MessagingBridgeInterfaceTest, CreateServerAfterInitialize) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    auto server = bridge_->create_server("test_server");
    ASSERT_NE(server, nullptr) << "Should create server after initialization";
}

TEST_F(MessagingBridgeInterfaceTest, CreateClientAfterInitialize) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    auto client = bridge_->create_client("test_client");
    ASSERT_NE(client, nullptr) << "Should create client after initialization";
}

TEST_F(MessagingBridgeInterfaceTest, BackwardCompatibilityPerformanceMetrics) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    // Test deprecated performance_metrics getter
    auto perf_metrics = bridge_->get_performance_metrics();

    EXPECT_EQ(perf_metrics.messages_sent, 0);
    EXPECT_EQ(perf_metrics.messages_received, 0);
    EXPECT_EQ(perf_metrics.bytes_sent, 0);
    EXPECT_EQ(perf_metrics.bytes_received, 0);
    EXPECT_EQ(perf_metrics.connections_active, 0);
}

TEST_F(MessagingBridgeInterfaceTest, ResetMetricsPreservesInitialization) {
    BridgeConfig config;
    config.integration_name = "messaging_system";
    bridge_->initialize(config);

    bridge_->reset_metrics();

    EXPECT_TRUE(bridge_->is_initialized())
        << "Reset metrics should not affect initialization state";

    auto metrics = bridge_->get_metrics();
    EXPECT_EQ(metrics.custom_metrics["messages_sent"], 0.0)
        << "Metrics should be reset to zero";
}

// ============================================================
// Lifecycle Tests
// ============================================================

TEST_F(MessagingBridgeInterfaceTest, DestructorCallsShutdownAutomatically) {
    {
        auto temp_bridge = std::make_shared<messaging_bridge>();
        BridgeConfig config;
        config.integration_name = "messaging_system";
        temp_bridge->initialize(config);

        EXPECT_TRUE(temp_bridge->is_initialized());
    } // temp_bridge destructor should call shutdown()

    // If we reach here without crash, destructor worked correctly
    SUCCEED();
}

} // namespace
} // namespace kcenon::network::integration
