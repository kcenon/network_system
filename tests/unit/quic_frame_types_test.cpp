/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/frame_types.h"
#include "internal/protocols/quic/frame.h"
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
 * - ACK frame encoding with ranges
 * - Frame serialization/deserialization round-trips
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

TEST_F(GetFrameTypeTest, ResetStreamFrame)
{
	frame f = reset_stream_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::reset_stream);
}

TEST_F(GetFrameTypeTest, StopSendingFrame)
{
	frame f = stop_sending_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::stop_sending);
}

TEST_F(GetFrameTypeTest, NewTokenFrame)
{
	frame f = new_token_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::new_token);
}

TEST_F(GetFrameTypeTest, MaxDataFrame)
{
	frame f = max_data_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::max_data);
}

TEST_F(GetFrameTypeTest, MaxStreamDataFrame)
{
	frame f = max_stream_data_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::max_stream_data);
}

TEST_F(GetFrameTypeTest, DataBlockedFrame)
{
	frame f = data_blocked_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::data_blocked);
}

TEST_F(GetFrameTypeTest, StreamDataBlockedFrame)
{
	frame f = stream_data_blocked_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::stream_data_blocked);
}

TEST_F(GetFrameTypeTest, NewConnectionIdFrame)
{
	frame f = new_connection_id_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::new_connection_id);
}

TEST_F(GetFrameTypeTest, RetireConnectionIdFrame)
{
	frame f = retire_connection_id_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::retire_connection_id);
}

TEST_F(GetFrameTypeTest, PathChallengeFrame)
{
	frame f = path_challenge_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::path_challenge);
}

TEST_F(GetFrameTypeTest, PathResponseFrame)
{
	frame f = path_response_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::path_response);
}

TEST_F(GetFrameTypeTest, ConnectionCloseFrame)
{
	frame f = connection_close_frame{};
	EXPECT_EQ(get_frame_type(f), frame_type::connection_close);
}

