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

#include "kcenon/network/detail/protocols/quic/connection_id.h"
#include "internal/protocols/quic/connection.h"
#include "internal/protocols/quic/crypto.h"
#include "internal/protocols/quic/keys.h"
#include "internal/protocols/quic/packet.h"

using namespace kcenon::network::protocols::quic;

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

TEST_F(CryptoIntegrationTest, VersionTwoKeyDerivation)
{
    std::vector<uint8_t> dcid_bytes = {0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08};
    connection_id dcid(dcid_bytes);

    auto v1_result = initial_keys::derive(dcid, quic_version::version_1);
    auto v2_result = initial_keys::derive(dcid, quic_version::version_2);

    ASSERT_TRUE(v1_result.is_ok());
    ASSERT_TRUE(v2_result.is_ok());

    // Version 1 and 2 should produce different keys (different salts)
    EXPECT_NE(v1_result.value().write.key, v2_result.value().write.key);
    EXPECT_NE(v1_result.value().read.key, v2_result.value().read.key);

    // Both should be valid
    EXPECT_TRUE(v2_result.value().is_valid());
}

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

// ============================================================================
// Packet Protection Error Path Tests
// ============================================================================

class PacketProtectionErrorTest : public ::testing::Test
{
protected:
    quic_keys test_keys;

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

TEST_F(PacketProtectionErrorTest, UnprotectTooShortPacket)
{
    // Packet shorter than header_length + aead_tag_size
    std::vector<uint8_t> short_packet = {0xC0, 0x00, 0x01};
    auto result = packet_protection::unprotect(test_keys, short_packet, 3, 0);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PacketProtectionErrorTest, UnprotectExactlyTagSize)
{
    // Packet with exactly header + tag but no ciphertext
    std::vector<uint8_t> packet(5 + aead_tag_size, 0x00);
    auto result = packet_protection::unprotect(test_keys, packet, 5, 0);
    // Should succeed (0 bytes of payload) or fail cleanly
    // The behavior depends on AES-GCM with 0 plaintext
}

TEST_F(PacketProtectionErrorTest, GenerateHpMaskTooShortSample)
{
    std::vector<uint8_t> short_sample(hp_sample_size - 1, 0xAB);
    auto result = packet_protection::generate_hp_mask(test_keys.hp_key, short_sample);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(PacketProtectionErrorTest, ProtectEmptyPayload)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01};
    std::vector<uint8_t> empty_payload;

    auto result = packet_protection::protect(test_keys, header, empty_payload, 0);
    ASSERT_TRUE(result.is_ok());
    // Output should be header + tag only
    EXPECT_EQ(result.value().size(), header.size() + aead_tag_size);
}

TEST_F(PacketProtectionErrorTest, ProtectLargePayload)
{
    std::vector<uint8_t> header = {0xC0, 0x00};
    std::vector<uint8_t> large_payload(4096, 0xAB);

    auto protect_result = packet_protection::protect(
        test_keys, header, large_payload, 42);
    ASSERT_TRUE(protect_result.is_ok());

    auto unprotect_result = packet_protection::unprotect(
        test_keys, protect_result.value(), header.size(), 42);
    ASSERT_TRUE(unprotect_result.is_ok());
    EXPECT_EQ(unprotect_result.value().second, large_payload);
}

TEST_F(PacketProtectionErrorTest, WrongKeyFailsDecryption)
{
    std::vector<uint8_t> header = {0xC0, 0x00};
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};

    auto protect_result = packet_protection::protect(
        test_keys, header, payload, 0);
    ASSERT_TRUE(protect_result.is_ok());

    // Use different keys to decrypt
    quic_keys wrong_keys;
    wrong_keys.key.fill(0xFF);
    wrong_keys.iv.fill(0xEE);

    auto unprotect_result = packet_protection::unprotect(
        wrong_keys, protect_result.value(), header.size(), 0);
    EXPECT_FALSE(unprotect_result.is_ok());
}

// ============================================================================
// Header Protection Edge Cases
// ============================================================================

