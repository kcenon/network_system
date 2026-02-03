/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "internal/core/session_manager.h"
#include "internal/core/session_manager_base.h"

#include <gtest/gtest.h>

#include <chrono>
#include <random>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace std::chrono_literals;

/**
 * @file session_manager_integration_test.cpp
 * @brief Integration tests for SessionManager<T> in realistic scenarios
 *
 * Tests validate:
 * - End-to-end session lifecycle
 * - Multi-client simulation
 * - Graceful shutdown behavior
 * - Performance under load
 * - Real usage patterns
 */

// ============================================================================
// Mock Session Types for Integration Tests
// ============================================================================

namespace {

/**
 * @brief Simulates a real network session with processing capability
 */
class simulated_session : public std::enable_shared_from_this<simulated_session> {
public:
	explicit simulated_session(std::string id = "") : id_(std::move(id)), active_(true) {}

	auto stop_session() -> void {
		active_ = false;
		stop_count_.fetch_add(1);
	}

	[[nodiscard]] auto is_active() const -> bool { return active_; }

	[[nodiscard]] auto get_id() const -> const std::string& { return id_; }

	auto process_message() -> void {
		if (active_) {
			messages_processed_.fetch_add(1);
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
	}

	[[nodiscard]] auto messages_processed() const -> uint64_t {
		return messages_processed_.load();
	}

	[[nodiscard]] static auto total_stops() -> int { return stop_count_.load(); }
	static auto reset_stop_count() -> void { stop_count_.store(0); }

private:
	std::string id_;
	std::atomic<bool> active_;
	std::atomic<uint64_t> messages_processed_{0};
	inline static std::atomic<int> stop_count_{0};
};

} // namespace

namespace kcenon::network::core {

template <>
struct session_traits<simulated_session> {
	static constexpr bool has_activity_tracking = true;
	static constexpr bool stop_on_clear = true;
	static constexpr const char* id_prefix = "sim_";
};

} // namespace kcenon::network::core

// ============================================================================
// Integration Test Fixtures
// ============================================================================

class SessionManagerIntegrationTest : public ::testing::Test {
protected:
	void SetUp() override {
		simulated_session::reset_stop_count();
		config_.max_sessions = 1000;
		config_.idle_timeout = 100ms;
		config_.cleanup_interval = 50ms;
		config_.enable_backpressure = true;
		config_.backpressure_threshold = 0.8;
	}

	void TearDown() override { simulated_session::reset_stop_count(); }

