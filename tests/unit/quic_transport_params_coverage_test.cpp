/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/transport_params.h"
#include "internal/protocols/quic/varint.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_transport_params_coverage_test.cpp
 * @brief Error-path and boundary-condition coverage for transport_params.cpp
 *
 * Complements quic_transport_params_test.cpp (45 TEST_F happy-path / basic
 * error cases) by driving the decode() switch's per-case error branches,
 * encode() field-isolation and skip-default paths, and off-by-one boundary
 * values for all documented limits.
 *
 * Scope is strictly additive: existing tests remain the round-trip regression
 * suite; this file targets the unused branches surfaced by the coverage run
 * at 24659295053 (4.2% line coverage on transport_params.cpp, Issue #1015).
 */

namespace
{

// ---------------------------------------------------------------------------
// Helpers for constructing encoded parameter blocks
// ---------------------------------------------------------------------------

auto make_param(uint64_t id, std::span<const uint8_t> value) -> std::vector<uint8_t>
{
    auto id_bytes = quic::varint::encode(id);
    auto len_bytes = quic::varint::encode(value.size());
    std::vector<uint8_t> out;
    out.reserve(id_bytes.size() + len_bytes.size() + value.size());
    out.insert(out.end(), id_bytes.begin(), id_bytes.end());
    out.insert(out.end(), len_bytes.begin(), len_bytes.end());
    out.insert(out.end(), value.begin(), value.end());
    return out;
}

auto make_varint_param(uint64_t id, uint64_t value) -> std::vector<uint8_t>
{
    auto value_enc = quic::varint::encode(value);
    return make_param(id, std::span<const uint8_t>(value_enc.data(), value_enc.size()));
}

auto make_empty_param(uint64_t id) -> std::vector<uint8_t>
{
    return make_param(id, std::span<const uint8_t>{});
}

auto concat(std::initializer_list<std::vector<uint8_t>> parts) -> std::vector<uint8_t>
{
    std::vector<uint8_t> out;
    for (const auto& part : parts)
    {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

auto as_span(const std::vector<uint8_t>& v) -> std::span<const uint8_t>
{
    return std::span<const uint8_t>(v.data(), v.size());
}

// Build a preferred_address value payload (not the full param, just the value)
auto make_preferred_address_payload(uint8_t cid_len, bool truncate_token = false)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> payload;
    payload.reserve(4 + 2 + 16 + 2 + 1 + cid_len + 16);
    // IPv4 (4 bytes)
    for (int i = 0; i < 4; ++i) payload.push_back(static_cast<uint8_t>(192 + i));
    // IPv4 port (2 bytes)
    payload.push_back(0x01);
    payload.push_back(0xBB);  // 443
    // IPv6 (16 bytes)
    for (int i = 0; i < 16; ++i) payload.push_back(static_cast<uint8_t>(0x20 + i));
    // IPv6 port (2 bytes)
    payload.push_back(0x20);
    payload.push_back(0xFB);  // 8443
    // Connection ID length
    payload.push_back(cid_len);
    // Connection ID bytes
    for (uint8_t i = 0; i < cid_len; ++i)
    {
        payload.push_back(static_cast<uint8_t>(0xA0 + i));
    }
    // Stateless reset token (16 bytes)
    size_t token_bytes = truncate_token ? 8 : 16;
    for (size_t i = 0; i < token_bytes; ++i)
    {
        payload.push_back(static_cast<uint8_t>(0xF0 + i));
    }
    return payload;
}

} // anonymous namespace

// ============================================================================
// Fixture: Encode per-field bytewise verification
// ============================================================================

class TransportParamsEncodeBytewiseTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeBytewiseTest, SingleNonDefaultMaxIdleTimeoutEmitsOneParam)
{
    quic::transport_parameters params;
    params.max_idle_timeout = 30000;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::max_idle_timeout, 30000);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, SingleAckDelayExponentEmitsOneParam)
{
    quic::transport_parameters params;
    params.ack_delay_exponent = 10;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::ack_delay_exponent, 10);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, SingleMaxAckDelayEmitsOneParam)
{
    quic::transport_parameters params;
    params.max_ack_delay = 500;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::max_ack_delay, 500);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, SingleMaxUdpPayloadSizeEmitsOneParam)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 1500;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::max_udp_payload_size, 1500);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, SingleInitialMaxDataEmitsOneParam)
{
    quic::transport_parameters params;
    params.initial_max_data = 1048576;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::initial_max_data, 1048576);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, SingleInitialMaxStreamsBidiEmitsOneParam)
{
    quic::transport_parameters params;
    params.initial_max_streams_bidi = 100;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::initial_max_streams_bidi, 100);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, SingleActiveConnectionIdLimitEmitsOneParam)
{
    quic::transport_parameters params;
    params.active_connection_id_limit = 8;
    auto encoded = params.encode();
    auto expected = make_varint_param(quic::transport_param_id::active_connection_id_limit, 8);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, DisableActiveMigrationEmitsEmptyParam)
{
    quic::transport_parameters params;
    params.disable_active_migration = true;
    auto encoded = params.encode();
    auto expected = make_empty_param(quic::transport_param_id::disable_active_migration);
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, StatelessResetTokenEmits16ByteValue)
{
    quic::transport_parameters params;
    std::array<uint8_t, 16> token{};
    for (int i = 0; i < 16; ++i) token[i] = static_cast<uint8_t>(0xA0 + i);
    params.stateless_reset_token = token;
    auto encoded = params.encode();
    auto expected = make_param(quic::transport_param_id::stateless_reset_token,
                               std::span<const uint8_t>(token.data(), token.size()));
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, OriginalDestinationConnectionIdEmitsRawBytes)
{
    quic::transport_parameters params;
    std::array<uint8_t, 8> cid_bytes = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78};
    params.original_destination_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    auto encoded = params.encode();
    auto expected = make_param(
        quic::transport_param_id::original_destination_connection_id,
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    EXPECT_EQ(encoded, expected);
}

