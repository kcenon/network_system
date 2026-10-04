// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_observability_bridge.cpp
 * @brief Unit tests for ObservabilityBridge class
 */

#include <gtest/gtest.h>

#include "internal/integration/observability_bridge.h"
#include "internal/integration/logger_integration.h"
#include "internal/integration/monitoring_integration.h"
#include <kcenon/network/detail/config/feature_flags.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace kcenon::network::integration {
namespace {

// Mock logger for testing
class mock_logger : public logger_interface {
public:
    mock_logger() : min_level_(log_level::info) {}

    void log(log_level level, const std::string& message) override {
        (void)level;
        (void)message;
        log_count_++;
    }

    void log(log_level level, const std::string& message,
            const std::string& file, int line,
            const std::string& function) override {
        (void)level;
        (void)message;
        (void)file;
        (void)line;
        (void)function;
        log_count_++;
    }

    bool is_level_enabled(log_level level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    void flush() override {
        flush_count_++;
    }

    size_t log_count() const { return log_count_.load(); }
    size_t flush_count() const { return flush_count_.load(); }
    void reset_counts() { log_count_ = 0; flush_count_ = 0; }

private:
    log_level min_level_;
    std::atomic<size_t> log_count_{0};
    std::atomic<size_t> flush_count_{0};
};

// Mock monitoring for testing
class mock_monitoring : public monitoring_interface {
public:
    mock_monitoring() = default;

    void report_counter(const std::string& name, double value,
                       const std::map<std::string, std::string>& labels = {}) override {
        (void)name;
        (void)value;
        (void)labels;
        counter_count_++;
    }

    void report_gauge(const std::string& name, double value,
                     const std::map<std::string, std::string>& labels = {}) override {
        (void)name;
        (void)value;
        (void)labels;
        gauge_count_++;
    }

    void report_histogram(const std::string& name, double value,
                         const std::map<std::string, std::string>& labels = {}) override {
        (void)name;
        (void)value;
        (void)labels;
        histogram_count_++;
    }

    void report_health(const std::string& connection_id, bool is_alive,
                      double response_time_ms, size_t missed_heartbeats,
                      double packet_loss_rate) override {
        (void)connection_id;
        (void)is_alive;
        (void)response_time_ms;
        (void)missed_heartbeats;
        (void)packet_loss_rate;
        health_count_++;
    }

    size_t counter_count() const { return counter_count_.load(); }
    size_t gauge_count() const { return gauge_count_.load(); }
    size_t histogram_count() const { return histogram_count_.load(); }
    size_t health_count() const { return health_count_.load(); }
    void reset_counts() {
        counter_count_ = 0;
        gauge_count_ = 0;
        histogram_count_ = 0;
        health_count_ = 0;
    }

private:
    std::atomic<size_t> counter_count_{0};
    std::atomic<size_t> gauge_count_{0};
    std::atomic<size_t> histogram_count_{0};
    std::atomic<size_t> health_count_{0};
};

class ObservabilityBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger_ = std::make_shared<mock_logger>();
        monitor_ = std::make_shared<mock_monitoring>();
    }

    void TearDown() override {
        logger_.reset();
        monitor_.reset();
    }

    std::shared_ptr<mock_logger> logger_;
    std::shared_ptr<mock_monitoring> monitor_;
};

TEST_F(ObservabilityBridgeTest, CreateWithNullLoggerReturnsError) {
    auto result = ObservabilityBridge::create(nullptr, monitor_, ObservabilityBridge::BackendType::Standalone);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ObservabilityBridgeTest, CreateWithNullMonitorReturnsError) {
    auto result = ObservabilityBridge::create(logger_, nullptr, ObservabilityBridge::BackendType::Standalone);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ObservabilityBridgeTest, CreateWithValidInterfaces) {
    auto result = ObservabilityBridge::create(logger_, monitor_, ObservabilityBridge::BackendType::Standalone);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ObservabilityBridgeTest, InitializeSuccess) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    config.properties["log_level"] = "info";
    config.properties["enable_monitoring"] = "true";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge->is_initialized());
}

TEST_F(ObservabilityBridgeTest, InitializeWithMonitoringDisabled) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    config.properties["enable_monitoring"] = "false";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(bridge->is_initialized());

    auto metrics = bridge->get_metrics();
    EXPECT_EQ(0.0, metrics.custom_metrics["monitoring_enabled"]);
}

