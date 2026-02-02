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

#include "internal/protocols/quic/congestion_controller.h"
#include "internal/protocols/quic/ecn_tracker.h"
#include "internal/protocols/quic/loss_detector.h"
#include "internal/protocols/quic/rtt_estimator.h"

#include <chrono>
#include <thread>

using namespace kcenon::network::protocols::quic;
using namespace std::chrono_literals;

// ============================================================================
// ECN Tracker Tests
// ============================================================================

class EcnTrackerTest : public ::testing::Test
{
protected:
    ecn_tracker tracker_;

    ecn_counts make_counts(uint64_t ect0, uint64_t ect1, uint64_t ecn_ce)
    {
        ecn_counts counts;
        counts.ect0 = ect0;
        counts.ect1 = ect1;
        counts.ecn_ce = ecn_ce;
        return counts;
    }
};

TEST_F(EcnTrackerTest, InitialState)
{
    EXPECT_FALSE(tracker_.is_ecn_capable());
    EXPECT_TRUE(tracker_.is_testing());
    EXPECT_FALSE(tracker_.has_failed());
    EXPECT_EQ(tracker_.get_ecn_marking(), ecn_marking::ect0);
}

TEST_F(EcnTrackerTest, ProcessEcnCountsNoSignal)
{
    auto sent_time = std::chrono::steady_clock::now();
    auto counts = make_counts(5, 0, 0);

    auto result = tracker_.process_ecn_counts(counts, 5, sent_time);

    EXPECT_EQ(result, ecn_result::none);
    EXPECT_FALSE(tracker_.has_failed());
}

TEST_F(EcnTrackerTest, ProcessEcnCountsCongestionSignal)
{
    auto sent_time = std::chrono::steady_clock::now();

    // First, set baseline counts
    auto counts1 = make_counts(5, 0, 0);
    tracker_.process_ecn_counts(counts1, 5, sent_time);

    // Now ECN-CE increases - congestion signal
    auto counts2 = make_counts(5, 0, 2);
    auto result = tracker_.process_ecn_counts(counts2, 0, sent_time);

    EXPECT_EQ(result, ecn_result::congestion_signal);
}

TEST_F(EcnTrackerTest, EcnValidationFailureCountsDecrease)
{
    auto sent_time = std::chrono::steady_clock::now();

    // Set initial counts
    auto counts1 = make_counts(10, 0, 0);
    tracker_.process_ecn_counts(counts1, 10, sent_time);

    // Counts decrease - validation failure
    auto counts2 = make_counts(5, 0, 0);
    auto result = tracker_.process_ecn_counts(counts2, 0, sent_time);

    EXPECT_EQ(result, ecn_result::ecn_failure);
    EXPECT_TRUE(tracker_.has_failed());
    EXPECT_FALSE(tracker_.is_ecn_capable());
    EXPECT_EQ(tracker_.get_ecn_marking(), ecn_marking::not_ect);
}

TEST_F(EcnTrackerTest, EcnValidationSuccess)
{
    auto sent_time = std::chrono::steady_clock::now();

    // Send enough packets to pass validation threshold
    tracker_.on_packets_sent(15);

    // Process counts that match sent packets
    auto counts = make_counts(15, 0, 0);
    tracker_.process_ecn_counts(counts, 15, sent_time);

    EXPECT_TRUE(tracker_.is_ecn_capable());
    EXPECT_FALSE(tracker_.is_testing());
    EXPECT_FALSE(tracker_.has_failed());
}

TEST_F(EcnTrackerTest, OnPacketsSent)
{
    tracker_.on_packets_sent(5);
    tracker_.on_packets_sent(3);

    // After ECN is disabled, packets should not be tracked
    tracker_.disable();
    tracker_.on_packets_sent(10);

    EXPECT_TRUE(tracker_.has_failed());
}

TEST_F(EcnTrackerTest, Disable)
{
    tracker_.disable();

    EXPECT_TRUE(tracker_.has_failed());
    EXPECT_FALSE(tracker_.is_ecn_capable());
    EXPECT_FALSE(tracker_.is_testing());
    EXPECT_EQ(tracker_.get_ecn_marking(), ecn_marking::not_ect);
}

TEST_F(EcnTrackerTest, Reset)
{
    auto sent_time = std::chrono::steady_clock::now();

    // Set some state
    auto counts = make_counts(10, 0, 2);
    tracker_.process_ecn_counts(counts, 10, sent_time);

    tracker_.reset();

    EXPECT_FALSE(tracker_.is_ecn_capable());
    EXPECT_TRUE(tracker_.is_testing());
    EXPECT_FALSE(tracker_.has_failed());
}

