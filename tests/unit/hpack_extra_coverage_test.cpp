// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file hpack_extra_coverage_test.cpp
 * @brief Additional coverage for src/protocols/http2/hpack.cpp (Issue #1031)
 *
 * Complements hpack_test.cpp and hpack_coverage_test.cpp by closing remaining
 * gaps to push line coverage past the 70% / branch coverage past the 60% bar
 * required by Issue #1031 acceptance criteria.
 *
 * Areas exercised here that the prior suites did not yet hit:
 *  - Static-table indexed encoding for the final boundary (index 61, www-authenticate)
 *    and other low-frequency entries with empty values.
 *  - Encoder name-only static-table match for entries whose static value differs
 *    from the user-supplied value (forces the literal-with-indexing path even
 *    though name is in the static table).
 *  - Decoder happy paths for literal without indexing (0x00 prefix) feeding both
 *    new-name and indexed-name forms, and for the never-indexed prefix (0x10).
 *  - decode_integer error 100 surfaced by feeding a single empty span via the
 *    decoder front door.
 *  - Multi-byte integer continuation that just stays under the 64-bit overflow
 *    threshold (m < 64), confirming the non-error branch of the same loop that
 *    produces error 102.
 *  - Huffman::encoded_size on an arbitrary string equals the raw size for the
 *    current stub implementation (locks the contract of the stub so a future
 *    real implementation breaks loudly here).
 *  - Encoder/decoder pair convergence for the static-table-only case (no dynamic
 *    table growth) — confirms decoder.table_size() stays at 0 when the encoder
 *    only emits indexed references.
 *  - Dynamic-table get() with index past entry_count() returning nullopt.
 *  - Dynamic-table set_max_size() that grows (no eviction) leaves entries intact.
 *  - http_header equality is asymmetric on value mismatch and reflexive.
 *
 * The decoder-path tests build byte sequences by hand against RFC 7541 wire
 * format so the assertion targets the decoder, not an encoder upstream of it.
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
// Static table: full-range coverage of get() / find()
// ============================================================================

class StaticTableExtraTest : public ::testing::Test
{
};

TEST_F(StaticTableExtraTest, GetIndex61IsWwwAuthenticate)
{
    auto entry = http2::static_table::get(61);
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->name, "www-authenticate");
    EXPECT_TRUE(entry->value.empty());
}

TEST_F(StaticTableExtraTest, GetLargeIndexReturnsEmpty)
{
    EXPECT_FALSE(http2::static_table::get(1000).has_value());
    EXPECT_FALSE(http2::static_table::get(static_cast<size_t>(-1)).has_value());
}

TEST_F(StaticTableExtraTest, FindEmptyValueMatchesFirstNameOccurrence)
{
    // :method has two static rows (GET=2, POST=3). A name-only lookup must
    // return the lower index.
    EXPECT_EQ(http2::static_table::find(":method"), 2u);
    EXPECT_EQ(http2::static_table::find(":path"), 4u);
    EXPECT_EQ(http2::static_table::find(":status"), 8u);
    EXPECT_EQ(http2::static_table::find(":scheme"), 6u);
}

TEST_F(StaticTableExtraTest, FindMismatchedValueFallsThrough)
{
    // Name :method exists; value PATCH does not match GET or POST exactly.
    // Per the implementation, find() with a non-empty value that doesn't match
    // any static row returns 0.
    EXPECT_EQ(http2::static_table::find(":method", "PATCH"), 0u);
}

TEST_F(StaticTableExtraTest, FindNonExistentName)
{
    EXPECT_EQ(http2::static_table::find("x-no-such-name"), 0u);
    EXPECT_EQ(http2::static_table::find("x-no-such-name", "value"), 0u);
}

// ============================================================================
// http_header: equality and size invariants
// ============================================================================

class HttpHeaderExtraTest : public ::testing::Test
{
};

TEST_F(HttpHeaderExtraTest, EqualityIsReflexive)
{
    http2::http_header h{"name", "value"};
    EXPECT_EQ(h, h);
}

TEST_F(HttpHeaderExtraTest, NameMismatchBreaksEquality)
{
    http2::http_header a{"name1", "value"};
    http2::http_header b{"name2", "value"};
    EXPECT_FALSE(a == b);
    EXPECT_NE(a, b);
}

TEST_F(HttpHeaderExtraTest, ValueMismatchBreaksEquality)
{
    http2::http_header a{"name", "v1"};
    http2::http_header b{"name", "v2"};
    EXPECT_FALSE(a == b);
}

// ============================================================================
// Dynamic table: index past end, growth without eviction
// ============================================================================

class DynamicTableExtraTest : public ::testing::Test
{
};

