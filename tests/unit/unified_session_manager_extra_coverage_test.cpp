/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "internal/core/session_traits.h"
#include "internal/core/unified_session_manager.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace kcenon::network;
using namespace std::chrono_literals;

/**
 * @file unified_session_manager_extra_coverage_test.cpp
 * @brief Extra coverage tests for unified_session_manager (Issue #1034)
 *
 * Targets uncovered branches and edge cases not exercised by
 * unified_session_manager_test.cpp:
 *   - Default-constructed manager configuration defaults
 *   - is_backpressure_active() short-circuit when disabled
 *   - get_utilization() with max_sessions == 0 (divide-by-zero guard)
 *   - add_session(session_handle, id) overload (pre-wrapped handle path)
 *   - add_session_with_id rejection / explicit id / collision returns
 *   - Duplicate-id add returns false without inflating session_count
 *   - update_activity() and remove_session() on missing ids
 *   - cleanup_idle_sessions() no-op when nothing is idle
 *   - clear_all_sessions() and stop_all_sessions() are equivalent and
 *     re-usable (manager is functional after each)
 *   - for_each over an empty manager
 *   - generate_id() monotonic increase under concurrency
 *   - set_max_sessions() narrowing below current count keeps existing
 *     sessions but blocks further accepts and toggles backpressure
 *   - stats fields fully populated (idle_timeout / backpressure_active)
 */

// ============================================================================
// Test Session Types (mirroring unified_session_manager_test.cpp)
// ============================================================================

namespace {

class extra_tcp_session {
public:
    explicit extra_tcp_session(std::string id = "tcp")
        : id_(std::move(id)) {}

    [[nodiscard]] auto id() const -> std::string_view { return id_; }
    [[nodiscard]] auto is_connected() const -> bool { return connected_; }
    auto close() -> void { connected_ = false; }
    [[nodiscard]] auto send(std::vector<uint8_t>&&) -> VoidResult {
        if (!connected_) {
            return VoidResult::err(-1, "Not connected");
        }
        ++send_count_;
        return VoidResult::ok(std::monostate{});
    }
    [[nodiscard]] auto get_send_count() const -> int { return send_count_; }

private:
    std::string id_;
    bool connected_{true};
    int send_count_{0};
};

class extra_idle_session {
public:
    explicit extra_idle_session(std::string id = "idle")
        : id_(std::move(id)) {}

    [[nodiscard]] auto id() const -> std::string_view { return id_; }
    [[nodiscard]] auto is_connected() const -> bool { return connected_; }
    auto close() -> void { connected_ = false; }
    auto stop_session() -> void { connected_ = false; }
    [[nodiscard]] auto send(std::vector<uint8_t>&&) -> VoidResult {
        return VoidResult::ok(std::monostate{});
    }

private:
    std::string id_;
    bool connected_{true};
};

} // namespace

// ============================================================================
// Session Traits Specializations
// ============================================================================

namespace kcenon::network::core {

template <>
struct session_traits<extra_tcp_session> {
    static constexpr bool has_activity_tracking = false;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "extra_tcp_";
};

template <>
struct session_traits<extra_idle_session> {
    static constexpr bool has_activity_tracking = true;
    static constexpr bool stop_on_clear = true;
    static constexpr const char* id_prefix = "extra_idle_";
};

} // namespace kcenon::network::core

// ============================================================================
// Default-constructed manager
// ============================================================================

TEST(UnifiedSessionManagerExtra, DefaultConstructorUsesDefaultConfig) {
    unified_session_manager mgr;

    const auto& cfg = mgr.get_config();
    // Defaults from unified_session_config{} per the header
    EXPECT_EQ(cfg.max_sessions, 1000u);
    EXPECT_TRUE(cfg.enable_backpressure);
    EXPECT_DOUBLE_EQ(cfg.backpressure_threshold, 0.8);
    EXPECT_EQ(cfg.idle_timeout, std::chrono::minutes(5));
    EXPECT_EQ(cfg.cleanup_interval, std::chrono::seconds(30));

    EXPECT_TRUE(mgr.can_accept_connection());
    EXPECT_FALSE(mgr.is_backpressure_active());
    EXPECT_EQ(mgr.get_session_count(), 0u);
    EXPECT_DOUBLE_EQ(mgr.get_utilization(), 0.0);
}

// ============================================================================
// is_backpressure_active() short-circuit
// ============================================================================