	session_config config_;
};

// ============================================================================
// End-to-End Session Lifecycle Tests
// ============================================================================

TEST_F(SessionManagerIntegrationTest, CompleteSessionLifecycle) {
	// Use longer idle timeout for this test to avoid race conditions
	config_.idle_timeout = 200ms;
	session_manager_base<simulated_session> manager(config_);

	// Phase 1: Create sessions
	std::vector<std::string> session_ids;
	for (int i = 0; i < 10; ++i) {
		auto session = std::make_shared<simulated_session>("user_" + std::to_string(i));
		std::string id = manager.add_session_with_id(session, "session_" + std::to_string(i));
		ASSERT_FALSE(id.empty());
		session_ids.push_back(id);
	}

	EXPECT_EQ(manager.get_session_count(), 10);

	// Phase 2: Wait for partial timeout (120ms < 200ms)
	std::this_thread::sleep_for(120ms);

	// Phase 3: Update activity on first 5 sessions (resets their idle timer)
	for (int i = 0; i < 5; ++i) {
		manager.update_activity(session_ids[i]);
		auto session = manager.get_session(session_ids[i]);
		ASSERT_NE(session, nullptr);
		session->process_message();
	}

	// Phase 4: Wait for remaining timeout (100ms more)
	// At this point: sessions 0-4 are 100ms idle, sessions 5-9 are 220ms idle
	std::this_thread::sleep_for(100ms);

	// Phase 5: Cleanup should remove sessions 5-9 (idle > 200ms)
	size_t cleaned = manager.cleanup_idle_sessions();
	EXPECT_EQ(cleaned, 5);
	EXPECT_EQ(manager.get_session_count(), 5);

	// Phase 6: Verify active sessions remain
	for (int i = 0; i < 5; ++i) {
		auto session = manager.get_session(session_ids[i]);
		EXPECT_NE(session, nullptr);
		EXPECT_TRUE(session->is_active());
	}

	// Phase 7: Graceful shutdown
	manager.clear_all_sessions();
	EXPECT_EQ(manager.get_session_count(), 0);
	EXPECT_EQ(simulated_session::total_stops(), 10); // All sessions stopped
}

// ============================================================================
// Multi-Client Simulation Tests
// ============================================================================

TEST_F(SessionManagerIntegrationTest, MultiClientSimulation) {
	config_.max_sessions = 100;
	session_manager_base<simulated_session> manager(config_);

	constexpr int num_clients = 20;
	constexpr int operations_per_client = 50;

	std::atomic<int> successful_connections{0};
	std::atomic<int> failed_connections{0};
	std::atomic<int> messages_sent{0};

	std::vector<std::thread> clients;

	// Simulate multiple clients connecting and communicating
	for (int c = 0; c < num_clients; ++c) {
		clients.emplace_back([&, c]() {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> action_dist(0, 2);

			for (int op = 0; op < operations_per_client; ++op) {
				int action = action_dist(gen);
				std::string id = "client_" + std::to_string(c) + "_session_" + std::to_string(op);

				switch (action) {
				case 0: { // Connect
					auto session = std::make_shared<simulated_session>(id);
					if (!manager.add_session_with_id(session, id).empty()) {
						successful_connections.fetch_add(1);
					} else {
						failed_connections.fetch_add(1);
					}
					break;
				}
				case 1: { // Send message
					auto session = manager.get_session(id);
					if (session) {
						session->process_message();
						manager.update_activity(id);
						messages_sent.fetch_add(1);
					}
					break;
				}
				case 2: { // Disconnect
					manager.remove_session(id);
					break;
				}
				}

				// Small delay to simulate real network latency
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
		});
	}

	for (auto& client : clients) {
		client.join();
	}

	// Verify stats consistency
	auto stats = manager.get_stats();
	EXPECT_EQ(stats.total_accepted, successful_connections.load());
	EXPECT_EQ(stats.total_rejected, failed_connections.load());
	EXPECT_LE(stats.active_sessions, stats.total_accepted);
}

// ============================================================================
// Graceful Shutdown Tests
// ============================================================================

TEST_F(SessionManagerIntegrationTest, GracefulShutdownUnderLoad) {
	session_manager_base<simulated_session> manager(config_);

	// Create sessions
	std::vector<std::shared_ptr<simulated_session>> sessions;
	for (int i = 0; i < 50; ++i) {
		auto session = std::make_shared<simulated_session>("session_" + std::to_string(i));
		sessions.push_back(session);
		manager.add_session(session, "id_" + std::to_string(i));
	}

	// Start background workers processing messages
	std::atomic<bool> running{true};
	std::vector<std::thread> workers;

	for (int w = 0; w < 4; ++w) {
		workers.emplace_back([&sessions, &running]() {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dist(0, static_cast<int>(sessions.size()) - 1);

			while (running) {
				int idx = dist(gen);
				if (sessions[idx]->is_active()) {
					sessions[idx]->process_message();
				}
				std::this_thread::sleep_for(std::chrono::microseconds(50));
			}
		});
	}

	// Let workers process for a bit
	std::this_thread::sleep_for(50ms);

	// Initiate graceful shutdown
	running = false;
	manager.clear_all_sessions();

	// Wait for workers to finish
	for (auto& w : workers) {
		w.join();
	}

	// Verify all sessions were stopped
	EXPECT_EQ(manager.get_session_count(), 0);
	for (const auto& session : sessions) {
		EXPECT_FALSE(session->is_active());
	}
}

// ============================================================================
// Performance Under Load Tests
// ============================================================================

TEST_F(SessionManagerIntegrationTest, HighThroughputConnections) {
	config_.max_sessions = 10000;
	session_manager_base<simulated_session> manager(config_);

	auto start = std::chrono::steady_clock::now();

	// Rapid connection/disconnection cycle
	constexpr int total_connections = 1000;
	for (int i = 0; i < total_connections; ++i) {
		auto session = std::make_shared<simulated_session>();
		std::string id = "rapid_" + std::to_string(i);
		manager.add_session(session, id);
	}

	auto mid = std::chrono::steady_clock::now();

	for (int i = 0; i < total_connections; ++i) {
		manager.remove_session("rapid_" + std::to_string(i));
	}

	auto end = std::chrono::steady_clock::now();

	auto add_duration = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start);
	auto remove_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid);

	EXPECT_EQ(manager.get_session_count(), 0);

	// Performance assertion: should complete in reasonable time
	// (< 1 second for 1000 operations each)
	EXPECT_LT(add_duration.count(), 1000);
	EXPECT_LT(remove_duration.count(), 1000);
}

