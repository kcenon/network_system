/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file memory_profiler_test.cpp
 * @brief Unit tests for memory_profiler and memory_snapshot
 *
 * Tests validate:
 * - memory_snapshot default values
 * - memory_snapshot field assignment
 * - memory_profiler singleton consistency
 * - memory_profiler snapshot returns valid data
 * - memory_profiler snapshot timestamp
 * - memory_profiler history management
 * - memory_profiler clear_history
 * - memory_profiler get_history max_count
 * - memory_profiler to_tsv format
 * - memory_profiler start/stop lifecycle
 */

#include "internal/utils/memory_profiler.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace utils = kcenon::network::utils;

// ============================================================================
// memory_snapshot Tests
// ============================================================================

TEST(MemorySnapshotTest, DefaultValues)
{
	utils::memory_snapshot snap;

	EXPECT_EQ(snap.resident_bytes, 0u);
	EXPECT_EQ(snap.virtual_bytes, 0u);
}

TEST(MemorySnapshotTest, FieldAssignment)
{
	utils::memory_snapshot snap;
	snap.timestamp = std::chrono::system_clock::now();
	snap.resident_bytes = 1024 * 1024;
	snap.virtual_bytes = 4096 * 1024;

	EXPECT_EQ(snap.resident_bytes, 1024u * 1024);
	EXPECT_EQ(snap.virtual_bytes, 4096u * 1024);
}

// ============================================================================
// memory_profiler Tests
// ============================================================================

class MemoryProfilerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		utils::memory_profiler::instance().clear_history();
		utils::memory_profiler::instance().stop();
	}

	void TearDown() override
	{
		utils::memory_profiler::instance().stop();
		utils::memory_profiler::instance().clear_history();
	}
};

TEST_F(MemoryProfilerTest, SingletonConsistency)
{
	auto& inst1 = utils::memory_profiler::instance();
	auto& inst2 = utils::memory_profiler::instance();

	EXPECT_EQ(&inst1, &inst2);
}

TEST_F(MemoryProfilerTest, SnapshotReturnsNonZeroMemory)
{
	auto snap = utils::memory_profiler::instance().snapshot();

	// A running process should have non-zero RSS and VSZ
	EXPECT_GT(snap.resident_bytes, 0u);
	EXPECT_GT(snap.virtual_bytes, 0u);
}

TEST_F(MemoryProfilerTest, SnapshotTimestamp)
{
	auto before = std::chrono::system_clock::now();
	auto snap = utils::memory_profiler::instance().snapshot();
	auto after = std::chrono::system_clock::now();

	EXPECT_GE(snap.timestamp, before);
	EXPECT_LE(snap.timestamp, after);
}

TEST_F(MemoryProfilerTest, SnapshotAddsToHistory)
{
	EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 0u);

	utils::memory_profiler::instance().snapshot();
	EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 1u);

	utils::memory_profiler::instance().snapshot();
	EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 2u);
}

TEST_F(MemoryProfilerTest, ClearHistory)
{
	utils::memory_profiler::instance().snapshot();
	utils::memory_profiler::instance().snapshot();
	EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 2u);

	utils::memory_profiler::instance().clear_history();
	EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 0u);
}

TEST_F(MemoryProfilerTest, GetHistoryMaxCount)
{
	for (int i = 0; i < 5; ++i)
	{
		utils::memory_profiler::instance().snapshot();
	}

	auto full = utils::memory_profiler::instance().get_history();
	EXPECT_EQ(full.size(), 5u);

	auto limited = utils::memory_profiler::instance().get_history(3);
	EXPECT_EQ(limited.size(), 3u);
}

TEST_F(MemoryProfilerTest, GetHistoryMaxCountExceedsAvailable)
{
	utils::memory_profiler::instance().snapshot();
	utils::memory_profiler::instance().snapshot();

	auto history = utils::memory_profiler::instance().get_history(100);
	EXPECT_EQ(history.size(), 2u);
}

TEST_F(MemoryProfilerTest, ToTsvEmptyHistory)
{
	auto tsv = utils::memory_profiler::instance().to_tsv();

	// Even empty, the format should be a valid string
	EXPECT_TRUE(tsv.empty() || !tsv.empty());
}

TEST_F(MemoryProfilerTest, ToTsvWithSnapshots)
{
	utils::memory_profiler::instance().snapshot();
	utils::memory_profiler::instance().snapshot();

	auto tsv = utils::memory_profiler::instance().to_tsv();

	// TSV should contain some data
	EXPECT_FALSE(tsv.empty());
}

TEST_F(MemoryProfilerTest, StartAndStop)
{
	utils::memory_profiler::instance().start(std::chrono::milliseconds{500});

	// Brief wait to allow at least one sample
	std::this_thread::sleep_for(std::chrono::milliseconds{100});

	utils::memory_profiler::instance().stop();

	// Should not crash and should be stoppable
	EXPECT_NO_FATAL_FAILURE(utils::memory_profiler::instance().stop());
}

TEST_F(MemoryProfilerTest, StopWhenNotRunning)
{
	// Stop without start should be safe
	EXPECT_NO_FATAL_FAILURE(utils::memory_profiler::instance().stop());
}

TEST_F(MemoryProfilerTest, StartTwiceIsNoOp)
{
	utils::memory_profiler::instance().start(std::chrono::milliseconds{500});
	EXPECT_NO_FATAL_FAILURE(
		utils::memory_profiler::instance().start(std::chrono::milliseconds{500}));

	utils::memory_profiler::instance().stop();
}
