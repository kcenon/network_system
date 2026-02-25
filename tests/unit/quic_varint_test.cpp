/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/varint.h"
#include <gtest/gtest.h>

using namespace kcenon::network::protocols::quic;

/**
 * @file quic_varint_test.cpp
 * @brief Unit tests for QUIC variable-length integer encoding (RFC 9000 Section 16)
 *
 * Tests validate:
 * - encoded_length() boundary values for all 4 ranges
 * - length_from_prefix() for all prefix patterns
 * - is_valid() range validation
 * - encode()/decode() roundtrip for all ranges
 * - encode_with_length() minimum length requirement
 * - Decode error handling (empty, truncated)
 * - Boundary value encoding/decoding
 */

// ============================================================================
// encoded_length() Tests
// ============================================================================

class VarintEncodedLengthTest : public ::testing::Test
{
};

TEST_F(VarintEncodedLengthTest, OneByteRange)
{
	EXPECT_EQ(varint::encoded_length(0), 1);
	EXPECT_EQ(varint::encoded_length(1), 1);
	EXPECT_EQ(varint::encoded_length(63), 1);
}

TEST_F(VarintEncodedLengthTest, TwoByteRange)
{
	EXPECT_EQ(varint::encoded_length(64), 2);
	EXPECT_EQ(varint::encoded_length(100), 2);
	EXPECT_EQ(varint::encoded_length(16383), 2);
}

TEST_F(VarintEncodedLengthTest, FourByteRange)
{
	EXPECT_EQ(varint::encoded_length(16384), 4);
	EXPECT_EQ(varint::encoded_length(1000000), 4);
	EXPECT_EQ(varint::encoded_length(1073741823), 4);
}

TEST_F(VarintEncodedLengthTest, EightByteRange)
{
	EXPECT_EQ(varint::encoded_length(1073741824), 8);
	EXPECT_EQ(varint::encoded_length(varint_max), 8);
}

TEST_F(VarintEncodedLengthTest, IsConstexpr)
{
	constexpr auto len = varint::encoded_length(42);

	static_assert(len == 1, "encoded_length should be constexpr");
	EXPECT_EQ(len, 1);
}

// ============================================================================
// length_from_prefix() Tests
// ============================================================================

class VarintLengthFromPrefixTest : public ::testing::Test
{
};

TEST_F(VarintLengthFromPrefixTest, PrefixZeroMeansOneByte)
{
	// 0b00xxxxxx -> 1 byte
	EXPECT_EQ(varint::length_from_prefix(0x00), 1);
	EXPECT_EQ(varint::length_from_prefix(0x3F), 1);
	EXPECT_EQ(varint::length_from_prefix(0x25), 1);
}

TEST_F(VarintLengthFromPrefixTest, PrefixOneMeansTwoBytes)
{
	// 0b01xxxxxx -> 2 bytes
	EXPECT_EQ(varint::length_from_prefix(0x40), 2);
	EXPECT_EQ(varint::length_from_prefix(0x7F), 2);
}

TEST_F(VarintLengthFromPrefixTest, PrefixTwoMeansFourBytes)
{
	// 0b10xxxxxx -> 4 bytes
	EXPECT_EQ(varint::length_from_prefix(0x80), 4);
	EXPECT_EQ(varint::length_from_prefix(0xBF), 4);
}

TEST_F(VarintLengthFromPrefixTest, PrefixThreeMeansEightBytes)
{
	// 0b11xxxxxx -> 8 bytes
	EXPECT_EQ(varint::length_from_prefix(0xC0), 8);
	EXPECT_EQ(varint::length_from_prefix(0xFF), 8);
}

TEST_F(VarintLengthFromPrefixTest, IsConstexpr)
{
	constexpr auto len = varint::length_from_prefix(0xC0);

	static_assert(len == 8, "length_from_prefix should be constexpr");
	EXPECT_EQ(len, 8);
}

// ============================================================================
// is_valid() Tests
// ============================================================================

class VarintIsValidTest : public ::testing::Test
{
};

