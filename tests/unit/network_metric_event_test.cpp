/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/events/network_metric_event.h"
#include <gtest/gtest.h>

#include <string>

namespace events = kcenon::network::events;

/**
 * @file network_metric_event_test.cpp
 * @brief Unit tests for network metric event structs
 *
 * Tests validate:
 * - network_metric_type enum values
 * - network_metric_event default and parametric constructors
 * - network_connection_event default and parametric constructors
 * - network_transfer_event default and parametric constructors
 * - network_latency_event default and parametric constructors
 * - network_health_event default and parametric constructors
 * - Copy and move semantics for all event types
 */

// ============================================================================
// network_metric_type Enum Tests
// ============================================================================

class NetworkMetricTypeTest : public ::testing::Test
{
};

TEST_F(NetworkMetricTypeTest, EnumValuesAreDistinct)
{
	auto counter = events::network_metric_type::counter;
	auto gauge = events::network_metric_type::gauge;
	auto histogram = events::network_metric_type::histogram;
	auto summary = events::network_metric_type::summary;

	EXPECT_NE(counter, gauge);
	EXPECT_NE(counter, histogram);
	EXPECT_NE(counter, summary);
	EXPECT_NE(gauge, histogram);
	EXPECT_NE(gauge, summary);
	EXPECT_NE(histogram, summary);
}

// ============================================================================
// network_metric_event Tests
// ============================================================================

class NetworkMetricEventTest : public ::testing::Test
{
};

TEST_F(NetworkMetricEventTest, DefaultConstructor)
{
	events::network_metric_event event;

	EXPECT_TRUE(event.name.empty());
	EXPECT_DOUBLE_EQ(event.value, 0.0);
	EXPECT_TRUE(event.unit.empty());
	EXPECT_EQ(event.type, events::network_metric_type::counter);
	EXPECT_TRUE(event.labels.empty());
}

TEST_F(NetworkMetricEventTest, ParametricConstructorMinimal)
{
	events::network_metric_event event("cpu.usage", 75.5);

	EXPECT_EQ(event.name, "cpu.usage");
	EXPECT_DOUBLE_EQ(event.value, 75.5);
	EXPECT_EQ(event.type, events::network_metric_type::counter);
	EXPECT_TRUE(event.labels.empty());
	EXPECT_TRUE(event.unit.empty());
}

TEST_F(NetworkMetricEventTest, ParametricConstructorFull)
{
	std::map<std::string, std::string> labels = {{"host", "server1"},
												  {"region", "us-east"}};
	events::network_metric_event event("network.bytes_sent", 1024.0,
									   events::network_metric_type::histogram,
									   labels, "bytes");

	EXPECT_EQ(event.name, "network.bytes_sent");
	EXPECT_DOUBLE_EQ(event.value, 1024.0);
	EXPECT_EQ(event.type, events::network_metric_type::histogram);
	EXPECT_EQ(event.unit, "bytes");
	EXPECT_EQ(event.labels.size(), 2);
	EXPECT_EQ(event.labels.at("host"), "server1");
}

TEST_F(NetworkMetricEventTest, TimestampIsSet)
{
	auto before = std::chrono::steady_clock::now();
	events::network_metric_event event("test", 1.0);
	auto after = std::chrono::steady_clock::now();

	EXPECT_GE(event.timestamp, before);
	EXPECT_LE(event.timestamp, after);
}

TEST_F(NetworkMetricEventTest, CopySemantics)
{
	events::network_metric_event original("test.metric", 42.0,
										  events::network_metric_type::gauge);

	events::network_metric_event copy(original);

	EXPECT_EQ(copy.name, original.name);
	EXPECT_DOUBLE_EQ(copy.value, original.value);
	EXPECT_EQ(copy.type, original.type);
}

TEST_F(NetworkMetricEventTest, MoveSemantics)
{
	events::network_metric_event original("test.metric", 42.0);
	std::string original_name = original.name;

	events::network_metric_event moved(std::move(original));

	EXPECT_EQ(moved.name, original_name);
	EXPECT_DOUBLE_EQ(moved.value, 42.0);
}

// ============================================================================
// network_connection_event Tests
// ============================================================================