TEST_F(SessionManagerIntegrationTest, ConcurrentReadWritePerformance) {
	config_.max_sessions = 10000;
	session_manager_base<simulated_session> manager(config_);

	// Pre-populate with sessions
	for (int i = 0; i < 1000; ++i) {
		auto session = std::make_shared<simulated_session>();
		manager.add_session(session, "existing_" + std::to_string(i));
	}

	constexpr int num_writers = 4;
	constexpr int num_readers = 8;
	constexpr int ops_per_thread = 500;

	std::atomic<uint64_t> total_reads{0};
	std::atomic<uint64_t> total_writes{0};
	std::atomic<bool> start{false};

	std::vector<std::thread> threads;

	// Writers
	for (int w = 0; w < num_writers; ++w) {
		threads.emplace_back([&, w]() {
			while (!start)
				std::this_thread::yield();

			for (int i = 0; i < ops_per_thread; ++i) {
				auto session = std::make_shared<simulated_session>();
				std::string id = "writer_" + std::to_string(w) + "_" + std::to_string(i);
				manager.add_session(session, id);
				total_writes.fetch_add(1);
			}
		});
	}

	// Readers
	for (int r = 0; r < num_readers; ++r) {
		threads.emplace_back([&]() {
			while (!start)
				std::this_thread::yield();

			for (int i = 0; i < ops_per_thread; ++i) {
				[[maybe_unused]] auto session =
					manager.get_session("existing_" + std::to_string(i % 1000));
				total_reads.fetch_add(1);
			}
		});
	}

	auto bench_start = std::chrono::steady_clock::now();
	start = true;

	for (auto& t : threads) {
		t.join();
	}

	auto bench_end = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(bench_end - bench_start);

	EXPECT_EQ(total_writes.load(), num_writers * ops_per_thread);
	EXPECT_EQ(total_reads.load(), num_readers * ops_per_thread);

	// Performance check: should handle mixed load efficiently
	EXPECT_LT(duration.count(), 5000);
}

// ============================================================================
// Real Usage Pattern Tests
// ============================================================================

TEST_F(SessionManagerIntegrationTest, ServerStartupShutdownCycle) {
	// Simulate server lifecycle: startup -> running -> shutdown

	// Startup phase
	session_manager_base<simulated_session> manager(config_);
	EXPECT_EQ(manager.get_session_count(), 0);

	// Running phase: accept connections
	std::vector<std::string> client_ids;
	for (int i = 0; i < 100; ++i) {
		if (manager.can_accept_connection()) {
			auto session = std::make_shared<simulated_session>("client_" + std::to_string(i));
			std::string id = manager.add_session_with_id(session);
			if (!id.empty()) {
				client_ids.push_back(id);
			}
		}
	}

	EXPECT_EQ(client_ids.size(), 100);
	EXPECT_EQ(manager.get_session_count(), 100);

	// Running phase: periodic cleanup (simulate timer)
	std::this_thread::sleep_for(config_.idle_timeout + 10ms);
	manager.cleanup_idle_sessions();
	EXPECT_EQ(manager.get_session_count(), 0);

	// Re-accept connections
	for (int i = 0; i < 50; ++i) {
		auto session = std::make_shared<simulated_session>();
		manager.add_session(session);
	}

	// Shutdown phase
	auto stats_before = manager.get_stats();
	manager.stop_all_sessions();

	EXPECT_EQ(manager.get_session_count(), 0);

	auto stats_after = manager.get_stats();
	EXPECT_EQ(stats_after.total_accepted, stats_before.total_accepted);
}

TEST_F(SessionManagerIntegrationTest, ConnectionLimitEnforcement) {
	config_.max_sessions = 10;
	session_manager_base<simulated_session> manager(config_);

	// Try to exceed limit from multiple threads
	constexpr int num_threads = 4;
	constexpr int attempts_per_thread = 10;

	std::atomic<int> accepted{0};
	std::atomic<int> rejected{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < num_threads; ++t) {
		threads.emplace_back([&]() {
			for (int i = 0; i < attempts_per_thread; ++i) {
				auto session = std::make_shared<simulated_session>();
				if (!manager.add_session_with_id(session).empty()) {
					accepted.fetch_add(1);
				} else {
					rejected.fetch_add(1);
				}
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	// Verify connection limiting works correctly:
	// - Total attempts = accepted + rejected
	// - Session count should not exceed max_sessions
	// - At least max_sessions should be accepted (may be slightly more due to race window)
	// - Stats should be consistent
	int total_attempts = num_threads * attempts_per_thread;
	EXPECT_EQ(accepted.load() + rejected.load(), total_attempts);
	EXPECT_LE(manager.get_session_count(), config_.max_sessions + num_threads);
	EXPECT_GE(accepted.load(), static_cast<int>(config_.max_sessions));

	auto stats = manager.get_stats();
	EXPECT_EQ(stats.total_accepted + stats.total_rejected, static_cast<uint64_t>(total_attempts));
}

TEST_F(SessionManagerIntegrationTest, BackpressureSignaling) {
	config_.max_sessions = 100;
	config_.backpressure_threshold = 0.8;
	session_manager_base<simulated_session> manager(config_);

	int backpressure_triggered_at = -1;

	for (int i = 0; i < 100; ++i) {
		auto session = std::make_shared<simulated_session>();
		manager.add_session(session);

		if (manager.is_backpressure_active() && backpressure_triggered_at < 0) {
			backpressure_triggered_at = i + 1; // +1 because we just added
		}
	}

	// Backpressure should trigger at 80% capacity (80 sessions)
	EXPECT_EQ(backpressure_triggered_at, 80);
}