TEST_F(EcnTrackerTest, LastCongestionSentTime)
{
    auto sent_time1 = std::chrono::steady_clock::now();
    auto counts1 = make_counts(5, 0, 0);
    tracker_.process_ecn_counts(counts1, 5, sent_time1);

    std::this_thread::sleep_for(1ms);
    auto sent_time2 = std::chrono::steady_clock::now();

    // Trigger congestion
    auto counts2 = make_counts(5, 0, 1);
    tracker_.process_ecn_counts(counts2, 0, sent_time2);

    EXPECT_EQ(tracker_.last_congestion_sent_time(), sent_time2);
}

TEST_F(EcnTrackerTest, EcnResultStrings)
{
    EXPECT_STREQ(ecn_result_to_string(ecn_result::none), "none");
    EXPECT_STREQ(ecn_result_to_string(ecn_result::congestion_signal), "congestion_signal");
    EXPECT_STREQ(ecn_result_to_string(ecn_result::ecn_failure), "ecn_failure");
}

TEST_F(EcnTrackerTest, MultipleCongestionsSignals)
{
    auto sent_time = std::chrono::steady_clock::now();

    auto counts1 = make_counts(5, 0, 0);
    tracker_.process_ecn_counts(counts1, 5, sent_time);

    // First congestion signal
    auto counts2 = make_counts(5, 0, 1);
    auto result1 = tracker_.process_ecn_counts(counts2, 0, sent_time);
    EXPECT_EQ(result1, ecn_result::congestion_signal);

    // Second congestion signal (ECN-CE increases again)
    auto counts3 = make_counts(5, 0, 3);
    auto result2 = tracker_.process_ecn_counts(counts3, 0, sent_time);
    EXPECT_EQ(result2, ecn_result::congestion_signal);
}

// ============================================================================
// ECN Integration with Loss Detector Tests
// ============================================================================

class EcnLossDetectorIntegrationTest : public ::testing::Test
{
protected:
    rtt_estimator rtt_;
    loss_detector detector_{rtt_};

    sent_packet make_packet(uint64_t pn, size_t bytes = 1200)
    {
        sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_time = std::chrono::steady_clock::now();
        pkt.sent_bytes = bytes;
        pkt.ack_eliciting = true;
        pkt.in_flight = true;
        pkt.level = encryption_level::application;
        return pkt;
    }

    ack_frame make_ack_ecn(uint64_t largest, uint64_t ect0, uint64_t ect1, uint64_t ecn_ce)
    {
        ack_frame ack;
        ack.largest_acknowledged = largest;
        ack.ack_delay = 0;
        ack.ranges.push_back({0, 0});
        ack.ecn = ecn_counts{ect0, ect1, ecn_ce};
        return ack;
    }
};