TEST_F(TransportParamsEncodeBytewiseTest, InitialSourceConnectionIdEmitsRawBytes)
{
    quic::transport_parameters params;
    std::array<uint8_t, 4> cid_bytes = {0x01, 0x02, 0x03, 0x04};
    params.initial_source_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    auto encoded = params.encode();
    auto expected = make_param(
        quic::transport_param_id::initial_source_connection_id,
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    EXPECT_EQ(encoded, expected);
}

// ============================================================================
// Fixture: Encode skips fields at their default values
// ============================================================================

class TransportParamsEncodeSkipDefaultTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeSkipDefaultTest, AllDefaultsProduceEmptyBuffer)
{
    quic::transport_parameters params;  // all defaults
    auto encoded = params.encode();
    EXPECT_TRUE(encoded.empty());
}

TEST_F(TransportParamsEncodeSkipDefaultTest, MaxIdleTimeoutZeroOmitted)
{
    quic::transport_parameters params;
    params.max_idle_timeout = 0;  // default
    params.initial_max_data = 42;  // trigger at least one emission
    auto encoded = params.encode();
    EXPECT_NE(encoded.end(),
              std::find(encoded.begin(), encoded.end(),
                        static_cast<uint8_t>(quic::transport_param_id::initial_max_data)));
    EXPECT_EQ(encoded.end(),
              std::find(encoded.begin(), encoded.end(),
                        static_cast<uint8_t>(quic::transport_param_id::max_idle_timeout)));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, AckDelayExponentDefaultThreeOmitted)
{
    quic::transport_parameters params;
    params.ack_delay_exponent = 3;  // default, omitted
    auto encoded = params.encode();
    EXPECT_TRUE(encoded.empty());
}

TEST_F(TransportParamsEncodeSkipDefaultTest, MaxAckDelayDefault25Omitted)
{
    quic::transport_parameters params;
    params.max_ack_delay = 25;  // default
    auto encoded = params.encode();
    EXPECT_TRUE(encoded.empty());
}

TEST_F(TransportParamsEncodeSkipDefaultTest, MaxUdpPayloadDefault65527Omitted)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 65527;  // default
    auto encoded = params.encode();
    EXPECT_TRUE(encoded.empty());
}

TEST_F(TransportParamsEncodeSkipDefaultTest, ActiveConnectionIdLimitDefault2Omitted)
{
    quic::transport_parameters params;
    params.active_connection_id_limit = 2;  // default
    auto encoded = params.encode();
    EXPECT_TRUE(encoded.empty());
}

TEST_F(TransportParamsEncodeSkipDefaultTest, DisableActiveMigrationFalseOmitted)
{
    quic::transport_parameters params;
    params.disable_active_migration = false;  // default
    auto encoded = params.encode();
    EXPECT_TRUE(encoded.empty());
}

// ============================================================================
// Fixture: Encode ordering — field source order is preserved
// ============================================================================

class TransportParamsEncodeOrderingTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeOrderingTest, TimingBeforeFlowControl)
{
    quic::transport_parameters params;
    params.max_idle_timeout = 1000;
    params.initial_max_data = 2000;
    auto encoded = params.encode();
    // max_idle_timeout (0x01) should appear before initial_max_data (0x04) in source order
    auto idle_pos = std::find(encoded.begin(), encoded.end(),
                              static_cast<uint8_t>(quic::transport_param_id::max_idle_timeout));
    auto data_pos = std::find(encoded.begin(), encoded.end(),
                              static_cast<uint8_t>(quic::transport_param_id::initial_max_data));
    ASSERT_NE(idle_pos, encoded.end());
    ASSERT_NE(data_pos, encoded.end());
    EXPECT_LT(idle_pos, data_pos);
}

