/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/flow_control.h"
#include <gtest/gtest.h>

#include <cstdint>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_flow_control_test.cpp
 * @brief Unit tests for QUIC connection-level flow control (RFC 9000 Section 4)
 *
 * Tests validate:
 * - flow_controller constructor and default window size
 * - Send side: consume_send_window(), available_send_window(), is_send_blocked()
 * - Send limit updates via update_send_limit()
 * - Receive side: record_received(), record_consumed()
 * - MAX_DATA generation via should_send_max_data(), generate_max_data()
 * - DATA_BLOCKED tracking
 * - Window size and update_threshold configuration
 * - reset() restores state
 * - Error codes (send_blocked, receive_overflow, window_exceeded)
 * - get_flow_control_stats() utility
 */

// ============================================================================
// Constructor Tests
// ============================================================================

class FlowControllerConstructorTest : public ::testing::Test
{
};

TEST_F(FlowControllerConstructorTest, DefaultWindowSize)
{
	quic::flow_controller fc;

	EXPECT_EQ(fc.window_size(), 1048576u); // 1MB default
}

TEST_F(FlowControllerConstructorTest, CustomWindowSize)
{
	quic::flow_controller fc(65536);

	EXPECT_EQ(fc.window_size(), 65536u);
}

TEST_F(FlowControllerConstructorTest, InitialBytesSentZero)
{
	quic::flow_controller fc;

	EXPECT_EQ(fc.bytes_sent(), 0u);
}

TEST_F(FlowControllerConstructorTest, InitialBytesReceivedZero)
{
	quic::flow_controller fc;

	EXPECT_EQ(fc.bytes_received(), 0u);
}

TEST_F(FlowControllerConstructorTest, InitialBytesConsumedZero)
{
	quic::flow_controller fc;

	EXPECT_EQ(fc.bytes_consumed(), 0u);
}

// ============================================================================
// Send Side Tests
// ============================================================================

class FlowControllerSendTest : public ::testing::Test
{
protected:
	quic::flow_controller fc_{65536};

	void SetUp() override
	{
		fc_.update_send_limit(65536);
	}
};

TEST_F(FlowControllerSendTest, AvailableSendWindowInitial)
{
	EXPECT_EQ(fc_.available_send_window(), 65536u);
}

TEST_F(FlowControllerSendTest, ConsumeSendWindowSuccess)
{
	auto result = fc_.consume_send_window(1000);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(fc_.bytes_sent(), 1000u);
	EXPECT_EQ(fc_.available_send_window(), 65536u - 1000u);
}

TEST_F(FlowControllerSendTest, ConsumeSendWindowMultiple)
{
	fc_.consume_send_window(1000);
	fc_.consume_send_window(2000);

	EXPECT_EQ(fc_.bytes_sent(), 3000u);
	EXPECT_EQ(fc_.available_send_window(), 65536u - 3000u);
}

TEST_F(FlowControllerSendTest, ConsumeSendWindowExactLimit)
{
	auto result = fc_.consume_send_window(65536);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(fc_.available_send_window(), 0u);
}

TEST_F(FlowControllerSendTest, ConsumeSendWindowBlocked)
{
	fc_.consume_send_window(65536);

	auto result = fc_.consume_send_window(1);

	EXPECT_TRUE(result.is_err());
}

TEST_F(FlowControllerSendTest, IsSendBlockedWhenFull)
{
	fc_.consume_send_window(65536);

	EXPECT_TRUE(fc_.is_send_blocked());
}

TEST_F(FlowControllerSendTest, IsNotSendBlockedWhenAvailable)
{
	fc_.consume_send_window(1000);

	EXPECT_FALSE(fc_.is_send_blocked());
}

TEST_F(FlowControllerSendTest, UpdateSendLimitIncreasesWindow)
{
	fc_.consume_send_window(65536);
	ASSERT_TRUE(fc_.is_send_blocked());

	fc_.update_send_limit(131072);

	EXPECT_FALSE(fc_.is_send_blocked());
	EXPECT_EQ(fc_.available_send_window(), 131072u - 65536u);
}

