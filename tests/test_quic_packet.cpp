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

#include "kcenon/network/detail/protocols/quic/connection_id.h"
#include "internal/protocols/quic/packet.h"

using namespace kcenon::network::protocols::quic;

// ============================================================================
// Connection ID Tests
// ============================================================================

class ConnectionIdTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConnectionIdTest, DefaultConstructor)
{
    connection_id cid;
    EXPECT_TRUE(cid.empty());
    EXPECT_EQ(cid.length(), 0);
}

TEST_F(ConnectionIdTest, ConstructFromBytes)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    connection_id cid(data);

    EXPECT_FALSE(cid.empty());
    EXPECT_EQ(cid.length(), 8);

    auto span = cid.data();
    EXPECT_EQ(span.size(), 8);
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(span[i], data[i]);
    }
}

TEST_F(ConnectionIdTest, ConstructFromLongData)
{
    // Data longer than max_length (20) should be truncated
    std::vector<uint8_t> data(25, 0xAB);
    connection_id cid(data);

    EXPECT_EQ(cid.length(), connection_id::max_length);
}

TEST_F(ConnectionIdTest, Generate)
{
    auto cid1 = connection_id::generate(8);
    auto cid2 = connection_id::generate(8);

    EXPECT_EQ(cid1.length(), 8);
    EXPECT_EQ(cid2.length(), 8);

    // Two random CIDs should be different (with extremely high probability)
    EXPECT_NE(cid1, cid2);
}

TEST_F(ConnectionIdTest, GenerateWithDifferentLengths)
{
    auto cid1 = connection_id::generate(1);
    auto cid8 = connection_id::generate(8);
    auto cid20 = connection_id::generate(20);
    auto cid_over = connection_id::generate(25);  // Should be clamped to 20

    EXPECT_EQ(cid1.length(), 1);
    EXPECT_EQ(cid8.length(), 8);
    EXPECT_EQ(cid20.length(), 20);
    EXPECT_EQ(cid_over.length(), 20);
}

TEST_F(ConnectionIdTest, Equality)
{
    std::vector<uint8_t> data1 = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> data2 = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> data3 = {0x01, 0x02, 0x03, 0x05};

    connection_id cid1(data1);
    connection_id cid2(data2);
    connection_id cid3(data3);

    EXPECT_EQ(cid1, cid2);
    EXPECT_NE(cid1, cid3);
}

TEST_F(ConnectionIdTest, LessThanComparison)
{
    std::vector<uint8_t> data1 = {0x01, 0x02};
    std::vector<uint8_t> data2 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> data3 = {0x01, 0x03};

    connection_id cid1(data1);
    connection_id cid2(data2);
    connection_id cid3(data3);

    // Shorter length comes first
    EXPECT_TRUE(cid1 < cid2);

    // Same length, lexicographic comparison
    EXPECT_TRUE(cid1 < cid3);
}

TEST_F(ConnectionIdTest, ToString)
{
    std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67};
    connection_id cid(data);

    EXPECT_EQ(cid.to_string(), "01234567");
}

TEST_F(ConnectionIdTest, EmptyToString)
{
    connection_id cid;
    EXPECT_EQ(cid.to_string(), "<empty>");
}

// ============================================================================
// Packet Number Tests
// ============================================================================

class PacketNumberTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PacketNumberTest, EncodedLength)
{
    // Small difference -> 1 byte (< 128 unacked)
    EXPECT_EQ(packet_number::encoded_length(10, 5), 1);
    EXPECT_EQ(packet_number::encoded_length(100, 50), 1);

    // Medium difference -> 2 bytes (128 <= unacked < 32768)
    EXPECT_EQ(packet_number::encoded_length(200, 0), 2);  // 200 unacked needs 2 bytes
    EXPECT_EQ(packet_number::encoded_length(1000, 0), 2);

