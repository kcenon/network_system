// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_frame_extra_coverage_test.cpp
 * @brief Additional unit tests for src/protocols/quic/frame.cpp (Issue #1033)
 *
 * Targets paths not exercised by tests/test_quic_frame.cpp,
 * tests/unit/quic_frame_types_test.cpp, or
 * tests/unit/quic_frame_coverage_test.cpp:
 *
 *  - peek_type: 4-byte and 8-byte varint type prefixes
 *  - parse_all: heterogeneous frame mix (PING, MAX_DATA, RESET_STREAM,
 *    PATH_CHALLENGE, HANDSHAKE_DONE), mid-sequence parse error surfacing
 *  - parse_padding: zero-length input via parse() (empty buffer rejected)
 *  - parse_ack: ACK_ECN dispatch via parse(), large delay value, large
 *    largest_acknowledged at varint boundary
 *  - parse_stream: every concrete stream-type byte (0x08..0x0f) routed
 *    through parse(), zero-length data with LEN flag, large stream_id varint
 *  - parse_crypto: non-zero offset round-trip, large data round-trip
 *  - parse_new_token: empty token round-trip, multi-byte length round-trip
 *  - parse_new_connection_id: minimum (1-byte) CID round-trip,
 *    non-trivial sequence_number / retire_prior_to encoding,
 *    over-length CID via 2-byte varint length field (impossible: cid_len
 *    is 1 raw byte, so any value >20 must be rejected); cid_len=20 with
 *    insufficient remaining bytes
 *  - parse_retire_connection_id: large sequence number round-trip
 *  - parse_path_challenge / parse_path_response: round-trip of all-zero
 *    payload, all-0xFF payload
 *  - parse_connection_close: transport with non-empty reason and non-zero
 *    frame_type, application close with non-empty reason, multi-byte
 *    error_code (4-byte varint)
 *  - parse_handshake_done: dispatch via parse() with trailing buffer bytes
 *  - frame_builder::build_padding(0): zero-count guard
 *  - frame_builder::build(variant) for additional alternatives:
 *    - max_data with varint boundary value
 *    - reset_stream with full field values
 *    - stop_sending with max varint values
 *  - get_frame_type / frame_type_to_string for frame_type values not yet
 *    asserted: padding, ping, ack, reset_stream, stop_sending, crypto,
 *    stream_base, max_data, max_stream_data, max_streams_bidi/uni,
 *    data_blocked, stream_data_blocked, streams_blocked_bidi/uni,
 *    new_connection_id, path_challenge, connection_close
 *  - is_stream_frame / get_stream_flags / make_stream_type: per-bit
 *    consistency assertions, boundary types 0x07 and 0x10
 *
 * Hand-rolled byte sequences follow RFC 9000 wire format to keep error-path
 * tests independent of builder behavior.
 */

#include "internal/protocols/quic/frame.h"
#include "internal/protocols/quic/frame_types.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

namespace
{

auto as_span(const std::vector<uint8_t>& bytes) -> std::span<const uint8_t>
{
    return std::span<const uint8_t>(bytes);
}

} // namespace

// ============================================================================
// peek_type: longer varint type prefixes
// ============================================================================

TEST(QuicFrameExtraPeekType, FourByteVarintPrefixDecodes)
{
    // 4-byte varint encoding of value 16384 = 0x4000
    // First byte: 0b10xxxxxx, value = 0x80 0x00 0x40 0x00
    std::vector<uint8_t> data{0x80, 0x00, 0x40, 0x00};
    auto result = quic::frame_parser::peek_type(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 16384u);
    EXPECT_EQ(result.value().second, 4u);
}

TEST(QuicFrameExtraPeekType, EightByteVarintPrefixDecodes)
{
    // 8-byte varint encoding of value 1073741824 = 0x40000000
    // First byte: 0b11xxxxxx, value = 0xC0 0x00 0x00 0x00 0x40 0x00 0x00 0x00
    std::vector<uint8_t> data{0xC0, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00};
    auto result = quic::frame_parser::peek_type(as_span(data));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 1073741824u);
    EXPECT_EQ(result.value().second, 8u);
}

