/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/congestion_controller.h"
#include <gtest/gtest.h>

#include <chrono>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_congestion_controller_test.cpp
 * @brief Unit tests for QUIC NewReno congestion control (RFC 9002 Section 7)
 *
 * Tests validate:
 * - Default constructor (initial window = 10 * max_datagram_size)
 * - Custom max_datagram_size constructor
 * - congestion_state enum values and string conversion
 * - Initial state is slow_start
 * - can_send() and available_window() in initial state
 * - on_packet_sent() tracks bytes_in_flight
 * - on_packet_acked() in slow_start grows cwnd
 * - on_packet_lost() halves cwnd, enters recovery
 * - on_congestion_event() enters recovery
 * - on_ecn_congestion() triggers recovery from ECN signal
 * - on_persistent_congestion() reduces to minimum window
 * - set_max_datagram_size() adjusts windows
 * - reset() restores initial state
 */

// ============================================================================
// Congestion State Tests
// ============================================================================

class CongestionStateTest : public ::testing::Test
{
};

TEST_F(CongestionStateTest, StateToStringSlowStart)
{
	auto str = quic::congestion_state_to_string(quic::congestion_state::slow_start);

	EXPECT_NE(str, nullptr);
}

TEST_F(CongestionStateTest, StateToStringCongestionAvoidance)
{
	auto str = quic::congestion_state_to_string(quic::congestion_state::congestion_avoidance);

	EXPECT_NE(str, nullptr);
}

TEST_F(CongestionStateTest, StateToStringRecovery)
{
	auto str = quic::congestion_state_to_string(quic::congestion_state::recovery);

	EXPECT_NE(str, nullptr);
}

// ============================================================================
// Default Constructor Tests
// ============================================================================

class CongestionControllerDefaultTest : public ::testing::Test
{
};

TEST_F(CongestionControllerDefaultTest, InitialState)
{
	quic::congestion_controller cc;

	EXPECT_EQ(cc.state(), quic::congestion_state::slow_start);
}

TEST_F(CongestionControllerDefaultTest, InitialWindow)
{
	quic::congestion_controller cc;

	// initial_window = 10 * 1200 = 12000
	EXPECT_EQ(cc.cwnd(), 12000u);
}

TEST_F(CongestionControllerDefaultTest, DefaultMaxDatagramSize)
{
	quic::congestion_controller cc;

	EXPECT_EQ(cc.max_datagram_size(), 1200u);
}

TEST_F(CongestionControllerDefaultTest, InitialBytesInFlight)
{
	quic::congestion_controller cc;

	EXPECT_EQ(cc.bytes_in_flight(), 0u);
}

TEST_F(CongestionControllerDefaultTest, InitialSsthresh)
{
	quic::congestion_controller cc;

	// ssthresh starts at max
	EXPECT_GT(cc.ssthresh(), cc.cwnd());
}

// ============================================================================
// Custom Max Datagram Size Tests
// ============================================================================

class CongestionControllerCustomTest : public ::testing::Test
{
};

TEST_F(CongestionControllerCustomTest, CustomMaxDatagramSize)
{
	quic::congestion_controller cc(1472);

	EXPECT_EQ(cc.max_datagram_size(), 1472u);
	// initial_window = 10 * 1472 = 14720
	EXPECT_EQ(cc.cwnd(), 14720u);
}

// ============================================================================
// Send Control Tests
// ============================================================================

class CongestionControllerSendTest : public ::testing::Test
{
protected:
	quic::congestion_controller cc_;
};

TEST_F(CongestionControllerSendTest, CanSendInitially)
{
	EXPECT_TRUE(cc_.can_send());
}

TEST_F(CongestionControllerSendTest, CanSendWithBytes)
{
	EXPECT_TRUE(cc_.can_send(1000));
}

TEST_F(CongestionControllerSendTest, AvailableWindowInitial)
{
	EXPECT_EQ(cc_.available_window(), 12000u);
}

