/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file sliding_histogram_test.cpp
 * @brief Unit tests for sliding_histogram class
 *
 * Tests validate:
 * - Construction with various configurations
 * - Recording values and querying statistics
 * - Percentile calculations (p50, p95, p99, p999)
 * - Snapshot generation and aggregation
 * - Reset behavior
 * - Move semantics
 * - Thread safety under concurrent access
 * - Zero bucket count edge case
 * - Empty histogram behavior
 * - Window duration configuration
 */

#include "kcenon/network/detail/metrics/sliding_histogram.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <thread>
#include <vector>

using namespace kcenon::network::metrics;

// ============================================================================
// Construction Tests
// ============================================================================

class SlidingHistogramConstructionTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramConstructionTest, DefaultConstruction)
{
	sliding_histogram sh;

	EXPECT_EQ(sh.count(), 0);
	EXPECT_DOUBLE_EQ(sh.sum(), 0.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 0.0);
	EXPECT_EQ(sh.window_duration(), std::chrono::seconds{60});
}

TEST_F(SlidingHistogramConstructionTest, ConstructWithDefaultConfig)
{
	auto cfg = sliding_histogram_config::default_config();
	sliding_histogram sh(cfg);

	EXPECT_EQ(sh.count(), 0);
	EXPECT_EQ(sh.window_duration(), std::chrono::seconds{60});
}

TEST_F(SlidingHistogramConstructionTest, ConstructWithCustomConfig)
{
	sliding_histogram_config cfg;
	cfg.window_duration = std::chrono::seconds{120};
	cfg.bucket_count = 12;
	cfg.hist_config.bucket_boundaries = {1.0, 5.0, 10.0, 50.0, 100.0};

	sliding_histogram sh(cfg);

	EXPECT_EQ(sh.count(), 0);
	EXPECT_EQ(sh.window_duration(), std::chrono::seconds{120});
}

TEST_F(SlidingHistogramConstructionTest, ConstructWithZeroBucketCount)
{
	sliding_histogram_config cfg;
	cfg.bucket_count = 0;
	cfg.window_duration = std::chrono::seconds{30};

	// Zero bucket count should be corrected to 1
	sliding_histogram sh(cfg);

	// Should still work correctly
	sh.record(5.0);
	EXPECT_EQ(sh.count(), 1);
	EXPECT_DOUBLE_EQ(sh.sum(), 5.0);
}

TEST_F(SlidingHistogramConstructionTest, ConstructWithSingleBucket)
{
	sliding_histogram_config cfg;
	cfg.bucket_count = 1;
	cfg.window_duration = std::chrono::seconds{10};

	sliding_histogram sh(cfg);

	sh.record(10.0);
	EXPECT_EQ(sh.count(), 1);
}

TEST_F(SlidingHistogramConstructionTest, ConstructWithLargeBucketCount)
{
	sliding_histogram_config cfg;
	cfg.bucket_count = 1000;
	cfg.window_duration = std::chrono::seconds{60};

	sliding_histogram sh(cfg);

	sh.record(42.0);
	EXPECT_EQ(sh.count(), 1);
	EXPECT_DOUBLE_EQ(sh.sum(), 42.0);
}

// ============================================================================
// Recording and Statistics Tests
// ============================================================================

class SlidingHistogramRecordTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramRecordTest, RecordSingleValue)
{
	sliding_histogram sh;

	sh.record(10.0);

	EXPECT_EQ(sh.count(), 1);
	EXPECT_DOUBLE_EQ(sh.sum(), 10.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 10.0);
}

TEST_F(SlidingHistogramRecordTest, RecordMultipleValues)
{
	sliding_histogram sh;

	sh.record(10.0);
	sh.record(20.0);
	sh.record(30.0);

	EXPECT_EQ(sh.count(), 3);
	EXPECT_DOUBLE_EQ(sh.sum(), 60.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 20.0);
}

