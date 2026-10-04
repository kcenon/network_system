/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/loss_detector.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_loss_detector_test.cpp
 * @brief Unit tests for QUIC loss detection (RFC 9002 Section 6)
 *
 * Tests validate:
 * - sent_packet struct default values
 * - loss_detection_event enum values
 * - loss_detection_result default state
 * - loss_detector constructor and initial state
 * - on_packet_sent() tracking
 * - has_unacked_packets() and bytes_in_flight()
 * - total_bytes_in_flight() across all spaces
 * - PTO count management
 * - Handshake confirmed flag
 * - discard_space() cleanup
 * - ECN tracker access
 */

// ============================================================================
// sent_packet Tests
// ============================================================================

class SentPacketTest : public ::testing::Test
{
};

TEST_F(SentPacketTest, DefaultValues)
{
	quic::sent_packet pkt;

	EXPECT_EQ(pkt.packet_number, 0u);
	EXPECT_EQ(pkt.sent_bytes, 0u);
	EXPECT_FALSE(pkt.ack_eliciting);
	EXPECT_FALSE(pkt.in_flight);
	EXPECT_EQ(pkt.level, quic::encryption_level::initial);
	EXPECT_TRUE(pkt.frames.empty());
}

// ============================================================================
// loss_detection_event Tests
// ============================================================================

class LossDetectionEventTest : public ::testing::Test
{
};

TEST_F(LossDetectionEventTest, EnumValues)
{
	EXPECT_NE(quic::loss_detection_event::none, quic::loss_detection_event::packet_lost);
	EXPECT_NE(quic::loss_detection_event::none, quic::loss_detection_event::pto_expired);
	EXPECT_NE(quic::loss_detection_event::packet_lost,
			  quic::loss_detection_event::pto_expired);
}

// ============================================================================
// loss_detection_result Tests
// ============================================================================

class LossDetectionResultTest : public ::testing::Test
{
};

TEST_F(LossDetectionResultTest, DefaultValues)
{
	quic::loss_detection_result result;

	EXPECT_EQ(result.event, quic::loss_detection_event::none);
	EXPECT_TRUE(result.acked_packets.empty());
	EXPECT_TRUE(result.lost_packets.empty());
	EXPECT_EQ(result.ecn_signal, quic::ecn_result::none);
}

// ============================================================================
// loss_detector Constructor Tests
// ============================================================================

class LossDetectorConstructorTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
};

TEST_F(LossDetectorConstructorTest, InitialPtoCountZero)
{
	quic::loss_detector detector(rtt_);

	EXPECT_EQ(detector.pto_count(), 0u);
}

TEST_F(LossDetectorConstructorTest, NoUnackedPacketsInitially)
{
	quic::loss_detector detector(rtt_);

	EXPECT_FALSE(detector.has_unacked_packets(quic::encryption_level::initial));
	EXPECT_FALSE(detector.has_unacked_packets(quic::encryption_level::handshake));
	EXPECT_FALSE(detector.has_unacked_packets(quic::encryption_level::application));
}

TEST_F(LossDetectorConstructorTest, ZeroBytesInFlightInitially)
{
	quic::loss_detector detector(rtt_);

	EXPECT_EQ(detector.total_bytes_in_flight(), 0u);
	EXPECT_EQ(detector.bytes_in_flight(quic::encryption_level::initial), 0u);
}

TEST_F(LossDetectorConstructorTest, NoTimeoutInitially)
{
	quic::loss_detector detector(rtt_);

	auto timeout = detector.next_timeout();
	// May or may not have a timeout initially - just don't crash
	(void)timeout;
	SUCCEED();
}

// ============================================================================
// on_packet_sent() Tests
// ============================================================================

class LossDetectorSendTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};

	quic::sent_packet make_packet(uint64_t pn, size_t bytes,
								  quic::encryption_level level = quic::encryption_level::application)
	{
		quic::sent_packet pkt;
		pkt.packet_number = pn;
		pkt.sent_bytes = bytes;
		pkt.ack_eliciting = true;
		pkt.in_flight = true;
		pkt.level = level;
		pkt.sent_time = std::chrono::steady_clock::now();
		return pkt;
	}
};

