// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_packet_branch_test.cpp
 * @brief Branch-coverage focused tests for src/protocols/quic/packet.cpp
 *        (Issue #1147, Part of #953).
 *
 * Targets the unhit branches identified from the coverage HTML of run
 * 25620254919 (develop @ fc52441, 56.5% branch / 96.9% line baseline).
 * packet.cpp is a pure parser/builder/encoder with no handshake gate,
 * so tests drive @c packet_parser::parse_header, @c parse_long_header,
 * @c parse_short_header, @c packet_number::encode / decode /
 * encoded_length, @c packet_builder::build_* and @c is_version_negotiation
 * directly with hand-crafted byte sequences that target each reachable
 * condition.
 *
 * Strategy: the 171 unhit branches are concentrated on the false side
 * of every length-check, the wrap-around arms of @c packet_number::decode,
 * the boundary cases of @c encoded_length (each width threshold), and
 * the exception edges that gcov tracks for every potentially-throwing
 * operation (varint::decode, vector::insert, span::subspan,
 * connection_id ctor, vector::assign). This file widens call-site
 * variety to drive those edge counters and exercises the explicit
 * false branches the happy-path tests skip past.
 *
 * Coverage targets (per HTML inspection of the develop baseline):
 * - @c packet_type_to_string: default branch on out-of-range enum,
 *   every named arm (one_rtt = 0xFF in particular sits in default
 *   from the switch's perspective if not in the cases — verify the
 *   actual mapping).
 * - @c long_header::type: every 2-bit shift outcome 00/01/10/11.
 * - @c long_header::is_retry: true and false arms.
 * - @c short_header::spin_bit / @c key_phase: 4-way bit combination
 *   matrix (00, 10, 01, 11) over the spin/key-phase product.
 * - @c packet_number::encode: every encoded width 1/2/3/4 produced
 *   from full_pn alone, and from full_pn vs largest_acked gap.
 * - @c packet_number::encoded_length: each side of every <(1<<N)
 *   threshold (127/128, 32767/32768, 8388607/8388608) plus the
 *   full_pn <= largest_acked vs full_pn > largest_acked ternary.
 * - @c packet_number::decode: each branch of the wrap-around logic
 *   (candidate <= expected - hwin with and without the second
 *   conjunction, candidate > expected + hwin with and without the
 *   second conjunction, and the fall-through return).
 * - @c is_version_negotiation: data.size() < 5 false (already covered)
 *   and true with long-header & version=0 (covered) — plus the rare
 *   "data.size() == 5" boundary, the "data.size() >= 5 but first byte
 *   is short header" branch, and the "long header but version != 0"
 *   branch (already partially covered, but with additional version
 *   variations to widen call-site counters).
 * - @c parse_header dispatch: long-header dispatch then error
 *   propagation (already covered), short-header guidance error (already
 *   covered), explicit empty-buffer error, and the long-header
 *   value-extraction branch where parse_long_header returns ok and the
 *   dispatch wraps it into a variant.
 * - @c parse_long_header: every length-check false branch separately —
 *   data.size() < 7, data.size() < offset+4 (version), data.size() <
 *   offset+1 (dcid_len byte), data.size() < offset+dcid_len, data.size()
 *   < offset+1 (scid_len byte), data.size() < offset+scid_len, plus
 *   the dcid_len > max_length and scid_len > max_length false-edge
 *   variations at the boundary value (exactly 20 vs 21).
 * - @c parse_long_header Initial: token_len_result.is_err() (truncated
 *   varint), data.size() < offset+token_len (token too short),
 *   pkt_len_result.is_err() (truncated packet length varint), and the
 *   value-extraction success branch with non-empty token and pn_length
 *   derived from each first_byte reserved-bit value (0,1,2,3).
 * - @c parse_long_header Handshake / 0-RTT: pkt_len_result.is_err()
 *   per type, success per type with each pn_length.
 * - @c parse_long_header Retry: returns ok without consuming token or
 *   tag — verify offset value matches the consumed-by-header amount.
 * - @c parse_short_header: data too short for header+conn_id+1,
 *   not-a-short-header (long-header form set), invalid fixed bit,
 *   conn_id_length==0 success, and conn_id_length>0 success across
 *   every pn_length value.
 * - @c packet_builder::build_*: round-trip with extreme connection-id
 *   lengths (0 and 20), every packet_number width produced by the
 *   encoded_length thresholds, and the dispatch in build(long_header)
 *   for each of the four type arms (the default arm is structurally
 *   unreachable through the 2-bit type mask).
 *
 * Each test asserts the expected @c Result error/ok and the parsed
 * field values, not just "did not crash."
 */

#include "internal/protocols/quic/packet.h"
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

auto make_cid(std::vector<uint8_t> bytes) -> quic::connection_id
{
    std::span<const uint8_t> s{bytes};
    return quic::connection_id{s};
}

// Append a varint produced by the project encoder.
void append_varint_to(std::vector<uint8_t>& buf, uint64_t value)
{
    auto v = quic::varint::encode(value);
    buf.insert(buf.end(), v.begin(), v.end());
}

} // namespace

// ============================================================================
// packet_type_to_string — default branch and named branches with widened
// call-site counters
// ============================================================================

TEST(QuicPacketBranchTypeToString, NamedArmsReturnRfcLabels)
{
    EXPECT_EQ(quic::packet_type_to_string(quic::packet_type::initial), "Initial");
    EXPECT_EQ(quic::packet_type_to_string(quic::packet_type::zero_rtt), "0-RTT");
    EXPECT_EQ(quic::packet_type_to_string(quic::packet_type::handshake), "Handshake");
    EXPECT_EQ(quic::packet_type_to_string(quic::packet_type::retry), "Retry");
    EXPECT_EQ(quic::packet_type_to_string(quic::packet_type::one_rtt), "1-RTT");
}