TEST_F(SlidingHistogramRecordTest, RecordZeroValue)
{
	sliding_histogram sh;

	sh.record(0.0);

	EXPECT_EQ(sh.count(), 1);
	EXPECT_DOUBLE_EQ(sh.sum(), 0.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 0.0);
}

TEST_F(SlidingHistogramRecordTest, RecordNegativeValue)
{
	sliding_histogram sh;

	sh.record(-5.0);
	sh.record(15.0);

	EXPECT_EQ(sh.count(), 2);
	EXPECT_DOUBLE_EQ(sh.sum(), 10.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 5.0);
}

TEST_F(SlidingHistogramRecordTest, RecordLargeNumberOfValues)
{
	sliding_histogram sh;

	for (int i = 0; i < 1000; ++i)
	{
		sh.record(static_cast<double>(i));
	}

	EXPECT_EQ(sh.count(), 1000);
	// Sum of 0..999 = 999*1000/2 = 499500
	EXPECT_DOUBLE_EQ(sh.sum(), 499500.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 499.5);
}

TEST_F(SlidingHistogramRecordTest, RecordVerySmallValues)
{
	sliding_histogram sh;

	sh.record(0.001);
	sh.record(0.002);
	sh.record(0.003);

	EXPECT_EQ(sh.count(), 3);
	EXPECT_NEAR(sh.sum(), 0.006, 1e-10);
}

TEST_F(SlidingHistogramRecordTest, RecordVeryLargeValues)
{
	sliding_histogram sh;

	sh.record(1e9);
	sh.record(2e9);

	EXPECT_EQ(sh.count(), 2);
	EXPECT_DOUBLE_EQ(sh.sum(), 3e9);
}

// ============================================================================
// Percentile Tests
// ============================================================================

class SlidingHistogramPercentileTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramPercentileTest, EmptyHistogramPercentiles)
{
	sliding_histogram sh;

	EXPECT_DOUBLE_EQ(sh.p50(), 0.0);
	EXPECT_DOUBLE_EQ(sh.p95(), 0.0);
	EXPECT_DOUBLE_EQ(sh.p99(), 0.0);
	EXPECT_DOUBLE_EQ(sh.p999(), 0.0);
}

TEST_F(SlidingHistogramPercentileTest, SingleValuePercentiles)
{
	sliding_histogram_config cfg;
	cfg.hist_config.bucket_boundaries = {10.0, 20.0, 30.0, 40.0, 50.0};
	cfg.window_duration = std::chrono::seconds{60};
	cfg.bucket_count = 6;

	sliding_histogram sh(cfg);
	sh.record(15.0);

	// All percentiles should return something reasonable for single value
	EXPECT_GE(sh.p50(), 0.0);
	EXPECT_GE(sh.p99(), 0.0);
}

TEST_F(SlidingHistogramPercentileTest, DistributedValuesPercentiles)
{
	sliding_histogram_config cfg;
	cfg.hist_config.bucket_boundaries = {10.0, 20.0, 30.0, 40.0, 50.0};
	cfg.window_duration = std::chrono::seconds{60};
	cfg.bucket_count = 6;

	sliding_histogram sh(cfg);

	// Add values across the distribution
	for (int i = 0; i < 20; ++i)
	{
		sh.record(5.0);   // bucket <= 10
		sh.record(15.0);  // bucket <= 20
		sh.record(25.0);  // bucket <= 30
		sh.record(35.0);  // bucket <= 40
		sh.record(45.0);  // bucket <= 50
	}

	// p50 should be near the middle
	double p50 = sh.p50();
	EXPECT_GT(p50, 10.0);
	EXPECT_LT(p50, 40.0);

	// p99 should be near the upper end
	double p99 = sh.p99();
	EXPECT_GT(p99, 30.0);
}

TEST_F(SlidingHistogramPercentileTest, PercentileBoundaryValues)
{
	sliding_histogram sh;

	sh.record(50.0);

	// Percentile with 0.0 and 1.0
	double p0 = sh.percentile(0.0);
	double p100 = sh.percentile(1.0);

	// Should not crash and return valid values
	EXPECT_GE(p0, 0.0);
	EXPECT_GE(p100, 0.0);
}

