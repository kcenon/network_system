/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/**
 * @file network_metrics_histogram_test.cpp
 * @brief Extended unit tests for histogram-based metric_reporter methods
 *
 * Tests validate:
 * - record_latency / record_connection_time / record_request_duration
 * - get_latency_p50 / p95 / p99 percentile accessors
 * - get_connection_time_p99 / get_request_duration_p99
 * - get_all_histograms snapshot export
 * - reset_histograms clears all data
 * - Thread safety of concurrent histogram recording
 */

#include "kcenon/network/detail/metrics/network_metrics.h"
#include "kcenon/network/detail/metrics/sliding_histogram.h"
#include "internal/integration/monitoring_integration.h"
#include "../helpers/mock_monitor.h"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

using namespace kcenon::network::metrics;
using namespace kcenon::network::integration;
using namespace kcenon::network::testing;

// ============================================================================
// Fixture: Reset histograms before and after each test
// ============================================================================

class MetricHistogramTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mock_ = std::make_shared<mock_monitor>();
		monitoring_integration_manager::instance().set_monitoring(mock_);
		metric_reporter::reset_histograms();
	}

	void TearDown() override
	{
		metric_reporter::reset_histograms();
		monitoring_integration_manager::instance().set_monitoring(nullptr);
	}

	std::shared_ptr<mock_monitor> mock_;
};

// ============================================================================
// record_latency Tests
// ============================================================================

TEST_F(MetricHistogramTest, RecordLatencySingleValue)
{
	metric_reporter::record_latency(50.0);

	// record_latency also calls report_latency internally
	EXPECT_TRUE(mock_->has_histogram(metric_names::LATENCY_MS));
}

TEST_F(MetricHistogramTest, RecordLatencyMultipleValues)
{
	for (int i = 1; i <= 100; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i));
	}

	double p50 = metric_reporter::get_latency_p50();
	EXPECT_GT(p50, 30.0);
	EXPECT_LT(p50, 70.0);
}

TEST_F(MetricHistogramTest, RecordLatencyZeroValue)
{
	metric_reporter::record_latency(0.0);

	double p50 = metric_reporter::get_latency_p50();
	// With a single zero value, p50 should be close to zero
	EXPECT_GE(p50, 0.0);
}

// ============================================================================
// Latency Percentile Tests
// ============================================================================

TEST_F(MetricHistogramTest, GetLatencyP50Median)
{
	// Record uniform distribution 1..100
	for (int i = 1; i <= 100; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i));
	}

	double p50 = metric_reporter::get_latency_p50();
	// Median of 1..100 is ~50
	EXPECT_GT(p50, 35.0);
	EXPECT_LT(p50, 65.0);
}

TEST_F(MetricHistogramTest, GetLatencyP95HighPercentile)
{
	for (int i = 1; i <= 100; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i));
	}

	double p95 = metric_reporter::get_latency_p95();
	EXPECT_GT(p95, 80.0);
	EXPECT_LE(p95, 100.0);
}

TEST_F(MetricHistogramTest, GetLatencyP99TailPercentile)
{
	for (int i = 1; i <= 100; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i));
	}

	double p99 = metric_reporter::get_latency_p99();
	EXPECT_GT(p99, 90.0);
	EXPECT_LE(p99, 100.0);
}

TEST_F(MetricHistogramTest, GetLatencyP50EmptyHistogram)
{
	double p50 = metric_reporter::get_latency_p50();
	// Empty histogram should return 0
	EXPECT_DOUBLE_EQ(p50, 0.0);
}

TEST_F(MetricHistogramTest, GetLatencyP95EmptyHistogram)
{
	double p95 = metric_reporter::get_latency_p95();
	EXPECT_DOUBLE_EQ(p95, 0.0);
}

TEST_F(MetricHistogramTest, GetLatencyP99EmptyHistogram)
{
	double p99 = metric_reporter::get_latency_p99();
	EXPECT_DOUBLE_EQ(p99, 0.0);
}

TEST_F(MetricHistogramTest, PercentileOrderP50LessThanP95LessThanP99)
{
	for (int i = 1; i <= 1000; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i));
	}

	double p50 = metric_reporter::get_latency_p50();
	double p95 = metric_reporter::get_latency_p95();
	double p99 = metric_reporter::get_latency_p99();

	EXPECT_LE(p50, p95);
	EXPECT_LE(p95, p99);
}

// ============================================================================
// record_connection_time Tests
// ============================================================================

TEST_F(MetricHistogramTest, RecordConnectionTimeSingle)
{
	metric_reporter::record_connection_time(25.0);

	EXPECT_TRUE(mock_->has_histogram(metric_names::CONNECTION_TIME_HISTOGRAM));
}

