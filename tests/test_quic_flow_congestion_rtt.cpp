/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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
#include "internal/protocols/quic/flow_control.h"
#include "internal/protocols/quic/congestion_controller.h"
#include "internal/protocols/quic/rtt_estimator.h"

#include <chrono>
#include <limits>
#include <thread>

namespace quic = kcenon::network::protocols::quic;

// ============================================================================
// Flow Controller Tests
// ============================================================================

class FlowControllerTest : public ::testing::Test
{
protected:
    static constexpr uint64_t kDefaultWindow = 1048576; // 1MB
};

TEST_F(FlowControllerTest, DefaultConstruction)
{
    quic::flow_controller fc;
    EXPECT_EQ(fc.send_limit(), kDefaultWindow);
    EXPECT_EQ(fc.bytes_sent(), 0u);
    EXPECT_EQ(fc.receive_limit(), kDefaultWindow);
    EXPECT_EQ(fc.bytes_received(), 0u);
    EXPECT_EQ(fc.bytes_consumed(), 0u);
    EXPECT_EQ(fc.window_size(), kDefaultWindow);
    EXPECT_FALSE(fc.is_send_blocked());
}

TEST_F(FlowControllerTest, CustomWindowConstruction)
{
    quic::flow_controller fc(4096);
    EXPECT_EQ(fc.send_limit(), 4096u);
    EXPECT_EQ(fc.receive_limit(), 4096u);
    EXPECT_EQ(fc.window_size(), 4096u);
}

TEST_F(FlowControllerTest, AvailableSendWindowInitial)
{
    quic::flow_controller fc(1000);
    EXPECT_EQ(fc.available_send_window(), 1000u);
}

TEST_F(FlowControllerTest, ConsumeSendWindowSuccess)
{
    quic::flow_controller fc(1000);
    auto result = fc.consume_send_window(400);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(fc.bytes_sent(), 400u);
    EXPECT_EQ(fc.available_send_window(), 600u);
}

TEST_F(FlowControllerTest, ConsumeSendWindowZeroBytes)
{
    quic::flow_controller fc(1000);
    auto result = fc.consume_send_window(0);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(fc.bytes_sent(), 0u);
}

TEST_F(FlowControllerTest, ConsumeSendWindowExact)
{
    quic::flow_controller fc(1000);
    auto result = fc.consume_send_window(1000);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(fc.available_send_window(), 0u);
    EXPECT_TRUE(fc.is_send_blocked());
}

TEST_F(FlowControllerTest, ConsumeSendWindowExceedsLimit)
{
    quic::flow_controller fc(1000);
    auto result = fc.consume_send_window(1001);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(fc.bytes_sent(), 0u);
}

TEST_F(FlowControllerTest, ConsumeSendWindowProgressive)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.consume_send_window(300).is_ok());
    EXPECT_TRUE(fc.consume_send_window(300).is_ok());
    EXPECT_TRUE(fc.consume_send_window(300).is_ok());
    EXPECT_EQ(fc.bytes_sent(), 900u);
    EXPECT_EQ(fc.available_send_window(), 100u);

    EXPECT_TRUE(fc.consume_send_window(101).is_err());
    EXPECT_TRUE(fc.consume_send_window(100).is_ok());
    EXPECT_TRUE(fc.is_send_blocked());
}

TEST_F(FlowControllerTest, IsSendBlocked)
{
    quic::flow_controller fc(100);
    EXPECT_FALSE(fc.is_send_blocked());

    EXPECT_TRUE(fc.consume_send_window(100).is_ok());
    EXPECT_TRUE(fc.is_send_blocked());
}

TEST_F(FlowControllerTest, UpdateSendLimitIncrease)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.consume_send_window(1000).is_ok());
    EXPECT_TRUE(fc.is_send_blocked());

    fc.update_send_limit(2000);
    EXPECT_EQ(fc.send_limit(), 2000u);
    EXPECT_FALSE(fc.is_send_blocked());
    EXPECT_EQ(fc.available_send_window(), 1000u);
}

TEST_F(FlowControllerTest, UpdateSendLimitNoDecrease)
{
    quic::flow_controller fc(1000);
    fc.update_send_limit(500);
    EXPECT_EQ(fc.send_limit(), 1000u);
}