TEST_F(CongestionControllerSendTest, OnPacketSentTracksBytesInFlight)
{
	cc_.on_packet_sent(1000);

	EXPECT_EQ(cc_.bytes_in_flight(), 1000u);
	EXPECT_EQ(cc_.available_window(), 11000u);
}

TEST_F(CongestionControllerSendTest, MultipleSends)
{
	cc_.on_packet_sent(3000);
	cc_.on_packet_sent(4000);

	EXPECT_EQ(cc_.bytes_in_flight(), 7000u);
	EXPECT_EQ(cc_.available_window(), 5000u);
}

TEST_F(CongestionControllerSendTest, CannotSendWhenFull)
{
	cc_.on_packet_sent(12000);

	EXPECT_EQ(cc_.available_window(), 0u);
	EXPECT_FALSE(cc_.can_send(1));
}

// ============================================================================
// ACK Processing Tests (Slow Start)
// ============================================================================

class CongestionControllerAckTest : public ::testing::Test
{
protected:
	quic::congestion_controller cc_;
	quic::sent_packet make_sent_packet(size_t bytes)
	{
		quic::sent_packet pkt;
		pkt.sent_bytes = bytes;
		pkt.in_flight = true;
		pkt.ack_eliciting = true;
		pkt.sent_time = std::chrono::steady_clock::now();
		return pkt;
	}
};

TEST_F(CongestionControllerAckTest, AckReducesBytesInFlight)
{
	cc_.on_packet_sent(5000);

	auto pkt = make_sent_packet(5000);
	cc_.on_packet_acked(pkt, std::chrono::steady_clock::now());

	EXPECT_EQ(cc_.bytes_in_flight(), 0u);
}

TEST_F(CongestionControllerAckTest, SlowStartGrowsCwnd)
{
	auto initial_cwnd = cc_.cwnd();

	cc_.on_packet_sent(1000);
	auto pkt = make_sent_packet(1000);
	cc_.on_packet_acked(pkt, std::chrono::steady_clock::now());

	// In slow start, cwnd grows by sent_bytes
	EXPECT_GT(cc_.cwnd(), initial_cwnd);
}

TEST_F(CongestionControllerAckTest, RemainsInSlowStartBelowSsthresh)
{
	cc_.on_packet_sent(1000);
	auto pkt = make_sent_packet(1000);
	cc_.on_packet_acked(pkt, std::chrono::steady_clock::now());

	EXPECT_EQ(cc_.state(), quic::congestion_state::slow_start);
}

// ============================================================================
// Loss Processing Tests
// ============================================================================

class CongestionControllerLossTest : public ::testing::Test
{
protected:
	quic::congestion_controller cc_;
	quic::sent_packet make_sent_packet(size_t bytes)
	{
		quic::sent_packet pkt;
		pkt.sent_bytes = bytes;
		pkt.in_flight = true;
		pkt.ack_eliciting = true;
		pkt.sent_time = std::chrono::steady_clock::now();
		return pkt;
	}
};

TEST_F(CongestionControllerLossTest, LossReducesCwnd)
{
	cc_.on_packet_sent(5000);
	auto initial_cwnd = cc_.cwnd();

	auto pkt = make_sent_packet(5000);
	cc_.on_packet_lost(pkt);

	// cwnd should be reduced (halved per NewReno)
	EXPECT_LT(cc_.cwnd(), initial_cwnd);
}

TEST_F(CongestionControllerLossTest, LossSetsSsthresh)
{
	cc_.on_packet_sent(5000);
	auto initial_cwnd = cc_.cwnd();

	auto pkt = make_sent_packet(5000);
	cc_.on_packet_lost(pkt);

	// ssthresh should be set
	EXPECT_LE(cc_.ssthresh(), initial_cwnd);
}

TEST_F(CongestionControllerLossTest, LossReducesBytesInFlight)
{
	cc_.on_packet_sent(5000);

	auto pkt = make_sent_packet(5000);
	cc_.on_packet_lost(pkt);

	EXPECT_EQ(cc_.bytes_in_flight(), 0u);
}

