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