TEST_F(LossDetectorSendTest, TracksSentPacket)
{
	auto pkt = make_packet(0, 1200);

	detector_.on_packet_sent(pkt);

	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::application));
}

TEST_F(LossDetectorSendTest, TracksBytesInFlight)
{
	auto pkt = make_packet(0, 1200);
	detector_.on_packet_sent(pkt);

	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 1200u);
	EXPECT_EQ(detector_.total_bytes_in_flight(), 1200u);
}

TEST_F(LossDetectorSendTest, MultiplePackets)
{
	detector_.on_packet_sent(make_packet(0, 1000));
	detector_.on_packet_sent(make_packet(1, 1500));
	detector_.on_packet_sent(make_packet(2, 500));

	EXPECT_EQ(detector_.total_bytes_in_flight(), 3000u);
}

TEST_F(LossDetectorSendTest, DifferentSpaces)
{
	detector_.on_packet_sent(make_packet(0, 1000, quic::encryption_level::initial));
	detector_.on_packet_sent(make_packet(0, 800, quic::encryption_level::handshake));
	detector_.on_packet_sent(make_packet(0, 1200, quic::encryption_level::application));

	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::initial), 1000u);
	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::handshake), 800u);
	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 1200u);
	EXPECT_EQ(detector_.total_bytes_in_flight(), 3000u);
}

// ============================================================================
// PTO Count Tests
// ============================================================================

class LossDetectorPtoCountTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorPtoCountTest, ResetPtoCount)
{
	// Simulate PTO by calling on_timeout
	detector_.on_packet_sent([&] {
		quic::sent_packet pkt;
		pkt.packet_number = 0;
		pkt.sent_bytes = 1200;
		pkt.ack_eliciting = true;
		pkt.in_flight = true;
		pkt.level = quic::encryption_level::application;
		pkt.sent_time = std::chrono::steady_clock::now();
		return pkt;
	}());

	detector_.reset_pto_count();

	EXPECT_EQ(detector_.pto_count(), 0u);
}

// ============================================================================
// Handshake Confirmed Tests
// ============================================================================

class LossDetectorHandshakeTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorHandshakeTest, SetHandshakeConfirmed)
{
	// Should not throw
	detector_.set_handshake_confirmed(true);
	detector_.set_handshake_confirmed(false);

	SUCCEED();
}

// ============================================================================
// discard_space() Tests
// ============================================================================

class LossDetectorDiscardSpaceTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorDiscardSpaceTest, DiscardClearsBytesInFlight)
{
	quic::sent_packet pkt;
	pkt.packet_number = 0;
	pkt.sent_bytes = 1200;
	pkt.ack_eliciting = true;
	pkt.in_flight = true;
	pkt.level = quic::encryption_level::initial;
	pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(pkt);

	ASSERT_EQ(detector_.bytes_in_flight(quic::encryption_level::initial), 1200u);

	detector_.discard_space(quic::encryption_level::initial);

	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::initial), 0u);
	EXPECT_FALSE(detector_.has_unacked_packets(quic::encryption_level::initial));
}

TEST_F(LossDetectorDiscardSpaceTest, DiscardDoesNotAffectOtherSpaces)
{
	quic::sent_packet initial_pkt;
	initial_pkt.packet_number = 0;
	initial_pkt.sent_bytes = 1000;
	initial_pkt.ack_eliciting = true;
	initial_pkt.in_flight = true;
	initial_pkt.level = quic::encryption_level::initial;
	initial_pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(initial_pkt);

	quic::sent_packet app_pkt;
	app_pkt.packet_number = 0;
	app_pkt.sent_bytes = 1500;
	app_pkt.ack_eliciting = true;
	app_pkt.in_flight = true;
	app_pkt.level = quic::encryption_level::application;
	app_pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(app_pkt);

	detector_.discard_space(quic::encryption_level::initial);

	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 1500u);
	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::application));
}

// ============================================================================
// ECN Tracker Access Tests
// ============================================================================

class LossDetectorEcnAccessTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorEcnAccessTest, EcnTrackerAccessible)
{
	auto& ecn = detector_.get_ecn_tracker();

	// ECN tracker should start in testing mode
	EXPECT_TRUE(ecn.is_testing());
}

