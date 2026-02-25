/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/detail/protocols/quic/connection_id.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <set>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_connection_id_test.cpp
 * @brief Unit tests for QUIC connection_id class (RFC 9000 Section 5.1)
 *
 * Tests validate:
 * - Default constructor creates empty connection ID
 * - Span constructor with various lengths and truncation at max_length
 * - generate() factory with default and custom lengths, clamping behavior
 * - data(), length(), empty() accessors
 * - Equality and inequality operators
 * - Less-than operator for ordered containers
 * - to_string() hex representation
 * - max_length constant
 * - Uniqueness of generated IDs
 * - Copy and move semantics
 */

// ============================================================================
// Default Constructor Tests
// ============================================================================

class ConnectionIdDefaultTest : public ::testing::Test
{
};

TEST_F(ConnectionIdDefaultTest, IsEmpty)
{
	quic::connection_id cid;

	EXPECT_TRUE(cid.empty());
}

TEST_F(ConnectionIdDefaultTest, LengthIsZero)
{
	quic::connection_id cid;

	EXPECT_EQ(cid.length(), 0);
}

TEST_F(ConnectionIdDefaultTest, DataSpanIsEmpty)
{
	quic::connection_id cid;

	EXPECT_TRUE(cid.data().empty());
}

TEST_F(ConnectionIdDefaultTest, ToStringReturnsEmpty)
{
	quic::connection_id cid;

	EXPECT_EQ(cid.to_string(), "<empty>");
}

// ============================================================================
// Span Constructor Tests
// ============================================================================

class ConnectionIdSpanConstructorTest : public ::testing::Test
{
};

TEST_F(ConnectionIdSpanConstructorTest, SingleByte)
{
	std::vector<uint8_t> bytes = {0xAB};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.length(), 1);
	EXPECT_FALSE(cid.empty());
	EXPECT_EQ(cid.data()[0], 0xAB);
}

TEST_F(ConnectionIdSpanConstructorTest, MultipleBytesPreserved)
{
	std::vector<uint8_t> bytes = {0x01, 0x02, 0x03, 0x04, 0x05};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.length(), 5);
	auto data = cid.data();
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		EXPECT_EQ(data[i], bytes[i]) << "Mismatch at index " << i;
	}
}

TEST_F(ConnectionIdSpanConstructorTest, MaxLengthExact)
{
	std::vector<uint8_t> bytes(quic::connection_id::max_length);
	std::iota(bytes.begin(), bytes.end(), 0);
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.length(), quic::connection_id::max_length);
}

TEST_F(ConnectionIdSpanConstructorTest, TruncatesToMaxLength)
{
	std::vector<uint8_t> bytes(25, 0xFF);
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.length(), quic::connection_id::max_length);
}

TEST_F(ConnectionIdSpanConstructorTest, EmptySpanCreatesEmptyId)
{
	std::vector<uint8_t> bytes;
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_TRUE(cid.empty());
	EXPECT_EQ(cid.length(), 0);
}

TEST_F(ConnectionIdSpanConstructorTest, TruncationPreservesFirstBytes)
{
	std::vector<uint8_t> bytes(30);
	std::iota(bytes.begin(), bytes.end(), 0);
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	auto data = cid.data();
	for (size_t i = 0; i < quic::connection_id::max_length; ++i)
	{
		EXPECT_EQ(data[i], static_cast<uint8_t>(i))
			<< "Mismatch at index " << i;
	}
}

// ============================================================================
// generate() Factory Tests
// ============================================================================

class ConnectionIdGenerateTest : public ::testing::Test
{
};

TEST_F(ConnectionIdGenerateTest, DefaultLengthIsEight)
{
	auto cid = quic::connection_id::generate();

	EXPECT_EQ(cid.length(), 8);
	EXPECT_FALSE(cid.empty());
}

TEST_F(ConnectionIdGenerateTest, CustomLength)
{
	auto cid = quic::connection_id::generate(16);

	EXPECT_EQ(cid.length(), 16);
}

TEST_F(ConnectionIdGenerateTest, LengthOneByte)
{
	auto cid = quic::connection_id::generate(1);

	EXPECT_EQ(cid.length(), 1);
}

TEST_F(ConnectionIdGenerateTest, MaxLength)
{
	auto cid = quic::connection_id::generate(quic::connection_id::max_length);

	EXPECT_EQ(cid.length(), quic::connection_id::max_length);
}

TEST_F(ConnectionIdGenerateTest, ZeroClampedToOne)
{
	auto cid = quic::connection_id::generate(0);

	EXPECT_EQ(cid.length(), 1);
	EXPECT_FALSE(cid.empty());
}

TEST_F(ConnectionIdGenerateTest, OverMaxClampedToMax)
{
	auto cid = quic::connection_id::generate(100);

	EXPECT_EQ(cid.length(), quic::connection_id::max_length);
}

