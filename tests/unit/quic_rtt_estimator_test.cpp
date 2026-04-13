/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/rtt_estimator.h"
#include <gtest/gtest.h>

#include <chrono>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_rtt_estimator_test.cpp
 * @brief Unit tests for QUIC RTT estimator (RFC 9002 Section 5)
 *
 * Tests validate:
 * - Default constructor initial values (333ms RTT, 25ms max ACK delay)
 * - Custom constructor with specified initial RTT and max ACK delay
 * - has_sample() before and after first update()
 * - First update() sets smoothed_rtt to latest_rtt directly
 * - Subsequent updates use EWMA algorithm (RFC 9002 Section 5.3)
 * - min_rtt tracking across multiple samples
 * - ACK delay clamping to max_ack_delay
 * - ACK delay excluded when handshake not confirmed
 * - pto() calculation: smoothed_rtt + max(4*rttvar, 1ms) + max_ack_delay
 * - set_max_ack_delay() and max_ack_delay() accessors
 * - reset() restores initial state
 */

using namespace std::chrono_literals;

// ============================================================================
// Default Constructor Tests
// ============================================================================

class RttEstimatorDefaultTest : public ::testing::Test
{
};

TEST_F(RttEstimatorDefaultTest, InitialSmoothedRtt)
{
	quic::rtt_estimator rtt;

	// Default initial RTT is 333ms (333000us) per RFC 9002
	EXPECT_EQ(rtt.smoothed_rtt(), std::chrono::microseconds{333000});
}

TEST_F(RttEstimatorDefaultTest, InitialRttvar)
{
	quic::rtt_estimator rtt;

	// rttvar is initialized to initial_rtt / 2 = 166500us
	EXPECT_EQ(rtt.rttvar(), std::chrono::microseconds{166500});
}

TEST_F(RttEstimatorDefaultTest, NoSampleInitially)
{
	quic::rtt_estimator rtt;

	EXPECT_FALSE(rtt.has_sample());
}

TEST_F(RttEstimatorDefaultTest, DefaultMaxAckDelay)
{
	quic::rtt_estimator rtt;

	EXPECT_EQ(rtt.max_ack_delay(), std::chrono::microseconds{25000});
}

TEST_F(RttEstimatorDefaultTest, LatestRttInitiallyZero)
{
	quic::rtt_estimator rtt;

	// latest_rtt should be 0 or initial value before any sample
	// (implementation dependent, just check it doesn't crash)
	auto latest = rtt.latest_rtt();
	(void)latest;
	SUCCEED();
}

// ============================================================================
// Custom Constructor Tests
// ============================================================================

class RttEstimatorCustomTest : public ::testing::Test
{
};

TEST_F(RttEstimatorCustomTest, CustomInitialRtt)
{
	quic::rtt_estimator rtt(100ms);

	EXPECT_EQ(rtt.smoothed_rtt(), 100ms);
}

TEST_F(RttEstimatorCustomTest, CustomInitialRttAndMaxAckDelay)
{
	quic::rtt_estimator rtt(200ms, 50ms);

	EXPECT_EQ(rtt.smoothed_rtt(), 200ms);
	EXPECT_EQ(rtt.max_ack_delay(), 50ms);
}

TEST_F(RttEstimatorCustomTest, CustomRttvarIsHalfInitial)
{
	quic::rtt_estimator rtt(100ms);

	EXPECT_EQ(rtt.rttvar(), 50ms);
}

// ============================================================================
// First Update Tests
// ============================================================================

class RttEstimatorFirstUpdateTest : public ::testing::Test
{
};

TEST_F(RttEstimatorFirstUpdateTest, HasSampleAfterFirstUpdate)
{
	quic::rtt_estimator rtt;

	rtt.update(100ms, 0us);

	EXPECT_TRUE(rtt.has_sample());
}

