/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/utils/buffer_pool.h"
#include <gtest/gtest.h>

#include <thread>
#include <vector>

using namespace kcenon::network::utils;

/**
 * @file buffer_pool_test.cpp
 * @brief Unit tests for buffer_pool
 *
 * Tests validate:
 * - Construction with default and custom parameters
 * - Buffer acquisition and automatic return to pool
 * - Pool reuse behavior (buffers returned are reused)
 * - Statistics tracking (available count, total allocated)
 * - Clear operation releases cached buffers
 * - Pool size limit enforcement (excess buffers deleted)
 * - Minimum capacity selection during acquire
 * - Concurrent acquire/release safety
 */

// ============================================================================
// Construction Tests
// ============================================================================

class BufferPoolConstructionTest : public ::testing::Test
{
};

TEST_F(BufferPoolConstructionTest, ConstructsWithDefaultParameters)
{
	auto pool = std::make_shared<buffer_pool>();

	auto [available, total] = pool->get_stats();
	EXPECT_EQ(available, 0);
	EXPECT_EQ(total, 0);
}

TEST_F(BufferPoolConstructionTest, ConstructsWithCustomParameters)
{
	auto pool = std::make_shared<buffer_pool>(16, 4096);

	auto [available, total] = pool->get_stats();
	EXPECT_EQ(available, 0);
	EXPECT_EQ(total, 0);
}

TEST_F(BufferPoolConstructionTest, DestructorDoesNotDeadlock)
{
	{
		auto pool = std::make_shared<buffer_pool>(8, 1024);
		auto buf = pool->acquire();
	}
	SUCCEED();
}

// ============================================================================
// Acquire Tests
// ============================================================================

class BufferPoolAcquireTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(4, 1024); }
};

TEST_F(BufferPoolAcquireTest, AcquireReturnsNonNullBuffer)
{
	auto buffer = pool_->acquire();

	ASSERT_NE(buffer, nullptr);
}

TEST_F(BufferPoolAcquireTest, AcquiredBufferIsEmpty)
{
	auto buffer = pool_->acquire();

	EXPECT_TRUE(buffer->empty());
}

TEST_F(BufferPoolAcquireTest, AcquireWithMinCapacity)
{
	auto buffer = pool_->acquire(2048);

	ASSERT_NE(buffer, nullptr);
	EXPECT_GE(buffer->capacity(), 2048);
}

TEST_F(BufferPoolAcquireTest, AcquireWithZeroCapacityUsesDefault)
{
	auto buffer = pool_->acquire(0);

	ASSERT_NE(buffer, nullptr);
	EXPECT_GE(buffer->capacity(), 1024);
}

TEST_F(BufferPoolAcquireTest, AcquireIncrementsAllocatedCount)
{
	auto buffer = pool_->acquire();

	auto [available, total] = pool_->get_stats();
	EXPECT_EQ(total, 1);
}

TEST_F(BufferPoolAcquireTest, MultipleAcquiresTrackCorrectly)
{
	auto buf1 = pool_->acquire();
	auto buf2 = pool_->acquire();
	auto buf3 = pool_->acquire();

	auto [available, total] = pool_->get_stats();
	EXPECT_EQ(total, 3);
	EXPECT_EQ(available, 0);
}

// ============================================================================
// Release and Reuse Tests
// ============================================================================

class BufferPoolReuseTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(4, 1024); }
};

TEST_F(BufferPoolReuseTest, ReleasedBufferReturnsToPool)
{
	{
		auto buffer = pool_->acquire();
		buffer->resize(100);
		// buffer goes out of scope, should return to pool
	}

	auto [available, total] = pool_->get_stats();
	EXPECT_EQ(available, 1);
	EXPECT_EQ(total, 1);
}

TEST_F(BufferPoolReuseTest, ReusedBufferIsCleared)
{
	{
		auto buffer = pool_->acquire();
		buffer->resize(512);
		std::fill(buffer->begin(), buffer->end(), 0xAB);
	}

	auto buffer = pool_->acquire();
	EXPECT_TRUE(buffer->empty());
}

TEST_F(BufferPoolReuseTest, ReusedBufferRetainsCapacity)
{
	{
		auto buffer = pool_->acquire(2048);
		buffer->resize(100);
	}

	auto buffer = pool_->acquire(2048);
	ASSERT_NE(buffer, nullptr);
	EXPECT_GE(buffer->capacity(), 2048);
}

TEST_F(BufferPoolReuseTest, AcquireSkipsSmallBuffersInPool)
{
	// Return a small buffer to pool
	{
		auto buffer = pool_->acquire(64);
	}

	// Request a large buffer - should create new one since pooled buffer is too small
	auto buffer = pool_->acquire(4096);
	ASSERT_NE(buffer, nullptr);
	EXPECT_GE(buffer->capacity(), 4096);
}

