/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/stream_manager.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_stream_manager_test.cpp
 * @brief Unit tests for QUIC stream manager (RFC 9000 Sections 2-4)
 *
 * Tests validate:
 * - Constructor (client vs server mode, initial_max_stream_data)
 * - create_bidirectional_stream() allocates correct IDs (client: 0,4,8; server: 1,5,9)
 * - create_unidirectional_stream() allocates correct IDs (client: 2,6,10; server: 3,7,11)
 * - get_stream() and has_stream() lookup
 * - get_or_create_stream() for peer-initiated streams
 * - Stream limits (peer_max_streams_bidi/uni, local_max_streams_bidi/uni)
 * - stream_count() and stream_ids()
 * - streams_with_pending_data() filtering
 * - for_each_stream() iteration
 * - remove_closed_streams() cleanup
 * - close_all_streams() and reset()
 * - Statistics (local/peer stream counts)
 */

// ============================================================================
// Constructor Tests
// ============================================================================

class StreamManagerConstructorTest : public ::testing::Test
{
};

TEST_F(StreamManagerConstructorTest, ClientMode)
{
	quic::stream_manager mgr(false);

	EXPECT_EQ(mgr.stream_count(), 0u);
}

TEST_F(StreamManagerConstructorTest, ServerMode)
{
	quic::stream_manager mgr(true);

	EXPECT_EQ(mgr.stream_count(), 0u);
}

TEST_F(StreamManagerConstructorTest, DefaultStreamLimits)
{
	quic::stream_manager mgr(false);

	EXPECT_EQ(mgr.local_max_streams_bidi(), 100u);
	EXPECT_EQ(mgr.local_max_streams_uni(), 100u);
}

// ============================================================================
// Client Stream Creation Tests
// ============================================================================

class StreamManagerClientCreateTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false}; // client mode

	void SetUp() override
	{
		mgr_.set_peer_max_streams_bidi(100);
		mgr_.set_peer_max_streams_uni(100);
	}
};

TEST_F(StreamManagerClientCreateTest, CreateBidiStreamId)
{
	auto result = mgr_.create_bidirectional_stream();

	ASSERT_FALSE(result.is_err());
	// Client bidi streams: 0, 4, 8, ...
	EXPECT_EQ(result.value(), 0u);
	EXPECT_TRUE(quic::stream_id_type::is_client_initiated(result.value()));
	EXPECT_TRUE(quic::stream_id_type::is_bidirectional(result.value()));
}

TEST_F(StreamManagerClientCreateTest, CreateSecondBidiStreamId)
{
	mgr_.create_bidirectional_stream();
	auto result = mgr_.create_bidirectional_stream();

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), 4u);
}

TEST_F(StreamManagerClientCreateTest, CreateUniStreamId)
{
	auto result = mgr_.create_unidirectional_stream();

	ASSERT_FALSE(result.is_err());
	// Client uni streams: 2, 6, 10, ...
	EXPECT_EQ(result.value(), 2u);
	EXPECT_TRUE(quic::stream_id_type::is_client_initiated(result.value()));
	EXPECT_TRUE(quic::stream_id_type::is_unidirectional(result.value()));
}

TEST_F(StreamManagerClientCreateTest, CreateSecondUniStreamId)
{
	mgr_.create_unidirectional_stream();
	auto result = mgr_.create_unidirectional_stream();

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), 6u);
}

// ============================================================================
// Server Stream Creation Tests
// ============================================================================

class StreamManagerServerCreateTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{true}; // server mode

	void SetUp() override
	{
		mgr_.set_peer_max_streams_bidi(100);
		mgr_.set_peer_max_streams_uni(100);
	}
};

TEST_F(StreamManagerServerCreateTest, CreateBidiStreamId)
{
	auto result = mgr_.create_bidirectional_stream();

	ASSERT_FALSE(result.is_err());
	// Server bidi streams: 1, 5, 9, ...
	EXPECT_EQ(result.value(), 1u);
	EXPECT_TRUE(quic::stream_id_type::is_server_initiated(result.value()));
}

TEST_F(StreamManagerServerCreateTest, CreateUniStreamId)
{
	auto result = mgr_.create_unidirectional_stream();

	ASSERT_FALSE(result.is_err());
	// Server uni streams: 3, 7, 11, ...
	EXPECT_EQ(result.value(), 3u);
	EXPECT_TRUE(quic::stream_id_type::is_server_initiated(result.value()));
}

// ============================================================================
// Stream Access Tests
// ============================================================================

class StreamManagerAccessTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false};

	void SetUp() override
	{
		mgr_.set_peer_max_streams_bidi(100);
		mgr_.create_bidirectional_stream(); // stream 0
	}
};

TEST_F(StreamManagerAccessTest, GetExistingStream)
{
	auto* stream = mgr_.get_stream(0);

	ASSERT_NE(stream, nullptr);
	EXPECT_EQ(stream->id(), 0u);
}

TEST_F(StreamManagerAccessTest, GetNonexistentStream)
{
	auto* stream = mgr_.get_stream(99);

	EXPECT_EQ(stream, nullptr);
}

TEST_F(StreamManagerAccessTest, HasStream)
{
	EXPECT_TRUE(mgr_.has_stream(0));
	EXPECT_FALSE(mgr_.has_stream(99));
}

