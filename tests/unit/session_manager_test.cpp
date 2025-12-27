/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/core/session_manager.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace kcenon::network::core;
using namespace std::chrono_literals;

/**
 * @file session_manager_test.cpp
 * @brief Unit tests for session_manager idle timeout functionality
 *
 * Tests validate:
 * - Session activity tracking
 * - Idle session detection and cleanup
 * - Statistics tracking for cleaned up sessions
 * - Thread-safe operations
 */

// ============================================================================
// Mock Session for Testing
// ============================================================================

namespace {

/**
 * @brief Mock session for testing session_manager functionality
 *
 * This mock doesn't require ASIO and provides tracking for stop_session calls.
 */
class mock_messaging_session
	: public std::enable_shared_from_this<mock_messaging_session>
{
public:
	mock_messaging_session() : stopped_(false) {}

	auto stop_session() -> void { stopped_ = true; }

	[[nodiscard]] auto is_stopped() const noexcept -> bool { return stopped_; }

	[[nodiscard]] auto server_id() const -> const std::string& { return id_; }

private:
	std::atomic<bool> stopped_;
	std::string id_{"mock_session"};
};

} // namespace

// ============================================================================
// Session Info Tests
// ============================================================================

class SessionInfoTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Create a mock session using reinterpret_cast for testing
		// In real usage, messaging_session would be used
		mock_session_ = std::make_shared<mock_messaging_session>();
	}

	std::shared_ptr<mock_messaging_session> mock_session_;
};

TEST_F(SessionInfoTest, SessionInfoInitializesTimestamps)
{
	auto before = std::chrono::steady_clock::now();

	// Create session_info with a nullptr (just testing timestamps)
	session_info info(nullptr);

	auto after = std::chrono::steady_clock::now();

	// Verify created_at and last_activity are within reasonable bounds
	EXPECT_GE(info.created_at, before);
	EXPECT_LE(info.created_at, after);
	EXPECT_EQ(info.created_at, info.last_activity);
}

// ============================================================================
// Session Manager Config Tests
// ============================================================================

class SessionManagerConfigTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		config_.max_sessions = 100;
		config_.idle_timeout = 100ms;
		config_.cleanup_interval = 50ms;
		config_.enable_backpressure = true;
		config_.backpressure_threshold = 0.8;
	}

	session_config config_;
};

TEST_F(SessionManagerConfigTest, DefaultConfig)
{
	session_config default_config;

	EXPECT_EQ(default_config.max_sessions, 1000);
	EXPECT_EQ(default_config.idle_timeout, std::chrono::minutes(5));
	EXPECT_EQ(default_config.cleanup_interval, std::chrono::seconds(30));
	EXPECT_TRUE(default_config.enable_backpressure);
	EXPECT_DOUBLE_EQ(default_config.backpressure_threshold, 0.8);
}

TEST_F(SessionManagerConfigTest, CustomConfig)
{
	session_manager manager(config_);

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.max_sessions, 100);
	EXPECT_EQ(stats.idle_timeout, 100ms);
}

// ============================================================================
// Session Manager Basic Operations Tests
// ============================================================================

class SessionManagerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		config_.max_sessions = 10;
		config_.idle_timeout = 50ms;  // Short timeout for testing
		config_.cleanup_interval = 25ms;

		manager_ = std::make_unique<session_manager>(config_);
	}

	session_config config_;
	std::unique_ptr<session_manager> manager_;
};

TEST_F(SessionManagerTest, AddSessionSucceeds)
{
	bool result = manager_->add_session(nullptr, "session_1");

	EXPECT_TRUE(result);
	EXPECT_EQ(manager_->get_session_count(), 1);
}

TEST_F(SessionManagerTest, AddSessionRejectsWhenFull)
{
	// Fill up to max_sessions
	for (size_t i = 0; i < config_.max_sessions; ++i)
	{
		EXPECT_TRUE(manager_->add_session(nullptr, "session_" + std::to_string(i)));
	}

	// Next add should fail
	bool result = manager_->add_session(nullptr, "overflow_session");

	EXPECT_FALSE(result);
	EXPECT_EQ(manager_->get_session_count(), config_.max_sessions);

	auto stats = manager_->get_stats();
	EXPECT_EQ(stats.total_rejected, 1);
}