TEST(QuicPacketBranchTypeToString, DefaultBranchOnUnknownValue)
{
    // Any value outside the named cases must hit the default arm.
    // packet_type is uint8_t so we sweep representative out-of-range values.
    EXPECT_EQ(quic::packet_type_to_string(static_cast<quic::packet_type>(0x04)),
              "Unknown");
    EXPECT_EQ(quic::packet_type_to_string(static_cast<quic::packet_type>(0x05)),
              "Unknown");
    EXPECT_EQ(quic::packet_type_to_string(static_cast<quic::packet_type>(0x10)),
              "Unknown");
    EXPECT_EQ(quic::packet_type_to_string(static_cast<quic::packet_type>(0x7E)),
              "Unknown");
    EXPECT_EQ(quic::packet_type_to_string(static_cast<quic::packet_type>(0xFE)),
              "Unknown");
}

// ============================================================================
// long_header::type / is_retry — every 2-bit shifted value
// ============================================================================

class QuicPacketBranchLongHeaderType : public ::testing::TestWithParam<std::pair<uint8_t, quic::packet_type>>
{
};

TEST_P(QuicPacketBranchLongHeaderType, EveryTypeMaskingPattern)
{
    auto [first_byte, expected_type] = GetParam();
    quic::long_header h{};
    h.first_byte = first_byte;
    EXPECT_EQ(h.type(), expected_type);
    EXPECT_EQ(h.is_retry(), expected_type == quic::packet_type::retry);
}

INSTANTIATE_TEST_SUITE_P(
    AllTypeBits, QuicPacketBranchLongHeaderType,
    ::testing::Values(
        std::make_pair(uint8_t{0xC0}, quic::packet_type::initial),
        std::make_pair(uint8_t{0xD0}, quic::packet_type::zero_rtt),
        std::make_pair(uint8_t{0xE0}, quic::packet_type::handshake),
        std::make_pair(uint8_t{0xF0}, quic::packet_type::retry),
        // High four bits irrelevant beyond the form / fixed bits — verify
        // the 2-bit shift extracts only the low 2 of the upper nibble.
        std::make_pair(uint8_t{0xCF}, quic::packet_type::initial),
        std::make_pair(uint8_t{0xDF}, quic::packet_type::zero_rtt),
        std::make_pair(uint8_t{0xEF}, quic::packet_type::handshake),
        std::make_pair(uint8_t{0xFF}, quic::packet_type::retry)));

// ============================================================================
// short_header::spin_bit / key_phase — full 2x2 matrix
// ============================================================================

TEST(QuicPacketBranchShortHeader, SpinFalseKeyPhaseFalse)
{
    quic::short_header h{};
    h.first_byte = 0x40; // fixed only
    EXPECT_FALSE(h.spin_bit());
    EXPECT_FALSE(h.key_phase());
}

TEST(QuicPacketBranchShortHeader, SpinTrueKeyPhaseFalse)
{
    quic::short_header h{};
    h.first_byte = 0x40 | 0x20; // fixed + spin
    EXPECT_TRUE(h.spin_bit());
    EXPECT_FALSE(h.key_phase());
}

TEST(QuicPacketBranchShortHeader, SpinFalseKeyPhaseTrue)
{
    quic::short_header h{};
    h.first_byte = 0x40 | 0x04; // fixed + key_phase
    EXPECT_FALSE(h.spin_bit());
    EXPECT_TRUE(h.key_phase());
}

TEST(QuicPacketBranchShortHeader, SpinTrueKeyPhaseTrue)
{
    quic::short_header h{};
    h.first_byte = 0x40 | 0x20 | 0x04; // fixed + spin + key_phase
    EXPECT_TRUE(h.spin_bit());
    EXPECT_TRUE(h.key_phase());
}

// ============================================================================
// packet_number::encoded_length — each threshold's true/false edge
// ============================================================================

TEST(QuicPacketBranchPacketNumberLength, FullPnGreaterThanLargestAckedTernaryTrue)
{
    // full_pn > largest_acked -> num_unacked = full_pn - largest_acked.
    EXPECT_EQ(quic::packet_number::encoded_length(10, 5), 1u);
}

TEST(QuicPacketBranchPacketNumberLength, FullPnEqualToLargestAckedTernaryFalse)
{
    // full_pn == largest_acked -> num_unacked falls into the false arm = 1.
    EXPECT_EQ(quic::packet_number::encoded_length(5, 5), 1u);
}

TEST(QuicPacketBranchPacketNumberLength, FullPnLessThanLargestAckedTernaryFalse)
{
    // full_pn < largest_acked -> num_unacked = 1 (false arm).
    EXPECT_EQ(quic::packet_number::encoded_length(3, 5), 1u);
}

TEST(QuicPacketBranchPacketNumberLength, OneByteJustBelowSevenBitThreshold)
{
    EXPECT_EQ(quic::packet_number::encoded_length(127, 0), 1u);
}

TEST(QuicPacketBranchPacketNumberLength, TwoBytesAtSevenBitThresholdCrossing)
{
    EXPECT_EQ(quic::packet_number::encoded_length(128, 0), 2u);
}

TEST(QuicPacketBranchPacketNumberLength, TwoBytesJustBelowFifteenBitThreshold)
{
    EXPECT_EQ(quic::packet_number::encoded_length(32767, 0), 2u);
}

TEST(QuicPacketBranchPacketNumberLength, ThreeBytesAtFifteenBitThresholdCrossing)
{
    EXPECT_EQ(quic::packet_number::encoded_length(32768, 0), 3u);
}

TEST(QuicPacketBranchPacketNumberLength, ThreeBytesJustBelowTwentyThreeBitThreshold)
{
    EXPECT_EQ(quic::packet_number::encoded_length(8388607, 0), 3u);
}

TEST(QuicPacketBranchPacketNumberLength, FourBytesAtTwentyThreeBitThresholdCrossing)
{
    EXPECT_EQ(quic::packet_number::encoded_length(8388608, 0), 4u);
}

TEST(QuicPacketBranchPacketNumberLength, FourBytesAtLargeValue)
{
    EXPECT_EQ(quic::packet_number::encoded_length(1ULL << 30, 0), 4u);
    EXPECT_EQ(quic::packet_number::encoded_length(1ULL << 40, 0), 4u);
}