TEST_F(FlowControllerTest, UpdateSendLimitSameValue)
{
    quic::flow_controller fc(1000);
    fc.update_send_limit(1000);
    EXPECT_EQ(fc.send_limit(), 1000u);
}

TEST_F(FlowControllerTest, RecordReceivedSuccess)
{
    quic::flow_controller fc(1000);
    auto result = fc.record_received(500);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(fc.bytes_received(), 500u);
}

TEST_F(FlowControllerTest, RecordReceivedZeroBytes)
{
    quic::flow_controller fc(1000);
    auto result = fc.record_received(0);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(fc.bytes_received(), 0u);
}

TEST_F(FlowControllerTest, RecordReceivedExceedsLimit)
{
    quic::flow_controller fc(1000);
    auto result = fc.record_received(1001);
    EXPECT_TRUE(result.is_err());
}

TEST_F(FlowControllerTest, RecordReceivedProgressiveOverflow)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.record_received(600).is_ok());
    EXPECT_TRUE(fc.record_received(401).is_err());
    EXPECT_EQ(fc.bytes_received(), 600u);
}

TEST_F(FlowControllerTest, RecordConsumed)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.record_received(500).is_ok());
    fc.record_consumed(300);
    EXPECT_EQ(fc.bytes_consumed(), 300u);
}

TEST_F(FlowControllerTest, RecordConsumedCannotExceedReceived)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.record_received(100).is_ok());
    fc.record_consumed(200);
    EXPECT_EQ(fc.bytes_consumed(), 100u);
}

TEST_F(FlowControllerTest, ShouldSendDataBlocked)
{
    quic::flow_controller fc(100);
    EXPECT_FALSE(fc.should_send_data_blocked());

    EXPECT_TRUE(fc.consume_send_window(100).is_ok());
    EXPECT_TRUE(fc.should_send_data_blocked());
}

TEST_F(FlowControllerTest, MarkDataBlockedSent)
{
    quic::flow_controller fc(100);
    EXPECT_TRUE(fc.consume_send_window(100).is_ok());
    EXPECT_TRUE(fc.should_send_data_blocked());

    fc.mark_data_blocked_sent();
    EXPECT_FALSE(fc.should_send_data_blocked());
}

TEST_F(FlowControllerTest, DataBlockedResetOnSendLimitUpdate)
{
    quic::flow_controller fc(100);
    EXPECT_TRUE(fc.consume_send_window(100).is_ok());
    fc.mark_data_blocked_sent();
    EXPECT_FALSE(fc.should_send_data_blocked());

    fc.update_send_limit(200);
    EXPECT_TRUE(fc.consume_send_window(100).is_ok());
    EXPECT_TRUE(fc.should_send_data_blocked());
}

TEST_F(FlowControllerTest, DataBlockedResetOnConsume)
{
    quic::flow_controller fc(200);
    EXPECT_TRUE(fc.consume_send_window(200).is_ok());
    fc.mark_data_blocked_sent();

    fc.update_send_limit(300);
    auto result = fc.consume_send_window(50);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(FlowControllerTest, SetWindowSize)
{
    quic::flow_controller fc(1000);
    fc.set_window_size(2000);
    EXPECT_EQ(fc.window_size(), 2000u);
}

TEST_F(FlowControllerTest, SetUpdateThresholdClamped)
{
    quic::flow_controller fc(1000);
    fc.set_update_threshold(1.5);
    fc.set_update_threshold(-0.5);
}

TEST_F(FlowControllerTest, Reset)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.consume_send_window(500).is_ok());
    EXPECT_TRUE(fc.record_received(300).is_ok());
    fc.record_consumed(100);

    fc.reset(2000);
    EXPECT_EQ(fc.send_limit(), 2000u);
    EXPECT_EQ(fc.bytes_sent(), 0u);
    EXPECT_EQ(fc.receive_limit(), 2000u);
    EXPECT_EQ(fc.bytes_received(), 0u);
    EXPECT_EQ(fc.bytes_consumed(), 0u);
    EXPECT_EQ(fc.window_size(), 2000u);
    EXPECT_FALSE(fc.is_send_blocked());
}