TEST_F(PacketProtectionErrorTest, ShortHeaderProtection)
{
    // Short header (bit 7 = 0)
    std::vector<uint8_t> header = {0x43, 0x00, 0x00, 0x00, 0x01};
    std::vector<uint8_t> original_header = header;
    std::vector<uint8_t> sample(hp_sample_size, 0x77);
    size_t pn_offset = 1;
    size_t pn_length = 4;

    auto protect_result = packet_protection::protect_header(
        test_keys, header, pn_offset, pn_length, sample);
    ASSERT_TRUE(protect_result.is_ok());

    // Header should be modified (short header masks 5 bits of first byte)
    EXPECT_NE(header, original_header);

    auto unprotect_result = packet_protection::unprotect_header(
        test_keys, header, pn_offset, sample);
    ASSERT_TRUE(unprotect_result.is_ok());

    EXPECT_EQ(header, original_header);
}

TEST_F(PacketProtectionErrorTest, HeaderProtectionOneBytePn)
{
    std::vector<uint8_t> header = {0xC0, 0x00, 0x00, 0x01, 0x00, 0x42};
    std::vector<uint8_t> original = header;
    std::vector<uint8_t> sample(hp_sample_size, 0x33);
    size_t pn_offset = 5;
    size_t pn_length = 1;

    auto pr = packet_protection::protect_header(
        test_keys, header, pn_offset, pn_length, sample);
    ASSERT_TRUE(pr.is_ok());

    auto ur = packet_protection::unprotect_header(
        test_keys, header, pn_offset, sample);
    ASSERT_TRUE(ur.is_ok());
    EXPECT_EQ(header, original);
}

// ============================================================================
// QUIC Crypto Handler Extended Tests
// ============================================================================

TEST_F(QuicCryptoTest, SetAlpn)
{
    quic_crypto crypto;
    auto result = crypto.init_client("localhost");
    ASSERT_TRUE(result.is_ok());

    auto alpn_result = crypto.set_alpn({"h3"});
    (void)alpn_result;
}

TEST_F(QuicCryptoTest, GetAlpn)
{
    quic_crypto crypto;
    auto alpn = crypto.get_alpn();
    EXPECT_TRUE(alpn.empty()); // No handshake done
}

TEST_F(QuicCryptoTest, EarlyDataNotAccepted)
{
    quic_crypto crypto;
    EXPECT_FALSE(crypto.is_early_data_accepted());
    EXPECT_FALSE(crypto.has_zero_rtt_keys());
}

// ============================================================================
// Connection State Tests
// ============================================================================

class ConnectionStateTest : public ::testing::Test
{
};

TEST_F(ConnectionStateTest, StateToStringAll)
{
    EXPECT_STREQ(connection_state_to_string(connection_state::idle), "idle");
    EXPECT_STREQ(connection_state_to_string(connection_state::handshaking), "handshaking");
    EXPECT_STREQ(connection_state_to_string(connection_state::connected), "connected");
    EXPECT_STREQ(connection_state_to_string(connection_state::closing), "closing");
    EXPECT_STREQ(connection_state_to_string(connection_state::draining), "draining");
    EXPECT_STREQ(connection_state_to_string(connection_state::closed), "closed");
}

TEST_F(ConnectionStateTest, HandshakeStateToStringAll)
{
    EXPECT_STREQ(handshake_state_to_string(handshake_state::initial), "initial");
    EXPECT_STREQ(handshake_state_to_string(handshake_state::waiting_server_hello),
                 "waiting_server_hello");
    EXPECT_STREQ(handshake_state_to_string(handshake_state::waiting_finished),
                 "waiting_finished");
    EXPECT_STREQ(handshake_state_to_string(handshake_state::complete), "complete");
}

TEST_F(ConnectionStateTest, ClientConnectionInitialState)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    connection_id dcid(dcid_bytes);

    connection conn(false, dcid);

    EXPECT_EQ(conn.state(), connection_state::idle);
    EXPECT_EQ(conn.handshake_state(), handshake_state::initial);
    EXPECT_FALSE(conn.is_established());
    EXPECT_FALSE(conn.is_draining());
    EXPECT_FALSE(conn.is_closed());
    EXPECT_FALSE(conn.is_server());
    EXPECT_EQ(conn.initial_dcid(), dcid);
    EXPECT_FALSE(conn.local_cid().empty());
    EXPECT_EQ(conn.remote_cid(), dcid);
    EXPECT_FALSE(conn.close_error_code().has_value());
    EXPECT_TRUE(conn.close_reason().empty());
}

TEST_F(ConnectionStateTest, ServerConnectionInitialState)
{
    std::vector<uint8_t> dcid_bytes = {0x05, 0x06, 0x07, 0x08};
    connection_id dcid(dcid_bytes);

    connection conn(true, dcid);

    EXPECT_EQ(conn.state(), connection_state::idle);
    EXPECT_TRUE(conn.is_server());
    EXPECT_FALSE(conn.is_established());
    // Server's remote CID is empty until client's SCID is received
}

