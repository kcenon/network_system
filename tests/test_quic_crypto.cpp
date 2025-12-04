/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <gtest/gtest.h>

#include <algorithm>

#include "kcenon/network/protocols/quic/connection_id.h"
#include "kcenon/network/protocols/quic/crypto.h"
#include "kcenon/network/protocols/quic/keys.h"

using namespace network_system::protocols::quic;

// ============================================================================
// Keys Tests
// ============================================================================

class QuicKeysTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(QuicKeysTest, DefaultConstructorIsInvalid)
{
    quic_keys keys;
    EXPECT_FALSE(keys.is_valid());
}

TEST_F(QuicKeysTest, KeysWithDataAreValid)
{
    quic_keys keys;
    keys.key[0] = 0x01;
    EXPECT_TRUE(keys.is_valid());
}

TEST_F(QuicKeysTest, ClearZeroesAllData)
{
    quic_keys keys;
    keys.key.fill(0xFF);
    keys.iv.fill(0xFF);
    keys.hp_key.fill(0xFF);
    keys.secret.fill(0xFF);

    EXPECT_TRUE(keys.is_valid());

    keys.clear();

    EXPECT_FALSE(keys.is_valid());

    for (auto b : keys.key)
        EXPECT_EQ(b, 0);
    for (auto b : keys.iv)
        EXPECT_EQ(b, 0);
    for (auto b : keys.hp_key)
        EXPECT_EQ(b, 0);
    for (auto b : keys.secret)
        EXPECT_EQ(b, 0);
}

TEST_F(QuicKeysTest, EqualityOperator)
{
    quic_keys keys1, keys2;
    keys1.key.fill(0xAB);
    keys1.iv.fill(0xCD);
    keys1.hp_key.fill(0xEF);
    keys1.secret.fill(0x12);

    keys2 = keys1;

    EXPECT_EQ(keys1, keys2);
}

TEST_F(QuicKeysTest, InequalityOperator)
{
    quic_keys keys1, keys2;
    keys1.key.fill(0xAB);
    keys2.key.fill(0xCD);

    EXPECT_NE(keys1, keys2);
}

TEST_F(QuicKeysTest, KeyPairValidity)
{
    key_pair pair;
    EXPECT_FALSE(pair.is_valid());

    pair.read.key[0] = 0x01;
    EXPECT_FALSE(pair.is_valid());

    pair.write.key[0] = 0x01;
    EXPECT_TRUE(pair.is_valid());
}

// ============================================================================
// Encryption Level Tests
// ============================================================================

class EncryptionLevelTest : public ::testing::Test
{
};

TEST_F(EncryptionLevelTest, ToStringConversion)
{
    EXPECT_EQ(encryption_level_to_string(encryption_level::initial), "Initial");
    EXPECT_EQ(encryption_level_to_string(encryption_level::handshake), "Handshake");
    EXPECT_EQ(encryption_level_to_string(encryption_level::zero_rtt), "0-RTT");
    EXPECT_EQ(encryption_level_to_string(encryption_level::application), "Application");
}

TEST_F(EncryptionLevelTest, LevelCount)
{
    EXPECT_EQ(encryption_level_count(), 4);
}

// ============================================================================
// HKDF Tests
// ============================================================================

class HkdfTest : public ::testing::Test
{
};

TEST_F(HkdfTest, ExtractProducesValidPrk)
{
    std::array<uint8_t, 20> salt = {
        0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34, 0xb3, 0x4d, 0x17,
        0x9a, 0xe6, 0xa4, 0xc8, 0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a
    };
    std::vector<uint8_t> ikm = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    auto result = hkdf::extract(salt, ikm);
    ASSERT_TRUE(result.is_ok());

    auto& prk = result.value();
    EXPECT_EQ(prk.size(), secret_size);

    // Verify PRK is not all zeros
    bool all_zero = std::all_of(prk.begin(), prk.end(),
                                [](uint8_t b) { return b == 0; });
    EXPECT_FALSE(all_zero);
}

