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
 * @file unified_session_manager_test.cpp
 * @brief Unit tests for unified_session_manager (Phase 2 of #522)
 *
 * Tests validate:
 * - Type-erased session management
 * - Heterogeneous session storage
 * - Thread safety
 * - Metrics and statistics
 * - Idle session cleanup
 */

// ============================================================================
// Test Session Types
// ============================================================================

namespace {

class test_tcp_session {
public:
    explicit test_tcp_session(std::string id = "tcp")
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

class test_ws_session {
public:
    explicit test_ws_session(std::string id = "ws")
        : id_(std::move(id)) {}

    [[nodiscard]] auto id() const -> std::string_view { return id_; }
    [[nodiscard]] auto is_connected() const -> bool { return connected_; }
    auto close() -> void { connected_ = false; }
    [[nodiscard]] auto send(std::vector<uint8_t>&&) -> VoidResult {
        if (!connected_) {
            return VoidResult::err(-1, "Not connected");
        }
        return VoidResult::ok(std::monostate{});
    }

    auto set_protocol(const std::string& protocol) -> void {
        protocol_ = protocol;
    }
    [[nodiscard]] auto get_protocol() const -> const std::string& {
        return protocol_;
    }

private:
    std::string id_;
    bool connected_{true};
    std::string protocol_{"ws"};
};

class test_idle_session {
public:
    explicit test_idle_session(std::string id = "idle")
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
struct session_traits<test_tcp_session> {
    static constexpr bool has_activity_tracking = false;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "tcp_";
};

template <>
struct session_traits<test_ws_session> {
    static constexpr bool has_activity_tracking = false;
    static constexpr bool stop_on_clear = false;
    static constexpr const char* id_prefix = "ws_";
};

template <>
struct session_traits<test_idle_session> {
    static constexpr bool has_activity_tracking = true;
    static constexpr bool stop_on_clear = true;
    static constexpr const char* id_prefix = "idle_";
};

} // namespace kcenon::network::core

// ============================================================================
// Basic Operations Tests
// ============================================================================

class UnifiedSessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_sessions = 100;
        config_.idle_timeout = 50ms;
        config_.enable_backpressure = true;
        config_.backpressure_threshold = 0.8;

        manager_ = std::make_unique<unified_session_manager>(config_);
    }

    unified_session_config config_;
    std::unique_ptr<unified_session_manager> manager_;
};

TEST_F(UnifiedSessionManagerTest, AddAndGetSession) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_1");

    EXPECT_TRUE(manager_->add_session(tcp, "tcp_1"));
    EXPECT_EQ(manager_->get_session_count(), 1);

    auto* handle = manager_->get_session("tcp_1");
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->id(), "tcp_1");
    EXPECT_TRUE(handle->is_connected());
}

TEST_F(UnifiedSessionManagerTest, AddSessionWithAutoId) {
    auto tcp = std::make_shared<test_tcp_session>("auto_id_tcp");

    std::string id = manager_->add_session_with_id(tcp);
    EXPECT_FALSE(id.empty());
    EXPECT_TRUE(id.find("session_") == 0);
    EXPECT_EQ(manager_->get_session_count(), 1);
}

TEST_F(UnifiedSessionManagerTest, RemoveSession) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_remove");

    manager_->add_session(tcp, "tcp_remove");
    EXPECT_EQ(manager_->get_session_count(), 1);

    EXPECT_TRUE(manager_->remove_session("tcp_remove"));
    EXPECT_EQ(manager_->get_session_count(), 0);
    EXPECT_EQ(manager_->get_session("tcp_remove"), nullptr);
}

TEST_F(UnifiedSessionManagerTest, HasSession) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_has");

    EXPECT_FALSE(manager_->has_session("tcp_has"));
    manager_->add_session(tcp, "tcp_has");
    EXPECT_TRUE(manager_->has_session("tcp_has"));
}

TEST_F(UnifiedSessionManagerTest, GetAllSessionIds) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp3"), "tcp3");

    auto ids = manager_->get_all_session_ids();
    EXPECT_EQ(ids.size(), 3);

    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids[0], "tcp1");
    EXPECT_EQ(ids[1], "tcp2");
    EXPECT_EQ(ids[2], "tcp3");
}

