/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "internal/core/session_manager_base.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace std::chrono_literals;

/**
 * @file session_manager_base_test.cpp
 * @brief Unit tests for session_manager_base<T> template
 *
 * Tests validate:
 * - Template instantiation with custom session types
 * - Connection acceptance logic with configurable limits
 * - Add/remove/get operations
 * - Metrics accuracy
 * - Thread safety (concurrent operations)
 * - Activity tracking for enabled traits
 * - Backpressure detection
 */

// ============================================================================
// Test Session Types
// ============================================================================

namespace {

/**
 * @brief Simple session type without activity tracking
 */
class simple_session {
public:
	simple_session() = default;
	explicit simple_session(std::string id) : id_(std::move(id)) {}

	[[nodiscard]] auto id() const -> const std::string& { return id_; }

private:
	std::string id_{"simple"};
};

/**
 * @brief Session type with stop_session support
 */
class stoppable_session {
public:
	stoppable_session() = default;

	auto stop_session() -> void { stopped_ = true; }

	[[nodiscard]] auto is_stopped() const -> bool { return stopped_; }

private:
	std::atomic<bool> stopped_{false};
};

} // namespace

// ============================================================================
// Custom Traits for Test Session Types
// ============================================================================

namespace kcenon::network::core {

template <>
struct session_traits<simple_session> {
	static constexpr bool has_activity_tracking = false;
	static constexpr bool stop_on_clear = false;
	static constexpr const char* id_prefix = "simple_";
};

template <>
struct session_traits<stoppable_session> {
	static constexpr bool has_activity_tracking = true;
	static constexpr bool stop_on_clear = true;
	static constexpr const char* id_prefix = "stoppable_";
};

} // namespace kcenon::network::core

// ============================================================================
// Session Manager Base Template Tests
// ============================================================================

class SessionManagerBaseTest : public ::testing::Test {
protected:
	void SetUp() override {
		config_.max_sessions = 100;
		config_.idle_timeout = 100ms;
		config_.cleanup_interval = 50ms;
		config_.enable_backpressure = true;
		config_.backpressure_threshold = 0.8;
	}

	session_config config_;
};

// ============================================================================
// Basic Template Instantiation Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, InstantiateWithSimpleSession) {
	session_manager_base<simple_session> manager(config_);

	EXPECT_EQ(manager.get_session_count(), 0);
	EXPECT_TRUE(manager.can_accept_connection());
}

TEST_F(SessionManagerBaseTest, InstantiateWithStoppableSession) {
	session_manager_base<stoppable_session> manager(config_);

	EXPECT_EQ(manager.get_session_count(), 0);
	EXPECT_TRUE(manager.can_accept_connection());
}

TEST_F(SessionManagerBaseTest, DefaultConfiguration) {
	session_manager_base<simple_session> manager;

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.max_sessions, 1000);
	EXPECT_EQ(stats.idle_timeout, std::chrono::minutes(5));
}

// ============================================================================
// Connection Acceptance Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, CanAcceptConnectionUnderLimit) {
	config_.max_sessions = 10;
	session_manager_base<simple_session> manager(config_);

	for (size_t i = 0; i < 5; ++i) {
		auto session = std::make_shared<simple_session>();
		EXPECT_TRUE(manager.add_session(session));
	}

	EXPECT_TRUE(manager.can_accept_connection());
	EXPECT_EQ(manager.get_session_count(), 5);
}

TEST_F(SessionManagerBaseTest, RejectsConnectionAtLimit) {
	config_.max_sessions = 3;
	session_manager_base<simple_session> manager(config_);

	// Fill to capacity
	for (size_t i = 0; i < 3; ++i) {
		auto session = std::make_shared<simple_session>();
		EXPECT_TRUE(manager.add_session(session, "session_" + std::to_string(i)));
	}

	EXPECT_FALSE(manager.can_accept_connection());

	// New session should be rejected
	auto overflow = std::make_shared<simple_session>();
	EXPECT_FALSE(manager.add_session(overflow, "overflow"));

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.total_rejected, 1);
	EXPECT_EQ(stats.total_accepted, 3);
}

// ============================================================================
// Add/Remove/Get Operations Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, AddSessionWithAutoId) {
	session_manager_base<simple_session> manager(config_);

	auto session = std::make_shared<simple_session>("test");
	std::string id = manager.add_session_with_id(session);

	EXPECT_FALSE(id.empty());
	EXPECT_TRUE(id.find("simple_") == 0);
	EXPECT_EQ(manager.get_session_count(), 1);
}