TEST_F(CongestionControllerLossTest, CwndDoesNotGoBelowMinimumWindow)
{
	// Send many packets and lose them all
	for (int i = 0; i < 10; ++i)
	{
		cc_.on_packet_sent(1200);
		auto pkt = make_sent_packet(1200);
		cc_.on_packet_lost(pkt);
	}

	// Minimum window = 2 * max_datagram_size = 2 * 1200 = 2400
	EXPECT_GE(cc_.cwnd(), 2 * cc_.max_datagram_size());
}

// ============================================================================
// Congestion Event Tests
// ============================================================================

class CongestionControllerCongestionEventTest : public ::testing::Test
{
protected:
	quic::congestion_controller cc_;
};

TEST_F(CongestionControllerCongestionEventTest, EntersRecovery)
{
	cc_.on_packet_sent(5000);
	auto sent_time = std::chrono::steady_clock::now();

	cc_.on_congestion_event(sent_time);

	EXPECT_EQ(cc_.state(), quic::congestion_state::recovery);
}

TEST_F(CongestionControllerCongestionEventTest, CwndHalvedOnCongestionEvent)
{
	auto initial_cwnd = cc_.cwnd();

	cc_.on_congestion_event(std::chrono::steady_clock::now());

	EXPECT_LT(cc_.cwnd(), initial_cwnd);
}

// ============================================================================
// ECN Congestion Tests
// ============================================================================

class CongestionControllerEcnTest : public ::testing::Test
{
protected:
	quic::congestion_controller cc_;
};

TEST_F(CongestionControllerEcnTest, EcnCongestionReducesCwnd)
{
	auto initial_cwnd = cc_.cwnd();
	cc_.on_packet_sent(5000);

	cc_.on_ecn_congestion(std::chrono::steady_clock::now());

	EXPECT_LT(cc_.cwnd(), initial_cwnd);
}

// ============================================================================
// Persistent Congestion Tests
// ============================================================================

class CongestionControllerPersistentCongestionTest : public ::testing::Test
{
protected:
	quic::congestion_controller cc_;
};

TEST_F(CongestionControllerPersistentCongestionTest, ReducesToMinimumWindow)
{
	quic::rtt_estimator rtt;
	rtt.update(std::chrono::milliseconds(100), std::chrono::microseconds(0));

	cc_.on_persistent_congestion(rtt);

	// Should reduce to minimum window = 2 * max_datagram_size
	EXPECT_EQ(cc_.cwnd(), 2 * cc_.max_datagram_size());
}

TEST_F(CongestionControllerPersistentCongestionTest, ResetsToSlowStart)
{
	quic::rtt_estimator rtt;
	rtt.update(std::chrono::milliseconds(100), std::chrono::microseconds(0));

	cc_.on_persistent_congestion(rtt);

	EXPECT_EQ(cc_.state(), quic::congestion_state::slow_start);
}

// ============================================================================
// Configuration Tests
// ============================================================================

class CongestionControllerConfigTest : public ::testing::Test
{
};

TEST_F(CongestionControllerConfigTest, SetMaxDatagramSize)
{
	quic::congestion_controller cc;

	cc.set_max_datagram_size(1472);

	EXPECT_EQ(cc.max_datagram_size(), 1472u);
}

// ============================================================================
// Reset Tests
// ============================================================================

class CongestionControllerResetTest : public ::testing::Test
{
};

TEST_F(CongestionControllerResetTest, ResetRestoresInitialState)
{
	quic::congestion_controller cc;
	cc.on_packet_sent(5000);
	cc.on_congestion_event(std::chrono::steady_clock::now());

	cc.reset();

	EXPECT_EQ(cc.state(), quic::congestion_state::slow_start);
	EXPECT_EQ(cc.bytes_in_flight(), 0u);
	EXPECT_EQ(cc.cwnd(), 12000u);
}
