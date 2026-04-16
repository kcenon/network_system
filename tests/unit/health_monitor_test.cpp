/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file health_monitor_test.cpp
 * @brief Unit tests for health_monitor and connection_health
 *
 * Tests validate:
 * - connection_health default values
 * - connection_health field assignment
 * - health_monitor construction with default and custom parameters
 * - health_monitor initial state (not monitoring)
 * - health_monitor callback registration
 * - health_monitor get_health before monitoring
 * - health_monitor stop when not monitoring
 * - health_monitor destructor safety
 *
 * Note: Tests requiring a live messaging_client connection are covered
 * in integration tests. These unit tests focus on construction, state
 * queries, and safe behavior without network.
 */

#include "internal/utils/health_monitor.h"

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>

using namespace kcenon::network::utils;

// ============================================================================
// connection_health Tests
// ============================================================================

TEST(ConnectionHealthTest, DefaultValues)
{
	connection_health health;

	EXPECT_TRUE(health.is_alive);
	EXPECT_EQ(health.last_response_time, std::chrono::milliseconds{0});
	EXPECT_EQ(health.missed_heartbeats, 0u);
	EXPECT_DOUBLE_EQ(health.packet_loss_rate, 0.0);
}

TEST(ConnectionHealthTest, FieldAssignment)
{
	connection_health health;
	health.is_alive = false;
	health.last_response_time = std::chrono::milliseconds{150};
	health.missed_heartbeats = 3;
	health.packet_loss_rate = 0.25;
	health.last_heartbeat = std::chrono::steady_clock::now();

	EXPECT_FALSE(health.is_alive);
	EXPECT_EQ(health.last_response_time, std::chrono::milliseconds{150});
	EXPECT_EQ(health.missed_heartbeats, 3u);
	EXPECT_DOUBLE_EQ(health.packet_loss_rate, 0.25);
}

// ============================================================================
// health_monitor Construction Tests
// ============================================================================

TEST(HealthMonitorTest, DefaultConstruction)
{
	auto monitor = std::make_shared<health_monitor>();

	EXPECT_FALSE(monitor->is_monitoring());
}

TEST(HealthMonitorTest, CustomInterval)
{
	auto monitor = std::make_shared<health_monitor>(std::chrono::seconds(10));

	EXPECT_FALSE(monitor->is_monitoring());
}

TEST(HealthMonitorTest, CustomIntervalAndMaxMissed)
{
	auto monitor = std::make_shared<health_monitor>(std::chrono::seconds(15), 5);

	EXPECT_FALSE(monitor->is_monitoring());
}

// ============================================================================
// health_monitor State Tests
// ============================================================================

TEST(HealthMonitorTest, GetHealthBeforeMonitoring)
{
	auto monitor = std::make_shared<health_monitor>();
	auto health = monitor->get_health();

	// Before monitoring starts, health should have default values
	EXPECT_TRUE(health.is_alive);
	EXPECT_EQ(health.missed_heartbeats, 0u);
	EXPECT_DOUBLE_EQ(health.packet_loss_rate, 0.0);
}

TEST(HealthMonitorTest, StopWhenNotMonitoring)
{
	auto monitor = std::make_shared<health_monitor>();

	// Stop when not monitoring should be safe (no-op)
	EXPECT_NO_FATAL_FAILURE(monitor->stop_monitoring());
	EXPECT_FALSE(monitor->is_monitoring());
}

// ============================================================================
// health_monitor Callback Tests
// ============================================================================

TEST(HealthMonitorTest, SetHealthCallback)
{
	auto monitor = std::make_shared<health_monitor>();

	bool callback_set = false;
	EXPECT_NO_FATAL_FAILURE(
		monitor->set_health_callback([&callback_set](const connection_health&) {
			callback_set = true;
		}));
}

TEST(HealthMonitorTest, SetNullCallback)
{
	auto monitor = std::make_shared<health_monitor>();

	EXPECT_NO_FATAL_FAILURE(
		monitor->set_health_callback(nullptr));
}

// ============================================================================
// health_monitor Destructor Safety Tests
// ============================================================================

TEST(HealthMonitorTest, DestructorWhenNotMonitoring)
{
	// Destructor should be safe when not monitoring
	EXPECT_NO_FATAL_FAILURE({
		auto monitor = std::make_shared<health_monitor>();
		// monitor goes out of scope
	});
}