TEST_F(MetricHistogramTest, RecordConnectionTimeMultiple)
{
	for (int i = 0; i < 50; ++i)
	{
		metric_reporter::record_connection_time(static_cast<double>(i * 2));
	}

	double p99 = metric_reporter::get_connection_time_p99();
	EXPECT_GT(p99, 80.0);
}

TEST_F(MetricHistogramTest, GetConnectionTimeP99Empty)
{
	double p99 = metric_reporter::get_connection_time_p99();
	EXPECT_DOUBLE_EQ(p99, 0.0);
}

// ============================================================================
// record_request_duration Tests
// ============================================================================

TEST_F(MetricHistogramTest, RecordRequestDurationSingle)
{
	metric_reporter::record_request_duration(100.0);

	EXPECT_TRUE(mock_->has_histogram(metric_names::REQUEST_DURATION_HISTOGRAM));
}

TEST_F(MetricHistogramTest, RecordRequestDurationMultiple)
{
	for (int i = 0; i < 100; ++i)
	{
		metric_reporter::record_request_duration(static_cast<double>(i));
	}

	double p99 = metric_reporter::get_request_duration_p99();
	EXPECT_GT(p99, 90.0);
}

TEST_F(MetricHistogramTest, GetRequestDurationP99Empty)
{
	double p99 = metric_reporter::get_request_duration_p99();
	EXPECT_DOUBLE_EQ(p99, 0.0);
}

// ============================================================================
// get_all_histograms Tests
// ============================================================================

TEST_F(MetricHistogramTest, GetAllHistogramsEmpty)
{
	auto snapshots = metric_reporter::get_all_histograms();

	// Should contain entries for all three histogram types
	EXPECT_TRUE(snapshots.count(metric_names::LATENCY_HISTOGRAM) > 0);
	EXPECT_TRUE(snapshots.count(metric_names::CONNECTION_TIME_HISTOGRAM) > 0);
	EXPECT_TRUE(snapshots.count(metric_names::REQUEST_DURATION_HISTOGRAM) > 0);
}

TEST_F(MetricHistogramTest, GetAllHistogramsWithData)
{
	metric_reporter::record_latency(10.0);
	metric_reporter::record_connection_time(20.0);
	metric_reporter::record_request_duration(30.0);

	auto snapshots = metric_reporter::get_all_histograms();

	EXPECT_EQ(snapshots.size(), 3);

	// Each snapshot should have data
	auto latency = snapshots.at(metric_names::LATENCY_HISTOGRAM);
	EXPECT_EQ(latency.count, 1);
	EXPECT_DOUBLE_EQ(latency.sum, 10.0);

	auto conn_time = snapshots.at(metric_names::CONNECTION_TIME_HISTOGRAM);
	EXPECT_EQ(conn_time.count, 1);
	EXPECT_DOUBLE_EQ(conn_time.sum, 20.0);

	auto req_dur = snapshots.at(metric_names::REQUEST_DURATION_HISTOGRAM);
	EXPECT_EQ(req_dur.count, 1);
	EXPECT_DOUBLE_EQ(req_dur.sum, 30.0);
}

TEST_F(MetricHistogramTest, GetAllHistogramsLabels)
{
	metric_reporter::record_latency(5.0);

	auto snapshots = metric_reporter::get_all_histograms();
	auto latency = snapshots.at(metric_names::LATENCY_HISTOGRAM);

	// Snapshot should include the "metric" label
	EXPECT_TRUE(latency.labels.count("metric") > 0);
	EXPECT_EQ(latency.labels.at("metric"), "latency");
}

// ============================================================================
// reset_histograms Tests
// ============================================================================

TEST_F(MetricHistogramTest, ResetHistogramsClearsAllData)
{
	metric_reporter::record_latency(100.0);
	metric_reporter::record_connection_time(200.0);
	metric_reporter::record_request_duration(300.0);

	// Verify data exists before reset
	EXPECT_GT(metric_reporter::get_latency_p50(), 0.0);

	metric_reporter::reset_histograms();

	// After reset, all percentiles should return 0
	EXPECT_DOUBLE_EQ(metric_reporter::get_latency_p50(), 0.0);
	EXPECT_DOUBLE_EQ(metric_reporter::get_latency_p95(), 0.0);
	EXPECT_DOUBLE_EQ(metric_reporter::get_latency_p99(), 0.0);
	EXPECT_DOUBLE_EQ(metric_reporter::get_connection_time_p99(), 0.0);
	EXPECT_DOUBLE_EQ(metric_reporter::get_request_duration_p99(), 0.0);
}