TEST(UnifiedSessionManagerExtra, BackpressureDisabledNeverActive) {
    unified_session_config cfg;
    cfg.max_sessions = 4;
    cfg.enable_backpressure = false;
    cfg.backpressure_threshold = 0.5; // would normally trigger after 2
    unified_session_manager mgr(cfg);

    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(mgr.add_session(
            std::make_shared<extra_tcp_session>("t" + std::to_string(i)),
            "t" + std::to_string(i)));
    }

    // With backpressure disabled, the threshold is irrelevant.
    EXPECT_FALSE(mgr.is_backpressure_active());

    // stats must reflect the same.
    auto s = mgr.get_stats();
    EXPECT_FALSE(s.backpressure_active);
}

// ============================================================================
// get_utilization() divide-by-zero guard
// ============================================================================

TEST(UnifiedSessionManagerExtra, UtilizationWithZeroMaxSessions) {
    unified_session_config cfg;
    cfg.max_sessions = 0;
    unified_session_manager mgr(cfg);

    // Adding any session must be rejected (limit is zero).
    EXPECT_FALSE(mgr.add_session(
        std::make_shared<extra_tcp_session>("nope"), "nope"));

    EXPECT_DOUBLE_EQ(mgr.get_utilization(), 0.0);
    EXPECT_EQ(mgr.get_total_rejected(), 1u);

    // can_accept_connection must return false when max_sessions is zero.
    EXPECT_FALSE(mgr.can_accept_connection());
}

// ============================================================================
// add_session(session_handle, id) overload
// ============================================================================

TEST(UnifiedSessionManagerExtra, AddSessionViaPreWrappedHandle) {
    unified_session_manager mgr;

    // Build a session_handle outside the manager and add it via the
    // non-template overload.
    auto raw = std::make_shared<extra_tcp_session>("pre_wrapped");
    session_handle h(raw);
    EXPECT_TRUE(h.valid());

    EXPECT_TRUE(mgr.add_session(std::move(h), "pre_id"));
    EXPECT_EQ(mgr.get_session_count(), 1u);

    auto* recovered = mgr.get_session("pre_id");
    ASSERT_NE(recovered, nullptr);
    EXPECT_TRUE(recovered->is_type<extra_tcp_session>());
}

TEST(UnifiedSessionManagerExtra, AddSessionViaPreWrappedHandleAutoId) {
    unified_session_manager mgr;

    auto raw = std::make_shared<extra_tcp_session>("auto_via_handle");
    session_handle h(raw);

    // Empty id triggers auto-generation through the overload.
    EXPECT_TRUE(mgr.add_session(std::move(h), ""));
    EXPECT_EQ(mgr.get_session_count(), 1u);

    auto ids = mgr.get_all_session_ids();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0].rfind("session_", 0), 0u);
}

// ============================================================================
// add_session_with_id rejection / explicit id / collision
// ============================================================================

TEST(UnifiedSessionManagerExtra, AddSessionWithIdRejectsAtLimit) {
    unified_session_config cfg;
    cfg.max_sessions = 1;
    unified_session_manager mgr(cfg);

    auto first = std::make_shared<extra_tcp_session>("first");
    EXPECT_FALSE(mgr.add_session_with_id(first, "first").empty());

    auto second = std::make_shared<extra_tcp_session>("second");
    auto rejected = mgr.add_session_with_id(second, "second");
    EXPECT_TRUE(rejected.empty());
    EXPECT_EQ(mgr.get_session_count(), 1u);
    EXPECT_EQ(mgr.get_total_rejected(), 1u);
}

TEST(UnifiedSessionManagerExtra, AddSessionWithIdReturnsExplicitId) {
    unified_session_manager mgr;

    auto s = std::make_shared<extra_tcp_session>("explicit");
    auto returned = mgr.add_session_with_id(s, "my-explicit-id");
    EXPECT_EQ(returned, "my-explicit-id");
    EXPECT_TRUE(mgr.has_session("my-explicit-id"));
}

TEST(UnifiedSessionManagerExtra, AddSessionWithIdCollisionReturnsEmpty) {
    unified_session_manager mgr;

    auto first = std::make_shared<extra_tcp_session>("a");
    EXPECT_EQ(mgr.add_session_with_id(first, "dup"), "dup");

    auto second = std::make_shared<extra_tcp_session>("b");
    // Same id twice — emplace fails, returned id is empty.
    EXPECT_TRUE(mgr.add_session_with_id(second, "dup").empty());

    // session_count must not have been incremented for the failed insertion.
    EXPECT_EQ(mgr.get_session_count(), 1u);
}

TEST(UnifiedSessionManagerExtra, AddSessionWithIdAutoGeneratedHasPrefix) {
    unified_session_manager mgr;
    auto s = std::make_shared<extra_tcp_session>("auto");
    auto id = mgr.add_session_with_id(s);
    ASSERT_FALSE(id.empty());
    EXPECT_EQ(id.rfind("session_", 0), 0u);
}

