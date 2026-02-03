/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "internal/core/ws_session_manager.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace std::chrono_literals;

/**
 * @file ws_session_manager_test.cpp
 * @brief Unit tests for ws_session_manager (WebSocket specialization)
 *
 * Tests validate:
 * - WebSocket-specific connection management
 * - Backward-compatible API (add_connection, remove_connection, etc.)
 * - Auto-generated connection IDs with ws_conn_ prefix
 * - No activity tracking (per traits)
 * - Thread safety for WebSocket connections
 */

// ============================================================================
// Mock WebSocket Connection for Testing
// ============================================================================

namespace kcenon::network::core {

/**
 * @brief Mock ws_connection for testing
 *
 * Real ws_connection requires WebSocket server infrastructure.
 * This mock provides the minimal interface needed for testing session manager.
 */
class ws_connection {
public:
	ws_connection() = default;
	explicit ws_connection(std::string id) : id_(std::move(id)) {}

	[[nodiscard]] auto connection_id() const -> const std::string& { return id_; }

private:
	std::string id_{"mock_ws_conn"};
};

} // namespace kcenon::network::core

// ============================================================================
// WebSocket Session Manager Tests
// ============================================================================

class WsSessionManagerTest : public ::testing::Test {
protected:
	void SetUp() override {
		config_.max_sessions = 100;
		config_.enable_backpressure = true;
		config_.backpressure_threshold = 0.8;
	}

	session_config config_;
};

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST_F(WsSessionManagerTest, Construction) {
	ws_session_manager manager(config_);

	EXPECT_EQ(manager.get_connection_count(), 0);
	EXPECT_TRUE(manager.can_accept_connection());
}

TEST_F(WsSessionManagerTest, AddConnectionWithAutoId) {
	ws_session_manager manager(config_);

	auto conn = std::make_shared<ws_connection>("test");
	std::string id = manager.add_connection(conn);

	EXPECT_FALSE(id.empty());
	EXPECT_TRUE(id.find("ws_conn_") == 0);
	EXPECT_EQ(manager.get_connection_count(), 1);
}

TEST_F(WsSessionManagerTest, AddConnectionWithCustomId) {
	ws_session_manager manager(config_);

	auto conn = std::make_shared<ws_connection>();
	std::string custom_id = "my_ws_connection";
	std::string returned_id = manager.add_connection(conn, custom_id);

	EXPECT_EQ(returned_id, custom_id);

	auto retrieved = manager.get_connection(custom_id);
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(WsSessionManagerTest, AddConnectionRejectsWhenFull) {
	config_.max_sessions = 3;
	ws_session_manager manager(config_);

	// Fill to capacity
	for (size_t i = 0; i < 3; ++i) {
		auto conn = std::make_shared<ws_connection>();
		std::string id = manager.add_connection(conn);
		EXPECT_FALSE(id.empty());
	}

	// New connection should be rejected
	auto overflow = std::make_shared<ws_connection>();
	std::string id = manager.add_connection(overflow);
	EXPECT_TRUE(id.empty());

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.total_rejected, 1);
}

// ============================================================================
// Remove Connection Tests
// ============================================================================

TEST_F(WsSessionManagerTest, RemoveConnection) {
	ws_session_manager manager(config_);

	auto conn = std::make_shared<ws_connection>();
	std::string id = manager.add_connection(conn, "to_remove");

	EXPECT_EQ(manager.get_connection_count(), 1);

	bool removed = manager.remove_connection("to_remove");
	EXPECT_TRUE(removed);
	EXPECT_EQ(manager.get_connection_count(), 0);
}

TEST_F(WsSessionManagerTest, RemoveConnectionNotFound) {
	ws_session_manager manager(config_);

	bool removed = manager.remove_connection("nonexistent");
	EXPECT_FALSE(removed);
}

// ============================================================================
// Get Connection Tests
// ============================================================================

