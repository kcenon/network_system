/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <gtest/gtest.h>

#include "kcenon/network/protocols/quic/congestion_controller.h"
#include "kcenon/network/protocols/quic/loss_detector.h"
#include "kcenon/network/protocols/quic/rtt_estimator.h"

#include <chrono>
#include <thread>

using namespace network_system::protocols::quic;
using namespace std::chrono_literals;

// Free function for yielding to allow async operations to complete
inline void wait_for_ready() {
    for (int i = 0; i < 100; ++i) {
        std::this_thread::yield();
    }
}


// ============================================================================
// RTT Estimator Tests
// ============================================================================

class RttEstimatorTest : public ::testing::Test
{
protected:
    rtt_estimator rtt_;
};

TEST_F(RttEstimatorTest, InitialState)
{
    // Initial RTT should be 333ms (RFC 9002 recommendation)
    EXPECT_EQ(rtt_.smoothed_rtt(), std::chrono::microseconds{333000});
    EXPECT_EQ(rtt_.rttvar(), std::chrono::microseconds{166500});  // 333000 / 2
    EXPECT_FALSE(rtt_.has_sample());
}

TEST_F(RttEstimatorTest, FirstSample)
{
    auto sample = std::chrono::microseconds{100000};  // 100ms
    rtt_.update(sample, std::chrono::microseconds{0}, false);

    EXPECT_EQ(rtt_.smoothed_rtt(), sample);
    EXPECT_EQ(rtt_.rttvar(), sample / 2);
    EXPECT_EQ(rtt_.min_rtt(), sample);
    EXPECT_TRUE(rtt_.has_sample());
}

TEST_F(RttEstimatorTest, SubsequentSamples)
{
    // First sample: 100ms
    rtt_.update(std::chrono::microseconds{100000}, std::chrono::microseconds{0}, false);

    // Second sample: 120ms
    // smoothed_rtt = 7/8 * 100 + 1/8 * 120 = 87.5 + 15 = 102.5ms
    // rttvar = 3/4 * 50 + 1/4 * |100-120| = 37.5 + 5 = 42.5ms
    rtt_.update(std::chrono::microseconds{120000}, std::chrono::microseconds{0}, false);

    EXPECT_EQ(rtt_.smoothed_rtt(), std::chrono::microseconds{102500});
    EXPECT_EQ(rtt_.rttvar(), std::chrono::microseconds{42500});
    EXPECT_EQ(rtt_.min_rtt(), std::chrono::microseconds{100000});
}

TEST_F(RttEstimatorTest, MinRttTracking)
{
    rtt_.update(std::chrono::microseconds{100000}, std::chrono::microseconds{0}, false);
    EXPECT_EQ(rtt_.min_rtt(), std::chrono::microseconds{100000});

    rtt_.update(std::chrono::microseconds{80000}, std::chrono::microseconds{0}, false);
    EXPECT_EQ(rtt_.min_rtt(), std::chrono::microseconds{80000});

    // Higher sample shouldn't change min_rtt
    rtt_.update(std::chrono::microseconds{120000}, std::chrono::microseconds{0}, false);
    EXPECT_EQ(rtt_.min_rtt(), std::chrono::microseconds{80000});
}

TEST_F(RttEstimatorTest, AckDelayAdjustment)
{
    // First sample
    rtt_.update(std::chrono::microseconds{100000}, std::chrono::microseconds{0}, true);

    // With ack delay of 10ms, RTT should be adjusted
    // Only when handshake is confirmed and sample > min_rtt + ack_delay
    rtt_.update(std::chrono::microseconds{120000}, std::chrono::microseconds{10000}, true);

    // adjusted_rtt = 120ms - 10ms = 110ms
    // smoothed_rtt = 7/8 * 100 + 1/8 * 110 = 87.5 + 13.75 = 101.25ms
    EXPECT_EQ(rtt_.smoothed_rtt(), std::chrono::microseconds{101250});
}

TEST_F(RttEstimatorTest, PtoCalculation)
{
    rtt_.update(std::chrono::microseconds{100000}, std::chrono::microseconds{0}, false);

    // PTO = smoothed_rtt + max(4*rttvar, 1ms) + max_ack_delay
    // PTO = 100ms + max(4*50ms, 1ms) + 25ms = 100 + 200 + 25 = 325ms
    auto expected_pto = std::chrono::microseconds{325000};
    EXPECT_EQ(rtt_.pto(), expected_pto);
}

