/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file test_histogram.cpp
 * @brief Unit tests for histogram and sliding_histogram classes
 *
 * Tests validate:
 * - Histogram bucket classification
 * - Percentile calculations
 * - Thread safety
 * - Prometheus/JSON export formats
 * - Sliding window expiration
 */

#include "kcenon/network/detail/metrics/histogram.h"
#include "kcenon/network/detail/metrics/sliding_histogram.h"
#include "kcenon/network/detail/metrics/network_metrics.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <thread>
#include <vector>

using namespace kcenon::network::metrics;

// ============================================================================
// Histogram Basic Tests
// ============================================================================

TEST(HistogramTest, DefaultConstruction)
{
	histogram h;
	EXPECT_EQ(h.count(), 0);
	EXPECT_DOUBLE_EQ(h.sum(), 0.0);
	EXPECT_DOUBLE_EQ(h.mean(), 0.0);
}

TEST(HistogramTest, CustomBucketBoundaries)
{
	histogram_config cfg;
	cfg.bucket_boundaries = {1.0, 5.0, 10.0, 50.0, 100.0};
	histogram h(cfg);

	h.record(3.0);
	h.record(7.0);
	h.record(75.0);

	EXPECT_EQ(h.count(), 3);
	EXPECT_DOUBLE_EQ(h.sum(), 85.0);
}

TEST(HistogramTest, RecordValues)
{
	histogram h;

	h.record(1.0);
	h.record(5.0);
	h.record(10.0);
	h.record(50.0);
	h.record(100.0);

	EXPECT_EQ(h.count(), 5);
	EXPECT_DOUBLE_EQ(h.sum(), 166.0);
	EXPECT_DOUBLE_EQ(h.min(), 1.0);
	EXPECT_DOUBLE_EQ(h.max(), 100.0);
	EXPECT_DOUBLE_EQ(h.mean(), 33.2);
}

TEST(HistogramTest, MinMaxTracking)
{
	histogram h;

	h.record(50.0);
	EXPECT_DOUBLE_EQ(h.min(), 50.0);
	EXPECT_DOUBLE_EQ(h.max(), 50.0);

	h.record(10.0);
	EXPECT_DOUBLE_EQ(h.min(), 10.0);
	EXPECT_DOUBLE_EQ(h.max(), 50.0);

	h.record(100.0);
	EXPECT_DOUBLE_EQ(h.min(), 10.0);
	EXPECT_DOUBLE_EQ(h.max(), 100.0);
}

TEST(HistogramTest, EmptyPercentile)
{
	histogram h;
	EXPECT_DOUBLE_EQ(h.p50(), 0.0);
	EXPECT_DOUBLE_EQ(h.p95(), 0.0);
	EXPECT_DOUBLE_EQ(h.p99(), 0.0);
}

TEST(HistogramTest, PercentileCalculation)
{
	histogram_config cfg;
	cfg.bucket_boundaries = {10.0, 20.0, 30.0, 40.0, 50.0};
	histogram h(cfg);

	// Add 100 values: 10 each at 5, 15, 25, 35, 45, 55, etc.
	for (int i = 0; i < 10; ++i)
	{
		h.record(5.0);   // bucket <= 10
		h.record(15.0);  // bucket <= 20
		h.record(25.0);  // bucket <= 30
		h.record(35.0);  // bucket <= 40
		h.record(45.0);  // bucket <= 50
	}

	// p50 should be around 25 (middle of the distribution)
	double p50 = h.p50();
	EXPECT_GT(p50, 15.0);
	EXPECT_LT(p50, 35.0);

	// p99 should be near the higher end
	double p99 = h.p99();
	EXPECT_GT(p99, 35.0);
}