TEST_F(ConnectionIdGenerateTest, TwoGenerationsProduceDifferentIds)
{
	auto cid1 = quic::connection_id::generate();
	auto cid2 = quic::connection_id::generate();

	EXPECT_NE(cid1, cid2);
}

TEST_F(ConnectionIdGenerateTest, MultipleGenerationsAreUnique)
{
	std::set<std::string> ids;
	for (int i = 0; i < 100; ++i)
	{
		auto cid = quic::connection_id::generate();
		ids.insert(cid.to_string());
	}

	// With 8-byte random IDs, collisions are astronomically unlikely
	EXPECT_EQ(ids.size(), 100);
}

// ============================================================================
// Equality Operator Tests
// ============================================================================

class ConnectionIdEqualityTest : public ::testing::Test
{
};

TEST_F(ConnectionIdEqualityTest, SameDataAreEqual)
{
	std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid1{span};
	quic::connection_id cid2{span};

	EXPECT_EQ(cid1, cid2);
}

TEST_F(ConnectionIdEqualityTest, DifferentDataAreNotEqual)
{
	std::vector<uint8_t> bytes1 = {0x01, 0x02, 0x03};
	std::vector<uint8_t> bytes2 = {0x01, 0x02, 0x04};
	std::span<const uint8_t> span1{bytes1};
	std::span<const uint8_t> span2{bytes2};
	quic::connection_id cid1{span1};
	quic::connection_id cid2{span2};

	EXPECT_NE(cid1, cid2);
}

TEST_F(ConnectionIdEqualityTest, DifferentLengthsAreNotEqual)
{
	std::vector<uint8_t> bytes1 = {0x01, 0x02};
	std::vector<uint8_t> bytes2 = {0x01, 0x02, 0x03};
	std::span<const uint8_t> span1{bytes1};
	std::span<const uint8_t> span2{bytes2};
	quic::connection_id cid1{span1};
	quic::connection_id cid2{span2};

	EXPECT_NE(cid1, cid2);
}

TEST_F(ConnectionIdEqualityTest, TwoEmptyIdsAreEqual)
{
	quic::connection_id cid1;
	quic::connection_id cid2;

	EXPECT_EQ(cid1, cid2);
}

TEST_F(ConnectionIdEqualityTest, EmptyNotEqualToNonEmpty)
{
	quic::connection_id empty;
	auto non_empty = quic::connection_id::generate(1);

	EXPECT_NE(empty, non_empty);
}

// ============================================================================
// Less-Than Operator Tests
// ============================================================================

class ConnectionIdLessThanTest : public ::testing::Test
{
};

TEST_F(ConnectionIdLessThanTest, ShorterIsLessThanLonger)
{
	std::vector<uint8_t> short_bytes = {0xFF, 0xFF};
	std::vector<uint8_t> long_bytes = {0x00, 0x00, 0x00};
	std::span<const uint8_t> short_span{short_bytes};
	std::span<const uint8_t> long_span{long_bytes};
	quic::connection_id shorter{short_span};
	quic::connection_id longer{long_span};

	EXPECT_TRUE(shorter < longer);
	EXPECT_FALSE(longer < shorter);
}

TEST_F(ConnectionIdLessThanTest, SameLengthLexicographic)
{
	std::vector<uint8_t> bytes1 = {0x01, 0x02, 0x03};
	std::vector<uint8_t> bytes2 = {0x01, 0x02, 0x04};
	std::span<const uint8_t> span1{bytes1};
	std::span<const uint8_t> span2{bytes2};
	quic::connection_id cid1{span1};
	quic::connection_id cid2{span2};

	EXPECT_TRUE(cid1 < cid2);
	EXPECT_FALSE(cid2 < cid1);
}

TEST_F(ConnectionIdLessThanTest, EqualNotLessThan)
{
	std::vector<uint8_t> bytes = {0x01, 0x02};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid1{span};
	quic::connection_id cid2{span};

	EXPECT_FALSE(cid1 < cid2);
	EXPECT_FALSE(cid2 < cid1);
}

TEST_F(ConnectionIdLessThanTest, EmptyIsLessThanNonEmpty)
{
	quic::connection_id empty;
	std::vector<uint8_t> bytes = {0x00};
	std::span<const uint8_t> span{bytes};
	quic::connection_id non_empty{span};

	EXPECT_TRUE(empty < non_empty);
	EXPECT_FALSE(non_empty < empty);
}

TEST_F(ConnectionIdLessThanTest, UsableInOrderedContainer)
{
	std::vector<uint8_t> b1 = {0x03};
	std::vector<uint8_t> b2 = {0x01};
	std::vector<uint8_t> b3 = {0x02};
	std::span<const uint8_t> s1{b1};
	std::span<const uint8_t> s2{b2};
	std::span<const uint8_t> s3{b3};

	std::set<quic::connection_id> id_set;
	id_set.insert(quic::connection_id{s1});
	id_set.insert(quic::connection_id{s2});
	id_set.insert(quic::connection_id{s3});

	EXPECT_EQ(id_set.size(), 3);

	auto it = id_set.begin();
	EXPECT_EQ(it->data()[0], 0x01);
	++it;
	EXPECT_EQ(it->data()[0], 0x02);
	++it;
	EXPECT_EQ(it->data()[0], 0x03);
}

