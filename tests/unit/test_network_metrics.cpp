/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file test_network_metrics.cpp
 * @brief Unit tests for network metrics system
 *
 * Tests validate:
 * - Metric name constants
 * - metric_reporter methods
 * - Thread safety of metric reporting
 * - Edge cases
 */

#include "kcenon/network/detail/metrics/network_metrics.h"
#include "internal/integration/monitoring_integration.h"
#include "../helpers/mock_monitor.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

using namespace kcenon::network::metrics;
using namespace kcenon::network::integration;
using namespace kcenon::network::testing;

// ============================================================================
// Metric Names Constants Tests
// ============================================================================

TEST(MetricNamesTest, ConnectionMetrics)
{
	EXPECT_STREQ(metric_names::CONNECTIONS_ACTIVE, "network.connections.active");
	EXPECT_STREQ(metric_names::CONNECTIONS_TOTAL, "network.connections.total");
	EXPECT_STREQ(metric_names::CONNECTIONS_FAILED, "network.connections.failed");
}

TEST(MetricNamesTest, TransferMetrics)
{
	EXPECT_STREQ(metric_names::BYTES_SENT, "network.bytes.sent");
	EXPECT_STREQ(metric_names::BYTES_RECEIVED, "network.bytes.received");
	EXPECT_STREQ(metric_names::PACKETS_SENT, "network.packets.sent");
	EXPECT_STREQ(metric_names::PACKETS_RECEIVED, "network.packets.received");
}

TEST(MetricNamesTest, PerformanceMetrics)
{
	EXPECT_STREQ(metric_names::LATENCY_MS, "network.latency.ms");
	EXPECT_STREQ(metric_names::THROUGHPUT_MBPS, "network.throughput.mbps");
	EXPECT_STREQ(metric_names::SESSION_DURATION_MS, "network.session.duration.ms");
}

TEST(MetricNamesTest, ErrorMetrics)
{
	EXPECT_STREQ(metric_names::ERRORS_TOTAL, "network.errors.total");
	EXPECT_STREQ(metric_names::TIMEOUTS_TOTAL, "network.timeouts.total");
}

TEST(MetricNamesTest, ServerMetrics)
{
	EXPECT_STREQ(metric_names::SERVER_START_TIME, "network.server.start_time.ms");
	EXPECT_STREQ(metric_names::SERVER_ACCEPT_COUNT, "network.server.accept.count");
	EXPECT_STREQ(metric_names::SERVER_ACCEPT_FAILED, "network.server.accept.failed");
}

// ============================================================================
// Test Fixture for Metric Reporter Tests
// ============================================================================

class MetricReporterTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mock_monitor_ = std::make_shared<mock_monitor>();
		monitoring_integration_manager::instance().set_monitoring(mock_monitor_);
	}

	void TearDown() override
	{
		// Reset to default monitoring
		monitoring_integration_manager::instance().set_monitoring(nullptr);
	}

	std::shared_ptr<mock_monitor> mock_monitor_;
};

// ============================================================================
// Reporter Method Tests
// ============================================================================

TEST_F(MetricReporterTest, ReportConnectionAccepted)
{
	metric_reporter::report_connection_accepted();

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::CONNECTIONS_TOTAL));
	EXPECT_EQ(mock_monitor_->counter_call_count(), 1);
}

TEST_F(MetricReporterTest, ReportConnectionFailed)
{
	metric_reporter::report_connection_failed("timeout");

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::CONNECTIONS_FAILED));
	EXPECT_EQ(mock_monitor_->counter_call_count(), 1);

	auto counters = mock_monitor_->get_counters();
	ASSERT_EQ(counters.size(), 1);
	EXPECT_EQ(counters[0].name, metric_names::CONNECTIONS_FAILED);
	EXPECT_EQ(counters[0].value, 1.0);
	ASSERT_TRUE(counters[0].labels.count("reason") > 0);
	EXPECT_EQ(counters[0].labels.at("reason"), "timeout");
}

TEST_F(MetricReporterTest, ReportBytesSent)
{
	metric_reporter::report_bytes_sent(1024);

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::BYTES_SENT));
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::PACKETS_SENT));

	// bytes_sent and packets_sent = 2 counter calls
	EXPECT_EQ(mock_monitor_->counter_call_count(), 2);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_SENT), 1024.0);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::PACKETS_SENT), 1.0);
}

TEST_F(MetricReporterTest, ReportBytesReceived)
{
	metric_reporter::report_bytes_received(2048);

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::BYTES_RECEIVED));
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::PACKETS_RECEIVED));

	EXPECT_EQ(mock_monitor_->counter_call_count(), 2);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_RECEIVED), 2048.0);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::PACKETS_RECEIVED), 1.0);
}