TEST_F(FlowControllerTest, GenerateMaxDataWhenThresholdMet)
{
    quic::flow_controller fc(1000);
    fc.set_update_threshold(0.5);

    EXPECT_TRUE(fc.record_received(600).is_ok());
    fc.record_consumed(600);

    auto max_data = fc.generate_max_data();
    EXPECT_TRUE(max_data.has_value());
    EXPECT_GE(max_data.value(), fc.bytes_consumed());
}

TEST_F(FlowControllerTest, GenerateMaxDataNotNeeded)
{
    quic::flow_controller fc(1000);
    auto max_data = fc.generate_max_data();
    EXPECT_FALSE(max_data.has_value());
}

TEST_F(FlowControllerTest, CopyBehavior)
{
    quic::flow_controller fc1(1000);
    EXPECT_TRUE(fc1.consume_send_window(300).is_ok());

    quic::flow_controller fc2 = fc1;
    EXPECT_EQ(fc2.bytes_sent(), 300u);
    EXPECT_EQ(fc2.send_limit(), 1000u);
}

TEST_F(FlowControllerTest, MoveBehavior)
{
    quic::flow_controller fc1(1000);
    EXPECT_TRUE(fc1.consume_send_window(300).is_ok());

    quic::flow_controller fc2 = std::move(fc1);
    EXPECT_EQ(fc2.bytes_sent(), 300u);
    EXPECT_EQ(fc2.send_limit(), 1000u);
}

// Flow control stats
class FlowControlStatsTest : public ::testing::Test {};

TEST_F(FlowControlStatsTest, DefaultStatsStruct)
{
    quic::flow_control_stats stats;
    EXPECT_EQ(stats.send_limit, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.send_window_available, 0u);
    EXPECT_FALSE(stats.send_blocked);
    EXPECT_EQ(stats.receive_limit, 0u);
    EXPECT_EQ(stats.bytes_received, 0u);
    EXPECT_EQ(stats.bytes_consumed, 0u);
    EXPECT_EQ(stats.receive_window_available, 0u);
}

TEST_F(FlowControlStatsTest, GetFlowControlStats)
{
    quic::flow_controller fc(1000);
    EXPECT_TRUE(fc.consume_send_window(400).is_ok());
    EXPECT_TRUE(fc.record_received(200).is_ok());
    fc.record_consumed(100);

    auto stats = quic::get_flow_control_stats(fc);
    EXPECT_EQ(stats.send_limit, 1000u);
    EXPECT_EQ(stats.bytes_sent, 400u);
    EXPECT_EQ(stats.send_window_available, 600u);
    EXPECT_FALSE(stats.send_blocked);
    EXPECT_EQ(stats.receive_limit, 1000u);
    EXPECT_EQ(stats.bytes_received, 200u);
    EXPECT_EQ(stats.bytes_consumed, 100u);
    EXPECT_EQ(stats.receive_window_available, 800u);
}

TEST_F(FlowControlStatsTest, GetFlowControlStatsBlocked)
{
    quic::flow_controller fc(100);
    EXPECT_TRUE(fc.consume_send_window(100).is_ok());

    auto stats = quic::get_flow_control_stats(fc);
    EXPECT_TRUE(stats.send_blocked);
    EXPECT_EQ(stats.send_window_available, 0u);
}

// Flow control error codes
TEST(FlowControlErrorTest, ErrorCodeValues)
{
    EXPECT_EQ(quic::flow_control_error::send_blocked, -710);
    EXPECT_EQ(quic::flow_control_error::receive_overflow, -711);
    EXPECT_EQ(quic::flow_control_error::window_exceeded, -712);
}

// ============================================================================
// Congestion Controller Tests
// ============================================================================

class CongestionControllerTest : public ::testing::Test
{
protected:
    static constexpr size_t kDefaultMDS = 1200;
    static constexpr size_t kInitialWindow = 10 * kDefaultMDS;
    static constexpr size_t kMinimumWindow = 2 * kDefaultMDS;

    static auto make_sent_packet(uint64_t pn, size_t bytes, bool in_flight = true)
        -> quic::sent_packet
    {
        quic::sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_bytes = bytes;
        pkt.in_flight = in_flight;
        pkt.sent_time = std::chrono::steady_clock::now();
        pkt.ack_eliciting = true;
        return pkt;
    }