TEST_F(ObservabilityBridgeTest, InitializeDisabledBridgeFails) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    config.properties["enabled"] = "false";

    auto result = bridge->initialize(config);
    ASSERT_TRUE(result.is_err());
    EXPECT_FALSE(bridge->is_initialized());
}

TEST_F(ObservabilityBridgeTest, InitializeAlreadyInitializedFails) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";

    auto result1 = bridge->initialize(config);
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge->initialize(config);
    ASSERT_TRUE(result2.is_err());
}

TEST_F(ObservabilityBridgeTest, ShutdownSuccess) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";

    auto init_result = bridge->initialize(config);
    ASSERT_TRUE(init_result.is_ok());

    size_t flush_count_before = logger_->flush_count();
    auto shutdown_result = bridge->shutdown();
    ASSERT_TRUE(shutdown_result.is_ok());
    EXPECT_FALSE(bridge->is_initialized());
    EXPECT_GT(logger_->flush_count(), flush_count_before);
}

TEST_F(ObservabilityBridgeTest, ShutdownIdempotent) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";

    bridge->initialize(config);
    auto result1 = bridge->shutdown();
    ASSERT_TRUE(result1.is_ok());

    auto result2 = bridge->shutdown();
    ASSERT_TRUE(result2.is_ok());
}

TEST_F(ObservabilityBridgeTest, GetLoggerReturnsValidPointer) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    bridge->initialize(config);

    auto logger = bridge->get_logger();
    ASSERT_NE(nullptr, logger);
    EXPECT_EQ(logger_, logger);
}

TEST_F(ObservabilityBridgeTest, GetMonitorReturnsValidPointer) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    bridge->initialize(config);

    auto monitor = bridge->get_monitor();
    ASSERT_NE(nullptr, monitor);
    EXPECT_EQ(monitor_, monitor);
}

TEST_F(ObservabilityBridgeTest, GetMetricsReturnsCorrectBackendType) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    bridge->initialize(config);

    auto metrics = bridge->get_metrics();
    EXPECT_TRUE(metrics.is_healthy);
    EXPECT_EQ(1.0, metrics.custom_metrics["backend_type"]); // Standalone = 1
    EXPECT_EQ(1.0, metrics.custom_metrics["monitoring_enabled"]);
    EXPECT_EQ(1.0, metrics.custom_metrics["logger_available"]);
    EXPECT_EQ(1.0, metrics.custom_metrics["monitor_available"]);
}

TEST_F(ObservabilityBridgeTest, GetMetricsAfterShutdownReportsUnhealthy) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    bridge->initialize(config);
    bridge->shutdown();

    auto metrics = bridge->get_metrics();
    EXPECT_FALSE(metrics.is_healthy);
}

TEST_F(ObservabilityBridgeTest, GetBackendTypeReturnsCorrectType) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    EXPECT_EQ(ObservabilityBridge::BackendType::Standalone, bridge->get_backend_type());
}

TEST_F(ObservabilityBridgeTest, LoggerUsageAfterInitialization) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    bridge->initialize(config);

    auto logger = bridge->get_logger();
    ASSERT_NE(nullptr, logger);

    size_t log_count_before = logger_->log_count();
    logger->log(log_level::info, "Test log message");
    EXPECT_GT(logger_->log_count(), log_count_before);
}

TEST_F(ObservabilityBridgeTest, MonitoringUsageAfterInitialization) {
    auto bridge = ObservabilityBridge::create(
        logger_, monitor_, ObservabilityBridge::BackendType::Standalone).value();

    BridgeConfig config;
    config.integration_name = "test_observability";
    bridge->initialize(config);

    auto monitor = bridge->get_monitor();
    ASSERT_NE(nullptr, monitor);

    size_t counter_count_before = monitor_->counter_count();
    monitor->report_counter("test_metric", 1.0);
    EXPECT_GT(monitor_->counter_count(), counter_count_before);
}

#if KCENON_WITH_COMMON_SYSTEM
TEST_F(ObservabilityBridgeTest, FromCommonSystemWithNullArgsReturnsError) {
    // Note: This test requires common_system to be available
    // For now, we just verify that the factory method returns error with null arguments

    auto result = ObservabilityBridge::from_common_system(nullptr, nullptr);
    EXPECT_TRUE(result.is_err());
}
#endif

} // namespace
} // namespace kcenon::network::integration