TEST_F(EcnLossDetectorIntegrationTest, AckEcnProcessed)
{
    auto pkt = make_packet(0);
    detector_.on_packet_sent(pkt);

    auto ack = make_ack_ecn(0, 1, 0, 0);
    auto result = detector_.on_ack_received(
        ack, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_EQ(result.ecn_signal, ecn_result::none);
    EXPECT_FALSE(result.acked_packets.empty());
}

TEST_F(EcnLossDetectorIntegrationTest, AckEcnCongestionSignal)
{
    // Send first packet
    auto pkt1 = make_packet(0);
    detector_.on_packet_sent(pkt1);

    // First ACK with no congestion
    auto ack1 = make_ack_ecn(0, 1, 0, 0);
    detector_.on_ack_received(
        ack1, encryption_level::application, std::chrono::steady_clock::now());

    // Send second packet
    auto pkt2 = make_packet(1);
    detector_.on_packet_sent(pkt2);

    // Second ACK with ECN-CE increase
    auto ack2 = make_ack_ecn(1, 1, 0, 1);
    auto result = detector_.on_ack_received(
        ack2, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_EQ(result.ecn_signal, ecn_result::congestion_signal);
}

TEST_F(EcnLossDetectorIntegrationTest, EcnTrackerAccessor)
{
    auto& tracker = detector_.get_ecn_tracker();
    EXPECT_TRUE(tracker.is_testing());
    EXPECT_FALSE(tracker.has_failed());
}

// ============================================================================
// ECN Integration with Congestion Controller Tests
// ============================================================================

class EcnCongestionControllerTest : public ::testing::Test
{
protected:
    congestion_controller cc_;

    sent_packet make_packet(uint64_t pn, size_t bytes = 1200)
    {
        sent_packet pkt;
        pkt.packet_number = pn;
        pkt.sent_time = std::chrono::steady_clock::now();
        pkt.sent_bytes = bytes;
        pkt.ack_eliciting = true;
        pkt.in_flight = true;
        pkt.level = encryption_level::application;
        return pkt;
    }
};

TEST_F(EcnCongestionControllerTest, EcnCongestionReducesCwnd)
{
    auto initial_cwnd = cc_.cwnd();
    auto sent_time = std::chrono::steady_clock::now();

    cc_.on_packet_sent(1200);
    cc_.on_ecn_congestion(sent_time);

    // cwnd should be reduced by half
    EXPECT_EQ(cc_.cwnd(), initial_cwnd / 2);
    EXPECT_EQ(cc_.state(), congestion_state::recovery);
}

TEST_F(EcnCongestionControllerTest, EcnCongestionOnlyOncePerRtt)
{
    auto sent_time = std::chrono::steady_clock::now();

    cc_.on_packet_sent(1200);
    cc_.on_ecn_congestion(sent_time);

    auto cwnd_after_first = cc_.cwnd();

    // Second ECN congestion with same sent_time shouldn't reduce cwnd again
    cc_.on_packet_sent(1200);
    cc_.on_ecn_congestion(sent_time);

    EXPECT_EQ(cc_.cwnd(), cwnd_after_first);
}

TEST_F(EcnCongestionControllerTest, EcnCongestionSameAsPacketLoss)
{
    // Create two identical congestion controllers
    congestion_controller cc1;
    congestion_controller cc2;

    auto sent_time = std::chrono::steady_clock::now();
    auto pkt = make_packet(0);

    // One uses ECN congestion
    cc1.on_packet_sent(1200);
    cc1.on_ecn_congestion(sent_time);

    // Other uses packet loss
    cc2.on_packet_sent(1200);
    cc2.on_packet_lost(pkt);

    // Both should have same cwnd
    EXPECT_EQ(cc1.cwnd(), cc2.cwnd());
    EXPECT_EQ(cc1.state(), cc2.state());
}

// ============================================================================
// Full ECN Integration Test
// ============================================================================

class FullEcnIntegrationTest : public ::testing::Test
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

TEST_F(FullEcnIntegrationTest, EcnFlowWithCongestionResponse)
{
    // Send packets
    for (uint64_t i = 0; i < 5; ++i)
    {
        auto pkt = make_packet(i);
        detector_.on_packet_sent(pkt);
        cc_.on_packet_sent(pkt.sent_bytes);
        std::this_thread::yield();
    }

    auto initial_cwnd = cc_.cwnd();

    // First ACK without congestion
    ack_frame ack1;
    ack1.largest_acknowledged = 2;
    ack1.ack_delay = 0;
    ack1.ranges.push_back({0, 2});
    ack1.ecn = ecn_counts{3, 0, 0};

    auto result1 = detector_.on_ack_received(
        ack1, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_EQ(result1.ecn_signal, ecn_result::none);

    // Handle acked packets
    for (const auto& acked : result1.acked_packets)
    {
        cc_.on_packet_acked(acked, std::chrono::steady_clock::now());
    }

    // Second ACK with ECN-CE (congestion)
    ack_frame ack2;
    ack2.largest_acknowledged = 4;
    ack2.ack_delay = 0;
    ack2.ranges.push_back({0, 1});
    ack2.ecn = ecn_counts{3, 0, 2};  // ECN-CE increased

    auto result2 = detector_.on_ack_received(
        ack2, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_EQ(result2.ecn_signal, ecn_result::congestion_signal);

    // Handle ECN congestion
    if (result2.ecn_signal == ecn_result::congestion_signal)
    {
        cc_.on_ecn_congestion(result2.ecn_congestion_sent_time);
    }

    // cwnd should be reduced
    EXPECT_LT(cc_.cwnd(), initial_cwnd);
    EXPECT_EQ(cc_.state(), congestion_state::recovery);
}

TEST_F(FullEcnIntegrationTest, EcnValidationFailurePath)
{
    auto pkt = make_packet(0);
    detector_.on_packet_sent(pkt);
    cc_.on_packet_sent(pkt.sent_bytes);

    // ACK with ECN counts
    ack_frame ack1;
    ack1.largest_acknowledged = 0;
    ack1.ack_delay = 0;
    ack1.ranges.push_back({0, 0});
    ack1.ecn = ecn_counts{1, 0, 0};

    auto result1 = detector_.on_ack_received(
        ack1, encryption_level::application, std::chrono::steady_clock::now());

    // Send another packet
    auto pkt2 = make_packet(1);
    detector_.on_packet_sent(pkt2);
    cc_.on_packet_sent(pkt2.sent_bytes);

    // ACK with decreased ECN counts (validation failure)
    ack_frame ack2;
    ack2.largest_acknowledged = 1;
    ack2.ack_delay = 0;
    ack2.ranges.push_back({0, 0});
    ack2.ecn = ecn_counts{0, 0, 0};  // Counts decreased - invalid

    auto result2 = detector_.on_ack_received(
        ack2, encryption_level::application, std::chrono::steady_clock::now());

    EXPECT_EQ(result2.ecn_signal, ecn_result::ecn_failure);

    // ECN should be disabled
    EXPECT_TRUE(detector_.get_ecn_tracker().has_failed());
}