TEST_F(SessionManagerBaseTest, AddSessionWithCustomId) {
	session_manager_base<simple_session> manager(config_);

	auto session = std::make_shared<simple_session>();
	std::string custom_id = "my_custom_id";
	std::string returned_id = manager.add_session_with_id(session, custom_id);

	EXPECT_EQ(returned_id, custom_id);

	auto retrieved = manager.get_session(custom_id);
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(SessionManagerBaseTest, RemoveSession) {
	session_manager_base<simple_session> manager(config_);

	auto session = std::make_shared<simple_session>();
	manager.add_session(session, "to_remove");

	EXPECT_EQ(manager.get_session_count(), 1);

	bool removed = manager.remove_session("to_remove");
	EXPECT_TRUE(removed);
	EXPECT_EQ(manager.get_session_count(), 0);

	// Second removal should fail
	EXPECT_FALSE(manager.remove_session("to_remove"));
}

TEST_F(SessionManagerBaseTest, GetSessionNotFound) {
	session_manager_base<simple_session> manager(config_);

	auto result = manager.get_session("nonexistent");
	EXPECT_EQ(result, nullptr);
}

TEST_F(SessionManagerBaseTest, GetAllSessions) {
	session_manager_base<simple_session> manager(config_);

	for (size_t i = 0; i < 5; ++i) {
		auto session = std::make_shared<simple_session>(std::to_string(i));
		manager.add_session(session, "session_" + std::to_string(i));
	}

	auto sessions = manager.get_all_sessions();
	EXPECT_EQ(sessions.size(), 5);
}

TEST_F(SessionManagerBaseTest, GetAllSessionIds) {
	session_manager_base<simple_session> manager(config_);

	manager.add_session(std::make_shared<simple_session>(), "id_1");
	manager.add_session(std::make_shared<simple_session>(), "id_2");
	manager.add_session(std::make_shared<simple_session>(), "id_3");

	auto ids = manager.get_all_session_ids();
	EXPECT_EQ(ids.size(), 3);

	// Verify all IDs are present
	std::sort(ids.begin(), ids.end());
	EXPECT_EQ(ids[0], "id_1");
	EXPECT_EQ(ids[1], "id_2");
	EXPECT_EQ(ids[2], "id_3");
}

// ============================================================================
// Metrics Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, MetricsAccuracy) {
	config_.max_sessions = 5;
	session_manager_base<simple_session> manager(config_);

	// Add 5 sessions
	for (size_t i = 0; i < 5; ++i) {
		manager.add_session(std::make_shared<simple_session>(), std::to_string(i));
	}

	// Try to add 2 more (should be rejected)
	manager.add_session(std::make_shared<simple_session>(), "overflow1");
	manager.add_session(std::make_shared<simple_session>(), "overflow2");

	// Remove 2 sessions
	manager.remove_session("0");
	manager.remove_session("1");

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.total_accepted, 5);
	EXPECT_EQ(stats.total_rejected, 2);
	EXPECT_EQ(stats.active_sessions, 3);
}

TEST_F(SessionManagerBaseTest, UtilizationCalculation) {
	config_.max_sessions = 10;
	session_manager_base<simple_session> manager(config_);

	EXPECT_DOUBLE_EQ(manager.get_utilization(), 0.0);

	for (size_t i = 0; i < 5; ++i) {
		manager.add_session(std::make_shared<simple_session>());
	}

	EXPECT_DOUBLE_EQ(manager.get_utilization(), 0.5);
}

TEST_F(SessionManagerBaseTest, UtilizationWithZeroMax) {
	config_.max_sessions = 0;
	session_manager_base<simple_session> manager(config_);

	EXPECT_DOUBLE_EQ(manager.get_utilization(), 0.0);
}

// ============================================================================
// Backpressure Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, BackpressureActivation) {
	config_.max_sessions = 10;
	config_.enable_backpressure = true;
	config_.backpressure_threshold = 0.8;
	session_manager_base<simple_session> manager(config_);

	// Add 7 sessions (70%) - below threshold
	for (size_t i = 0; i < 7; ++i) {
		manager.add_session(std::make_shared<simple_session>());
	}
	EXPECT_FALSE(manager.is_backpressure_active());

	// Add 1 more (80%) - at threshold
	manager.add_session(std::make_shared<simple_session>());
	EXPECT_TRUE(manager.is_backpressure_active());
}