TEST(HistogramTest, Reset)
{
	histogram h;

	h.record(10.0);
	h.record(20.0);
	h.record(30.0);

	EXPECT_EQ(h.count(), 3);
	EXPECT_GT(h.sum(), 0.0);

	h.reset();

	EXPECT_EQ(h.count(), 0);
	EXPECT_DOUBLE_EQ(h.sum(), 0.0);
	EXPECT_EQ(h.min(), std::numeric_limits<double>::infinity());
	EXPECT_EQ(h.max(), -std::numeric_limits<double>::infinity());
}

TEST(HistogramTest, BucketsAreCumulative)
{
	histogram_config cfg;
	cfg.bucket_boundaries = {10.0, 20.0, 30.0};
	histogram h(cfg);

	h.record(5.0);   // bucket <= 10
	h.record(15.0);  // bucket <= 20
	h.record(25.0);  // bucket <= 30

	auto buckets = h.buckets();

	// Buckets should be cumulative
	ASSERT_GE(buckets.size(), 3);
	EXPECT_EQ(buckets[0].second, 1);  // <= 10: 1 value
	EXPECT_EQ(buckets[1].second, 2);  // <= 20: 2 values (cumulative)
	EXPECT_EQ(buckets[2].second, 3);  // <= 30: 3 values (cumulative)
}

// ============================================================================
// Histogram Snapshot Tests
// ============================================================================

TEST(HistogramSnapshotTest, SnapshotContainsAllData)
{
	histogram h;
	h.record(10.0);
	h.record(20.0);
	h.record(30.0);

	auto snap = h.snapshot({{"service", "test"}});

	EXPECT_EQ(snap.count, 3);
	EXPECT_DOUBLE_EQ(snap.sum, 60.0);
	EXPECT_DOUBLE_EQ(snap.min_value, 10.0);
	EXPECT_DOUBLE_EQ(snap.max_value, 30.0);
	EXPECT_FALSE(snap.buckets.empty());
	EXPECT_EQ(snap.labels.at("service"), "test");

	// Should have standard percentiles
	EXPECT_TRUE(snap.percentiles.count(0.5) > 0);
	EXPECT_TRUE(snap.percentiles.count(0.95) > 0);
	EXPECT_TRUE(snap.percentiles.count(0.99) > 0);
}

TEST(HistogramSnapshotTest, PrometheusFormat)
{
	histogram_config cfg;
	cfg.bucket_boundaries = {1.0, 5.0, 10.0};
	histogram h(cfg);

	h.record(0.5);
	h.record(3.0);
	h.record(7.0);

	auto snap = h.snapshot();
	std::string prom = snap.to_prometheus("test_latency");

	// Should contain bucket lines
	EXPECT_NE(prom.find("test_latency_bucket"), std::string::npos);
	// Bucket boundaries are formatted with precision, check for "le=\"1" prefix
	EXPECT_NE(prom.find("le=\"1"), std::string::npos);
	EXPECT_NE(prom.find("le=\"+Inf\""), std::string::npos);

	// Should contain sum and count
	EXPECT_NE(prom.find("test_latency_sum"), std::string::npos);
	EXPECT_NE(prom.find("test_latency_count"), std::string::npos);
}

TEST(HistogramSnapshotTest, JsonFormat)
{
	histogram h;
	h.record(10.0);
	h.record(20.0);

	auto snap = h.snapshot();
	std::string json = snap.to_json();

	// Should be valid JSON structure
	EXPECT_NE(json.find("\"count\":"), std::string::npos);
	EXPECT_NE(json.find("\"sum\":"), std::string::npos);
	EXPECT_NE(json.find("\"percentiles\":"), std::string::npos);
	EXPECT_NE(json.find("\"buckets\":"), std::string::npos);
}

// ============================================================================
// Histogram Thread Safety Tests
// ============================================================================