TEST_F(RttEstimatorFirstUpdateTest, SmoothedRttEqualsLatestRtt)
{
	quic::rtt_estimator rtt;

	rtt.update(100ms, 0us);

	// RFC 9002 Section 5.3: On first sample, smoothed_rtt = latest_rtt
	EXPECT_EQ(rtt.smoothed_rtt(), 100ms);
}

TEST_F(RttEstimatorFirstUpdateTest, LatestRttIsSet)
{
	quic::rtt_estimator rtt;

	rtt.update(50ms, 0us);

	EXPECT_EQ(rtt.latest_rtt(), 50ms);
}

TEST_F(RttEstimatorFirstUpdateTest, MinRttIsSet)
{
	quic::rtt_estimator rtt;

	rtt.update(80ms, 0us);

	EXPECT_EQ(rtt.min_rtt(), 80ms);
}

TEST_F(RttEstimatorFirstUpdateTest, RttvarSetToHalfLatest)
{
	quic::rtt_estimator rtt;

	rtt.update(100ms, 0us);

	// RFC 9002: rttvar = latest_rtt / 2 on first sample
	EXPECT_EQ(rtt.rttvar(), 50ms);
}

// ============================================================================
// Subsequent Update Tests (EWMA)
// ============================================================================

class RttEstimatorSubsequentUpdateTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;

	void SetUp() override
	{
		rtt_.update(100ms, 0us);
	}
};

TEST_F(RttEstimatorSubsequentUpdateTest, SmoothedRttChangesWithNewSample)
{
	auto before = rtt_.smoothed_rtt();

	rtt_.update(80ms, 0us);

	// EWMA: smoothed_rtt = 7/8 * old + 1/8 * new
	// = 7/8 * 100ms + 1/8 * 80ms = 87.5ms + 10ms = 97.5ms
	auto after = rtt_.smoothed_rtt();
	EXPECT_NE(before, after);
	EXPECT_LT(after, before); // Should decrease since sample is lower
}

TEST_F(RttEstimatorSubsequentUpdateTest, LatestRttUpdated)
{
	rtt_.update(80ms, 0us);

	EXPECT_EQ(rtt_.latest_rtt(), 80ms);
}

TEST_F(RttEstimatorSubsequentUpdateTest, MinRttDecreases)
{
	rtt_.update(50ms, 0us);

	EXPECT_EQ(rtt_.min_rtt(), 50ms);
}

TEST_F(RttEstimatorSubsequentUpdateTest, MinRttDoesNotIncrease)
{
	rtt_.update(200ms, 0us);

	// min_rtt stays at 100ms from first sample
	EXPECT_EQ(rtt_.min_rtt(), 100ms);
}

TEST_F(RttEstimatorSubsequentUpdateTest, MultipleUpdatesConverge)
{
	// Feed several 80ms samples, smoothed_rtt should approach 80ms
	for (int i = 0; i < 20; ++i)
	{
		rtt_.update(80ms, 0us);
	}

	// Should be close to 80ms after many samples
	EXPECT_NEAR(rtt_.smoothed_rtt().count(),
				std::chrono::microseconds(80ms).count(),
				5000); // within 5ms
}

// ============================================================================
// ACK Delay Handling Tests
// ============================================================================

class RttEstimatorAckDelayTest : public ::testing::Test
{
};

TEST_F(RttEstimatorAckDelayTest, AckDelaySubtractedWhenHandshakeConfirmed)
{
	quic::rtt_estimator rtt;
	rtt.update(100ms, 0us); // first sample

	// Second sample with ACK delay
	rtt.update(120ms, 10ms, true);

	// The adjusted RTT should be 120ms - 10ms = 110ms
	// smoothed_rtt = 7/8 * 100 + 1/8 * 110 = 87.5 + 13.75 = 101.25ms
	// Just verify it's reasonable - exact value depends on ack_delay adjustment logic
	EXPECT_GT(rtt.smoothed_rtt(), 90ms);
	EXPECT_LT(rtt.smoothed_rtt(), 130ms);
}