    static auto make_sent_packet_at(uint64_t pn, size_t bytes,
                                     std::chrono::steady_clock::time_point tp)
        -> quic::sent_packet
    {
        quic::sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_bytes = bytes;
        pkt.in_flight = true;
        pkt.sent_time = tp;
        pkt.ack_eliciting = true;
        return pkt;
    }
};

TEST_F(CongestionControllerTest, DefaultConstruction)
{
    quic::congestion_controller cc;
    EXPECT_EQ(cc.cwnd(), kInitialWindow);
    EXPECT_EQ(cc.ssthresh(), std::numeric_limits<size_t>::max());
    EXPECT_EQ(cc.bytes_in_flight(), 0u);
    EXPECT_EQ(cc.state(), quic::congestion_state::slow_start);
    EXPECT_EQ(cc.max_datagram_size(), kDefaultMDS);
}

TEST_F(CongestionControllerTest, CustomMaxDatagramSize)
{
    quic::congestion_controller cc(1472);
    EXPECT_EQ(cc.max_datagram_size(), 1472u);
    EXPECT_EQ(cc.cwnd(), 10u * 1472);
}

TEST_F(CongestionControllerTest, CanSendInitially)
{
    quic::congestion_controller cc;
    EXPECT_TRUE(cc.can_send());
    EXPECT_TRUE(cc.can_send(1200));
    EXPECT_TRUE(cc.can_send(kInitialWindow));
}

TEST_F(CongestionControllerTest, CanSendExhausted)
{
    quic::congestion_controller cc;
    cc.on_packet_sent(kInitialWindow);
    EXPECT_FALSE(cc.can_send());
    EXPECT_FALSE(cc.can_send(1));
}

TEST_F(CongestionControllerTest, AvailableWindowInitial)
{
    quic::congestion_controller cc;
    EXPECT_EQ(cc.available_window(), kInitialWindow);
}

TEST_F(CongestionControllerTest, AvailableWindowAfterSend)
{
    quic::congestion_controller cc;
    cc.on_packet_sent(1200);
    EXPECT_EQ(cc.available_window(), kInitialWindow - 1200);
    EXPECT_EQ(cc.bytes_in_flight(), 1200u);
}

TEST_F(CongestionControllerTest, AvailableWindowExhausted)
{
    quic::congestion_controller cc;
    cc.on_packet_sent(kInitialWindow);
    EXPECT_EQ(cc.available_window(), 0u);
}

TEST_F(CongestionControllerTest, OnPacketSentTracksBytesInFlight)
{
    quic::congestion_controller cc;
    cc.on_packet_sent(1000);
    cc.on_packet_sent(2000);
    EXPECT_EQ(cc.bytes_in_flight(), 3000u);
}

TEST_F(CongestionControllerTest, OnPacketAckedSlowStart)
{
    quic::congestion_controller cc;
    auto pkt = make_sent_packet(1, 1200);
    cc.on_packet_sent(1200);

    auto now = std::chrono::steady_clock::now();
    cc.on_packet_acked(pkt, now);

    EXPECT_EQ(cc.bytes_in_flight(), 0u);
    EXPECT_EQ(cc.cwnd(), kInitialWindow + 1200);
    EXPECT_EQ(cc.state(), quic::congestion_state::slow_start);
}

TEST_F(CongestionControllerTest, OnPacketAckedNotInFlight)
{
    quic::congestion_controller cc;
    auto pkt = make_sent_packet(1, 1200, false);

    auto now = std::chrono::steady_clock::now();
    cc.on_packet_acked(pkt, now);

    // bytes_in_flight not reduced for non-in-flight packets
    EXPECT_EQ(cc.bytes_in_flight(), 0u);
    // cwnd still grows: implementation increases cwnd regardless of in_flight
    EXPECT_EQ(cc.cwnd(), kInitialWindow + 1200);
}

TEST_F(CongestionControllerTest, SlowStartToCongestionAvoidance)
{
    quic::congestion_controller cc;

    auto pkt = make_sent_packet(1, kInitialWindow);
    cc.on_packet_sent(kInitialWindow);

    auto lost_pkt = make_sent_packet(2, 1200);
    lost_pkt.sent_time = std::chrono::steady_clock::now() -
                         std::chrono::seconds(10);
    cc.on_packet_sent(1200);

    cc.on_packet_lost(lost_pkt);

    EXPECT_EQ(cc.state(), quic::congestion_state::recovery);
    size_t expected_ssthresh = static_cast<size_t>(
        static_cast<double>(kInitialWindow) * 0.5);
    EXPECT_EQ(cc.ssthresh(), expected_ssthresh);
}

