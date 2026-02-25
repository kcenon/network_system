/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/keys.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_keys_test.cpp
 * @brief Unit tests for QUIC encryption keys and encryption level utilities
 *
 * Tests validate:
 * - Cryptographic constant values (AES key sizes, IV, tag, etc.)
 * - encryption_level enum values and string conversion
 * - encryption_level_count() constexpr function
 * - quic_keys default state, is_valid(), clear(), equality operators
 * - key_pair is_valid(), clear(), and composition behavior
 * - Copy semantics for quic_keys and key_pair
 */

// ============================================================================
// Cryptographic Constants Tests
// ============================================================================

class QuicKeysConstantsTest : public ::testing::Test
{
};

TEST_F(QuicKeysConstantsTest, Aes128KeySize)
{
	EXPECT_EQ(quic::aes_128_key_size, 16);
}

TEST_F(QuicKeysConstantsTest, Aes256KeySize)
{
	EXPECT_EQ(quic::aes_256_key_size, 32);
}

TEST_F(QuicKeysConstantsTest, AeadIvSize)
{
	EXPECT_EQ(quic::aead_iv_size, 12);
}

TEST_F(QuicKeysConstantsTest, AeadTagSize)
{
	EXPECT_EQ(quic::aead_tag_size, 16);
}

TEST_F(QuicKeysConstantsTest, SecretSize)
{
	EXPECT_EQ(quic::secret_size, 32);
}

TEST_F(QuicKeysConstantsTest, HpKeySize)
{
	EXPECT_EQ(quic::hp_key_size, 16);
}

TEST_F(QuicKeysConstantsTest, HpSampleSize)
{
	EXPECT_EQ(quic::hp_sample_size, 16);
}

TEST_F(QuicKeysConstantsTest, ConstantsAreConstexpr)
{
	static_assert(quic::aes_128_key_size == 16);
	static_assert(quic::aes_256_key_size == 32);
	static_assert(quic::aead_iv_size == 12);
	static_assert(quic::aead_tag_size == 16);
	static_assert(quic::secret_size == 32);
	static_assert(quic::hp_key_size == 16);
	static_assert(quic::hp_sample_size == 16);
	SUCCEED();
}

// ============================================================================
// Encryption Level Enum Tests
// ============================================================================

class EncryptionLevelTest : public ::testing::Test
{
};

TEST_F(EncryptionLevelTest, EnumValuesAreDistinct)
{
	EXPECT_NE(static_cast<uint8_t>(quic::encryption_level::initial),
			  static_cast<uint8_t>(quic::encryption_level::handshake));
	EXPECT_NE(static_cast<uint8_t>(quic::encryption_level::handshake),
			  static_cast<uint8_t>(quic::encryption_level::zero_rtt));
	EXPECT_NE(static_cast<uint8_t>(quic::encryption_level::zero_rtt),
			  static_cast<uint8_t>(quic::encryption_level::application));
}

TEST_F(EncryptionLevelTest, EnumValuesMatchRfc9001)
{
	EXPECT_EQ(static_cast<uint8_t>(quic::encryption_level::initial), 0);
	EXPECT_EQ(static_cast<uint8_t>(quic::encryption_level::handshake), 1);
	EXPECT_EQ(static_cast<uint8_t>(quic::encryption_level::zero_rtt), 2);
	EXPECT_EQ(static_cast<uint8_t>(quic::encryption_level::application), 3);
}

TEST_F(EncryptionLevelTest, ToStringInitial)
{
	EXPECT_EQ(quic::encryption_level_to_string(quic::encryption_level::initial),
			  "Initial");
}

TEST_F(EncryptionLevelTest, ToStringHandshake)
{
	EXPECT_EQ(
		quic::encryption_level_to_string(quic::encryption_level::handshake),
		"Handshake");
}

TEST_F(EncryptionLevelTest, ToStringZeroRtt)
{
	EXPECT_EQ(
		quic::encryption_level_to_string(quic::encryption_level::zero_rtt),
		"0-RTT");
}

TEST_F(EncryptionLevelTest, ToStringApplication)
{
	EXPECT_EQ(
		quic::encryption_level_to_string(quic::encryption_level::application),
		"Application");
}

TEST_F(EncryptionLevelTest, ToStringUnknown)
{
	auto unknown = static_cast<quic::encryption_level>(255);
	EXPECT_EQ(quic::encryption_level_to_string(unknown), "Unknown");
}

TEST_F(EncryptionLevelTest, LevelCountIsFour)
{
	EXPECT_EQ(quic::encryption_level_count(), 4);
}

TEST_F(EncryptionLevelTest, LevelCountIsConstexpr)
{
	static_assert(quic::encryption_level_count() == 4,
				  "encryption_level_count must be 4");
	SUCCEED();
}

// ============================================================================
// quic_keys Default State Tests
// ============================================================================

class QuicKeysDefaultTest : public ::testing::Test
{
};

TEST_F(QuicKeysDefaultTest, SecretIsZero)
{
	quic::quic_keys keys;

	EXPECT_TRUE(
		std::all_of(keys.secret.begin(), keys.secret.end(),
					[](uint8_t b) { return b == 0; }));
}