// ============================================================================
// Heterogeneous Session Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, HeterogeneousSessions) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_hetero");
    auto ws = std::make_shared<test_ws_session>("ws_hetero");

    manager_->add_session(tcp, "tcp_1");
    manager_->add_session(ws, "ws_1");

    EXPECT_EQ(manager_->get_session_count(), 2);

    auto* tcp_handle = manager_->get_session("tcp_1");
    ASSERT_NE(tcp_handle, nullptr);
    EXPECT_TRUE(tcp_handle->is_type<test_tcp_session>());
    EXPECT_FALSE(tcp_handle->is_type<test_ws_session>());

    auto* ws_handle = manager_->get_session("ws_1");
    ASSERT_NE(ws_handle, nullptr);
    EXPECT_TRUE(ws_handle->is_type<test_ws_session>());
    EXPECT_FALSE(ws_handle->is_type<test_tcp_session>());
}

TEST_F(UnifiedSessionManagerTest, TypeRecoveryFromManager) {
    auto ws = std::make_shared<test_ws_session>("ws_recovery");
    ws->set_protocol("wss");

    manager_->add_session(ws, "ws_1");

    auto* handle = manager_->get_session("ws_1");
    ASSERT_NE(handle, nullptr);

    auto* recovered = handle->as<test_ws_session>();
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(recovered->get_protocol(), "wss");

    recovered->set_protocol("ws_modified");
    EXPECT_EQ(ws->get_protocol(), "ws_modified");
}

// ============================================================================
// with_session Callback Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, WithSessionCallback) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_callback");
    manager_->add_session(tcp, "tcp_1");

    bool callback_called = false;
    bool result = manager_->with_session("tcp_1", [&](session_handle& handle) {
        callback_called = true;
        EXPECT_EQ(handle.id(), "tcp_callback");
        EXPECT_TRUE(handle.is_connected());
    });

    EXPECT_TRUE(result);
    EXPECT_TRUE(callback_called);
}

TEST_F(UnifiedSessionManagerTest, WithSessionNotFound) {
    bool callback_called = false;
    bool result = manager_->with_session("nonexistent", [&](session_handle&) {
        callback_called = true;
    });

    EXPECT_FALSE(result);
    EXPECT_FALSE(callback_called);
}

// ============================================================================
// Iteration Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, ForEachMutable) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");

    int count = 0;
    manager_->for_each([&count](session_handle& handle) {
        EXPECT_TRUE(handle.is_connected());
        ++count;
    });

    EXPECT_EQ(count, 2);
}

TEST_F(UnifiedSessionManagerTest, ForEachConst) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");

    const auto& const_manager = *manager_;
    int count = 0;
    const_manager.for_each([&count](const session_handle& handle) {
        EXPECT_TRUE(handle.is_connected());
        ++count;
    });

    EXPECT_EQ(count, 2);
}

// ============================================================================
// Broadcast Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, BroadcastToAllSessions) {
    auto tcp1 = std::make_shared<test_tcp_session>("tcp1");
    auto tcp2 = std::make_shared<test_tcp_session>("tcp2");

    manager_->add_session(tcp1, "tcp1");
    manager_->add_session(tcp2, "tcp2");

    std::vector<uint8_t> data{1, 2, 3, 4};
    size_t sent = manager_->broadcast(std::move(data));

    EXPECT_EQ(sent, 2);
    EXPECT_EQ(tcp1->get_send_count(), 1);
    EXPECT_EQ(tcp2->get_send_count(), 1);
}

TEST_F(UnifiedSessionManagerTest, BroadcastSkipsDisconnected) {
    auto tcp1 = std::make_shared<test_tcp_session>("tcp1");
    auto tcp2 = std::make_shared<test_tcp_session>("tcp2");

    manager_->add_session(tcp1, "tcp1");
    manager_->add_session(tcp2, "tcp2");

    tcp2->close();

    std::vector<uint8_t> data{1, 2, 3, 4};
    size_t sent = manager_->broadcast(std::move(data));

    EXPECT_EQ(sent, 1);
    EXPECT_EQ(tcp1->get_send_count(), 1);
    EXPECT_EQ(tcp2->get_send_count(), 0);
}

