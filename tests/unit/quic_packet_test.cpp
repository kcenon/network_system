/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/packet.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_packet_test.cpp
 * @brief Unit tests for QUIC packet parsing and serialization (RFC 9000 Section 17)
 *
 * Tests validate:
 * - Version constants (v1, v2, negotiation)
 * - packet_type enum values and packet_type_to_string()
 * - long_header type() and is_retry() accessors
 * - short_header spin_bit() and key_phase() accessors
 * - packet_parser static helpers (is_long_header, has_valid_fixed_bit, get_long_packet_type)
 * - packet_number encode/decode/encoded_length round-trip
 * - packet_builder build methods (Initial, Handshake, 0-RTT, Retry, Short)
 * - Build-then-parse round-trip for all long header types
 * - Build-then-parse round-trip for short header
 * - Error handling for truncated/malformed input
 */

// ============================================================================
// Version Constants Tests
// ============================================================================

class QuicVersionConstantsTest : public ::testing::Test
{
};

TEST_F(QuicVersionConstantsTest, Version1)
{
	EXPECT_EQ(quic::quic_version::version_1, 0x00000001u);
}

TEST_F(QuicVersionConstantsTest, Version2)
{
	EXPECT_EQ(quic::quic_version::version_2, 0x6b3343cfu);
}

TEST_F(QuicVersionConstantsTest, Negotiation)
{
	EXPECT_EQ(quic::quic_version::negotiation, 0x00000000u);
}

// ============================================================================
// packet_type Enum Tests
// ============================================================================

class PacketTypeTest : public ::testing::Test
{
};

TEST_F(PacketTypeTest, EnumValues)
{
	EXPECT_EQ(static_cast<uint8_t>(quic::packet_type::initial), 0x00);
	EXPECT_EQ(static_cast<uint8_t>(quic::packet_type::zero_rtt), 0x01);
	EXPECT_EQ(static_cast<uint8_t>(quic::packet_type::handshake), 0x02);
	EXPECT_EQ(static_cast<uint8_t>(quic::packet_type::retry), 0x03);
	EXPECT_EQ(static_cast<uint8_t>(quic::packet_type::one_rtt), 0xFF);
}

TEST_F(PacketTypeTest, TypeToStringInitial)
{
	auto str = quic::packet_type_to_string(quic::packet_type::initial);

	EXPECT_FALSE(str.empty());
}

TEST_F(PacketTypeTest, TypeToStringHandshake)
{
	auto str = quic::packet_type_to_string(quic::packet_type::handshake);

	EXPECT_FALSE(str.empty());
}

TEST_F(PacketTypeTest, TypeToStringOneRtt)
{
	auto str = quic::packet_type_to_string(quic::packet_type::one_rtt);

	EXPECT_FALSE(str.empty());
}

// ============================================================================
// long_header Tests
// ============================================================================

class LongHeaderTest : public ::testing::Test
{
};

TEST_F(LongHeaderTest, TypeFromFirstByteInitial)
{
	quic::long_header header;
	// Initial type = 0x00 in bits 4-5, with long header bit 7 and fixed bit 6 set
	header.first_byte = 0xC0; // 1100_0000 -> type bits = 00 = initial

	EXPECT_EQ(header.type(), quic::packet_type::initial);
}

TEST_F(LongHeaderTest, TypeFromFirstByteHandshake)
{
	quic::long_header header;
	// Handshake type = 0x02 in bits 4-5
	header.first_byte = 0xE0; // 1110_0000 -> type bits = 10 = handshake

	EXPECT_EQ(header.type(), quic::packet_type::handshake);
}

TEST_F(LongHeaderTest, IsRetryForRetryType)
{
	quic::long_header header;
	// Retry type = 0x03 in bits 4-5
	header.first_byte = 0xF0; // 1111_0000 -> type bits = 11 = retry

	EXPECT_TRUE(header.is_retry());
}

TEST_F(LongHeaderTest, IsNotRetryForInitial)
{
	quic::long_header header;
	header.first_byte = 0xC0;

	EXPECT_FALSE(header.is_retry());
}

TEST_F(LongHeaderTest, DefaultValues)
{
	quic::long_header header;

	EXPECT_EQ(header.first_byte, 0);
	EXPECT_EQ(header.version, 0u);
	EXPECT_EQ(header.packet_number, 0u);
	EXPECT_EQ(header.packet_number_length, 0u);
	EXPECT_TRUE(header.token.empty());
}

// ============================================================================
// short_header Tests
// ============================================================================

class ShortHeaderTest : public ::testing::Test
{
};