TEST_F(ConnectionStateTest, AddLocalCid)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto new_cid = connection_id::generate();
    auto result = conn.add_local_cid(new_cid, 1);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionStateTest, AddDuplicateCidSequence)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto cid1 = connection_id::generate();
    auto cid2 = connection_id::generate();

    auto r1 = conn.add_local_cid(cid1, 1);
    EXPECT_TRUE(r1.is_ok());

    auto r2 = conn.add_local_cid(cid2, 1); // Duplicate sequence
    EXPECT_FALSE(r2.is_ok());
}

TEST_F(ConnectionStateTest, RetireCid)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto new_cid = connection_id::generate();
    (void)conn.add_local_cid(new_cid, 1);

    // Retire CID 1 (still have CID 0 from constructor)
    auto result = conn.retire_cid(1);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionStateTest, RetireNonexistentCid)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto result = conn.retire_cid(99);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionStateTest, RetireLastCid)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    // Only CID 0 exists, cannot retire it
    auto result = conn.retire_cid(0);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionStateTest, SetLocalParams)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    transport_parameters params;
    params.initial_max_data = 1048576;
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 50;

    conn.set_local_params(params);

    auto& lp = conn.local_params();
    EXPECT_EQ(lp.initial_max_data, 1048576);
    EXPECT_EQ(lp.initial_max_streams_bidi, 100);
    EXPECT_EQ(lp.initial_max_streams_uni, 50);
}

TEST_F(ConnectionStateTest, SetRemoteParams)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    transport_parameters params;
    params.initial_max_data = 2097152;
    params.initial_max_streams_bidi = 200;
    params.initial_max_streams_uni = 100;
    params.active_connection_id_limit = 4;
    params.max_idle_timeout = 30000;

    conn.set_remote_params(params);

    auto& rp = conn.remote_params();
    EXPECT_EQ(rp.initial_max_data, 2097152);
    EXPECT_EQ(rp.initial_max_streams_bidi, 200);
    EXPECT_EQ(rp.active_connection_id_limit, 4);
}

TEST_F(ConnectionStateTest, StartHandshakeAsServer)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(true, connection_id(dcid_bytes));

    auto result = conn.start_handshake("localhost");
    EXPECT_FALSE(result.is_ok()); // Server cannot start handshake
}

TEST_F(ConnectionStateTest, StartHandshakeAsClient)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    connection conn(false, connection_id(dcid_bytes));

    auto result = conn.start_handshake("localhost");
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(conn.state(), connection_state::handshaking);
    EXPECT_EQ(conn.handshake_state(), handshake_state::waiting_server_hello);
    EXPECT_FALSE(conn.is_established());
}

TEST_F(ConnectionStateTest, StartHandshakeTwice)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    connection conn(false, connection_id(dcid_bytes));

    auto r1 = conn.start_handshake("localhost");
    ASSERT_TRUE(r1.is_ok());

    auto r2 = conn.start_handshake("localhost");
    EXPECT_FALSE(r2.is_ok()); // Already started
}

TEST_F(ConnectionStateTest, CloseIdleConnection)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto result = conn.close(0, "test close");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(conn.is_draining() || conn.is_closed());
}

TEST_F(ConnectionStateTest, CloseWithErrorCode)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto result = conn.close(0x0A, "flow control error");
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionStateTest, FlowControlAndStreamAccess)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    // Test accessors compile and return references
    auto& fc = conn.flow_control();
    auto& sm = conn.streams();
    const auto& cfc = std::as_const(conn).flow_control();
    const auto& csm = std::as_const(conn).streams();

    (void)fc;
    (void)sm;
    (void)cfc;
    (void)csm;
}

TEST_F(ConnectionStateTest, HasPendingDataInitially)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    // Idle connection should not have pending data
    EXPECT_FALSE(conn.has_pending_data());
}

TEST_F(ConnectionStateTest, ReceiveEmptyPacket)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    std::vector<uint8_t> empty_packet;
    auto result = conn.receive_packet(empty_packet);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionStateTest, PeerCidManagerAccess)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    connection conn(false, connection_id(dcid_bytes));

    auto& mgr = conn.peer_cid_manager();
    const auto& cmgr = std::as_const(conn).peer_cid_manager();

    (void)mgr;
    (void)cmgr;
}