TEST_F(SessionManagerTest, GetSessionInfo)
{
	manager_->add_session(nullptr, "test_session");

	auto info = manager_->get_session_info("test_session");

	EXPECT_TRUE(info.has_value());
	EXPECT_EQ(info->session, nullptr);
}

TEST_F(SessionManagerTest, GetSessionInfoNotFound)
{
	auto info = manager_->get_session_info("nonexistent");

	EXPECT_FALSE(info.has_value());
}

// ============================================================================
// Activity Tracking Tests
// ============================================================================

class ActivityTrackingTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		config_.idle_timeout = 100ms;
		manager_ = std::make_unique<session_manager>(config_);
		manager_->add_session(nullptr, "tracked_session");
	}

	session_config config_;
	std::unique_ptr<session_manager> manager_;
};

TEST_F(ActivityTrackingTest, UpdateActivityRefreshesTimestamp)
{
	auto before_update = manager_->get_session_info("tracked_session");
	ASSERT_TRUE(before_update.has_value());

	std::this_thread::sleep_for(10ms);

	manager_->update_activity("tracked_session");

	auto after_update = manager_->get_session_info("tracked_session");
	ASSERT_TRUE(after_update.has_value());

	// last_activity should be updated
	EXPECT_GT(after_update->last_activity, before_update->last_activity);
	// created_at should remain unchanged
	EXPECT_EQ(after_update->created_at, before_update->created_at);
}

TEST_F(ActivityTrackingTest, GetIdleDuration)
{
	std::this_thread::sleep_for(20ms);

	auto idle_duration = manager_->get_idle_duration("tracked_session");

	ASSERT_TRUE(idle_duration.has_value());
	EXPECT_GE(idle_duration->count(), 20);
}

TEST_F(ActivityTrackingTest, GetIdleDurationAfterUpdate)
{
	std::this_thread::sleep_for(30ms);
	manager_->update_activity("tracked_session");
	std::this_thread::sleep_for(10ms);

	auto idle_duration = manager_->get_idle_duration("tracked_session");

	ASSERT_TRUE(idle_duration.has_value());
	// Should be around 10ms, not 40ms
	EXPECT_LT(idle_duration->count(), 30);
}

TEST_F(ActivityTrackingTest, GetIdleDurationNotFound)
{
	auto idle_duration = manager_->get_idle_duration("nonexistent");

	EXPECT_FALSE(idle_duration.has_value());
}

// ============================================================================
// Idle Session Cleanup Tests
// ============================================================================

class IdleCleanupTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		config_.max_sessions = 100;
		config_.idle_timeout = 50ms;  // Very short for testing

		manager_ = std::make_unique<session_manager>(config_);
	}

	session_config config_;
	std::unique_ptr<session_manager> manager_;
};

TEST_F(IdleCleanupTest, CleanupRemovesIdleSessions)
{
	// Add sessions
	manager_->add_session(nullptr, "idle_session_1");
	manager_->add_session(nullptr, "idle_session_2");

	EXPECT_EQ(manager_->get_session_count(), 2);

	// Wait for idle timeout
	std::this_thread::sleep_for(config_.idle_timeout + 20ms);

	// Cleanup should remove idle sessions
	size_t cleaned = manager_->cleanup_idle_sessions();

	EXPECT_EQ(cleaned, 2);
	EXPECT_EQ(manager_->get_session_count(), 0);
}

TEST_F(IdleCleanupTest, CleanupPreservesActiveSessions)
{
	// Use longer idle timeout for this specific test to avoid race conditions
	session_config longer_config;
	longer_config.idle_timeout = 100ms;
	session_manager test_manager(longer_config);

	// Add sessions
	test_manager.add_session(nullptr, "active_session");
	test_manager.add_session(nullptr, "idle_session");

	// Wait for partial timeout
	std::this_thread::sleep_for(60ms);

	// Update activity on one session (resets its timer)
	test_manager.update_activity("active_session");

	// Wait for idle timeout to pass for the idle session only
	// idle_session: 60ms + 50ms = 110ms > 100ms (idle)
	// active_session: 50ms < 100ms (not idle)
	std::this_thread::sleep_for(50ms);

	// Cleanup should only remove idle_session
	size_t cleaned = test_manager.cleanup_idle_sessions();

	EXPECT_EQ(cleaned, 1);
	EXPECT_EQ(test_manager.get_session_count(), 1);
	EXPECT_FALSE(test_manager.get_session_info("idle_session").has_value());  // Removed
	EXPECT_TRUE(test_manager.get_session_info("active_session").has_value());  // Still exists
}