TEST_F(TransportParamsEncodeOrderingTest, StreamLimitsAppearBeforeConnectionOptions)
{
    quic::transport_parameters params;
    params.initial_max_streams_bidi = 50;
    params.disable_active_migration = true;
    auto encoded = params.encode();
    auto streams_pos = std::find(
        encoded.begin(), encoded.end(),
        static_cast<uint8_t>(quic::transport_param_id::initial_max_streams_bidi));
    auto disable_pos = std::find(
        encoded.begin(), encoded.end(),
        static_cast<uint8_t>(quic::transport_param_id::disable_active_migration));
    ASSERT_NE(streams_pos, encoded.end());
    ASSERT_NE(disable_pos, encoded.end());
    EXPECT_LT(streams_pos, disable_pos);
}

// ============================================================================
// Fixture: Encode preferred_address wire layout
// ============================================================================

class TransportParamsEncodePreferredAddressTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodePreferredAddressTest, WithEmptyCidProduces41ByteValue)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    addr.ipv4_address = {10, 0, 0, 1};
    addr.ipv4_port = 443;
    addr.ipv6_port = 8443;
    // Empty connection_id (default)
    for (int i = 0; i < 16; ++i)
    {
        addr.stateless_reset_token[i] = static_cast<uint8_t>(0xAA + i);
    }
    params.preferred_address = addr;
    auto encoded = params.encode();

    // Locate the preferred_address param in the encoded buffer
    // (it's the only param emitted)
    ASSERT_FALSE(encoded.empty());
    EXPECT_EQ(encoded[0],
              static_cast<uint8_t>(quic::transport_param_id::preferred_address));
    // Next byte is param length varint; 41 fits in 1-byte varint only if <=63.
    // 41 < 63, so single byte 0x29.
    EXPECT_EQ(encoded[1], 41u);
    // Total = 1 (id) + 1 (len) + 41 (value) = 43
    EXPECT_EQ(encoded.size(), 43u);
}

TEST_F(TransportParamsEncodePreferredAddressTest, WithMaxCidProduces61ByteValue)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    std::array<uint8_t, 20> cid_bytes{};
    for (int i = 0; i < 20; ++i) cid_bytes[i] = static_cast<uint8_t>(0xC0 + i);
    addr.connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    params.preferred_address = addr;
    auto encoded = params.encode();

    ASSERT_FALSE(encoded.empty());
    EXPECT_EQ(encoded[0],
              static_cast<uint8_t>(quic::transport_param_id::preferred_address));
    EXPECT_EQ(encoded[1], 61u);  // 41 + 20
    EXPECT_EQ(encoded.size(), 63u);  // 1 + 1 + 61
}

TEST_F(TransportParamsEncodePreferredAddressTest, IPv4AddressAtFixedOffset)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    addr.ipv4_address = {203, 0, 113, 5};
    params.preferred_address = addr;
    auto encoded = params.encode();

    // encoded[0] = id, encoded[1] = len, encoded[2..5] = IPv4 address
    ASSERT_GE(encoded.size(), 6u);
    EXPECT_EQ(encoded[2], 203);
    EXPECT_EQ(encoded[3], 0);
    EXPECT_EQ(encoded[4], 113);
    EXPECT_EQ(encoded[5], 5);
}

TEST_F(TransportParamsEncodePreferredAddressTest, IPv4PortBigEndianAtOffset6)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    addr.ipv4_port = 0x1234;
    params.preferred_address = addr;
    auto encoded = params.encode();
    ASSERT_GE(encoded.size(), 8u);
    // Value bytes start at encoded[2]; port is at offset 4 within the value (after 4-byte IPv4)
    // So in the full buffer: encoded[2 + 4] = high byte, encoded[2 + 5] = low byte
    EXPECT_EQ(encoded[6], 0x12);
    EXPECT_EQ(encoded[7], 0x34);
}

TEST_F(TransportParamsEncodePreferredAddressTest, CidLengthByteAtOffset24)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    std::array<uint8_t, 5> cid_bytes = {0xDE, 0xAD, 0xBE, 0xEF, 0x01};
    addr.connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    params.preferred_address = addr;
    auto encoded = params.encode();
    // Value starts at encoded[2]; cid_len is at value offset 24 → encoded[2+24] = encoded[26]
    ASSERT_GE(encoded.size(), 27u);
    EXPECT_EQ(encoded[26], 5u);
}