TEST_F(ShortHeaderTest, SpinBitSet)
{
	quic::short_header header;
	header.first_byte = 0x60; // 0110_0000 -> spin bit is bit 5

	EXPECT_TRUE(header.spin_bit());
}

TEST_F(ShortHeaderTest, SpinBitNotSet)
{
	quic::short_header header;
	header.first_byte = 0x40; // 0100_0000

	EXPECT_FALSE(header.spin_bit());
}

TEST_F(ShortHeaderTest, KeyPhaseSet)
{
	quic::short_header header;
	header.first_byte = 0x44; // 0100_0100 -> key phase is bit 2

	EXPECT_TRUE(header.key_phase());
}

TEST_F(ShortHeaderTest, KeyPhaseNotSet)
{
	quic::short_header header;
	header.first_byte = 0x40; // 0100_0000

	EXPECT_FALSE(header.key_phase());
}

TEST_F(ShortHeaderTest, DefaultValues)
{
	quic::short_header header;

	EXPECT_EQ(header.first_byte, 0);
	EXPECT_EQ(header.packet_number, 0u);
	EXPECT_EQ(header.packet_number_length, 0u);
}

// ============================================================================
// packet_parser Static Methods Tests
// ============================================================================

class PacketParserStaticTest : public ::testing::Test
{
};

TEST_F(PacketParserStaticTest, IsLongHeaderWithBit7Set)
{
	EXPECT_TRUE(quic::packet_parser::is_long_header(0x80));
	EXPECT_TRUE(quic::packet_parser::is_long_header(0xFF));
	EXPECT_TRUE(quic::packet_parser::is_long_header(0xC0));
}

TEST_F(PacketParserStaticTest, IsNotLongHeaderWithBit7Clear)
{
	EXPECT_FALSE(quic::packet_parser::is_long_header(0x00));
	EXPECT_FALSE(quic::packet_parser::is_long_header(0x40));
	EXPECT_FALSE(quic::packet_parser::is_long_header(0x7F));
}

TEST_F(PacketParserStaticTest, HasValidFixedBit)
{
	EXPECT_TRUE(quic::packet_parser::has_valid_fixed_bit(0x40));
	EXPECT_TRUE(quic::packet_parser::has_valid_fixed_bit(0xC0));
}

TEST_F(PacketParserStaticTest, HasInvalidFixedBit)
{
	EXPECT_FALSE(quic::packet_parser::has_valid_fixed_bit(0x00));
	EXPECT_FALSE(quic::packet_parser::has_valid_fixed_bit(0x80));
}

TEST_F(PacketParserStaticTest, GetLongPacketTypeInitial)
{
	// Initial: type bits = 00
	EXPECT_EQ(quic::packet_parser::get_long_packet_type(0xC0), quic::packet_type::initial);
}

TEST_F(PacketParserStaticTest, GetLongPacketTypeZeroRtt)
{
	// 0-RTT: type bits = 01
	EXPECT_EQ(quic::packet_parser::get_long_packet_type(0xD0), quic::packet_type::zero_rtt);
}

TEST_F(PacketParserStaticTest, GetLongPacketTypeHandshake)
{
	// Handshake: type bits = 10
	EXPECT_EQ(quic::packet_parser::get_long_packet_type(0xE0), quic::packet_type::handshake);
}

TEST_F(PacketParserStaticTest, GetLongPacketTypeRetry)
{
	// Retry: type bits = 11
	EXPECT_EQ(quic::packet_parser::get_long_packet_type(0xF0), quic::packet_type::retry);
}

TEST_F(PacketParserStaticTest, IsLongHeaderIsConstexpr)
{
	static_assert(quic::packet_parser::is_long_header(0x80), "should be constexpr");
	static_assert(!quic::packet_parser::is_long_header(0x40), "should be constexpr");
	SUCCEED();
}

TEST_F(PacketParserStaticTest, HasValidFixedBitIsConstexpr)
{
	static_assert(quic::packet_parser::has_valid_fixed_bit(0x40), "should be constexpr");
	static_assert(!quic::packet_parser::has_valid_fixed_bit(0x00), "should be constexpr");
	SUCCEED();
}

// ============================================================================
// packet_parser Error Handling Tests
// ============================================================================

class PacketParserErrorTest : public ::testing::Test
{
};

TEST_F(PacketParserErrorTest, EmptyInputReturnsError)
{
	std::vector<uint8_t> empty;

	auto result = quic::packet_parser::parse_header(empty);

	EXPECT_TRUE(result.is_err());
}