// ============================================================================
// packet_number::encode — round-trip across each width
// ============================================================================

TEST(QuicPacketBranchPacketNumberEncode, OneByteEncoding)
{
    auto [bytes, len] = quic::packet_number::encode(0x42, 0);
    EXPECT_EQ(len, 1u);
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], 0x42);
}

TEST(QuicPacketBranchPacketNumberEncode, TwoByteEncodingBigEndian)
{
    auto [bytes, len] = quic::packet_number::encode(0x1234, 0);
    EXPECT_EQ(len, 2u);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0x12);
    EXPECT_EQ(bytes[1], 0x34);
}

TEST(QuicPacketBranchPacketNumberEncode, ThreeByteEncodingBigEndian)
{
    auto [bytes, len] = quic::packet_number::encode(0x012345, 0);
    EXPECT_EQ(len, 3u);
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x01);
    EXPECT_EQ(bytes[1], 0x23);
    EXPECT_EQ(bytes[2], 0x45);
}

TEST(QuicPacketBranchPacketNumberEncode, FourByteEncodingBigEndian)
{
    auto [bytes, len] = quic::packet_number::encode(0x01020304, 0);
    EXPECT_EQ(len, 4u);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x01);
    EXPECT_EQ(bytes[1], 0x02);
    EXPECT_EQ(bytes[2], 0x03);
    EXPECT_EQ(bytes[3], 0x04);
}

TEST(QuicPacketBranchPacketNumberEncode, WidthShrinksWithLargestAcked)
{
    // full_pn = 1000, largest_acked = 999 -> num_unacked = 1 -> one byte.
    auto [bytes, len] = quic::packet_number::encode(1000, 999);
    EXPECT_EQ(len, 1u);
    ASSERT_EQ(bytes.size(), 1u);
    // big-endian low byte of 1000 = 0xE8.
    EXPECT_EQ(bytes[0], static_cast<uint8_t>(1000 & 0xFF));
}

// ============================================================================
// packet_number::decode — RFC 9000 Appendix A: every wrap-around branch
// ============================================================================

// Reference implementation: keeps tests honest if a refactor is ever
// proposed. Mirrors the spec literally.
namespace
{
auto reference_decode_pn(uint64_t truncated_pn, size_t pn_length,
                         uint64_t largest_pn) -> uint64_t
{
    uint64_t expected_pn = largest_pn + 1;
    uint64_t pn_win = 1ULL << (pn_length * 8);
    uint64_t pn_hwin = pn_win / 2;
    uint64_t pn_mask = pn_win - 1;
    uint64_t candidate_pn = (expected_pn & ~pn_mask) | truncated_pn;
    if (candidate_pn <= expected_pn - pn_hwin
        && candidate_pn < (1ULL << 62) - pn_win)
    {
        return candidate_pn + pn_win;
    }
    if (candidate_pn > expected_pn + pn_hwin && candidate_pn >= pn_win)
    {
        return candidate_pn - pn_win;
    }
    return candidate_pn;
}
} // namespace

TEST(QuicPacketBranchPacketNumberDecode, FallThroughReturnBranch)
{
    // expected_pn = 0xA82F30EA (largest = 0xA82F30E9), truncated = 0x9B32
    // (2 bytes). candidate_pn = 0xA82F9B32. Within window — fall-through.
    auto v = quic::packet_number::decode(0x9B32, 2, 0xA82F30E9);
    EXPECT_EQ(v, reference_decode_pn(0x9B32, 2, 0xA82F30E9));
    EXPECT_EQ(v, 0xA82F9B32u);
}

TEST(QuicPacketBranchPacketNumberDecode, AddWindowBranchBothConjunctsTrue)
{
    // largest = 0xFE -> expected = 0xFF. truncated_pn = 0x01, pn_length = 1.
    // pn_win = 256, pn_hwin = 128. candidate = (0xFF & ~0xFF) | 0x01 = 0x01.
    // 0x01 <= (0xFF - 128) = 0x7F  -> true.
    // 0x01 <  (2^62 - 256)         -> true.
    // Result must be candidate + pn_win = 0x101.
    auto v = quic::packet_number::decode(0x01, 1, 0xFE);
    EXPECT_EQ(v, 0x101u);
    EXPECT_EQ(v, reference_decode_pn(0x01, 1, 0xFE));
}

TEST(QuicPacketBranchPacketNumberDecode, AddWindowBranchFirstConjunctFalse)
{
    // Choose a candidate just above expected - pn_hwin so the first
    // conjunct is false. largest = 0x80 -> expected = 0x81; trunc = 0x80,
    // pn_length = 1; candidate = (0x81 & ~0xFF) | 0x80 = 0x80.
    // expected - pn_hwin = 0x81 - 128 = 0x01. 0x80 <= 0x01 is FALSE,
    // so the add-window branch is not taken — fall-through.
    auto v = quic::packet_number::decode(0x80, 1, 0x80);
    EXPECT_EQ(v, reference_decode_pn(0x80, 1, 0x80));
}

TEST(QuicPacketBranchPacketNumberDecode, SubtractWindowBranchBothConjunctsTrue)
{
    // Force a candidate above expected + pn_hwin with candidate >= pn_win.
    // largest = 0x100 -> expected = 0x101. truncated = 0xFF, pn_length = 1.
    // pn_win = 256. pn_hwin = 128. candidate = (0x101 & ~0xFF) | 0xFF = 0x1FF.
    // 0x1FF > 0x101 + 128 = 0x181 -> TRUE.
    // 0x1FF >= 256                -> TRUE.
    // Result = candidate - pn_win = 0xFF.
    auto v = quic::packet_number::decode(0xFF, 1, 0x100);
    EXPECT_EQ(v, 0xFFu);
    EXPECT_EQ(v, reference_decode_pn(0xFF, 1, 0x100));
}