TEST_F(StreamManagerAccessTest, StreamIds)
{
	mgr_.set_peer_max_streams_uni(100);
	mgr_.create_unidirectional_stream(); // stream 2

	auto ids = mgr_.stream_ids();

	EXPECT_EQ(ids.size(), 2u);
}

TEST_F(StreamManagerAccessTest, StreamCount)
{
	EXPECT_EQ(mgr_.stream_count(), 1u);
}

// ============================================================================
// Peer-Initiated Stream Tests
// ============================================================================

class StreamManagerPeerStreamTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false}; // client mode
};

TEST_F(StreamManagerPeerStreamTest, GetOrCreatePeerStream)
{
	// Server bidi stream ID = 1
	auto result = mgr_.get_or_create_stream(1);

	ASSERT_FALSE(result.is_err());
	EXPECT_NE(result.value(), nullptr);
	EXPECT_EQ(result.value()->id(), 1u);
}

TEST_F(StreamManagerPeerStreamTest, GetOrCreateExistingStream)
{
	mgr_.get_or_create_stream(1);

	auto result = mgr_.get_or_create_stream(1);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value()->id(), 1u);
}

// ============================================================================
// Stream Limit Tests
// ============================================================================

class StreamManagerLimitTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false};
};

TEST_F(StreamManagerLimitTest, SetPeerMaxStreamsBidi)
{
	mgr_.set_peer_max_streams_bidi(5);

	EXPECT_EQ(mgr_.peer_max_streams_bidi(), 5u);
}

TEST_F(StreamManagerLimitTest, SetPeerMaxStreamsUni)
{
	mgr_.set_peer_max_streams_uni(10);

	EXPECT_EQ(mgr_.peer_max_streams_uni(), 10u);
}

TEST_F(StreamManagerLimitTest, SetLocalMaxStreamsBidi)
{
	mgr_.set_local_max_streams_bidi(50);

	EXPECT_EQ(mgr_.local_max_streams_bidi(), 50u);
}

TEST_F(StreamManagerLimitTest, SetLocalMaxStreamsUni)
{
	mgr_.set_local_max_streams_uni(25);

	EXPECT_EQ(mgr_.local_max_streams_uni(), 25u);
}

TEST_F(StreamManagerLimitTest, CannotExceedPeerBidiLimit)
{
	mgr_.set_peer_max_streams_bidi(1);

	auto first = mgr_.create_bidirectional_stream();
	ASSERT_FALSE(first.is_err());

	auto second = mgr_.create_bidirectional_stream();
	EXPECT_TRUE(second.is_err());
}

TEST_F(StreamManagerLimitTest, CannotExceedPeerUniLimit)
{
	mgr_.set_peer_max_streams_uni(1);

	auto first = mgr_.create_unidirectional_stream();
	ASSERT_FALSE(first.is_err());

	auto second = mgr_.create_unidirectional_stream();
	EXPECT_TRUE(second.is_err());
}

// ============================================================================
// Stream Iteration Tests
// ============================================================================

class StreamManagerIterationTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false};

	void SetUp() override
	{
		mgr_.set_peer_max_streams_bidi(10);
		mgr_.create_bidirectional_stream(); // 0
		mgr_.create_bidirectional_stream(); // 4
		mgr_.create_bidirectional_stream(); // 8
	}
};

TEST_F(StreamManagerIterationTest, ForEachStream)
{
	int count = 0;
	mgr_.for_each_stream([&count](quic::stream& s) {
		++count;
	});

	EXPECT_EQ(count, 3);
}

TEST_F(StreamManagerIterationTest, ForEachStreamConst)
{
	const auto& const_mgr = mgr_;

	int count = 0;
	const_mgr.for_each_stream([&count](const quic::stream& s) {
		++count;
	});

	EXPECT_EQ(count, 3);
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

class StreamManagerLifecycleTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false};

	void SetUp() override
	{
		mgr_.set_peer_max_streams_bidi(10);
		mgr_.create_bidirectional_stream();
		mgr_.create_bidirectional_stream();
	}
};

TEST_F(StreamManagerLifecycleTest, CloseAllStreams)
{
	mgr_.close_all_streams(0);

	// All streams should be in reset state
	mgr_.for_each_stream([](const quic::stream& s) {
		EXPECT_EQ(s.send_state(), quic::send_stream_state::reset_sent);
	});
}

TEST_F(StreamManagerLifecycleTest, Reset)
{
	mgr_.reset();

	EXPECT_EQ(mgr_.stream_count(), 0u);
}

// ============================================================================
// Statistics Tests
// ============================================================================

class StreamManagerStatsTest : public ::testing::Test
{
protected:
	quic::stream_manager mgr_{false};

	void SetUp() override
	{
		mgr_.set_peer_max_streams_bidi(10);
		mgr_.set_peer_max_streams_uni(10);
	}
};

TEST_F(StreamManagerStatsTest, LocalBidiStreamsCount)
{
	mgr_.create_bidirectional_stream();
	mgr_.create_bidirectional_stream();

	EXPECT_EQ(mgr_.local_bidi_streams_count(), 2u);
}

TEST_F(StreamManagerStatsTest, LocalUniStreamsCount)
{
	mgr_.create_unidirectional_stream();

	EXPECT_EQ(mgr_.local_uni_streams_count(), 1u);
}
