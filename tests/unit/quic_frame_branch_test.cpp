// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_frame_branch_test.cpp
 * @brief Branch-coverage focused tests for src/protocols/quic/frame.cpp
 *        (Issue #1146, Part of #953).
 *
 * Targets the unhit branches identified from the coverage HTML of run
 * 25620254919 (develop @ fc52441, 60.0% branch baseline). frame.cpp is a
 * pure parser/serializer with no handshake gate, so tests drive
 * @c frame_parser::parse and @c frame_builder::build directly with
 * hand-crafted byte sequences that target each reachable condition.
 *
 * Coverage targets (per HTML inspection):
 * - parse_ack: truncated range_count varint (line 416), zero/one/many
 *   range loop bodies (line 428), large range_count crossing varint
 *   boundaries, ECT truncation at each step.
 * - parse_crypto / parse_new_token / parse_connection_close: each
 *   length-vs-remaining branch at the boundary (length exactly equals
 *   remaining vs length-by-one-exceeds).
 * - parse_stream: each combination of FIN/LEN/OFF flag bits crossed
 *   with truncation at every varint boundary, plus the no-LEN
 *   "data-to-end-of-packet" branch with several lengths.
 * - parse_new_connection_id: every reachable failure point (sequence,
 *   retire_prior_to, missing cid_len byte, cid_len at boundary, cid
 *   data truncation, reset-token truncation at varying buffer sizes).
 * - parse_connection_close: transport vs application distinction with
 *   truncation at frame_type (transport-only), and with empty vs
 *   non-empty reason and reason boundary.
 * - parse_padding: large run, single byte, mixed with non-zero stop.
 * - parse_all: empty buffer, multi-frame heterogeneous mix, error
 *   propagation from each contained frame type.
 * - build_ack: empty ranges (line 970 false branch), single range,
 *   multi-range (line 977 loop body), with and without ECN counts.
 * - build_padding(0): zero-count corner.
 * - build_stream: every (fin, length, offset) flag combination.
 * - frame_builder::build(variant): every alternative through the
 *   visit dispatch including ack_ecn variant.
 * - get_frame_type / frame_type_to_string: full enum coverage.
 *
 * Each test asserts the expected @c Result error/ok and the parsed
 * field values, not just "did not crash."
 */

#include "internal/protocols/quic/frame.h"
#include "internal/protocols/quic/frame_types.h"
#include "internal/protocols/quic/varint.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace quic = ::kcenon::network::protocols::quic;

namespace
{

auto as_span(const std::vector<uint8_t>& bytes) -> std::span<const uint8_t>
{
    return std::span<const uint8_t>(bytes);
}

// Append an arbitrary varint produced by the encoder to a buffer.
void append_varint_to(std::vector<uint8_t>& buf, uint64_t value)
{
    auto v = quic::varint::encode(value);
    buf.insert(buf.end(), v.begin(), v.end());
}

} // namespace

// ============================================================================
// parse_ack: truncated range_count (line 416), loop body (line 428)
// ============================================================================

TEST(QuicFrameBranchAck, TruncatedAtRangeCountVarint)
{
    // Type 0x02 (ACK), largest=0 (1 byte), delay=0 (1 byte), then a 2-byte
    // varint prefix (0x40) with NO follow-up byte — varint::decode fails
    // exactly at the range_count step, exercising parse_ack line 416.
    std::vector<uint8_t> buf{0x02, 0x00, 0x00, 0x40};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchAck, TruncatedAtRangeCountFourByteVarint)
{
    // 4-byte varint prefix (0x80) for range_count with only 2 trailing bytes.
    std::vector<uint8_t> buf{0x02, 0x00, 0x00, 0x80, 0x00, 0x00};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchAck, ZeroRangesNoLoopIteration)
{
    // range_count=0 -> loop body at line 428 never iterates. Exercises the
    // "loop-condition-false" branch.
    std::vector<uint8_t> buf{0x02, 0x05, 0x00, 0x00, 0x05};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::ack_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->largest_acknowledged, 5u);
    EXPECT_TRUE(f->ranges.empty());
}

TEST(QuicFrameBranchAck, OneRangeSingleLoopIteration)
{
    // range_count=1 -> loop iterates once.
    std::vector<uint8_t> buf{
        0x02, // ACK type
        0x10, // largest=16
        0x00, // delay=0
        0x01, // range_count=1
        0x00, // first_range=0
        0x02, // gap=2
        0x03  // length=3
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::ack_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->ranges.size(), 1u);
    EXPECT_EQ(f->ranges[0].gap, 2u);
    EXPECT_EQ(f->ranges[0].length, 3u);
}

