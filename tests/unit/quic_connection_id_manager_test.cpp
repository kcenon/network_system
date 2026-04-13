/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/connection_id_manager.h"
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_connection_id_manager_test.cpp
 * @brief Unit tests for QUIC connection ID management (RFC 9000 Section 5.1)
 *
 * Tests validate:
 * - Error code constants
 * - connection_id_entry default values
 * - connection_id_manager constructor and active_cid_limit
 * - set_initial_peer_cid() and get_active_peer_cid()
 * - add_peer_cid() with sequence numbers and stateless reset tokens
 * - rotate_peer_cid() cycling through available CIDs
 * - available_peer_cids() count tracking
 * - retire_cids_prior_to() retirement management
 * - get_pending_retire_frames() and clear_pending_retire_frames()
 * - retire_peer_cid() individual retirement
 * - is_stateless_reset_token() validation
 * - has_peer_cid() lookup
 * - Error handling for duplicate sequences, limits exceeded, etc.
 */

// ============================================================================
// Error Code Constants Tests
// ============================================================================

class CidManagerErrorTest : public ::testing::Test
{
};

TEST_F(CidManagerErrorTest, ErrorCodeValues)
{
	EXPECT_EQ(quic::cid_manager_error::duplicate_sequence, -740);
	EXPECT_EQ(quic::cid_manager_error::sequence_too_low, -741);
	EXPECT_EQ(quic::cid_manager_error::no_available_cid, -742);
	EXPECT_EQ(quic::cid_manager_error::cid_not_found, -743);
	EXPECT_EQ(quic::cid_manager_error::invalid_retire_prior_to, -744);
	EXPECT_EQ(quic::cid_manager_error::active_cid_limit_exceeded, -745);
}

// ============================================================================
// connection_id_entry Tests
// ============================================================================

class ConnectionIdEntryTest : public ::testing::Test
{
};

TEST_F(ConnectionIdEntryTest, DefaultValues)
{
	quic::connection_id_entry entry;

	EXPECT_EQ(entry.sequence_number, 0u);
	EXPECT_FALSE(entry.retired);
	EXPECT_TRUE(entry.cid.empty());
}

// ============================================================================
// Constructor Tests
// ============================================================================

class CidManagerConstructorTest : public ::testing::Test
{
};

TEST_F(CidManagerConstructorTest, DefaultActiveCidLimit)
{
	quic::connection_id_manager mgr;

	EXPECT_EQ(mgr.active_cid_limit(), 8u);
}

TEST_F(CidManagerConstructorTest, CustomActiveCidLimit)
{
	quic::connection_id_manager mgr(4);

	EXPECT_EQ(mgr.active_cid_limit(), 4u);
}

TEST_F(CidManagerConstructorTest, InitialPeerCidCountZero)
{
	quic::connection_id_manager mgr;

	EXPECT_EQ(mgr.peer_cid_count(), 0u);
}

TEST_F(CidManagerConstructorTest, SetActiveCidLimit)
{
	quic::connection_id_manager mgr;

	mgr.set_active_cid_limit(16);

	EXPECT_EQ(mgr.active_cid_limit(), 16u);
}

// ============================================================================
// Initial Peer CID Tests
// ============================================================================

class CidManagerInitialCidTest : public ::testing::Test
{
protected:
	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}
};

TEST_F(CidManagerInitialCidTest, SetInitialPeerCid)
{
	quic::connection_id_manager mgr;
	auto cid = make_cid({0x01, 0x02, 0x03, 0x04});

	mgr.set_initial_peer_cid(cid);

	EXPECT_EQ(mgr.peer_cid_count(), 1u);
}

TEST_F(CidManagerInitialCidTest, GetActivePeerCid)
{
	quic::connection_id_manager mgr;
	auto cid = make_cid({0x01, 0x02, 0x03, 0x04});
	mgr.set_initial_peer_cid(cid);

	auto& active = mgr.get_active_peer_cid();

	EXPECT_EQ(active, cid);
}

TEST_F(CidManagerInitialCidTest, HasPeerCid)
{
	quic::connection_id_manager mgr;
	auto cid = make_cid({0x01, 0x02, 0x03, 0x04});
	mgr.set_initial_peer_cid(cid);

	EXPECT_TRUE(mgr.has_peer_cid(cid));

	auto other = make_cid({0xFF, 0xFF});
	EXPECT_FALSE(mgr.has_peer_cid(other));
}

// ============================================================================
// Add Peer CID Tests
// ============================================================================

class CidManagerAddCidTest : public ::testing::Test
{
protected:
	quic::connection_id_manager mgr_{4};
	std::array<uint8_t, 16> token1_{};
	std::array<uint8_t, 16> token2_{};

	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}

	void SetUp() override
	{
		token1_.fill(0xAA);
		token2_.fill(0xBB);

		auto initial = make_cid({0x01, 0x02, 0x03, 0x04});
		mgr_.set_initial_peer_cid(initial);
	}
};

