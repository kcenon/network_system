/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file reliable_udp_client_test.cpp
 * @brief Unit tests for reliable_udp_client class
 *
 * Tests validate:
 * - Construction with various reliability modes
 * - Client ID property
 * - Reliability mode property
 * - Initial state (not running)
 * - Initial statistics (all zeros)
 * - Configuration setters (congestion window, retries, timeout)
 * - Error paths (send/stop when not running)
 * - Callback registration
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/reliable_udp_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

class ReliableUdpClientTest : public ::testing::Test
{
};

TEST_F(ReliableUdpClientTest, ConstructWithDefaultMode)
{
	reliable_udp_client client("test-client");

	EXPECT_EQ(client.client_id(), "test-client");
	EXPECT_EQ(client.mode(), reliability_mode::reliable_ordered);
	EXPECT_FALSE(client.is_running());
}

TEST_F(ReliableUdpClientTest, ConstructUnreliableMode)
{
	reliable_udp_client client("unreliable-client", reliability_mode::unreliable);

	EXPECT_EQ(client.client_id(), "unreliable-client");
	EXPECT_EQ(client.mode(), reliability_mode::unreliable);
}

TEST_F(ReliableUdpClientTest, ConstructReliableOrderedMode)
{
	reliable_udp_client client("ordered-client", reliability_mode::reliable_ordered);

	EXPECT_EQ(client.mode(), reliability_mode::reliable_ordered);
}

TEST_F(ReliableUdpClientTest, ConstructReliableUnorderedMode)
{
	reliable_udp_client client("unordered-client", reliability_mode::reliable_unordered);

	EXPECT_EQ(client.mode(), reliability_mode::reliable_unordered);
}

TEST_F(ReliableUdpClientTest, ConstructSequencedMode)
{
	reliable_udp_client client("sequenced-client", reliability_mode::sequenced);

	EXPECT_EQ(client.mode(), reliability_mode::sequenced);
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST_F(ReliableUdpClientTest, InitialStateNotRunning)
{
	reliable_udp_client client("client");
	EXPECT_FALSE(client.is_running());
}

TEST_F(ReliableUdpClientTest, InitialStatsAllZero)
{
	reliable_udp_client client("client");
	auto stats = client.get_stats();

	EXPECT_EQ(stats.packets_sent, 0u);
	EXPECT_EQ(stats.packets_received, 0u);
	EXPECT_EQ(stats.packets_retransmitted, 0u);
	EXPECT_EQ(stats.packets_dropped, 0u);
	EXPECT_EQ(stats.acks_sent, 0u);
	EXPECT_EQ(stats.acks_received, 0u);
	EXPECT_DOUBLE_EQ(stats.average_rtt_ms, 0.0);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ReliableUdpClientTest, SetCongestionWindow)
{
	reliable_udp_client client("client");
	EXPECT_NO_FATAL_FAILURE(client.set_congestion_window(64));
}

TEST_F(ReliableUdpClientTest, SetMaxRetries)
{
	reliable_udp_client client("client");
	EXPECT_NO_FATAL_FAILURE(client.set_max_retries(10));
}

TEST_F(ReliableUdpClientTest, SetRetransmissionTimeout)
{
	reliable_udp_client client("client");
	EXPECT_NO_FATAL_FAILURE(client.set_retransmission_timeout(500));
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(ReliableUdpClientTest, SetReceiveCallback)
{
	reliable_udp_client client("client");

	EXPECT_NO_FATAL_FAILURE(
		client.set_receive_callback([](const std::vector<uint8_t>&) {}));
}

TEST_F(ReliableUdpClientTest, SetErrorCallback)
{
	reliable_udp_client client("client");

	EXPECT_NO_FATAL_FAILURE(
		client.set_error_callback([](std::error_code) {}));
}

TEST_F(ReliableUdpClientTest, SetNullCallbacks)
{
	reliable_udp_client client("client");

	EXPECT_NO_FATAL_FAILURE(client.set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client.set_error_callback(nullptr));
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST_F(ReliableUdpClientTest, SendWhenNotRunningFails)
{
	reliable_udp_client client("client");

	auto result = client.send_packet(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST_F(ReliableUdpClientTest, StopWhenNotRunningSucceeds)
{
	reliable_udp_client client("client");

	auto result = client.stop_client();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Stats Structure Tests
// ============================================================================

TEST_F(ReliableUdpClientTest, StatsStructDefaultValues)
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

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(ReliableUdpClientTest, DestructorWhenNotRunning)
{
	{
		reliable_udp_client client("client");
	}
	// No crash = success
}