TEST_F(SlidingHistogramPercentileTest, PercentileClampingBelowZero)
{
	sliding_histogram sh;
	sh.record(10.0);

	// Negative percentile should be clamped to 0
	double result = sh.percentile(-0.5);
	EXPECT_GE(result, 0.0);
}

TEST_F(SlidingHistogramPercentileTest, PercentileClampingAboveOne)
{
	sliding_histogram sh;
	sh.record(10.0);

	// Percentile > 1.0 should be clamped to 1.0
	double result = sh.percentile(1.5);
	EXPECT_GE(result, 0.0);
}

// ============================================================================
// Snapshot Tests
// ============================================================================

class SlidingHistogramSnapshotTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramSnapshotTest, EmptySnapshot)
{
	sliding_histogram sh;

	auto snap = sh.snapshot();

	EXPECT_EQ(snap.count, 0);
	EXPECT_DOUBLE_EQ(snap.sum, 0.0);
}

TEST_F(SlidingHistogramSnapshotTest, SnapshotWithData)
{
	sliding_histogram sh;

	sh.record(10.0);
	sh.record(20.0);
	sh.record(30.0);

	auto snap = sh.snapshot();

	EXPECT_EQ(snap.count, 3);
	EXPECT_DOUBLE_EQ(snap.sum, 60.0);
	EXPECT_LE(snap.min_value, 10.0);
	EXPECT_GE(snap.max_value, 30.0);
}

TEST_F(SlidingHistogramSnapshotTest, SnapshotWithLabels)
{
	sliding_histogram sh;
	sh.record(5.0);

	auto snap = sh.snapshot({{"service", "test"}, {"endpoint", "/api"}});

	EXPECT_EQ(snap.labels.at("service"), "test");
	EXPECT_EQ(snap.labels.at("endpoint"), "/api");
	EXPECT_EQ(snap.count, 1);
}

TEST_F(SlidingHistogramSnapshotTest, SnapshotContainsPercentiles)
{
	sliding_histogram sh;

	for (int i = 0; i < 100; ++i)
	{
		sh.record(static_cast<double>(i));
	}

	auto snap = sh.snapshot();

	EXPECT_EQ(snap.count, 100);
	EXPECT_TRUE(snap.percentiles.count(0.5) > 0);
	EXPECT_TRUE(snap.percentiles.count(0.9) > 0);
	EXPECT_TRUE(snap.percentiles.count(0.95) > 0);
	EXPECT_TRUE(snap.percentiles.count(0.99) > 0);
	EXPECT_TRUE(snap.percentiles.count(0.999) > 0);
}

TEST_F(SlidingHistogramSnapshotTest, SnapshotBucketsNotEmpty)
{
	sliding_histogram sh;

	sh.record(1.0);
	sh.record(50.0);
	sh.record(500.0);

	auto snap = sh.snapshot();

	EXPECT_FALSE(snap.buckets.empty());
}

// ============================================================================
// Reset Tests
// ============================================================================

class SlidingHistogramResetTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramResetTest, ResetClearsAllData)
{
	sliding_histogram sh;

	sh.record(10.0);
	sh.record(20.0);
	sh.record(30.0);

	EXPECT_EQ(sh.count(), 3);

	sh.reset();

	EXPECT_EQ(sh.count(), 0);
	EXPECT_DOUBLE_EQ(sh.sum(), 0.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 0.0);
}

TEST_F(SlidingHistogramResetTest, RecordAfterReset)
{
	sliding_histogram sh;

	sh.record(100.0);
	sh.reset();
	sh.record(50.0);

	EXPECT_EQ(sh.count(), 1);
	EXPECT_DOUBLE_EQ(sh.sum(), 50.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 50.0);
}

