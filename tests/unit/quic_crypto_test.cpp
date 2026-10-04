/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/crypto.h"
#include <gtest/gtest.h>

#include <algorithm>

#include "kcenon/network/detail/protocols/quic/connection_id.h"
#include "internal/protocols/quic/keys.h"

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_crypto_test.cpp
 * @brief Unit tests for QUIC crypto handler, HKDF, initial keys, and packet protection
 *
 * Tests validate:
 * - quic_crypto default construction and initial state
 * - is_handshake_complete(), current_level(), key_phase()
 * - init_client(), init_server() behavior
 * - HKDF extract/expand with known inputs
 * - initial_keys::derive with known connection ID
 * - Packet protection round-trip
 * - Header protection round-trip
 * - Move semantics for quic_crypto
 * - Key setting and retrieval
 */

// ============================================================================
// quic_crypto Default State Tests
// ============================================================================

class QuicCryptoDefaultTest : public ::testing::Test
{
};

TEST_F(QuicCryptoDefaultTest, HandshakeNotComplete)
{
    quic::quic_crypto crypto;
    EXPECT_FALSE(crypto.is_handshake_complete());
}

TEST_F(QuicCryptoDefaultTest, CurrentLevelIsInitial)
{
    quic::quic_crypto crypto;
    EXPECT_EQ(crypto.current_level(), quic::encryption_level::initial);
}

TEST_F(QuicCryptoDefaultTest, KeyPhaseIsZero)
{
    quic::quic_crypto crypto;
    EXPECT_EQ(crypto.key_phase(), 0);
}

TEST_F(QuicCryptoDefaultTest, EarlyDataNotAccepted)
{
    quic::quic_crypto crypto;
    EXPECT_FALSE(crypto.is_early_data_accepted());
}

TEST_F(QuicCryptoDefaultTest, NoZeroRttKeys)
{
    quic::quic_crypto crypto;
    EXPECT_FALSE(crypto.has_zero_rtt_keys());
}

TEST_F(QuicCryptoDefaultTest, GetAlpnReturnsEmpty)
{
    quic::quic_crypto crypto;
    EXPECT_TRUE(crypto.get_alpn().empty());
}