class NetworkConnectionEventTest : public ::testing::Test
{
};

TEST_F(NetworkConnectionEventTest, DefaultConstructor)
{
	events::network_connection_event event;

	EXPECT_TRUE(event.connection_id.empty());
	EXPECT_TRUE(event.event_type.empty());
	EXPECT_TRUE(event.protocol.empty());
	EXPECT_TRUE(event.remote_address.empty());
	EXPECT_TRUE(event.labels.empty());
}

TEST_F(NetworkConnectionEventTest, ParametricConstructorMinimal)
{
	events::network_connection_event event("conn-123", "accepted");

	EXPECT_EQ(event.connection_id, "conn-123");
	EXPECT_EQ(event.event_type, "accepted");
	EXPECT_EQ(event.protocol, "tcp");
	EXPECT_TRUE(event.remote_address.empty());
}

TEST_F(NetworkConnectionEventTest, ParametricConstructorFull)
{
	std::map<std::string, std::string> labels = {{"tls", "true"}};
	events::network_connection_event event("conn-456", "closed", "websocket",
										   "192.168.1.1:8080", labels);

	EXPECT_EQ(event.connection_id, "conn-456");
	EXPECT_EQ(event.event_type, "closed");
	EXPECT_EQ(event.protocol, "websocket");
	EXPECT_EQ(event.remote_address, "192.168.1.1:8080");
	EXPECT_EQ(event.labels.at("tls"), "true");
}

TEST_F(NetworkConnectionEventTest, CopyAndMoveSemantics)
{
	events::network_connection_event original("conn-1", "accepted", "quic");

	events::network_connection_event copy(original);
	EXPECT_EQ(copy.connection_id, "conn-1");
	EXPECT_EQ(copy.protocol, "quic");

	events::network_connection_event moved(std::move(copy));
	EXPECT_EQ(moved.connection_id, "conn-1");
}

// ============================================================================
// network_transfer_event Tests
// ============================================================================

class NetworkTransferEventTest : public ::testing::Test
{
};

TEST_F(NetworkTransferEventTest, DefaultConstructor)
{
	events::network_transfer_event event;

	EXPECT_TRUE(event.connection_id.empty());
	EXPECT_TRUE(event.direction.empty());
	EXPECT_EQ(event.bytes, 0);
	EXPECT_EQ(event.packets, 0);
	EXPECT_TRUE(event.labels.empty());
}

TEST_F(NetworkTransferEventTest, ParametricConstructorMinimal)
{
	events::network_transfer_event event("conn-1", "sent", 4096);

	EXPECT_EQ(event.connection_id, "conn-1");
	EXPECT_EQ(event.direction, "sent");
	EXPECT_EQ(event.bytes, 4096);
	EXPECT_EQ(event.packets, 1);
}

TEST_F(NetworkTransferEventTest, ParametricConstructorFull)
{
	std::map<std::string, std::string> labels = {{"stream", "0"}};
	events::network_transfer_event event("conn-2", "received", 65536, 10,
										 labels);

	EXPECT_EQ(event.bytes, 65536);
	EXPECT_EQ(event.packets, 10);
	EXPECT_EQ(event.labels.at("stream"), "0");
}

TEST_F(NetworkTransferEventTest, CopyAndMoveSemantics)
{
	events::network_transfer_event original("conn-1", "sent", 1024, 2);

	events::network_transfer_event copy(original);
	EXPECT_EQ(copy.bytes, 1024);

	events::network_transfer_event moved(std::move(copy));
	EXPECT_EQ(moved.bytes, 1024);
	EXPECT_EQ(moved.packets, 2);
}

// ============================================================================
// network_latency_event Tests
// ============================================================================

class NetworkLatencyEventTest : public ::testing::Test
{
};

TEST_F(NetworkLatencyEventTest, DefaultConstructor)
{
	events::network_latency_event event;

	EXPECT_TRUE(event.connection_id.empty());
	EXPECT_DOUBLE_EQ(event.latency_ms, 0.0);
	EXPECT_TRUE(event.operation.empty());
	EXPECT_TRUE(event.labels.empty());
}

