// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file hpack_branch_test.cpp
 * @brief Branch-coverage extension for src/protocols/http2/hpack.cpp (Issue #1031)
 *
 * Pre-Step-1 baseline: line 12.8% / branch 12.77% (lcov, 2026-04-13).
 * Post-Step-1 baseline (run 24926703633, develop @ c76aa72): line 89.86%,
 * branch 56.06% — already over the 70% line target, just under the 60%
 * branch target.
 *
 * Existing files cover the bulk of the encoder/decoder. This file adds tests
 * that lift specific branches still uncovered after #1009:
 *  - Decoder literal-without-indexing with an *indexed name* (first byte in
 *    [0x01..0x0F]) — the existing tests only exercise the new-name path
 *    (first byte == 0x00).
 *  - Decoder literal-without-indexing where the indexed name resolves into
 *    the dynamic table after a prior literal-with-indexing emission.
 *  - Decoder literal-without-indexing error: indexed-name index references an
 *    empty dynamic table → get_indexed_header error 107.
 *  - Decoder literal-without-indexing missing value bytes after an indexed
 *    name (decode_string error after the index decodes successfully).
 *  - Decoder literal-without-indexing where the indexed-name index is 0 but
 *    the name-string read is truncated.
 *  - Decoder literal-with-indexing where indexed name references the dynamic
 *    table successfully.
 *  - Encoder dynamic-table indexed reference for header inserted earlier in
 *    the same encode() call (full-match dynamic lookup hit on second header).
 *  - dynamic_table::find with a value mismatch — name matches but value does
 *    not, exercising the "entry.name == name && entry.value != value" branch
 *    that returns std::nullopt.
 *  - dynamic_table::find with empty entries returning nullopt (already
 *    covered) plus dynamic_table::get out-of-bounds on a populated table.
 *
 * All tests are hermetic — no I/O, no threads, no sleeps.
 */

#include "internal/protocols/http2/hpack.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

namespace
{
    auto as_span(const std::vector<uint8_t>& bytes) -> std::span<const uint8_t>
    {
        return std::span<const uint8_t>(bytes);
    }
}

// ============================================================================
// Decoder: literal-without-indexing with indexed name (static table)
// ============================================================================

TEST(HpackDecoderLiteralWithoutIndexing, IndexedStaticNameProducesHeader)
{
    // First byte 0x0F is literal-without-indexing (top nibble 0x00) carrying
    // an index value of 0xF in the low 4-bit prefix. With the encoded prefix
    // continuation the actual index becomes 15 + 1 = 16 → static-table entry
    // {"accept-encoding", "gzip, deflate"}. The decoder uses only the name.
    // Then a 1-byte value of length 4 ("test") follows.
    std::vector<uint8_t> bytes = {0x0F, 0x01, 0x04, 't', 'e', 's', 't'};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "accept-encoding");
    EXPECT_EQ(result.value()[0].value, "test");
}

TEST(HpackDecoderLiteralWithoutIndexing, IndexedStaticNameSingleByte)
{
    // First byte 0x01 = literal-without-indexing with 4-bit index value 1 →
    // static-table entry 1 (":authority"). Value is empty (length 0).
    std::vector<uint8_t> bytes = {0x01, 0x00};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, ":authority");
    EXPECT_EQ(result.value()[0].value, "");
}

TEST(HpackDecoderLiteralWithoutIndexing, IndexedNameValueWithBytes)
{
    // 0x07 = literal-without-indexing index=7 → ":scheme" / "https" entry
    // (we only use the name). Value of length 4 = "demo".
    std::vector<uint8_t> bytes = {0x07, 0x04, 'd', 'e', 'm', 'o'};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, ":scheme");
    EXPECT_EQ(result.value()[0].value, "demo");
}

// ============================================================================
// Decoder: literal-without-indexing with indexed name in dynamic table
// ============================================================================