// ============================================================================
// Pool Size Limit Tests
// ============================================================================

class BufferPoolLimitTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(2, 1024); }
};

TEST_F(BufferPoolLimitTest, ExcessBuffersAreDeleted)
{
	// Acquire 3 buffers (pool size is 2)
	auto buf1 = pool_->acquire();
	auto buf2 = pool_->acquire();
	auto buf3 = pool_->acquire();

	// Release all 3
	buf1.reset();
	buf2.reset();
	buf3.reset();

	auto [available, total] = pool_->get_stats();
	EXPECT_LE(available, 2);
}

TEST_F(BufferPoolLimitTest, PoolSizeZeroDeletesAllReturned)
{
	auto pool = std::make_shared<buffer_pool>(0, 1024);

	{
		auto buffer = pool->acquire();
		buffer->resize(100);
	}

	auto [available, total] = pool->get_stats();
	EXPECT_EQ(available, 0);
}

// ============================================================================
// Clear Tests
// ============================================================================

class BufferPoolClearTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(8, 1024); }
};

TEST_F(BufferPoolClearTest, ClearEmptyPoolIsNoOp)
{
	pool_->clear();

	auto [available, total] = pool_->get_stats();
	EXPECT_EQ(available, 0);
}

TEST_F(BufferPoolClearTest, ClearReleasesAllCachedBuffers)
{
	// Acquire and return several buffers
	{
		auto buf1 = pool_->acquire();
		auto buf2 = pool_->acquire();
		auto buf3 = pool_->acquire();
	}

	auto [before_available, before_total] = pool_->get_stats();
	EXPECT_EQ(before_available, 3);

	pool_->clear();

	auto [after_available, after_total] = pool_->get_stats();
	EXPECT_EQ(after_available, 0);
}

TEST_F(BufferPoolClearTest, AcquireWorksAfterClear)
{
	{
		auto buf = pool_->acquire();
	}

	pool_->clear();

	auto buffer = pool_->acquire();
	ASSERT_NE(buffer, nullptr);
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

class BufferPoolConcurrencyTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(32, 512); }
};

TEST_F(BufferPoolConcurrencyTest, ConcurrentAcquireRelease)
{
	constexpr int num_threads = 8;
	constexpr int ops_per_thread = 100;

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this]() {
			for (int i = 0; i < ops_per_thread; ++i)
			{
				auto buffer = pool_->acquire(256);
				ASSERT_NE(buffer, nullptr);
				buffer->resize(128);
				// buffer returned to pool on scope exit
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	SUCCEED();
}

TEST_F(BufferPoolConcurrencyTest, ConcurrentAcquireWithClear)
{
	constexpr int num_threads = 4;
	constexpr int ops_per_thread = 50;
	std::atomic<bool> stop{false};

	// Threads that acquire and release
	std::vector<std::thread> threads;
	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, &stop]() {
			while (!stop.load())
			{
				auto buffer = pool_->acquire();
				if (buffer)
				{
					buffer->resize(64);
				}
			}
		});
	}

	// Main thread clears the pool periodically
	for (int i = 0; i < ops_per_thread; ++i)
	{
		pool_->clear();
		std::this_thread::yield();
	}

	stop.store(true);
	for (auto& th : threads)
	{
		th.join();
	}

	SUCCEED();
}

TEST_F(BufferPoolConcurrencyTest, ConcurrentAcquireWithVaryingCapacities)
{
	constexpr int num_threads = 4;
	constexpr int ops_per_thread = 50;

	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, t]() {
			for (int i = 0; i < ops_per_thread; ++i)
			{
				size_t capacity = static_cast<size_t>((t * 100 + i) % 2048 + 64);
				auto buffer = pool_->acquire(capacity);
				ASSERT_NE(buffer, nullptr);
				EXPECT_GE(buffer->capacity(), capacity);
				buffer->resize(capacity / 2);
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	SUCCEED();
}

// ============================================================================
// Memory Limit Enforcement Tests
// ============================================================================

class BufferPoolMemoryTest : public ::testing::Test
{
};

TEST_F(BufferPoolMemoryTest, SmallPoolLimitsBufferCaching)
{
	auto pool = std::make_shared<buffer_pool>(1, 1024);

	// Acquire and return multiple buffers
	{
		auto buf1 = pool->acquire();
		auto buf2 = pool->acquire();
		buf1.reset();
		buf2.reset();
	}

	auto [available, total] = pool->get_stats();
	// Pool should cache at most 1 buffer
	EXPECT_LE(available, 1);
}

TEST_F(BufferPoolMemoryTest, LargeDefaultCapacity)
{
	auto pool = std::make_shared<buffer_pool>(4, 1024 * 1024);

	auto buffer = pool->acquire();
	ASSERT_NE(buffer, nullptr);
	EXPECT_GE(buffer->capacity(), 1024 * 1024);
}

TEST_F(BufferPoolMemoryTest, ZeroDefaultCapacityProducesUsableBuffer)
{
	auto pool = std::make_shared<buffer_pool>(4, 0);

	auto buffer = pool->acquire();
	ASSERT_NE(buffer, nullptr);
	// Buffer should still be usable (can resize)
	buffer->resize(100);
	EXPECT_EQ(buffer->size(), 100);
}

TEST_F(BufferPoolMemoryTest, RepeatedAcquireReleaseDoesNotGrowMemory)
{
	auto pool = std::make_shared<buffer_pool>(4, 1024);

	for (int i = 0; i < 100; ++i)
	{
		auto buffer = pool->acquire();
		buffer->resize(512);
	}

	auto [available, total] = pool->get_stats();
	// All buffers returned, available should not exceed pool size
	EXPECT_LE(available, 4);
}

// ============================================================================
// Buffer Reuse Validation Tests
// ============================================================================

class BufferPoolReuseValidationTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(8, 1024); }
};