// ============================================================================
// Fixture: Decode boundary accept / reject
// ============================================================================

class TransportParamsDecodeBoundaryTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeBoundaryTest, AckDelayExponentExactly20Accepted)
{
    auto data = make_varint_param(quic::transport_param_id::ack_delay_exponent, 20);
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().ack_delay_exponent, 20u);
}

TEST_F(TransportParamsDecodeBoundaryTest, AckDelayExponent21Rejected)
{
    auto data = make_varint_param(quic::transport_param_id::ack_delay_exponent, 21);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxAckDelayExactly16383Accepted)
{
    auto data = make_varint_param(quic::transport_param_id::max_ack_delay, 16383);
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().max_ack_delay, 16383u);
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxAckDelay16384Rejected)
{
    auto data = make_varint_param(quic::transport_param_id::max_ack_delay, 16384);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxUdpPayloadSizeExactly1200Accepted)
{
    auto data = make_varint_param(quic::transport_param_id::max_udp_payload_size, 1200);
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().max_udp_payload_size, 1200u);
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxUdpPayloadSize1199Rejected)
{
    auto data = make_varint_param(quic::transport_param_id::max_udp_payload_size, 1199);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, ActiveConnectionIdLimitExactly2Accepted)
{
    auto data = make_varint_param(quic::transport_param_id::active_connection_id_limit, 2);
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().active_connection_id_limit, 2u);
}

TEST_F(TransportParamsDecodeBoundaryTest, ActiveConnectionIdLimit1Rejected)
{
    auto data = make_varint_param(quic::transport_param_id::active_connection_id_limit, 1);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, ActiveConnectionIdLimit0Rejected)
{
    auto data = make_varint_param(quic::transport_param_id::active_connection_id_limit, 0);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, ConnectionIdExactly20BytesAccepted)
{
    std::vector<uint8_t> cid_bytes(20, 0xAB);
    auto data = make_param(quic::transport_param_id::original_destination_connection_id,
                           as_span(cid_bytes));
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result.value().original_destination_connection_id.has_value());
    EXPECT_EQ(result.value().original_destination_connection_id->length(), 20u);
}

TEST_F(TransportParamsDecodeBoundaryTest, ConnectionIdZeroBytesAccepted)
{
    auto data = make_empty_param(quic::transport_param_id::initial_source_connection_id);
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result.value().initial_source_connection_id.has_value());
    EXPECT_TRUE(result.value().initial_source_connection_id->empty());
}

TEST_F(TransportParamsDecodeBoundaryTest, ConnectionId21BytesRejected)
{
    std::vector<uint8_t> cid_bytes(21, 0xAB);
    auto data = make_param(quic::transport_param_id::retry_source_connection_id,
                           as_span(cid_bytes));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, StatelessResetTokenExactly16BytesAccepted)
{
    std::vector<uint8_t> token(16, 0x5A);
    auto data = make_param(quic::transport_param_id::stateless_reset_token, as_span(token));
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result.value().stateless_reset_token.has_value());
}

TEST_F(TransportParamsDecodeBoundaryTest, StatelessResetToken15BytesRejected)
{
    std::vector<uint8_t> token(15, 0x5A);
    auto data = make_param(quic::transport_param_id::stateless_reset_token, as_span(token));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, StatelessResetToken17BytesRejected)
{
    std::vector<uint8_t> token(17, 0x5A);
    auto data = make_param(quic::transport_param_id::stateless_reset_token, as_span(token));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, DisableActiveMigrationZeroLengthAccepted)
{
    auto data = make_empty_param(quic::transport_param_id::disable_active_migration);
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().disable_active_migration);
}