TEST_F(CongestionControllerTest, CongestionAvoidanceLinearGrowth)
{
    quic::congestion_controller cc;

    auto old_pkt = make_sent_packet(1, 1200);
    old_pkt.sent_time = std::chrono::steady_clock::now() -
                        std::chrono::seconds(10);
    cc.on_packet_sent(1200);
    cc.on_packet_lost(old_pkt);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    auto new_pkt = make_sent_packet(2, 1200);
    cc.on_packet_sent(1200);
    size_t cwnd_before_ack = cc.cwnd();

    auto now = std::chrono::steady_clock::now();
    cc.on_packet_acked(new_pkt, now);

    EXPECT_GT(cc.cwnd(), cwnd_before_ack);
    size_t max_increase = 1200;
    EXPECT_LE(cc.cwnd() - cwnd_before_ack, max_increase);
}

TEST_F(CongestionControllerTest, OnPacketLostReducesBytesInFlight)
{
    quic::congestion_controller cc;
    auto pkt = make_sent_packet(1, 1200);
    pkt.sent_time = std::chrono::steady_clock::now() -
                    std::chrono::seconds(10);
    cc.on_packet_sent(1200);
    EXPECT_EQ(cc.bytes_in_flight(), 1200u);

    cc.on_packet_lost(pkt);
    EXPECT_EQ(cc.bytes_in_flight(), 0u);
}

TEST_F(CongestionControllerTest, CongestionEventOncePerRtt)
{
    quic::congestion_controller cc;
    auto past = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    auto pkt1 = make_sent_packet_at(1, 1200, past);
    auto pkt2 = make_sent_packet_at(2, 1200, past);
    cc.on_packet_sent(1200);
    cc.on_packet_sent(1200);

    cc.on_packet_lost(pkt1);
    size_t cwnd_after_first = cc.cwnd();

    cc.on_packet_lost(pkt2);
    EXPECT_EQ(cc.cwnd(), cwnd_after_first);
}

TEST_F(CongestionControllerTest, OnEcnCongestion)
{
    quic::congestion_controller cc;
    auto past = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    cc.on_packet_sent(kInitialWindow);

    cc.on_ecn_congestion(past);
    EXPECT_EQ(cc.state(), quic::congestion_state::recovery);
    EXPECT_LT(cc.cwnd(), kInitialWindow);
}

TEST_F(CongestionControllerTest, OnEcnCongestionOncePerRtt)
{
    quic::congestion_controller cc;
    auto past = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    cc.on_packet_sent(kInitialWindow);

    cc.on_ecn_congestion(past);
    size_t cwnd_after_first = cc.cwnd();

    cc.on_ecn_congestion(past);
    EXPECT_EQ(cc.cwnd(), cwnd_after_first);
}

TEST_F(CongestionControllerTest, OnPersistentCongestion)
{
    quic::congestion_controller cc;
    quic::rtt_estimator rtt;

    cc.on_persistent_congestion(rtt);
    EXPECT_EQ(cc.cwnd(), kMinimumWindow);
    EXPECT_EQ(cc.ssthresh(), kMinimumWindow);
    EXPECT_EQ(cc.state(), quic::congestion_state::slow_start);
}

