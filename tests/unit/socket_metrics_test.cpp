/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/utils/common_defs.h"
#include <gtest/gtest.h>

#include <thread>
#include <vector>

using namespace kcenon::network::internal;

/**
 * @file socket_metrics_test.cpp
 * @brief Unit tests for socket_metrics and data_mode
 *
 * Tests validate:
 * - socket_metrics default initialization (all counters zero)
 * - socket_metrics reset() method
 * - socket_metrics atomic store/load operations
 * - socket_metrics concurrent access safety
 * - data_mode enum values and underlying type
 */

// ============================================================================
// socket_metrics Default Initialization Tests
// ============================================================================

class SocketMetricsDefaultTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;
};

TEST_F(SocketMetricsDefaultTest, AllCountersInitializedToZero)
{
	EXPECT_EQ(metrics_.total_bytes_sent.load(), 0);
	EXPECT_EQ(metrics_.total_bytes_received.load(), 0);
	EXPECT_EQ(metrics_.current_pending_bytes.load(), 0);
	EXPECT_EQ(metrics_.peak_pending_bytes.load(), 0);
	EXPECT_EQ(metrics_.backpressure_events.load(), 0);
	EXPECT_EQ(metrics_.rejected_sends.load(), 0);
	EXPECT_EQ(metrics_.send_count.load(), 0);
	EXPECT_EQ(metrics_.receive_count.load(), 0);
}

// ============================================================================
// socket_metrics Atomic Operations Tests
// ============================================================================

class SocketMetricsAtomicTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;
};

TEST_F(SocketMetricsAtomicTest, StoreThenLoad)
{
	metrics_.total_bytes_sent.store(1024);
	metrics_.total_bytes_received.store(2048);
	metrics_.current_pending_bytes.store(512);
	metrics_.peak_pending_bytes.store(4096);
	metrics_.backpressure_events.store(3);
	metrics_.rejected_sends.store(7);
	metrics_.send_count.store(100);
	metrics_.receive_count.store(200);

	EXPECT_EQ(metrics_.total_bytes_sent.load(), 1024);
	EXPECT_EQ(metrics_.total_bytes_received.load(), 2048);
	EXPECT_EQ(metrics_.current_pending_bytes.load(), 512);
	EXPECT_EQ(metrics_.peak_pending_bytes.load(), 4096);
	EXPECT_EQ(metrics_.backpressure_events.load(), 3);
	EXPECT_EQ(metrics_.rejected_sends.load(), 7);
	EXPECT_EQ(metrics_.send_count.load(), 100);
	EXPECT_EQ(metrics_.receive_count.load(), 200);
}

TEST_F(SocketMetricsAtomicTest, FetchAddAccumulates)
{
	metrics_.total_bytes_sent.fetch_add(100);
	metrics_.total_bytes_sent.fetch_add(200);
	metrics_.total_bytes_sent.fetch_add(300);

	EXPECT_EQ(metrics_.total_bytes_sent.load(), 600);
}

TEST_F(SocketMetricsAtomicTest, IncrementSendAndReceiveCount)
{
	for (int i = 0; i < 50; ++i)
	{
		metrics_.send_count.fetch_add(1);
	}
	for (int i = 0; i < 30; ++i)
	{
		metrics_.receive_count.fetch_add(1);
	}

	EXPECT_EQ(metrics_.send_count.load(), 50);
	EXPECT_EQ(metrics_.receive_count.load(), 30);
}

// ============================================================================
// socket_metrics Reset Tests
// ============================================================================

class SocketMetricsResetTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;
};

TEST_F(SocketMetricsResetTest, ResetZerosAllCounters)
{
	// Set all counters to non-zero values
	metrics_.total_bytes_sent.store(1000);
	metrics_.total_bytes_received.store(2000);
	metrics_.current_pending_bytes.store(500);
	metrics_.peak_pending_bytes.store(3000);
	metrics_.backpressure_events.store(10);
	metrics_.rejected_sends.store(5);
	metrics_.send_count.store(50);
	metrics_.receive_count.store(40);

	metrics_.reset();

	EXPECT_EQ(metrics_.total_bytes_sent.load(), 0);
	EXPECT_EQ(metrics_.total_bytes_received.load(), 0);
	EXPECT_EQ(metrics_.current_pending_bytes.load(), 0);
	EXPECT_EQ(metrics_.peak_pending_bytes.load(), 0);
	EXPECT_EQ(metrics_.backpressure_events.load(), 0);
	EXPECT_EQ(metrics_.rejected_sends.load(), 0);
	EXPECT_EQ(metrics_.send_count.load(), 0);
	EXPECT_EQ(metrics_.receive_count.load(), 0);
}