// ============================================================================
// Connection Limit Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, RejectWhenAtLimit) {
    unified_session_config limited_config;
    limited_config.max_sessions = 2;
    unified_session_manager limited_manager(limited_config);

    EXPECT_TRUE(limited_manager.add_session(
        std::make_shared<test_tcp_session>("tcp1"), "tcp1"));
    EXPECT_TRUE(limited_manager.add_session(
        std::make_shared<test_tcp_session>("tcp2"), "tcp2"));
    EXPECT_FALSE(limited_manager.add_session(
        std::make_shared<test_tcp_session>("tcp3"), "tcp3"));

    EXPECT_EQ(limited_manager.get_session_count(), 2);
    EXPECT_EQ(limited_manager.get_total_rejected(), 1);
}

TEST_F(UnifiedSessionManagerTest, BackpressureActivation) {
    unified_session_config bp_config;
    bp_config.max_sessions = 10;
    bp_config.enable_backpressure = true;
    bp_config.backpressure_threshold = 0.5;
    unified_session_manager bp_manager(bp_config);

    for (int i = 0; i < 4; ++i) {
        bp_manager.add_session(
            std::make_shared<test_tcp_session>("tcp" + std::to_string(i)),
            "tcp" + std::to_string(i));
    }
    EXPECT_FALSE(bp_manager.is_backpressure_active());

    bp_manager.add_session(std::make_shared<test_tcp_session>("tcp4"), "tcp4");
    EXPECT_TRUE(bp_manager.is_backpressure_active());
}

// ============================================================================
// Activity Tracking & Cleanup Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, ActivityTracking) {
    auto idle = std::make_shared<test_idle_session>("idle1");
    manager_->add_session(idle, "idle1");

    std::this_thread::sleep_for(30ms);

    auto* handle = manager_->get_session("idle1");
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(handle->has_activity_tracking());
    EXPECT_GE(handle->idle_duration().count(), 25);

    EXPECT_TRUE(manager_->update_activity("idle1"));
    EXPECT_LT(handle->idle_duration().count(), 10);
}

TEST_F(UnifiedSessionManagerTest, CleanupIdleSessions) {
    auto idle1 = std::make_shared<test_idle_session>("idle1");
    auto tcp1 = std::make_shared<test_tcp_session>("tcp1");

    manager_->add_session(idle1, "idle1");
    manager_->add_session(tcp1, "tcp1");

    EXPECT_EQ(manager_->get_session_count(), 2);

    std::this_thread::sleep_for(60ms);

    size_t cleaned = manager_->cleanup_idle_sessions();
    EXPECT_EQ(cleaned, 1);
    EXPECT_EQ(manager_->get_session_count(), 1);

    EXPECT_FALSE(manager_->has_session("idle1"));
    EXPECT_TRUE(manager_->has_session("tcp1"));
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, ClearAllSessions) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp3"), "tcp3");

    EXPECT_EQ(manager_->get_session_count(), 3);

    manager_->clear_all_sessions();

    EXPECT_EQ(manager_->get_session_count(), 0);
}

TEST_F(UnifiedSessionManagerTest, StopAllSessions) {
    auto idle = std::make_shared<test_idle_session>("idle1");
    manager_->add_session(idle, "idle1");

    EXPECT_TRUE(idle->is_connected());

    manager_->stop_all_sessions();

    EXPECT_EQ(manager_->get_session_count(), 0);
}

// ============================================================================
// Metrics Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, MetricsTracking) {
    unified_session_config metric_config;
    metric_config.max_sessions = 2;
    unified_session_manager metric_manager(metric_config);

    metric_manager.add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    metric_manager.add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");
    metric_manager.add_session(std::make_shared<test_tcp_session>("tcp3"), "tcp3");

    EXPECT_EQ(metric_manager.get_total_accepted(), 2);
    EXPECT_EQ(metric_manager.get_total_rejected(), 1);
    EXPECT_EQ(metric_manager.get_session_count(), 2);
}

TEST_F(UnifiedSessionManagerTest, UtilizationCalculation) {
    unified_session_config util_config;
    util_config.max_sessions = 10;
    unified_session_manager util_manager(util_config);

    EXPECT_DOUBLE_EQ(util_manager.get_utilization(), 0.0);

    for (int i = 0; i < 5; ++i) {
        util_manager.add_session(
            std::make_shared<test_tcp_session>("tcp" + std::to_string(i)),
            "tcp" + std::to_string(i));
    }

    EXPECT_DOUBLE_EQ(util_manager.get_utilization(), 0.5);
}