TEST_F(SlidingHistogramResetTest, MultipleResets)
{
	sliding_histogram sh;

	for (int round = 0; round < 5; ++round)
	{
		for (int i = 0; i < 10; ++i)
		{
			sh.record(static_cast<double>(i));
		}
		EXPECT_EQ(sh.count(), 10);

		sh.reset();
		EXPECT_EQ(sh.count(), 0);
	}
}

TEST_F(SlidingHistogramResetTest, ResetEmptyHistogram)
{
	sliding_histogram sh;

	// Reset on empty should not crash
	sh.reset();

	EXPECT_EQ(sh.count(), 0);
	EXPECT_DOUBLE_EQ(sh.sum(), 0.0);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

class SlidingHistogramMoveTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramMoveTest, MoveConstruction)
{
	sliding_histogram sh1;
	sh1.record(10.0);
	sh1.record(20.0);

	sliding_histogram sh2(std::move(sh1));

	EXPECT_EQ(sh2.count(), 2);
	EXPECT_DOUBLE_EQ(sh2.sum(), 30.0);
}

TEST_F(SlidingHistogramMoveTest, MoveAssignment)
{
	sliding_histogram sh1;
	sh1.record(10.0);
	sh1.record(20.0);

	sliding_histogram sh2;
	sh2.record(100.0);

	sh2 = std::move(sh1);

	EXPECT_EQ(sh2.count(), 2);
	EXPECT_DOUBLE_EQ(sh2.sum(), 30.0);
}