TEST_F(MetricReporterTest, ReportLatency)
{
	metric_reporter::report_latency(42.5);

	EXPECT_TRUE(mock_monitor_->has_histogram(metric_names::LATENCY_MS));
	EXPECT_EQ(mock_monitor_->histogram_call_count(), 1);

	auto histograms = mock_monitor_->get_histograms();
	ASSERT_EQ(histograms.size(), 1);
	EXPECT_EQ(histograms[0].name, metric_names::LATENCY_MS);
	EXPECT_DOUBLE_EQ(histograms[0].value, 42.5);
}

TEST_F(MetricReporterTest, ReportError)
{
	metric_reporter::report_error("connection_reset");

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::ERRORS_TOTAL));
	EXPECT_EQ(mock_monitor_->counter_call_count(), 1);

	auto counters = mock_monitor_->get_counters();
	ASSERT_EQ(counters.size(), 1);
	ASSERT_TRUE(counters[0].labels.count("error_type") > 0);
	EXPECT_EQ(counters[0].labels.at("error_type"), "connection_reset");
}

TEST_F(MetricReporterTest, ReportTimeout)
{
	metric_reporter::report_timeout();

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::TIMEOUTS_TOTAL));
	EXPECT_EQ(mock_monitor_->counter_call_count(), 1);
}

TEST_F(MetricReporterTest, ReportActiveConnections)
{
	metric_reporter::report_active_connections(10);

	EXPECT_TRUE(mock_monitor_->has_gauge(metric_names::CONNECTIONS_ACTIVE));
	EXPECT_EQ(mock_monitor_->gauge_call_count(), 1);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_gauge_value(metric_names::CONNECTIONS_ACTIVE), 10.0);
}

TEST_F(MetricReporterTest, ReportSessionDuration)
{
	metric_reporter::report_session_duration(5000.0);

	EXPECT_TRUE(mock_monitor_->has_histogram(metric_names::SESSION_DURATION_MS));
	EXPECT_EQ(mock_monitor_->histogram_call_count(), 1);

	auto histograms = mock_monitor_->get_histograms();
	ASSERT_EQ(histograms.size(), 1);
	EXPECT_EQ(histograms[0].name, metric_names::SESSION_DURATION_MS);
	EXPECT_DOUBLE_EQ(histograms[0].value, 5000.0);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(MetricReporterTest, ReportZeroBytes)
{
	metric_reporter::report_bytes_sent(0);

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::BYTES_SENT));
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_SENT), 0.0);
}

TEST_F(MetricReporterTest, ReportLargeBytes)
{
	constexpr size_t large_bytes = 1ULL << 40; // 1 TB
	metric_reporter::report_bytes_sent(large_bytes);

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::BYTES_SENT));
	auto counters = mock_monitor_->get_counters();
	ASSERT_FALSE(counters.empty());
	// Verify the value was recorded (may lose precision in double conversion)
	EXPECT_GT(counters[0].value, 0.0);
}

TEST_F(MetricReporterTest, ReportEmptyErrorType)
{
	metric_reporter::report_error("");

	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::ERRORS_TOTAL));
	EXPECT_EQ(mock_monitor_->counter_call_count(), 1);

	auto counters = mock_monitor_->get_counters();
	ASSERT_EQ(counters.size(), 1);
	EXPECT_EQ(counters[0].labels.at("error_type"), "");
}

TEST_F(MetricReporterTest, ReportZeroLatency)
{
	metric_reporter::report_latency(0.0);

	EXPECT_TRUE(mock_monitor_->has_histogram(metric_names::LATENCY_MS));
	auto histograms = mock_monitor_->get_histograms();
	ASSERT_EQ(histograms.size(), 1);
	EXPECT_DOUBLE_EQ(histograms[0].value, 0.0);
}

TEST_F(MetricReporterTest, ReportNegativeLatency)
{
	// Should still report, even if logically incorrect
	metric_reporter::report_latency(-1.0);

	EXPECT_TRUE(mock_monitor_->has_histogram(metric_names::LATENCY_MS));
	auto histograms = mock_monitor_->get_histograms();
	ASSERT_EQ(histograms.size(), 1);
	EXPECT_DOUBLE_EQ(histograms[0].value, -1.0);
}

