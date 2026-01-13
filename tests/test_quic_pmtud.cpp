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

#include "kcenon/network/protocols/quic/pmtud_controller.h"

#include <chrono>

using namespace kcenon::network::protocols::quic;
using namespace std::chrono_literals;

// ============================================================================
// PMTUD Controller Tests
// ============================================================================

class PmtudControllerTest : public ::testing::Test
{
protected:
    pmtud_controller pmtud_;
};

TEST_F(PmtudControllerTest, InitialState)
{
    // Should be disabled by default
    EXPECT_EQ(pmtud_.state(), pmtud_state::disabled);
    EXPECT_FALSE(pmtud_.is_enabled());
    EXPECT_FALSE(pmtud_.is_search_complete());

    // Current MTU should be minimum (1200)
    EXPECT_EQ(pmtud_.current_mtu(), 1200);
    EXPECT_EQ(pmtud_.min_mtu(), 1200);
    EXPECT_EQ(pmtud_.max_mtu(), 1500);
}

TEST_F(PmtudControllerTest, EnableDisable)
{
    // Enable PMTUD
    pmtud_.enable();
    EXPECT_TRUE(pmtud_.is_enabled());
    EXPECT_EQ(pmtud_.state(), pmtud_state::searching);

    // Disable PMTUD
    pmtud_.disable();
    EXPECT_FALSE(pmtud_.is_enabled());
    EXPECT_EQ(pmtud_.state(), pmtud_state::disabled);
    EXPECT_EQ(pmtud_.current_mtu(), 1200);
}

TEST_F(PmtudControllerTest, Reset)
{
    // Enable and make some progress
    pmtud_.enable();
    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());
    pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
    pmtud_.on_probe_acked(probe_size.value());

    // Reset should return to initial state
    pmtud_.reset();
    EXPECT_EQ(pmtud_.state(), pmtud_state::disabled);
    EXPECT_EQ(pmtud_.current_mtu(), 1200);
    EXPECT_FALSE(pmtud_.is_enabled());
}

TEST_F(PmtudControllerTest, ShouldProbeWhenEnabled)
{
    auto now = std::chrono::steady_clock::now();

    // Should not probe when disabled
    EXPECT_FALSE(pmtud_.should_probe(now));

    // Enable and check
    pmtud_.enable();
    EXPECT_TRUE(pmtud_.should_probe(now));
}

TEST_F(PmtudControllerTest, ProbeSize)
{
    // No probe when disabled
    EXPECT_FALSE(pmtud_.probe_size().has_value());

    // Enable and get probe size
    pmtud_.enable();
    auto probe = pmtud_.probe_size();
    ASSERT_TRUE(probe.has_value());

    // Probe size should be between min and max
    EXPECT_GT(probe.value(), pmtud_.min_mtu());
    EXPECT_LE(probe.value(), pmtud_.max_mtu());
}

TEST_F(PmtudControllerTest, BinarySearchConvergence)
{
    pmtud_.enable();

    // Simulate successful probes until search completes
    size_t iterations = 0;
    const size_t max_iterations = 20; // Safety limit

    while (!pmtud_.is_search_complete() && iterations < max_iterations)
    {
        auto probe_size = pmtud_.probe_size();
        if (!probe_size.has_value())
        {
            break;
        }

        pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
        pmtud_.on_probe_acked(probe_size.value());
        ++iterations;
    }

    // Should have completed search
    EXPECT_TRUE(pmtud_.is_search_complete());
    EXPECT_EQ(pmtud_.state(), pmtud_state::search_complete);

    // Current MTU should be close to max
    EXPECT_GT(pmtud_.current_mtu(), pmtud_.min_mtu());
    EXPECT_LE(pmtud_.current_mtu(), pmtud_.max_mtu());
}

TEST_F(PmtudControllerTest, ProbeLossReducesSearchRange)
{
    pmtud_.enable();

    auto initial_probe = pmtud_.probe_size();
    ASSERT_TRUE(initial_probe.has_value());

    // Multiple losses at same size should reduce search range
    // max_probes defaults to 3, so after 3 failures, range should shrink
    for (size_t i = 0; i < 3; ++i)
    {
        pmtud_.on_probe_sent(initial_probe.value(), std::chrono::steady_clock::now());
        pmtud_.on_probe_lost(initial_probe.value());
    }

    // Should still be searching
    EXPECT_EQ(pmtud_.state(), pmtud_state::searching);

    // Next probe should be smaller (search_high was reduced)
    auto next_probe = pmtud_.probe_size();
    ASSERT_TRUE(next_probe.has_value());
    EXPECT_LT(next_probe.value(), initial_probe.value());
}

TEST_F(PmtudControllerTest, PacketTooBigHandling)
{
    pmtud_.enable();

    // Simulate successful discovery to 1400
    pmtud_.on_probe_sent(1400, std::chrono::steady_clock::now());
    pmtud_.on_probe_acked(1400);

    EXPECT_GE(pmtud_.current_mtu(), 1400);

    // Receive PTB with smaller MTU
    pmtud_.on_packet_too_big(1350);

    // MTU should be reduced
    EXPECT_LE(pmtud_.current_mtu(), 1350);
}