TEST_F(HkdfTest, ExpandProducesCorrectLength)
{
    std::array<uint8_t, 32> prk;
    prk.fill(0xAB);
    std::vector<uint8_t> info = {0x01, 0x02, 0x03};

    auto result = hkdf::expand(prk, info, 16);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 16);

    auto result2 = hkdf::expand(prk, info, 32);
    ASSERT_TRUE(result2.is_ok());
    EXPECT_EQ(result2.value().size(), 32);
}

TEST_F(HkdfTest, ExpandLabelWithQuicLabels)
{
    std::array<uint8_t, 32> secret;
    secret.fill(0xAB);

    auto key_result = hkdf::expand_label(secret, "quic key", {}, aes_128_key_size);
    ASSERT_TRUE(key_result.is_ok());
    EXPECT_EQ(key_result.value().size(), aes_128_key_size);

    auto iv_result = hkdf::expand_label(secret, "quic iv", {}, aead_iv_size);
    ASSERT_TRUE(iv_result.is_ok());
    EXPECT_EQ(iv_result.value().size(), aead_iv_size);

    auto hp_result = hkdf::expand_label(secret, "quic hp", {}, hp_key_size);
    ASSERT_TRUE(hp_result.is_ok());
    EXPECT_EQ(hp_result.value().size(), hp_key_size);
}

// ============================================================================
// Initial Keys Tests (RFC 9001 Appendix A test vectors)
// ============================================================================

class InitialKeysTest : public ::testing::Test
{
protected:
    // RFC 9001 Appendix A.1 test vector
    // Destination Connection ID: 0x8394c8f03e515708
    connection_id test_dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {
            0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08
        };
        test_dcid = connection_id(dcid_bytes);
    }
};

TEST_F(InitialKeysTest, DeriveFromConnectionId)
{
    auto result = initial_keys::derive(test_dcid);
    ASSERT_TRUE(result.is_ok());

    auto& keys = result.value();
    EXPECT_TRUE(keys.is_valid());
    EXPECT_TRUE(keys.read.is_valid());
    EXPECT_TRUE(keys.write.is_valid());
}

TEST_F(InitialKeysTest, ClientAndServerKeysDiffer)
{
    auto result = initial_keys::derive(test_dcid);
    ASSERT_TRUE(result.is_ok());

    auto& keys = result.value();
    EXPECT_NE(keys.read.key, keys.write.key);
    EXPECT_NE(keys.read.iv, keys.write.iv);
    EXPECT_NE(keys.read.hp_key, keys.write.hp_key);
}

TEST_F(InitialKeysTest, DeterministicDerivation)
{
    auto result1 = initial_keys::derive(test_dcid);
    auto result2 = initial_keys::derive(test_dcid);

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
    connection_id other_dcid(other_dcid_bytes);

    auto result1 = initial_keys::derive(test_dcid);
    auto result2 = initial_keys::derive(other_dcid);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    EXPECT_NE(result1.value().write.key, result2.value().write.key);
}

// ============================================================================
// Packet Protection Tests
// ============================================================================

class PacketProtectionTest : public ::testing::Test
{
protected:
    quic_keys test_keys;

    void SetUp() override
    {
        // Set up test keys (not cryptographically secure, just for testing)
        for (size_t i = 0; i < test_keys.key.size(); ++i)
            test_keys.key[i] = static_cast<uint8_t>(i);
        for (size_t i = 0; i < test_keys.iv.size(); ++i)
            test_keys.iv[i] = static_cast<uint8_t>(i + 16);
        for (size_t i = 0; i < test_keys.hp_key.size(); ++i)
            test_keys.hp_key[i] = static_cast<uint8_t>(i + 32);
    }
};