TEST_F(MetricReporterTest, ReportZeroActiveConnections)
{
	metric_reporter::report_active_connections(0);

	EXPECT_TRUE(mock_monitor_->has_gauge(metric_names::CONNECTIONS_ACTIVE));
	EXPECT_DOUBLE_EQ(mock_monitor_->get_gauge_value(metric_names::CONNECTIONS_ACTIVE), 0.0);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(MetricReporterTest, ConcurrentCounterReporting)
{
	constexpr int kThreads = 10;
	constexpr int kIterations = 100;

	std::vector<std::thread> threads;
	threads.reserve(kThreads);

	for (int i = 0; i < kThreads; ++i)
	{
		threads.emplace_back(
			[&]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					metric_reporter::report_connection_accepted();
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// Each thread reports kIterations times
	EXPECT_EQ(mock_monitor_->counter_call_count(), kThreads * kIterations);
}

TEST_F(MetricReporterTest, ConcurrentMixedReporting)
{
	constexpr int kThreads = 8;
	constexpr int kIterations = 50;

	std::vector<std::thread> threads;
	threads.reserve(kThreads);

	for (int i = 0; i < kThreads; ++i)
	{
		threads.emplace_back(
			[&, i]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					switch (i % 4)
					{
					case 0:
						metric_reporter::report_bytes_sent(100);
						break;
					case 1:
						metric_reporter::report_connection_accepted();
						break;
					case 2:
						metric_reporter::report_latency(10.0);
						break;
					case 3:
						metric_reporter::report_active_connections(5);
						break;
					}
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// Verify no crashes and metrics were recorded
	EXPECT_GT(mock_monitor_->total_call_count(), 0);
}

// ============================================================================
// Multiple Reports Accumulation Tests
// ============================================================================

TEST_F(MetricReporterTest, MultipleBytesSentAccumulates)
{
	metric_reporter::report_bytes_sent(100);
	metric_reporter::report_bytes_sent(200);
	metric_reporter::report_bytes_sent(300);

	// Total bytes should be sum
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_SENT), 600.0);
	// Each call also reports packets_sent
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::PACKETS_SENT), 3.0);
}

TEST_F(MetricReporterTest, GaugeOverwrites)
{
	metric_reporter::report_active_connections(10);
	metric_reporter::report_active_connections(20);
	metric_reporter::report_active_connections(5);

	// Gauge should return the most recent value
	EXPECT_DOUBLE_EQ(mock_monitor_->get_gauge_value(metric_names::CONNECTIONS_ACTIVE), 5.0);
	EXPECT_EQ(mock_monitor_->gauge_call_count(), 3);
}

// ============================================================================
// Monitoring Integration Manager Tests
// ============================================================================

TEST(MonitoringIntegrationManagerTest, SingletonInstance)
{
	auto& instance1 = monitoring_integration_manager::instance();
	auto& instance2 = monitoring_integration_manager::instance();

	EXPECT_EQ(&instance1, &instance2);
}

TEST(MonitoringIntegrationManagerTest, DefaultMonitoringCreated)
{
	// Reset to force creation of default monitoring
	monitoring_integration_manager::instance().set_monitoring(nullptr);

	auto monitoring = monitoring_integration_manager::instance().get_monitoring();
	EXPECT_NE(monitoring, nullptr);
}

TEST(MonitoringIntegrationManagerTest, SetAndGetMonitoring)
{
	auto mock = std::make_shared<mock_monitor>();
	monitoring_integration_manager::instance().set_monitoring(mock);

	auto retrieved = monitoring_integration_manager::instance().get_monitoring();
	EXPECT_EQ(mock.get(), retrieved.get());

	// Cleanup
	monitoring_integration_manager::instance().set_monitoring(nullptr);
}

// ============================================================================
// Basic Monitoring Tests
// ============================================================================

TEST(BasicMonitoringTest, Construction)
{
	EXPECT_NO_THROW({ basic_monitoring monitor(true); });
	EXPECT_NO_THROW({ basic_monitoring monitor(false); });
}

TEST(BasicMonitoringTest, LoggingEnabledState)
{
	basic_monitoring monitor(true);
	EXPECT_TRUE(monitor.is_logging_enabled());

	monitor.set_logging_enabled(false);
	EXPECT_FALSE(monitor.is_logging_enabled());

	monitor.set_logging_enabled(true);
	EXPECT_TRUE(monitor.is_logging_enabled());
}

TEST(BasicMonitoringTest, ReportMethodsNoThrow)
{
	basic_monitoring monitor(false); // Disable logging to avoid console output

	EXPECT_NO_THROW(monitor.report_counter("test.counter", 1.0, {}));
	EXPECT_NO_THROW(monitor.report_gauge("test.gauge", 42.0, {}));
	EXPECT_NO_THROW(monitor.report_histogram("test.histogram", 10.5, {}));
	EXPECT_NO_THROW(monitor.report_health("conn-1", true, 5.0, 0, 0.0));
}

TEST(BasicMonitoringTest, ReportWithLabels)
{
	basic_monitoring monitor(false);

	std::map<std::string, std::string> labels = {{"key1", "value1"}, {"key2", "value2"}};

	EXPECT_NO_THROW(monitor.report_counter("test.counter", 1.0, labels));
	EXPECT_NO_THROW(monitor.report_gauge("test.gauge", 42.0, labels));
	EXPECT_NO_THROW(monitor.report_histogram("test.histogram", 10.5, labels));
}