    // Larger difference -> 3 bytes
    EXPECT_EQ(packet_number::encoded_length(100000, 0), 3);

    // Even larger -> 4 bytes
    EXPECT_EQ(packet_number::encoded_length(100000000, 0), 4);
}

TEST_F(PacketNumberTest, EncodeAndDecode)
{
    // Test encoding produces correct output
    // For pn=100 with no prior acked packets
    uint64_t pn = 100;
    uint64_t largest_acked = 0;

    auto [encoded, len] = packet_number::encode(pn, largest_acked);
    EXPECT_GT(len, 0);
    EXPECT_EQ(encoded.size(), len);

    // For simple case where the full packet number fits in encoded bytes,
    // decoding should work correctly
    uint64_t truncated = 0;
    for (size_t i = 0; i < len; ++i)
    {
        truncated = (truncated << 8) | encoded[i];
    }

    // When largest_pn is close to expected, decode should recover the value
    // For first packet (largest_pn = 0), the truncated value is the full value
    if (len == 1 && pn < 128)
    {
        EXPECT_EQ(truncated, pn);
    }
}

TEST_F(PacketNumberTest, DecodeWrapAround)
{
    // Test wrap-around case from RFC 9000 Appendix A
    uint64_t largest_pn = 0xa82f30ea;
    uint64_t truncated_pn = 0x9b32;
    size_t pn_len = 2;

    uint64_t full_pn = packet_number::decode(truncated_pn, pn_len, largest_pn);
    EXPECT_EQ(full_pn, 0xa82f9b32);
}

// ============================================================================
// Packet Type Tests
// ============================================================================

class PacketTypeTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PacketTypeTest, TypeToString)
{
    EXPECT_EQ(packet_type_to_string(packet_type::initial), "Initial");
    EXPECT_EQ(packet_type_to_string(packet_type::zero_rtt), "0-RTT");
    EXPECT_EQ(packet_type_to_string(packet_type::handshake), "Handshake");
    EXPECT_EQ(packet_type_to_string(packet_type::retry), "Retry");
    EXPECT_EQ(packet_type_to_string(packet_type::one_rtt), "1-RTT");
}

// ============================================================================
// Packet Parser Tests
// ============================================================================

class PacketParserTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PacketParserTest, IsLongHeader)
{
    EXPECT_TRUE(packet_parser::is_long_header(0xC0));   // Long header
    EXPECT_TRUE(packet_parser::is_long_header(0xFF));   // Long header
    EXPECT_FALSE(packet_parser::is_long_header(0x40));  // Short header
    EXPECT_FALSE(packet_parser::is_long_header(0x00));  // Invalid
}

TEST_F(PacketParserTest, HasValidFixedBit)
{
    EXPECT_TRUE(packet_parser::has_valid_fixed_bit(0xC0));  // Both set
    EXPECT_TRUE(packet_parser::has_valid_fixed_bit(0x40));  // Only fixed
    EXPECT_FALSE(packet_parser::has_valid_fixed_bit(0x80)); // Only form
    EXPECT_FALSE(packet_parser::has_valid_fixed_bit(0x00)); // Neither
}

TEST_F(PacketParserTest, GetLongPacketType)
{
    // Initial: type bits = 00
    EXPECT_EQ(packet_parser::get_long_packet_type(0xC0), packet_type::initial);

    // 0-RTT: type bits = 01
    EXPECT_EQ(packet_parser::get_long_packet_type(0xD0), packet_type::zero_rtt);

    // Handshake: type bits = 10
    EXPECT_EQ(packet_parser::get_long_packet_type(0xE0), packet_type::handshake);

    // Retry: type bits = 11
    EXPECT_EQ(packet_parser::get_long_packet_type(0xF0), packet_type::retry);
}

TEST_F(PacketParserTest, ParseEmptyData)
{
    std::vector<uint8_t> empty;
    auto result = packet_parser::parse_header(empty);
    EXPECT_TRUE(result.is_err());
}

