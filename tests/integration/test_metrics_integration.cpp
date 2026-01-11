/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file test_metrics_integration.cpp
 * @brief Integration tests for network metrics system
 *
 * Tests validate:
 * - Integration between metric_reporter and monitoring_integration_manager
 * - Full metric reporting flow from reporter to monitor
 * - Real-world usage patterns
 */

#include "kcenon/network/metrics/network_metrics.h"
#include "kcenon/network/integration/monitoring_integration.h"
#include "../helpers/mock_monitor.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace kcenon::network::metrics;
using namespace kcenon::network::integration;
using namespace kcenon::network::testing;

// ============================================================================
// Test Fixture for Integration Tests
// ============================================================================

class MetricsIntegrationTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mock_monitor_ = std::make_shared<mock_monitor>();
		monitoring_integration_manager::instance().set_monitoring(mock_monitor_);
	}

	void TearDown() override
	{
		monitoring_integration_manager::instance().set_monitoring(nullptr);
	}

	std::shared_ptr<mock_monitor> mock_monitor_;
};

// ============================================================================
// Full Flow Integration Tests
// ============================================================================

TEST_F(MetricsIntegrationTest, ConnectionLifecycleMetrics)
{
	// Simulate connection lifecycle
	metric_reporter::report_active_connections(0);

	// Connection accepted
	metric_reporter::report_connection_accepted();
	metric_reporter::report_active_connections(1);

	// Data transfer
	metric_reporter::report_bytes_sent(1024);
	metric_reporter::report_bytes_received(2048);
	metric_reporter::report_latency(15.5);

	// More connections
	metric_reporter::report_connection_accepted();
	metric_reporter::report_active_connections(2);

	// Verify all metrics recorded
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::CONNECTIONS_TOTAL));
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::BYTES_SENT));
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::BYTES_RECEIVED));
	EXPECT_TRUE(mock_monitor_->has_histogram(metric_names::LATENCY_MS));
	EXPECT_TRUE(mock_monitor_->has_gauge(metric_names::CONNECTIONS_ACTIVE));

	// Verify values
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::CONNECTIONS_TOTAL), 2.0);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_SENT), 1024.0);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_RECEIVED), 2048.0);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_gauge_value(metric_names::CONNECTIONS_ACTIVE), 2.0);
}

TEST_F(MetricsIntegrationTest, ErrorHandlingMetrics)
{
	// Simulate error scenarios
	metric_reporter::report_connection_failed("connection_refused");
	metric_reporter::report_error("timeout");
	metric_reporter::report_timeout();
	metric_reporter::report_error("protocol_error");

	// Verify error metrics
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::CONNECTIONS_FAILED));
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::ERRORS_TOTAL));
	EXPECT_TRUE(mock_monitor_->has_counter(metric_names::TIMEOUTS_TOTAL));

	// Check error types recorded
	auto counters = mock_monitor_->get_counters();
	int error_count = 0;
	for (const auto& counter : counters)
	{
		if (counter.name == metric_names::ERRORS_TOTAL)
		{
			++error_count;
		}
	}
	EXPECT_EQ(error_count, 2); // Two error reports
}

TEST_F(MetricsIntegrationTest, SessionMetrics)
{
	// Start session
	auto start = std::chrono::steady_clock::now();

	metric_reporter::report_connection_accepted();
	metric_reporter::report_active_connections(1);

	// Simulate some activity
	metric_reporter::report_bytes_sent(100);
	metric_reporter::report_bytes_received(100);

	// End session
	auto end = std::chrono::steady_clock::now();
	auto duration_ms =
		std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	metric_reporter::report_session_duration(static_cast<double>(duration_ms));
	metric_reporter::report_active_connections(0);

	// Verify session metrics
	EXPECT_TRUE(mock_monitor_->has_histogram(metric_names::SESSION_DURATION_MS));
	EXPECT_DOUBLE_EQ(mock_monitor_->get_gauge_value(metric_names::CONNECTIONS_ACTIVE), 0.0);
}

TEST_F(MetricsIntegrationTest, HighVolumeDataTransfer)
{
	constexpr size_t num_packets = 100;
	constexpr size_t packet_size = 1500; // MTU size

	for (size_t i = 0; i < num_packets; ++i)
	{
		metric_reporter::report_bytes_sent(packet_size);
		metric_reporter::report_bytes_received(packet_size);
	}

	// Verify total transfer metrics
	auto total_bytes = static_cast<double>(num_packets * packet_size);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_SENT), total_bytes);
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::BYTES_RECEIVED), total_bytes);

	// Packet counts
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::PACKETS_SENT),
	                 static_cast<double>(num_packets));
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::PACKETS_RECEIVED),
	                 static_cast<double>(num_packets));
}

TEST_F(MetricsIntegrationTest, LatencyDistribution)
{
	// Report various latency values
	std::vector<double> latencies = {1.0, 2.5, 5.0, 10.0, 15.0, 50.0, 100.0, 5.0, 3.0, 7.0};

	for (double latency : latencies)
	{
		metric_reporter::report_latency(latency);
	}

	// Verify all latency reports recorded
	auto histograms = mock_monitor_->get_histograms();
	size_t latency_count = 0;
	for (const auto& hist : histograms)
	{
		if (hist.name == metric_names::LATENCY_MS)
		{
			++latency_count;
		}
	}
	EXPECT_EQ(latency_count, latencies.size());
}

// ============================================================================
// Monitoring Manager Integration Tests
// ============================================================================