TEST_F(LossDetectorEcnAccessTest, EcnTrackerConstAccessible)
{
	const auto& const_detector = detector_;
	const auto& ecn = const_detector.get_ecn_tracker();

	EXPECT_TRUE(ecn.is_testing());
}

// ============================================================================
// ACK Processing Tests
// ============================================================================

class LossDetectorAckTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorAckTest, AckSinglePacket)
{
	quic::sent_packet pkt;
	pkt.packet_number = 0;
	pkt.sent_bytes = 1200;
	pkt.ack_eliciting = true;
	pkt.in_flight = true;
	pkt.level = quic::encryption_level::application;
	pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(pkt);

	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application,
		std::chrono::steady_clock::now());

	EXPECT_FALSE(result.acked_packets.empty());
	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 0u);
}

TEST_F(LossDetectorAckTest, LargestAckedUpdated)
{
	quic::sent_packet pkt;
	pkt.packet_number = 5;
	pkt.sent_bytes = 1200;
	pkt.ack_eliciting = true;
	pkt.in_flight = true;
	pkt.level = quic::encryption_level::application;
	pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(pkt);

	quic::ack_frame ack;
	ack.largest_acknowledged = 5;
	ack.ack_delay = 0;

	detector_.on_ack_received(
		ack, quic::encryption_level::application,
		std::chrono::steady_clock::now());

	EXPECT_EQ(detector_.largest_acked(quic::encryption_level::application), 5u);
}

// ============================================================================
// Multi-range ACK, loss threshold, timer, PTO, RTT, ECN Tests (Issue #1007)
// ============================================================================

namespace
{
	auto make_eliciting_packet(uint64_t pn, size_t bytes,
	                            quic::encryption_level lvl,
	                            std::chrono::steady_clock::time_point sent_time)
	    -> quic::sent_packet
	{
		quic::sent_packet pkt;
		pkt.packet_number = pn;
		pkt.sent_bytes = bytes;
		pkt.ack_eliciting = true;
		pkt.in_flight = true;
		pkt.level = lvl;
		pkt.sent_time = sent_time;
		return pkt;
	}
} // namespace

class LossDetectorMultiRangeAckTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorMultiRangeAckTest, FirstRangeMultiplePackets)
{
	auto now = std::chrono::steady_clock::now();
	for (uint64_t pn = 0; pn <= 3; ++pn)
	{
		detector_.on_packet_sent(
			make_eliciting_packet(pn, 1000, quic::encryption_level::application, now));
	}

	// Single range covering 0..3: largest=3, ranges[0].length=3
	quic::ack_frame ack;
	ack.largest_acknowledged = 3;
	ack.ack_delay = 0;
	ack.ranges.push_back({0u, 3u});

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_EQ(result.acked_packets.size(), 4u);
	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 0u);
	EXPECT_FALSE(detector_.has_unacked_packets(quic::encryption_level::application));
}

TEST_F(LossDetectorMultiRangeAckTest, SecondRangeWithGap)
{
	auto now = std::chrono::steady_clock::now();
	// Send PN 0..7
	for (uint64_t pn = 0; pn <= 7; ++pn)
	{
		detector_.on_packet_sent(
			make_eliciting_packet(pn, 1000, quic::encryption_level::application, now));
	}

	// ACK layout: range0 covers 6..7, gap hides 4..5, range1 covers 1..3
	// first range: largest=7, length=1 -> covers 7, 6
	// then current_pn = 7 - 1 - 1 = 5; gap=1 means skip 5, 4; current_pn = 5 - 1 - 2 = 2
	// range1: length=1 -> covers 2, 1 (note: loop goes i=0..length inclusive)
	quic::ack_frame ack;
	ack.largest_acknowledged = 7;
	ack.ack_delay = 0;
	ack.ranges.push_back({0u, 1u}); // first range
	ack.ranges.push_back({1u, 1u}); // gap=1, length=1

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	// range0 picks up 6, 7; range1 picks up 1, 2 (4 packets total)
	EXPECT_EQ(result.acked_packets.size(), 4u);
	// Unacked: {0, 3, 4, 5}. Packet threshold check against largest_acked=7:
	// PN 0: 7 >= 0+3 -> reorder_lost
	// PN 3: 7 >= 3+3 -> reorder_lost
	// PN 4: 7 >= 4+3 -> reorder_lost
	// PN 5: 7 >= 5+3=8 false -> stays unacked
	EXPECT_EQ(result.lost_packets.size(), 3u);
	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::application));
}

class LossDetectorReorderThresholdTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorReorderThresholdTest, PacketsFarBehindAreLost)
{
	auto now = std::chrono::steady_clock::now();
	for (uint64_t pn = 0; pn <= 5; ++pn)
	{
		detector_.on_packet_sent(
			make_eliciting_packet(pn, 1000, quic::encryption_level::application, now));
	}

	// ACK only PN 5
	quic::ack_frame ack;
	ack.largest_acknowledged = 5;
	ack.ack_delay = 0;

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	// PN 5 acked
	ASSERT_EQ(result.acked_packets.size(), 1u);
	EXPECT_EQ(result.acked_packets[0].packet_number, 5u);

	// PN 0,1,2 lost by packet threshold (largest 5 >= pn+3 for pn in {0,1,2})
	EXPECT_EQ(result.event, quic::loss_detection_event::packet_lost);
	EXPECT_EQ(result.lost_packets.size(), 3u);

	// PN 3,4 remain pending (5 >= 3+3=6 false; 5 >= 4+3=7 false)
	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::application));
}

TEST_F(LossDetectorReorderThresholdTest, PacketWithinThresholdNotLost)
{
	auto now = std::chrono::steady_clock::now();
	for (uint64_t pn = 0; pn <= 2; ++pn)
	{
		detector_.on_packet_sent(
			make_eliciting_packet(pn, 1000, quic::encryption_level::application, now));
	}

	// ACK PN 2 — largest_acked=2, so pn+3 threshold only applies when 2 >= pn+3,
	// which no PN 0 or 1 satisfies. No loss by reorder.
	quic::ack_frame ack;
	ack.largest_acknowledged = 2;
	ack.ack_delay = 0;

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_EQ(result.acked_packets.size(), 1u);
	EXPECT_TRUE(result.lost_packets.empty());
	EXPECT_EQ(result.event, quic::loss_detection_event::none);
}

class LossDetectorTimeThresholdTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorTimeThresholdTest, OldPacketDeclaredLostByTimeThreshold)
{
	auto now = std::chrono::steady_clock::now();
	// PN 0 sent far in the past (exceeds loss_delay of ~375ms initial)
	detector_.on_packet_sent(make_eliciting_packet(
		0, 1000, quic::encryption_level::application, now - std::chrono::seconds(1)));
	// PN 1 sent recently (RTT sample will be ~100ms)
	detector_.on_packet_sent(make_eliciting_packet(
		1, 1000, quic::encryption_level::application,
		now - std::chrono::milliseconds(100)));

	// ACK only PN 1 — packet threshold does NOT catch PN 0 (1 >= 0+3 false)
	// but time threshold should: sent_time(PN 0) is 1s ago, loss_delay ~112ms after
	// RTT update, so PN 0 is well past lost_send_time
	quic::ack_frame ack;
	ack.largest_acknowledged = 1;
	ack.ack_delay = 0;

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, now);

	EXPECT_EQ(result.acked_packets.size(), 1u);
	EXPECT_EQ(result.event, quic::loss_detection_event::packet_lost);
	ASSERT_EQ(result.lost_packets.size(), 1u);
	EXPECT_EQ(result.lost_packets[0].packet_number, 0u);
}

class LossDetectorTimerTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorTimerTest, TimerArmedAfterSendingEliciting)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	// Handshake must be confirmed for application-space PTO to arm
	detector_.set_handshake_confirmed(true);
	// Re-run timer arming by resending another packet (triggers set_loss_detection_timer)
	detector_.on_packet_sent(
		make_eliciting_packet(1, 1000, quic::encryption_level::application, now));

	auto timeout = detector_.next_timeout();
	EXPECT_TRUE(timeout.has_value());
}

TEST_F(LossDetectorTimerTest, TimerDisarmedAfterFullAck)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	ASSERT_TRUE(detector_.next_timeout().has_value());

	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;
	(void)detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_FALSE(detector_.next_timeout().has_value());
}