TEST_F(FlowControllerSendTest, UpdateSendLimitDoesNotDecrease)
{
	fc_.update_send_limit(32768); // lower than current 65536

	// send_limit should not decrease (per RFC 9000)
	EXPECT_EQ(fc_.send_limit(), 65536u);
}

// ============================================================================
// Receive Side Tests
// ============================================================================

class FlowControllerReceiveTest : public ::testing::Test
{
protected:
	quic::flow_controller fc_{65536};
};

TEST_F(FlowControllerReceiveTest, RecordReceivedSuccess)
{
	auto result = fc_.record_received(1000);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(fc_.bytes_received(), 1000u);
}

TEST_F(FlowControllerReceiveTest, RecordReceivedMultiple)
{
	fc_.record_received(1000);
	fc_.record_received(2000);

	EXPECT_EQ(fc_.bytes_received(), 3000u);
}

TEST_F(FlowControllerReceiveTest, RecordReceivedExceedsLimit)
{
	// Receive limit equals the initial window
	auto result = fc_.record_received(fc_.receive_limit() + 1);

	EXPECT_TRUE(result.is_err());
}

TEST_F(FlowControllerReceiveTest, RecordConsumed)
{
	fc_.record_received(5000);

	fc_.record_consumed(3000);

	EXPECT_EQ(fc_.bytes_consumed(), 3000u);
}

// ============================================================================
// MAX_DATA Generation Tests
// ============================================================================

class FlowControllerMaxDataTest : public ::testing::Test
{
protected:
	quic::flow_controller fc_{10000};
};

TEST_F(FlowControllerMaxDataTest, ShouldNotSendMaxDataInitially)
{
	EXPECT_FALSE(fc_.should_send_max_data());
}

TEST_F(FlowControllerMaxDataTest, ShouldSendMaxDataAfterConsumingThreshold)
{
	// Default threshold is 50% of window
	fc_.record_received(6000);
	fc_.record_consumed(6000);

	EXPECT_TRUE(fc_.should_send_max_data());
}

TEST_F(FlowControllerMaxDataTest, GenerateMaxDataReturnsNewLimit)
{
	fc_.record_received(6000);
	fc_.record_consumed(6000);

	auto new_limit = fc_.generate_max_data();

	ASSERT_TRUE(new_limit.has_value());
	EXPECT_GT(*new_limit, fc_.bytes_received());
}

TEST_F(FlowControllerMaxDataTest, GenerateMaxDataReturnsNulloptWhenNotNeeded)
{
	auto new_limit = fc_.generate_max_data();

	EXPECT_FALSE(new_limit.has_value());
}

// ============================================================================
// DATA_BLOCKED Tests
// ============================================================================

class FlowControllerDataBlockedTest : public ::testing::Test
{
protected:
	quic::flow_controller fc_{10000};

	void SetUp() override
	{
		fc_.update_send_limit(10000);
	}
};

TEST_F(FlowControllerDataBlockedTest, ShouldSendDataBlockedWhenBlocked)
{
	fc_.consume_send_window(10000);

	EXPECT_TRUE(fc_.should_send_data_blocked());
}

TEST_F(FlowControllerDataBlockedTest, ShouldNotSendDataBlockedWhenNotBlocked)
{
	fc_.consume_send_window(5000);

	EXPECT_FALSE(fc_.should_send_data_blocked());
}

TEST_F(FlowControllerDataBlockedTest, MarkDataBlockedSentPreventsResend)
{
	fc_.consume_send_window(10000);
	ASSERT_TRUE(fc_.should_send_data_blocked());

	fc_.mark_data_blocked_sent();

	EXPECT_FALSE(fc_.should_send_data_blocked());
}