TEST_F(UnifiedSessionManagerTest, GetStats) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");

    auto stats = manager_->get_stats();

    EXPECT_EQ(stats.active_sessions, 2);
    EXPECT_EQ(stats.max_sessions, 100);
    EXPECT_EQ(stats.total_accepted, 2);
    EXPECT_EQ(stats.total_rejected, 0);
    EXPECT_DOUBLE_EQ(stats.utilization, 0.02);
    EXPECT_FALSE(stats.backpressure_active);
    EXPECT_EQ(stats.idle_timeout, 50ms);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, SetMaxSessions) {
    EXPECT_EQ(manager_->get_config().max_sessions, 100);

    manager_->set_max_sessions(200);

    EXPECT_EQ(manager_->get_config().max_sessions, 200);
}

// ============================================================================
// ID Generation Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, GenerateUniqueIds) {
    std::string id1 = unified_session_manager::generate_id();
    std::string id2 = unified_session_manager::generate_id();
    std::string id3 = unified_session_manager::generate_id("custom_");

    EXPECT_NE(id1, id2);
    EXPECT_TRUE(id1.find("session_") == 0);
    EXPECT_TRUE(id2.find("session_") == 0);
    EXPECT_TRUE(id3.find("custom_") == 0);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, ConcurrentAddRemove) {
    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 50;

    std::vector<std::thread> threads;
    std::atomic<int> added{0};
    std::atomic<int> removed{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &added, &removed]() {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string id = "thread_" + std::to_string(t) + "_" + std::to_string(i);
                auto session = std::make_shared<test_tcp_session>(id);

                if (manager_->add_session(session, id)) {
                    ++added;
                }

                if (i % 2 == 0) {
                    if (manager_->remove_session(id)) {
                        ++removed;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(added.load(), NUM_THREADS * OPS_PER_THREAD);
    EXPECT_GT(removed.load(), 0);
}

TEST_F(UnifiedSessionManagerTest, ConcurrentIteration) {
    for (int i = 0; i < 20; ++i) {
        manager_->add_session(
            std::make_shared<test_tcp_session>("tcp" + std::to_string(i)),
            "tcp" + std::to_string(i));
    }

    std::atomic<int> iteration_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([this, &iteration_count]() {
            for (int i = 0; i < 10; ++i) {
                manager_->for_each([&iteration_count](const session_handle&) {
                    ++iteration_count;
                });
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(iteration_count.load(), 4 * 10 * 20);
}

// ============================================================================
// Type Erasure Safety Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, InvalidCastReturnsNullptr) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_cast");
    manager_->add_session(tcp, "tcp_cast");

    auto* handle = manager_->get_session("tcp_cast");
    ASSERT_NE(handle, nullptr);

    // Attempt to cast to wrong type
    auto* wrong_type = handle->as<test_ws_session>();
    EXPECT_EQ(wrong_type, nullptr);

    // Correct cast should succeed
    auto* correct_type = handle->as<test_tcp_session>();
    EXPECT_NE(correct_type, nullptr);
}

TEST_F(UnifiedSessionManagerTest, IsTypeChecksAllSessionTypes) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_type");
    auto ws = std::make_shared<test_ws_session>("ws_type");
    auto idle = std::make_shared<test_idle_session>("idle_type");

    manager_->add_session(tcp, "tcp");
    manager_->add_session(ws, "ws");
    manager_->add_session(idle, "idle");

    auto* tcp_handle = manager_->get_session("tcp");
    EXPECT_TRUE(tcp_handle->is_type<test_tcp_session>());
    EXPECT_FALSE(tcp_handle->is_type<test_ws_session>());
    EXPECT_FALSE(tcp_handle->is_type<test_idle_session>());

    auto* ws_handle = manager_->get_session("ws");
    EXPECT_FALSE(ws_handle->is_type<test_tcp_session>());
    EXPECT_TRUE(ws_handle->is_type<test_ws_session>());

    auto* idle_handle = manager_->get_session("idle");
    EXPECT_FALSE(idle_handle->is_type<test_tcp_session>());
    EXPECT_TRUE(idle_handle->is_type<test_idle_session>());
}

// ============================================================================
// Broadcast Edge Cases
// ============================================================================

TEST_F(UnifiedSessionManagerTest, BroadcastToEmptyManager) {
    std::vector<uint8_t> data{1, 2, 3};
    size_t sent = manager_->broadcast(std::move(data));

    EXPECT_EQ(sent, 0);
}

TEST_F(UnifiedSessionManagerTest, BroadcastEmptyData) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_empty");
    manager_->add_session(tcp, "tcp_empty");

    std::vector<uint8_t> empty_data;
    size_t sent = manager_->broadcast(std::move(empty_data));

    EXPECT_EQ(sent, 1);
    EXPECT_EQ(tcp->get_send_count(), 1);
}

TEST_F(UnifiedSessionManagerTest, BroadcastToMixedSessionTypes) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_mix");
    auto ws = std::make_shared<test_ws_session>("ws_mix");
    auto idle = std::make_shared<test_idle_session>("idle_mix");

    manager_->add_session(tcp, "tcp");
    manager_->add_session(ws, "ws");
    manager_->add_session(idle, "idle");

    std::vector<uint8_t> data{1, 2, 3};
    size_t sent = manager_->broadcast(std::move(data));

    // All three session types should receive the broadcast
    EXPECT_EQ(sent, 3);
}

TEST_F(UnifiedSessionManagerTest, BroadcastAllDisconnected) {
    auto tcp1 = std::make_shared<test_tcp_session>("tcp1");
    auto tcp2 = std::make_shared<test_tcp_session>("tcp2");

    manager_->add_session(tcp1, "tcp1");
    manager_->add_session(tcp2, "tcp2");

    // Disconnect all
    tcp1->close();
    tcp2->close();

    std::vector<uint8_t> data{1, 2, 3};
    size_t sent = manager_->broadcast(std::move(data));

    EXPECT_EQ(sent, 0);
}

// ============================================================================
// with_session Callback Safety Tests
// ============================================================================

TEST_F(UnifiedSessionManagerTest, WithSessionModifiesSession) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_modify");
    manager_->add_session(tcp, "tcp_modify");

    bool modified = manager_->with_session("tcp_modify", [](session_handle& handle) {
        // Send data through the handle
        std::vector<uint8_t> data{0x01};
        handle.send(std::move(data));
    });

    EXPECT_TRUE(modified);
    EXPECT_EQ(tcp->get_send_count(), 1);
}

TEST_F(UnifiedSessionManagerTest, WithSessionOnMultipleSessions) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp1"), "tcp1");
    manager_->add_session(std::make_shared<test_tcp_session>("tcp2"), "tcp2");

    int found_count = 0;
    for (const auto& id : {"tcp1", "tcp2", "nonexistent"}) {
        if (manager_->with_session(id, [](session_handle&) {})) {
            ++found_count;
        }
    }

    EXPECT_EQ(found_count, 2);
}