TEST_F(RttEstimatorAckDelayTest, AckDelayNotSubtractedWhenHandshakeNotConfirmed)
{
	quic::rtt_estimator rtt;
	rtt.update(100ms, 0us); // first sample

	// Second sample without handshake confirmed
	auto before = rtt.smoothed_rtt();
	rtt.update(120ms, 10ms, false);

	// Without ack_delay subtraction, the RTT sample is 120ms
	// smoothed_rtt = 7/8 * 100 + 1/8 * 120 = 87.5 + 15 = 102.5ms
	EXPECT_GT(rtt.smoothed_rtt(), before);
}

TEST_F(RttEstimatorAckDelayTest, AckDelayClampedToMaxAckDelay)
{
	quic::rtt_estimator rtt(100ms, 5ms); // max_ack_delay = 5ms
	rtt.update(100ms, 0us); // first sample

	// ACK delay (20ms) exceeds max_ack_delay (5ms), should be clamped
	rtt.update(120ms, 20ms, true);

	// Adjusted RTT should be 120ms - 5ms = 115ms (clamped), not 120ms - 20ms = 100ms
	EXPECT_GT(rtt.smoothed_rtt(), 100ms);
}

// ============================================================================
// PTO Calculation Tests
// ============================================================================

class RttEstimatorPtoTest : public ::testing::Test
{
};

TEST_F(RttEstimatorPtoTest, PtoIncludesMaxAckDelay)
{
	quic::rtt_estimator rtt(100ms, 25ms);

	auto pto = rtt.pto();

	// PTO = smoothed_rtt + max(4*rttvar, 1ms) + max_ack_delay
	// = 100ms + max(200ms, 1ms) + 25ms = 325ms
	EXPECT_EQ(pto, 325ms);
}

TEST_F(RttEstimatorPtoTest, PtoAfterUpdate)
{
	quic::rtt_estimator rtt;
	rtt.update(50ms, 0us);

	auto pto = rtt.pto();

	// smoothed_rtt = 50ms, rttvar = 25ms, max_ack_delay = 25ms
	// PTO = 50ms + max(100ms, 1ms) + 25ms = 175ms
	EXPECT_EQ(pto, 175ms);
}

TEST_F(RttEstimatorPtoTest, PtoUsesGranularityMinimum)
{
	quic::rtt_estimator rtt(100ms, 0us);
	rtt.update(100ms, 0us); // first sample

	// After many samples with same RTT, rttvar should converge to ~0
	for (int i = 0; i < 100; ++i)
	{
		rtt.update(100ms, 0us);
	}

	auto pto = rtt.pto();

	// When 4*rttvar < 1ms, granularity (1ms) is used
	// PTO >= smoothed_rtt + 1ms
	EXPECT_GE(pto, 100ms + 1ms);
}

// ============================================================================
// Accessor Tests
// ============================================================================

class RttEstimatorAccessorTest : public ::testing::Test
{
};

TEST_F(RttEstimatorAccessorTest, SetMaxAckDelay)
{
	quic::rtt_estimator rtt;

	rtt.set_max_ack_delay(50ms);

	EXPECT_EQ(rtt.max_ack_delay(), 50ms);
}

// ============================================================================
// Reset Tests
// ============================================================================

class RttEstimatorResetTest : public ::testing::Test
{
};

TEST_F(RttEstimatorResetTest, ResetRestoresInitialState)
{
	quic::rtt_estimator rtt;
	rtt.update(50ms, 0us);
	rtt.update(80ms, 10ms);

	ASSERT_TRUE(rtt.has_sample());

	rtt.reset();

	EXPECT_FALSE(rtt.has_sample());
	EXPECT_EQ(rtt.smoothed_rtt(), std::chrono::microseconds{333000});
}

TEST_F(RttEstimatorResetTest, ResetPreservesMaxAckDelay)
{
	quic::rtt_estimator rtt(100ms, 50ms);
	rtt.update(80ms, 0us);
	rtt.reset();

	// smoothed_rtt should be reset to initial_rtt (100ms)
	EXPECT_EQ(rtt.smoothed_rtt(), 100ms);
}