TEST(HistogramTest, ConcurrentRecording)
{
	histogram h;
	constexpr int kThreads = 8;
	constexpr int kIterations = 1000;

	std::vector<std::thread> threads;
	threads.reserve(kThreads);

	for (int i = 0; i < kThreads; ++i)
	{
		threads.emplace_back(
			[&h, i]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					h.record(static_cast<double>(i * 10 + j % 10));
				}
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(h.count(), kThreads * kIterations);
}

TEST(HistogramTest, ConcurrentReadsAndWrites)
{
	histogram h;
	constexpr int kWriters = 4;
	constexpr int kReaders = 4;
	constexpr int kIterations = 500;

	std::atomic<bool> stop{false};
	std::vector<std::thread> threads;

	// Writer threads
	for (int i = 0; i < kWriters; ++i)
	{
		threads.emplace_back(
			[&h, i]()
			{
				for (int j = 0; j < kIterations; ++j)
				{
					h.record(static_cast<double>(i + j));
				}
			});
	}

	// Reader threads
	for (int i = 0; i < kReaders; ++i)
	{
		threads.emplace_back(
			[&h, &stop]()
			{
				while (!stop.load())
				{
					(void)h.count();
					(void)h.p99();
					(void)h.snapshot();
				}
			});
	}

	// Wait for writers
	for (int i = 0; i < kWriters; ++i)
	{
		threads[i].join();
	}

	stop.store(true);

	// Wait for readers
	for (size_t i = kWriters; i < threads.size(); ++i)
	{
		threads[i].join();
	}

	EXPECT_EQ(h.count(), kWriters * kIterations);
}

// ============================================================================
// Sliding Histogram Tests
// ============================================================================

TEST(SlidingHistogramTest, DefaultConstruction)
{
	sliding_histogram sh;
	EXPECT_EQ(sh.count(), 0);
	EXPECT_EQ(sh.window_duration(), std::chrono::seconds{60});
}

TEST(SlidingHistogramTest, RecordValues)
{
	sliding_histogram sh;

	sh.record(10.0);
	sh.record(20.0);
	sh.record(30.0);

	EXPECT_EQ(sh.count(), 3);
	EXPECT_DOUBLE_EQ(sh.sum(), 60.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 20.0);
}

TEST(SlidingHistogramTest, PercentileCalculation)
{
	sliding_histogram_config cfg;
	cfg.hist_config.bucket_boundaries = {10.0, 20.0, 30.0, 40.0, 50.0};
	cfg.window_duration = std::chrono::seconds{60};
	cfg.bucket_count = 6;

	sliding_histogram sh(cfg);

	for (int i = 0; i < 100; ++i)
	{
		sh.record(static_cast<double>(i % 50));
	}

	// Should have reasonable percentiles
	double p50 = sh.p50();
	double p99 = sh.p99();

	EXPECT_GE(p50, 0.0);
	EXPECT_GE(p99, p50);
}

TEST(SlidingHistogramTest, Reset)
{
	sliding_histogram sh;

	sh.record(10.0);
	sh.record(20.0);
	EXPECT_EQ(sh.count(), 2);

	sh.reset();
	EXPECT_EQ(sh.count(), 0);
}

TEST(SlidingHistogramTest, Snapshot)
{
	sliding_histogram sh;
	sh.record(10.0);
	sh.record(20.0);

	auto snap = sh.snapshot({{"test", "value"}});

	EXPECT_EQ(snap.count, 2);
	EXPECT_EQ(snap.labels.at("test"), "value");
}

// ============================================================================
// metric_reporter Histogram Integration Tests
// ============================================================================

TEST(MetricReporterHistogramTest, RecordLatency)
{
	metric_reporter::reset_histograms();

	metric_reporter::record_latency(10.0);
	metric_reporter::record_latency(20.0);
	metric_reporter::record_latency(30.0);

	// Percentiles should be reasonable
	double p50 = metric_reporter::get_latency_p50();
	double p99 = metric_reporter::get_latency_p99();

	EXPECT_GE(p50, 0.0);
	EXPECT_GE(p99, 0.0);

	metric_reporter::reset_histograms();
}

TEST(MetricReporterHistogramTest, RecordConnectionTime)
{
	metric_reporter::reset_histograms();

	metric_reporter::record_connection_time(5.0);
	metric_reporter::record_connection_time(10.0);

	double p99 = metric_reporter::get_connection_time_p99();
	EXPECT_GE(p99, 0.0);

	metric_reporter::reset_histograms();
}

TEST(MetricReporterHistogramTest, RecordRequestDuration)
{
	metric_reporter::reset_histograms();

	metric_reporter::record_request_duration(100.0);
	metric_reporter::record_request_duration(200.0);

	double p99 = metric_reporter::get_request_duration_p99();
	EXPECT_GE(p99, 0.0);

	metric_reporter::reset_histograms();
}

TEST(MetricReporterHistogramTest, GetAllHistograms)
{
	metric_reporter::reset_histograms();

	metric_reporter::record_latency(10.0);
	metric_reporter::record_connection_time(5.0);
	metric_reporter::record_request_duration(100.0);

	auto histograms = metric_reporter::get_all_histograms();

	EXPECT_EQ(histograms.size(), 3);
	EXPECT_TRUE(histograms.count(metric_names::LATENCY_HISTOGRAM) > 0);
	EXPECT_TRUE(histograms.count(metric_names::CONNECTION_TIME_HISTOGRAM) > 0);
	EXPECT_TRUE(histograms.count(metric_names::REQUEST_DURATION_HISTOGRAM) > 0);

	metric_reporter::reset_histograms();
}

TEST(MetricReporterHistogramTest, ResetHistograms)
{
	metric_reporter::record_latency(10.0);
	metric_reporter::record_latency(20.0);

	auto before = metric_reporter::get_all_histograms();
	EXPECT_GT(before[metric_names::LATENCY_HISTOGRAM].count, 0);

	metric_reporter::reset_histograms();

	auto after = metric_reporter::get_all_histograms();
	EXPECT_EQ(after[metric_names::LATENCY_HISTOGRAM].count, 0);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(HistogramTest, VeryLargeValues)
{
	histogram h;

	h.record(1e10);
	h.record(1e11);
	h.record(1e12);

	EXPECT_EQ(h.count(), 3);
	EXPECT_GT(h.max(), 1e11);
}

TEST(HistogramTest, VerySmallValues)
{
	histogram h;

	h.record(0.001);
	h.record(0.0001);
	h.record(0.00001);

	EXPECT_EQ(h.count(), 3);
	EXPECT_LT(h.min(), 0.001);
}

TEST(HistogramTest, NegativeValues)
{
	histogram h;

	h.record(-10.0);
	h.record(-5.0);
	h.record(0.0);

	EXPECT_EQ(h.count(), 3);
	EXPECT_DOUBLE_EQ(h.min(), -10.0);
	EXPECT_DOUBLE_EQ(h.max(), 0.0);
}

TEST(HistogramTest, SingleValue)
{
	histogram h;
	h.record(42.0);

	EXPECT_EQ(h.count(), 1);
	EXPECT_DOUBLE_EQ(h.min(), 42.0);
	EXPECT_DOUBLE_EQ(h.max(), 42.0);
	EXPECT_DOUBLE_EQ(h.mean(), 42.0);
	// Percentile uses linear interpolation within bucket, so exact match not expected
	EXPECT_GT(h.p50(), 0.0);
}

TEST(HistogramTest, MoveConstruction)
{
	histogram h1;
	h1.record(10.0);
	h1.record(20.0);

	histogram h2(std::move(h1));

	EXPECT_EQ(h2.count(), 2);
	EXPECT_DOUBLE_EQ(h2.sum(), 30.0);
}

TEST(HistogramTest, MoveAssignment)
{
	histogram h1;
	h1.record(10.0);
	h1.record(20.0);

	histogram h2;
	h2 = std::move(h1);

	EXPECT_EQ(h2.count(), 2);
	EXPECT_DOUBLE_EQ(h2.sum(), 30.0);
}