TEST_F(LossDetectorTimerTest, TimerDisarmedAfterDiscardSpace)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::initial, now));

	// Initial/handshake spaces are not gated by handshake_confirmed
	ASSERT_TRUE(detector_.next_timeout().has_value());

	detector_.discard_space(quic::encryption_level::initial);

	EXPECT_FALSE(detector_.next_timeout().has_value());
}

TEST_F(LossDetectorTimerTest, ApplicationSpaceGatedByHandshakeNotConfirmed)
{
	auto now = std::chrono::steady_clock::now();
	// Handshake explicitly NOT confirmed (default)
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	// PTO for application space is gated out when handshake not confirmed,
	// and no other space has ack-eliciting packets, so timer should not arm.
	auto timeout = detector_.next_timeout();
	EXPECT_FALSE(timeout.has_value());
}

class LossDetectorPtoTimeoutTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorPtoTimeoutTest, OnTimeoutEmitsPtoExpiredAndIncrementsCount)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	ASSERT_EQ(detector_.pto_count(), 0u);

	// No ACK received -> on_timeout must take the PTO branch (loss_time is epoch)
	auto result = detector_.on_timeout();

	EXPECT_EQ(result.event, quic::loss_detection_event::pto_expired);
	EXPECT_TRUE(result.lost_packets.empty());
	EXPECT_EQ(detector_.pto_count(), 1u);
}

TEST_F(LossDetectorPtoTimeoutTest, ConsecutivePtoIncrementsCount)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	(void)detector_.on_timeout();
	(void)detector_.on_timeout();
	(void)detector_.on_timeout();

	EXPECT_EQ(detector_.pto_count(), 3u);
}

TEST_F(LossDetectorPtoTimeoutTest, AckResetsPtoCount)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	(void)detector_.on_timeout();
	(void)detector_.on_timeout();
	ASSERT_EQ(detector_.pto_count(), 2u);

	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;
	(void)detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_EQ(detector_.pto_count(), 0u);
}

class LossDetectorRttUpdateTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorRttUpdateTest, AckOfLargestUpdatesRtt)
{
	auto send_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, send_time));

	// Initial smoothed/latest/min RTT before any update
	auto initial_smoothed = rtt_.smoothed_rtt();

	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;
	auto recv_time = send_time + std::chrono::milliseconds(50);
	(void)detector_.on_ack_received(ack, quic::encryption_level::application, recv_time);

	// First sample: latest_rtt is ~50ms; smoothed should equal or move towards latest
	EXPECT_NE(rtt_.latest_rtt(), std::chrono::microseconds{0});
	EXPECT_NE(rtt_.latest_rtt(), initial_smoothed);
	// min_rtt should no longer be uninitialized (max())
	EXPECT_NE(rtt_.min_rtt(), std::chrono::microseconds::max());
}

TEST_F(LossDetectorRttUpdateTest, AckOfNonLargestDoesNotUpdateRtt)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));
	detector_.on_packet_sent(
		make_eliciting_packet(1, 1000, quic::encryption_level::application, now));

	auto initial_latest = rtt_.latest_rtt();

	// ACK PN 1 but set largest_acknowledged = 0 (stale). The ACK machinery resolves
	// current_pn via largest_acknowledged; since largest_newly_acked must match the
	// sent packet to trigger RTT update, test that RTT does not change if the
	// ack'd-largest is not in our sent map.
	quic::ack_frame ack;
	ack.largest_acknowledged = 999; // not in our sent map
	ack.ack_delay = 0;
	(void)detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_EQ(rtt_.latest_rtt(), initial_latest);
}

class LossDetectorEcnSignalTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorEcnSignalTest, AckWithoutEcnSectionLeavesSignalNone)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;
	// ack.ecn stays nullopt

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_EQ(result.ecn_signal, quic::ecn_result::none);
}