TEST_F(QuicCryptoDefaultTest, MissingReadKeysReturnError)
{
    quic::quic_crypto crypto;
    auto result = crypto.get_read_keys(quic::encryption_level::application);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicCryptoDefaultTest, MissingWriteKeysReturnError)
{
    quic::quic_crypto crypto;
    auto result = crypto.get_write_keys(quic::encryption_level::application);
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// quic_crypto Init Tests
// ============================================================================

class QuicCryptoInitTest : public ::testing::Test
{
};

TEST_F(QuicCryptoInitTest, InitClientSucceeds)
{
    quic::quic_crypto crypto;
    auto result = crypto.init_client("localhost");
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(crypto.is_server());
}

TEST_F(QuicCryptoInitTest, InitServerFailsWithoutValidCerts)
{
    quic::quic_crypto crypto;
    auto result = crypto.init_server("nonexistent_cert.pem", "nonexistent_key.pem");
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// HKDF Tests
// ============================================================================

class HkdfTest : public ::testing::Test
{
};

TEST_F(HkdfTest, ExtractProducesNonZeroResult)
{
    // Use QUIC v1 initial salt
    std::array<uint8_t, 20> salt = {
        0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34, 0xb3, 0x4d, 0x17,
        0x9a, 0xe6, 0xa4, 0xc8, 0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a};
    std::vector<uint8_t> ikm = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    auto result = quic::hkdf::extract(salt, ikm);
    ASSERT_TRUE(result.is_ok());

    auto& prk = result.value();
    EXPECT_EQ(prk.size(), quic::secret_size);

    bool all_zero = std::all_of(prk.begin(), prk.end(),
                                [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(all_zero);
}

TEST_F(HkdfTest, ExpandProducesCorrectLength)
{
    std::array<uint8_t, 32> prk;
    prk.fill(0xAB);
    std::vector<uint8_t> info = {0x01, 0x02, 0x03};

    auto result16 = quic::hkdf::expand(prk, info, 16);
    ASSERT_TRUE(result16.is_ok());
    EXPECT_EQ(result16.value().size(), 16);

    auto result32 = quic::hkdf::expand(prk, info, 32);
    ASSERT_TRUE(result32.is_ok());
    EXPECT_EQ(result32.value().size(), 32);
}

TEST_F(HkdfTest, ExpandLabelWithQuicLabels)
{
    std::array<uint8_t, 32> secret;
    secret.fill(0xAB);

    auto key_result = quic::hkdf::expand_label(
        secret, "quic key", {}, quic::aes_128_key_size);
    ASSERT_TRUE(key_result.is_ok());
    EXPECT_EQ(key_result.value().size(), quic::aes_128_key_size);

    auto iv_result = quic::hkdf::expand_label(
        secret, "quic iv", {}, quic::aead_iv_size);
    ASSERT_TRUE(iv_result.is_ok());
    EXPECT_EQ(iv_result.value().size(), quic::aead_iv_size);

    auto hp_result = quic::hkdf::expand_label(
        secret, "quic hp", {}, quic::hp_key_size);
    ASSERT_TRUE(hp_result.is_ok());
    EXPECT_EQ(hp_result.value().size(), quic::hp_key_size);
}

// ============================================================================
// initial_keys Tests
// ============================================================================

class InitialKeysTest : public ::testing::Test
{
protected:
    quic::connection_id test_dcid;

    void SetUp() override
    {
        // RFC 9001 Appendix A.1 test vector DCID
        std::vector<uint8_t> dcid_bytes = {
            0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08};
        test_dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(InitialKeysTest, DeriveProducesValidKeys)
{
    auto result = quic::initial_keys::derive(test_dcid);
    ASSERT_TRUE(result.is_ok());

    auto& keys = result.value();
    EXPECT_TRUE(keys.is_valid());
    EXPECT_TRUE(keys.read.is_valid());
    EXPECT_TRUE(keys.write.is_valid());
}

TEST_F(InitialKeysTest, ClientAndServerKeysDiffer)
{
    auto result = quic::initial_keys::derive(test_dcid);
    ASSERT_TRUE(result.is_ok());

    auto& keys = result.value();
    EXPECT_NE(keys.read.key, keys.write.key);
    EXPECT_NE(keys.read.iv, keys.write.iv);
    EXPECT_NE(keys.read.hp_key, keys.write.hp_key);
}

TEST_F(InitialKeysTest, DeterministicDerivation)
{
    auto result1 = quic::initial_keys::derive(test_dcid);
    auto result2 = quic::initial_keys::derive(test_dcid);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    EXPECT_EQ(result1.value().write.key, result2.value().write.key);
    EXPECT_EQ(result1.value().write.iv, result2.value().write.iv);
    EXPECT_EQ(result1.value().read.key, result2.value().read.key);
    EXPECT_EQ(result1.value().read.iv, result2.value().read.iv);
}

TEST_F(InitialKeysTest, DifferentCidProducesDifferentKeys)
{
    std::vector<uint8_t> other_dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    quic::connection_id other_dcid(other_dcid_bytes);

    auto result1 = quic::initial_keys::derive(test_dcid);
    auto result2 = quic::initial_keys::derive(other_dcid);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    EXPECT_NE(result1.value().write.key, result2.value().write.key);
}

// ============================================================================
// quic_crypto Derive Initial Secrets Tests
// ============================================================================

class QuicCryptoDeriveTest : public ::testing::Test
{
};

TEST_F(QuicCryptoDeriveTest, DeriveInitialSecretsAfterInit)
{
    quic::quic_crypto crypto;
    auto init_result = crypto.init_client("localhost");
    ASSERT_TRUE(init_result.is_ok());

    std::vector<uint8_t> dcid_bytes = {0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08};
    quic::connection_id dcid(dcid_bytes);

    auto derive_result = crypto.derive_initial_secrets(dcid);
    ASSERT_TRUE(derive_result.is_ok());

    auto write_keys = crypto.get_write_keys(quic::encryption_level::initial);
    ASSERT_TRUE(write_keys.is_ok());
    EXPECT_TRUE(write_keys.value().is_valid());

    auto read_keys = crypto.get_read_keys(quic::encryption_level::initial);
    ASSERT_TRUE(read_keys.is_ok());
    EXPECT_TRUE(read_keys.value().is_valid());
}

// ============================================================================
// quic_crypto Set/Get Keys Tests
// ============================================================================

class QuicCryptoSetKeysTest : public ::testing::Test
{
};

TEST_F(QuicCryptoSetKeysTest, SetAndGetKeys)
{
    quic::quic_crypto crypto;

    quic::quic_keys read_keys, write_keys;
    read_keys.key.fill(0xAA);
    write_keys.key.fill(0xBB);

    crypto.set_keys(quic::encryption_level::handshake, read_keys, write_keys);

    auto get_read = crypto.get_read_keys(quic::encryption_level::handshake);
    ASSERT_TRUE(get_read.is_ok());
    EXPECT_EQ(get_read.value().key, read_keys.key);

    auto get_write = crypto.get_write_keys(quic::encryption_level::handshake);
    ASSERT_TRUE(get_write.is_ok());
    EXPECT_EQ(get_write.value().key, write_keys.key);
}

// ============================================================================
// quic_crypto Move Semantics Tests
// ============================================================================

class QuicCryptoMoveTest : public ::testing::Test
{
};

TEST_F(QuicCryptoMoveTest, MoveConstruction)
{
    quic::quic_crypto crypto1;
    auto result = crypto1.init_client("localhost");
    ASSERT_TRUE(result.is_ok());

    quic::quic_crypto crypto2(std::move(crypto1));
    EXPECT_FALSE(crypto2.is_server());
}

TEST_F(QuicCryptoMoveTest, MoveAssignment)
{
    quic::quic_crypto crypto1;
    auto result = crypto1.init_client("localhost");
    ASSERT_TRUE(result.is_ok());

    quic::quic_crypto crypto2;
    crypto2 = std::move(crypto1);
    EXPECT_FALSE(crypto2.is_server());
}

// ============================================================================
// Packet Protection Tests
// ============================================================================

class PacketProtectionTest : public ::testing::Test
{
protected:
    quic::quic_keys test_keys;

    void SetUp() override
    {
        for (size_t i = 0; i < test_keys.key.size(); ++i)
            test_keys.key[i] = static_cast<uint8_t>(i);
        for (size_t i = 0; i < test_keys.iv.size(); ++i)
            test_keys.iv[i] = static_cast<uint8_t>(i + 16);
        for (size_t i = 0; i < test_keys.hp_key.size(); ++i)
            test_keys.hp_key[i] = static_cast<uint8_t>(i + 32);
    }
};

TEST_F(PacketProtectionTest, ProtectUnprotectRoundTrip)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01, 0x08};
    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', ' ', 'Q', 'U', 'I', 'C'};
    uint64_t packet_number = 42;

    auto protect_result = quic::packet_protection::protect(
        test_keys, header, payload, packet_number);
    ASSERT_TRUE(protect_result.is_ok());

    auto& protected_packet = protect_result.value();
    EXPECT_GT(protected_packet.size(), header.size() + payload.size());

    auto unprotect_result = quic::packet_protection::unprotect(
        test_keys, protected_packet, header.size(), packet_number);
    ASSERT_TRUE(unprotect_result.is_ok());

    auto& [recovered_header, recovered_payload] = unprotect_result.value();
    EXPECT_EQ(recovered_header, header);
    EXPECT_EQ(recovered_payload, payload);
}

TEST_F(PacketProtectionTest, TamperedDataFailsAuthentication)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01, 0x08};
    std::vector<uint8_t> payload = {'S', 'e', 'c', 'r', 'e', 't'};
    uint64_t packet_number = 100;

    auto protect_result = quic::packet_protection::protect(
        test_keys, header, payload, packet_number);
    ASSERT_TRUE(protect_result.is_ok());

    auto& protected_packet = protect_result.value();
    if (protected_packet.size() > header.size() + 1)
    {
        protected_packet[header.size() + 1] ^= 0xFF;
    }

    auto unprotect_result = quic::packet_protection::unprotect(
        test_keys, protected_packet, header.size(), packet_number);
    EXPECT_FALSE(unprotect_result.is_ok());
}

TEST_F(PacketProtectionTest, GenerateHpMask)
{
    std::vector<uint8_t> sample(quic::hp_sample_size, 0xAB);

    auto result = quic::packet_protection::generate_hp_mask(test_keys.hp_key, sample);
    ASSERT_TRUE(result.is_ok());

    auto& mask = result.value();
    EXPECT_EQ(mask.size(), 5);
}

TEST_F(PacketProtectionTest, HeaderProtectionRoundTrip)
{
    std::vector<uint8_t> header = {0xC3, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x01};
    std::vector<uint8_t> original_header = header;
    std::vector<uint8_t> sample(quic::hp_sample_size, 0x55);
    size_t pn_offset = 5;
    size_t pn_length = 4;

    auto protect_result = quic::packet_protection::protect_header(
        test_keys, header, pn_offset, pn_length, sample);
    ASSERT_TRUE(protect_result.is_ok());

    EXPECT_NE(header, original_header);

    auto unprotect_result = quic::packet_protection::unprotect_header(
        test_keys, header, pn_offset, sample);
    ASSERT_TRUE(unprotect_result.is_ok());

    auto [first_byte, recovered_pn_len] = unprotect_result.value();
    EXPECT_EQ(recovered_pn_len, pn_length);
    EXPECT_EQ(header, original_header);
}