TEST_F(QuicKeysDefaultTest, KeyIsZero)
{
	quic::quic_keys keys;

	EXPECT_TRUE(std::all_of(keys.key.begin(), keys.key.end(),
							[](uint8_t b) { return b == 0; }));
}

TEST_F(QuicKeysDefaultTest, IvIsZero)
{
	quic::quic_keys keys;

	EXPECT_TRUE(std::all_of(keys.iv.begin(), keys.iv.end(),
							[](uint8_t b) { return b == 0; }));
}

TEST_F(QuicKeysDefaultTest, HpKeyIsZero)
{
	quic::quic_keys keys;

	EXPECT_TRUE(std::all_of(keys.hp_key.begin(), keys.hp_key.end(),
							[](uint8_t b) { return b == 0; }));
}

TEST_F(QuicKeysDefaultTest, IsNotValid)
{
	quic::quic_keys keys;

	EXPECT_FALSE(keys.is_valid());
}

TEST_F(QuicKeysDefaultTest, ArraySizesMatchConstants)
{
	quic::quic_keys keys;

	EXPECT_EQ(keys.secret.size(), quic::secret_size);
	EXPECT_EQ(keys.key.size(), quic::aes_128_key_size);
	EXPECT_EQ(keys.iv.size(), quic::aead_iv_size);
	EXPECT_EQ(keys.hp_key.size(), quic::hp_key_size);
}

// ============================================================================
// quic_keys::is_valid() Tests
// ============================================================================

class QuicKeysIsValidTest : public ::testing::Test
{
};

TEST_F(QuicKeysIsValidTest, ValidWhenKeyHasNonZeroByte)
{
	quic::quic_keys keys;
	keys.key[0] = 0x42;

	EXPECT_TRUE(keys.is_valid());
}

TEST_F(QuicKeysIsValidTest, ValidWhenKeyLastByteNonZero)
{
	quic::quic_keys keys;
	keys.key[quic::aes_128_key_size - 1] = 0x01;

	EXPECT_TRUE(keys.is_valid());
}

TEST_F(QuicKeysIsValidTest, NotValidWhenOnlySecretSet)
{
	quic::quic_keys keys;
	keys.secret[0] = 0xFF;

	// is_valid() checks the key array, not secret
	EXPECT_FALSE(keys.is_valid());
}

TEST_F(QuicKeysIsValidTest, NotValidWhenOnlyIvSet)
{
	quic::quic_keys keys;
	keys.iv[0] = 0xFF;

	// is_valid() checks the key array, not iv
	EXPECT_FALSE(keys.is_valid());
}

TEST_F(QuicKeysIsValidTest, NotValidWhenOnlyHpKeySet)
{
	quic::quic_keys keys;
	keys.hp_key[0] = 0xFF;

	// is_valid() checks the key array, not hp_key
	EXPECT_FALSE(keys.is_valid());
}

// ============================================================================
// quic_keys::clear() Tests
// ============================================================================

class QuicKeysClearTest : public ::testing::Test
{
};

TEST_F(QuicKeysClearTest, ClearsAllFields)
{
	quic::quic_keys keys;
	// Fill with non-zero data
	std::fill(keys.secret.begin(), keys.secret.end(), 0xAA);
	std::fill(keys.key.begin(), keys.key.end(), 0xBB);
	std::fill(keys.iv.begin(), keys.iv.end(), 0xCC);
	std::fill(keys.hp_key.begin(), keys.hp_key.end(), 0xDD);

	keys.clear();

	EXPECT_TRUE(std::all_of(keys.secret.begin(), keys.secret.end(),
							[](uint8_t b) { return b == 0; }));
	EXPECT_TRUE(std::all_of(keys.key.begin(), keys.key.end(),
							[](uint8_t b) { return b == 0; }));
	EXPECT_TRUE(std::all_of(keys.iv.begin(), keys.iv.end(),
							[](uint8_t b) { return b == 0; }));
	EXPECT_TRUE(std::all_of(keys.hp_key.begin(), keys.hp_key.end(),
							[](uint8_t b) { return b == 0; }));
}

TEST_F(QuicKeysClearTest, IsNotValidAfterClear)
{
	quic::quic_keys keys;
	std::fill(keys.key.begin(), keys.key.end(), 0xFF);
	ASSERT_TRUE(keys.is_valid());

	keys.clear();

	EXPECT_FALSE(keys.is_valid());
}

TEST_F(QuicKeysClearTest, ClearOnAlreadyZeroIsNoOp)
{
	quic::quic_keys keys;

	keys.clear();

	EXPECT_FALSE(keys.is_valid());
}

// ============================================================================
// quic_keys Equality Operator Tests
// ============================================================================

class QuicKeysEqualityTest : public ::testing::Test
{
};

TEST_F(QuicKeysEqualityTest, DefaultKeysAreEqual)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;

	EXPECT_EQ(keys1, keys2);
}