TEST_F(IdleCleanupTest, CleanupNoOpWhenNoIdleSessions)
{
	manager_->add_session(nullptr, "fresh_session");

	// No wait, session is fresh
	size_t cleaned = manager_->cleanup_idle_sessions();

	EXPECT_EQ(cleaned, 0);
	EXPECT_EQ(manager_->get_session_count(), 1);
}

TEST_F(IdleCleanupTest, CleanupUpdatesStats)
{
	// Add sessions
	manager_->add_session(nullptr, "session_1");
	manager_->add_session(nullptr, "session_2");
	manager_->add_session(nullptr, "session_3");

	// Wait for idle timeout
	std::this_thread::sleep_for(config_.idle_timeout + 20ms);

	// Cleanup
	manager_->cleanup_idle_sessions();

	auto stats = manager_->get_stats();
	EXPECT_EQ(stats.total_cleaned_up, 3);
	EXPECT_EQ(stats.active_sessions, 0);
	EXPECT_EQ(stats.total_accepted, 3);
}

TEST_F(IdleCleanupTest, MultipleCleanupCycles)
{
	// First batch
	manager_->add_session(nullptr, "batch1_session");
	std::this_thread::sleep_for(config_.idle_timeout + 10ms);
	size_t cleaned1 = manager_->cleanup_idle_sessions();
	EXPECT_EQ(cleaned1, 1);

	// Second batch
	manager_->add_session(nullptr, "batch2_session_1");
	manager_->add_session(nullptr, "batch2_session_2");
	std::this_thread::sleep_for(config_.idle_timeout + 10ms);
	size_t cleaned2 = manager_->cleanup_idle_sessions();
	EXPECT_EQ(cleaned2, 2);

	// Total stats
	auto stats = manager_->get_stats();
	EXPECT_EQ(stats.total_cleaned_up, 3);
	EXPECT_EQ(stats.total_accepted, 3);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class ThreadSafetyTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		config_.max_sessions = 1000;
		config_.idle_timeout = 10ms;

		manager_ = std::make_unique<session_manager>(config_);
	}

	session_config config_;
	std::unique_ptr<session_manager> manager_;
};

TEST_F(ThreadSafetyTest, ConcurrentAddAndCleanup)
{
	constexpr int num_threads = 4;
	constexpr int sessions_per_thread = 50;
	std::atomic<size_t> total_added{0};
	std::atomic<size_t> total_cleaned{0};

	std::vector<std::thread> threads;

	// Add sessions from multiple threads
	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, t, &total_added]()
		{
			for (int i = 0; i < sessions_per_thread; ++i)
			{
				std::string id = "thread_" + std::to_string(t) + "_session_" + std::to_string(i);
				if (manager_->add_session(nullptr, id))
				{
					total_added.fetch_add(1);
				}
			}
		});
	}

	// Cleanup thread running concurrently
	threads.emplace_back([this, &total_cleaned]()
	{
		for (int i = 0; i < 20; ++i)
		{
			std::this_thread::sleep_for(5ms);
			total_cleaned.fetch_add(manager_->cleanup_idle_sessions());
		}
	});

	for (auto& t : threads)
	{
		t.join();
	}

	// Final cleanup
	std::this_thread::sleep_for(config_.idle_timeout + 10ms);
	total_cleaned.fetch_add(manager_->cleanup_idle_sessions());

	// Verify consistency
	EXPECT_EQ(total_added.load(), num_threads * sessions_per_thread);
	auto stats = manager_->get_stats();
	EXPECT_EQ(stats.total_accepted, total_added.load());
	EXPECT_EQ(stats.active_sessions + stats.total_cleaned_up, total_added.load());
}

TEST_F(ThreadSafetyTest, ConcurrentActivityUpdates)
{
	manager_->add_session(nullptr, "shared_session");

	constexpr int num_threads = 8;
	constexpr int updates_per_thread = 100;
	std::atomic<int> update_count{0};

	std::vector<std::thread> threads;

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, &update_count]()
		{
			for (int i = 0; i < updates_per_thread; ++i)
			{
				manager_->update_activity("shared_session");
				update_count.fetch_add(1);
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(update_count.load(), num_threads * updates_per_thread);
	// Session should still exist (activity kept it alive)
	EXPECT_TRUE(manager_->get_session_info("shared_session").has_value());
}