TEST_F(BufferPoolReuseValidationTest, HighCapacityBufferReusedForSmallRequest)
{
	// Return a large buffer
	{
		auto buffer = pool_->acquire(4096);
		buffer->resize(100);
	}

	// Request a small buffer - should reuse the large one
	auto buffer = pool_->acquire(512);
	ASSERT_NE(buffer, nullptr);
	EXPECT_GE(buffer->capacity(), 512);
}

TEST_F(BufferPoolReuseValidationTest, BufferContentClearedOnReuse)
{
	const uint8_t pattern = 0xDE;
	{
		auto buffer = pool_->acquire();
		buffer->resize(256);
		std::fill(buffer->begin(), buffer->end(), pattern);
	}

	auto buffer = pool_->acquire();
	ASSERT_NE(buffer, nullptr);
	EXPECT_TRUE(buffer->empty());
}

TEST_F(BufferPoolReuseValidationTest, MultipleBuffersReturnedAndReused)
{
	constexpr int count = 5;

	// Acquire and return multiple buffers
	{
		std::vector<std::shared_ptr<std::vector<uint8_t>>> buffers;
		for (int i = 0; i < count; ++i)
		{
			auto buf = pool_->acquire();
			buf->resize(128);
			buffers.push_back(buf);
		}
	}

	auto [available, total] = pool_->get_stats();
	EXPECT_EQ(available, count);
	EXPECT_EQ(total, count);

	// Acquire them all again - should come from pool
	std::vector<std::shared_ptr<std::vector<uint8_t>>> reacquired;
	for (int i = 0; i < count; ++i)
	{
		auto buf = pool_->acquire();
		ASSERT_NE(buf, nullptr);
		EXPECT_TRUE(buf->empty());
		reacquired.push_back(buf);
	}

	auto [available2, total2] = pool_->get_stats();
	EXPECT_EQ(available2, 0);
}

// ============================================================================
// Statistics Accuracy Tests
// ============================================================================

class BufferPoolStatsTest : public ::testing::Test
{
protected:
	std::shared_ptr<buffer_pool> pool_;

	void SetUp() override { pool_ = std::make_shared<buffer_pool>(16, 512); }
};

TEST_F(BufferPoolStatsTest, StatsAccurateAfterAcquireSequence)
{
	auto buf1 = pool_->acquire();
	auto [avail1, total1] = pool_->get_stats();
	EXPECT_EQ(total1, 1);
	EXPECT_EQ(avail1, 0);

	auto buf2 = pool_->acquire();
	auto [avail2, total2] = pool_->get_stats();
	EXPECT_EQ(total2, 2);
	EXPECT_EQ(avail2, 0);
}

TEST_F(BufferPoolStatsTest, StatsAccurateAfterMixedOperations)
{
	auto buf1 = pool_->acquire();
	auto buf2 = pool_->acquire();
	auto buf3 = pool_->acquire();

	buf1.reset(); // Return to pool

	auto [avail, total] = pool_->get_stats();
	EXPECT_EQ(avail, 1);
	EXPECT_EQ(total, 3);

	buf2.reset();
	buf3.reset();

	auto [avail2, total2] = pool_->get_stats();
	EXPECT_EQ(avail2, 3);
	EXPECT_EQ(total2, 3);
}

TEST_F(BufferPoolStatsTest, StatsAccurateAfterClearAndReacquire)
{
	{
		auto buf = pool_->acquire();
	}

	pool_->clear();

	auto [avail, total] = pool_->get_stats();
	EXPECT_EQ(avail, 0);

	auto buf = pool_->acquire();
	auto [avail2, total2] = pool_->get_stats();
	EXPECT_GE(total2, 1);
}
