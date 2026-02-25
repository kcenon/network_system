/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/frame_types.h"
#include <gtest/gtest.h>

using namespace kcenon::network::protocols::quic;

/**
 * @file quic_frame_types_test.cpp
 * @brief Unit tests for QUIC frame type utilities (RFC 9000)
 *
 * Tests validate:
 * - frame_type enum values match RFC 9000 Section 12.4
 * - is_stream_frame() boundary detection
 * - get_stream_flags() flag extraction
 * - make_stream_type() flag-to-type construction
 * - stream_flags namespace constants
 * - frame_type_to_string() for all known types
 * - get_frame_type() variant visitor
 * - Frame struct default initialization
 */

// ============================================================================
// frame_type Enum Value Tests
// ============================================================================

class FrameTypeEnumTest : public ::testing::Test
{
};

TEST_F(FrameTypeEnumTest, CoreFrameTypes)
{
	EXPECT_EQ(static_cast<uint64_t>(frame_type::padding), 0x00);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::ping), 0x01);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::ack), 0x02);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::ack_ecn), 0x03);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::reset_stream), 0x04);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::stop_sending), 0x05);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::crypto), 0x06);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::new_token), 0x07);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::stream_base), 0x08);
}

TEST_F(FrameTypeEnumTest, FlowControlFrameTypes)
{
	EXPECT_EQ(static_cast<uint64_t>(frame_type::max_data), 0x10);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::max_stream_data), 0x11);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::max_streams_bidi), 0x12);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::max_streams_uni), 0x13);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::data_blocked), 0x14);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::stream_data_blocked), 0x15);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::streams_blocked_bidi), 0x16);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::streams_blocked_uni), 0x17);
}

TEST_F(FrameTypeEnumTest, ConnectionManagementFrameTypes)
{
	EXPECT_EQ(static_cast<uint64_t>(frame_type::new_connection_id), 0x18);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::retire_connection_id), 0x19);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::path_challenge), 0x1a);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::path_response), 0x1b);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::connection_close), 0x1c);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::connection_close_app), 0x1d);
	EXPECT_EQ(static_cast<uint64_t>(frame_type::handshake_done), 0x1e);
}

// ============================================================================
// is_stream_frame() Tests
// ============================================================================

class IsStreamFrameTest : public ::testing::Test
{
};

TEST_F(IsStreamFrameTest, BelowRangeIsNotStreamFrame)
{
	EXPECT_FALSE(is_stream_frame(0x07));
}

TEST_F(IsStreamFrameTest, StreamBaseIsStreamFrame)
{
	EXPECT_TRUE(is_stream_frame(0x08));
}

TEST_F(IsStreamFrameTest, AllStreamRangeIsStreamFrame)
{
	for (uint64_t type = 0x08; type <= 0x0f; ++type)
	{
		EXPECT_TRUE(is_stream_frame(type)) << "Type 0x" << std::hex << type;
	}
}

TEST_F(IsStreamFrameTest, AboveRangeIsNotStreamFrame)
{
	EXPECT_FALSE(is_stream_frame(0x10));
}

TEST_F(IsStreamFrameTest, ZeroIsNotStreamFrame)
{
	EXPECT_FALSE(is_stream_frame(0x00));
}

// ============================================================================
// get_stream_flags() Tests
// ============================================================================

class GetStreamFlagsTest : public ::testing::Test
{
};

TEST_F(GetStreamFlagsTest, BaseTypeHasNoFlags)
{
	EXPECT_EQ(get_stream_flags(0x08), 0x00);
}

TEST_F(GetStreamFlagsTest, FinFlagOnly)
{
	EXPECT_EQ(get_stream_flags(0x09), stream_flags::fin);
}

TEST_F(GetStreamFlagsTest, LenFlagOnly)
{
	EXPECT_EQ(get_stream_flags(0x0A), stream_flags::len);
}

TEST_F(GetStreamFlagsTest, OffFlagOnly)
{
	EXPECT_EQ(get_stream_flags(0x0C), stream_flags::off);
}

TEST_F(GetStreamFlagsTest, AllFlags)
{
	uint8_t flags = get_stream_flags(0x0F);

	EXPECT_TRUE(flags & stream_flags::fin);
	EXPECT_TRUE(flags & stream_flags::len);
	EXPECT_TRUE(flags & stream_flags::off);
}

TEST_F(GetStreamFlagsTest, FinAndLenFlags)
{
	uint8_t flags = get_stream_flags(0x0B);

	EXPECT_TRUE(flags & stream_flags::fin);
	EXPECT_TRUE(flags & stream_flags::len);
	EXPECT_FALSE(flags & stream_flags::off);
}

// ============================================================================
// make_stream_type() Tests
// ============================================================================

class MakeStreamTypeTest : public ::testing::Test
{
};

TEST_F(MakeStreamTypeTest, NoFlagsReturnsBase)
{
	EXPECT_EQ(make_stream_type(false, false, false), 0x08);
}