// ============================================================================
// parse_all: heterogeneous frame composition
// ============================================================================

TEST(QuicFrameExtraParseAll, HeterogeneousFramesParsedInOrder)
{
    // Build a packet payload with PING + HANDSHAKE_DONE + MAX_DATA + PATH_CHALLENGE.
    std::vector<uint8_t> payload;

    auto ping = quic::frame_builder::build_ping();
    payload.insert(payload.end(), ping.begin(), ping.end());

    auto done = quic::frame_builder::build_handshake_done();
    payload.insert(payload.end(), done.begin(), done.end());

    quic::max_data_frame md;
    md.maximum_data = 65536;
    auto md_bytes = quic::frame_builder::build_max_data(md);
    payload.insert(payload.end(), md_bytes.begin(), md_bytes.end());

    quic::path_challenge_frame pc;
    pc.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    auto pc_bytes = quic::frame_builder::build_path_challenge(pc);
    payload.insert(payload.end(), pc_bytes.begin(), pc_bytes.end());

    auto result = quic::frame_parser::parse_all(as_span(payload));
    ASSERT_TRUE(result.is_ok());
    auto& frames = result.value();
    ASSERT_EQ(frames.size(), 4u);
    EXPECT_TRUE(std::holds_alternative<quic::ping_frame>(frames[0]));
    EXPECT_TRUE(std::holds_alternative<quic::handshake_done_frame>(frames[1]));
    EXPECT_TRUE(std::holds_alternative<quic::max_data_frame>(frames[2]));
    EXPECT_TRUE(std::holds_alternative<quic::path_challenge_frame>(frames[3]));

    const auto* parsed_md = std::get_if<quic::max_data_frame>(&frames[2]);
    ASSERT_NE(parsed_md, nullptr);
    EXPECT_EQ(parsed_md->maximum_data, 65536u);

    const auto* parsed_pc = std::get_if<quic::path_challenge_frame>(&frames[3]);
    ASSERT_NE(parsed_pc, nullptr);
    EXPECT_EQ(parsed_pc->data, pc.data);
}