TEST_F(TransportParamsDecodeBoundaryTest, DisableActiveMigrationOneByteLengthRejected)
{
    std::vector<uint8_t> one_byte{0x00};
    auto data = make_param(quic::transport_param_id::disable_active_migration,
                           as_span(one_byte));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Fixture: Decode malformed inner varint
// ============================================================================

class TransportParamsDecodeMalformedVarintTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeMalformedVarintTest, MaxIdleTimeoutEmptyValueFails)
{
    // Advertises zero-length value for max_idle_timeout → inner varint decode
    // of zero bytes fails.
    auto data = make_empty_param(quic::transport_param_id::max_idle_timeout);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, AckDelayExponentEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::ack_delay_exponent);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, MaxAckDelayEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::max_ack_delay);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, MaxUdpPayloadSizeEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::max_udp_payload_size);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxDataEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::initial_max_data);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxStreamDataBidiLocalEmptyValueFails)
{
    auto data = make_empty_param(
        quic::transport_param_id::initial_max_stream_data_bidi_local);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxStreamDataBidiRemoteEmptyValueFails)
{
    auto data = make_empty_param(
        quic::transport_param_id::initial_max_stream_data_bidi_remote);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxStreamDataUniEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::initial_max_stream_data_uni);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxStreamsBidiEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::initial_max_streams_bidi);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxStreamsUniEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::initial_max_streams_uni);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, ActiveConnectionIdLimitEmptyValueFails)
{
    auto data = make_empty_param(quic::transport_param_id::active_connection_id_limit);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Fixture: Decode preferred_address edge cases
// ============================================================================

class TransportParamsDecodePreferredAddressTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodePreferredAddressTest, ExactMinimum41BytesWithEmptyCidAccepted)
{
    auto payload = make_preferred_address_payload(/*cid_len=*/0);
    ASSERT_EQ(payload.size(), 41u);
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result.value().preferred_address.has_value());
    EXPECT_TRUE(result.value().preferred_address->connection_id.empty());
}

TEST_F(TransportParamsDecodePreferredAddressTest, BelowMinimum40BytesRejected)
{
    std::vector<uint8_t> payload(40, 0);  // just below minimum
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, EmptyPayloadRejected)
{
    auto data = make_empty_param(quic::transport_param_id::preferred_address);
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, MaxCidLength20Accepted)
{
    auto payload = make_preferred_address_payload(/*cid_len=*/20);
    ASSERT_EQ(payload.size(), 61u);
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result.value().preferred_address.has_value());
    EXPECT_EQ(result.value().preferred_address->connection_id.length(), 20u);
}

TEST_F(TransportParamsDecodePreferredAddressTest, CidLengthField21Rejected)
{
    // Payload declares cid_len=21 but provides only 20 cid bytes + token; the
    // cid_len > 20 branch fires first regardless of trailing bytes.
    std::vector<uint8_t> payload;
    payload.reserve(41);
    for (int i = 0; i < 4; ++i) payload.push_back(0);
    payload.push_back(0); payload.push_back(0);
    for (int i = 0; i < 16; ++i) payload.push_back(0);
    payload.push_back(0); payload.push_back(0);
    payload.push_back(21);  // cid_len field
    // We still need at least 41 bytes total so that param_len < 41 check doesn't
    // fire first. Add 16 bytes of padding to satisfy length; actual content
    // beyond cid_len byte is irrelevant because cid_len > 20 rejects early.
    while (payload.size() < 41) payload.push_back(0);
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, TruncatedCidRejected)
{
    // Declare cid_len=10 but param_len allows only 5 cid bytes + 16 token bytes.
    // Expected: offset(25) + cid_len(10) + 16 > param_len(25+5+16=46 would work,
    // but we want 40 which forces rejection). Set param_len so that
    // offset(25) + 10 + 16 = 51 > param_len (e.g., 45).
    std::vector<uint8_t> payload;
    payload.reserve(45);
    for (int i = 0; i < 4; ++i) payload.push_back(0);
    payload.push_back(0); payload.push_back(0);
    for (int i = 0; i < 16; ++i) payload.push_back(0);
    payload.push_back(0); payload.push_back(0);
    payload.push_back(10);  // cid_len = 10, needs 10 + 16 = 26 more bytes
    // But we provide only 19 more bytes (total 45), so offset+cid_len+16 = 51 > 45
    while (payload.size() < 45) payload.push_back(0);
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, ParsedFieldsPreserveWireValues)
{
    auto payload = make_preferred_address_payload(/*cid_len=*/5);
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    const auto& addr = *result.value().preferred_address;
    EXPECT_EQ(addr.ipv4_address[0], 192);
    EXPECT_EQ(addr.ipv4_address[1], 193);
    EXPECT_EQ(addr.ipv4_address[2], 194);
    EXPECT_EQ(addr.ipv4_address[3], 195);
    EXPECT_EQ(addr.ipv4_port, 443u);
    EXPECT_EQ(addr.ipv6_port, 8443u);
    EXPECT_EQ(addr.connection_id.length(), 5u);
}

TEST_F(TransportParamsDecodePreferredAddressTest, StatelessResetTokenPreserved)
{
    auto payload = make_preferred_address_payload(/*cid_len=*/0);
    auto data = make_param(quic::transport_param_id::preferred_address, as_span(payload));
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    const auto& addr = *result.value().preferred_address;
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(addr.stateless_reset_token[i], static_cast<uint8_t>(0xF0 + i));
    }
}

// ============================================================================
// Fixture: Decode unknown parameter placement
// ============================================================================

class TransportParamsDecodeUnknownParamTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownAtStartThenKnownParsed)
{
    auto unknown = make_varint_param(/*id=*/0x3FFF, 42);  // 0x3FFF is not a defined param
    auto known = make_varint_param(quic::transport_param_id::initial_max_data, 1024);
    auto data = concat({unknown, known});
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().initial_max_data, 1024u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownAtEndAfterKnownParsed)
{
    auto known = make_varint_param(quic::transport_param_id::max_idle_timeout, 5000);
    auto unknown = make_varint_param(/*id=*/0x2FFF, 99);
    auto data = concat({known, unknown});
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().max_idle_timeout, 5000u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownBetweenKnownsBothParsed)
{
    auto known_a = make_varint_param(quic::transport_param_id::max_idle_timeout, 1000);
    auto unknown = make_varint_param(/*id=*/0x1234, 77);
    auto known_b = make_varint_param(quic::transport_param_id::initial_max_data, 2048);
    auto data = concat({known_a, unknown, known_b});
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().max_idle_timeout, 1000u);
    EXPECT_EQ(result.value().initial_max_data, 2048u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, MultipleUnknownsIgnored)
{
    auto unknown1 = make_varint_param(/*id=*/0x1111, 1);
    auto unknown2 = make_varint_param(/*id=*/0x2222, 2);
    auto unknown3 = make_empty_param(/*id=*/0x3333);
    auto data = concat({unknown1, unknown2, unknown3});
    auto result = quic::transport_parameters::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    // No known params set → defaults preserved
    EXPECT_EQ(result.value().max_idle_timeout, 0u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownWithNonEmptyArbitraryValue)
{
    std::vector<uint8_t> arbitrary_value = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
    auto unknown = make_param(/*id=*/0x4444, as_span(arbitrary_value));
    auto result = quic::transport_parameters::decode(as_span(unknown));
    ASSERT_TRUE(result.is_ok());
}

// ============================================================================
// Fixture: Decode varint ID read failures
// ============================================================================

class TransportParamsDecodeVarintIdTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeVarintIdTest, TruncatedParamIdFails)
{
    // Start a 2-byte varint ID (prefix 0x40) but provide only 1 byte.
    std::vector<uint8_t> data = {0x40};
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeVarintIdTest, TruncatedParamIdFourByteForm)
{
    // Prefix 0x80 indicates 4-byte varint but provide only 3 bytes.
    std::vector<uint8_t> data = {0x80, 0x00, 0x00};
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeVarintIdTest, TruncatedParamLengthAfterValidId)
{
    // Valid 1-byte ID (0x04 = initial_max_data), then start of 2-byte length
    // varint (0x40) with no second byte.
    std::vector<uint8_t> data = {0x04, 0x40};
    auto result = quic::transport_parameters::decode(as_span(data));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Fixture: Validate boundary values
// ============================================================================

class TransportParamsValidateBoundaryTest : public ::testing::Test
{
};

TEST_F(TransportParamsValidateBoundaryTest, AckDelayExponentExactly20PassesValidate)
{
    quic::transport_parameters params;
    params.ack_delay_exponent = 20;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_ok());
}

TEST_F(TransportParamsValidateBoundaryTest, AckDelayExponent21FailsValidate)
{
    quic::transport_parameters params;
    params.ack_delay_exponent = 21;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, MaxAckDelayExactly16383PassesValidate)
{
    quic::transport_parameters params;
    params.max_ack_delay = 16383;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_ok());
}

TEST_F(TransportParamsValidateBoundaryTest, MaxAckDelay16384FailsValidate)
{
    quic::transport_parameters params;
    params.max_ack_delay = 16384;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, MaxUdpPayloadExactly1200PassesValidate)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 1200;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_ok());
}

TEST_F(TransportParamsValidateBoundaryTest, MaxUdpPayload1199FailsValidate)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 1199;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ActiveConnectionIdLimitExactly2PassesValidate)
{
    quic::transport_parameters params;
    params.active_connection_id_limit = 2;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_ok());
}