TEST(HpackDecoderLiteralWithoutIndexing, IndexedDynamicNameAfterPriorInsertion)
{
    // First decode a literal-with-indexing header so the decoder's dynamic
    // table receives a "x-tag" entry. The dynamic-table-relative index of the
    // freshly inserted header is 0; the absolute HPACK index is
    // static_table::size() + 1 + 0 = 62.
    //
    // 62 does not fit in a 4-bit prefix (max value 15), so the literal-
    // without-indexing prefix byte is 0x0F (value 15) followed by a
    // continuation byte encoding (62 - 15) = 47.
    std::vector<uint8_t> bytes = {
        // Literal-with-indexing, new name, name="x-tag", value="alpha"
        0x40,
        0x05, 'x', '-', 't', 'a', 'g',
        0x05, 'a', 'l', 'p', 'h', 'a',
        // Literal-without-indexing, indexed name = 62 (dynamic[0]), value="omega"
        0x0F, 0x2F,
        0x05, 'o', 'm', 'e', 'g', 'a',
    };
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0].name, "x-tag");
    EXPECT_EQ(result.value()[0].value, "alpha");
    EXPECT_EQ(result.value()[1].name, "x-tag");
    EXPECT_EQ(result.value()[1].value, "omega");
}

// ============================================================================
// Decoder: literal-without-indexing error paths
// ============================================================================

