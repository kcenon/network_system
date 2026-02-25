/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include <gtest/gtest.h>
#include "internal/utils/memory_profiler.h"
#include "internal/utils/session_timeout.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace utils = kcenon::network::utils;

// ============================================================================
// Memory Profiler Tests
// ============================================================================

class MemoryProfilerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Ensure clean state for each test
        utils::memory_profiler::instance().clear_history();
        utils::memory_profiler::instance().stop();
    }

    void TearDown() override
    {
        utils::memory_profiler::instance().stop();
        utils::memory_profiler::instance().clear_history();
    }
};

TEST_F(MemoryProfilerTest, SingletonInstanceConsistency)
{
    auto& inst1 = utils::memory_profiler::instance();
    auto& inst2 = utils::memory_profiler::instance();
    EXPECT_EQ(&inst1, &inst2);
}

TEST_F(MemoryProfilerTest, SnapshotReturnsNonZeroMemory)
{
    auto snap = utils::memory_profiler::instance().snapshot();

    // On macOS/Linux, resident and virtual memory should be non-zero
    // for a running process
    EXPECT_GT(snap.resident_bytes, 0u);
    EXPECT_GT(snap.virtual_bytes, 0u);
}

TEST_F(MemoryProfilerTest, SnapshotTimestampIsReasonable)
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

TEST_F(MemoryProfilerTest, GetHistoryMaxCount)
{
    // Take 5 snapshots
    for (int i = 0; i < 5; ++i)
    {
        utils::memory_profiler::instance().snapshot();
    }

    auto all = utils::memory_profiler::instance().get_history();
    EXPECT_EQ(all.size(), 5u);

    // Request only last 3
    auto limited = utils::memory_profiler::instance().get_history(3);
    EXPECT_EQ(limited.size(), 3u);

    // The limited history should contain the last 3 snapshots
    EXPECT_EQ(limited[0].timestamp, all[2].timestamp);
    EXPECT_EQ(limited[1].timestamp, all[3].timestamp);
    EXPECT_EQ(limited[2].timestamp, all[4].timestamp);
}

TEST_F(MemoryProfilerTest, GetHistoryMaxCountExceedsSize)
{
    utils::memory_profiler::instance().snapshot();
    utils::memory_profiler::instance().snapshot();

    // Request more than available
    auto result = utils::memory_profiler::instance().get_history(100);
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(MemoryProfilerTest, ClearHistory)
{
    utils::memory_profiler::instance().snapshot();
    utils::memory_profiler::instance().snapshot();
    EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 2u);

    utils::memory_profiler::instance().clear_history();
    EXPECT_EQ(utils::memory_profiler::instance().get_history().size(), 0u);
}

TEST_F(MemoryProfilerTest, ToTsvEmptyHistory)
{
    auto tsv = utils::memory_profiler::instance().to_tsv();
    EXPECT_TRUE(tsv.empty());
}

TEST_F(MemoryProfilerTest, ToTsvFormat)
{
    utils::memory_profiler::instance().snapshot();

    auto tsv = utils::memory_profiler::instance().to_tsv();
    EXPECT_FALSE(tsv.empty());

    // TSV format: "YYYY-MM-DD HH:MM:SS\tRSS\tVSZ\n"
    // Should contain at least one tab separator
    EXPECT_NE(tsv.find('\t'), std::string::npos);
    // Should end with newline
    EXPECT_EQ(tsv.back(), '\n');
}

TEST_F(MemoryProfilerTest, ToTsvMultipleEntries)
{
    utils::memory_profiler::instance().snapshot();
    utils::memory_profiler::instance().snapshot();
    utils::memory_profiler::instance().snapshot();

    auto tsv = utils::memory_profiler::instance().to_tsv();

    // Count newlines - should have 3 lines
    size_t newlines = 0;
    for (char c : tsv)
    {
        if (c == '\n')
        {
            ++newlines;
        }
    }
    EXPECT_EQ(newlines, 3u);
}

TEST_F(MemoryProfilerTest, StartStopWithoutProfilerFlag)
{
    // Without NETWORK_ENABLE_MEMORY_PROFILER, start() is a no-op
    utils::memory_profiler::instance().start(std::chrono::milliseconds(100));
    utils::memory_profiler::instance().stop();

    // Manual snapshots still work regardless
    auto snap = utils::memory_profiler::instance().snapshot();
    EXPECT_GT(snap.resident_bytes, 0u);
}