TEST_F(TransportParamsValidateBoundaryTest, ActiveConnectionIdLimit1FailsValidate)
{
    quic::transport_parameters params;
    params.active_connection_id_limit = 1;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ServerValidationAcceptsAllServerOnlyFields)
{
    quic::transport_parameters params;
    std::array<uint8_t, 4> cid_bytes = {1, 2, 3, 4};
    params.original_destination_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    params.retry_source_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    std::array<uint8_t, 16> token{};
    params.stateless_reset_token = token;
    params.preferred_address = quic::preferred_address_info{};
    EXPECT_TRUE(params.validate(/*is_server=*/true).is_ok());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientValidationRejectsOriginalDestCidOnly)
{
    quic::transport_parameters params;
    std::array<uint8_t, 4> cid_bytes = {1, 2, 3, 4};
    params.original_destination_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientValidationRejectsRetrySourceCidOnly)
{
    quic::transport_parameters params;
    std::array<uint8_t, 4> cid_bytes = {1, 2, 3, 4};
    params.retry_source_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientValidationRejectsStatelessResetTokenOnly)
{
    quic::transport_parameters params;
    std::array<uint8_t, 16> token{};
    params.stateless_reset_token = token;
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientValidationRejectsPreferredAddressOnly)
{
    quic::transport_parameters params;
    params.preferred_address = quic::preferred_address_info{};
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

// ============================================================================
// Fixture: apply_defaults partial zero override
// ============================================================================

class TransportParamsApplyDefaultsPartialTest : public ::testing::Test
{
};

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyMaxUdpPayloadZeroOverridden)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 0;
    params.ack_delay_exponent = 7;   // non-zero preserved
    params.max_ack_delay = 100;      // non-zero preserved
    params.active_connection_id_limit = 4;  // non-zero preserved
    params.apply_defaults();
    EXPECT_EQ(params.max_udp_payload_size, 65527u);
    EXPECT_EQ(params.ack_delay_exponent, 7u);
    EXPECT_EQ(params.max_ack_delay, 100u);
    EXPECT_EQ(params.active_connection_id_limit, 4u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyAckDelayExponentZeroOverridden)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 2000;
    params.ack_delay_exponent = 0;
    params.max_ack_delay = 50;
    params.active_connection_id_limit = 5;
    params.apply_defaults();
    EXPECT_EQ(params.max_udp_payload_size, 2000u);
    EXPECT_EQ(params.ack_delay_exponent, 3u);
    EXPECT_EQ(params.max_ack_delay, 50u);
    EXPECT_EQ(params.active_connection_id_limit, 5u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyMaxAckDelayZeroOverridden)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 1500;
    params.ack_delay_exponent = 6;
    params.max_ack_delay = 0;
    params.active_connection_id_limit = 3;
    params.apply_defaults();
    EXPECT_EQ(params.max_udp_payload_size, 1500u);
    EXPECT_EQ(params.ack_delay_exponent, 6u);
    EXPECT_EQ(params.max_ack_delay, 25u);
    EXPECT_EQ(params.active_connection_id_limit, 3u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyActiveConnectionIdLimitZeroOverridden)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 1500;
    params.ack_delay_exponent = 6;
    params.max_ack_delay = 50;
    params.active_connection_id_limit = 0;
    params.apply_defaults();
    EXPECT_EQ(params.max_udp_payload_size, 1500u);
    EXPECT_EQ(params.ack_delay_exponent, 6u);
    EXPECT_EQ(params.max_ack_delay, 50u);
    EXPECT_EQ(params.active_connection_id_limit, 2u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, IdempotentAfterSecondCall)
{
    quic::transport_parameters params;
    params.max_udp_payload_size = 0;
    params.apply_defaults();
    auto once = params;
    params.apply_defaults();
    EXPECT_EQ(params, once);
}

// ============================================================================
// Fixture: Factory validation pairing
// ============================================================================

class TransportParamsFactoryValidationTest : public ::testing::Test
{
};

TEST_F(TransportParamsFactoryValidationTest, ClientFactoryPassesClientValidate)
{
    auto params = quic::make_default_client_params();
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ClientFactoryPassesServerValidate)
{
    // Client factory sets no server-only fields, so it's also valid as server
    auto params = quic::make_default_client_params();
    EXPECT_TRUE(params.validate(/*is_server=*/true).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ServerFactoryPassesServerValidate)
{
    auto params = quic::make_default_server_params();
    EXPECT_TRUE(params.validate(/*is_server=*/true).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ServerFactoryPassesClientValidateWhenNoServerOnlyFields)
{
    // Server factory as written sets no server-only optional fields, so it
    // passes client validation too. This guards against future regressions that
    // accidentally attach server-only fields.
    auto params = quic::make_default_server_params();
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ServerFactoryWithOriginalDestCidFailsClientValidate)
{
    auto params = quic::make_default_server_params();
    std::array<uint8_t, 4> cid_bytes = {1, 2, 3, 4};
    params.original_destination_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    EXPECT_TRUE(params.validate(/*is_server=*/false).is_err());
}

TEST_F(TransportParamsFactoryValidationTest, ClientAndServerFactoriesProduceEquivalentBaseFields)
{
    auto client = quic::make_default_client_params();
    auto server = quic::make_default_server_params();
    EXPECT_EQ(client.max_idle_timeout, server.max_idle_timeout);
    EXPECT_EQ(client.max_udp_payload_size, server.max_udp_payload_size);
    EXPECT_EQ(client.initial_max_data, server.initial_max_data);
    EXPECT_EQ(client.initial_max_streams_bidi, server.initial_max_streams_bidi);
    EXPECT_EQ(client.active_connection_id_limit, server.active_connection_id_limit);
}

// ============================================================================
// Fixture: Exotic round-trip scenarios
// ============================================================================

class TransportParamsRoundTripExoticTest : public ::testing::Test
{
};

TEST_F(TransportParamsRoundTripExoticTest, MaxLengthConnectionIdsRoundTrip)
{
    quic::transport_parameters params;
    std::array<uint8_t, 20> cid_bytes{};
    for (int i = 0; i < 20; ++i) cid_bytes[i] = static_cast<uint8_t>(i);
    params.original_destination_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    params.initial_source_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    params.retry_source_connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));

    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value(), params);
}