TEST(HpackDecoderLiteralWithoutIndexing, IndexedNameOutOfRangeFails)
{
    // 0x0F, 0x01: literal-without-indexing, index 15 + 1 = 16 (valid static
    // entry "accept-encoding"). But provide nothing after — the value-string
    // read should fail.
    std::vector<uint8_t> bytes = {0x0F, 0x01};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST(HpackDecoderLiteralWithoutIndexing, IndexedNameDynamicTableMissFails)
{
    // 0x0F, 0x01: index 16 is valid in the static table, but to test a
    // dynamic-table miss, use 0x0F, 0x32 → 15 + 50 = 65, which sits in the
    // dynamic table. The dynamic table is empty on a fresh decoder, so
    // get_indexed_header returns error 107.
    std::vector<uint8_t> bytes = {0x0F, 0x32};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST(HpackDecoderLiteralWithoutIndexing, NewNameTruncatedAfterPrefix)
{
    // 0x00 = literal-without-indexing, new name. Next byte is 0x05 (string
    // length 5) but no further bytes → decode_string fails.
    std::vector<uint8_t> bytes = {0x00, 0x05};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST(HpackDecoderLiteralWithoutIndexing, NewNameValidNameButTruncatedValue)
{
    // 0x00, name length 1, name byte 'k', then a value length of 5 but only
    // 2 actual bytes → decoder returns the decode_string error.
    std::vector<uint8_t> bytes = {0x00, 0x01, 'k', 0x05, 'a', 'b'};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Decoder: literal-with-indexing using a dynamic-table indexed name
// ============================================================================

TEST(HpackDecoderLiteralWithIndexing, IndexedDynamicNameSucceeds)
{
    // First insert "x-app" with a literal-with-indexing-new-name. Then
    // reference its name via a literal-with-indexing-indexed-name with a
    // 6-bit prefix (max 63). Dynamic[0] is index 62, which fits in 6 bits.
    std::vector<uint8_t> bytes = {
        // Literal-with-indexing, new name "x-app", value "v1"
        0x40,
        0x05, 'x', '-', 'a', 'p', 'p',
        0x02, 'v', '1',
        // Literal-with-indexing, indexed name = 62 → 0x40 | 62 = 0x7E,
        // value "v2"
        0x7E,
        0x02, 'v', '2',
    };
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0].name, "x-app");
    EXPECT_EQ(result.value()[0].value, "v1");
    EXPECT_EQ(result.value()[1].name, "x-app");
    EXPECT_EQ(result.value()[1].value, "v2");
}

TEST(HpackDecoderLiteralWithIndexing, IndexedNameRefuseDynamicMiss)
{
    // 0x7F (literal-with-indexing prefix 0x40 with 6-bit value 63 →
    // continuation needed) followed by 0x01 → final index 63 + 1 = 64. That
    // resolves to dynamic-table position 64 - 61 - 1 = 2, which does not
    // exist in a fresh decoder.
    std::vector<uint8_t> bytes = {0x7F, 0x01};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Encoder: dynamic-table reference for header inserted in same encode() call
// ============================================================================

TEST(HpackEncoderInternalReuse, RepeatedHeaderInSameEncodeCallEncodesIndexed)
{
    // Encoding two identical brand-new headers in a single encode() call:
    // first emission goes through literal-with-indexing-new-name and adds to
    // the dynamic table; second emission must hit the dynamic-table full
    // match branch and produce a single indexed byte.
    http2::hpack_encoder encoder;
    std::vector<http2::http_header> headers = {
        {"x-double", "twice"},
        {"x-double", "twice"},
    };
    auto encoded = encoder.encode(headers);
    ASSERT_FALSE(encoded.empty());
    // Last byte of the encoded sequence must be an indexed reference (high
    // bit set).
    EXPECT_NE((encoded.back() & 0x80), 0);
    EXPECT_GT(encoder.table_size(), 0u);
}

TEST(HpackEncoderInternalReuse, NameOnlyMatchInSameEncodeCallUsesDynamicName)
{
    // First header inserts "x-tag" into the dynamic table; second header has
    // the same name with a different value. The second emission must hit the
    // dynamic-table name-only branch and produce a literal-with-indexing-
    // indexed-name byte sequence (high bit clear, 0x40 bit set).
    http2::hpack_encoder encoder;
    std::vector<http2::http_header> headers = {
        {"x-tag", "first"},
        {"x-tag", "second"},
    };
    auto encoded = encoder.encode(headers);
    ASSERT_FALSE(encoded.empty());

    // Round-trip with a matching decoder to confirm semantics.
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0].name, "x-tag");
    EXPECT_EQ(result.value()[0].value, "first");
    EXPECT_EQ(result.value()[1].name, "x-tag");
    EXPECT_EQ(result.value()[1].value, "second");
}

// ============================================================================
// dynamic_table: branch coverage for find/get edge cases
// ============================================================================

TEST(HpackDynamicTableBranches, FindByNameAndValueMismatchReturnsNullopt)
{
    // Name matches a populated entry but the value does not — exercises the
    // "entry.name == name && !value.empty() && entry.value != value" branch
    // that falls through and ultimately returns nullopt.
    http2::dynamic_table table;
    table.insert("x-h", "alpha");
    table.insert("x-h", "beta");

    auto missing = table.find("x-h", "gamma");
    EXPECT_FALSE(missing.has_value());

    // Sanity: an actual match still works (covers the value-equal branch).
    auto found = table.find("x-h", "beta");
    ASSERT_TRUE(found.has_value());
}

TEST(HpackDynamicTableBranches, GetOutOfBoundsOnPopulatedTableReturnsNullopt)
{
    // Insert two entries then request index 2 (past the end). Exercises the
    // "index >= entries_.size()" true branch with a non-empty container.
    http2::dynamic_table table;
    table.insert("a", "1");
    table.insert("b", "2");
    EXPECT_FALSE(table.get(2).has_value());
    EXPECT_FALSE(table.get(99).has_value());
    // Sanity: in-bounds access still works.
    auto entry0 = table.get(0);
    ASSERT_TRUE(entry0.has_value());
    EXPECT_EQ(entry0->name, "b");
}

TEST(HpackDynamicTableBranches, FindOnEmptyTableShortCircuits)
{
    // Empty table with non-empty name argument exercises the loop-condition
    // false branch on the very first iteration check.
    http2::dynamic_table table;
    EXPECT_FALSE(table.find("anything").has_value());
    EXPECT_FALSE(table.find("anything", "value").has_value());
}

// ============================================================================
// static_table: out-of-range branch coverage
// ============================================================================

TEST(HpackStaticTableBranches, FindWithEmptyNameReturnsZero)
{
    // Searching for an empty header name should not match anything (entry 0
    // has empty name but the loop starts at 1, and other entries have
    // non-empty names). Exercises the "entry.name == name" false path
    // for all 61 entries.
    EXPECT_EQ(http2::static_table::find("", ""), 0u);
    EXPECT_EQ(http2::static_table::find("", "value"), 0u);
}

TEST(HpackStaticTableBranches, FindWithMatchingNameButWrongValueReturnsZero)
{
    // ":method" appears at indices 2 (GET) and 3 (POST). Searching with
    // "DELETE" must miss both — exercises the "value.empty() false &&
    // entry.value != value" branch on multiple iterations.
    EXPECT_EQ(http2::static_table::find(":method", "DELETE"), 0u);
}

TEST(HpackStaticTableBranches, FindWithEmptyValueMatchesFirstName)
{
    // ":status" appears at indices 8..14. Calling find with empty value
    // returns the first match (index 8) — covers the "value.empty() true"
    // branch reaching the success return.
    EXPECT_EQ(http2::static_table::find(":status", ""), 8u);
}

// ============================================================================
// Decoder: get_indexed_header static-table boundary
// ============================================================================

TEST(HpackDecoderIndexedHeader, MaxStaticIndexResolves)
{
    // Index 61 → static_table::get returns "www-authenticate". Indexed header
    // representation: high bit set, value = 61, but 61 fits in 7-bit prefix
    // only via continuation since prefix max is 127. 61 < 127 so single byte
    // 0x80 | 61 = 0xBD.
    std::vector<uint8_t> bytes = {0xBD};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "www-authenticate");
}