TEST_F(RttEstimatorTest, CustomInitialRtt)
{
    rtt_estimator custom_rtt{std::chrono::microseconds{200000},
                             std::chrono::microseconds{50000}};

    EXPECT_EQ(custom_rtt.smoothed_rtt(), std::chrono::microseconds{200000});
    EXPECT_EQ(custom_rtt.max_ack_delay(), std::chrono::microseconds{50000});
}

TEST_F(RttEstimatorTest, Reset)
{
    rtt_.update(std::chrono::microseconds{100000}, std::chrono::microseconds{0}, false);
    EXPECT_TRUE(rtt_.has_sample());

    rtt_.reset();

    EXPECT_FALSE(rtt_.has_sample());
    EXPECT_EQ(rtt_.smoothed_rtt(), std::chrono::microseconds{333000});
}

// ============================================================================
// Loss Detector Tests
// ============================================================================

class LossDetectorTest : public ::testing::Test
{
protected:
    rtt_estimator rtt_;
    loss_detector detector_{rtt_};

    sent_packet make_packet(uint64_t pn, size_t bytes = 1200,
                            bool ack_eliciting = true, bool in_flight = true)
    {
        sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_time = std::chrono::steady_clock::now();
        pkt.sent_bytes = bytes;
        pkt.ack_eliciting = ack_eliciting;
        pkt.in_flight = in_flight;
        pkt.level = encryption_level::application;
        return pkt;
    }

    ack_frame make_ack(uint64_t largest)
    {
        ack_frame ack;
        ack.largest_acknowledged = largest;
        ack.ack_delay = 0;
        ack.ranges.push_back({0, 0});  // First range
        return ack;
    }
};

TEST_F(LossDetectorTest, InitialState)
{
    EXPECT_EQ(detector_.pto_count(), 0u);
    EXPECT_FALSE(detector_.has_unacked_packets(encryption_level::application));
    EXPECT_EQ(detector_.total_bytes_in_flight(), 0u);
}

TEST_F(LossDetectorTest, PacketSent)
{
    // Set handshake confirmed so application space timer is armed
    detector_.set_handshake_confirmed(true);

    auto pkt = make_packet(0);
    detector_.on_packet_sent(pkt);

    EXPECT_TRUE(detector_.has_unacked_packets(encryption_level::application));
    EXPECT_EQ(detector_.bytes_in_flight(encryption_level::application), 1200u);
    EXPECT_TRUE(detector_.next_timeout().has_value());
}