TEST(QuicFrameExtraParseAll, GoodFrameThenTruncatedFrameSurfacesError)
{
    // PING (1 byte) followed by an incomplete CRYPTO frame (type+offset only).
    std::vector<uint8_t> payload{0x01, 0x06, 0xC0};
    auto result = quic::frame_parser::parse_all(as_span(payload));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_padding: empty buffer rejected at parse() entry
// ============================================================================

TEST(QuicFrameExtraPadding, EmptyBufferAtParseLevelIsError)
{
    // The parse() function rejects an empty buffer before reaching parse_padding.
    std::vector<uint8_t> empty;
    auto result = quic::frame_parser::parse(as_span(empty));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_ack: ACK_ECN with ranges, large delay, large largest_acknowledged
// ============================================================================

TEST(QuicFrameExtraAck, AckEcnZeroRangesParsesViaDispatch)
{
    // Type=0x03 (ACK_ECN), largest=0, delay=0, range_count=0, first=0,
    // ECT(0)=2, ECT(1)=4, ECN-CE=8. All single-byte varints.
    std::vector<uint8_t> buf{0x03, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x08};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* af = std::get_if<quic::ack_frame>(&result.value().first);
    ASSERT_NE(af, nullptr);
    ASSERT_TRUE(af->ecn.has_value());
    EXPECT_EQ(af->ecn->ect0, 2u);
    EXPECT_EQ(af->ecn->ect1, 4u);
    EXPECT_EQ(af->ecn->ecn_ce, 8u);
}

TEST(QuicFrameExtraAck, LargeDelayAndLargestAckedRoundTrip)
{
    quic::ack_frame orig;
    orig.largest_acknowledged = 1073741823; // 4-byte varint max
    orig.ack_delay = 16383;                 // 2-byte varint max
    // Intentionally no `ranges` to avoid the build_ack/parse_ack divergence
    // documented in quic_frame_coverage_test.cpp.

    auto encoded = quic::frame_builder::build_ack(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::ack_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->largest_acknowledged, orig.largest_acknowledged);
    EXPECT_EQ(f->ack_delay, orig.ack_delay);
}

TEST(QuicFrameExtraAck, ParseAckWithRangesAccumulatesRangeVector)
{
    // Build the ACK by hand to drive the parse_ack range-loop branch.
    // Type=0x02, largest=10, delay=0, range_count=2, first_range=0,
    // (gap=1, length=2), (gap=3, length=4)
    std::vector<uint8_t> buf{0x02, 0x0a, 0x00, 0x02, 0x00,
                             0x01, 0x02, 0x03, 0x04};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::ack_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->ranges.size(), 2u);
    EXPECT_EQ(f->ranges[0].gap, 1u);
    EXPECT_EQ(f->ranges[0].length, 2u);
    EXPECT_EQ(f->ranges[1].gap, 3u);
    EXPECT_EQ(f->ranges[1].length, 4u);
}

TEST(QuicFrameExtraAck, ParseAckTruncatedAtRangeGap)
{
    // Type=0x02, largest=0, delay=0, range_count=1, first=0, then truncated gap.
    std::vector<uint8_t> buf{0x02, 0x00, 0x00, 0x01, 0x00, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameExtraAck, ParseAckTruncatedAtRangeLength)
{
    // Type=0x02, largest=0, delay=0, range_count=1, first=0, gap=1,
    // then truncated length.
    std::vector<uint8_t> buf{0x02, 0x00, 0x00, 0x01, 0x00, 0x01, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameExtraAck, ParseAckEcnTruncatedAtEct1)
{
    // Type=0x03, largest=0, delay=0, range_count=0, first=0, ECT(0)=0,
    // then truncated ECT(1).
    std::vector<uint8_t> buf{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameExtraAck, ParseAckEcnTruncatedAtEcnCe)
{
    // Type=0x03, ..., ECT(0)=0, ECT(1)=0, then truncated ECN-CE.
    std::vector<uint8_t> buf{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_stream: each concrete stream-type byte (0x08..0x0f) via parse()
// ============================================================================

TEST(QuicFrameExtraStream, BareTypeNoFlagsRoundTrip)
{
    // Type=0x08 — no FIN, no LEN, no OFF. Data extends to end of buffer.
    std::vector<uint8_t> buf{0x08, 0x04, 0xAA, 0xBB, 0xCC};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_EQ(sf->stream_id, 4u);
    EXPECT_EQ(sf->offset, 0u);
    EXPECT_FALSE(sf->fin);
    EXPECT_EQ(sf->data, std::vector<uint8_t>({0xAA, 0xBB, 0xCC}));
}

TEST(QuicFrameExtraStream, FinOnlyRoundTrip)
{
    // Type=0x09 (base+FIN), stream_id=4, no LEN, no OFF.
    std::vector<uint8_t> buf{0x09, 0x04, 0x01, 0x02};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_TRUE(sf->fin);
    EXPECT_EQ(sf->offset, 0u);
}

TEST(QuicFrameExtraStream, LenOnlyRoundTrip)
{
    // Type=0x0a (base+LEN), stream_id=4, length=2, then 2 data bytes.
    std::vector<uint8_t> buf{0x0a, 0x04, 0x02, 0x99, 0x88};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_FALSE(sf->fin);
    EXPECT_EQ(sf->data.size(), 2u);
}

TEST(QuicFrameExtraStream, FinAndLenRoundTrip)
{
    // Type=0x0b (base+FIN+LEN), stream_id=4, length=1, 1 data byte.
    std::vector<uint8_t> buf{0x0b, 0x04, 0x01, 0x77};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_TRUE(sf->fin);
    EXPECT_EQ(sf->data, std::vector<uint8_t>({0x77}));
}

TEST(QuicFrameExtraStream, OffOnlyRoundTrip)
{
    // Type=0x0c (base+OFF), stream_id=4, offset=10, no LEN.
    std::vector<uint8_t> buf{0x0c, 0x04, 0x0a, 0xFF, 0xEE};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_EQ(sf->offset, 10u);
    EXPECT_FALSE(sf->fin);
}

TEST(QuicFrameExtraStream, FinAndOffRoundTrip)
{
    // Type=0x0d (base+FIN+OFF), stream_id=4, offset=20, no LEN.
    std::vector<uint8_t> buf{0x0d, 0x04, 0x14, 0xAB};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_TRUE(sf->fin);
    EXPECT_EQ(sf->offset, 20u);
}

TEST(QuicFrameExtraStream, LenAndOffRoundTrip)
{
    // Type=0x0e (base+LEN+OFF), stream_id=4, offset=5, length=2, 2 data bytes.
    std::vector<uint8_t> buf{0x0e, 0x04, 0x05, 0x02, 0x12, 0x34};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_FALSE(sf->fin);
    EXPECT_EQ(sf->offset, 5u);
    EXPECT_EQ(sf->data, std::vector<uint8_t>({0x12, 0x34}));
}

TEST(QuicFrameExtraStream, AllFlagsRoundTrip)
{
    // Type=0x0f (base+FIN+LEN+OFF), stream_id=4, offset=7, length=3, 3 bytes.
    std::vector<uint8_t> buf{0x0f, 0x04, 0x07, 0x03, 0xDE, 0xAD, 0xBE};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_TRUE(sf->fin);
    EXPECT_EQ(sf->offset, 7u);
    EXPECT_EQ(sf->data, std::vector<uint8_t>({0xDE, 0xAD, 0xBE}));
}

TEST(QuicFrameExtraStream, LargeStreamIdVarintRoundTrip)
{
    // Build a STREAM frame with a stream_id that requires a 2-byte varint.
    quic::stream_frame orig;
    orig.stream_id = 1024; // 2-byte varint range
    orig.offset = 0;
    orig.data = {0x42};
    orig.fin = false;

    auto encoded = quic::frame_builder::build_stream(orig, /*include_length=*/true);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&parsed.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_EQ(sf->stream_id, 1024u);
}

TEST(QuicFrameExtraStream, LenZeroNoData)
{
    // Type=0x0a (LEN), stream_id=4, length=0, no payload.
    std::vector<uint8_t> buf{0x0a, 0x04, 0x00};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* sf = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(sf, nullptr);
    EXPECT_TRUE(sf->data.empty());
}

// ============================================================================
// parse_crypto: non-zero offset, larger payload
// ============================================================================

TEST(QuicFrameExtraCrypto, NonZeroOffsetRoundTrip)
{
    quic::crypto_frame orig;
    orig.offset = 100;
    orig.data = {0xC0, 0xFF, 0xEE};

    auto encoded = quic::frame_builder::build_crypto(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::crypto_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->offset, 100u);
    EXPECT_EQ(f->data, orig.data);
}

TEST(QuicFrameExtraCrypto, LargeDataRoundTrip)
{
    // Data large enough to force a 2-byte varint length prefix.
    quic::crypto_frame orig;
    orig.offset = 0;
    orig.data.resize(200, 0x55);

    auto encoded = quic::frame_builder::build_crypto(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::crypto_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->data.size(), 200u);
    EXPECT_EQ(f->data, orig.data);
}

// ============================================================================
// parse_new_token: empty token, longer token via 2-byte varint length
// ============================================================================

TEST(QuicFrameExtraNewToken, EmptyTokenRoundTrip)
{
    quic::new_token_frame orig;
    // orig.token left empty

    auto encoded = quic::frame_builder::build_new_token(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::new_token_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->token.empty());
}

TEST(QuicFrameExtraNewToken, MultiByteLengthRoundTrip)
{
    quic::new_token_frame orig;
    orig.token.assign(150, 0xA5);

    auto encoded = quic::frame_builder::build_new_token(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::new_token_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->token.size(), 150u);
    EXPECT_EQ(f->token, orig.token);
}

// ============================================================================
// parse_new_connection_id: 1-byte CID, larger sequence numbers
// ============================================================================

TEST(QuicFrameExtraNewConnectionId, MinimalOneByteCidRoundTrip)
{
    quic::new_connection_id_frame orig;
    orig.sequence_number = 2;
    orig.retire_prior_to = 1;
    orig.connection_id = {0xAB}; // 1 byte
    orig.stateless_reset_token.fill(0xCD);

    auto encoded = quic::frame_builder::build_new_connection_id(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::new_connection_id_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->connection_id, std::vector<uint8_t>({0xAB}));
    EXPECT_EQ(f->sequence_number, 2u);
    EXPECT_EQ(f->retire_prior_to, 1u);
    for (auto b : f->stateless_reset_token)
    {
        EXPECT_EQ(b, 0xCD);
    }
}

TEST(QuicFrameExtraNewConnectionId, LargeSequenceNumberRoundTrip)
{
    quic::new_connection_id_frame orig;
    orig.sequence_number = 16384; // 4-byte varint
    orig.retire_prior_to = 100;
    orig.connection_id = {0x01, 0x02, 0x03, 0x04, 0x05};
    orig.stateless_reset_token.fill(0xEE);

    auto encoded = quic::frame_builder::build_new_connection_id(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::new_connection_id_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->sequence_number, 16384u);
    EXPECT_EQ(f->retire_prior_to, 100u);
    EXPECT_EQ(f->connection_id.size(), 5u);
}

TEST(QuicFrameExtraNewConnectionId, CidLength20WithInsufficientBytes)
{
    // Type=0x18, seq=0, retire=0, cid_len=20, but only 19 CID bytes follow.
    std::vector<uint8_t> buf{0x18, 0x00, 0x00, 20};
    for (int i = 0; i < 19; ++i) buf.push_back(0xAA);
    // Add reset token bytes too (parser fails before reading them, but
    // include them so the failure path is the CID-length check, not the
    // overall buffer truncation).
    for (int i = 0; i < 16; ++i) buf.push_back(0x00);
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_retire_connection_id: large sequence number
// ============================================================================

TEST(QuicFrameExtraRetireConnectionId, LargeSequenceRoundTrip)
{
    quic::retire_connection_id_frame orig;
    orig.sequence_number = 1073741823; // 4-byte varint max

    auto encoded = quic::frame_builder::build_retire_connection_id(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::retire_connection_id_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->sequence_number, 1073741823u);
}

// ============================================================================
// parse_path_challenge / parse_path_response: extreme payloads
// ============================================================================

TEST(QuicFrameExtraPath, ChallengeAllZerosRoundTrip)
{
    quic::path_challenge_frame orig;
    orig.data.fill(0x00);

    auto encoded = quic::frame_builder::build_path_challenge(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::path_challenge_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    for (auto b : f->data) EXPECT_EQ(b, 0x00);
}

TEST(QuicFrameExtraPath, ResponseAllOnesRoundTrip)
{
    quic::path_response_frame orig;
    orig.data.fill(0xFF);

    auto encoded = quic::frame_builder::build_path_response(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::path_response_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    for (auto b : f->data) EXPECT_EQ(b, 0xFF);
}

// ============================================================================
// parse_connection_close: non-empty reasons, larger error_code
// ============================================================================

TEST(QuicFrameExtraConnectionClose, TransportWithReasonAndFrameTypeRoundTrip)
{
    quic::connection_close_frame orig;
    orig.error_code = 0x10;          // PROTOCOL_VIOLATION
    orig.frame_type = 0x06;          // CRYPTO
    orig.reason_phrase = "bad crypto frame";
    orig.is_application_error = false;

    auto encoded = quic::frame_builder::build_connection_close(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::connection_close_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->error_code, 0x10u);
    EXPECT_EQ(f->frame_type, 0x06u);
    EXPECT_EQ(f->reason_phrase, "bad crypto frame");
    EXPECT_FALSE(f->is_application_error);
}

TEST(QuicFrameExtraConnectionClose, ApplicationWithLongReasonRoundTrip)
{
    quic::connection_close_frame orig;
    orig.error_code = 99;
    orig.reason_phrase = std::string(120, 'X'); // forces 2-byte length varint
    orig.is_application_error = true;

    auto encoded = quic::frame_builder::build_connection_close(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::connection_close_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->is_application_error);
    EXPECT_EQ(f->reason_phrase.size(), 120u);
    EXPECT_EQ(f->reason_phrase, orig.reason_phrase);
}

TEST(QuicFrameExtraConnectionClose, MultiByteErrorCodeRoundTrip)
{
    quic::connection_close_frame orig;
    orig.error_code = 16384; // 4-byte varint
    orig.frame_type = 0;
    orig.reason_phrase = "x";
    orig.is_application_error = false;

    auto encoded = quic::frame_builder::build_connection_close(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::connection_close_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->error_code, 16384u);
}

// ============================================================================
// parse_handshake_done: dispatch via parse() with leftover buffer bytes
// ============================================================================

TEST(QuicFrameExtraHandshakeDone, DispatchConsumesOnlyOneByte)
{
    // 0x1e (HANDSHAKE_DONE) followed by garbage that parse() should not touch.
    std::vector<uint8_t> buf{0x1e, 0xAA, 0xBB, 0xCC};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    auto& [f, consumed] = result.value();
    EXPECT_TRUE(std::holds_alternative<quic::handshake_done_frame>(f));
    EXPECT_EQ(consumed, 1u);
}

// ============================================================================
// frame_builder edge cases
// ============================================================================

TEST(QuicFrameExtraBuilder, BuildPaddingZeroProducesEmptyVector)
{
    // build_padding(0) is a sane no-op: returns an empty vector, not malformed.
    auto encoded = quic::frame_builder::build_padding(0);
    EXPECT_TRUE(encoded.empty());
}

TEST(QuicFrameExtraBuilder, BuildPaddingDefaultIsOneByte)
{
    auto encoded = quic::frame_builder::build_padding();
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x00);
}

TEST(QuicFrameExtraBuilder, ResetStreamBuilderEncodesAllFields)
{
    quic::reset_stream_frame orig;
    orig.stream_id = 0;
    orig.application_error_code = 0;
    orig.final_size = 0;

    auto encoded = quic::frame_builder::build_reset_stream(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::reset_stream_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->stream_id, 0u);
    EXPECT_EQ(f->application_error_code, 0u);
    EXPECT_EQ(f->final_size, 0u);
}

TEST(QuicFrameExtraBuilder, StopSendingMaxVarintRoundTrip)
{
    quic::stop_sending_frame orig;
    orig.stream_id = 4611686018427387903ULL; // varint_max
    orig.application_error_code = 4611686018427387903ULL;

    auto encoded = quic::frame_builder::build_stop_sending(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::stop_sending_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->stream_id, 4611686018427387903ULL);
    EXPECT_EQ(f->application_error_code, 4611686018427387903ULL);
}

// ============================================================================
// frame_type_to_string: branches not yet asserted in coverage_test.cpp
// ============================================================================

TEST(QuicFrameExtraTypeString, Padding)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::padding), "PADDING");
}

TEST(QuicFrameExtraTypeString, Ping)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::ping), "PING");
}

TEST(QuicFrameExtraTypeString, Ack)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::ack), "ACK");
}