TEST_F(WsSessionManagerTest, GetConnection) {
	ws_session_manager manager(config_);

	auto conn = std::make_shared<ws_connection>("test_conn");
	manager.add_connection(conn, "conn_id");

	auto retrieved = manager.get_connection("conn_id");
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(WsSessionManagerTest, GetConnectionNotFound) {
	ws_session_manager manager(config_);

	auto conn = manager.get_connection("nonexistent");
	EXPECT_EQ(conn, nullptr);
}

TEST_F(WsSessionManagerTest, GetAllConnections) {
	ws_session_manager manager(config_);

	for (int i = 0; i < 5; ++i) {
		manager.add_connection(std::make_shared<ws_connection>());
	}

	auto connections = manager.get_all_connections();
	EXPECT_EQ(connections.size(), 5);
}

TEST_F(WsSessionManagerTest, GetAllConnectionIds) {
	ws_session_manager manager(config_);

	manager.add_connection(std::make_shared<ws_connection>(), "ws_1");
	manager.add_connection(std::make_shared<ws_connection>(), "ws_2");
	manager.add_connection(std::make_shared<ws_connection>(), "ws_3");

	auto ids = manager.get_all_connection_ids();
	EXPECT_EQ(ids.size(), 3);

	std::sort(ids.begin(), ids.end());
	EXPECT_EQ(ids[0], "ws_1");
	EXPECT_EQ(ids[1], "ws_2");
	EXPECT_EQ(ids[2], "ws_3");
}

// ============================================================================
// Clear Connections Tests
// ============================================================================

TEST_F(WsSessionManagerTest, ClearAllConnections) {
	ws_session_manager manager(config_);

	for (int i = 0; i < 10; ++i) {
		manager.add_connection(std::make_shared<ws_connection>());
	}

	EXPECT_EQ(manager.get_connection_count(), 10);

	manager.clear_all_connections();

	EXPECT_EQ(manager.get_connection_count(), 0);
}

// ============================================================================
// ID Generation Tests
// ============================================================================

TEST_F(WsSessionManagerTest, GenerateConnectionIdPrefix) {
	auto id = ws_session_manager::generate_connection_id();

	EXPECT_TRUE(id.find("ws_conn_") == 0);
}

TEST_F(WsSessionManagerTest, GenerateConnectionIdUnique) {
	std::vector<std::string> ids;
	for (int i = 0; i < 100; ++i) {
		ids.push_back(ws_session_manager::generate_connection_id());
	}

	std::sort(ids.begin(), ids.end());
	auto last = std::unique(ids.begin(), ids.end());
	EXPECT_EQ(last, ids.end());
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(WsSessionManagerTest, StatsAccuracy) {
	config_.max_sessions = 5;
	ws_session_manager manager(config_);

	// Add connections
	for (int i = 0; i < 5; ++i) {
		manager.add_connection(std::make_shared<ws_connection>(), "conn_" + std::to_string(i));
	}

	// Try to add more (rejected)
	manager.add_connection(std::make_shared<ws_connection>(), "overflow1");
	manager.add_connection(std::make_shared<ws_connection>(), "overflow2");

	// Remove some
	manager.remove_connection("conn_0");
	manager.remove_connection("conn_1");

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.total_accepted, 5);
	EXPECT_EQ(stats.total_rejected, 2);
	EXPECT_EQ(stats.active_sessions, 3);
}

// ============================================================================
// Backpressure Tests
// ============================================================================

TEST_F(WsSessionManagerTest, BackpressureActivation) {
	config_.max_sessions = 10;
	config_.backpressure_threshold = 0.8;
	ws_session_manager manager(config_);

	// Below threshold (7/10 = 70%)
	for (int i = 0; i < 7; ++i) {
		manager.add_connection(std::make_shared<ws_connection>());
	}
	EXPECT_FALSE(manager.is_backpressure_active());

	// At threshold (8/10 = 80%)
	manager.add_connection(std::make_shared<ws_connection>());
	EXPECT_TRUE(manager.is_backpressure_active());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(WsSessionManagerTest, ConcurrentAddRemove) {
	config_.max_sessions = 10000;
	ws_session_manager manager(config_);

	constexpr int num_threads = 8;
	constexpr int ops_per_thread = 100;

	std::atomic<int> added{0};
	std::atomic<int> removed{0};

	std::vector<std::thread> threads;

	// Add threads
	for (int t = 0; t < num_threads / 2; ++t) {
		threads.emplace_back([&manager, &added, t]() {
			for (int i = 0; i < ops_per_thread; ++i) {
				auto conn = std::make_shared<ws_connection>();
				std::string id = "thread_" + std::to_string(t) + "_conn_" + std::to_string(i);
				if (!manager.add_connection(conn, id).empty()) {
					added.fetch_add(1);
				}
			}
		});
	}

	// Remove threads
	for (int t = 0; t < num_threads / 2; ++t) {
		threads.emplace_back([&manager, &removed, t]() {
			for (int i = 0; i < ops_per_thread; ++i) {
				std::string id = "thread_" + std::to_string(t) + "_conn_" + std::to_string(i);
				if (manager.remove_connection(id)) {
					removed.fetch_add(1);
				}
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.active_sessions, added.load() - removed.load());
}

TEST_F(WsSessionManagerTest, ConcurrentReads) {
	ws_session_manager manager(config_);

	// Add connections
	for (int i = 0; i < 50; ++i) {
		manager.add_connection(std::make_shared<ws_connection>(), "conn_" + std::to_string(i));
	}

	constexpr int num_readers = 8;
	constexpr int reads_per_thread = 500;
	std::atomic<int> successful_reads{0};

	std::vector<std::thread> readers;
	for (int t = 0; t < num_readers; ++t) {
		readers.emplace_back([&manager, &successful_reads]() {
			for (int i = 0; i < reads_per_thread; ++i) {
				auto conn = manager.get_connection("conn_" + std::to_string(i % 50));
				if (conn != nullptr) {
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

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(WsSessionManagerTest, SessionApiAlias) {
	ws_session_manager manager(config_);

	// Test that session API aliases work correctly
	auto conn = std::make_shared<ws_connection>();

	// add_session should work via inheritance
	EXPECT_TRUE(manager.add_session(conn, "via_session_api"));
	EXPECT_EQ(manager.get_session_count(), 1);
	EXPECT_EQ(manager.get_connection_count(), 1);

	// get_session should work
	auto retrieved = manager.get_session("via_session_api");
	EXPECT_NE(retrieved, nullptr);

	// remove_session should work
	EXPECT_TRUE(manager.remove_session("via_session_api"));
	EXPECT_EQ(manager.get_session_count(), 0);
}