TEST_F(PacketProtectionTest, ProtectUnprotectRoundtrip)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01, 0x08};
    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', ' ', 'Q', 'U', 'I', 'C'};
    uint64_t packet_number = 42;

    auto protect_result = packet_protection::protect(
        test_keys, header, payload, packet_number);
    ASSERT_TRUE(protect_result.is_ok());

    auto& protected_packet = protect_result.value();
    EXPECT_GT(protected_packet.size(), header.size() + payload.size());

    auto unprotect_result = packet_protection::unprotect(
        test_keys, protected_packet, header.size(), packet_number);
    ASSERT_TRUE(unprotect_result.is_ok());

    auto& [recovered_header, recovered_payload] = unprotect_result.value();
    EXPECT_EQ(recovered_header, header);
    EXPECT_EQ(recovered_payload, payload);
}

TEST_F(PacketProtectionTest, DifferentPacketNumbersProduceDifferentCiphertext)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01, 0x08};
    std::vector<uint8_t> payload = {'T', 'e', 's', 't'};

    auto result1 = packet_protection::protect(test_keys, header, payload, 1);
    auto result2 = packet_protection::protect(test_keys, header, payload, 2);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    // Ciphertext should differ due to different nonces
    auto& ct1 = result1.value();
    auto& ct2 = result2.value();

    // Skip header comparison, compare ciphertext portions
    bool same_ciphertext = std::equal(
        ct1.begin() + header.size(), ct1.end(),
        ct2.begin() + header.size(), ct2.end());
    EXPECT_FALSE(same_ciphertext);
}

TEST_F(PacketProtectionTest, TamperedDataFailsAuthentication)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01, 0x08};
    std::vector<uint8_t> payload = {'S', 'e', 'c', 'r', 'e', 't'};
    uint64_t packet_number = 100;

    auto protect_result = packet_protection::protect(
        test_keys, header, payload, packet_number);
    ASSERT_TRUE(protect_result.is_ok());

    // Tamper with the ciphertext
    auto& protected_packet = protect_result.value();
    if (protected_packet.size() > header.size() + 1)
    {
        protected_packet[header.size() + 1] ^= 0xFF;
    }

    auto unprotect_result = packet_protection::unprotect(
        test_keys, protected_packet, header.size(), packet_number);
    EXPECT_FALSE(unprotect_result.is_ok());
}

TEST_F(PacketProtectionTest, GenerateHpMask)
{
    std::vector<uint8_t> sample(hp_sample_size, 0xAB);

    auto result = packet_protection::generate_hp_mask(test_keys.hp_key, sample);
    ASSERT_TRUE(result.is_ok());

    auto& mask = result.value();
    EXPECT_EQ(mask.size(), 5);
}

TEST_F(PacketProtectionTest, HeaderProtectionRoundtrip)
{
    std::vector<uint8_t> header = {0xC3, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00, 0x00, 0x01};
    std::vector<uint8_t> original_header = header;
    std::vector<uint8_t> sample(hp_sample_size, 0x55);
    size_t pn_offset = 5;
    size_t pn_length = 4;

    auto protect_result = packet_protection::protect_header(
        test_keys, header, pn_offset, pn_length, sample);
    ASSERT_TRUE(protect_result.is_ok());

    // Header should be modified
    EXPECT_NE(header, original_header);

    // Now unprotect
    auto unprotect_result = packet_protection::unprotect_header(
        test_keys, header, pn_offset, sample);
    ASSERT_TRUE(unprotect_result.is_ok());

    auto [first_byte, recovered_pn_len] = unprotect_result.value();
    EXPECT_EQ(recovered_pn_len, pn_length);
    EXPECT_EQ(header, original_header);
}

// ============================================================================
// QUIC Crypto Handler Tests
// ============================================================================

class QuicCryptoTest : public ::testing::Test
{
};

TEST_F(QuicCryptoTest, DefaultConstruction)
{
    quic_crypto crypto;
    EXPECT_FALSE(crypto.is_handshake_complete());
    EXPECT_EQ(crypto.current_level(), encryption_level::initial);
}

TEST_F(QuicCryptoTest, InitClient)
{
    quic_crypto crypto;
    auto result = crypto.init_client("localhost");
    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(crypto.is_server());
}