// ============================================================================
// Remove Non-existent Session
// ============================================================================

TEST_F(UnifiedSessionManagerTest, RemoveNonexistentSession) {
    EXPECT_FALSE(manager_->remove_session("ghost_session"));
    EXPECT_EQ(manager_->get_session_count(), 0);
}

TEST_F(UnifiedSessionManagerTest, DoubleRemoveReturnsFalse) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp_double"), "tcp_double");

    EXPECT_TRUE(manager_->remove_session("tcp_double"));
    EXPECT_FALSE(manager_->remove_session("tcp_double"));
}

// ============================================================================
// Activity Tracking on Non-Trackable Sessions
// ============================================================================

TEST_F(UnifiedSessionManagerTest, UpdateActivityOnNonTrackableSession) {
    auto tcp = std::make_shared<test_tcp_session>("tcp_notrack");
    manager_->add_session(tcp, "tcp_notrack");

    // tcp_session has has_activity_tracking = false
    auto* handle = manager_->get_session("tcp_notrack");
    ASSERT_NE(handle, nullptr);
    EXPECT_FALSE(handle->has_activity_tracking());

    // update_activity should still not crash
    bool result = manager_->update_activity("tcp_notrack");
    // May return true or false depending on implementation
    EXPECT_TRUE(result || !result);  // No crash is the goal
}

