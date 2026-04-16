/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/integration/monitoring_integration.h"
#include <gtest/gtest.h>

#include <map>
#include <string>

namespace integration = kcenon::network::integration;

/**
 * @file monitoring_integration_test.cpp
 * @brief Unit tests for monitoring_integration module
 *
 * Tests validate:
 * - metric_type enum values
 * - basic_monitoring construction and logging toggle
 * - basic_monitoring metric reporting (counter, gauge, histogram, health)
 * - monitoring_integration_manager singleton access
 * - monitoring_integration_manager set/get monitoring
 * - monitoring_integration_manager metric reporting through manager
 */

// ============================================================================
// metric_type Enum Tests
// ============================================================================

class MetricTypeTest : public ::testing::Test
{
};

TEST_F(MetricTypeTest, EnumValues)
{
	EXPECT_NE(integration::metric_type::counter, integration::metric_type::gauge);
	EXPECT_NE(integration::metric_type::gauge, integration::metric_type::histogram);
	EXPECT_NE(integration::metric_type::histogram, integration::metric_type::summary);
}

// ============================================================================
// basic_monitoring Constructor Tests
// ============================================================================

class BasicMonitoringConstructorTest : public ::testing::Test
{
};

TEST_F(BasicMonitoringConstructorTest, DefaultLoggingEnabled)
{
	integration::basic_monitoring monitoring;

	EXPECT_TRUE(monitoring.is_logging_enabled());
}

TEST_F(BasicMonitoringConstructorTest, LoggingDisabled)
{
	integration::basic_monitoring monitoring(false);

	EXPECT_FALSE(monitoring.is_logging_enabled());
}

TEST_F(BasicMonitoringConstructorTest, ToggleLogging)
{
	integration::basic_monitoring monitoring(true);

	monitoring.set_logging_enabled(false);
	EXPECT_FALSE(monitoring.is_logging_enabled());

	monitoring.set_logging_enabled(true);
	EXPECT_TRUE(monitoring.is_logging_enabled());
}

// ============================================================================
// basic_monitoring Metric Reporting Tests
// ============================================================================

class BasicMonitoringReportTest : public ::testing::Test
{
protected:
	integration::basic_monitoring monitoring_{false}; // disable console output
};

TEST_F(BasicMonitoringReportTest, ReportCounter)
{
	EXPECT_NO_THROW(
		monitoring_.report_counter("test.counter", 1.0));
}

TEST_F(BasicMonitoringReportTest, ReportCounterWithLabels)
{
	std::map<std::string, std::string> labels = {{"method", "GET"}, {"status", "200"}};

	EXPECT_NO_THROW(
		monitoring_.report_counter("http.requests", 42.0, labels));
}

TEST_F(BasicMonitoringReportTest, ReportGauge)
{
	EXPECT_NO_THROW(
		monitoring_.report_gauge("memory.usage", 512.0));
}

TEST_F(BasicMonitoringReportTest, ReportGaugeWithLabels)
{
	std::map<std::string, std::string> labels = {{"host", "server1"}};

	EXPECT_NO_THROW(
		monitoring_.report_gauge("cpu.percent", 75.5, labels));
}

TEST_F(BasicMonitoringReportTest, ReportHistogram)
{
	EXPECT_NO_THROW(
		monitoring_.report_histogram("request.duration", 0.150));
}

TEST_F(BasicMonitoringReportTest, ReportHistogramWithLabels)
{
	std::map<std::string, std::string> labels = {{"endpoint", "/api/users"}};

	EXPECT_NO_THROW(
		monitoring_.report_histogram("latency.ms", 25.3, labels));
}

TEST_F(BasicMonitoringReportTest, ReportHealth)
{
	EXPECT_NO_THROW(
		monitoring_.report_health("conn-123", true, 15.0, 0, 0.0));
}

TEST_F(BasicMonitoringReportTest, ReportHealthUnhealthy)
{
	EXPECT_NO_THROW(
		monitoring_.report_health("conn-456", false, 5000.0, 3, 0.15));
}

// ============================================================================
// monitoring_integration_manager Tests
// ============================================================================

class MonitoringIntegrationManagerTest : public ::testing::Test
{
};

TEST_F(MonitoringIntegrationManagerTest, SingletonAccess)
{
	auto& mgr1 = integration::monitoring_integration_manager::instance();
	auto& mgr2 = integration::monitoring_integration_manager::instance();

	EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(MonitoringIntegrationManagerTest, GetMonitoringNotNull)
{
	auto& mgr = integration::monitoring_integration_manager::instance();

	auto monitoring = mgr.get_monitoring();

	EXPECT_NE(monitoring, nullptr);
}

TEST_F(MonitoringIntegrationManagerTest, SetCustomMonitoring)
{
	auto& mgr = integration::monitoring_integration_manager::instance();
	auto custom = std::make_shared<integration::basic_monitoring>(false);

	mgr.set_monitoring(custom);

	auto retrieved = mgr.get_monitoring();
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(MonitoringIntegrationManagerTest, ReportCounterThroughManager)
{
	auto& mgr = integration::monitoring_integration_manager::instance();

	EXPECT_NO_THROW(mgr.report_counter("mgr.test", 1.0));
}

TEST_F(MonitoringIntegrationManagerTest, ReportGaugeThroughManager)
{
	auto& mgr = integration::monitoring_integration_manager::instance();

	EXPECT_NO_THROW(mgr.report_gauge("mgr.gauge", 99.0));
}

TEST_F(MonitoringIntegrationManagerTest, ReportHistogramThroughManager)
{
	auto& mgr = integration::monitoring_integration_manager::instance();

	EXPECT_NO_THROW(mgr.report_histogram("mgr.histogram", 0.5));
}

TEST_F(MonitoringIntegrationManagerTest, ReportHealthThroughManager)
{
	auto& mgr = integration::monitoring_integration_manager::instance();

	EXPECT_NO_THROW(mgr.report_health("conn-789", true, 10.0, 0, 0.0));
}