// ============================================================================
// Duplicate-id add via add_session
// ============================================================================

TEST(UnifiedSessionManagerExtra, DuplicateAddSessionDoesNotInflateCount) {
    unified_session_manager mgr;

    auto s1 = std::make_shared<extra_tcp_session>("s1");
    auto s2 = std::make_shared<extra_tcp_session>("s2");

    EXPECT_TRUE(mgr.add_session(s1, "same"));
    EXPECT_FALSE(mgr.add_session(s2, "same"));
    EXPECT_EQ(mgr.get_session_count(), 1u);
    // Duplicate id on add_session does not bump total_accepted.
    EXPECT_EQ(mgr.get_total_accepted(), 1u);
}

// ============================================================================
// update_activity / remove_session on missing id
// ============================================================================

TEST(UnifiedSessionManagerExtra, UpdateActivityOnMissingIdReturnsFalse) {
    unified_session_manager mgr;
    EXPECT_FALSE(mgr.update_activity("nonexistent"));
}

TEST(UnifiedSessionManagerExtra, RemoveSessionEmptyManagerReturnsFalse) {
    unified_session_manager mgr;
    EXPECT_FALSE(mgr.remove_session("nope"));
    EXPECT_EQ(mgr.get_session_count(), 0u);
}

// ============================================================================
// cleanup_idle_sessions no-op
// ============================================================================

TEST(UnifiedSessionManagerExtra, CleanupNoIdleSessionsReturnsZero) {
    unified_session_config cfg;
    cfg.idle_timeout = 1h; // far in the future
    unified_session_manager mgr(cfg);

    mgr.add_session(std::make_shared<extra_idle_session>("i1"), "i1");
    mgr.add_session(std::make_shared<extra_idle_session>("i2"), "i2");

    EXPECT_EQ(mgr.cleanup_idle_sessions(), 0u);
    EXPECT_EQ(mgr.get_session_count(), 2u);
    EXPECT_EQ(mgr.get_total_cleaned_up(), 0u);
}

TEST(UnifiedSessionManagerExtra, CleanupWithEmptyManagerReturnsZero) {
    unified_session_manager mgr;
    EXPECT_EQ(mgr.cleanup_idle_sessions(), 0u);
}

// ============================================================================
// Lifecycle: clear/stop equivalence and reusability
// ============================================================================

TEST(UnifiedSessionManagerExtra, StopAllIsEquivalentToClearAll) {
    unified_session_manager mgr;
    mgr.add_session(std::make_shared<extra_tcp_session>("t1"), "t1");
    mgr.add_session(std::make_shared<extra_idle_session>("i1"), "i1");
    EXPECT_EQ(mgr.get_session_count(), 2u);

    mgr.stop_all_sessions();
    EXPECT_EQ(mgr.get_session_count(), 0u);

    // Manager remains usable: add again, then clear, then add once more.
    mgr.add_session(std::make_shared<extra_tcp_session>("t2"), "t2");
    EXPECT_EQ(mgr.get_session_count(), 1u);

    mgr.clear_all_sessions();
    EXPECT_EQ(mgr.get_session_count(), 0u);

    mgr.add_session(std::make_shared<extra_tcp_session>("t3"), "t3");
    EXPECT_EQ(mgr.get_session_count(), 1u);
}

// ============================================================================
// for_each on empty manager
// ============================================================================

TEST(UnifiedSessionManagerExtra, ForEachOverEmptyManagerInvokesNothing) {
    unified_session_manager mgr;

    int mutable_count = 0;
    mgr.for_each([&](session_handle&) { ++mutable_count; });
    EXPECT_EQ(mutable_count, 0);

    int const_count = 0;
    const auto& cmgr = mgr;
    cmgr.for_each([&](const session_handle&) { ++const_count; });
    EXPECT_EQ(const_count, 0);
}

// ============================================================================
// generate_id monotonic under concurrency
// ============================================================================