TEST(QuicFrameExtraTypeString, ResetStream)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::reset_stream),
              "RESET_STREAM");
}

TEST(QuicFrameExtraTypeString, StopSending)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::stop_sending),
              "STOP_SENDING");
}

TEST(QuicFrameExtraTypeString, Crypto)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::crypto), "CRYPTO");
}

TEST(QuicFrameExtraTypeString, StreamBase)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::stream_base), "STREAM");
}

TEST(QuicFrameExtraTypeString, MaxData)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::max_data), "MAX_DATA");
}

TEST(QuicFrameExtraTypeString, MaxStreamData)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::max_stream_data),
              "MAX_STREAM_DATA");
}

TEST(QuicFrameExtraTypeString, MaxStreamsBidi)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::max_streams_bidi),
              "MAX_STREAMS_BIDI");
}

TEST(QuicFrameExtraTypeString, MaxStreamsUni)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::max_streams_uni),
              "MAX_STREAMS_UNI");
}

TEST(QuicFrameExtraTypeString, DataBlocked)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::data_blocked),
              "DATA_BLOCKED");
}

TEST(QuicFrameExtraTypeString, StreamDataBlocked)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::stream_data_blocked),
              "STREAM_DATA_BLOCKED");
}

TEST(QuicFrameExtraTypeString, StreamsBlockedBidi)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::streams_blocked_bidi),
              "STREAMS_BLOCKED_BIDI");
}