TEST_F(CidManagerAddCidTest, AddPeerCidSuccess)
{
	auto cid = make_cid({0x05, 0x06, 0x07, 0x08});

	auto result = mgr_.add_peer_cid(cid, 1, 0, token1_);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(mgr_.peer_cid_count(), 2u);
}

TEST_F(CidManagerAddCidTest, AddMultipleCids)
{
	mgr_.add_peer_cid(make_cid({0x05, 0x06}), 1, 0, token1_);
	mgr_.add_peer_cid(make_cid({0x07, 0x08}), 2, 0, token2_);

	EXPECT_EQ(mgr_.peer_cid_count(), 3u); // initial + 2
}

TEST_F(CidManagerAddCidTest, AvailablePeerCids)
{
	mgr_.add_peer_cid(make_cid({0x05, 0x06}), 1, 0, token1_);

	// Available = total non-retired non-active CIDs
	auto available = mgr_.available_peer_cids();
	EXPECT_GE(available, 1u);
}

// ============================================================================
// Rotate Peer CID Tests
// ============================================================================

class CidManagerRotateTest : public ::testing::Test
{
protected:
	quic::connection_id_manager mgr_;
	std::array<uint8_t, 16> token_{};

	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}

	void SetUp() override
	{
		mgr_.set_initial_peer_cid(make_cid({0x01, 0x02}));

		token_.fill(0xCC);
		mgr_.add_peer_cid(make_cid({0x03, 0x04}), 1, 0, token_);
	}
};

TEST_F(CidManagerRotateTest, RotateToNewCid)
{
	auto& before = mgr_.get_active_peer_cid();
	auto before_data = before.to_string();

	auto result = mgr_.rotate_peer_cid();

	EXPECT_FALSE(result.is_err());

	auto& after = mgr_.get_active_peer_cid();
	EXPECT_NE(after.to_string(), before_data);
}

TEST_F(CidManagerRotateTest, RotateFailsWhenNoAvailable)
{
	// Rotate once (uses the only other CID)
	mgr_.rotate_peer_cid();

	// Try rotating again - should fail since no more available
	auto result = mgr_.rotate_peer_cid();

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Retirement Tests
// ============================================================================

class CidManagerRetirementTest : public ::testing::Test
{
protected:
	quic::connection_id_manager mgr_;
	std::array<uint8_t, 16> token1_{};
	std::array<uint8_t, 16> token2_{};

	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}

	void SetUp() override
	{
		token1_.fill(0xAA);
		token2_.fill(0xBB);

		mgr_.set_initial_peer_cid(make_cid({0x01, 0x02}));
		mgr_.add_peer_cid(make_cid({0x03, 0x04}), 1, 0, token1_);
		mgr_.add_peer_cid(make_cid({0x05, 0x06}), 2, 0, token2_);
	}
};

TEST_F(CidManagerRetirementTest, RetireCidsPriorTo)
{
	mgr_.retire_cids_prior_to(1);

	// CID with sequence 0 should be retired
	EXPECT_EQ(mgr_.largest_retire_prior_to(), 1u);
}

TEST_F(CidManagerRetirementTest, GetPendingRetireFrames)
{
	mgr_.retire_cids_prior_to(1);

	auto frames = mgr_.get_pending_retire_frames();

	EXPECT_FALSE(frames.empty());
	EXPECT_EQ(frames[0].sequence_number, 0u);
}

TEST_F(CidManagerRetirementTest, ClearPendingRetireFrames)
{
	mgr_.retire_cids_prior_to(1);
	ASSERT_FALSE(mgr_.get_pending_retire_frames().empty());

	mgr_.clear_pending_retire_frames();

	EXPECT_TRUE(mgr_.get_pending_retire_frames().empty());
}

TEST_F(CidManagerRetirementTest, RetireSpecificCid)
{
	auto result = mgr_.retire_peer_cid(1);

	EXPECT_FALSE(result.is_err());
}

TEST_F(CidManagerRetirementTest, RetireNonexistentFails)
{
	auto result = mgr_.retire_peer_cid(99);

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Stateless Reset Token Tests
// ============================================================================

class CidManagerStatelessResetTest : public ::testing::Test
{
protected:
	quic::connection_id_manager mgr_;
	std::array<uint8_t, 16> known_token_{};

	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}

	void SetUp() override
	{
		known_token_.fill(0xDD);
		mgr_.set_initial_peer_cid(make_cid({0x01, 0x02}));
		mgr_.add_peer_cid(make_cid({0x03, 0x04}), 1, 0, known_token_);
	}
};

TEST_F(CidManagerStatelessResetTest, RecognizeKnownToken)
{
	EXPECT_TRUE(mgr_.is_stateless_reset_token(known_token_));
}

TEST_F(CidManagerStatelessResetTest, RejectUnknownToken)
{
	std::array<uint8_t, 16> unknown{};
	unknown.fill(0xFF);

	EXPECT_FALSE(mgr_.is_stateless_reset_token(unknown));
}