// ============================================================================
// to_string() Tests
// ============================================================================

class ConnectionIdToStringTest : public ::testing::Test
{
};

TEST_F(ConnectionIdToStringTest, EmptyReturnsEmptyTag)
{
	quic::connection_id cid;

	EXPECT_EQ(cid.to_string(), "<empty>");
}

TEST_F(ConnectionIdToStringTest, SingleByteHex)
{
	std::vector<uint8_t> bytes = {0xAB};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.to_string(), "ab");
}

TEST_F(ConnectionIdToStringTest, MultiByteHex)
{
	std::vector<uint8_t> bytes = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD,
								  0xEF};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.to_string(), "0123456789abcdef");
}

TEST_F(ConnectionIdToStringTest, LeadingZerosPreserved)
{
	std::vector<uint8_t> bytes = {0x00, 0x00, 0x01};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.to_string(), "000001");
}

TEST_F(ConnectionIdToStringTest, AllZeros)
{
	std::vector<uint8_t> bytes = {0x00, 0x00, 0x00, 0x00};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.to_string(), "00000000");
}

TEST_F(ConnectionIdToStringTest, AllOnes)
{
	std::vector<uint8_t> bytes = {0xFF, 0xFF, 0xFF};
	std::span<const uint8_t> span{bytes};
	quic::connection_id cid{span};

	EXPECT_EQ(cid.to_string(), "ffffff");
}

TEST_F(ConnectionIdToStringTest, StringLengthIsTwiceByteLength)
{
	auto cid = quic::connection_id::generate(10);

	EXPECT_EQ(cid.to_string().length(), 20);
}

// ============================================================================
// max_length Constant Tests
// ============================================================================

class ConnectionIdMaxLengthTest : public ::testing::Test
{
};

TEST_F(ConnectionIdMaxLengthTest, MaxLengthIsTwenty)
{
	EXPECT_EQ(quic::connection_id::max_length, 20);
}

TEST_F(ConnectionIdMaxLengthTest, MaxLengthIsConstexpr)
{
	static_assert(quic::connection_id::max_length == 20,
				  "max_length must be 20 per RFC 9000");
	SUCCEED();
}

// ============================================================================
// Copy and Move Semantics Tests
// ============================================================================

class ConnectionIdCopyMoveTest : public ::testing::Test
{
};

TEST_F(ConnectionIdCopyMoveTest, CopyConstruction)
{
	std::vector<uint8_t> bytes = {0x01, 0x02, 0x03, 0x04};
	std::span<const uint8_t> span{bytes};
	quic::connection_id original{span};

	quic::connection_id copy(original);

	EXPECT_EQ(copy, original);
	EXPECT_EQ(copy.length(), 4);
	EXPECT_EQ(copy.to_string(), original.to_string());
}

TEST_F(ConnectionIdCopyMoveTest, CopyAssignment)
{
	std::vector<uint8_t> bytes = {0xAA, 0xBB};
	std::span<const uint8_t> span{bytes};
	quic::connection_id original{span};

	quic::connection_id copy;
	copy = original;

	EXPECT_EQ(copy, original);
	EXPECT_EQ(copy.length(), 2);
}

TEST_F(ConnectionIdCopyMoveTest, MoveConstruction)
{
	std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
	std::span<const uint8_t> span{bytes};
	quic::connection_id original{span};
	std::string original_str = original.to_string();

	quic::connection_id moved(std::move(original));

	EXPECT_EQ(moved.to_string(), original_str);
	EXPECT_EQ(moved.length(), 3);
}

TEST_F(ConnectionIdCopyMoveTest, MoveAssignment)
{
	std::vector<uint8_t> bytes = {0xDE, 0xAD, 0xBE, 0xEF};
	std::span<const uint8_t> span{bytes};
	quic::connection_id original{span};
	std::string original_str = original.to_string();

	quic::connection_id moved;
	moved = std::move(original);

	EXPECT_EQ(moved.to_string(), original_str);
	EXPECT_EQ(moved.length(), 4);
}

TEST_F(ConnectionIdCopyMoveTest, CopyIndependence)
{
	std::vector<uint8_t> bytes = {0x01, 0x02};
	std::span<const uint8_t> span{bytes};
	quic::connection_id original{span};
	quic::connection_id copy(original);

	// Verify copy is independent - modifying via new assignment
	std::vector<uint8_t> new_bytes = {0xFF};
	std::span<const uint8_t> new_span{new_bytes};
	quic::connection_id new_id{new_span};
	copy = new_id;

	EXPECT_NE(copy, original);
	EXPECT_EQ(original.length(), 2);
	EXPECT_EQ(copy.length(), 1);
}