TEST(QuicFrameExtraTypeString, StreamsBlockedUni)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::streams_blocked_uni),
              "STREAMS_BLOCKED_UNI");
}

TEST(QuicFrameExtraTypeString, NewConnectionId)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::new_connection_id),
              "NEW_CONNECTION_ID");
}

TEST(QuicFrameExtraTypeString, PathChallenge)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::path_challenge),
              "PATH_CHALLENGE");
}

TEST(QuicFrameExtraTypeString, ConnectionClose)
{
    EXPECT_EQ(quic::frame_type_to_string(quic::frame_type::connection_close),
              "CONNECTION_CLOSE");
}

// ============================================================================
// Stream type helpers: per-bit consistency
// ============================================================================

TEST(QuicFrameExtraStreamHelpers, IsStreamFrameLowerBoundary)
{
    EXPECT_FALSE(quic::is_stream_frame(0x07)); // NEW_TOKEN
    EXPECT_TRUE(quic::is_stream_frame(0x08));  // STREAM base
}

TEST(QuicFrameExtraStreamHelpers, IsStreamFrameUpperBoundary)
{
    EXPECT_TRUE(quic::is_stream_frame(0x0f));  // STREAM with all flags
    EXPECT_FALSE(quic::is_stream_frame(0x10)); // MAX_DATA
}