TEST_F(QuicKeysEqualityTest, SameDataAreEqual)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;
	std::fill(keys1.key.begin(), keys1.key.end(), 0x42);
	std::fill(keys1.secret.begin(), keys1.secret.end(), 0x11);
	std::fill(keys1.iv.begin(), keys1.iv.end(), 0x22);
	std::fill(keys1.hp_key.begin(), keys1.hp_key.end(), 0x33);
	keys2 = keys1;

	EXPECT_EQ(keys1, keys2);
}

TEST_F(QuicKeysEqualityTest, DifferentSecretAreNotEqual)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;
	keys1.secret[0] = 0x01;

	EXPECT_NE(keys1, keys2);
}

TEST_F(QuicKeysEqualityTest, DifferentKeyAreNotEqual)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;
	keys1.key[0] = 0x01;

	EXPECT_NE(keys1, keys2);
}

TEST_F(QuicKeysEqualityTest, DifferentIvAreNotEqual)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;
	keys1.iv[0] = 0x01;

	EXPECT_NE(keys1, keys2);
}

TEST_F(QuicKeysEqualityTest, DifferentHpKeyAreNotEqual)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;
	keys1.hp_key[0] = 0x01;

	EXPECT_NE(keys1, keys2);
}

TEST_F(QuicKeysEqualityTest, InequalityOperator)
{
	quic::quic_keys keys1;
	quic::quic_keys keys2;
	keys1.key[0] = 0x01;

	EXPECT_TRUE(keys1 != keys2);
	EXPECT_FALSE(keys1 == keys2);
}

// ============================================================================
// quic_keys Copy Semantics Tests
// ============================================================================

class QuicKeysCopyTest : public ::testing::Test
{
};

TEST_F(QuicKeysCopyTest, CopyConstruction)
{
	quic::quic_keys original;
	std::fill(original.key.begin(), original.key.end(), 0xAB);
	std::fill(original.secret.begin(), original.secret.end(), 0xCD);

	quic::quic_keys copy(original);

	EXPECT_EQ(copy, original);
}

TEST_F(QuicKeysCopyTest, CopyAssignment)
{
	quic::quic_keys original;
	std::fill(original.key.begin(), original.key.end(), 0x55);

	quic::quic_keys copy;
	copy = original;

	EXPECT_EQ(copy, original);
}

TEST_F(QuicKeysCopyTest, CopyIndependence)
{
	quic::quic_keys original;
	std::fill(original.key.begin(), original.key.end(), 0xFF);

	quic::quic_keys copy(original);
	copy.key[0] = 0x00;

	EXPECT_NE(copy, original);
}

// ============================================================================
// key_pair Tests
// ============================================================================

class KeyPairTest : public ::testing::Test
{
};

TEST_F(KeyPairTest, DefaultIsNotValid)
{
	quic::key_pair pair;

	EXPECT_FALSE(pair.is_valid());
}

TEST_F(KeyPairTest, ValidWhenBothReadAndWriteValid)
{
	quic::key_pair pair;
	pair.read.key[0] = 0x01;
	pair.write.key[0] = 0x02;

	EXPECT_TRUE(pair.is_valid());
}

TEST_F(KeyPairTest, NotValidWhenOnlyReadValid)
{
	quic::key_pair pair;
	pair.read.key[0] = 0x01;

	EXPECT_FALSE(pair.is_valid());
}

TEST_F(KeyPairTest, NotValidWhenOnlyWriteValid)
{
	quic::key_pair pair;
	pair.write.key[0] = 0x01;

	EXPECT_FALSE(pair.is_valid());
}

TEST_F(KeyPairTest, ClearZeroesBothKeys)
{
	quic::key_pair pair;
	std::fill(pair.read.key.begin(), pair.read.key.end(), 0xAA);
	std::fill(pair.read.secret.begin(), pair.read.secret.end(), 0xBB);
	std::fill(pair.write.key.begin(), pair.write.key.end(), 0xCC);
	std::fill(pair.write.secret.begin(), pair.write.secret.end(), 0xDD);
	ASSERT_TRUE(pair.is_valid());

	pair.clear();

	EXPECT_FALSE(pair.read.is_valid());
	EXPECT_FALSE(pair.write.is_valid());
	EXPECT_FALSE(pair.is_valid());
}

TEST_F(KeyPairTest, ClearOnDefaultIsNoOp)
{
	quic::key_pair pair;

	pair.clear();

	EXPECT_FALSE(pair.is_valid());
}

TEST_F(KeyPairTest, ReadAndWriteAreIndependent)
{
	quic::key_pair pair;
	std::fill(pair.read.key.begin(), pair.read.key.end(), 0x11);
	std::fill(pair.write.key.begin(), pair.write.key.end(), 0x22);

	EXPECT_NE(pair.read, pair.write);
	EXPECT_TRUE(pair.read.is_valid());
	EXPECT_TRUE(pair.write.is_valid());
}

TEST_F(KeyPairTest, CopySemantics)
{
	quic::key_pair original;
	std::fill(original.read.key.begin(), original.read.key.end(), 0xAA);
	std::fill(original.write.key.begin(), original.write.key.end(), 0xBB);

	quic::key_pair copy(original);

	EXPECT_EQ(copy.read, original.read);
	EXPECT_EQ(copy.write, original.write);
}