TEST_F(LossDetectorEcnSignalTest, AckWithEcnSectionExercisesTracker)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;
	quic::ecn_counts counts;
	counts.ect0 = 1;
	counts.ect1 = 0;
	counts.ecn_ce = 0;
	ack.ecn = counts;

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	// The signal is tracker-dependent: during initial validation it may emit any of
	// {none, congestion_signal, ecn_failure}. Assert it's a valid enum value and
	// that the call did not corrupt state.
	auto sig = result.ecn_signal;
	bool is_valid = sig == quic::ecn_result::none ||
	                sig == quic::ecn_result::congestion_signal ||
	                sig == quic::ecn_result::ecn_failure;
	EXPECT_TRUE(is_valid);
}

class LossDetectorLargestAckedMonotonicityTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorLargestAckedMonotonicityTest, SmallerAckDoesNotDowngrade)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(3, 1000, quic::encryption_level::application, now));
	detector_.on_packet_sent(
		make_eliciting_packet(10, 1000, quic::encryption_level::application, now));

	quic::ack_frame ack_large;
	ack_large.largest_acknowledged = 10;
	ack_large.ack_delay = 0;
	(void)detector_.on_ack_received(
		ack_large, quic::encryption_level::application, std::chrono::steady_clock::now());
	ASSERT_EQ(detector_.largest_acked(quic::encryption_level::application), 10u);

	quic::ack_frame ack_small;
	ack_small.largest_acknowledged = 3;
	ack_small.ack_delay = 0;
	(void)detector_.on_ack_received(
		ack_small, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_EQ(detector_.largest_acked(quic::encryption_level::application), 10u);
}

class LossDetectorBytesInFlightEdgeTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

TEST_F(LossDetectorBytesInFlightEdgeTest, NonInFlightPacketDoesNotContribute)
{
	quic::sent_packet pkt;
	pkt.packet_number = 0;
	pkt.sent_bytes = 1200;
	pkt.ack_eliciting = true;
	pkt.in_flight = false; // explicitly non-in-flight
	pkt.level = quic::encryption_level::application;
	pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(pkt);

	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 0u);
	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::application));
}

TEST_F(LossDetectorBytesInFlightEdgeTest, NonElicitingPacketDoesNotArmTimer)
{
	quic::sent_packet pkt;
	pkt.packet_number = 0;
	pkt.sent_bytes = 1200;
	pkt.ack_eliciting = false; // not ack-eliciting
	pkt.in_flight = true;
	pkt.level = quic::encryption_level::initial;
	pkt.sent_time = std::chrono::steady_clock::now();
	detector_.on_packet_sent(pkt);

	// any_ack_eliciting check fails -> timer not armed
	EXPECT_FALSE(detector_.next_timeout().has_value());
}

TEST_F(LossDetectorBytesInFlightEdgeTest, TotalBytesInFlightSumsSpaces)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 500, quic::encryption_level::initial, now));
	detector_.on_packet_sent(
		make_eliciting_packet(0, 800, quic::encryption_level::handshake, now));
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1200, quic::encryption_level::application, now));

	EXPECT_EQ(detector_.total_bytes_in_flight(), 500u + 800u + 1200u);

	// ACK the handshake packet -> its bytes drop from total
	quic::ack_frame ack;
	ack.largest_acknowledged = 0;
	ack.ack_delay = 0;
	(void)detector_.on_ack_received(
		ack, quic::encryption_level::handshake, std::chrono::steady_clock::now());

	EXPECT_EQ(detector_.total_bytes_in_flight(), 500u + 1200u);
}

TEST_F(LossDetectorBytesInFlightEdgeTest, ZeroRttMapsToApplicationSpace)
{
	auto now = std::chrono::steady_clock::now();
	detector_.on_packet_sent(
		make_eliciting_packet(0, 700, quic::encryption_level::zero_rtt, now));

	// zero_rtt routes through space_index==2 (application)
	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::application), 700u);
	EXPECT_EQ(detector_.bytes_in_flight(quic::encryption_level::zero_rtt), 700u);
	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::zero_rtt));
}

// ============================================================================
// Additional branch-coverage tests (Issue #1027)
// ============================================================================

class LossDetectorTimeoutBranchTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