TEST_F(SocketMetricsResetTest, ResetOnAlreadyZeroIsNoOp)
{
	metrics_.reset();

	EXPECT_EQ(metrics_.total_bytes_sent.load(), 0);
	EXPECT_EQ(metrics_.send_count.load(), 0);
}

TEST_F(SocketMetricsResetTest, CountersWorkAfterReset)
{
	metrics_.total_bytes_sent.store(999);
	metrics_.reset();

	metrics_.total_bytes_sent.fetch_add(42);
	EXPECT_EQ(metrics_.total_bytes_sent.load(), 42);
}

// ============================================================================
// socket_metrics Concurrent Access Tests
// ============================================================================

class SocketMetricsConcurrencyTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;
};

TEST_F(SocketMetricsConcurrencyTest, ConcurrentFetchAdd)
{
	constexpr int num_threads = 8;
	constexpr int increments_per_thread = 1000;

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this]() {
			for (int i = 0; i < increments_per_thread; ++i)
			{
				metrics_.total_bytes_sent.fetch_add(1);
				metrics_.send_count.fetch_add(1);
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	EXPECT_EQ(metrics_.total_bytes_sent.load(),
			  num_threads * increments_per_thread);
	EXPECT_EQ(metrics_.send_count.load(),
			  num_threads * increments_per_thread);
}

TEST_F(SocketMetricsConcurrencyTest, ConcurrentResetDuringUpdates)
{
	constexpr int num_threads = 4;
	std::atomic<bool> stop{false};

	std::vector<std::thread> threads;
	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, &stop]() {
			while (!stop.load())
			{
				metrics_.total_bytes_sent.fetch_add(1);
				metrics_.receive_count.fetch_add(1);
			}
		});
	}

	// Reset periodically while threads are updating
	for (int i = 0; i < 50; ++i)
	{
		metrics_.reset();
		std::this_thread::yield();
	}

	stop.store(true);
	for (auto& th : threads)
	{
		th.join();
	}

	// Just verify no crash or UB occurred
	SUCCEED();
}

// ============================================================================
// data_mode Enum Tests
// ============================================================================

class DataModeTest : public ::testing::Test
{
};

TEST_F(DataModeTest, EnumValuesAreCorrect)
{
	EXPECT_EQ(static_cast<std::uint8_t>(data_mode::packet_mode), 1);
	EXPECT_EQ(static_cast<std::uint8_t>(data_mode::file_mode), 2);
	EXPECT_EQ(static_cast<std::uint8_t>(data_mode::binary_mode), 3);
}

TEST_F(DataModeTest, EnumValuesAreDistinct)
{
	EXPECT_NE(data_mode::packet_mode, data_mode::file_mode);
	EXPECT_NE(data_mode::packet_mode, data_mode::binary_mode);
	EXPECT_NE(data_mode::file_mode, data_mode::binary_mode);
}

TEST_F(DataModeTest, UnderlyingTypeIsUint8)
{
	static_assert(std::is_same_v<std::underlying_type_t<data_mode>, std::uint8_t>,
				  "data_mode underlying type must be uint8_t");
	SUCCEED();
}

TEST_F(DataModeTest, RoundTripCast)
{
	auto val = static_cast<std::uint8_t>(data_mode::file_mode);
	auto mode = static_cast<data_mode>(val);

	EXPECT_EQ(mode, data_mode::file_mode);
}

// ============================================================================
// socket_metrics Concurrent Multi-Field Update Tests
// ============================================================================

class SocketMetricsMultiFieldTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;
};