TEST_F(NetworkLatencyEventTest, ParametricConstructorMinimal)
{
	events::network_latency_event event("conn-1", 15.5);

	EXPECT_EQ(event.connection_id, "conn-1");
	EXPECT_DOUBLE_EQ(event.latency_ms, 15.5);
	EXPECT_EQ(event.operation, "roundtrip");
}

TEST_F(NetworkLatencyEventTest, ParametricConstructorFull)
{
	std::map<std::string, std::string> labels = {{"endpoint", "/api/data"}};
	events::network_latency_event event("conn-2", 250.0, "request", labels);

	EXPECT_EQ(event.connection_id, "conn-2");
	EXPECT_DOUBLE_EQ(event.latency_ms, 250.0);
	EXPECT_EQ(event.operation, "request");
	EXPECT_EQ(event.labels.at("endpoint"), "/api/data");
}

TEST_F(NetworkLatencyEventTest, CopyAndMoveSemantics)
{
	events::network_latency_event original("conn-1", 100.0, "response");

	events::network_latency_event copy(original);
	EXPECT_DOUBLE_EQ(copy.latency_ms, 100.0);

	events::network_latency_event moved(std::move(copy));
	EXPECT_DOUBLE_EQ(moved.latency_ms, 100.0);
	EXPECT_EQ(moved.operation, "response");
}

// ============================================================================
// network_health_event Tests
// ============================================================================

class NetworkHealthEventTest : public ::testing::Test
{
};

TEST_F(NetworkHealthEventTest, DefaultConstructor)
{
	events::network_health_event event;

	EXPECT_TRUE(event.connection_id.empty());
	EXPECT_FALSE(event.is_alive);
	EXPECT_DOUBLE_EQ(event.response_time_ms, 0.0);
	EXPECT_EQ(event.missed_heartbeats, 0);
	EXPECT_DOUBLE_EQ(event.packet_loss_rate, 0.0);
	EXPECT_TRUE(event.labels.empty());
}

TEST_F(NetworkHealthEventTest, ParametricConstructorMinimal)
{
	events::network_health_event event("conn-1", true);

	EXPECT_EQ(event.connection_id, "conn-1");
	EXPECT_TRUE(event.is_alive);
	EXPECT_DOUBLE_EQ(event.response_time_ms, 0.0);
	EXPECT_EQ(event.missed_heartbeats, 0);
	EXPECT_DOUBLE_EQ(event.packet_loss_rate, 0.0);
}

TEST_F(NetworkHealthEventTest, ParametricConstructorFull)
{
	std::map<std::string, std::string> labels = {{"server", "primary"}};
	events::network_health_event event("conn-2", false, 500.0, 3, 0.15,
									   labels);

	EXPECT_EQ(event.connection_id, "conn-2");
	EXPECT_FALSE(event.is_alive);
	EXPECT_DOUBLE_EQ(event.response_time_ms, 500.0);
	EXPECT_EQ(event.missed_heartbeats, 3);
	EXPECT_DOUBLE_EQ(event.packet_loss_rate, 0.15);
	EXPECT_EQ(event.labels.at("server"), "primary");
}

TEST_F(NetworkHealthEventTest, CopyAndMoveSemantics)
{
	events::network_health_event original("conn-1", true, 10.0, 0, 0.0);

	events::network_health_event copy(original);
	EXPECT_TRUE(copy.is_alive);
	EXPECT_DOUBLE_EQ(copy.response_time_ms, 10.0);

	events::network_health_event moved(std::move(copy));
	EXPECT_TRUE(moved.is_alive);
	EXPECT_EQ(moved.connection_id, "conn-1");
}

TEST_F(NetworkHealthEventTest, AliveToDeadTransition)
{
	events::network_health_event alive("conn-1", true, 5.0, 0, 0.0);
	events::network_health_event dead("conn-1", false, 0.0, 4, 0.75);

	EXPECT_TRUE(alive.is_alive);
	EXPECT_FALSE(dead.is_alive);
	EXPECT_GT(dead.missed_heartbeats, alive.missed_heartbeats);
	EXPECT_GT(dead.packet_loss_rate, alive.packet_loss_rate);
}