TEST_F(MakeStreamTypeTest, FinOnly)
{
	EXPECT_EQ(make_stream_type(true, false, false), 0x09);
}

TEST_F(MakeStreamTypeTest, LenOnly)
{
	EXPECT_EQ(make_stream_type(false, true, false), 0x0A);
}

TEST_F(MakeStreamTypeTest, OffOnly)
{
	EXPECT_EQ(make_stream_type(false, false, true), 0x0C);
}

TEST_F(MakeStreamTypeTest, AllFlags)
{
	EXPECT_EQ(make_stream_type(true, true, true), 0x0F);
}

TEST_F(MakeStreamTypeTest, FinAndLen)
{
	EXPECT_EQ(make_stream_type(true, true, false), 0x0B);
}

TEST_F(MakeStreamTypeTest, LenAndOff)
{
	EXPECT_EQ(make_stream_type(false, true, true), 0x0E);
}

TEST_F(MakeStreamTypeTest, ResultIsAlwaysInStreamRange)
{
	for (int fin = 0; fin <= 1; ++fin)
	{
		for (int len = 0; len <= 1; ++len)
		{
			for (int off = 0; off <= 1; ++off)
			{
				auto type = make_stream_type(fin, len, off);
				EXPECT_TRUE(is_stream_frame(type))
					<< "fin=" << fin << " len=" << len << " off=" << off;
			}
		}
	}
}

// ============================================================================
// stream_flags Constants Tests
// ============================================================================

class StreamFlagsConstantsTest : public ::testing::Test
{
};

TEST_F(StreamFlagsConstantsTest, FlagValues)
{
	EXPECT_EQ(stream_flags::fin, 0x01);
	EXPECT_EQ(stream_flags::len, 0x02);
	EXPECT_EQ(stream_flags::off, 0x04);
	EXPECT_EQ(stream_flags::mask, 0x07);
	EXPECT_EQ(stream_flags::base, 0x08);
}

TEST_F(StreamFlagsConstantsTest, MaskCoversAllFlags)
{
	EXPECT_EQ(stream_flags::mask,
			  stream_flags::fin | stream_flags::len | stream_flags::off);
}

// ============================================================================
// frame_type_to_string() Tests
// ============================================================================

class FrameTypeToStringTest : public ::testing::Test
{
};

TEST_F(FrameTypeToStringTest, CoreFrameTypeStrings)
{
	EXPECT_EQ(frame_type_to_string(frame_type::padding), "PADDING");
	EXPECT_EQ(frame_type_to_string(frame_type::ping), "PING");
	EXPECT_EQ(frame_type_to_string(frame_type::ack), "ACK");
	EXPECT_EQ(frame_type_to_string(frame_type::ack_ecn), "ACK_ECN");
	EXPECT_EQ(frame_type_to_string(frame_type::reset_stream), "RESET_STREAM");
	EXPECT_EQ(frame_type_to_string(frame_type::stop_sending), "STOP_SENDING");
	EXPECT_EQ(frame_type_to_string(frame_type::crypto), "CRYPTO");
	EXPECT_EQ(frame_type_to_string(frame_type::new_token), "NEW_TOKEN");
	EXPECT_EQ(frame_type_to_string(frame_type::stream_base), "STREAM");
}

TEST_F(FrameTypeToStringTest, FlowControlFrameTypeStrings)
{
	EXPECT_EQ(frame_type_to_string(frame_type::max_data), "MAX_DATA");
	EXPECT_EQ(frame_type_to_string(frame_type::max_stream_data), "MAX_STREAM_DATA");
	EXPECT_EQ(frame_type_to_string(frame_type::max_streams_bidi), "MAX_STREAMS_BIDI");
	EXPECT_EQ(frame_type_to_string(frame_type::max_streams_uni), "MAX_STREAMS_UNI");
	EXPECT_EQ(frame_type_to_string(frame_type::data_blocked), "DATA_BLOCKED");
	EXPECT_EQ(frame_type_to_string(frame_type::stream_data_blocked), "STREAM_DATA_BLOCKED");
	EXPECT_EQ(frame_type_to_string(frame_type::streams_blocked_bidi), "STREAMS_BLOCKED_BIDI");
	EXPECT_EQ(frame_type_to_string(frame_type::streams_blocked_uni), "STREAMS_BLOCKED_UNI");
}

TEST_F(FrameTypeToStringTest, ConnectionFrameTypeStrings)
{
	EXPECT_EQ(frame_type_to_string(frame_type::new_connection_id), "NEW_CONNECTION_ID");
	EXPECT_EQ(frame_type_to_string(frame_type::retire_connection_id), "RETIRE_CONNECTION_ID");
	EXPECT_EQ(frame_type_to_string(frame_type::path_challenge), "PATH_CHALLENGE");
	EXPECT_EQ(frame_type_to_string(frame_type::path_response), "PATH_RESPONSE");
	EXPECT_EQ(frame_type_to_string(frame_type::connection_close), "CONNECTION_CLOSE");
	EXPECT_EQ(frame_type_to_string(frame_type::connection_close_app), "CONNECTION_CLOSE_APP");
	EXPECT_EQ(frame_type_to_string(frame_type::handshake_done), "HANDSHAKE_DONE");
}

