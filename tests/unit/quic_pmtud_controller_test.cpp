/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/pmtud_controller.h"
#include <gtest/gtest.h>

#include <chrono>

namespace quic = kcenon::network::protocols::quic;

using namespace std::chrono_literals;

/**
 * @file quic_pmtud_controller_test.cpp
 * @brief Unit tests for QUIC PMTUD controller (RFC 8899 DPLPMTUD)
 *
 * Tests validate:
 * - Default and custom config construction
 * - State transitions: enable(), disable(), reset()
 * - State queries: is_enabled(), is_search_complete()
 * - MTU accessors: current_mtu(), min_mtu(), max_mtu()
 * - Probing: should_probe(), probe_size()
 * - Probe acknowledgment and loss handling
 * - Packet Too Big handling
 * - Binary search convergence
 * - Black hole detection
 * - Timeout handling
 * - State string conversion
 */

// ============================================================================
// Default Construction Tests
// ============================================================================

class PmtudDefaultConstructionTest : public ::testing::Test
{
};

TEST_F(PmtudDefaultConstructionTest, StateIsDisabled)
{
    quic::pmtud_controller pmtud;
    EXPECT_EQ(pmtud.state(), quic::pmtud_state::disabled);
}

TEST_F(PmtudDefaultConstructionTest, IsNotEnabled)
{
    quic::pmtud_controller pmtud;
    EXPECT_FALSE(pmtud.is_enabled());
}

TEST_F(PmtudDefaultConstructionTest, IsNotSearchComplete)
{
    quic::pmtud_controller pmtud;
    EXPECT_FALSE(pmtud.is_search_complete());
}

TEST_F(PmtudDefaultConstructionTest, CurrentMtuIs1200)
{
    quic::pmtud_controller pmtud;
    EXPECT_EQ(pmtud.current_mtu(), 1200);
}

TEST_F(PmtudDefaultConstructionTest, MinMtuIs1200)
{
    quic::pmtud_controller pmtud;
    EXPECT_EQ(pmtud.min_mtu(), 1200);
}

TEST_F(PmtudDefaultConstructionTest, MaxMtuIs1500)
{
    quic::pmtud_controller pmtud;
    EXPECT_EQ(pmtud.max_mtu(), 1500);
}

// ============================================================================
// Custom Configuration Tests
// ============================================================================

class PmtudCustomConfigTest : public ::testing::Test
{
};

TEST_F(PmtudCustomConfigTest, CustomMinMaxMtu)
{
    quic::pmtud_config config;
    config.min_mtu = 1280;
    config.max_probe_mtu = 9000;

    quic::pmtud_controller pmtud(config);

    EXPECT_EQ(pmtud.min_mtu(), 1280);
    EXPECT_EQ(pmtud.max_mtu(), 9000);
    EXPECT_EQ(pmtud.current_mtu(), 1280);
}

TEST_F(PmtudCustomConfigTest, JumboFrameDiscovery)
{
    quic::pmtud_config config;
    config.min_mtu = 1200;
    config.max_probe_mtu = 9000;

    quic::pmtud_controller pmtud(config);
    pmtud.enable();

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

    EXPECT_GT(pmtud.current_mtu(), 8000);
}

// ============================================================================
// Enable/Disable Tests
// ============================================================================

class PmtudEnableDisableTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudEnableDisableTest, EnableTransitionsToSearching)
{
    pmtud_.enable();
    EXPECT_TRUE(pmtud_.is_enabled());
    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::searching);
}

TEST_F(PmtudEnableDisableTest, DisableTransitionsToDisabled)
{
    pmtud_.enable();
    pmtud_.disable();

    EXPECT_FALSE(pmtud_.is_enabled());
    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::disabled);
    EXPECT_EQ(pmtud_.current_mtu(), 1200);
}

TEST_F(PmtudEnableDisableTest, ResetReturnsToInitialState)
{
    pmtud_.enable();
    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());
    pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
    pmtud_.on_probe_acked(probe_size.value());

    pmtud_.reset();
    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::disabled);
    EXPECT_EQ(pmtud_.current_mtu(), 1200);
    EXPECT_FALSE(pmtud_.is_enabled());
}

// ============================================================================
// Probing Tests
// ============================================================================

class PmtudProbingTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudProbingTest, ShouldProbeReturnsFalseWhenDisabled)
{
    auto now = std::chrono::steady_clock::now();
    EXPECT_FALSE(pmtud_.should_probe(now));
}

TEST_F(PmtudProbingTest, ShouldProbeReturnsTrueWhenEnabled)
{
    auto now = std::chrono::steady_clock::now();
    pmtud_.enable();
    EXPECT_TRUE(pmtud_.should_probe(now));
}

TEST_F(PmtudProbingTest, ProbeSizeReturnsNulloptWhenDisabled)
{
    EXPECT_FALSE(pmtud_.probe_size().has_value());
}

TEST_F(PmtudProbingTest, ProbeSizeBetweenMinAndMax)
{
    pmtud_.enable();
    auto probe = pmtud_.probe_size();
    ASSERT_TRUE(probe.has_value());

    EXPECT_GT(probe.value(), pmtud_.min_mtu());
    EXPECT_LE(probe.value(), pmtud_.max_mtu());
}

// ============================================================================
// Probe Acked/Lost Tests
// ============================================================================

class PmtudProbeAckedTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudProbeAckedTest, AckedProbeIncreasesMtu)
{
    pmtud_.enable();
    auto initial_mtu = pmtud_.current_mtu();

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());

    pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
    pmtud_.on_probe_acked(probe_size.value());

    EXPECT_GE(pmtud_.current_mtu(), initial_mtu);
}

TEST_F(PmtudProbeAckedTest, BinarySearchConvergence)
{
    pmtud_.enable();

    size_t iterations = 0;
    const size_t max_iterations = 20;

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

    EXPECT_TRUE(pmtud_.is_search_complete());
    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::search_complete);
    EXPECT_GT(pmtud_.current_mtu(), pmtud_.min_mtu());
    EXPECT_LE(pmtud_.current_mtu(), pmtud_.max_mtu());
}

class PmtudProbeLostTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudProbeLostTest, MultipleLossesReduceSearchRange)
{
    pmtud_.enable();

    auto initial_probe = pmtud_.probe_size();
    ASSERT_TRUE(initial_probe.has_value());

    // max_probes defaults to 3, so after 3 failures the range should shrink
    for (size_t i = 0; i < 3; ++i)
    {
        pmtud_.on_probe_sent(initial_probe.value(), std::chrono::steady_clock::now());
        pmtud_.on_probe_lost(initial_probe.value());
    }

    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::searching);

    auto next_probe = pmtud_.probe_size();
    ASSERT_TRUE(next_probe.has_value());
    EXPECT_LT(next_probe.value(), initial_probe.value());
}

// ============================================================================
// Packet Too Big Tests
// ============================================================================

class PmtudPacketTooBigTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudPacketTooBigTest, ReducesMtu)
{
    pmtud_.enable();

    pmtud_.on_probe_sent(1400, std::chrono::steady_clock::now());
    pmtud_.on_probe_acked(1400);
    EXPECT_GE(pmtud_.current_mtu(), 1400);

    pmtud_.on_packet_too_big(1350);
    EXPECT_LE(pmtud_.current_mtu(), 1350);
}

TEST_F(PmtudPacketTooBigTest, BelowMinimumTriggersError)
{
    pmtud_.enable();

    pmtud_.on_packet_too_big(1000);

    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::error);
    EXPECT_EQ(pmtud_.current_mtu(), pmtud_.min_mtu());
}

// ============================================================================
// Black Hole Detection Tests
// ============================================================================

class PmtudBlackHoleTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudBlackHoleTest, ConsecutiveFailuresEnterError)
{
    pmtud_.enable();

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());

    for (size_t i = 0; i < 6; ++i)
    {
        pmtud_.on_probe_sent(probe_size.value(), std::chrono::steady_clock::now());
        pmtud_.on_probe_lost(probe_size.value());
    }

    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::error);
    EXPECT_EQ(pmtud_.current_mtu(), pmtud_.min_mtu());
}

// ============================================================================
// Timeout Tests
// ============================================================================

class PmtudTimeoutTest : public ::testing::Test
{
protected:
    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudTimeoutTest, TimeoutIsSetAfterProbeSent)
{
    pmtud_.enable();

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());

    auto sent_time = std::chrono::steady_clock::now();
    pmtud_.on_probe_sent(probe_size.value(), sent_time);

    auto timeout = pmtud_.next_timeout();
    ASSERT_TRUE(timeout.has_value());
    EXPECT_GT(timeout.value(), sent_time);
}

TEST_F(PmtudTimeoutTest, TimeoutTriggersLoss)
{
    pmtud_.enable();

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());

    auto sent_time = std::chrono::steady_clock::now();
    pmtud_.on_probe_sent(probe_size.value(), sent_time);

    auto timeout_time = sent_time + std::chrono::seconds{4};
    pmtud_.on_timeout(timeout_time);

    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::searching);
}

// ============================================================================
// State String Conversion Tests
// ============================================================================

class PmtudStateToStringTest : public ::testing::Test
{
};

TEST_F(PmtudStateToStringTest, AllStates)
{
    EXPECT_STREQ(quic::pmtud_state_to_string(quic::pmtud_state::disabled), "disabled");
    EXPECT_STREQ(quic::pmtud_state_to_string(quic::pmtud_state::base), "base");
    EXPECT_STREQ(quic::pmtud_state_to_string(quic::pmtud_state::searching), "searching");
    EXPECT_STREQ(quic::pmtud_state_to_string(quic::pmtud_state::search_complete),
                 "search_complete");
    EXPECT_STREQ(quic::pmtud_state_to_string(quic::pmtud_state::error), "error");
}

// ============================================================================
// Re-validation Tests
// ============================================================================

class PmtudRevalidationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
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

    quic::pmtud_controller pmtud_;
};

TEST_F(PmtudRevalidationTest, ProbeSizeEqualsCurrentMtu)
{
    ASSERT_TRUE(pmtud_.is_search_complete());

    auto probe_size = pmtud_.probe_size();
    ASSERT_TRUE(probe_size.has_value());
    EXPECT_EQ(probe_size.value(), pmtud_.current_mtu());
}

TEST_F(PmtudRevalidationTest, FailureDuringRevalidation)
{
    ASSERT_TRUE(pmtud_.is_search_complete());
    auto mtu_before = pmtud_.current_mtu();

    pmtud_.on_probe_sent(mtu_before, std::chrono::steady_clock::now());
    pmtud_.on_probe_lost(mtu_before);

    EXPECT_EQ(pmtud_.state(), quic::pmtud_state::error);
    EXPECT_EQ(pmtud_.current_mtu(), pmtud_.min_mtu());
}