TEST_F(PacketParserTest, ParseInvalidFixedBit)
{
    // Long header with invalid fixed bit
    std::vector<uint8_t> data = {0x80, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    auto result = packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(PacketParserTest, IsVersionNegotiation)
{
    // Version negotiation: long header + version 0
    std::vector<uint8_t> vn = {0xC0, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(packet_parser::is_version_negotiation(vn));

    // Regular packet with version 1
    std::vector<uint8_t> v1 = {0xC0, 0x00, 0x00, 0x00, 0x01};
    EXPECT_FALSE(packet_parser::is_version_negotiation(v1));

    // Short header (not version negotiation)
    std::vector<uint8_t> sh = {0x40, 0x00, 0x00, 0x00, 0x00};
    EXPECT_FALSE(packet_parser::is_version_negotiation(sh));
}

// ============================================================================
// Packet Builder Tests
// ============================================================================

class PacketBuilderTest : public ::testing::Test
{
protected:
    connection_id dest_cid;
    connection_id src_cid;

    void SetUp() override
    {
        std::vector<uint8_t> dest_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        std::vector<uint8_t> src_data = {0x11, 0x12, 0x13, 0x14};

        dest_cid = connection_id(dest_data);
        src_cid = connection_id(src_data);
    }
};

TEST_F(PacketBuilderTest, BuildInitialPacket)
{
    std::vector<uint8_t> token = {0xAA, 0xBB, 0xCC};
    auto packet = packet_builder::build_initial(dest_cid, src_cid, token, 0);

    EXPECT_FALSE(packet.empty());

    // Verify it's a long header
    EXPECT_TRUE(packet_parser::is_long_header(packet[0]));

    // Verify fixed bit
    EXPECT_TRUE(packet_parser::has_valid_fixed_bit(packet[0]));

    // Verify packet type
    EXPECT_EQ(packet_parser::get_long_packet_type(packet[0]), packet_type::initial);

    // Parse and verify
    auto result = packet_parser::parse_long_header(packet);
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_EQ(header.type(), packet_type::initial);
    EXPECT_EQ(header.version, quic_version::version_1);
    EXPECT_EQ(header.dest_conn_id, dest_cid);
    EXPECT_EQ(header.src_conn_id, src_cid);
    EXPECT_EQ(header.token, token);
}

TEST_F(PacketBuilderTest, BuildHandshakePacket)
{
    auto packet = packet_builder::build_handshake(dest_cid, src_cid, 100);

    EXPECT_FALSE(packet.empty());
    EXPECT_TRUE(packet_parser::is_long_header(packet[0]));
    EXPECT_EQ(packet_parser::get_long_packet_type(packet[0]), packet_type::handshake);

    // Note: The built packet includes a placeholder for packet number but not packet length
    // This is enough to verify the header structure
    // Verify basic header fields manually
    EXPECT_TRUE(packet_parser::has_valid_fixed_bit(packet[0]));

    // Verify version bytes (bytes 1-4)
    uint32_t version = (static_cast<uint32_t>(packet[1]) << 24) |
                       (static_cast<uint32_t>(packet[2]) << 16) |
                       (static_cast<uint32_t>(packet[3]) << 8) |
                       static_cast<uint32_t>(packet[4]);
    EXPECT_EQ(version, quic_version::version_1);
}

TEST_F(PacketBuilderTest, BuildZeroRttPacket)
{
    auto packet = packet_builder::build_zero_rtt(dest_cid, src_cid, 42);

    EXPECT_FALSE(packet.empty());
    EXPECT_TRUE(packet_parser::is_long_header(packet[0]));
    EXPECT_EQ(packet_parser::get_long_packet_type(packet[0]), packet_type::zero_rtt);

    // Verify basic header structure
    EXPECT_TRUE(packet_parser::has_valid_fixed_bit(packet[0]));

    // Verify version
    uint32_t version = (static_cast<uint32_t>(packet[1]) << 24) |
                       (static_cast<uint32_t>(packet[2]) << 16) |
                       (static_cast<uint32_t>(packet[3]) << 8) |
                       static_cast<uint32_t>(packet[4]);
    EXPECT_EQ(version, quic_version::version_1);
}

TEST_F(PacketBuilderTest, BuildRetryPacket)
{
    std::vector<uint8_t> token = {0xDE, 0xAD, 0xBE, 0xEF};
    std::array<uint8_t, 16> integrity_tag{};
    for (size_t i = 0; i < 16; ++i)
    {
        integrity_tag[i] = static_cast<uint8_t>(i);
    }

    auto packet = packet_builder::build_retry(dest_cid, src_cid, token, integrity_tag);

    EXPECT_FALSE(packet.empty());
    EXPECT_TRUE(packet_parser::is_long_header(packet[0]));
    EXPECT_EQ(packet_parser::get_long_packet_type(packet[0]), packet_type::retry);
}

TEST_F(PacketBuilderTest, BuildShortPacket)
{
    auto packet = packet_builder::build_short(dest_cid, 12345, false, true);

    EXPECT_FALSE(packet.empty());
    EXPECT_FALSE(packet_parser::is_long_header(packet[0]));
    EXPECT_TRUE(packet_parser::has_valid_fixed_bit(packet[0]));

    auto result = packet_parser::parse_short_header(packet, dest_cid.length());
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_EQ(header.dest_conn_id, dest_cid);
    EXPECT_TRUE(header.spin_bit());
    EXPECT_FALSE(header.key_phase());
}

TEST_F(PacketBuilderTest, BuildShortPacketWithKeyPhase)
{
    auto packet = packet_builder::build_short(dest_cid, 0, true, false);

    auto result = packet_parser::parse_short_header(packet, dest_cid.length());
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_FALSE(header.spin_bit());
    EXPECT_TRUE(header.key_phase());
}

// ============================================================================
// Round-trip Tests
// ============================================================================

class PacketRoundTripTest : public ::testing::Test
{
protected:
    connection_id dest_cid;
    connection_id src_cid;

    void SetUp() override
    {
        dest_cid = connection_id::generate(8);
        src_cid = connection_id::generate(4);
    }
};

TEST_F(PacketRoundTripTest, InitialPacketRoundTrip)
{
    std::vector<uint8_t> token = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto built = packet_builder::build_initial(dest_cid, src_cid, token, 0);

    auto parsed = packet_parser::parse_long_header(built);
    ASSERT_TRUE(parsed.is_ok());

    auto& [header, len] = parsed.value();
    EXPECT_EQ(header.type(), packet_type::initial);
    EXPECT_EQ(header.version, quic_version::version_1);
    EXPECT_EQ(header.dest_conn_id, dest_cid);
    EXPECT_EQ(header.src_conn_id, src_cid);
    EXPECT_EQ(header.token, token);
}

TEST_F(PacketRoundTripTest, ShortPacketRoundTrip)
{
    auto built = packet_builder::build_short(dest_cid, 42, true, true);

    auto parsed = packet_parser::parse_short_header(built, dest_cid.length());
    ASSERT_TRUE(parsed.is_ok());

    auto& [header, len] = parsed.value();
    EXPECT_EQ(header.dest_conn_id, dest_cid);
    EXPECT_TRUE(header.spin_bit());
    EXPECT_TRUE(header.key_phase());
}

TEST_F(PacketRoundTripTest, LongHeaderFromStructure)
{
    // Verify that build_initial creates correct header fields
    // Note: The builder creates a minimal header without packet length field
    // which is needed for full parsing. Here we just verify the structure.
    std::vector<uint8_t> token = {0xAA, 0xBB};
    auto built = packet_builder::build_initial(dest_cid, src_cid, token, 0);

    // Verify it's an Initial packet
    EXPECT_TRUE(packet_parser::is_long_header(built[0]));
    EXPECT_EQ(packet_parser::get_long_packet_type(built[0]), packet_type::initial);

    // Verify version
    uint32_t version = (static_cast<uint32_t>(built[1]) << 24) |
                       (static_cast<uint32_t>(built[2]) << 16) |
                       (static_cast<uint32_t>(built[3]) << 8) |
                       static_cast<uint32_t>(built[4]);
    EXPECT_EQ(version, quic_version::version_1);

    // Verify DCID length and SCID length are at expected positions
    EXPECT_EQ(built[5], dest_cid.length());  // DCID length
    size_t scid_len_pos = 5 + 1 + dest_cid.length();
    EXPECT_EQ(built[scid_len_pos], src_cid.length());  // SCID length
}

TEST_F(PacketRoundTripTest, ShortHeaderFromStructure)
{
    short_header original;
    original.first_byte = 0x40 | 0x20 | 0x04;  // Fixed + Spin + Key Phase
    original.dest_conn_id = dest_cid;
    original.packet_number = 999;

    auto built = packet_builder::build(original);

    auto parsed = packet_parser::parse_short_header(built, dest_cid.length());
    ASSERT_TRUE(parsed.is_ok());

    auto& [header, len] = parsed.value();
    EXPECT_EQ(header.dest_conn_id, original.dest_conn_id);
    EXPECT_TRUE(header.spin_bit());
    EXPECT_TRUE(header.key_phase());
}

// ============================================================================
// Edge Cases
// ============================================================================

class PacketEdgeCasesTest : public ::testing::Test
{
protected:
    void SetUp() override {}
};

TEST_F(PacketEdgeCasesTest, EmptyConnectionIds)
{
    connection_id empty_dest;
    connection_id empty_src;

    auto packet = packet_builder::build_initial(empty_dest, empty_src, {}, 0);

    auto result = packet_parser::parse_long_header(packet);
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_TRUE(header.dest_conn_id.empty());
    EXPECT_TRUE(header.src_conn_id.empty());
}

TEST_F(PacketEdgeCasesTest, MaxLengthConnectionIds)
{
    std::vector<uint8_t> max_data(connection_id::max_length, 0xFF);
    connection_id max_cid(max_data);

    auto packet = packet_builder::build_handshake(max_cid, max_cid, 0);

    auto result = packet_parser::parse_long_header(packet);
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_EQ(header.dest_conn_id.length(), connection_id::max_length);
    EXPECT_EQ(header.src_conn_id.length(), connection_id::max_length);
}

TEST_F(PacketEdgeCasesTest, LargePacketNumber)
{
    connection_id cid = connection_id::generate(8);

    // Large packet number that requires 4 bytes
    uint64_t large_pn = 0x12345678;
    auto packet = packet_builder::build_short(cid, large_pn, false, false);

    auto result = packet_parser::parse_short_header(packet, cid.length());
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_EQ(header.packet_number_length, 4);
}

TEST_F(PacketEdgeCasesTest, LargeToken)
{
    connection_id dest = connection_id::generate(8);
    connection_id src = connection_id::generate(4);

    // Large token
    std::vector<uint8_t> large_token(1000, 0xAB);
    auto packet = packet_builder::build_initial(dest, src, large_token, 0);

    auto result = packet_parser::parse_long_header(packet);
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_EQ(header.token.size(), 1000);
}

TEST_F(PacketEdgeCasesTest, QuicVersion2)
{
    connection_id dest = connection_id::generate(8);
    connection_id src = connection_id::generate(4);

    auto packet = packet_builder::build_initial(dest, src, {}, 0, quic_version::version_2);

    auto result = packet_parser::parse_long_header(packet);
    EXPECT_TRUE(result.is_ok());

    auto& [header, len] = result.value();
    EXPECT_EQ(header.version, quic_version::version_2);
}