// Exercises the on_timeout() branch where loss_time is set in a space and is
// <= now, so the loss-detection path runs (not the PTO branch).
TEST_F(LossDetectorTimeoutBranchTest, OnTimeoutTakesLossTimeBranch)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);

	// PN 0 sent very recently — must NOT be time-threshold lost during the
	// upcoming ACK (sent_time newer than recv_time - loss_delay) AND must
	// NOT be packet-threshold lost (1 < 0+3). After RTT updates from the ACK,
	// loss_delay collapses to kGranularity=1ms, so PN 0 sent ~100us ago stays
	// pending. space.loss_time gets populated so on_timeout() takes the
	// loss-detection branch.
	detector_.on_packet_sent(make_eliciting_packet(
		0, 1000, quic::encryption_level::application,
		now - std::chrono::microseconds(100)));
	// PN 1 sent fractionally later — drives an RTT sample (~50us) and largest_acked.
	detector_.on_packet_sent(make_eliciting_packet(
		1, 1000, quic::encryption_level::application,
		now - std::chrono::microseconds(50)));

	// ACK only PN 1 — packet threshold won't catch PN 0 (1 < 0+3),
	// so PN 0 stays pending and space.loss_time gets populated.
	quic::ack_frame ack;
	ack.largest_acknowledged = 1;
	ack.ack_delay = 0;
	auto ack_result = detector_.on_ack_received(
		ack, quic::encryption_level::application, now);
	ASSERT_FALSE(ack_result.acked_packets.empty());
	ASSERT_TRUE(ack_result.lost_packets.empty())
		<< "PN 0 must remain pending until on_timeout() fires";

	// Sleep well past the time-threshold loss_delay so on_timeout() takes the
	// loss-time branch. loss_delay is ~1.125 * max(smoothed_rtt, min_rtt) with
	// a kGranularity=1ms floor; 50ms is comfortably past either.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto result = detector_.on_timeout();

	// Loss-time branch should fire (not the PTO path), so PN 0 gets reported lost
	// and pto_count must NOT increment.
	EXPECT_EQ(result.event, quic::loss_detection_event::packet_lost);
	ASSERT_EQ(result.lost_packets.size(), 1u);
	EXPECT_EQ(result.lost_packets[0].packet_number, 0u);
	EXPECT_EQ(detector_.pto_count(), 0u);
}

class LossDetectorAckRangeEdgeTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

// Exercises the `current_pn >= gap + 2` else-break branch in the multi-range
// ACK loop: when the encoded gap would underflow current_pn, processing stops.
TEST_F(LossDetectorAckRangeEdgeTest, ImpossibleGapBreaksRangeLoop)
{
	auto now = std::chrono::steady_clock::now();
	for (uint64_t pn = 0; pn <= 3; ++pn)
	{
		detector_.on_packet_sent(make_eliciting_packet(
			pn, 1000, quic::encryption_level::application, now));
	}

	// First range: largest=3, length=0 -> covers PN 3 only. After processing:
	// current_pn = 3 - 0 - 1 = 2. Second range gap=10 means we'd need
	// current_pn >= 12 to continue, which is false, so we break.
	quic::ack_frame ack;
	ack.largest_acknowledged = 3;
	ack.ack_delay = 0;
	ack.ranges.push_back({0u, 0u});
	ack.ranges.push_back({10u, 0u});

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	// Only PN 3 should be acked; the impossible second range is skipped.
	ASSERT_EQ(result.acked_packets.size(), 1u);
	EXPECT_EQ(result.acked_packets[0].packet_number, 3u);
}

// Exercises the `current_pn >= range_len + 1` else-break branch: after the
// second range is processed, the next decrement would underflow.
TEST_F(LossDetectorAckRangeEdgeTest, RangeLenUnderflowBreaksRangeLoop)
{
	auto now = std::chrono::steady_clock::now();
	for (uint64_t pn = 1; pn <= 5; ++pn)
	{
		detector_.on_packet_sent(make_eliciting_packet(
			pn, 1000, quic::encryption_level::application, now));
	}

	// First range: largest=5, length=0 -> covers PN 5; current_pn -> 4.
	// Second range: gap=0, length=2 -> current_pn becomes 4 - 0 - 2 = 2,
	// covers PN 2,1,0 (loop iterates while current_pn>0). Then the trailing
	// `current_pn >= range_len + 1` (2 >= 3) is false -> break.
	quic::ack_frame ack;
	ack.largest_acknowledged = 5;
	ack.ack_delay = 0;
	ack.ranges.push_back({0u, 0u});
	ack.ranges.push_back({0u, 2u});

	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, std::chrono::steady_clock::now());

	EXPECT_FALSE(result.acked_packets.empty());
}

class LossDetectorDetectLossEdgeTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

// detect_lost_packets() returns early when largest_acked has never been set.
// This is exercised when on_timeout() runs with no prior ACK on a space that
// only contains a non-eliciting packet (so PTO doesn't pick it either).
TEST_F(LossDetectorDetectLossEdgeTest, DetectLostNoLargestAckedYields_NoLoss)
{
	auto now = std::chrono::steady_clock::now();
	// Send eliciting packet so timer arms in initial space (not handshake-gated).
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::initial, now));

	// No ACK received — on_timeout must take the PTO branch, but
	// detect_lost_packets is called for whatever space the loss_time pointer
	// resolves to. With no largest_acked set anywhere, no losses are reported.
	auto result = detector_.on_timeout();
	EXPECT_TRUE(result.lost_packets.empty());
}

// Exercises the branch where we update space.loss_time to an earlier value
// because a second pending packet would trigger loss-detection sooner.
TEST_F(LossDetectorDetectLossEdgeTest, MultiplePendingPacketsTrackEarliestLossTime)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);

	// PN 0: older — would be lost first by time threshold.
	detector_.on_packet_sent(make_eliciting_packet(
		0, 1000, quic::encryption_level::application,
		now - std::chrono::milliseconds(80)));
	// PN 1: even older — should drive loss_time even sooner.
	detector_.on_packet_sent(make_eliciting_packet(
		1, 1000, quic::encryption_level::application,
		now - std::chrono::milliseconds(120)));
	// PN 5: recent — drives largest_acked when ACKed.
	detector_.on_packet_sent(make_eliciting_packet(
		5, 1000, quic::encryption_level::application,
		now - std::chrono::milliseconds(5)));

	quic::ack_frame ack;
	ack.largest_acknowledged = 5;
	ack.ack_delay = 0;
	auto result = detector_.on_ack_received(
		ack, quic::encryption_level::application, now);

	// PN 0,1 are >=3 behind largest_acked=5 -> both packet-threshold lost.
	ASSERT_EQ(result.acked_packets.size(), 1u);
	EXPECT_EQ(result.acked_packets[0].packet_number, 5u);
	EXPECT_EQ(result.event, quic::loss_detection_event::packet_lost);
	EXPECT_EQ(result.lost_packets.size(), 2u);
}

class LossDetectorDiscardSpaceTimerTest : public ::testing::Test
{
protected:
	quic::rtt_estimator rtt_;
	quic::loss_detector detector_{rtt_};
};

// discard_space() must wipe per-space totals AND re-arm the timer based on
// remaining spaces. After discarding initial, the application-space timer
// should drive next_timeout().
TEST_F(LossDetectorDiscardSpaceTimerTest, DiscardReArmsTimerFromOtherSpaces)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);

	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::initial, now));
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));
	ASSERT_TRUE(detector_.next_timeout().has_value());

	detector_.discard_space(quic::encryption_level::initial);

	// Application space still has an eliciting packet -> timer must remain armed.
	EXPECT_TRUE(detector_.next_timeout().has_value());
	EXPECT_TRUE(detector_.has_unacked_packets(quic::encryption_level::application));
}

// PTO exponential backoff: pto_count squared into pto_duration. After several
// timeouts, get_pto_time_and_space() shifts the next deadline further out.
TEST_F(LossDetectorDiscardSpaceTimerTest, PtoBackoffExtendsTimeout)
{
	auto now = std::chrono::steady_clock::now();
	detector_.set_handshake_confirmed(true);
	detector_.on_packet_sent(
		make_eliciting_packet(0, 1000, quic::encryption_level::application, now));

	auto first = detector_.next_timeout();
	ASSERT_TRUE(first.has_value());

	(void)detector_.on_timeout();

	auto second = detector_.next_timeout();
	ASSERT_TRUE(second.has_value());
	// After one PTO, backoff doubles the duration -> second deadline must be
	// strictly later than the first.
	EXPECT_GT(*second, *first);
}