TEST_F(MetricsIntegrationTest, DirectManagerReporting)
{
	// Use manager directly instead of metric_reporter
	auto& manager = monitoring_integration_manager::instance();

	manager.report_counter("custom.counter", 5.0, {{"env", "test"}});
	manager.report_gauge("custom.gauge", 42.0);
	manager.report_histogram("custom.histogram", 10.5);

	EXPECT_TRUE(mock_monitor_->has_counter("custom.counter"));
	EXPECT_TRUE(mock_monitor_->has_gauge("custom.gauge"));
	EXPECT_TRUE(mock_monitor_->has_histogram("custom.histogram"));
}

TEST_F(MetricsIntegrationTest, HealthReporting)
{
	auto& manager = monitoring_integration_manager::instance();

	manager.report_health("conn-123", true, 5.0, 0, 0.0);
	manager.report_health("conn-456", true, 10.0, 1, 0.01);
	manager.report_health("conn-789", false, 0.0, 5, 0.5);

	auto health_reports = mock_monitor_->get_health_reports();
	ASSERT_EQ(health_reports.size(), 3);

	// Verify first report
	EXPECT_EQ(health_reports[0].connection_id, "conn-123");
	EXPECT_TRUE(health_reports[0].is_alive);
	EXPECT_DOUBLE_EQ(health_reports[0].response_time_ms, 5.0);

	// Verify unhealthy connection
	EXPECT_EQ(health_reports[2].connection_id, "conn-789");
	EXPECT_FALSE(health_reports[2].is_alive);
	EXPECT_EQ(health_reports[2].missed_heartbeats, 5);
}

// ============================================================================
// Concurrent Access Integration Tests
// ============================================================================

TEST_F(MetricsIntegrationTest, ConcurrentConnectionTracking)
{
	constexpr int num_threads = 4;
	constexpr int connections_per_thread = 25;
	std::atomic<int> active_connections{0};

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int i = 0; i < num_threads; ++i)
	{
		threads.emplace_back(
			[&]()
			{
				for (int j = 0; j < connections_per_thread; ++j)
				{
					metric_reporter::report_connection_accepted();
					int current = active_connections.fetch_add(1, std::memory_order_relaxed) + 1;
					metric_reporter::report_active_connections(static_cast<size_t>(current));

					// Simulate some activity
					metric_reporter::report_bytes_sent(100);

					// Disconnect
					current = active_connections.fetch_sub(1, std::memory_order_relaxed) - 1;
					metric_reporter::report_active_connections(static_cast<size_t>(current));
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// All connections should have been accepted
	int total_connections = num_threads * connections_per_thread;
	EXPECT_DOUBLE_EQ(mock_monitor_->get_counter_value(metric_names::CONNECTIONS_TOTAL),
	                 static_cast<double>(total_connections));

	// Final active connections should be 0
	EXPECT_EQ(active_connections.load(), 0);
}

// ============================================================================
// Default Monitoring Fallback Tests
// ============================================================================

TEST(DefaultMonitoringIntegrationTest, FallbackToBasicMonitoring)
{
	// Reset to force creation of default monitoring
	monitoring_integration_manager::instance().set_monitoring(nullptr);

	auto monitoring = monitoring_integration_manager::instance().get_monitoring();
	ASSERT_NE(monitoring, nullptr);

	// Should be able to report without crash
	EXPECT_NO_THROW({
		metric_reporter::report_connection_accepted();
		metric_reporter::report_bytes_sent(100);
		metric_reporter::report_latency(5.0);
	});
}

// ============================================================================
// Custom Monitoring Implementation Test
// ============================================================================

class custom_test_monitor : public monitoring_interface
{
public:
	void report_counter(const std::string& /*name*/, double value,
	                    const std::map<std::string, std::string>& /*labels*/ = {}) override
	{
		total_counter_value_ += value;
	}

	void report_gauge(const std::string& /*name*/, double value,
	                  const std::map<std::string, std::string>& /*labels*/ = {}) override
	{
		last_gauge_value_ = value;
	}

	void report_histogram(const std::string& /*name*/, double value,
	                      const std::map<std::string, std::string>& /*labels*/ = {}) override
	{
		histogram_sum_ += value;
		++histogram_count_;
	}

	void report_health(const std::string& /*connection_id*/, bool /*is_alive*/,
	                   double /*response_time_ms*/, size_t /*missed_heartbeats*/,
	                   double /*packet_loss_rate*/) override
	{
		++health_report_count_;
	}

	double total_counter_value_ = 0.0;
	double last_gauge_value_ = 0.0;
	double histogram_sum_ = 0.0;
	size_t histogram_count_ = 0;
	size_t health_report_count_ = 0;
};

TEST(CustomMonitoringTest, PluggableMonitoringImplementation)
{
	auto custom_monitor = std::make_shared<custom_test_monitor>();
	monitoring_integration_manager::instance().set_monitoring(custom_monitor);

	// Report metrics
	metric_reporter::report_connection_accepted();
	metric_reporter::report_connection_accepted();
	metric_reporter::report_active_connections(2);
	metric_reporter::report_latency(10.0);
	metric_reporter::report_latency(20.0);

	// Verify custom monitor received metrics
	EXPECT_GT(custom_monitor->total_counter_value_, 0.0);
	EXPECT_DOUBLE_EQ(custom_monitor->last_gauge_value_, 2.0);
	EXPECT_DOUBLE_EQ(custom_monitor->histogram_sum_, 30.0);
	EXPECT_EQ(custom_monitor->histogram_count_, 2);

	// Cleanup
	monitoring_integration_manager::instance().set_monitoring(nullptr);
}