TEST_F(MetricHistogramTest, ResetThenRecordWorks)
{
	metric_reporter::record_latency(100.0);
	metric_reporter::reset_histograms();

	// Record new data after reset
	metric_reporter::record_latency(50.0);

	double p50 = metric_reporter::get_latency_p50();
	// Should reflect new data, not old
	EXPECT_GT(p50, 0.0);
	EXPECT_LE(p50, 50.0);
}

TEST_F(MetricHistogramTest, ResetEmptyHistogramsNoThrow)
{
	// Resetting already-empty histograms should not throw
	EXPECT_NO_THROW(metric_reporter::reset_histograms());
}

TEST_F(MetricHistogramTest, ResetMultipleTimesNoThrow)
{
	metric_reporter::record_latency(10.0);
	EXPECT_NO_THROW(metric_reporter::reset_histograms());
	EXPECT_NO_THROW(metric_reporter::reset_histograms());
	EXPECT_NO_THROW(metric_reporter::reset_histograms());
}

// ============================================================================
// Cross-histogram Independence Tests
// ============================================================================

TEST_F(MetricHistogramTest, HistogramsAreIndependent)
{
	// Record only latency, not connection_time or request_duration
	for (int i = 1; i <= 100; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i));
	}

	// Latency should have data
	EXPECT_GT(metric_reporter::get_latency_p50(), 0.0);

	// Connection time and request duration should still be empty
	EXPECT_DOUBLE_EQ(metric_reporter::get_connection_time_p99(), 0.0);
	EXPECT_DOUBLE_EQ(metric_reporter::get_request_duration_p99(), 0.0);
}

// ============================================================================
// Large-Scale Recording Tests
// ============================================================================

TEST_F(MetricHistogramTest, LargeVolumeRecording)
{
	for (int i = 0; i < 10000; ++i)
	{
		metric_reporter::record_latency(static_cast<double>(i % 100));
	}

	double p50 = metric_reporter::get_latency_p50();
	double p99 = metric_reporter::get_latency_p99();

	EXPECT_GT(p50, 0.0);
	EXPECT_GT(p99, p50);
}

// ============================================================================
// Thread Safety Tests for Histogram Operations
// ============================================================================

TEST_F(MetricHistogramTest, ConcurrentRecordLatency)
{
	constexpr int kThreads = 8;
	constexpr int kIterations = 100;

	std::vector<std::thread> threads;
	threads.reserve(kThreads);

	for (int i = 0; i < kThreads; ++i)
	{
		threads.emplace_back(
			[i]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					metric_reporter::record_latency(static_cast<double>(i * kIterations + j));
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// Verify data was recorded without crashes
	double p50 = metric_reporter::get_latency_p50();
	EXPECT_GT(p50, 0.0);
}

TEST_F(MetricHistogramTest, ConcurrentRecordAndRead)
{
	constexpr int kWriters = 4;
	constexpr int kReaders = 2;
	constexpr int kIterations = 100;

	std::vector<std::thread> threads;
	threads.reserve(kWriters + kReaders);

	// Writers
	for (int i = 0; i < kWriters; ++i)
	{
		threads.emplace_back(
			[i]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					metric_reporter::record_latency(static_cast<double>(j));
					metric_reporter::record_connection_time(static_cast<double>(j));
					metric_reporter::record_request_duration(static_cast<double>(j));
				}
			});
	}

	// Readers
	for (int i = 0; i < kReaders; ++i)
	{
		threads.emplace_back(
			[]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					(void)metric_reporter::get_latency_p50();
					(void)metric_reporter::get_latency_p95();
					(void)metric_reporter::get_latency_p99();
					(void)metric_reporter::get_connection_time_p99();
					(void)metric_reporter::get_request_duration_p99();
					(void)metric_reporter::get_all_histograms();
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	SUCCEED();
}

TEST_F(MetricHistogramTest, ConcurrentRecordAndReset)
{
	constexpr int kWriters = 4;
	constexpr int kResetters = 2;
	constexpr int kIterations = 50;

	std::vector<std::thread> threads;
	threads.reserve(kWriters + kResetters);

	for (int i = 0; i < kWriters; ++i)
	{
		threads.emplace_back(
			[]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					metric_reporter::record_latency(static_cast<double>(j));
				}
			});
	}

	for (int i = 0; i < kResetters; ++i)
	{
		threads.emplace_back(
			[]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					metric_reporter::reset_histograms();
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	SUCCEED();
}