TEST(QuicPacketBranchPacketNumberDecode, SubtractWindowBranchFirstConjunctFalse)
{
    // Candidate <= expected + pn_hwin -> first conjunct false -> fall-through.
    // largest = 0x100 -> expected = 0x101. truncated = 0x50, pn_length = 1.
    // candidate = (0x101 & ~0xFF) | 0x50 = 0x150.
    // 0x150 > 0x181 -> FALSE. Fall-through.
    auto v = quic::packet_number::decode(0x50, 1, 0x100);
    EXPECT_EQ(v, reference_decode_pn(0x50, 1, 0x100));
}

TEST(QuicPacketBranchPacketNumberDecode, ZeroLargestRoundTrip)
{
    // expected = 1; truncated = 0; pn_length = 1.
    // candidate = (1 & ~0xFF) | 0 = 0. 0 <= 1 - 128 underflows in unsigned
    // arithmetic to a very large value, so 0 <= huge -> TRUE.
    // 0 < 2^62 - 256 -> TRUE. Result = pn_win = 256.
    // This tests the wrap-around when expected_pn - pn_hwin underflows.
    auto v = quic::packet_number::decode(0, 1, 0);
    EXPECT_EQ(v, reference_decode_pn(0, 1, 0));
}

TEST(QuicPacketBranchPacketNumberDecode, FourBytePacketNumberDecode)
{
    auto v = quic::packet_number::decode(0xDEADBEEF, 4, 0xDEADBEEE);
    EXPECT_EQ(v, reference_decode_pn(0xDEADBEEF, 4, 0xDEADBEEE));
}

TEST(QuicPacketBranchPacketNumberDecode, ThreeBytePacketNumberAtBoundary)
{
    auto v = quic::packet_number::decode(0x800000, 3, 0x7FFFFF);
    EXPECT_EQ(v, reference_decode_pn(0x800000, 3, 0x7FFFFF));
}

TEST(QuicPacketBranchPacketNumberDecode, TwoBytePacketNumberCrossesWindow)
{
    // expected = 0x10000; truncated = 0x0001. candidate = 0x10001.
    // No window cross. Fall-through.
    auto v = quic::packet_number::decode(0x0001, 2, 0xFFFF);
    EXPECT_EQ(v, reference_decode_pn(0x0001, 2, 0xFFFF));
}

// ============================================================================
// is_version_negotiation — every disjoint outcome
// ============================================================================

TEST(QuicPacketBranchVersionNegotiation, ShorterThanFiveBytesIsFalse)
{
    std::vector<uint8_t> too_short = {0xC0, 0x00, 0x00, 0x00};
    EXPECT_FALSE(quic::packet_parser::is_version_negotiation(too_short));
}

TEST(QuicPacketBranchVersionNegotiation, EmptyIsFalse)
{
    std::vector<uint8_t> empty;
    EXPECT_FALSE(quic::packet_parser::is_version_negotiation(empty));
}

TEST(QuicPacketBranchVersionNegotiation, ExactlyFiveBytesShortHeaderIsFalse)
{
    // First byte 0x40 -> short header. Version-negotiation requires long.
    std::vector<uint8_t> data = {0x40, 0x00, 0x00, 0x00, 0x00};
    EXPECT_FALSE(quic::packet_parser::is_version_negotiation(data));
}

TEST(QuicPacketBranchVersionNegotiation, LongHeaderVersionZeroIsTrue)
{
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(quic::packet_parser::is_version_negotiation(data));
}

TEST(QuicPacketBranchVersionNegotiation, LongHeaderVersionOneIsFalse)
{
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01};
    EXPECT_FALSE(quic::packet_parser::is_version_negotiation(data));
}