TEST_F(FrameTypeToStringTest, UnknownTypeReturnsUnknown)
{
	EXPECT_EQ(frame_type_to_string(static_cast<frame_type>(0xFF)), "UNKNOWN");
}

// ============================================================================
// get_frame_type() Variant Visitor Tests
// ============================================================================

class GetFrameTypeTest : public ::testing::Test
{
};

TEST_F(GetFrameTypeTest, PaddingFrame)
{
	frame f = padding_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::padding);
}

TEST_F(GetFrameTypeTest, PingFrame)
{
	frame f = ping_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::ping);
}

TEST_F(GetFrameTypeTest, AckFrameWithoutEcn)
{
	ack_frame af;
	af.ecn = std::nullopt;
	frame f = af;
	EXPECT_EQ(get_frame_type(f), frame_type::ack);
}

TEST_F(GetFrameTypeTest, AckFrameWithEcn)
{
	ack_frame af;
	af.ecn = ecn_counts{};
	frame f = af;
	EXPECT_EQ(get_frame_type(f), frame_type::ack_ecn);
}

TEST_F(GetFrameTypeTest, StreamFrame)
{
	frame f = stream_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::stream_base);
}

TEST_F(GetFrameTypeTest, CryptoFrame)
{
	frame f = crypto_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::crypto);
}

TEST_F(GetFrameTypeTest, MaxStreamsBidiFrame)
{
	max_streams_frame msf;
	msf.bidirectional = true;
	frame f = msf;
	EXPECT_EQ(get_frame_type(f), frame_type::max_streams_bidi);
}

TEST_F(GetFrameTypeTest, MaxStreamsUniFrame)
{
	max_streams_frame msf;
	msf.bidirectional = false;
	frame f = msf;
	EXPECT_EQ(get_frame_type(f), frame_type::max_streams_uni);
}

TEST_F(GetFrameTypeTest, StreamsBlockedBidiFrame)
{
	streams_blocked_frame sbf;
	sbf.bidirectional = true;
	frame f = sbf;
	EXPECT_EQ(get_frame_type(f), frame_type::streams_blocked_bidi);
}

TEST_F(GetFrameTypeTest, StreamsBlockedUniFrame)
{
	streams_blocked_frame sbf;
	sbf.bidirectional = false;
	frame f = sbf;
	EXPECT_EQ(get_frame_type(f), frame_type::streams_blocked_uni);
}

TEST_F(GetFrameTypeTest, ConnectionCloseFrame)
{
	frame f = connection_close_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::connection_close);
}

TEST_F(GetFrameTypeTest, HandshakeDoneFrame)
{
	frame f = handshake_done_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::handshake_done);
}

// ============================================================================
// Frame Struct Default Initialization Tests
// ============================================================================

class FrameStructDefaultsTest : public ::testing::Test
{
};

TEST_F(FrameStructDefaultsTest, PaddingFrameDefaults)
{
	padding_frame f;
	EXPECT_EQ(f.count, 1);
}

TEST_F(FrameStructDefaultsTest, AckFrameDefaults)
{
	ack_frame f;
	EXPECT_EQ(f.largest_acknowledged, 0);
	EXPECT_EQ(f.ack_delay, 0);
	EXPECT_TRUE(f.ranges.empty());
	EXPECT_FALSE(f.ecn.has_value());
}

TEST_F(FrameStructDefaultsTest, StreamFrameDefaults)
{
	stream_frame f;
	EXPECT_EQ(f.stream_id, 0);
	EXPECT_EQ(f.offset, 0);
	EXPECT_TRUE(f.data.empty());
	EXPECT_FALSE(f.fin);
}

TEST_F(FrameStructDefaultsTest, ConnectionCloseFrameDefaults)
{
	connection_close_frame f;
	EXPECT_EQ(f.error_code, 0);
	EXPECT_EQ(f.frame_type, 0);
	EXPECT_TRUE(f.reason_phrase.empty());
	EXPECT_FALSE(f.is_application_error);
}

TEST_F(FrameStructDefaultsTest, MaxStreamsFrameDefaults)
{
	max_streams_frame f;
	EXPECT_EQ(f.maximum_streams, 0);
	EXPECT_TRUE(f.bidirectional);
}

TEST_F(FrameStructDefaultsTest, EcnCountsDefaults)
{
	ecn_counts c;
	EXPECT_EQ(c.ect0, 0);
	EXPECT_EQ(c.ect1, 0);
	EXPECT_EQ(c.ecn_ce, 0);
}
