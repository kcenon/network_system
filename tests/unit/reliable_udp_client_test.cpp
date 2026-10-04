/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file reliable_udp_client_test.cpp
 * @brief Unit tests for reliable_udp_client enums, stats, and header-only types
 *
 * Tests validate:
 * - reliability_mode enum values
 * - reliable_udp_stats default values and field assignment
 *
 * Note: reliable_udp_client implementation is in libs/network-udp, NOT in the
 * main network_system library (Issue #561). Therefore, construction and method
 * tests are excluded to avoid linker errors with add_network_test. Only
 * header-only types (enums, POD structs) are tested here.
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/reliable_udp_client.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace kcenon::network::core;

// ============================================================================
// reliability_mode Enum Tests
// ============================================================================

TEST(ReliabilityModeTest, EnumValues)
{
	// Verify all enum values are distinct
	EXPECT_NE(reliability_mode::unreliable, reliability_mode::reliable_ordered);
	EXPECT_NE(reliability_mode::reliable_ordered, reliability_mode::reliable_unordered);
	EXPECT_NE(reliability_mode::reliable_unordered, reliability_mode::sequenced);
	EXPECT_NE(reliability_mode::unreliable, reliability_mode::sequenced);
}

TEST(ReliabilityModeTest, Assignment)
{
	reliability_mode mode = reliability_mode::unreliable;
	EXPECT_EQ(mode, reliability_mode::unreliable);

	mode = reliability_mode::reliable_ordered;
	EXPECT_EQ(mode, reliability_mode::reliable_ordered);

	mode = reliability_mode::reliable_unordered;
	EXPECT_EQ(mode, reliability_mode::reliable_unordered);

	mode = reliability_mode::sequenced;
	EXPECT_EQ(mode, reliability_mode::sequenced);
}

// ============================================================================
// reliable_udp_stats Tests
// ============================================================================

TEST(ReliableUdpStatsTest, DefaultValues)
{
	reliable_udp_stats stats;

	EXPECT_EQ(stats.packets_sent, 0u);
	EXPECT_EQ(stats.packets_received, 0u);
	EXPECT_EQ(stats.packets_retransmitted, 0u);
	EXPECT_EQ(stats.packets_dropped, 0u);
	EXPECT_EQ(stats.acks_sent, 0u);
	EXPECT_EQ(stats.acks_received, 0u);
	EXPECT_DOUBLE_EQ(stats.average_rtt_ms, 0.0);
}

TEST(ReliableUdpStatsTest, FieldAssignment)
{
	reliable_udp_stats stats;
	stats.packets_sent = 1000;
	stats.packets_received = 990;
	stats.packets_retransmitted = 15;
	stats.packets_dropped = 5;
	stats.acks_sent = 990;
	stats.acks_received = 985;
	stats.average_rtt_ms = 45.5;

	EXPECT_EQ(stats.packets_sent, 1000u);
	EXPECT_EQ(stats.packets_received, 990u);
	EXPECT_EQ(stats.packets_retransmitted, 15u);
	EXPECT_EQ(stats.packets_dropped, 5u);
	EXPECT_EQ(stats.acks_sent, 990u);
	EXPECT_EQ(stats.acks_received, 985u);
	EXPECT_DOUBLE_EQ(stats.average_rtt_ms, 45.5);
}

TEST(ReliableUdpStatsTest, CopySemantics)
{
	reliable_udp_stats original;
	original.packets_sent = 100;
	original.average_rtt_ms = 12.3;

	reliable_udp_stats copy = original;

	EXPECT_EQ(copy.packets_sent, 100u);
	EXPECT_DOUBLE_EQ(copy.average_rtt_ms, 12.3);
}