TEST(QuicPacketBranchVersionNegotiation, LongHeaderVersionAllOnesIsFalse)
{
    // 0xFFFFFFFF is a non-zero version even though it's reserved.
    std::vector<uint8_t> data = {0xC0, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_FALSE(quic::packet_parser::is_version_negotiation(data));
}

TEST(QuicPacketBranchVersionNegotiation, LongHeaderVersionTwoIsFalse)
{
    // QUIC version 2 (RFC 9369) is 0x6b3343cf.
    std::vector<uint8_t> data = {0xC0, 0x6b, 0x33, 0x43, 0xCF};
    EXPECT_FALSE(quic::packet_parser::is_version_negotiation(data));
}

TEST(QuicPacketBranchVersionNegotiation, LongHeaderWithoutFixedBitVersionZero)
{
    // Header form bit set but fixed bit cleared. is_version_negotiation
    // checks only the header-form bit (0x80) -- the fixed-bit gate is
    // applied by parse_long_header, not by this predicate.
    std::vector<uint8_t> data = {0x80, 0x00, 0x00, 0x00, 0x00};
    EXPECT_TRUE(quic::packet_parser::is_version_negotiation(data));
}

TEST(QuicPacketBranchVersionNegotiation, LongHeaderTrailingBytesIgnored)
{
    // is_version_negotiation only looks at the first five bytes.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x00,
                                 0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_TRUE(quic::packet_parser::is_version_negotiation(data));
}

// ============================================================================
// parse_header dispatch — empty, short-header, long-header value branch,
// long-header error propagation
// ============================================================================

TEST(QuicPacketBranchParseHeader, EmptyInputErrors)
{
    std::vector<uint8_t> data;
    auto result = quic::packet_parser::parse_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseHeader, ShortHeaderInputReturnsExplanatoryError)
{
    // 0x40 -> short header form. parse_header rejects with guidance.
    std::vector<uint8_t> data = {0x40};
    auto result = quic::packet_parser::parse_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseHeader, ShortHeaderZeroFirstByteIsAlsoShortHeader)
{
    // 0x00 has the long-header bit clear, so parse_header treats it as
    // a short header and returns the guidance error.
    std::vector<uint8_t> data = {0x00, 0x00, 0x00};
    auto result = quic::packet_parser::parse_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseHeader, LongHeaderHandshakeDispatchYieldsVariant)
{
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xE0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_header(data);
    ASSERT_TRUE(result.is_ok());
    auto& [header_variant, consumed] = result.value();
    ASSERT_TRUE(std::holds_alternative<quic::long_header>(header_variant));
    EXPECT_GT(consumed, 0u);
}

TEST(QuicPacketBranchParseHeader, LongHeaderRetryDispatchYieldsVariant)
{
    std::vector<uint8_t> data = {0xF0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    auto result = quic::packet_parser::parse_header(data);
    ASSERT_TRUE(result.is_ok());
    auto& [header_variant, consumed] = result.value();
    ASSERT_TRUE(std::holds_alternative<quic::long_header>(header_variant));
}

TEST(QuicPacketBranchParseHeader, LongHeaderInitialErrorPropagatesThroughDispatch)
{
    // First byte signals long header. parse_long_header fails due to a
    // truncated body — parse_header must wrap and propagate that error.
    std::vector<uint8_t> data = {0xC0, 0x00};
    auto result = quic::packet_parser::parse_header(data);
    ASSERT_TRUE(result.is_err());
}

// ============================================================================
// parse_long_header — each length-check false branch on its own boundary
// ============================================================================

TEST(QuicPacketBranchParseLongHeader, ExactlySixBytesTooShortForMinimum)
{
    std::vector<uint8_t> data(6, 0xC0);
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, ZeroBytesTooShortForMinimum)
{
    std::vector<uint8_t> data;
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, ShortHeaderInputRejected)
{
    std::vector<uint8_t> data = {0x40, 0, 0, 0, 0, 0, 0};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, MissingFixedBitRejected)
{
    // 0x80 -> long header form, but fixed bit (0x40) cleared.
    std::vector<uint8_t> data = {0x80, 0, 0, 0, 0x01, 0, 0};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, DcidLengthExactlyMaxOk)
{
    // dcid_len = 20 (max). Provide exactly 20 dcid bytes, then scid_len = 0,
    // then enough trailing to satisfy the Retry type-specific success arm.
    std::vector<uint8_t> data = {0xF0, 0x00, 0x00, 0x00, 0x01, 20};
    data.insert(data.end(), 20, 0xAA);
    data.push_back(0); // scid_len = 0
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.dest_conn_id.length(), 20u);
}

TEST(QuicPacketBranchParseLongHeader, DcidLengthTwentyOneRejected)
{
    // dcid_len = 21 -> exceeds max_length.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 21, 0x00};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, DcidLengthFortyTwoRejected)
{
    // Well above max -- still hits the dcid_len > max_length branch.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 42, 0x00};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, DcidBytesMissing)
{
    // dcid_len = 5 but only 1 byte left after the length byte.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 5, 0x00};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, ScidLengthExactlyMaxOk)
{
    // dcid_len = 0, scid_len = 20 with all 20 bytes present.
    std::vector<uint8_t> data = {0xF0, 0x00, 0x00, 0x00, 0x01, 0, 20};
    data.insert(data.end(), 20, 0xBB);
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.src_conn_id.length(), 20u);
}

TEST(QuicPacketBranchParseLongHeader, ScidLengthTwentyOneRejected)
{
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 21};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, ScidLengthByteMissing)
{
    // dcid_len = 7, exactly 7 dcid bytes -> no scid_len byte follows.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 7};
    data.insert(data.end(), 7, 0xCC);
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, ScidBytesMissing)
{
    // dcid_len = 0, scid_len = 4 but no scid bytes follow.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 4};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

// Initial type-specific branches

TEST(QuicPacketBranchParseLongHeader, InitialTokenLengthVarintTruncated)
{
    // Two-byte varint prefix (0x40) with NO follow-up byte -> token_len_result.is_err().
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0, 0x40};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, InitialTokenLengthMissingEntirely)
{
    // Exactly at the 7-byte minimum: no token length varint at all.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, InitialTokenBytesShortByOne)
{
    // token_len = 4 but only 3 bytes follow.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    append_varint_to(data, 4);
    data.push_back(0x01);
    data.push_back(0x02);
    data.push_back(0x03);
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, InitialTokenBytesExactlyMatchAndPacketLengthVarintTruncated)
{
    // token_len = 2 with exactly 2 bytes, then a 2-byte varint prefix with
    // no follow-up -> pkt_len_result.is_err().
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    append_varint_to(data, 2);
    data.push_back(0xAA);
    data.push_back(0xBB);
    data.push_back(0x40);
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, InitialReservedBitsZeroPnLengthOne)
{
    auto token_len = quic::varint::encode(0);
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), token_len.begin(), token_len.end());
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 1u);
}

TEST(QuicPacketBranchParseLongHeader, InitialReservedBitsOnePnLengthTwo)
{
    auto token_len = quic::varint::encode(0);
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xC1, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), token_len.begin(), token_len.end());
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 2u);
}

TEST(QuicPacketBranchParseLongHeader, InitialReservedBitsTwoPnLengthThree)
{
    auto token_len = quic::varint::encode(0);
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xC2, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), token_len.begin(), token_len.end());
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 3u);
}

TEST(QuicPacketBranchParseLongHeader, InitialReservedBitsThreePnLengthFour)
{
    auto token_len = quic::varint::encode(0);
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xC3, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), token_len.begin(), token_len.end());
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 4u);
}

TEST(QuicPacketBranchParseLongHeader, InitialWithTokenAndPacketLengthRoundTripsToken)
{
    // Verify the token bytes are copied verbatim into header.token.
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    append_varint_to(data, 3);
    data.push_back(0x11);
    data.push_back(0x22);
    data.push_back(0x33);
    append_varint_to(data, 0);
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    const auto& header = result.value().first;
    ASSERT_EQ(header.token.size(), 3u);
    EXPECT_EQ(header.token[0], 0x11);
    EXPECT_EQ(header.token[1], 0x22);
    EXPECT_EQ(header.token[2], 0x33);
}

// Handshake / 0-RTT type-specific branches

TEST(QuicPacketBranchParseLongHeader, HandshakePacketLengthVarintTruncated)
{
    // 0xE0 = handshake, then DCID/SCID = 0, then 2-byte varint prefix with no follow-up.
    std::vector<uint8_t> data = {0xE0, 0x00, 0x00, 0x00, 0x01, 0, 0, 0x40};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, ZeroRttPacketLengthVarintTruncated)
{
    // 0xD0 = 0-RTT.
    std::vector<uint8_t> data = {0xD0, 0x00, 0x00, 0x00, 0x01, 0, 0, 0x40};
    auto result = quic::packet_parser::parse_long_header(data);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseLongHeader, HandshakeReservedBitsThreePnLengthFour)
{
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xE3, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 4u);
}