TEST_F(TransportParamsRoundTripExoticTest, EmptyConnectionIdsRoundTrip)
{
    quic::transport_parameters params;
    params.original_destination_connection_id = quic::connection_id();
    params.initial_source_connection_id = quic::connection_id();
    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value(), params);
}

TEST_F(TransportParamsRoundTripExoticTest, PreferredAddressWithEmptyCidRoundTrips)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    addr.ipv4_address = {127, 0, 0, 1};
    addr.ipv4_port = 443;
    // Empty CID via default ctor
    for (int i = 0; i < 16; ++i) addr.stateless_reset_token[i] = static_cast<uint8_t>(i);
    params.preferred_address = addr;

    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    ASSERT_TRUE(decoded.value().preferred_address.has_value());
    EXPECT_TRUE(decoded.value().preferred_address->connection_id.empty());
    EXPECT_EQ(decoded.value().preferred_address->ipv4_port, 443u);
}

TEST_F(TransportParamsRoundTripExoticTest, PreferredAddressWithMaxCidRoundTrips)
{
    quic::transport_parameters params;
    quic::preferred_address_info addr;
    std::array<uint8_t, 20> cid_bytes{};
    for (int i = 0; i < 20; ++i) cid_bytes[i] = static_cast<uint8_t>(0xB0 + i);
    addr.connection_id = quic::connection_id(
        std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
    for (int i = 0; i < 16; ++i) addr.stateless_reset_token[i] = static_cast<uint8_t>(0xE0 + i);
    params.preferred_address = addr;

    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value().preferred_address->connection_id.length(), 20u);
}

TEST_F(TransportParamsRoundTripExoticTest, LargeVarintValuesRoundTrip)
{
    quic::transport_parameters params;
    params.initial_max_data = 1073741824;       // 4-byte varint range boundary + 1
    params.initial_max_stream_data_bidi_local = 4611686018427387903ULL;  // max varint
    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value().initial_max_data, 1073741824u);
    EXPECT_EQ(decoded.value().initial_max_stream_data_bidi_local, 4611686018427387903ULL);
}

TEST_F(TransportParamsRoundTripExoticTest, AllVarintFieldsSetRoundTrip)
{
    quic::transport_parameters params;
    params.max_idle_timeout = 60000;
    params.ack_delay_exponent = 4;
    params.max_ack_delay = 75;
    params.max_udp_payload_size = 1400;
    params.initial_max_data = 524288;
    params.initial_max_stream_data_bidi_local = 65536;
    params.initial_max_stream_data_bidi_remote = 131072;
    params.initial_max_stream_data_uni = 32768;
    params.initial_max_streams_bidi = 50;
    params.initial_max_streams_uni = 25;
    params.active_connection_id_limit = 4;
    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value(), params);
}

TEST_F(TransportParamsRoundTripExoticTest, DisableActiveMigrationWithOtherFieldsRoundTrips)
{
    quic::transport_parameters params;
    params.disable_active_migration = true;
    params.max_idle_timeout = 10000;
    params.initial_max_streams_bidi = 10;
    auto encoded = params.encode();
    auto decoded = quic::transport_parameters::decode(as_span(encoded));
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value(), params);
}

TEST_F(TransportParamsRoundTripExoticTest, EncodeDecodeDecodedParamsIdempotent)
{
    quic::transport_parameters original;
    original.max_idle_timeout = 20000;
    original.initial_max_data = 1000;
    auto first = original.encode();
    auto decoded_result = quic::transport_parameters::decode(as_span(first));
    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();
    auto second = decoded.encode();
    EXPECT_EQ(first, second);
}