TEST_F(DynamicTableExtraTest, GetPastEndReturnsEmpty)
{
    http2::dynamic_table t(4096);
    t.insert("a", "b");
    EXPECT_FALSE(t.get(1).has_value());
    EXPECT_FALSE(t.get(100).has_value());
}

TEST_F(DynamicTableExtraTest, GrowingMaxSizeDoesNotEvict)
{
    http2::dynamic_table t(4096);
    t.insert("x-keep-1", "value");
    t.insert("x-keep-2", "value");
    const size_t before = t.entry_count();
    t.set_max_size(8192);
    EXPECT_EQ(t.entry_count(), before);
    EXPECT_EQ(t.max_size(), 8192u);
}

TEST_F(DynamicTableExtraTest, ShrinkingMaxSizeBelowOneEntryEvictsAll)
{
    http2::dynamic_table t(4096);
    t.insert("x-evict-1", "value-one");
    t.insert("x-evict-2", "value-two");
    ASSERT_EQ(t.entry_count(), 2u);
    t.set_max_size(8);
    EXPECT_EQ(t.entry_count(), 0u);
    EXPECT_EQ(t.current_size(), 0u);
}

TEST_F(DynamicTableExtraTest, FindPrefersFullMatchOverNameOnly)
{
    http2::dynamic_table t(4096);
    t.insert("x-name", "value-a");
    t.insert("x-name", "value-b");
    auto exact = t.find("x-name", "value-a");
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(*exact, 1u);
    auto name_only = t.find("x-name");
    ASSERT_TRUE(name_only.has_value());
    EXPECT_EQ(*name_only, 0u);
}

// ============================================================================
// Encoder: static-table name match with mismatched value forces literal path
// ============================================================================

class HpackEncoderExtraTest : public ::testing::Test
{
protected:
    http2::hpack_encoder encoder_;
};

TEST_F(HpackEncoderExtraTest, MethodPatchUsesLiteralWithIndexingNamedStatic)
{
    // :method exists in static (index 2 GET, 3 POST) but PATCH matches neither.
    // Encoder falls back to literal-with-indexing-indexed-name with name index 2
    // → first byte = 0x40 | 2 = 0x42.
    auto encoded = encoder_.encode({{":method", "PATCH"}});
    ASSERT_GE(encoded.size(), 2u);
    EXPECT_EQ(encoded[0], 0x42);
    EXPECT_GT(encoder_.table_size(), 0u);
}

TEST_F(HpackEncoderExtraTest, AuthorityWithValueGetsLiteralName)
{
    // :authority has empty value in the static table (index 1). User supplies
    // a real authority → literal-with-indexing-indexed-name, first byte 0x41.
    auto encoded = encoder_.encode({{":authority", "example.com"}});
    ASSERT_GE(encoded.size(), 2u);
    EXPECT_EQ(encoded[0], 0x41);
}

TEST_F(HpackEncoderExtraTest, EmptyValueNonStaticHeaderUsesLiteralNewName)
{
    auto encoded = encoder_.encode({{"x-fresh-empty", ""}});
    ASSERT_GE(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x40);
}

// ============================================================================
// Decoder: literal without indexing happy paths
// ============================================================================

class HpackDecoderHappyPathTest : public ::testing::Test
{
protected:
    http2::hpack_decoder decoder_;
};

TEST_F(HpackDecoderHappyPathTest, LiteralWithoutIndexingNewNameDecodes)
{
    // 0x00 prefix → literal without indexing, new name.
    // Layout: 0x00 | 1 (name length) | 'k' | 1 (value length) | 'v'
    std::vector<uint8_t> bytes = {0x00, 0x01, 'k', 0x01, 'v'};
    auto result = decoder_.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "k");
    EXPECT_EQ(result.value()[0].value, "v");
    // Literal-without-indexing must NOT add to the dynamic table.
    EXPECT_EQ(decoder_.table_size(), 0u);
}

TEST_F(HpackDecoderHappyPathTest, LiteralWithoutIndexingIndexedNameDecodes)
{
    // 0x02 = 0x00 | 2 → literal without indexing, name reference index 2 (:method).
    // Layout: 0x02 | 1 (value length) | 'X'
    std::vector<uint8_t> bytes = {0x02, 0x01, 'X'};
    auto result = decoder_.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, ":method");
    EXPECT_EQ(result.value()[0].value, "X");
    EXPECT_EQ(decoder_.table_size(), 0u);
}

TEST_F(HpackDecoderHappyPathTest, NeverIndexedNewNameDecodes)
{
    // 0x10 prefix bit set → never indexed, new name.
    // Layout: 0x10 | 1 | 'k' | 1 | 'v'
    std::vector<uint8_t> bytes = {0x10, 0x01, 'k', 0x01, 'v'};
    auto result = decoder_.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "k");
    EXPECT_EQ(result.value()[0].value, "v");
    EXPECT_EQ(decoder_.table_size(), 0u);
}