TEST(QuicPacketBranchParseLongHeader, ZeroRttReservedBitsTwoPnLengthThree)
{
    auto pkt_len = quic::varint::encode(0);
    std::vector<uint8_t> data = {0xD2, 0x00, 0x00, 0x00, 0x01, 0, 0};
    data.insert(data.end(), pkt_len.begin(), pkt_len.end());
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 3u);
    EXPECT_EQ(result.value().first.type(), quic::packet_type::zero_rtt);
}

// Retry — neither pn_length nor packet_length is parsed
TEST(QuicPacketBranchParseLongHeader, RetryStopsAtSrcConnId)
{
    // The offset returned should be exactly 1 (first) + 4 (version)
    // + 1 (dcid_len) + 0 (dcid) + 1 (scid_len) + 0 (scid) = 7.
    std::vector<uint8_t> data = {0xF0, 0x00, 0x00, 0x00, 0x01, 0, 0};
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().second, 7u);
    EXPECT_EQ(result.value().first.type(), quic::packet_type::retry);
}

TEST(QuicPacketBranchParseLongHeader, RetryWithNonEmptyConnectionIdsStopsAtSrcConnId)
{
    std::vector<uint8_t> data = {0xF0, 0x00, 0x00, 0x00, 0x01, 3, 0xAA, 0xBB, 0xCC,
                                 2, 0x11, 0x22};
    auto result = quic::packet_parser::parse_long_header(data);
    ASSERT_TRUE(result.is_ok());
    const auto& [header, consumed] = result.value();
    EXPECT_EQ(consumed, 12u);
    EXPECT_EQ(header.dest_conn_id.length(), 3u);
    EXPECT_EQ(header.src_conn_id.length(), 2u);
}

// ============================================================================
// parse_short_header — boundary cases
// ============================================================================

TEST(QuicPacketBranchParseShortHeader, ZeroConnIdLengthZeroByteBufferTooShort)
{
    // min size = 1 + 0 + 1 = 2 bytes. Only 1 byte present.
    std::vector<uint8_t> data = {0x40};
    auto result = quic::packet_parser::parse_short_header(data, 0);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseShortHeader, ExactlyMinimumWithZeroConnIdSucceeds)
{
    std::vector<uint8_t> data = {0x40, 0x99};
    auto result = quic::packet_parser::parse_short_header(data, 0);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 1u);
}

TEST(QuicPacketBranchParseShortHeader, LongHeaderFormByteRejected)
{
    std::vector<uint8_t> data(10, 0xC0);
    auto result = quic::packet_parser::parse_short_header(data, 4);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseShortHeader, FixedBitClearedRejected)
{
    std::vector<uint8_t> data(10, 0x00);
    auto result = quic::packet_parser::parse_short_header(data, 4);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseShortHeader, ConnIdLengthExceedsBufferRejected)
{
    // conn_id_length = 8 but only 2 bytes follow the first byte.
    std::vector<uint8_t> data = {0x40, 0x01, 0x02};
    auto result = quic::packet_parser::parse_short_header(data, 8);
    EXPECT_TRUE(result.is_err());
}

TEST(QuicPacketBranchParseShortHeader, PnLengthBitsZeroProducesPnLengthOne)
{
    std::vector<uint8_t> data = {0x40, 0xAA, 0xBB, 0xCC, 0xDD, 0x00};
    auto result = quic::packet_parser::parse_short_header(data, 4);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 1u);
}

TEST(QuicPacketBranchParseShortHeader, PnLengthBitsOneProducesPnLengthTwo)
{
    std::vector<uint8_t> data = {0x41, 0xAA, 0xBB, 0xCC, 0xDD, 0x00};
    auto result = quic::packet_parser::parse_short_header(data, 4);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 2u);
}

TEST(QuicPacketBranchParseShortHeader, PnLengthBitsTwoProducesPnLengthThree)
{
    std::vector<uint8_t> data = {0x42, 0xAA, 0xBB, 0xCC, 0xDD, 0x00};
    auto result = quic::packet_parser::parse_short_header(data, 4);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 3u);
}

TEST(QuicPacketBranchParseShortHeader, PnLengthBitsThreeProducesPnLengthFour)
{
    std::vector<uint8_t> data = {0x43, 0xAA, 0xBB, 0xCC, 0xDD, 0x00};
    auto result = quic::packet_parser::parse_short_header(data, 4);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.packet_number_length, 4u);
}

TEST(QuicPacketBranchParseShortHeader, SpinAndKeyPhaseBitsPreserved)
{
    // first_byte = 0x40 + 0x20 (spin) + 0x04 (key_phase) + 0x01 (pn_length=2).
    std::vector<uint8_t> data = {0x65, 0xAA, 0xBB, 0xCC, 0xDD, 0x00};
    auto result = quic::packet_parser::parse_short_header(data, 4);
    ASSERT_TRUE(result.is_ok());
    const auto& [header, consumed] = result.value();
    EXPECT_TRUE(header.spin_bit());
    EXPECT_TRUE(header.key_phase());
    EXPECT_EQ(header.packet_number_length, 2u);
}

TEST(QuicPacketBranchParseShortHeader, TwentyByteConnIdSucceeds)
{
    std::vector<uint8_t> data = {0x40};
    data.insert(data.end(), 20, 0x77);
    data.push_back(0x00); // packet number byte
    auto result = quic::packet_parser::parse_short_header(data, 20);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.dest_conn_id.length(), 20u);
}

// ============================================================================
// build_* round-trip variations to widen call-site exception-edge counters
// ============================================================================

TEST(QuicPacketBranchBuild, InitialWithEmptyTokenAndCidsRoundTrips)
{
    auto dest = make_cid({});
    auto src = make_cid({});
    auto built = quic::packet_builder::build_initial(dest, src, {}, 0);
    auto result = quic::packet_parser::parse_long_header(built);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.type(), quic::packet_type::initial);
    EXPECT_TRUE(result.value().first.token.empty());
}