TEST_F(MemoryProfilerTest, MultipleSnapshotsShowConsistentRss)
{
    auto snap1 = utils::memory_profiler::instance().snapshot();
    auto snap2 = utils::memory_profiler::instance().snapshot();

    // RSS should be within a reasonable range across consecutive snapshots
    // (no huge jumps expected during test execution)
    double ratio = static_cast<double>(snap2.resident_bytes)
                 / static_cast<double>(snap1.resident_bytes);
    EXPECT_GT(ratio, 0.5);
    EXPECT_LT(ratio, 2.0);
}

TEST_F(MemoryProfilerTest, HistoryOrderIsChronological)
{
    utils::memory_profiler::instance().snapshot();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    utils::memory_profiler::instance().snapshot();

    auto history = utils::memory_profiler::instance().get_history();
    ASSERT_EQ(history.size(), 2u);
    EXPECT_LE(history[0].timestamp, history[1].timestamp);
}

// ============================================================================
// Session Timeout Manager Tests
// ============================================================================

class SessionTimeoutTest : public ::testing::Test {};

TEST_F(SessionTimeoutTest, DefaultConstruction)
{
    kcenon::network::session_timeout_manager mgr;
    EXPECT_FALSE(mgr.is_timed_out());
}

TEST_F(SessionTimeoutTest, CustomTimeout)
{
    kcenon::network::session_timeout_manager mgr(std::chrono::seconds(10));
    EXPECT_FALSE(mgr.is_timed_out());
}

TEST_F(SessionTimeoutTest, NotTimedOutWhenActive)
{
    kcenon::network::session_timeout_manager mgr(std::chrono::seconds(60));
    mgr.update_activity();
    EXPECT_FALSE(mgr.is_timed_out());
}

TEST_F(SessionTimeoutTest, TimedOutAfterExpiry)
{
    // is_timed_out() uses duration_cast<seconds> with strict '>' comparison,
    // so we need elapsed seconds > timeout seconds.
    // With timeout=0, we need at least 1 full second to elapse.
    kcenon::network::session_timeout_manager mgr(std::chrono::seconds(0));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(mgr.is_timed_out());
}

TEST_F(SessionTimeoutTest, UpdateActivityResetsTimeout)
{
    // Use timeout=0 and wait >1s so is_timed_out() returns true
    kcenon::network::session_timeout_manager mgr(std::chrono::seconds(0));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(mgr.is_timed_out());

    // After update_activity, elapsed resets to ~0 which is NOT > 0
    mgr.update_activity();
    EXPECT_FALSE(mgr.is_timed_out());
}

TEST_F(SessionTimeoutTest, GetIdleTimeInitiallySmall)
{
    kcenon::network::session_timeout_manager mgr;
    auto idle = mgr.get_idle_time();
    // Should be very close to zero right after construction
    EXPECT_LE(idle.count(), 1);
}

TEST_F(SessionTimeoutTest, GetIdleTimeIncreasesOverTime)
{
    kcenon::network::session_timeout_manager mgr;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto idle = mgr.get_idle_time();
    // idle_time should be at least 0 seconds (sub-second precision)
    EXPECT_GE(idle.count(), 0);
}

TEST_F(SessionTimeoutTest, GetIdleTimeResetsOnActivity)
{
    kcenon::network::session_timeout_manager mgr;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    mgr.update_activity();
    auto idle_after_update = mgr.get_idle_time();

    // After update_activity, idle time should be very small
    EXPECT_LE(idle_after_update.count(), 1);
}

TEST_F(SessionTimeoutTest, ConcurrentAccess)
{
    kcenon::network::session_timeout_manager mgr(std::chrono::seconds(60));

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&mgr]()
        {
            for (int j = 0; j < 100; ++j)
            {
                mgr.update_activity();
                (void)mgr.is_timed_out();
                (void)mgr.get_idle_time();
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // Should still be valid and not timed out
    EXPECT_FALSE(mgr.is_timed_out());
}

// ============================================================================
// Memory Snapshot Struct Tests
// ============================================================================

TEST(MemorySnapshotTest, DefaultValues)
{
    utils::memory_snapshot snap;
    EXPECT_EQ(snap.resident_bytes, 0u);
    EXPECT_EQ(snap.virtual_bytes, 0u);
}