TEST_F(GetFrameTypeTest, ConnectionCloseAppFrame)
{
	connection_close_frame ccf;
	ccf.is_application_error = true;
	frame f = ccf;
	EXPECT_EQ(get_frame_type(f), frame_type::connection_close_app);
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

TEST_F(FrameStructDefaultsTest, ResetStreamFrameDefaults)
{
	reset_stream_frame f;
	EXPECT_EQ(f.stream_id, 0);
	EXPECT_EQ(f.application_error_code, 0);
	EXPECT_EQ(f.final_size, 0);
}

TEST_F(FrameStructDefaultsTest, StopSendingFrameDefaults)
{
	stop_sending_frame f;
	EXPECT_EQ(f.stream_id, 0);
	EXPECT_EQ(f.application_error_code, 0);
}

TEST_F(FrameStructDefaultsTest, CryptoFrameDefaults)
{
	crypto_frame f;
	EXPECT_EQ(f.offset, 0);
	EXPECT_TRUE(f.data.empty());
}

TEST_F(FrameStructDefaultsTest, NewTokenFrameDefaults)
{
	new_token_frame f;
	EXPECT_TRUE(f.token.empty());
}

TEST_F(FrameStructDefaultsTest, MaxDataFrameDefaults)
{
	max_data_frame f;
	EXPECT_EQ(f.maximum_data, 0);
}

TEST_F(FrameStructDefaultsTest, MaxStreamDataFrameDefaults)
{
	max_stream_data_frame f;
	EXPECT_EQ(f.stream_id, 0);
	EXPECT_EQ(f.maximum_stream_data, 0);
}

TEST_F(FrameStructDefaultsTest, DataBlockedFrameDefaults)
{
	data_blocked_frame f;
	EXPECT_EQ(f.maximum_data, 0);
}

TEST_F(FrameStructDefaultsTest, StreamDataBlockedFrameDefaults)
{
	stream_data_blocked_frame f;
	EXPECT_EQ(f.stream_id, 0);
	EXPECT_EQ(f.maximum_stream_data, 0);
}

TEST_F(FrameStructDefaultsTest, StreamsBlockedFrameDefaults)
{
	streams_blocked_frame f;
	EXPECT_EQ(f.maximum_streams, 0);
	EXPECT_TRUE(f.bidirectional);
}

TEST_F(FrameStructDefaultsTest, NewConnectionIdFrameDefaults)
{
	new_connection_id_frame f;
	EXPECT_EQ(f.sequence_number, 0);
	EXPECT_EQ(f.retire_prior_to, 0);
	EXPECT_TRUE(f.connection_id.empty());
	for (auto byte : f.stateless_reset_token)
	{
		EXPECT_EQ(byte, 0);
	}
}

TEST_F(FrameStructDefaultsTest, RetireConnectionIdFrameDefaults)
{
	retire_connection_id_frame f;
	EXPECT_EQ(f.sequence_number, 0);
}

TEST_F(FrameStructDefaultsTest, PathChallengeFrameDefaults)
{
	path_challenge_frame f;
	for (auto byte : f.data)
	{
		EXPECT_EQ(byte, 0);
	}
}

TEST_F(FrameStructDefaultsTest, PathResponseFrameDefaults)
{
	path_response_frame f;
	for (auto byte : f.data)
	{
		EXPECT_EQ(byte, 0);
	}
}

TEST_F(FrameStructDefaultsTest, AckRangeDefaults)
{
	ack_range r;
	EXPECT_EQ(r.gap, 0);
	EXPECT_EQ(r.length, 0);
}

// ============================================================================
// ACK Frame Encoding Tests (RFC 9000 Section 19.3)
// ============================================================================

class AckFrameEncodingTest : public ::testing::Test
{
};

TEST_F(AckFrameEncodingTest, BasicAckRoundTrip)
{
	ack_frame original;
	original.largest_acknowledged = 42;
	original.ack_delay = 10;

	auto built = frame_builder::build_ack(original);
	ASSERT_GT(built.size(), 0);

	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ack = std::get_if<ack_frame>(&result.value().first);
	ASSERT_NE(ack, nullptr);
	EXPECT_EQ(ack->largest_acknowledged, 42);
	EXPECT_EQ(ack->ack_delay, 10);
	EXPECT_TRUE(ack->ranges.empty());
	EXPECT_FALSE(ack->ecn.has_value());
}

TEST_F(AckFrameEncodingTest, AckWithZeroDelay)
{
	ack_frame original;
	original.largest_acknowledged = 100;
	original.ack_delay = 0;

	auto built = frame_builder::build_ack(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ack = std::get_if<ack_frame>(&result.value().first);
	ASSERT_NE(ack, nullptr);
	EXPECT_EQ(ack->largest_acknowledged, 100);
	EXPECT_EQ(ack->ack_delay, 0);
}

TEST_F(AckFrameEncodingTest, AckSmallestPacketNumber)
{
	ack_frame original;
	original.largest_acknowledged = 0;
	original.ack_delay = 0;

	auto built = frame_builder::build_ack(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ack = std::get_if<ack_frame>(&result.value().first);
	ASSERT_NE(ack, nullptr);
	EXPECT_EQ(ack->largest_acknowledged, 0);
}

TEST_F(AckFrameEncodingTest, AckWithEcnCountsRoundTrip)
{
	ack_frame original;
	original.largest_acknowledged = 1000;
	original.ack_delay = 100;
	original.ecn = ecn_counts{50, 30, 10};

	auto built = frame_builder::build_ack(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ack = std::get_if<ack_frame>(&result.value().first);
	ASSERT_NE(ack, nullptr);
	ASSERT_TRUE(ack->ecn.has_value());
	EXPECT_EQ(ack->ecn->ect0, 50);
	EXPECT_EQ(ack->ecn->ect1, 30);
	EXPECT_EQ(ack->ecn->ecn_ce, 10);
}

TEST_F(AckFrameEncodingTest, AckEcnWithLargeValues)
{
	ack_frame original;
	original.largest_acknowledged = 2000;
	original.ack_delay = 50;
	original.ecn = ecn_counts{1000, 500, 25};

	auto built = frame_builder::build_ack(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ack = std::get_if<ack_frame>(&result.value().first);
	ASSERT_NE(ack, nullptr);
	EXPECT_EQ(ack->largest_acknowledged, 2000);
	EXPECT_EQ(ack->ack_delay, 50);
	ASSERT_TRUE(ack->ecn.has_value());
	EXPECT_EQ(ack->ecn->ect0, 1000);
	EXPECT_EQ(ack->ecn->ect1, 500);
	EXPECT_EQ(ack->ecn->ecn_ce, 25);
}

TEST_F(AckFrameEncodingTest, AckLargePacketNumber)
{
	ack_frame original;
	original.largest_acknowledged = 0xFFFFFFFF;
	original.ack_delay = 0;

	auto built = frame_builder::build_ack(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ack = std::get_if<ack_frame>(&result.value().first);
	ASSERT_NE(ack, nullptr);
	EXPECT_EQ(ack->largest_acknowledged, 0xFFFFFFFF);
}

// ============================================================================
// RESET_STREAM Frame Serialization Tests
// ============================================================================

class ResetStreamFrameEncodingTest : public ::testing::Test
{
};

TEST_F(ResetStreamFrameEncodingTest, BasicRoundTrip)
{
	reset_stream_frame original;
	original.stream_id = 4;
	original.application_error_code = 0x42;
	original.final_size = 1024;

	auto built = frame_builder::build_reset_stream(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* reset = std::get_if<reset_stream_frame>(&result.value().first);
	ASSERT_NE(reset, nullptr);
	EXPECT_EQ(reset->stream_id, 4);
	EXPECT_EQ(reset->application_error_code, 0x42);
	EXPECT_EQ(reset->final_size, 1024);
}

TEST_F(ResetStreamFrameEncodingTest, ZeroFinalSize)
{
	reset_stream_frame original;
	original.stream_id = 0;
	original.application_error_code = 0;
	original.final_size = 0;

	auto built = frame_builder::build_reset_stream(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* reset = std::get_if<reset_stream_frame>(&result.value().first);
	ASSERT_NE(reset, nullptr);
	EXPECT_EQ(reset->final_size, 0);
}

TEST_F(ResetStreamFrameEncodingTest, LargeFinalSize)
{
	reset_stream_frame original;
	original.stream_id = 8;
	original.application_error_code = 0xFF;
	original.final_size = 1000000;

	auto built = frame_builder::build_reset_stream(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* reset = std::get_if<reset_stream_frame>(&result.value().first);
	ASSERT_NE(reset, nullptr);
	EXPECT_EQ(reset->final_size, 1000000);
}

// ============================================================================
// STOP_SENDING Frame Serialization Tests
// ============================================================================

class StopSendingFrameEncodingTest : public ::testing::Test
{
};

TEST_F(StopSendingFrameEncodingTest, BasicRoundTrip)
{
	stop_sending_frame original;
	original.stream_id = 12;
	original.application_error_code = 0x100;

	auto built = frame_builder::build_stop_sending(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* stop = std::get_if<stop_sending_frame>(&result.value().first);
	ASSERT_NE(stop, nullptr);
	EXPECT_EQ(stop->stream_id, 12);
	EXPECT_EQ(stop->application_error_code, 0x100);
}

TEST_F(StopSendingFrameEncodingTest, ZeroErrorCode)
{
	stop_sending_frame original;
	original.stream_id = 0;
	original.application_error_code = 0;

	auto built = frame_builder::build_stop_sending(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* stop = std::get_if<stop_sending_frame>(&result.value().first);
	ASSERT_NE(stop, nullptr);
	EXPECT_EQ(stop->stream_id, 0);
	EXPECT_EQ(stop->application_error_code, 0);
}

// ============================================================================
// MAX_DATA Frame Serialization Tests
// ============================================================================

class MaxDataFrameEncodingTest : public ::testing::Test
{
};

TEST_F(MaxDataFrameEncodingTest, BasicRoundTrip)
{
	max_data_frame original;
	original.maximum_data = 1048576;

	auto built = frame_builder::build_max_data(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* md = std::get_if<max_data_frame>(&result.value().first);
	ASSERT_NE(md, nullptr);
	EXPECT_EQ(md->maximum_data, 1048576);
}

TEST_F(MaxDataFrameEncodingTest, ZeroMaxData)
{
	max_data_frame original;
	original.maximum_data = 0;

	auto built = frame_builder::build_max_data(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* md = std::get_if<max_data_frame>(&result.value().first);
	ASSERT_NE(md, nullptr);
	EXPECT_EQ(md->maximum_data, 0);
}

TEST_F(MaxDataFrameEncodingTest, LargeMaxData)
{
	max_data_frame original;
	original.maximum_data = 1000000000;

	auto built = frame_builder::build_max_data(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* md = std::get_if<max_data_frame>(&result.value().first);
	ASSERT_NE(md, nullptr);
	EXPECT_EQ(md->maximum_data, 1000000000);
}

// ============================================================================
// MAX_STREAMS Frame Serialization Tests
// ============================================================================

class MaxStreamsFrameEncodingTest : public ::testing::Test
{
};

TEST_F(MaxStreamsFrameEncodingTest, BidiRoundTrip)
{
	max_streams_frame original;
	original.maximum_streams = 100;
	original.bidirectional = true;

	auto built = frame_builder::build_max_streams(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ms = std::get_if<max_streams_frame>(&result.value().first);
	ASSERT_NE(ms, nullptr);
	EXPECT_EQ(ms->maximum_streams, 100);
	EXPECT_TRUE(ms->bidirectional);
}

TEST_F(MaxStreamsFrameEncodingTest, UniRoundTrip)
{
	max_streams_frame original;
	original.maximum_streams = 50;
	original.bidirectional = false;

	auto built = frame_builder::build_max_streams(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ms = std::get_if<max_streams_frame>(&result.value().first);
	ASSERT_NE(ms, nullptr);
	EXPECT_EQ(ms->maximum_streams, 50);
	EXPECT_FALSE(ms->bidirectional);
}

TEST_F(MaxStreamsFrameEncodingTest, ZeroStreams)
{
	max_streams_frame original;
	original.maximum_streams = 0;
	original.bidirectional = true;

	auto built = frame_builder::build_max_streams(original);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* ms = std::get_if<max_streams_frame>(&result.value().first);
	ASSERT_NE(ms, nullptr);
	EXPECT_EQ(ms->maximum_streams, 0);
}

// ============================================================================
// Generic Frame Serialization Round-Trip Tests
// ============================================================================

class FrameRoundTripTest : public ::testing::Test
{
};

TEST_F(FrameRoundTripTest, PaddingFrameViaVariant)
{
	frame f = padding_frame{5};
	auto built = frame_builder::build(f);
	EXPECT_EQ(built.size(), 5);

	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(std::get_if<padding_frame>(&result.value().first), nullptr);
}

TEST_F(FrameRoundTripTest, PingFrameViaVariant)
{
	frame f = ping_frame{};
	auto built = frame_builder::build(f);

	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(std::get_if<ping_frame>(&result.value().first), nullptr);
}

TEST_F(FrameRoundTripTest, StreamFrameWithAllFields)
{
	stream_frame sf;
	sf.stream_id = 16;
	sf.offset = 500;
	sf.data = {0x01, 0x02, 0x03, 0x04, 0x05};
	sf.fin = true;
	frame f = sf;

	auto built = frame_builder::build(f);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* parsed = std::get_if<stream_frame>(&result.value().first);
	ASSERT_NE(parsed, nullptr);
	EXPECT_EQ(parsed->stream_id, 16);
	EXPECT_EQ(parsed->offset, 500);
	EXPECT_EQ(parsed->data.size(), 5);
	EXPECT_TRUE(parsed->fin);
}

TEST_F(FrameRoundTripTest, ConnectionCloseViaVariant)
{
	connection_close_frame ccf;
	ccf.error_code = 0x0A;
	ccf.frame_type = 0x06;
	ccf.reason_phrase = "test error";
	ccf.is_application_error = false;
	frame f = ccf;

	auto built = frame_builder::build(f);
	auto result = frame_parser::parse(built);
	ASSERT_TRUE(result.is_ok());

	auto* parsed = std::get_if<connection_close_frame>(&result.value().first);
	ASSERT_NE(parsed, nullptr);
	EXPECT_EQ(parsed->error_code, 0x0A);
	EXPECT_EQ(parsed->reason_phrase, "test error");
}

TEST_F(FrameRoundTripTest, MultipleFramesParseAll)
{
	std::vector<uint8_t> buffer;

	// Build PING
	auto ping = frame_builder::build_ping();
	buffer.insert(buffer.end(), ping.begin(), ping.end());

	// Build RESET_STREAM
	reset_stream_frame rsf;
	rsf.stream_id = 4;
	rsf.application_error_code = 0x42;
	rsf.final_size = 100;
	auto reset = frame_builder::build_reset_stream(rsf);
	buffer.insert(buffer.end(), reset.begin(), reset.end());

	// Build STOP_SENDING
	stop_sending_frame ssf;
	ssf.stream_id = 8;
	ssf.application_error_code = 0x10;
	auto stop = frame_builder::build_stop_sending(ssf);
	buffer.insert(buffer.end(), stop.begin(), stop.end());

	// Build MAX_DATA
	max_data_frame mdf;
	mdf.maximum_data = 65536;
	auto max_data = frame_builder::build_max_data(mdf);
	buffer.insert(buffer.end(), max_data.begin(), max_data.end());

	// Parse all
	auto result = frame_parser::parse_all(buffer);
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().size(), 4);

	EXPECT_NE(std::get_if<ping_frame>(&result.value()[0]), nullptr);
	EXPECT_NE(std::get_if<reset_stream_frame>(&result.value()[1]), nullptr);
	EXPECT_NE(std::get_if<stop_sending_frame>(&result.value()[2]), nullptr);
	EXPECT_NE(std::get_if<max_data_frame>(&result.value()[3]), nullptr);
}