TEST_F(SlidingHistogramMoveTest, SelfMoveAssignment)
{
	sliding_histogram sh;
	sh.record(10.0);

	// Self-move should not crash
	auto& ref = sh;
	sh = std::move(ref);

	// State may be valid or not, but should not crash
	SUCCEED();
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class SlidingHistogramThreadSafetyTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramThreadSafetyTest, ConcurrentRecording)
{
	sliding_histogram sh;

	constexpr int num_threads = 8;
	constexpr int records_per_thread = 100;

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([&sh, t]() {
			for (int i = 0; i < records_per_thread; ++i)
			{
				sh.record(static_cast<double>(t * records_per_thread + i));
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	EXPECT_EQ(sh.count(), num_threads * records_per_thread);
}

TEST_F(SlidingHistogramThreadSafetyTest, ConcurrentRecordAndRead)
{
	sliding_histogram sh;

	constexpr int num_writers = 4;
	constexpr int num_readers = 4;
	constexpr int iterations = 100;

	std::vector<std::thread> threads;

	// Writers
	for (int t = 0; t < num_writers; ++t)
	{
		threads.emplace_back([&sh]() {
			for (int i = 0; i < iterations; ++i)
			{
				sh.record(static_cast<double>(i));
			}
		});
	}

	// Readers
	for (int t = 0; t < num_readers; ++t)
	{
		threads.emplace_back([&sh]() {
			for (int i = 0; i < iterations; ++i)
			{
				(void)sh.count();
				(void)sh.sum();
				(void)sh.mean();
				(void)sh.p50();
				(void)sh.p99();
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	EXPECT_EQ(sh.count(), static_cast<uint64_t>(num_writers * iterations));
}

TEST_F(SlidingHistogramThreadSafetyTest, ConcurrentRecordAndSnapshot)
{
	sliding_histogram sh;

	constexpr int num_threads = 4;
	constexpr int iterations = 50;

	std::vector<std::thread> threads;

	// Writers
	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([&sh]() {
			for (int i = 0; i < iterations; ++i)
			{
				sh.record(static_cast<double>(i));
			}
		});
	}

	// Snapshot readers
	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([&sh]() {
			for (int i = 0; i < iterations; ++i)
			{
				auto snap = sh.snapshot();
				(void)snap.count;
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	EXPECT_EQ(sh.count(), static_cast<uint64_t>(num_threads * iterations));
}

TEST_F(SlidingHistogramThreadSafetyTest, ConcurrentRecordAndReset)
{
	sliding_histogram sh;

	constexpr int num_writers = 4;
	constexpr int iterations = 100;

	std::vector<std::thread> threads;

	// Writers
	for (int t = 0; t < num_writers; ++t)
	{
		threads.emplace_back([&sh]() {
			for (int i = 0; i < iterations; ++i)
			{
				sh.record(static_cast<double>(i));
			}
		});
	}

	// Resetter
	threads.emplace_back([&sh]() {
		for (int i = 0; i < 10; ++i)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			sh.reset();
		}
	});

	for (auto& th : threads)
	{
		th.join();
	}

	// Count might be anything due to resets, but should not crash
	SUCCEED();
}

// ============================================================================
// Window Duration Tests
// ============================================================================

class SlidingHistogramWindowTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramWindowTest, WindowDurationPreserved)
{
	sliding_histogram_config cfg;
	cfg.window_duration = std::chrono::seconds{30};
	cfg.bucket_count = 3;

	sliding_histogram sh(cfg);

	EXPECT_EQ(sh.window_duration(), std::chrono::seconds{30});
}

TEST_F(SlidingHistogramWindowTest, ShortWindowDuration)
{
	sliding_histogram_config cfg;
	cfg.window_duration = std::chrono::seconds{1};
	cfg.bucket_count = 2;

	sliding_histogram sh(cfg);

	sh.record(10.0);
	EXPECT_EQ(sh.count(), 1);
}

TEST_F(SlidingHistogramWindowTest, LongWindowDuration)
{
	sliding_histogram_config cfg;
	cfg.window_duration = std::chrono::seconds{3600};
	cfg.bucket_count = 60;

	sliding_histogram sh(cfg);

	sh.record(10.0);
	sh.record(20.0);

	EXPECT_EQ(sh.count(), 2);
	EXPECT_DOUBLE_EQ(sh.sum(), 30.0);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

class SlidingHistogramEdgeCaseTest : public ::testing::Test
{
};

TEST_F(SlidingHistogramEdgeCaseTest, MeanOfEmptyHistogram)
{
	sliding_histogram sh;

	EXPECT_DOUBLE_EQ(sh.mean(), 0.0);
}

TEST_F(SlidingHistogramEdgeCaseTest, PercentileOfEmptyHistogram)
{
	sliding_histogram sh;

	EXPECT_DOUBLE_EQ(sh.percentile(0.5), 0.0);
	EXPECT_DOUBLE_EQ(sh.percentile(0.0), 0.0);
	EXPECT_DOUBLE_EQ(sh.percentile(1.0), 0.0);
}

TEST_F(SlidingHistogramEdgeCaseTest, SnapshotOfEmptyHistogramHasSaneDefaults)
{
	sliding_histogram sh;

	auto snap = sh.snapshot();

	EXPECT_EQ(snap.count, 0);
	EXPECT_DOUBLE_EQ(snap.sum, 0.0);
	// min/max should be infinity defaults
	EXPECT_EQ(snap.min_value, std::numeric_limits<double>::infinity());
	EXPECT_EQ(snap.max_value, -std::numeric_limits<double>::infinity());
}

TEST_F(SlidingHistogramEdgeCaseTest, RecordIdenticalValues)
{
	sliding_histogram sh;

	for (int i = 0; i < 100; ++i)
	{
		sh.record(42.0);
	}

	EXPECT_EQ(sh.count(), 100);
	EXPECT_DOUBLE_EQ(sh.sum(), 4200.0);
	EXPECT_DOUBLE_EQ(sh.mean(), 42.0);
}

TEST_F(SlidingHistogramEdgeCaseTest, RecordInfinityValue)
{
	sliding_histogram sh;

	sh.record(std::numeric_limits<double>::infinity());

	EXPECT_EQ(sh.count(), 1);
}

TEST_F(SlidingHistogramEdgeCaseTest, RecordMaxDoubleValue)
{
	sliding_histogram sh;

	sh.record(std::numeric_limits<double>::max());

	EXPECT_EQ(sh.count(), 1);
}