TEST_F(VarintIsValidTest, ZeroIsValid)
{
	EXPECT_TRUE(varint::is_valid(0));
}

TEST_F(VarintIsValidTest, MaxValueIsValid)
{
	EXPECT_TRUE(varint::is_valid(varint_max));
}

TEST_F(VarintIsValidTest, BeyondMaxIsInvalid)
{
	EXPECT_FALSE(varint::is_valid(varint_max + 1));
}

TEST_F(VarintIsValidTest, Uint64MaxIsInvalid)
{
	EXPECT_FALSE(varint::is_valid(UINT64_MAX));
}

TEST_F(VarintIsValidTest, IsConstexpr)
{
	constexpr bool valid = varint::is_valid(42);

	static_assert(valid, "is_valid should be constexpr");
	EXPECT_TRUE(valid);
}

// ============================================================================
// Encode/Decode Roundtrip Tests
// ============================================================================

class VarintRoundtripTest : public ::testing::Test
{
};

TEST_F(VarintRoundtripTest, RoundtripZero)
{
	auto encoded = varint::encode(0);
	ASSERT_EQ(encoded.size(), 1);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 0);
	EXPECT_EQ(decoded.value().second, 1);
}

TEST_F(VarintRoundtripTest, RoundtripOneByteMax)
{
	auto encoded = varint::encode(63);
	ASSERT_EQ(encoded.size(), 1);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 63);
	EXPECT_EQ(decoded.value().second, 1);
}

TEST_F(VarintRoundtripTest, RoundtripTwoByteMin)
{
	auto encoded = varint::encode(64);
	ASSERT_EQ(encoded.size(), 2);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 64);
	EXPECT_EQ(decoded.value().second, 2);
}

TEST_F(VarintRoundtripTest, RoundtripTwoByteMax)
{
	auto encoded = varint::encode(16383);
	ASSERT_EQ(encoded.size(), 2);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 16383);
	EXPECT_EQ(decoded.value().second, 2);
}

TEST_F(VarintRoundtripTest, RoundtripFourByteMin)
{
	auto encoded = varint::encode(16384);
	ASSERT_EQ(encoded.size(), 4);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 16384);
	EXPECT_EQ(decoded.value().second, 4);
}

TEST_F(VarintRoundtripTest, RoundtripFourByteMax)
{
	auto encoded = varint::encode(1073741823);
	ASSERT_EQ(encoded.size(), 4);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 1073741823);
	EXPECT_EQ(decoded.value().second, 4);
}

TEST_F(VarintRoundtripTest, RoundtripEightByteMin)
{
	auto encoded = varint::encode(1073741824);
	ASSERT_EQ(encoded.size(), 8);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 1073741824);
	EXPECT_EQ(decoded.value().second, 8);
}

TEST_F(VarintRoundtripTest, RoundtripVarintMax)
{
	auto encoded = varint::encode(varint_max);
	ASSERT_EQ(encoded.size(), 8);

	auto decoded = varint::decode(encoded);
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, varint_max);
	EXPECT_EQ(decoded.value().second, 8);
}

TEST_F(VarintRoundtripTest, RoundtripArbitraryValues)
{
	std::vector<uint64_t> values = {1, 42, 255, 1000, 65535, 1000000, 100000000};

	for (auto value : values)
	{
		auto encoded = varint::encode(value);
		auto decoded = varint::decode(encoded);
		ASSERT_FALSE(decoded.is_err()) << "Failed for value: " << value;
		EXPECT_EQ(decoded.value().first, value) << "Roundtrip mismatch for: " << value;
	}
}

// ============================================================================
// Encode Tests
// ============================================================================

class VarintEncodeTest : public ::testing::Test
{
};

TEST_F(VarintEncodeTest, EncodeSetsCorrectPrefix)
{
	// 1-byte: prefix 0b00
	auto one = varint::encode(0);
	EXPECT_EQ(one[0] >> 6, 0);

	// 2-byte: prefix 0b01
	auto two = varint::encode(64);
	EXPECT_EQ(two[0] >> 6, 1);

	// 4-byte: prefix 0b10
	auto four = varint::encode(16384);
	EXPECT_EQ(four[0] >> 6, 2);

	// 8-byte: prefix 0b11
	auto eight = varint::encode(1073741824);
	EXPECT_EQ(eight[0] >> 6, 3);
}