TEST(QuicPacketBranchBuild, InitialWithMaxCidsRoundTrips)
{
    auto dest = make_cid(std::vector<uint8_t>(20, 0xA1));
    auto src = make_cid(std::vector<uint8_t>(20, 0xB2));
    std::vector<uint8_t> token(8, 0xCC);
    auto built = quic::packet_builder::build_initial(dest, src, token, 1);
    auto result = quic::packet_parser::parse_long_header(built);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first.dest_conn_id.length(), 20u);
    EXPECT_EQ(result.value().first.src_conn_id.length(), 20u);
    EXPECT_EQ(result.value().first.token, token);
}

TEST(QuicPacketBranchBuild, HandshakeRoundTripsAcrossPacketNumberWidths)
{
    auto dest = make_cid({0x01, 0x02});
    auto src = make_cid({0x03, 0x04});

    auto built1 = quic::packet_builder::build_handshake(dest, src, 1);
    auto built2 = quic::packet_builder::build_handshake(dest, src, 128);
    auto built3 = quic::packet_builder::build_handshake(dest, src, 32768);
    auto built4 = quic::packet_builder::build_handshake(dest, src, 8388608);

    for (const auto& built : {built1, built2, built3, built4})
    {
        auto result = quic::packet_parser::parse_long_header(built);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first.type(), quic::packet_type::handshake);
    }
}

TEST(QuicPacketBranchBuild, ZeroRttRoundTripsAcrossVersions)
{
    auto dest = make_cid({0x05});
    auto src = make_cid({0x06});
    auto v1 = quic::packet_builder::build_zero_rtt(dest, src, 1,
                                                   quic::quic_version::version_1);
    auto v2 = quic::packet_builder::build_zero_rtt(dest, src, 1,
                                                   quic::quic_version::version_2);
    auto vn = quic::packet_builder::build_zero_rtt(dest, src, 1,
                                                   quic::quic_version::negotiation);

    for (const auto& built : {v1, v2, vn})
    {
        auto result = quic::packet_parser::parse_long_header(built);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first.type(), quic::packet_type::zero_rtt);
    }
}

TEST(QuicPacketBranchBuild, RetryAppendsTokenThenIntegrityTagInThatOrder)
{
    auto dest = make_cid({0xDE, 0xAD});
    auto src = make_cid({0xBE, 0xEF});
    std::vector<uint8_t> token = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::array<uint8_t, 16> tag{};
    for (std::size_t i = 0; i < tag.size(); ++i)
    {
        tag[i] = static_cast<uint8_t>(0x70 + i);
    }
    auto built = quic::packet_builder::build_retry(dest, src, token, tag);
    // Last 16 bytes should be the tag.
    ASSERT_GE(built.size(), tag.size());
    for (std::size_t i = 0; i < tag.size(); ++i)
    {
        EXPECT_EQ(built[built.size() - tag.size() + i], tag[i]);
    }
    // 5 bytes preceding the tag should be the token.
    ASSERT_GE(built.size(), tag.size() + token.size());
    for (std::size_t i = 0; i < token.size(); ++i)
    {
        EXPECT_EQ(built[built.size() - tag.size() - token.size() + i], token[i]);
    }
}

TEST(QuicPacketBranchBuild, RetryWithEmptyTokenAndZeroCids)
{
    auto dest = make_cid({});
    auto src = make_cid({});
    std::vector<uint8_t> token;
    std::array<uint8_t, 16> tag{};
    auto built = quic::packet_builder::build_retry(dest, src, token, tag);
    // Long form bit set, retry type bits (11) in upper nibble.
    ASSERT_FALSE(built.empty());
    EXPECT_NE(built[0] & 0x80, 0);
    EXPECT_EQ(quic::packet_parser::get_long_packet_type(built[0]),
              quic::packet_type::retry);
}

TEST(QuicPacketBranchBuild, ShortPacketRoundTripsAtEachPacketNumberWidth)
{
    auto cid = make_cid({0x10, 0x20, 0x30});

    auto built1 = quic::packet_builder::build_short(cid, 0);
    auto built2 = quic::packet_builder::build_short(cid, 200);
    auto built3 = quic::packet_builder::build_short(cid, 40000);
    auto built4 = quic::packet_builder::build_short(cid, 0x01000000);

    for (const auto& built : {built1, built2, built3, built4})
    {
        auto result = quic::packet_parser::parse_short_header(built, cid.length());
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first.dest_conn_id, cid);
    }
}

TEST(QuicPacketBranchBuild, ShortWithSpinTrueKeyPhaseFalseFlag)
{
    auto cid = make_cid({0xAB});
    auto built = quic::packet_builder::build_short(cid, 5, /*key_phase=*/false,
                                                   /*spin_bit=*/true);
    ASSERT_FALSE(built.empty());
    EXPECT_NE(built[0] & 0x20, 0); // spin set
    EXPECT_EQ(built[0] & 0x04, 0); // key_phase cleared
}

TEST(QuicPacketBranchBuild, ShortWithSpinFalseKeyPhaseTrueFlag)
{
    auto cid = make_cid({0xAB});
    auto built = quic::packet_builder::build_short(cid, 5, /*key_phase=*/true,
                                                   /*spin_bit=*/false);
    ASSERT_FALSE(built.empty());
    EXPECT_EQ(built[0] & 0x20, 0);
    EXPECT_NE(built[0] & 0x04, 0);
}

// ============================================================================
// build(long_header) dispatch — every type arm
// ============================================================================

class QuicPacketBranchBuildLongHeaderDispatch
    : public ::testing::TestWithParam<quic::packet_type>
{
};