TEST_F(UnifiedSessionManagerTest, CleanupOnlyRemovesTrackableSessions) {
    // Non-trackable sessions should not be cleaned up
    auto tcp = std::make_shared<test_tcp_session>("tcp_persist");
    auto idle = std::make_shared<test_idle_session>("idle_expire");

    manager_->add_session(tcp, "tcp_persist");
    manager_->add_session(idle, "idle_expire");

    std::this_thread::sleep_for(60ms);

    size_t cleaned = manager_->cleanup_idle_sessions();

    // Only the idle session (with activity tracking) should be cleaned up
    EXPECT_EQ(cleaned, 1);
    EXPECT_TRUE(manager_->has_session("tcp_persist"));
    EXPECT_FALSE(manager_->has_session("idle_expire"));
}

// ============================================================================
// Cleanup Stats Tracking
// ============================================================================

TEST_F(UnifiedSessionManagerTest, CleanupStatsAccumulate) {
    auto idle1 = std::make_shared<test_idle_session>("idle1");
    manager_->add_session(idle1, "idle1");

    std::this_thread::sleep_for(60ms);
    manager_->cleanup_idle_sessions();

    auto idle2 = std::make_shared<test_idle_session>("idle2");
    auto idle3 = std::make_shared<test_idle_session>("idle3");
    manager_->add_session(idle2, "idle2");
    manager_->add_session(idle3, "idle3");

    std::this_thread::sleep_for(60ms);
    manager_->cleanup_idle_sessions();

    EXPECT_EQ(manager_->get_total_cleaned_up(), 3);
}

// ============================================================================
// Lifecycle Edge Cases
// ============================================================================

TEST_F(UnifiedSessionManagerTest, ClearEmptyManager) {
    manager_->clear_all_sessions();
    EXPECT_EQ(manager_->get_session_count(), 0);
}

TEST_F(UnifiedSessionManagerTest, AddAfterClear) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp_before"), "tcp_before");
    manager_->clear_all_sessions();

    EXPECT_TRUE(manager_->add_session(
        std::make_shared<test_tcp_session>("tcp_after"), "tcp_after"));
    EXPECT_EQ(manager_->get_session_count(), 1);
}

TEST_F(UnifiedSessionManagerTest, GetSessionOnConstManager) {
    manager_->add_session(std::make_shared<test_tcp_session>("tcp_const"), "tcp_const");

    const auto& const_manager = *manager_;
    const auto* handle = const_manager.get_session("tcp_const");
    ASSERT_NE(handle, nullptr);
    EXPECT_TRUE(handle->is_connected());
}

TEST_F(UnifiedSessionManagerTest, GetSessionConstNotFound) {
    const auto& const_manager = *manager_;
    const auto* handle = const_manager.get_session("no_such_session");
    EXPECT_EQ(handle, nullptr);
}

// ============================================================================
// Concurrent Broadcast and Modification
// ============================================================================

TEST_F(UnifiedSessionManagerTest, ConcurrentBroadcastAndAddRemove) {
    // Seed some sessions
    for (int i = 0; i < 10; ++i) {
        manager_->add_session(
            std::make_shared<test_tcp_session>("tcp" + std::to_string(i)),
            "tcp" + std::to_string(i));
    }

    std::atomic<bool> stop{false};
    std::atomic<size_t> total_sent{0};

    // Broadcaster thread
    std::thread broadcaster([this, &stop, &total_sent]() {
        while (!stop.load()) {
            std::vector<uint8_t> data{1, 2, 3};
            total_sent.fetch_add(manager_->broadcast(std::move(data)));
            std::this_thread::yield();
        }
    });

    // Modifier thread: add and remove sessions
    std::thread modifier([this, &stop]() {
        int counter = 100;
        while (!stop.load()) {
            std::string id = "dynamic_" + std::to_string(counter++);
            manager_->add_session(std::make_shared<test_tcp_session>(id), id);
            manager_->remove_session(id);
            std::this_thread::yield();
        }
    });

    std::this_thread::sleep_for(50ms);
    stop.store(true);
    broadcaster.join();
    modifier.join();

    // No crash = success
    EXPECT_GT(total_sent.load(), 0u);
}