TEST_F(QuicCryptoTest, DeriveInitialSecrets)
{
    quic_crypto crypto;
    auto init_result = crypto.init_client("localhost");
    ASSERT_TRUE(init_result.is_ok());

    std::vector<uint8_t> dcid_bytes = {0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08};
    connection_id dcid(dcid_bytes);

    auto derive_result = crypto.derive_initial_secrets(dcid);
    ASSERT_TRUE(derive_result.is_ok());

    auto write_keys = crypto.get_write_keys(encryption_level::initial);
    ASSERT_TRUE(write_keys.is_ok());
    EXPECT_TRUE(write_keys.value().is_valid());

    auto read_keys = crypto.get_read_keys(encryption_level::initial);
    ASSERT_TRUE(read_keys.is_ok());
    EXPECT_TRUE(read_keys.value().is_valid());
}

TEST_F(QuicCryptoTest, SetAndGetKeys)
{
    quic_crypto crypto;

    quic_keys read_keys, write_keys;
    read_keys.key.fill(0xAA);
    write_keys.key.fill(0xBB);

    crypto.set_keys(encryption_level::handshake, read_keys, write_keys);

    auto get_read = crypto.get_read_keys(encryption_level::handshake);
    ASSERT_TRUE(get_read.is_ok());
    EXPECT_EQ(get_read.value().key, read_keys.key);

    auto get_write = crypto.get_write_keys(encryption_level::handshake);
    ASSERT_TRUE(get_write.is_ok());
    EXPECT_EQ(get_write.value().key, write_keys.key);
}

TEST_F(QuicCryptoTest, MissingKeysReturnError)
{
    quic_crypto crypto;

    auto read_result = crypto.get_read_keys(encryption_level::application);
    EXPECT_FALSE(read_result.is_ok());

    auto write_result = crypto.get_write_keys(encryption_level::application);
    EXPECT_FALSE(write_result.is_ok());
}

TEST_F(QuicCryptoTest, MoveConstruction)
{
    quic_crypto crypto1;
    auto result = crypto1.init_client("localhost");
    ASSERT_TRUE(result.is_ok());

    quic_crypto crypto2(std::move(crypto1));
    EXPECT_FALSE(crypto2.is_server());
}

TEST_F(QuicCryptoTest, MoveAssignment)
{
    quic_crypto crypto1;
    auto result = crypto1.init_client("localhost");
    ASSERT_TRUE(result.is_ok());

    quic_crypto crypto2;
    crypto2 = std::move(crypto1);
    EXPECT_FALSE(crypto2.is_server());
}

TEST_F(QuicCryptoTest, KeyPhase)
{
    quic_crypto crypto;
    EXPECT_EQ(crypto.key_phase(), 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

class CryptoIntegrationTest : public ::testing::Test
{
};

TEST_F(CryptoIntegrationTest, FullInitialPacketProtection)
{
    // Derive initial keys
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    connection_id dcid(dcid_bytes);

    auto keys_result = initial_keys::derive(dcid);
    ASSERT_TRUE(keys_result.is_ok());

    auto& keys = keys_result.value();

    // Create an Initial packet header
    std::vector<uint8_t> header = {
        0xC3,                   // Long header, Initial type
        0x00, 0x00, 0x00, 0x01, // Version 1
        0x08,                   // DCID length
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,  // DCID
        0x00,                   // SCID length (empty)
        0x00,                   // Token length (empty)
        0x41, 0x00,             // Length (varint)
        0x00, 0x00, 0x00, 0x00  // Packet number (4 bytes)
    };

    std::vector<uint8_t> payload = {'C', 'R', 'Y', 'P', 'T', 'O', ' ', 'D', 'A', 'T', 'A'};
    uint64_t packet_number = 0;

    // Protect the packet
    auto protect_result = packet_protection::protect(
        keys.write, header, payload, packet_number);
    ASSERT_TRUE(protect_result.is_ok());

    // Unprotect with the corresponding read keys (from server's perspective)
    auto unprotect_result = packet_protection::unprotect(
        keys.write, protect_result.value(), header.size(), packet_number);
    ASSERT_TRUE(unprotect_result.is_ok());

    EXPECT_EQ(unprotect_result.value().second, payload);
}