TEST_F(PacketParserErrorTest, TruncatedLongHeaderReturnsError)
{
	// Long header needs at least 7 bytes (1 first + 4 version + 1 dcid_len + 1 scid_len)
	std::vector<uint8_t> truncated = {0xC0, 0x00, 0x00};

	auto result = quic::packet_parser::parse_header(truncated);

	EXPECT_TRUE(result.is_err());
}

TEST_F(PacketParserErrorTest, TruncatedShortHeaderReturnsError)
{
	// Short header with nonzero conn_id_length but no data
	std::vector<uint8_t> data = {0x40}; // short header form

	auto result = quic::packet_parser::parse_short_header(data, 8);

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// packet_number Encode/Decode Tests
// ============================================================================

class PacketNumberTest : public ::testing::Test
{
};

TEST_F(PacketNumberTest, EncodeSmallPacketNumber)
{
	auto [encoded, length] = quic::packet_number::encode(0, 0);

	EXPECT_GE(length, 1u);
	EXPECT_EQ(length, encoded.size());
}

TEST_F(PacketNumberTest, EncodedLengthOneByteRange)
{
	auto len = quic::packet_number::encoded_length(10, 0);

	EXPECT_EQ(len, 1u);
}

TEST_F(PacketNumberTest, EncodedLengthTwoBytesForLargerGap)
{
	auto len = quic::packet_number::encoded_length(256, 0);

	EXPECT_GE(len, 2u);
}

TEST_F(PacketNumberTest, DecodeRoundtripSmall)
{
	uint64_t full_pn = 42;
	uint64_t largest_acked = 10;

	auto [encoded, pn_length] = quic::packet_number::encode(full_pn, largest_acked);
	uint64_t truncated = 0;
	for (size_t i = 0; i < pn_length; ++i)
	{
		truncated = (truncated << 8) | encoded[i];
	}

	auto decoded = quic::packet_number::decode(truncated, pn_length, largest_acked);

	EXPECT_EQ(decoded, full_pn);
}

TEST_F(PacketNumberTest, DecodeRoundtripLargeGap)
{
	uint64_t full_pn = 100000;
	uint64_t largest_acked = 50000;

	auto [encoded, pn_length] = quic::packet_number::encode(full_pn, largest_acked);
	uint64_t truncated = 0;
	for (size_t i = 0; i < pn_length; ++i)
	{
		truncated = (truncated << 8) | encoded[i];
	}

	auto decoded = quic::packet_number::decode(truncated, pn_length, largest_acked);

	EXPECT_EQ(decoded, full_pn);
}

TEST_F(PacketNumberTest, DecodeRoundtripZero)
{
	uint64_t full_pn = 0;
	uint64_t largest_acked = 0;

	auto [encoded, pn_length] = quic::packet_number::encode(full_pn, largest_acked);
	uint64_t truncated = 0;
	for (size_t i = 0; i < pn_length; ++i)
	{
		truncated = (truncated << 8) | encoded[i];
	}

	auto decoded = quic::packet_number::decode(truncated, pn_length, largest_acked);

	EXPECT_EQ(decoded, full_pn);
}

TEST_F(PacketNumberTest, DecodeRoundtripConsecutive)
{
	for (uint64_t pn = 0; pn < 50; ++pn)
	{
		uint64_t largest = (pn > 0) ? pn - 1 : 0;
		auto [encoded, pn_length] = quic::packet_number::encode(pn, largest);
		uint64_t truncated = 0;
		for (size_t i = 0; i < pn_length; ++i)
		{
			truncated = (truncated << 8) | encoded[i];
		}
		auto decoded = quic::packet_number::decode(truncated, pn_length, largest);

		EXPECT_EQ(decoded, pn) << "Roundtrip failed for pn=" << pn;
	}
}

// ============================================================================
// packet_builder Tests
// ============================================================================

class PacketBuilderTest : public ::testing::Test
{
protected:
	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}
};

TEST_F(PacketBuilderTest, BuildInitialHasLongHeader)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});
	std::vector<uint8_t> token;

	auto header = quic::packet_builder::build_initial(dest, src, token, 0);

	ASSERT_FALSE(header.empty());
	EXPECT_TRUE(quic::packet_parser::is_long_header(header[0]));
	EXPECT_TRUE(quic::packet_parser::has_valid_fixed_bit(header[0]));
}

TEST_F(PacketBuilderTest, BuildInitialWithToken)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});
	std::vector<uint8_t> token = {0xAA, 0xBB, 0xCC};

	auto header = quic::packet_builder::build_initial(dest, src, token, 0);

	ASSERT_FALSE(header.empty());
	EXPECT_TRUE(quic::packet_parser::is_long_header(header[0]));
}