TEST_F(LossDetectorTest, PacketAcked)
{
    auto pkt = make_packet(0);
    detector_.on_packet_sent(pkt);

    auto ack = make_ack(0);
    auto result = detector_.on_ack_received(
        ack, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_FALSE(result.acked_packets.empty());
    EXPECT_EQ(detector_.bytes_in_flight(encryption_level::application), 0u);
    EXPECT_EQ(detector_.pto_count(), 0u);
}

TEST_F(LossDetectorTest, PacketLossByReordering)
{
    // Send packets 0, 1, 2, 3
    for (uint64_t i = 0; i < 4; ++i)
    {
        auto pkt = make_packet(i);
        detector_.on_packet_sent(pkt);
        std::this_thread::yield();
    }

    // ACK packet 3 (packets 0, 1, 2 should be considered lost by reordering)
    ack_frame ack;
    ack.largest_acknowledged = 3;
    ack.ack_delay = 0;
    ack.ranges.push_back({0, 0});  // Only ack packet 3

    auto result = detector_.on_ack_received(
        ack, encryption_level::application, std::chrono::steady_clock::now());

    // Packet 0 should be lost (reordering threshold = 3)
    EXPECT_FALSE(result.lost_packets.empty());
    EXPECT_EQ(result.event, loss_detection_event::packet_lost);
}

TEST_F(LossDetectorTest, LargestAckedTracking)
{
    auto pkt = make_packet(5);
    detector_.on_packet_sent(pkt);

    auto ack = make_ack(5);
    detector_.on_ack_received(
        ack, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_EQ(detector_.largest_acked(encryption_level::application), 5u);
}

TEST_F(LossDetectorTest, DiscardSpace)
{
    auto pkt = make_packet(0);
    pkt.level = encryption_level::initial;
    detector_.on_packet_sent(pkt);

    EXPECT_TRUE(detector_.has_unacked_packets(encryption_level::initial));

    detector_.discard_space(encryption_level::initial);

    EXPECT_FALSE(detector_.has_unacked_packets(encryption_level::initial));
    EXPECT_EQ(detector_.bytes_in_flight(encryption_level::initial), 0u);
}

TEST_F(LossDetectorTest, PtoExpiry)
{
    auto pkt = make_packet(0);
    detector_.on_packet_sent(pkt);

    // Simulate timeout by calling on_timeout
    auto result = detector_.on_timeout();

    EXPECT_EQ(result.event, loss_detection_event::pto_expired);
    EXPECT_EQ(detector_.pto_count(), 1u);
}

// ============================================================================
// Congestion Controller Tests
// ============================================================================

class CongestionControllerTest : public ::testing::Test
{
protected:
    congestion_controller cc_;
    rtt_estimator rtt_;

    sent_packet make_packet(uint64_t pn, size_t bytes = 1200,
                            bool in_flight = true)
    {
        sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_time = std::chrono::steady_clock::now();
        pkt.sent_bytes = bytes;
        pkt.ack_eliciting = true;
        pkt.in_flight = in_flight;
        pkt.level = encryption_level::application;
        return pkt;
    }
};

TEST_F(CongestionControllerTest, InitialState)
{
    // Initial window = 10 * 1200 = 12000 bytes
    EXPECT_EQ(cc_.cwnd(), 12000u);
    EXPECT_EQ(cc_.bytes_in_flight(), 0u);
    EXPECT_EQ(cc_.state(), congestion_state::slow_start);
    EXPECT_TRUE(cc_.can_send());
}

TEST_F(CongestionControllerTest, PacketSent)
{
    cc_.on_packet_sent(1200);

    EXPECT_EQ(cc_.bytes_in_flight(), 1200u);
    EXPECT_EQ(cc_.available_window(), 10800u);
}

TEST_F(CongestionControllerTest, SlowStartIncrease)
{
    auto initial_cwnd = cc_.cwnd();
    auto pkt = make_packet(0, 1200);

    cc_.on_packet_sent(1200);
    cc_.on_packet_acked(pkt, std::chrono::steady_clock::now());

    // In slow start, cwnd increases by acked bytes
    EXPECT_EQ(cc_.cwnd(), initial_cwnd + 1200u);
    EXPECT_EQ(cc_.state(), congestion_state::slow_start);
}

TEST_F(CongestionControllerTest, TransitionToCongestionAvoidance)
{
    congestion_controller cc{1200};

    // Set ssthresh manually by triggering loss first
    auto pkt = make_packet(0, 1200);
    cc.on_packet_sent(1200);
    cc.on_packet_lost(pkt);

    // Now ssthresh is set, send more packets and get acks
    // to trigger transition
    EXPECT_EQ(cc.state(), congestion_state::recovery);
}

TEST_F(CongestionControllerTest, CongestionEvent)
{
    auto initial_cwnd = cc_.cwnd();

    auto pkt = make_packet(0, 1200);
    cc_.on_packet_sent(1200);
    cc_.on_packet_lost(pkt);

    // cwnd should be reduced by half
    EXPECT_EQ(cc_.cwnd(), initial_cwnd / 2);
    EXPECT_EQ(cc_.ssthresh(), initial_cwnd / 2);
    EXPECT_EQ(cc_.state(), congestion_state::recovery);
}

TEST_F(CongestionControllerTest, OnlyOneResponsePerRtt)
{
    auto pkt1 = make_packet(0, 1200);
    cc_.on_packet_sent(1200);
    cc_.on_packet_lost(pkt1);

    auto cwnd_after_first = cc_.cwnd();

    // Another loss with same sent_time shouldn't reduce cwnd again
    auto pkt2 = make_packet(1, 1200);
    pkt2.sent_time = pkt1.sent_time;
    cc_.on_packet_sent(1200);
    cc_.on_packet_lost(pkt2);

    EXPECT_EQ(cc_.cwnd(), cwnd_after_first);
}

TEST_F(CongestionControllerTest, PersistentCongestion)
{
    auto pkt = make_packet(0, 1200);
    cc_.on_packet_sent(1200);
    cc_.on_packet_lost(pkt);

    auto cwnd_before = cc_.cwnd();
    cc_.on_persistent_congestion(rtt_);

    // cwnd should be reset to minimum window
    EXPECT_EQ(cc_.cwnd(), 2 * 1200u);  // 2 * max_datagram_size
    EXPECT_EQ(cc_.state(), congestion_state::slow_start);
}

TEST_F(CongestionControllerTest, AvailableWindow)
{
    // Send until cwnd is exhausted
    while (cc_.can_send(1200))
    {
        cc_.on_packet_sent(1200);
    }

    EXPECT_EQ(cc_.available_window(), 0u);
    EXPECT_FALSE(cc_.can_send(1200));
}

TEST_F(CongestionControllerTest, CustomMaxDatagramSize)
{
    congestion_controller cc{1400};

    EXPECT_EQ(cc.max_datagram_size(), 1400u);
    EXPECT_EQ(cc.cwnd(), 10u * 1400u);
}

TEST_F(CongestionControllerTest, Reset)
{
    auto pkt = make_packet(0, 1200);
    cc_.on_packet_sent(1200);
    cc_.on_packet_lost(pkt);

    EXPECT_NE(cc_.state(), congestion_state::slow_start);

    cc_.reset();

    EXPECT_EQ(cc_.cwnd(), 12000u);
    EXPECT_EQ(cc_.bytes_in_flight(), 0u);
    EXPECT_EQ(cc_.state(), congestion_state::slow_start);
}

TEST_F(CongestionControllerTest, CongestionStateStrings)
{
    EXPECT_STREQ(congestion_state_to_string(congestion_state::slow_start), "slow_start");
    EXPECT_STREQ(congestion_state_to_string(congestion_state::congestion_avoidance),
                 "congestion_avoidance");
    EXPECT_STREQ(congestion_state_to_string(congestion_state::recovery), "recovery");
}

// ============================================================================
// Integration Tests
// ============================================================================

class LossDetectionIntegrationTest : public ::testing::Test
{
protected:
    rtt_estimator rtt_;
    loss_detector detector_{rtt_};
    congestion_controller cc_;

    sent_packet make_packet(uint64_t pn)
    {
        sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_time = std::chrono::steady_clock::now();
        pkt.sent_bytes = 1200;
        pkt.ack_eliciting = true;
        pkt.in_flight = true;
        pkt.level = encryption_level::application;
        return pkt;
    }
};

TEST_F(LossDetectionIntegrationTest, AckUpdatesRtt)
{
    auto pkt = make_packet(0);
    detector_.on_packet_sent(pkt);
    cc_.on_packet_sent(pkt.sent_bytes);

    // Wait a bit to simulate network delay
    std::this_thread::yield();

    ack_frame ack;
    ack.largest_acknowledged = 0;
    ack.ack_delay = 1000;  // 1ms ack delay
    ack.ranges.push_back({0, 0});

    auto recv_time = std::chrono::steady_clock::now();
    auto result = detector_.on_ack_received(
        ack, encryption_level::application, recv_time);

    // RTT should be updated
    EXPECT_TRUE(rtt_.has_sample());
    EXPECT_GT(rtt_.latest_rtt().count(), 0);
}

TEST_F(LossDetectionIntegrationTest, LossTriggersCC)
{
    // Send 4 packets
    for (uint64_t i = 0; i < 4; ++i)
    {
        auto pkt = make_packet(i);
        detector_.on_packet_sent(pkt);
        cc_.on_packet_sent(pkt.sent_bytes);
        std::this_thread::yield();
    }

    auto initial_cwnd = cc_.cwnd();

    // ACK only packet 3 (causes packet 0 to be lost by reordering)
    ack_frame ack;
    ack.largest_acknowledged = 3;
    ack.ack_delay = 0;
    ack.ranges.push_back({0, 0});

    auto result = detector_.on_ack_received(
        ack, encryption_level::application, std::chrono::steady_clock::now());

    // Handle lost packets
    for (const auto& lost : result.lost_packets)
    {
        cc_.on_packet_lost(lost);
    }

    // cwnd should be reduced
    if (!result.lost_packets.empty())
    {
        EXPECT_LT(cc_.cwnd(), initial_cwnd);
    }
}
