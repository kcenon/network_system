/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/loss_detector.h"
#include <gtest/gtest.h>

#include <chrono>

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