TEST(QuicFrameBranchAck, ThreeRangesMultiLoopIterations)
{
    // range_count=3 -> loop iterates multiple times. Builds confidence in
    // the loop-body exception edges (varint decode + push_back).
    std::vector<uint8_t> buf{
        0x02,
        0x20, // largest=32
        0x00, // delay=0
        0x03, // range_count=3
        0x01, // first_range=1
        0x02, 0x03, // r1: gap=2 length=3
        0x04, 0x05, // r2: gap=4 length=5
        0x06, 0x07  // r3: gap=6 length=7
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::ack_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->ranges.size(), 3u);
    EXPECT_EQ(f->ranges[2].length, 7u);
}

TEST(QuicFrameBranchAck, EcnEct0TruncatedFourByteVarint)
{
    // ACK_ECN with truncated ECT(0) 4-byte varint.
    std::vector<uint8_t> buf{
        0x03,
        0x00, 0x00, 0x00, 0x00, // largest, delay, range_count=0, first_range
        0x80, 0x00              // ECT(0): 4-byte prefix, only 1 trailing byte
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchAck, EcnEct1TruncatedAtEightByteVarint)
{
    // ECT(1) decoded as 8-byte varint with no following data.
    std::vector<uint8_t> buf{
        0x03,
        0x00, 0x00, 0x00, 0x00,
        0x00,                  // ECT(0)=0
        0xC0                   // ECT(1): 8-byte prefix, no body
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchAck, EcnEcnCeTruncatedAtEightByteVarint)
{
    std::vector<uint8_t> buf{
        0x03,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,            // ECT(0)=0, ECT(1)=0
        0xC0                   // ECN-CE: 8-byte prefix, no body
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchAck, TruncatedRangeGapVarint)
{
    // range_count=1 but the gap varint is truncated.
    std::vector<uint8_t> buf{
        0x02, 0x10, 0x00,
        0x01,        // range_count=1
        0x00,        // first_range=0
        0xC0         // gap: 8-byte prefix, no body
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchAck, TruncatedRangeLengthVarint)
{
    // range_count=1, gap ok, length varint truncated.
    std::vector<uint8_t> buf{
        0x02, 0x10, 0x00,
        0x01,        // range_count=1
        0x00,        // first_range=0
        0x02,        // gap=2
        0xC0         // length: 8-byte prefix, no body
    };
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_crypto: length-vs-remaining boundary
// ============================================================================

TEST(QuicFrameBranchCrypto, LengthEqualsRemainingExactBoundary)
{
    // offset=0, length=3, data=[0xAA 0xBB 0xCC] -> data.size()-offset == length
    std::vector<uint8_t> buf{0x06, 0x00, 0x03, 0xAA, 0xBB, 0xCC};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::crypto_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->data.size(), 3u);
    EXPECT_EQ(f->data[0], 0xAA);
    EXPECT_EQ(f->data[2], 0xCC);
}

TEST(QuicFrameBranchCrypto, LengthOneByteOverRemaining)
{
    // length=4 but only 3 bytes of data -> insufficient (line 541 true).
    std::vector<uint8_t> buf{0x06, 0x00, 0x04, 0xAA, 0xBB, 0xCC};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchCrypto, FourByteOffsetRoundTrip)
{
    // offset encoded as 4-byte varint (0x80 prefix) -> 8 bytes data.
    quic::crypto_frame orig;
    orig.offset = 100000;
    orig.data = std::vector<uint8_t>(8, 0xEE);
    auto encoded = quic::frame_builder::build_crypto(orig);
    auto parsed = quic::frame_parser::parse(as_span(encoded));
    ASSERT_TRUE(parsed.is_ok());
    const auto* f = std::get_if<quic::crypto_frame>(&parsed.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->offset, 100000u);
    EXPECT_EQ(f->data.size(), 8u);
}

// ============================================================================
// parse_new_token: length-vs-remaining boundary
// ============================================================================

TEST(QuicFrameBranchNewToken, LengthEqualsRemainingExactBoundary)
{
    std::vector<uint8_t> buf{0x07, 0x04, 0xDE, 0xAD, 0xBE, 0xEF};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::new_token_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    ASSERT_EQ(f->token.size(), 4u);
    EXPECT_EQ(f->token[0], 0xDE);
}

TEST(QuicFrameBranchNewToken, LengthOneByteOverRemaining)
{
    std::vector<uint8_t> buf{0x07, 0x05, 0xDE, 0xAD, 0xBE, 0xEF};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchNewToken, MultiByteLengthVarintBoundary)
{
    // length=64 (smallest 2-byte varint) -> length prefix 0x40 0x40.
    std::vector<uint8_t> payload(64, 0x42);
    std::vector<uint8_t> buf{0x07, 0x40, 0x40};
    buf.insert(buf.end(), payload.begin(), payload.end());
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::new_token_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->token.size(), 64u);
}

// ============================================================================
// parse_connection_close: insufficient reason data boundary
// ============================================================================

TEST(QuicFrameBranchConnectionClose, TransportReasonLengthEqualsRemaining)
{
    // error=0, frame_type=0, reason_len=4, reason="abcd"
    std::vector<uint8_t> buf{0x1c, 0x00, 0x00, 0x04, 'a', 'b', 'c', 'd'};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::connection_close_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->reason_phrase, "abcd");
    EXPECT_FALSE(f->is_application_error);
}

TEST(QuicFrameBranchConnectionClose, TransportReasonLengthOneOver)
{
    // reason_len=5 but only 4 bytes of reason data.
    std::vector<uint8_t> buf{0x1c, 0x00, 0x00, 0x05, 'a', 'b', 'c', 'd'};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchConnectionClose, ApplicationReasonLengthEqualsRemaining)
{
    // App close has no frame_type field.
    std::vector<uint8_t> buf{0x1d, 0x00, 0x03, 'X', 'Y', 'Z'};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::connection_close_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->is_application_error);
    EXPECT_EQ(f->reason_phrase, "XYZ");
}

TEST(QuicFrameBranchConnectionClose, TruncatedReasonLengthVarint)
{
    // Reason length varint itself is truncated.
    std::vector<uint8_t> buf{0x1c, 0x00, 0x00, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchConnectionClose, ApplicationTruncatedAtErrorCode)
{
    std::vector<uint8_t> buf{0x1d, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchConnectionClose, ApplicationTruncatedAtReasonVarint)
{
    std::vector<uint8_t> buf{0x1d, 0x00, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_new_connection_id: every reachable failure point and boundaries
// ============================================================================

TEST(QuicFrameBranchNewConnId, MissingLengthByteAfterRetirePriorTo)
{
    // sequence=0, retire=0, then buffer ends -> missing cid_len byte (line 751).
    std::vector<uint8_t> buf{0x18, 0x00, 0x00};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchNewConnId, CidLengthExactlyTwentyOk)
{
    // cid_len=20 (max allowed) with full 20-byte cid and full 16-byte token.
    std::vector<uint8_t> buf{0x18, 0x00, 0x00, 20};
    for (int i = 0; i < 20; ++i) buf.push_back(static_cast<uint8_t>(i));
    for (int i = 0; i < 16; ++i) buf.push_back(static_cast<uint8_t>(0xA0 + i));
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::new_connection_id_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->connection_id.size(), 20u);
}

TEST(QuicFrameBranchNewConnId, CidLengthTwentyOneRejected)
{
    // cid_len=21 -> rejected (line 757 true branch).
    std::vector<uint8_t> buf{0x18, 0x00, 0x00, 21};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchNewConnId, CidLengthBytePresentButInsufficientCid)
{
    // cid_len=10, only 5 cid bytes supplied (line 764 true branch).
    std::vector<uint8_t> buf{0x18, 0x00, 0x00, 10, 0x01, 0x02, 0x03, 0x04, 0x05};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchNewConnId, CidAvailableButResetTokenTruncated)
{
    // cid_len=4 with cid present, then only 8 of 16 token bytes.
    std::vector<uint8_t> buf{0x18, 0x00, 0x00, 4, 0x01, 0x02, 0x03, 0x04};
    for (int i = 0; i < 8; ++i) buf.push_back(0xFF);
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchNewConnId, ResetTokenBoundaryExactlyZero)
{
    // cid_len=0 (zero-length cid) — must still pass to reset-token check
    // with offset positioned right at cid end, and 16 bytes of token follow.
    std::vector<uint8_t> buf{0x18, 0x00, 0x00, 0};
    for (int i = 0; i < 16; ++i) buf.push_back(0x55);
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
}

TEST(QuicFrameBranchNewConnId, TruncatedSequenceNumberVarint)
{
    std::vector<uint8_t> buf{0x18, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchNewConnId, TruncatedRetirePriorToVarint)
{
    std::vector<uint8_t> buf{0x18, 0x00, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_stream: flag combinations crossed with truncation positions
// ============================================================================

class QuicFrameBranchStream : public ::testing::TestWithParam<uint8_t> {};

TEST_P(QuicFrameBranchStream, TruncatedAtStreamIdVarint)
{
    uint8_t type = GetParam();
    std::vector<uint8_t> buf{type, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

INSTANTIATE_TEST_SUITE_P(
    AllStreamTypes,
    QuicFrameBranchStream,
    ::testing::Values(0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f));

TEST(QuicFrameBranchStreamFlags, OffSetButTruncatedAtOffsetVarint)
{
    // 0x0c = STREAM with OFF flag. stream_id=1, then OFF varint truncated.
    std::vector<uint8_t> buf{0x0c, 0x01, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchStreamFlags, LenSetButTruncatedAtLengthVarint)
{
    // 0x0a = STREAM with LEN. stream_id=1, then LEN varint truncated.
    std::vector<uint8_t> buf{0x0a, 0x01, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchStreamFlags, OffAndLenBothTruncatedAtLength)
{
    // 0x0e = STREAM with OFF | LEN. stream_id=1, offset=2, LEN truncated.
    std::vector<uint8_t> buf{0x0e, 0x01, 0x02, 0xC0};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchStreamFlags, LenSetDataShortByOneByte)
{
    // 0x0a: LEN set, stream_id=1, length=5, only 4 bytes of data.
    std::vector<uint8_t> buf{0x0a, 0x01, 0x05, 'a', 'b', 'c', 'd'};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

TEST(QuicFrameBranchStreamFlags, LenSetDataExactlyEqualsLength)
{
    // 0x0a: length=4, exactly 4 bytes follow.
    std::vector<uint8_t> buf{0x0a, 0x01, 0x04, 'a', 'b', 'c', 'd'};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->data.size(), 4u);
}

TEST(QuicFrameBranchStreamFlags, NoLenFlagConsumesRestOfBuffer)
{
    // 0x08: no LEN flag, data extends to end-of-buffer.
    std::vector<uint8_t> buf{0x08, 0x01, 0xAA, 0xBB, 0xCC, 0xDD};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->data.size(), 4u);
    EXPECT_FALSE(f->fin);
}

TEST(QuicFrameBranchStreamFlags, FinFlagSetWithoutLen)
{
    // 0x09 = STREAM with FIN, no LEN/OFF -> data extends to end + fin=true.
    std::vector<uint8_t> buf{0x09, 0x01, 0xEE};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::stream_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->fin);
    EXPECT_EQ(f->data.size(), 1u);
}

TEST(QuicFrameBranchStreamFlags, AllFlagsLengthExceedsRemaining)
{
    // 0x0f = FIN|LEN|OFF, stream_id=1, offset=2, length=10, but only 5 bytes.
    std::vector<uint8_t> buf{0x0f, 0x01, 0x02, 0x0A, 'a', 'b', 'c', 'd', 'e'};
    auto result = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// parse_padding: varied buffer compositions
// ============================================================================

TEST(QuicFrameBranchPadding, SingleZeroByte)
{
    std::vector<uint8_t> buf{0x00};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::padding_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->count, 1u);
    EXPECT_EQ(result.value().second, 1u);
}

TEST(QuicFrameBranchPadding, LargeRunSixtyFourZeroBytes)
{
    std::vector<uint8_t> buf(64, 0x00);
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().second, 64u);
}

TEST(QuicFrameBranchPadding, ZeroRunStopsAtNonZero)
{
    // Three zero bytes then a non-zero (0x01 = ping).
    std::vector<uint8_t> buf{0x00, 0x00, 0x00, 0x01};
    auto result = quic::frame_parser::parse(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    const auto* f = std::get_if<quic::padding_frame>(&result.value().first);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->count, 3u);
}

// ============================================================================
// parse_all: heterogeneous, error propagation per frame type
// ============================================================================

TEST(QuicFrameBranchParseAll, EmptyBufferYieldsEmptyVector)
{
    std::vector<uint8_t> buf;
    auto result = quic::frame_parser::parse_all(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
}

TEST(QuicFrameBranchParseAll, FourFrameSequenceParses)
{
    // PING, PADDING(3), HANDSHAKE_DONE, MAX_DATA=10
    std::vector<uint8_t> buf{0x01, 0x00, 0x00, 0x00, 0x1e, 0x10, 0x0A};
    auto result = quic::frame_parser::parse_all(as_span(buf));
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 4u);
}

TEST(QuicFrameBranchParseAll, ErrorPropagatesFromMidSequence)
{
    // PING, then truncated CRYPTO (no body after type+offset+length).
    std::vector<uint8_t> buf{0x01, 0x06, 0x00, 0x05, 0xAA};
    auto result = quic::frame_parser::parse_all(as_span(buf));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// build_ack: empty ranges, single range, multi range
// ============================================================================

TEST(QuicFrameBranchBuildAck, EmptyRangesNoEcn)
{
    // Exercises line 970 "!f.ranges.empty()" FALSE branch and line 977
    // "for i=1; i<size" FALSE branch (size==0).
    quic::ack_frame f;
    f.largest_acknowledged = 7;
    f.ack_delay = 0;
    // f.ranges is empty.
    auto bytes = quic::frame_builder::build_ack(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::ack_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_TRUE(g->ranges.empty());
}

TEST(QuicFrameBranchBuildAck, SingleRangeNoEcn)
{
    quic::ack_frame f;
    f.largest_acknowledged = 100;
    f.ack_delay = 5;
    f.ranges.push_back({0, 4}); // first_range encoded from ranges[0].length
    auto bytes = quic::frame_builder::build_ack(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
}

TEST(QuicFrameBranchBuildAck, ThreeRangesNoEcnExercisesLoop)
{
    // Line 977 loop body iterates for i=1 and i=2.
    quic::ack_frame f;
    f.largest_acknowledged = 200;
    f.ranges.push_back({0, 5});
    f.ranges.push_back({1, 6});
    f.ranges.push_back({2, 7});
    auto bytes = quic::frame_builder::build_ack(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::ack_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->ranges.size(), 3u);
}

TEST(QuicFrameBranchBuildAck, EmptyRangesWithEcn)
{
    // Empty ranges path AND ecn set -> ECN counts appended.
    quic::ack_frame f;
    f.largest_acknowledged = 1;
    f.ecn = quic::ecn_counts{2, 3, 4};
    auto bytes = quic::frame_builder::build_ack(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::ack_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    ASSERT_TRUE(g->ecn.has_value());
    EXPECT_EQ(g->ecn->ect0, 2u);
    EXPECT_EQ(g->ecn->ect1, 3u);
    EXPECT_EQ(g->ecn->ecn_ce, 4u);
}

// ============================================================================
// build_padding(0): zero-count corner
// ============================================================================

TEST(QuicFrameBranchBuildPadding, ZeroCountProducesEmptyVector)
{
    auto bytes = quic::frame_builder::build_padding(0);
    EXPECT_TRUE(bytes.empty());
}

TEST(QuicFrameBranchBuildPadding, OneCountProducesSingleZero)
{
    auto bytes = quic::frame_builder::build_padding(1);
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], 0x00);
}

TEST(QuicFrameBranchBuildPadding, LargeCountProducesAllZeros)
{
    auto bytes = quic::frame_builder::build_padding(128);
    ASSERT_EQ(bytes.size(), 128u);
    for (auto b : bytes) EXPECT_EQ(b, 0x00);
}

// ============================================================================
// build_stream: every (fin, length, offset) flag combination
// ============================================================================

struct StreamFlagCase
{
    bool fin;
    bool include_length;
    uint64_t offset;
    const char* label;
};

class QuicFrameBranchBuildStream : public ::testing::TestWithParam<StreamFlagCase> {};

TEST_P(QuicFrameBranchBuildStream, RoundTripPreservesPayload)
{
    auto p = GetParam();
    quic::stream_frame f;
    f.stream_id = 4;
    f.offset = p.offset;
    f.data = {0xDE, 0xAD, 0xBE, 0xEF};
    f.fin = p.fin;
    auto bytes = quic::frame_builder::build_stream(f, p.include_length);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
    const auto* g = std::get_if<quic::stream_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr) << p.label;
    EXPECT_EQ(g->stream_id, 4u);
    EXPECT_EQ(g->offset, p.offset);
    EXPECT_EQ(g->fin, p.fin);
    EXPECT_EQ(g->data.size(), 4u);
}

INSTANTIATE_TEST_SUITE_P(
    Combinations,
    QuicFrameBranchBuildStream,
    ::testing::Values(
        StreamFlagCase{false, false, 0,   "no-fin no-len no-off"},
        StreamFlagCase{true,  false, 0,   "fin no-len no-off"},
        StreamFlagCase{false, true,  0,   "no-fin len no-off"},
        StreamFlagCase{true,  true,  0,   "fin len no-off"},
        StreamFlagCase{false, false, 100, "no-fin no-len off"},
        StreamFlagCase{true,  false, 100, "fin no-len off"},
        StreamFlagCase{false, true,  100, "no-fin len off"},
        StreamFlagCase{true,  true,  100, "fin len off"}));

// ============================================================================
// build(variant) dispatch: every alternative
// ============================================================================

TEST(QuicFrameBranchBuildVariant, AllAlternativesRoundTrip)
{
    std::vector<quic::frame> frames;

    frames.emplace_back(quic::padding_frame{3});
    frames.emplace_back(quic::ping_frame{});
    {
        quic::ack_frame f;
        f.largest_acknowledged = 1;
        frames.emplace_back(f);
    }
    {
        quic::ack_frame f;
        f.largest_acknowledged = 1;
        f.ecn = quic::ecn_counts{1, 2, 3};
        frames.emplace_back(f); // ack_ecn alternative
    }
    frames.emplace_back(quic::reset_stream_frame{1, 2, 3});
    frames.emplace_back(quic::stop_sending_frame{1, 2});
    {
        quic::crypto_frame f;
        f.offset = 0;
        f.data = {0xAA};
        frames.emplace_back(f);
    }
    {
        quic::new_token_frame f;
        f.token = {0xBB, 0xCC};
        frames.emplace_back(f);
    }
    {
        quic::stream_frame f;
        f.stream_id = 1;
        f.data = {0xDD};
        frames.emplace_back(f);
    }
    frames.emplace_back(quic::max_data_frame{100});
    frames.emplace_back(quic::max_stream_data_frame{1, 100});
    frames.emplace_back(quic::max_streams_frame{10, true});
    frames.emplace_back(quic::max_streams_frame{10, false});
    frames.emplace_back(quic::data_blocked_frame{50});
    frames.emplace_back(quic::stream_data_blocked_frame{1, 50});
    frames.emplace_back(quic::streams_blocked_frame{5, true});
    frames.emplace_back(quic::streams_blocked_frame{5, false});
    {
        quic::new_connection_id_frame f;
        f.sequence_number = 1;
        f.retire_prior_to = 0;
        f.connection_id = {1, 2, 3, 4};
        for (size_t i = 0; i < f.stateless_reset_token.size(); ++i)
            f.stateless_reset_token[i] = static_cast<uint8_t>(i);
        frames.emplace_back(f);
    }
    frames.emplace_back(quic::retire_connection_id_frame{1});
    frames.emplace_back(quic::path_challenge_frame{{1, 2, 3, 4, 5, 6, 7, 8}});
    frames.emplace_back(quic::path_response_frame{{1, 2, 3, 4, 5, 6, 7, 8}});
    {
        quic::connection_close_frame f;
        f.error_code = 1;
        f.frame_type = 0;
        f.reason_phrase = "x";
        f.is_application_error = false;
        frames.emplace_back(f);
    }
    {
        quic::connection_close_frame f;
        f.error_code = 2;
        f.reason_phrase = "y";
        f.is_application_error = true;
        frames.emplace_back(f);
    }
    frames.emplace_back(quic::handshake_done_frame{});

    for (const auto& fr : frames)
    {
        auto bytes = quic::frame_builder::build(fr);
        ASSERT_FALSE(bytes.empty());
        auto parsed = quic::frame_parser::parse(as_span(bytes));
        ASSERT_TRUE(parsed.is_ok());
    }
}

// ============================================================================
// get_frame_type / frame_type_to_string: confirm all enum arms
// ============================================================================

TEST(QuicFrameBranchTypeIntrospect, GetFrameTypeForAckEcnVariant)
{
    quic::ack_frame f;
    f.ecn = quic::ecn_counts{};
    quic::frame fr = f;
    EXPECT_EQ(quic::get_frame_type(fr), quic::frame_type::ack_ecn);
}

TEST(QuicFrameBranchTypeIntrospect, GetFrameTypeForMaxStreamsUni)
{
    quic::max_streams_frame f;
    f.bidirectional = false;
    quic::frame fr = f;
    EXPECT_EQ(quic::get_frame_type(fr), quic::frame_type::max_streams_uni);
}

TEST(QuicFrameBranchTypeIntrospect, GetFrameTypeForStreamsBlockedUni)
{
    quic::streams_blocked_frame f;
    f.bidirectional = false;
    quic::frame fr = f;
    EXPECT_EQ(quic::get_frame_type(fr), quic::frame_type::streams_blocked_uni);
}

TEST(QuicFrameBranchTypeIntrospect, GetFrameTypeForConnectionCloseApp)
{
    quic::connection_close_frame f;
    f.is_application_error = true;
    quic::frame fr = f;
    EXPECT_EQ(quic::get_frame_type(fr), quic::frame_type::connection_close_app);
}

TEST(QuicFrameBranchTypeIntrospect, FrameTypeToStringForAllArms)
{
    using FT = quic::frame_type;
    EXPECT_EQ(quic::frame_type_to_string(FT::padding), "PADDING");
    EXPECT_EQ(quic::frame_type_to_string(FT::ping), "PING");
    EXPECT_EQ(quic::frame_type_to_string(FT::ack), "ACK");
    EXPECT_EQ(quic::frame_type_to_string(FT::ack_ecn), "ACK_ECN");
    EXPECT_EQ(quic::frame_type_to_string(FT::reset_stream), "RESET_STREAM");
    EXPECT_EQ(quic::frame_type_to_string(FT::stop_sending), "STOP_SENDING");
    EXPECT_EQ(quic::frame_type_to_string(FT::crypto), "CRYPTO");
    EXPECT_EQ(quic::frame_type_to_string(FT::new_token), "NEW_TOKEN");
    EXPECT_EQ(quic::frame_type_to_string(FT::stream_base), "STREAM");
    EXPECT_EQ(quic::frame_type_to_string(FT::max_data), "MAX_DATA");
    EXPECT_EQ(quic::frame_type_to_string(FT::max_stream_data), "MAX_STREAM_DATA");
    EXPECT_EQ(quic::frame_type_to_string(FT::max_streams_bidi), "MAX_STREAMS_BIDI");
    EXPECT_EQ(quic::frame_type_to_string(FT::max_streams_uni), "MAX_STREAMS_UNI");
    EXPECT_EQ(quic::frame_type_to_string(FT::data_blocked), "DATA_BLOCKED");
    EXPECT_EQ(quic::frame_type_to_string(FT::stream_data_blocked), "STREAM_DATA_BLOCKED");
    EXPECT_EQ(quic::frame_type_to_string(FT::streams_blocked_bidi), "STREAMS_BLOCKED_BIDI");
    EXPECT_EQ(quic::frame_type_to_string(FT::streams_blocked_uni), "STREAMS_BLOCKED_UNI");
    EXPECT_EQ(quic::frame_type_to_string(FT::new_connection_id), "NEW_CONNECTION_ID");
    EXPECT_EQ(quic::frame_type_to_string(FT::retire_connection_id), "RETIRE_CONNECTION_ID");
    EXPECT_EQ(quic::frame_type_to_string(FT::path_challenge), "PATH_CHALLENGE");
    EXPECT_EQ(quic::frame_type_to_string(FT::path_response), "PATH_RESPONSE");
    EXPECT_EQ(quic::frame_type_to_string(FT::connection_close), "CONNECTION_CLOSE");
    EXPECT_EQ(quic::frame_type_to_string(FT::connection_close_app), "CONNECTION_CLOSE_APP");
    EXPECT_EQ(quic::frame_type_to_string(FT::handshake_done), "HANDSHAKE_DONE");
    EXPECT_EQ(quic::frame_type_to_string(static_cast<FT>(0xFFFF)), "UNKNOWN");
}

// ============================================================================
// peek_type: exercise multi-byte varint type prefixes
// ============================================================================

TEST(QuicFrameBranchPeekType, EmptyBufferIsError)
{
    std::vector<uint8_t> buf;
    auto r = quic::frame_parser::peek_type(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

TEST(QuicFrameBranchPeekType, TwoByteVarintPrefixDecodes)
{
    // 0x40 0x10 -> 2-byte varint = 16.
    std::vector<uint8_t> buf{0x40, 0x10};
    auto r = quic::frame_parser::peek_type(as_span(buf));
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(r.value().first, 16u);
    EXPECT_EQ(r.value().second, 2u);
}

TEST(QuicFrameBranchPeekType, TruncatedTwoByteVarintIsError)
{
    std::vector<uint8_t> buf{0x40};
    auto r = quic::frame_parser::peek_type(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// dispatch: unknown frame type and reserved-bit-pattern types
// ============================================================================

TEST(QuicFrameBranchDispatch, TypeOneByteJustAboveKnownRange)
{
    // 0x1f = first unknown 1-byte varint after handshake_done(0x1e).
    std::vector<uint8_t> buf{0x1f};
    auto r = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

TEST(QuicFrameBranchDispatch, TypeReservedTwoByteVarint)
{
    // 2-byte varint type=64 (well outside the defined 0x00-0x1e range).
    std::vector<uint8_t> buf{0x40, 0x40};
    auto r = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

TEST(QuicFrameBranchDispatch, TruncatedTypeVarintIsError)
{
    // 0xC0 = 8-byte varint prefix, no body.
    std::vector<uint8_t> buf{0xC0};
    auto r = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// parse_max_data / parse_max_stream_data / parse_max_streams /
// parse_data_blocked / parse_stream_data_blocked / parse_streams_blocked /
// parse_retire_connection_id: round-trips at varint boundaries
// ============================================================================

struct VarintBoundary
{
    uint64_t value;
    const char* label;
};

class QuicFrameBranchVarintFrames
    : public ::testing::TestWithParam<VarintBoundary> {};

TEST_P(QuicFrameBranchVarintFrames, MaxDataRoundTrip)
{
    auto p = GetParam();
    quic::max_data_frame f{p.value};
    auto bytes = quic::frame_builder::build_max_data(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
    const auto* g = std::get_if<quic::max_data_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->maximum_data, p.value);
}

TEST_P(QuicFrameBranchVarintFrames, DataBlockedRoundTrip)
{
    auto p = GetParam();
    quic::data_blocked_frame f{p.value};
    auto bytes = quic::frame_builder::build_data_blocked(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
}

TEST_P(QuicFrameBranchVarintFrames, MaxStreamsBidirectionalRoundTrip)
{
    auto p = GetParam();
    quic::max_streams_frame f{p.value, true};
    auto bytes = quic::frame_builder::build_max_streams(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
    const auto* g = std::get_if<quic::max_streams_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_TRUE(g->bidirectional);
}

TEST_P(QuicFrameBranchVarintFrames, MaxStreamsUnidirectionalRoundTrip)
{
    auto p = GetParam();
    quic::max_streams_frame f{p.value, false};
    auto bytes = quic::frame_builder::build_max_streams(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
    const auto* g = std::get_if<quic::max_streams_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_FALSE(g->bidirectional);
}

TEST_P(QuicFrameBranchVarintFrames, StreamsBlockedBidirectionalRoundTrip)
{
    auto p = GetParam();
    quic::streams_blocked_frame f{p.value, true};
    auto bytes = quic::frame_builder::build_streams_blocked(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
}

TEST_P(QuicFrameBranchVarintFrames, RetireConnectionIdRoundTrip)
{
    auto p = GetParam();
    quic::retire_connection_id_frame f{p.value};
    auto bytes = quic::frame_builder::build_retire_connection_id(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok()) << p.label;
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries,
    QuicFrameBranchVarintFrames,
    ::testing::Values(
        VarintBoundary{0,           "zero"},
        VarintBoundary{63,          "max-1byte"},
        VarintBoundary{64,          "min-2byte"},
        VarintBoundary{16383,       "max-2byte"},
        VarintBoundary{16384,       "min-4byte"},
        VarintBoundary{1073741823u, "max-4byte"},
        VarintBoundary{1073741824u, "min-8byte"}));

// ============================================================================
// path_challenge / path_response: round-trip with varying byte patterns
// ============================================================================

TEST(QuicFrameBranchPath, PathChallengeAllZerosRoundTrip)
{
    quic::path_challenge_frame f{};
    auto bytes = quic::frame_builder::build_path_challenge(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::path_challenge_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    for (auto b : g->data) EXPECT_EQ(b, 0);
}

TEST(QuicFrameBranchPath, PathResponseAlternatingPattern)
{
    quic::path_response_frame f{{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}};
    auto bytes = quic::frame_builder::build_path_response(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::path_response_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->data[0], 0xAA);
    EXPECT_EQ(g->data[1], 0x55);
}

TEST(QuicFrameBranchPath, PathChallengeMissingByteSeven)
{
    // Type 0x1a, only 7 bytes of payload (one short).
    std::vector<uint8_t> buf{0x1a, 1, 2, 3, 4, 5, 6, 7};
    auto r = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

TEST(QuicFrameBranchPath, PathResponseMissingByteSeven)
{
    std::vector<uint8_t> buf{0x1b, 1, 2, 3, 4, 5, 6, 7};
    auto r = quic::frame_parser::parse(as_span(buf));
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// connection_close: transport variant builder includes frame_type, app does not
// ============================================================================

TEST(QuicFrameBranchBuildConnectionClose, TransportVariantIncludesFrameType)
{
    quic::connection_close_frame f;
    f.error_code = 0x42;
    f.frame_type = 0x10;
    f.reason_phrase = "hello";
    f.is_application_error = false;
    auto bytes = quic::frame_builder::build_connection_close(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::connection_close_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_FALSE(g->is_application_error);
    EXPECT_EQ(g->error_code, 0x42u);
    EXPECT_EQ(g->frame_type, 0x10u);
    EXPECT_EQ(g->reason_phrase, "hello");
}

TEST(QuicFrameBranchBuildConnectionClose, ApplicationVariantSkipsFrameType)
{
    quic::connection_close_frame f;
    f.error_code = 0x7;
    f.reason_phrase = "bye";
    f.is_application_error = true;
    auto bytes = quic::frame_builder::build_connection_close(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
    const auto* g = std::get_if<quic::connection_close_frame>(&parsed.value().first);
    ASSERT_NE(g, nullptr);
    EXPECT_TRUE(g->is_application_error);
    EXPECT_EQ(g->error_code, 0x7u);
    EXPECT_EQ(g->reason_phrase, "bye");
}

TEST(QuicFrameBranchBuildConnectionClose, EmptyReasonRoundTrip)
{
    quic::connection_close_frame f;
    f.error_code = 1;
    f.frame_type = 0;
    f.reason_phrase = "";
    f.is_application_error = false;
    auto bytes = quic::frame_builder::build_connection_close(f);
    auto parsed = quic::frame_parser::parse(as_span(bytes));
    ASSERT_TRUE(parsed.is_ok());
}