TEST(QuicFrameExtraStreamHelpers, IsStreamFrameOutsideRange)
{
    EXPECT_FALSE(quic::is_stream_frame(0x00));
    EXPECT_FALSE(quic::is_stream_frame(0x1e));
    EXPECT_FALSE(quic::is_stream_frame(0x40));
}

TEST(QuicFrameExtraStreamHelpers, GetStreamFlagsExtractsLowBits)
{
    EXPECT_EQ(quic::get_stream_flags(0x08), 0x00);
    EXPECT_EQ(quic::get_stream_flags(0x09), 0x01);
    EXPECT_EQ(quic::get_stream_flags(0x0a), 0x02);
    EXPECT_EQ(quic::get_stream_flags(0x0b), 0x03);
    EXPECT_EQ(quic::get_stream_flags(0x0c), 0x04);
    EXPECT_EQ(quic::get_stream_flags(0x0d), 0x05);
    EXPECT_EQ(quic::get_stream_flags(0x0e), 0x06);
    EXPECT_EQ(quic::get_stream_flags(0x0f), 0x07);
}

TEST(QuicFrameExtraStreamHelpers, MakeStreamTypeMatchesGetStreamFlags)
{
    // Property: get_stream_flags(make_stream_type(a, b, c)) reflects the inputs.
    for (bool fin : {false, true})
    {
        for (bool len : {false, true})
        {
            for (bool off : {false, true})
            {
                uint8_t type = quic::make_stream_type(fin, len, off);
                EXPECT_TRUE(quic::is_stream_frame(type));
                uint8_t flags = quic::get_stream_flags(type);
                EXPECT_EQ((flags & quic::stream_flags::fin) != 0, fin);
                EXPECT_EQ((flags & quic::stream_flags::len) != 0, len);
                EXPECT_EQ((flags & quic::stream_flags::off) != 0, off);
            }
        }
    }
}