TEST_F(PmtudControllerTest, PacketTooBigBelowMinimum)
{
    pmtud_.enable();

    // PTB below minimum should trigger error state
    pmtud_.on_packet_too_big(1000);

    EXPECT_EQ(pmtud_.state(), pmtud_state::error);
    EXPECT_EQ(pmtud_.current_mtu(), pmtud_.min_mtu());
}

TEST_F(PmtudControllerTest, BlackHoleDetection)
{
    pmtud_.enable();

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());

    // Simulate consecutive failures to trigger black hole detection
    for (size_t i = 0; i < 6; ++i)
    {
        pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
        pmtud_.on_probe_lost(probe_size.value());
    }

    // Should be in error state with minimum MTU
    EXPECT_EQ(pmtud_.state(), pmtud_state::error);
    EXPECT_EQ(pmtud_.current_mtu(), pmtud_.min_mtu());
}

TEST_F(PmtudControllerTest, TimeoutHandling)
{
    pmtud_.enable();

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());

    auto sent_time = std::chrono::steady_clock::now();
    pmtud_.on_probe_sent(probe_size.value(), sent_time);

    // Timeout should be set
    auto timeout = pmtud_.next_timeout();
    ASSERT_TRUE(timeout.has_value());

    // Timeout should be after sent time
    EXPECT_GT(timeout.value(), sent_time);

    // Simulate timeout
    auto timeout_time = sent_time + std::chrono::seconds{4};
    pmtud_.on_timeout(timeout_time);

    // Probe should be treated as lost
    EXPECT_EQ(pmtud_.state(), pmtud_state::searching);
}

// ============================================================================
// PMTUD Configuration Tests
// ============================================================================

class PmtudConfigTest : public ::testing::Test
{
};

TEST_F(PmtudConfigTest, CustomConfiguration)
{
    pmtud_config config;
    config.min_mtu = 1280;
    config.max_probe_mtu = 9000; // Jumbo frames
    config.probe_step = 64;
    config.max_probes = 5;

    pmtud_controller pmtud(config);

    EXPECT_EQ(pmtud.min_mtu(), 1280);
    EXPECT_EQ(pmtud.max_mtu(), 9000);
    EXPECT_EQ(pmtud.current_mtu(), 1280);
}

TEST_F(PmtudConfigTest, JumboFrameDiscovery)
{
    pmtud_config config;
    config.min_mtu = 1200;
    config.max_probe_mtu = 9000;

    pmtud_controller pmtud(config);
    pmtud.enable();

    // Run discovery to completion
    size_t iterations = 0;
    while (!pmtud.is_search_complete() && iterations < 50)
    {
        auto probe_size = pmtud.probe_size();
        if (!probe_size.has_value())
        {
            break;
        }

        pmtud.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
        pmtud.on_probe_acked(probe_size.value());
        ++iterations;
    }

    // Should reach near maximum
    EXPECT_GT(pmtud.current_mtu(), 8000);
}

// ============================================================================
// PMTUD State String Conversion Tests
// ============================================================================

TEST(PmtudStateToStringTest, AllStates)
{
    EXPECT_STREQ(pmtud_state_to_string(pmtud_state::disabled), "disabled");
    EXPECT_STREQ(pmtud_state_to_string(pmtud_state::base), "base");
    EXPECT_STREQ(pmtud_state_to_string(pmtud_state::searching), "searching");
    EXPECT_STREQ(pmtud_state_to_string(pmtud_state::search_complete), "search_complete");
    EXPECT_STREQ(pmtud_state_to_string(pmtud_state::error), "error");
}

// ============================================================================
// PMTUD Re-validation Tests
// ============================================================================

class PmtudRevalidationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Run search to completion
        pmtud_.enable();
        while (!pmtud_.is_search_complete())
        {
            auto probe_size = pmtud_.probe_size();
            if (!probe_size.has_value())
            {
                break;
            }
            pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
            pmtud_.on_probe_acked(probe_size.value());
        }
    }

    pmtud_controller pmtud_;
};

TEST_F(PmtudRevalidationTest, RevalidationAfterSearchComplete)
{
    ASSERT_TRUE(pmtud_.is_search_complete());

    // Should have probe size for re-validation
    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());
    EXPECT_EQ(probe_size.value(), pmtud_.current_mtu());
}

TEST_F(PmtudRevalidationTest, RevalidationFailure)
{
    ASSERT_TRUE(pmtud_.is_search_complete());
    auto mtu_before = pmtud_.current_mtu();

    // Simulate re-validation failure
    pmtud_.on_probe_sent(mtu_before, std::chrono::steady_clock::now());
    pmtud_.on_probe_lost(mtu_before);

    // Should enter error state and reduce MTU
    EXPECT_EQ(pmtud_.state(), pmtud_state::error);
    EXPECT_EQ(pmtud_.current_mtu(), pmtud_.min_mtu());
}