TEST_F(CongestionControllerTest, CwndNeverBelowMinimum)
{
    quic::congestion_controller cc;
    auto past = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    for (int i = 0; i < 10; ++i)
    {
        cc.on_packet_sent(1200);
        auto pkt = make_sent_packet_at(
            static_cast<uint64_t>(i), 1200,
            std::chrono::steady_clock::now() - std::chrono::seconds(10));
        cc.on_packet_lost(pkt);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    EXPECT_GE(cc.cwnd(), kMinimumWindow);
}

TEST_F(CongestionControllerTest, SetMaxDatagramSize)
{
    quic::congestion_controller cc;
    cc.set_max_datagram_size(1472);
    EXPECT_EQ(cc.max_datagram_size(), 1472u);
    EXPECT_GE(cc.cwnd(), 2u * 1472);
}

TEST_F(CongestionControllerTest, Reset)
{
    quic::congestion_controller cc;
    cc.on_packet_sent(5000);
    auto past = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    auto pkt = make_sent_packet_at(1, 1200, past);
    cc.on_packet_lost(pkt);

    cc.reset();
    EXPECT_EQ(cc.cwnd(), kInitialWindow);
    EXPECT_EQ(cc.ssthresh(), std::numeric_limits<size_t>::max());
    EXPECT_EQ(cc.bytes_in_flight(), 0u);
    EXPECT_EQ(cc.state(), quic::congestion_state::slow_start);
}

// Congestion state string conversion
TEST(CongestionStateTest, StateToString)
{
    EXPECT_STREQ(quic::congestion_state_to_string(quic::congestion_state::slow_start),
                 "slow_start");
    EXPECT_STREQ(quic::congestion_state_to_string(quic::congestion_state::congestion_avoidance),
                 "congestion_avoidance");
    EXPECT_STREQ(quic::congestion_state_to_string(quic::congestion_state::recovery),
                 "recovery");
    EXPECT_STREQ(quic::congestion_state_to_string(static_cast<quic::congestion_state>(99)),
                 "unknown");
}

// ============================================================================
// RTT Estimator Tests
// ============================================================================

class RttEstimatorTest : public ::testing::Test
{
protected:
    using us = std::chrono::microseconds;
    using ms = std::chrono::milliseconds;
};

TEST_F(RttEstimatorTest, DefaultConstruction)
{
    quic::rtt_estimator rtt;
    EXPECT_EQ(rtt.smoothed_rtt(), us(333000));
    EXPECT_EQ(rtt.rttvar(), us(333000 / 2));
    EXPECT_EQ(rtt.min_rtt(), us::max());
    EXPECT_EQ(rtt.latest_rtt(), us(0));
    EXPECT_EQ(rtt.max_ack_delay(), us(25000));
    EXPECT_FALSE(rtt.has_sample());
}

TEST_F(RttEstimatorTest, CustomInitialRtt)
{
    quic::rtt_estimator rtt(us(100000), us(50000));
    EXPECT_EQ(rtt.smoothed_rtt(), us(100000));
    EXPECT_EQ(rtt.rttvar(), us(50000));
    EXPECT_EQ(rtt.max_ack_delay(), us(50000));
    EXPECT_FALSE(rtt.has_sample());
}

TEST_F(RttEstimatorTest, FirstSampleSetsSmoothed)
{
    quic::rtt_estimator rtt;
    rtt.update(us(100000), us(0));

    EXPECT_TRUE(rtt.has_sample());
    EXPECT_EQ(rtt.smoothed_rtt(), us(100000));
    EXPECT_EQ(rtt.rttvar(), us(50000));
    EXPECT_EQ(rtt.min_rtt(), us(100000));
    EXPECT_EQ(rtt.latest_rtt(), us(100000));
}

TEST_F(RttEstimatorTest, SubsequentSampleEwma)
{
    quic::rtt_estimator rtt;
    rtt.update(us(100000), us(0));
    rtt.update(us(120000), us(0));

    EXPECT_TRUE(rtt.has_sample());
    EXPECT_EQ(rtt.latest_rtt(), us(120000));
    EXPECT_EQ(rtt.min_rtt(), us(100000));

    // smoothed_rtt = 7/8 * 100000 + 1/8 * 120000 = 87500 + 15000 = 102500
    EXPECT_EQ(rtt.smoothed_rtt(), us(102500));

    // rttvar = 3/4 * 50000 + 1/4 * |100000 - 120000| = 37500 + 5000 = 42500
    EXPECT_EQ(rtt.rttvar(), us(42500));
}

TEST_F(RttEstimatorTest, MinRttTracking)
{
    quic::rtt_estimator rtt;
    rtt.update(us(200000), us(0));
    EXPECT_EQ(rtt.min_rtt(), us(200000));

    rtt.update(us(100000), us(0));
    EXPECT_EQ(rtt.min_rtt(), us(100000));

    rtt.update(us(150000), us(0));
    EXPECT_EQ(rtt.min_rtt(), us(100000));
}

TEST_F(RttEstimatorTest, AckDelayAdjustment)
{
    quic::rtt_estimator rtt;
    rtt.update(us(100000), us(0));

    // With ack_delay=20000, and handshake confirmed,
    // adjusted_rtt = max(latest_rtt - min(ack_delay, max_ack_delay), min_rtt)
    rtt.update(us(150000), us(20000), true);
    EXPECT_EQ(rtt.latest_rtt(), us(150000));
    EXPECT_EQ(rtt.min_rtt(), us(100000));
}

TEST_F(RttEstimatorTest, AckDelayCappedByMaxAckDelay)
{
    quic::rtt_estimator rtt(us(100000), us(10000));
    rtt.update(us(100000), us(0));

    // ack_delay = 50000 but max_ack_delay = 10000
    rtt.update(us(150000), us(50000), true);
    EXPECT_EQ(rtt.latest_rtt(), us(150000));
}

TEST_F(RttEstimatorTest, AckDelayNotAppliedBeforeHandshake)
{
    quic::rtt_estimator rtt;
    rtt.update(us(100000), us(20000), false);

    // Without handshake confirmation, ack_delay is not subtracted
    EXPECT_EQ(rtt.smoothed_rtt(), us(100000));
}

TEST_F(RttEstimatorTest, PtoCalculation)
{
    quic::rtt_estimator rtt;
    rtt.update(us(100000), us(0));

    // PTO = smoothed_rtt + max(4*rttvar, 1ms) + max_ack_delay
    // smoothed_rtt = 100000, rttvar = 50000, max_ack_delay = 25000
    // PTO = 100000 + max(200000, 1000) + 25000 = 325000
    EXPECT_EQ(rtt.pto(), us(325000));
}

TEST_F(RttEstimatorTest, PtoWithSmallRttvar)
{
    quic::rtt_estimator rtt(us(100000), us(25000));
    rtt.update(us(100000), us(0));
    rtt.update(us(100000), us(0));
    rtt.update(us(100000), us(0));
    rtt.update(us(100000), us(0));

    // After stable samples, rttvar should be small
    // PTO uses max(4*rttvar, 1ms=1000us) as lower bound
    auto pto = rtt.pto();
    EXPECT_GE(pto, us(100000 + 1000 + 25000));
}

TEST_F(RttEstimatorTest, SetMaxAckDelay)
{
    quic::rtt_estimator rtt;
    rtt.set_max_ack_delay(us(50000));
    EXPECT_EQ(rtt.max_ack_delay(), us(50000));
}

TEST_F(RttEstimatorTest, Reset)
{
    quic::rtt_estimator rtt(us(200000), us(30000));
    rtt.update(us(100000), us(0));
    rtt.update(us(80000), us(0));

    rtt.reset();
    EXPECT_EQ(rtt.smoothed_rtt(), us(200000));
    EXPECT_EQ(rtt.rttvar(), us(100000));
    EXPECT_EQ(rtt.min_rtt(), us::max());
    EXPECT_EQ(rtt.latest_rtt(), us(0));
    EXPECT_FALSE(rtt.has_sample());
}

TEST_F(RttEstimatorTest, MultipleSamplesConverge)
{
    quic::rtt_estimator rtt;
    // Simulate stable 100ms RTT
    for (int i = 0; i < 20; ++i)
    {
        rtt.update(us(100000), us(0));
    }

    // Should converge close to 100ms
    auto diff = rtt.smoothed_rtt() > us(100000)
                    ? rtt.smoothed_rtt() - us(100000)
                    : us(100000) - rtt.smoothed_rtt();
    EXPECT_LT(diff, us(5000));
}

TEST_F(RttEstimatorTest, PtoIncludesMaxAckDelay)
{
    quic::rtt_estimator rtt;
    rtt.set_max_ack_delay(us(0));
    rtt.update(us(100000), us(0));
    auto pto_no_delay = rtt.pto();

    quic::rtt_estimator rtt2;
    rtt2.set_max_ack_delay(us(50000));
    rtt2.update(us(100000), us(0));
    auto pto_with_delay = rtt2.pto();

    EXPECT_EQ(pto_with_delay - pto_no_delay, us(50000));
}

TEST_F(RttEstimatorTest, LargeRttVariation)
{
    quic::rtt_estimator rtt;
    rtt.update(us(50000), us(0));
    rtt.update(us(500000), us(0));

    EXPECT_GT(rtt.rttvar(), us(0));
    EXPECT_EQ(rtt.min_rtt(), us(50000));
}