TEST_F(SessionManagerBaseTest, BackpressureDisabled) {
	config_.max_sessions = 10;
	config_.enable_backpressure = false;
	session_manager_base<simple_session> manager(config_);

	// Fill to 90%
	for (size_t i = 0; i < 9; ++i) {
		manager.add_session(std::make_shared<simple_session>());
	}

	EXPECT_FALSE(manager.is_backpressure_active());
}

// ============================================================================
// Activity Tracking Tests (for traits with has_activity_tracking = true)
// ============================================================================

TEST_F(SessionManagerBaseTest, ActivityTrackingUpdateActivity) {
	session_manager_base<stoppable_session> manager(config_);

	auto session = std::make_shared<stoppable_session>();
	manager.add_session(session, "tracked");

	auto before = manager.get_idle_duration("tracked");
	ASSERT_TRUE(before.has_value());

	std::this_thread::sleep_for(20ms);

	manager.update_activity("tracked");

	auto after = manager.get_idle_duration("tracked");
	ASSERT_TRUE(after.has_value());

	// After update, idle duration should be reset
	EXPECT_LT(after->count(), before->count() + 20);
}

TEST_F(SessionManagerBaseTest, ActivityTrackingIdleDurationNotFound) {
	session_manager_base<stoppable_session> manager(config_);

	auto duration = manager.get_idle_duration("nonexistent");
	EXPECT_FALSE(duration.has_value());
}

TEST_F(SessionManagerBaseTest, CleanupIdleSessions) {
	config_.idle_timeout = 30ms;
	session_manager_base<stoppable_session> manager(config_);

	auto session1 = std::make_shared<stoppable_session>();
	auto session2 = std::make_shared<stoppable_session>();

	manager.add_session(session1, "session_1");
	manager.add_session(session2, "session_2");

	// Wait for idle timeout
	std::this_thread::sleep_for(50ms);

	size_t cleaned = manager.cleanup_idle_sessions();

	EXPECT_EQ(cleaned, 2);
	EXPECT_EQ(manager.get_session_count(), 0);
	EXPECT_TRUE(session1->is_stopped());
	EXPECT_TRUE(session2->is_stopped());
}

TEST_F(SessionManagerBaseTest, CleanupPreservesActiveSessions) {
	config_.idle_timeout = 50ms;
	session_manager_base<stoppable_session> manager(config_);

	auto active = std::make_shared<stoppable_session>();
	auto idle = std::make_shared<stoppable_session>();

	manager.add_session(active, "active");
	manager.add_session(idle, "idle");

	// Wait partial timeout
	std::this_thread::sleep_for(30ms);

	// Keep active session alive
	manager.update_activity("active");

	// Wait for idle session to timeout
	std::this_thread::sleep_for(30ms);

	size_t cleaned = manager.cleanup_idle_sessions();

	EXPECT_EQ(cleaned, 1);
	EXPECT_EQ(manager.get_session_count(), 1);
	EXPECT_NE(manager.get_session("active"), nullptr);
	EXPECT_EQ(manager.get_session("idle"), nullptr);
}

// ============================================================================
// Lifecycle Management Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, ClearAllSessions) {
	session_manager_base<simple_session> manager(config_);

	for (size_t i = 0; i < 10; ++i) {
		manager.add_session(std::make_shared<simple_session>());
	}

	EXPECT_EQ(manager.get_session_count(), 10);

	manager.clear_all_sessions();

	EXPECT_EQ(manager.get_session_count(), 0);
}

TEST_F(SessionManagerBaseTest, ClearAllSessionsCallsStop) {
	session_manager_base<stoppable_session> manager(config_);

	auto session1 = std::make_shared<stoppable_session>();
	auto session2 = std::make_shared<stoppable_session>();

	manager.add_session(session1, "s1");
	manager.add_session(session2, "s2");

	manager.clear_all_sessions();

	EXPECT_TRUE(session1->is_stopped());
	EXPECT_TRUE(session2->is_stopped());
}

TEST_F(SessionManagerBaseTest, StopAllSessionsAlias) {
	session_manager_base<stoppable_session> manager(config_);

	manager.add_session(std::make_shared<stoppable_session>(), "test");
	EXPECT_EQ(manager.get_session_count(), 1);

	manager.stop_all_sessions();
	EXPECT_EQ(manager.get_session_count(), 0);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, SetMaxSessions) {
	session_manager_base<simple_session> manager(config_);

	manager.set_max_sessions(50);

	auto cfg = manager.get_config();
	EXPECT_EQ(cfg.max_sessions, 50);
}