// ============================================================================
// Decode Error Tests
// ============================================================================

class VarintDecodeErrorTest : public ::testing::Test
{
};

TEST_F(VarintDecodeErrorTest, EmptyInputReturnsError)
{
	std::vector<uint8_t> empty;

	auto result = varint::decode(empty);

	EXPECT_TRUE(result.is_err());
}

TEST_F(VarintDecodeErrorTest, TruncatedTwoByteReturnsError)
{
	// Prefix 0b01 indicates 2 bytes, but only 1 byte provided
	std::vector<uint8_t> truncated = {0x40};

	auto result = varint::decode(truncated);

	EXPECT_TRUE(result.is_err());
}

TEST_F(VarintDecodeErrorTest, TruncatedFourByteReturnsError)
{
	// Prefix 0b10 indicates 4 bytes, but only 2 bytes provided
	std::vector<uint8_t> truncated = {0x80, 0x00};

	auto result = varint::decode(truncated);

	EXPECT_TRUE(result.is_err());
}

TEST_F(VarintDecodeErrorTest, TruncatedEightByteReturnsError)
{
	// Prefix 0b11 indicates 8 bytes, but only 4 bytes provided
	std::vector<uint8_t> truncated = {0xC0, 0x00, 0x00, 0x00};

	auto result = varint::decode(truncated);

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// encode_with_length() Tests
// ============================================================================

class VarintEncodeWithLengthTest : public ::testing::Test
{
};

TEST_F(VarintEncodeWithLengthTest, MinLengthOneByte)
{
	auto result = varint::encode_with_length(42, 1);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value().size(), 1);

	auto decoded = varint::decode(result.value());
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 42);
}

TEST_F(VarintEncodeWithLengthTest, MinLengthTwoBytes)
{
	auto result = varint::encode_with_length(42, 2);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value().size(), 2);

	auto decoded = varint::decode(result.value());
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 42);
}

TEST_F(VarintEncodeWithLengthTest, MinLengthFourBytes)
{
	auto result = varint::encode_with_length(42, 4);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value().size(), 4);

	auto decoded = varint::decode(result.value());
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 42);
}

TEST_F(VarintEncodeWithLengthTest, MinLengthEightBytes)
{
	auto result = varint::encode_with_length(42, 8);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value().size(), 8);

	auto decoded = varint::decode(result.value());
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 42);
}

TEST_F(VarintEncodeWithLengthTest, InvalidMinLengthReturnsError)
{
	// Invalid min_length (not 1, 2, 4, or 8)
	auto result = varint::encode_with_length(42, 3);

	EXPECT_TRUE(result.is_err());
}

TEST_F(VarintEncodeWithLengthTest, ValueExceedsOneByteForcesLarger)
{
	// Value 100 needs 2 bytes minimum, requesting min_length=1 should still work
	// by upgrading to the necessary size
	auto result = varint::encode_with_length(100, 1);

	ASSERT_FALSE(result.is_err());
	// The value 100 needs at least 2 bytes
	EXPECT_GE(result.value().size(), 2);

	auto decoded = varint::decode(result.value());
	ASSERT_FALSE(decoded.is_err());
	EXPECT_EQ(decoded.value().first, 100);
}

// ============================================================================
// Constants Tests
// ============================================================================

class VarintConstantsTest : public ::testing::Test
{
};

TEST_F(VarintConstantsTest, MaxConstants)
{
	EXPECT_EQ(varint::max_1byte, 63);
	EXPECT_EQ(varint::max_2byte, 16383);
	EXPECT_EQ(varint::max_4byte, 1073741823);
	EXPECT_EQ(varint::max_8byte, varint_max);
}

TEST_F(VarintConstantsTest, VarintMaxIs2Power62Minus1)
{
	EXPECT_EQ(varint_max, (1ULL << 62) - 1);
}