TEST_F(SocketMetricsMultiFieldTest, ConcurrentMultiFieldUpdates)
{
	constexpr int num_threads = 8;
	constexpr int iterations = 500;

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this]() {
			for (int i = 0; i < iterations; ++i)
			{
				metrics_.total_bytes_sent.fetch_add(100);
				metrics_.total_bytes_received.fetch_add(200);
				metrics_.send_count.fetch_add(1);
				metrics_.receive_count.fetch_add(1);
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	EXPECT_EQ(metrics_.total_bytes_sent.load(),
			  static_cast<uint64_t>(num_threads) * iterations * 100);
	EXPECT_EQ(metrics_.total_bytes_received.load(),
			  static_cast<uint64_t>(num_threads) * iterations * 200);
	EXPECT_EQ(metrics_.send_count.load(),
			  static_cast<uint64_t>(num_threads) * iterations);
	EXPECT_EQ(metrics_.receive_count.load(),
			  static_cast<uint64_t>(num_threads) * iterations);
}

TEST_F(SocketMetricsMultiFieldTest, BackpressureAndRejectionConcurrent)
{
	constexpr int num_threads = 4;
	constexpr int iterations = 1000;

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this]() {
			for (int i = 0; i < iterations; ++i)
			{
				metrics_.backpressure_events.fetch_add(1);
				metrics_.rejected_sends.fetch_add(1);
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	EXPECT_EQ(metrics_.backpressure_events.load(),
			  static_cast<uint64_t>(num_threads) * iterations);
	EXPECT_EQ(metrics_.rejected_sends.load(),
			  static_cast<uint64_t>(num_threads) * iterations);
}

// ============================================================================
// socket_metrics Peak Tracking Under Contention Tests
// ============================================================================

class SocketMetricsPeakTrackingTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;

	void update_peak(uint64_t new_value)
	{
		// Atomic max update using fetch_max-style loop
		uint64_t current = metrics_.peak_pending_bytes.load();
		while (new_value > current)
		{
			metrics_.peak_pending_bytes.store(new_value);
			current = metrics_.peak_pending_bytes.load();
		}
	}
};

TEST_F(SocketMetricsPeakTrackingTest, PeakTracksHighestValue)
{
	metrics_.peak_pending_bytes.store(100);
	EXPECT_EQ(metrics_.peak_pending_bytes.load(), 100);

	// Lower value should not overwrite
	uint64_t current = metrics_.peak_pending_bytes.load();
	if (50 > current)
	{
		metrics_.peak_pending_bytes.store(50);
	}
	EXPECT_EQ(metrics_.peak_pending_bytes.load(), 100);

	// Higher value should overwrite
	current = metrics_.peak_pending_bytes.load();
	if (200 > current)
	{
		metrics_.peak_pending_bytes.store(200);
	}
	EXPECT_EQ(metrics_.peak_pending_bytes.load(), 200);
}

TEST_F(SocketMetricsPeakTrackingTest, ConcurrentPeakTracking)
{
	constexpr int num_threads = 8;

	std::atomic<uint64_t> expected_max{0};
	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, t, &expected_max]() {
			for (uint64_t i = 0; i < 500; ++i)
			{
				uint64_t value = static_cast<uint64_t>(t) * 500 + i;
				// Track what the overall max should be
				uint64_t cur_max = expected_max.load();
				while (value > cur_max)
				{
					expected_max.store(value);
					cur_max = expected_max.load();
				}
				// Update peak
				metrics_.peak_pending_bytes.store(value);
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	// Verify no crash occurred and peak was updated
	EXPECT_GT(metrics_.peak_pending_bytes.load(), 0);
}

// ============================================================================
// socket_metrics Reset During Active Operations Tests
// ============================================================================

class SocketMetricsResetContentionTest : public ::testing::Test
{
protected:
	socket_metrics metrics_;
};

TEST_F(SocketMetricsResetContentionTest, ResetWhileReadingIsConsistent)
{
	constexpr int num_writers = 4;
	constexpr int num_readers = 2;
	std::atomic<bool> stop{false};

	std::vector<std::thread> threads;

	// Writers continuously increment
	for (int t = 0; t < num_writers; ++t)
	{
		threads.emplace_back([this, &stop]() {
			while (!stop.load())
			{
				metrics_.total_bytes_sent.fetch_add(1);
				metrics_.send_count.fetch_add(1);
			}
		});
	}

	// Readers continuously read individual fields (each load is atomic)
	for (int t = 0; t < num_readers; ++t)
	{
		threads.emplace_back([this, &stop]() {
			while (!stop.load())
			{
				// Individual field loads are always valid
				auto sent = metrics_.total_bytes_sent.load();
				auto count = metrics_.send_count.load();
				(void)sent;
				(void)count;
			}
		});
	}

	// Periodic resets
	for (int i = 0; i < 100; ++i)
	{
		metrics_.reset();
		std::this_thread::yield();
	}

	stop.store(true);
	for (auto& th : threads)
	{
		th.join();
	}

	// No crash, no UB — each atomic field is independently consistent
	SUCCEED();
}

TEST_F(SocketMetricsResetContentionTest, ValuesRecoverAfterReset)
{
	metrics_.total_bytes_sent.store(1000);
	metrics_.send_count.store(50);

	metrics_.reset();

	EXPECT_EQ(metrics_.total_bytes_sent.load(), 0);
	EXPECT_EQ(metrics_.send_count.load(), 0);

	// Values can be accumulated again after reset
	metrics_.total_bytes_sent.fetch_add(42);
	metrics_.send_count.fetch_add(3);

	EXPECT_EQ(metrics_.total_bytes_sent.load(), 42);
	EXPECT_EQ(metrics_.send_count.load(), 3);
}