TEST_P(QuicPacketBranchBuildLongHeaderDispatch, EveryTypeProducesNonEmptyAndPreservesTypeBits)
{
    auto ptype = GetParam();
    quic::long_header h{};
    h.first_byte = 0xC0 | (static_cast<uint8_t>(ptype) << 4);
    h.version = quic::quic_version::version_1;
    h.dest_conn_id = make_cid({0x01, 0x02, 0x03});
    h.src_conn_id = make_cid({0x04, 0x05});
    h.packet_number = 7;
    if (ptype == quic::packet_type::initial)
    {
        h.token = {0xAA, 0xBB};
    }
    if (ptype == quic::packet_type::retry)
    {
        h.retry_integrity_tag.fill(0xCD);
    }
    auto out = quic::packet_builder::build(h);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(quic::packet_parser::get_long_packet_type(out[0]), ptype);
}

INSTANTIATE_TEST_SUITE_P(
    EveryLongHeaderType, QuicPacketBranchBuildLongHeaderDispatch,
    ::testing::Values(quic::packet_type::initial,
                      quic::packet_type::zero_rtt,
                      quic::packet_type::handshake,
                      quic::packet_type::retry));

// build(short_header) — exercise the dispatch over short_header
TEST(QuicPacketBranchBuild, BuildShortHeaderVariantDispatch)
{
    quic::short_header h{};
    h.first_byte = 0x40 | 0x20 | 0x04 | 0x01; // fixed + spin + key + pn_length=2
    h.dest_conn_id = make_cid({0xAB, 0xCD});
    h.packet_number = 1;
    auto out = quic::packet_builder::build(h);
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out[0] & 0x80, 0);                 // header form = short
    EXPECT_NE(out[0] & 0x40, 0);                 // fixed
    EXPECT_NE(out[0] & 0x20, 0);                 // spin propagated
    EXPECT_NE(out[0] & 0x04, 0);                 // key_phase propagated
}

// ============================================================================
// Combined error-propagation matrix via parse_header (TEST_P over malformed
// long-header payloads) — drives the dispatch-error wrapping branch
// ============================================================================

struct MalformedLongHeaderCase
{
    const char* label;
    std::vector<uint8_t> bytes;
};

class QuicPacketBranchParseHeaderErrors
    : public ::testing::TestWithParam<MalformedLongHeaderCase>
{
};

TEST_P(QuicPacketBranchParseHeaderErrors, EveryMalformedLongHeaderPropagates)
{
    const auto& tc = GetParam();
    auto result = quic::packet_parser::parse_header(tc.bytes);
    EXPECT_TRUE(result.is_err()) << tc.label;
}

INSTANTIATE_TEST_SUITE_P(
    AllMalformed, QuicPacketBranchParseHeaderErrors,
    ::testing::Values(
        MalformedLongHeaderCase{
            "TooShort",
            std::vector<uint8_t>{0xC0, 0x00, 0x00}},
        MalformedLongHeaderCase{
            "NoFixedBit",
            std::vector<uint8_t>{0x80, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00}},
        MalformedLongHeaderCase{
            "DcidOverLimit",
            std::vector<uint8_t>{0xC0, 0x00, 0x00, 0x00, 0x01, 21, 0x00}},
        MalformedLongHeaderCase{
            "ScidOverLimit",
            std::vector<uint8_t>{0xC0, 0x00, 0x00, 0x00, 0x01, 0, 21}},
        MalformedLongHeaderCase{
            "DcidTruncated",
            std::vector<uint8_t>{0xC0, 0x00, 0x00, 0x00, 0x01, 8, 0x00}},
        MalformedLongHeaderCase{
            "ScidTruncated",
            std::vector<uint8_t>{0xC0, 0x00, 0x00, 0x00, 0x01, 0, 6}},
        MalformedLongHeaderCase{
            "InitialMissingTokenLen",
            std::vector<uint8_t>{0xC0, 0x00, 0x00, 0x00, 0x01, 0, 0, 0x40}},
        MalformedLongHeaderCase{
            "HandshakeMissingPacketLen",
            std::vector<uint8_t>{0xE0, 0x00, 0x00, 0x00, 0x01, 0, 0, 0x80}},
        MalformedLongHeaderCase{
            "ZeroRttMissingPacketLen",
            std::vector<uint8_t>{0xD0, 0x00, 0x00, 0x00, 0x01, 0, 0, 0x40}}));

// ============================================================================
// is_version_negotiation TEST_P — sweep first-byte variations
// ============================================================================

struct VersionNegotiationCase
{
    const char* label;
    uint8_t first_byte;
    uint32_t version;
    bool expected;
};

class QuicPacketBranchVersionNegotiationSweep
    : public ::testing::TestWithParam<VersionNegotiationCase>
{
};

TEST_P(QuicPacketBranchVersionNegotiationSweep, ReachesExpectedOutcome)
{
    const auto& tc = GetParam();
    std::vector<uint8_t> data = {tc.first_byte,
                                 static_cast<uint8_t>(tc.version >> 24),
                                 static_cast<uint8_t>(tc.version >> 16),
                                 static_cast<uint8_t>(tc.version >> 8),
                                 static_cast<uint8_t>(tc.version)};
    EXPECT_EQ(quic::packet_parser::is_version_negotiation(data), tc.expected)
        << tc.label;
}

INSTANTIATE_TEST_SUITE_P(
    Cases, QuicPacketBranchVersionNegotiationSweep,
    ::testing::Values(
        VersionNegotiationCase{"LongHdrVersionZero", 0xC0, 0x00000000, true},
        VersionNegotiationCase{"LongHdrFixedClearVersionZero", 0x80, 0x00000000, true},
        VersionNegotiationCase{"LongHdrVersionOne", 0xC0, 0x00000001, false},
        VersionNegotiationCase{"LongHdrVersionTwo", 0xC0, 0x6b3343cf, false},
        VersionNegotiationCase{"LongHdrVersionFFFFFFFF", 0xC0, 0xFFFFFFFF, false},
        VersionNegotiationCase{"ShortHdrVersionZero", 0x40, 0x00000000, false},
        VersionNegotiationCase{"ShortHdrZeroFirstByte", 0x00, 0x00000000, false},
        VersionNegotiationCase{"LongHdrUnusedTypeBitsSetVersionZero", 0xFF, 0x00000000, true}));