TEST_F(SessionManagerBaseTest, GetStats) {
	config_.max_sessions = 100;
	config_.idle_timeout = 200ms;
	session_manager_base<simple_session> manager(config_);

	auto stats = manager.get_stats();

	EXPECT_EQ(stats.max_sessions, 100);
	EXPECT_EQ(stats.idle_timeout, 200ms);
	EXPECT_EQ(stats.active_sessions, 0);
	EXPECT_EQ(stats.total_accepted, 0);
	EXPECT_EQ(stats.total_rejected, 0);
	EXPECT_EQ(stats.total_cleaned_up, 0);
	EXPECT_DOUBLE_EQ(stats.utilization, 0.0);
	EXPECT_FALSE(stats.backpressure_active);
}

// ============================================================================
// ID Generation Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, GenerateIdUsesPrefix) {
	auto id1 = session_manager_base<simple_session>::generate_id();
	auto id2 = session_manager_base<stoppable_session>::generate_id();

	EXPECT_TRUE(id1.find("simple_") == 0);
	EXPECT_TRUE(id2.find("stoppable_") == 0);
}

TEST_F(SessionManagerBaseTest, GenerateIdIsUnique) {
	std::vector<std::string> ids;
	for (int i = 0; i < 1000; ++i) {
		ids.push_back(session_manager_base<simple_session>::generate_id());
	}

	// Check uniqueness
	std::sort(ids.begin(), ids.end());
	auto last = std::unique(ids.begin(), ids.end());
	EXPECT_EQ(last, ids.end());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(SessionManagerBaseTest, ConcurrentAddRemove) {
	config_.max_sessions = 10000;
	session_manager_base<simple_session> manager(config_);

	constexpr int num_threads = 8;
	constexpr int ops_per_thread = 100;

	std::atomic<int> added{0};
	std::atomic<int> removed{0};

	std::vector<std::thread> threads;

	// Add threads
	for (int t = 0; t < num_threads / 2; ++t) {
		threads.emplace_back([&manager, &added, t]() {
			for (int i = 0; i < ops_per_thread; ++i) {
				auto session = std::make_shared<simple_session>();
				std::string id = "thread_" + std::to_string(t) + "_" + std::to_string(i);
				if (manager.add_session(session, id)) {
					added.fetch_add(1);
				}
			}
		});
	}

	// Remove threads
	for (int t = 0; t < num_threads / 2; ++t) {
		threads.emplace_back([&manager, &removed, t]() {
			for (int i = 0; i < ops_per_thread; ++i) {
				std::string id = "thread_" + std::to_string(t) + "_" + std::to_string(i);
				if (manager.remove_session(id)) {
					removed.fetch_add(1);
				}
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	// Verify consistency
	auto stats = manager.get_stats();
	EXPECT_EQ(stats.active_sessions, added.load() - removed.load());
}

TEST_F(SessionManagerBaseTest, ConcurrentReads) {
	session_manager_base<simple_session> manager(config_);

	// Add some sessions first
	for (int i = 0; i < 100; ++i) {
		manager.add_session(std::make_shared<simple_session>(), "session_" + std::to_string(i));
	}

	constexpr int num_readers = 8;
	constexpr int reads_per_thread = 1000;
	std::atomic<int> successful_reads{0};

	std::vector<std::thread> readers;
	for (int t = 0; t < num_readers; ++t) {
		readers.emplace_back([&manager, &successful_reads]() {
			for (int i = 0; i < reads_per_thread; ++i) {
				auto session = manager.get_session("session_" + std::to_string(i % 100));
				if (session != nullptr) {
					successful_reads.fetch_add(1);
				}
			}
		});
	}

	for (auto& r : readers) {
		r.join();
	}

	EXPECT_EQ(successful_reads.load(), num_readers * reads_per_thread);
}

TEST_F(SessionManagerBaseTest, ConcurrentActivityUpdates) {
	session_manager_base<stoppable_session> manager(config_);

	manager.add_session(std::make_shared<stoppable_session>(), "shared");

	constexpr int num_threads = 8;
	constexpr int updates_per_thread = 100;
	std::atomic<int> update_count{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < num_threads; ++t) {
		threads.emplace_back([&manager, &update_count]() {
			for (int i = 0; i < updates_per_thread; ++i) {
				manager.update_activity("shared");
				update_count.fetch_add(1);
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	EXPECT_EQ(update_count.load(), num_threads * updates_per_thread);
	EXPECT_NE(manager.get_session("shared"), nullptr);
}