TEST(HpackDecoderIndexedHeader, FirstStaticIndexResolves)
{
    // Index 1 → ":authority". Single byte 0x80 | 1 = 0x81.
    std::vector<uint8_t> bytes = {0x81};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, ":authority");
    EXPECT_EQ(result.value()[0].value, "");
}

// ============================================================================
// Decoder: literal-never-indexed prefix bit, Huffman-flag string handling
// ============================================================================

TEST(HpackDecoderNeverIndexed, FirstByteWithBit10SetTakesElseBranch)
{
    // 0x10 = literal-never-indexed (top nibble 0001). The decoder enters the
    // same "literal without indexing" else-branch and uses the 4-bit prefix.
    // Index 0 (low nibble 0) → new-name path.
    std::vector<uint8_t> bytes = {
        0x10,
        0x03, 'a', 'b', 'c',
        0x02, 'x', 'y',
    };
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "abc");
    EXPECT_EQ(result.value()[0].value, "xy");
}

TEST(HpackDecoderNeverIndexed, FirstByteWithBit10AndIndexedName)
{
    // 0x12 = literal-never-indexed, low-nibble index 2 → static[2] (":method").
    std::vector<uint8_t> bytes = {0x12, 0x03, 'P', 'U', 'T'};
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, ":method");
    EXPECT_EQ(result.value()[0].value, "PUT");
}

TEST(HpackDecoderHuffmanFlag, StringWithHbitSetTakesHuffmanBranch)
{
    // Literal-with-indexing-new-name where both name and value strings carry
    // the H bit (0x80). The implementation treats the Huffman branch as a
    // passthrough, so the bytes after the length are returned verbatim — but
    // the H bit still flips decode_string's `huffman` boolean, exercising
    // the if(huffman) branch.
    std::vector<uint8_t> bytes = {
        0x40,
        0x83, 'a', 'b', 'c',
        0x82, 'X', 'Y',
    };
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "abc");
    EXPECT_EQ(result.value()[0].value, "XY");
}

TEST(HpackDecoderHuffmanFlag, IndexedHeaderFollowedByLiteralWithHbitValue)
{
    // Indexed :method GET (0x82) followed by a literal-with-indexing-new-name
    // whose value uses the Huffman bit. Confirms the H-bit branch survives
    // back-to-back decode operations.
    std::vector<uint8_t> bytes = {
        0x82,
        0x40,
        0x02, 'k', '1',
        0x81, 'Z',
    };
    http2::hpack_decoder decoder;
    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0].name, ":method");
    EXPECT_EQ(result.value()[0].value, "GET");
    EXPECT_EQ(result.value()[1].name, "k1");
    EXPECT_EQ(result.value()[1].value, "Z");
}

// ============================================================================
// Encoder: header with empty value matches static table when empty entry exists
// ============================================================================

TEST(HpackEncoderEmptyValue, HeaderWithEmptyValueHitsStaticEmptyEntry)
{
    // Static index 15 = {"accept-charset", ""}. Encoding {"accept-charset",""}
    // exercises static_table::find's "value.empty() short-circuit" success
    // branch and returns indexed-byte 0x8F.
    http2::hpack_encoder encoder;
    auto encoded = encoder.encode({{"accept-charset", ""}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x8F);
}

TEST(HpackEncoderEmptyValue, HeaderWithEmptyValueAuthority)
{
    // Static index 1 = {":authority", ""}. Empty value matches → 0x81.
    http2::hpack_encoder encoder;
    auto encoded = encoder.encode({{":authority", ""}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x81);
}