// ============================================================================
// Configuration Tests
// ============================================================================

class FlowControllerConfigTest : public ::testing::Test
{
};

TEST_F(FlowControllerConfigTest, SetWindowSize)
{
	quic::flow_controller fc;

	fc.set_window_size(131072);

	EXPECT_EQ(fc.window_size(), 131072u);
}

TEST_F(FlowControllerConfigTest, SetUpdateThreshold)
{
	quic::flow_controller fc(10000);

	fc.set_update_threshold(0.25);

	// Consume 25% and should trigger update
	fc.record_received(3000);
	fc.record_consumed(3000);

	EXPECT_TRUE(fc.should_send_max_data());
}

// ============================================================================
// Reset Tests
// ============================================================================

class FlowControllerResetTest : public ::testing::Test
{
};

TEST_F(FlowControllerResetTest, ResetClearsState)
{
	quic::flow_controller fc(65536);
	fc.update_send_limit(65536);
	fc.consume_send_window(10000);
	fc.record_received(5000);
	fc.record_consumed(3000);

	fc.reset(32768);

	EXPECT_EQ(fc.bytes_sent(), 0u);
	EXPECT_EQ(fc.bytes_received(), 0u);
	EXPECT_EQ(fc.bytes_consumed(), 0u);
	EXPECT_EQ(fc.window_size(), 32768u);
}

// ============================================================================
// Error Code Constants Tests
// ============================================================================

class FlowControlErrorCodesTest : public ::testing::Test
{
};

TEST_F(FlowControlErrorCodesTest, ErrorCodeValues)
{
	EXPECT_EQ(quic::flow_control_error::send_blocked, -710);
	EXPECT_EQ(quic::flow_control_error::receive_overflow, -711);
	EXPECT_EQ(quic::flow_control_error::window_exceeded, -712);
}

// ============================================================================
// Statistics Tests
// ============================================================================

class FlowControlStatsTest : public ::testing::Test
{
};

TEST_F(FlowControlStatsTest, GetFlowControlStats)
{
	quic::flow_controller fc(10000);
	fc.update_send_limit(10000);
	fc.consume_send_window(3000);
	fc.record_received(2000);
	fc.record_consumed(1000);

	auto stats = quic::get_flow_control_stats(fc);

	EXPECT_EQ(stats.bytes_sent, 3000u);
	EXPECT_EQ(stats.bytes_received, 2000u);
	EXPECT_EQ(stats.bytes_consumed, 1000u);
	EXPECT_EQ(stats.send_limit, 10000u);
	EXPECT_EQ(stats.send_window_available, 7000u);
	EXPECT_FALSE(stats.send_blocked);
}

TEST_F(FlowControlStatsTest, StatsShowBlocked)
{
	quic::flow_controller fc(10000);
	fc.update_send_limit(10000);
	fc.consume_send_window(10000);

	auto stats = quic::get_flow_control_stats(fc);

	EXPECT_TRUE(stats.send_blocked);
	EXPECT_EQ(stats.send_window_available, 0u);
}

// ============================================================================
// Copy and Move Semantics Tests
// ============================================================================

class FlowControllerCopyMoveTest : public ::testing::Test
{
};

TEST_F(FlowControllerCopyMoveTest, CopyConstruction)
{
	quic::flow_controller original(65536);
	original.update_send_limit(65536);
	original.consume_send_window(10000);

	quic::flow_controller copy(original);

	EXPECT_EQ(copy.bytes_sent(), 10000u);
	EXPECT_EQ(copy.window_size(), 65536u);
}

TEST_F(FlowControllerCopyMoveTest, MoveConstruction)
{
	quic::flow_controller original(65536);
	original.update_send_limit(65536);
	original.consume_send_window(10000);

	quic::flow_controller moved(std::move(original));

	EXPECT_EQ(moved.bytes_sent(), 10000u);
	EXPECT_EQ(moved.window_size(), 65536u);
}