TEST_F(PacketBuilderTest, BuildHandshakeHasLongHeader)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});

	auto header = quic::packet_builder::build_handshake(dest, src, 1);

	ASSERT_FALSE(header.empty());
	EXPECT_TRUE(quic::packet_parser::is_long_header(header[0]));
}

TEST_F(PacketBuilderTest, BuildZeroRttHasLongHeader)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});

	auto header = quic::packet_builder::build_zero_rtt(dest, src, 0);

	ASSERT_FALSE(header.empty());
	EXPECT_TRUE(quic::packet_parser::is_long_header(header[0]));
}

TEST_F(PacketBuilderTest, BuildRetryHasLongHeader)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});
	std::vector<uint8_t> token = {0xDE, 0xAD};
	std::array<uint8_t, 16> tag{};

	auto header = quic::packet_builder::build_retry(dest, src, token, tag);

	ASSERT_FALSE(header.empty());
	EXPECT_TRUE(quic::packet_parser::is_long_header(header[0]));
}

TEST_F(PacketBuilderTest, BuildShortHasShortHeader)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});

	auto header = quic::packet_builder::build_short(dest, 0);

	ASSERT_FALSE(header.empty());
	EXPECT_FALSE(quic::packet_parser::is_long_header(header[0]));
	EXPECT_TRUE(quic::packet_parser::has_valid_fixed_bit(header[0]));
}

TEST_F(PacketBuilderTest, BuildShortWithKeyPhase)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});

	auto header = quic::packet_builder::build_short(dest, 0, true, false);

	ASSERT_FALSE(header.empty());
	// Key phase bit should be set (bit 2)
	EXPECT_NE(header[0] & 0x04, 0);
}

TEST_F(PacketBuilderTest, BuildShortWithSpinBit)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});

	auto header = quic::packet_builder::build_short(dest, 0, false, true);

	ASSERT_FALSE(header.empty());
	// Spin bit should be set (bit 5)
	EXPECT_NE(header[0] & 0x20, 0);
}

// ============================================================================
// Build-Parse Round-Trip Tests
// ============================================================================

class PacketBuildParseRoundtripTest : public ::testing::Test
{
protected:
	quic::connection_id make_cid(std::vector<uint8_t> bytes)
	{
		std::span<const uint8_t> span{bytes};
		return quic::connection_id{span};
	}
};

TEST_F(PacketBuildParseRoundtripTest, InitialPacketRoundtrip)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});
	std::vector<uint8_t> token;

	auto built = quic::packet_builder::build_initial(dest, src, token, 0);
	auto result = quic::packet_parser::parse_long_header(built);

	ASSERT_FALSE(result.is_err());
	auto& [header, length] = result.value();
	EXPECT_EQ(header.type(), quic::packet_type::initial);
	EXPECT_EQ(header.version, quic::quic_version::version_1);
}

TEST_F(PacketBuildParseRoundtripTest, HandshakePacketRoundtrip)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});
	auto src = make_cid({0x05, 0x06, 0x07, 0x08});

	auto built = quic::packet_builder::build_handshake(dest, src, 1);
	auto result = quic::packet_parser::parse_long_header(built);

	ASSERT_FALSE(result.is_err());
	auto& [header, length] = result.value();
	EXPECT_EQ(header.type(), quic::packet_type::handshake);
	EXPECT_EQ(header.version, quic::quic_version::version_1);
}

TEST_F(PacketBuildParseRoundtripTest, ShortPacketRoundtrip)
{
	auto dest = make_cid({0x01, 0x02, 0x03, 0x04});

	auto built = quic::packet_builder::build_short(dest, 5);
	auto result = quic::packet_parser::parse_short_header(built, 4);

	ASSERT_FALSE(result.is_err());
	auto& [header, length] = result.value();
	EXPECT_EQ(header.dest_conn_id.length(), 4u);
}

// ============================================================================
// Version Negotiation Tests
// ============================================================================

class VersionNegotiationTest : public ::testing::Test
{
};

TEST_F(VersionNegotiationTest, NonVersionNegotiationPacket)
{
	// A normal Initial packet with version_1
	std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};

	EXPECT_FALSE(quic::packet_parser::is_version_negotiation(data));
}

TEST_F(VersionNegotiationTest, TooShortForVersionCheck)
{
	std::vector<uint8_t> data = {0xC0, 0x00, 0x00};

	// Should handle gracefully (not crash)
	auto result = quic::packet_parser::is_version_negotiation(data);
	// Whether it returns true or false doesn't matter - just shouldn't crash
	(void)result;
}