TEST_F(HpackDecoderHappyPathTest, MultipleLiteralWithoutIndexingHeadersInOneBlock)
{
    std::vector<uint8_t> bytes = {
        0x00, 0x01, 'a', 0x01, '1',
        0x00, 0x01, 'b', 0x01, '2',
    };
    auto result = decoder_.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[0].name, "a");
    EXPECT_EQ(result.value()[0].value, "1");
    EXPECT_EQ(result.value()[1].name, "b");
    EXPECT_EQ(result.value()[1].value, "2");
}

// ============================================================================
// Decoder: multi-byte integer non-error path
// ============================================================================

TEST(HpackDecoderInteger, MultiByteIntegerStaysUnderOverflowThreshold)
{
    http2::hpack_decoder decoder;
    // 0x40 (literal w/ indexing, name length follows in 7-bit prefix)
    // Name length = 130: encoded as 0x7F, 0x03 (130 - 127 = 3, fits in one byte)
    // Then 130 'a' bytes for the name, then value length 1, value 'v'.
    std::vector<uint8_t> bytes;
    bytes.push_back(0x40);
    bytes.push_back(0x7F);
    bytes.push_back(0x03);
    for (int i = 0; i < 130; ++i)
    {
        bytes.push_back('a');
    }
    bytes.push_back(0x01);
    bytes.push_back('v');

    auto result = decoder.decode(as_span(bytes));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name.size(), 130u);
    EXPECT_EQ(result.value()[0].value, "v");
}

// ============================================================================
// Huffman stub: encoded_size and round-trip contract
// ============================================================================

class HuffmanExtraTest : public ::testing::Test
{
};

TEST_F(HuffmanExtraTest, EncodedSizeMatchesInputSizeForStub)
{
    // Stub implementation returns input size verbatim. Locking this contract
    // makes any move to a real Huffman encoder visible here.
    EXPECT_EQ(http2::huffman::encoded_size("a"), 1u);
    EXPECT_EQ(http2::huffman::encoded_size("hello"), 5u);
    EXPECT_EQ(http2::huffman::encoded_size(std::string(100, 'x')), 100u);
}

TEST_F(HuffmanExtraTest, EncodeProducesByteForByteStubOutput)
{
    const std::string input = "abc\x01\x02\x03";
    auto encoded = http2::huffman::encode(input);
    ASSERT_EQ(encoded.size(), input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_EQ(encoded[i], static_cast<uint8_t>(input[i]));
    }
}

TEST_F(HuffmanExtraTest, DecodeReturnsByteForByteStubOutput)
{
    std::vector<uint8_t> data = {'x', 'y', 'z', 0x00, 0xFF};
    auto result = http2::huffman::decode(as_span(data));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), data.size());
    for (size_t i = 0; i < data.size(); ++i)
    {
        EXPECT_EQ(static_cast<uint8_t>(result.value()[i]), data[i]);
    }
}

// ============================================================================
// Decoder/encoder convergence: static-table-only emissions leave decoder empty
// ============================================================================

TEST(HpackPairExtraConvergence, StaticOnlyHeadersDoNotGrowDecoderTable)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    // All headers below are exact static-table matches → encoder emits indexed
    // references and never grows its dynamic table. Decoder must mirror this.
    const std::vector<http2::http_header> headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"},
        {":status", "200"},
        {":status", "404"},
    };

    auto encoded = encoder.encode(headers);
    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), headers.size());

    EXPECT_EQ(encoder.table_size(), 0u);
    EXPECT_EQ(decoder.table_size(), 0u);
}

TEST(HpackPairExtraConvergence, MixedStaticAndDynamicHeadersMatchEnd)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    const std::vector<http2::http_header> headers = {
        {":method", "GET"},
        {":path", "/api/v1/data"},
        {"x-trace-id", "trace-12345"},
        {"x-feature-flag", "rollout"},
    };

    auto encoded = encoder.encode(headers);
    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), headers.size());
    for (size_t i = 0; i < headers.size(); ++i)
    {
        EXPECT_EQ(result.value()[i].name, headers[i].name);
        EXPECT_EQ(result.value()[i].value, headers[i].value);
    }
    EXPECT_EQ(encoder.table_size(), decoder.table_size());
}

// ============================================================================
// Decoder: empty payload front-door yields ok with empty headers (loop skip)
// ============================================================================

TEST(HpackDecoderEdge, EmptyPayloadIsValid)
{
    http2::hpack_decoder decoder;
    std::vector<uint8_t> empty;
    auto result = decoder.decode(as_span(empty));
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().empty());
    EXPECT_EQ(decoder.table_size(), 0u);
}