TEST(UnifiedSessionManagerExtra, GenerateIdsAreUniqueAcrossThreads) {
    constexpr int kThreads = 4;
    constexpr int kPerThread = 200;

    std::vector<std::thread> workers;
    std::vector<std::vector<std::string>> per_thread(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&, t]() {
            per_thread[t].reserve(kPerThread);
            for (int i = 0; i < kPerThread; ++i) {
                per_thread[t].push_back(
                    unified_session_manager::generate_id("uniq_"));
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    std::vector<std::string> all;
    all.reserve(kThreads * kPerThread);
    for (auto& v : per_thread) {
        for (auto& s : v) {
            EXPECT_EQ(s.rfind("uniq_", 0), 0u);
            all.push_back(std::move(s));
        }
    }
    std::sort(all.begin(), all.end());
    auto last = std::unique(all.begin(), all.end());
    EXPECT_EQ(last, all.end()) << "generate_id produced duplicate values";
}

// ============================================================================
// set_max_sessions narrowing behavior
// ============================================================================

TEST(UnifiedSessionManagerExtra, SetMaxSessionsNarrowingBlocksAccepts) {
    unified_session_config cfg;
    cfg.max_sessions = 5;
    cfg.enable_backpressure = true;
    cfg.backpressure_threshold = 0.5; // 0.5 of new max == 1 session
    unified_session_manager mgr(cfg);

    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(mgr.add_session(
            std::make_shared<extra_tcp_session>("s" + std::to_string(i)),
            "s" + std::to_string(i)));
    }
    EXPECT_EQ(mgr.get_session_count(), 3u);
    EXPECT_TRUE(mgr.can_accept_connection());

    // Narrow the limit below the current count.
    mgr.set_max_sessions(2);
    EXPECT_EQ(mgr.get_config().max_sessions, 2u);

    // Existing sessions are kept; new accepts must be rejected.
    EXPECT_EQ(mgr.get_session_count(), 3u);
    EXPECT_FALSE(mgr.can_accept_connection());
    EXPECT_FALSE(mgr.add_session(
        std::make_shared<extra_tcp_session>("overflow"), "overflow"));
    EXPECT_GE(mgr.get_total_rejected(), 1u);

    // Backpressure must be active because count >= max * threshold.
    EXPECT_TRUE(mgr.is_backpressure_active());

    // Utilization may exceed 1.0 transiently after narrowing — that is fine,
    // we only care that the value is finite and positive.
    auto util = mgr.get_utilization();
    EXPECT_GT(util, 0.0);
}

// ============================================================================
// stats: full population
// ============================================================================

TEST(UnifiedSessionManagerExtra, StatsCarryIdleTimeoutAndBackpressureFlag) {
    unified_session_config cfg;
    cfg.max_sessions = 4;
    cfg.idle_timeout = 250ms;
    cfg.enable_backpressure = true;
    cfg.backpressure_threshold = 0.5;
    unified_session_manager mgr(cfg);

    mgr.add_session(std::make_shared<extra_tcp_session>("a"), "a");
    mgr.add_session(std::make_shared<extra_tcp_session>("b"), "b");

    auto s = mgr.get_stats();
    EXPECT_EQ(s.active_sessions, 2u);
    EXPECT_EQ(s.max_sessions, 4u);
    EXPECT_EQ(s.total_accepted, 2u);
    EXPECT_EQ(s.total_rejected, 0u);
    EXPECT_EQ(s.total_cleaned_up, 0u);
    EXPECT_DOUBLE_EQ(s.utilization, 0.5);
    EXPECT_TRUE(s.backpressure_active);
    EXPECT_EQ(s.idle_timeout, 250ms);
}

// ============================================================================
// const get_session round-trip
// ============================================================================

TEST(UnifiedSessionManagerExtra, ConstGetSessionFindsAndReportsType) {
    unified_session_manager mgr;
    auto t = std::make_shared<extra_tcp_session>("c1");
    mgr.add_session(t, "c1");

    const auto& cmgr = mgr;
    const auto* h = cmgr.get_session("c1");
    ASSERT_NE(h, nullptr);
    EXPECT_TRUE(h->is_type<extra_tcp_session>());
    EXPECT_EQ(h->id(), "c1");

    EXPECT_EQ(cmgr.get_session("missing"), nullptr);
}

// ============================================================================
// Activity tracking: cleanup increments total_cleaned_up across calls
// ============================================================================

TEST(UnifiedSessionManagerExtra, CleanupAccumulatesAcrossInvocations) {
    unified_session_config cfg;
    cfg.max_sessions = 8;
    cfg.idle_timeout = 30ms;
    unified_session_manager mgr(cfg);

    mgr.add_session(std::make_shared<extra_idle_session>("a"), "a");
    mgr.add_session(std::make_shared<extra_idle_session>("b"), "b");

    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(mgr.cleanup_idle_sessions(), 2u);

    mgr.add_session(std::make_shared<extra_idle_session>("c"), "c");
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(mgr.cleanup_idle_sessions(), 1u);

    EXPECT_EQ(mgr.get_total_cleaned_up(), 3u);
    EXPECT_EQ(mgr.get_session_count(), 0u);
}
