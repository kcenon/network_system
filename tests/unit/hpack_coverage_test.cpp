// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file hpack_coverage_test.cpp
 * @brief Extended unit tests for src/protocols/http2/hpack.cpp (Issue #1009)
 *
 * Raises coverage of the HPACK translation unit by exercising paths that the
 * existing hpack_test.cpp does not reach:
 *  - Encoder static-table indexed-byte representation for common pseudo-headers
 *  - Encoder literal-with-indexing using a static-table name match
 *  - Encoder dynamic-table population and re-emission as indexed reference
 *  - Eviction during encode under a tight max_table_size
 *  - set_max_table_size shrinking the dynamic table to zero
 *  - Round-trip across header value/name lengths spanning the 7-bit length
 *    prefix boundary (forces multi-byte integer encoding for the length field)
 *  - Round-trip preserving raw binary bytes (NUL, high bytes, full byte sweep)
 *  - Decoder error paths: indexed-zero (code 105), incomplete integer (101),
 *    insufficient string data (104), invalid dynamic table index (107),
 *    and integer overflow (102)
 *  - Independent encoder instances producing identical output
 *  - A single decoder accumulating dynamic table state across multiple calls
 *  - Encoder/decoder pair table-size convergence after redundant rounds
 *
 * The decoder error-path tests construct byte sequences by hand against the
 * RFC 7541 wire format; they intentionally avoid relying on encoder output
 * for the malformed cases so the failure path is the test, not the producer.
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
    // Produce a span over a raw byte sequence; std::span<const uint8_t>
    // construction from a temporary vector requires the vector to outlive
    // the span, so callers always pass an lvalue.
    auto as_span(const std::vector<uint8_t>& bytes) -> std::span<const uint8_t>
    {
        return std::span<const uint8_t>(bytes);
    }
}

// ============================================================================
// Encoder: static-table indexed-byte representation
// ============================================================================

class HpackEncoderStaticTableTest : public ::testing::Test
{
protected:
    http2::hpack_encoder encoder_;
};

TEST_F(HpackEncoderStaticTableTest, MethodGetEncodesAsSingleByte)
{
    // RFC 7541 §6.1: indexed header field = 1xxxxxxx where xxxxxxx is the
    // 7-bit static-table index. :method GET is index 2 → 0x82.
    auto encoded = encoder_.encode({{":method", "GET"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x82);
}

TEST_F(HpackEncoderStaticTableTest, MethodPostEncodesAsSingleByte)
{
    auto encoded = encoder_.encode({{":method", "POST"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x83);
}

TEST_F(HpackEncoderStaticTableTest, PathSlashEncodesAsSingleByte)
{
    auto encoded = encoder_.encode({{":path", "/"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x84);
}

TEST_F(HpackEncoderStaticTableTest, SchemeHttpsEncodesAsSingleByte)
{
    auto encoded = encoder_.encode({{":scheme", "https"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x87);
}

TEST_F(HpackEncoderStaticTableTest, Status200EncodesAsSingleByte)
{
    auto encoded = encoder_.encode({{":status", "200"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x88);
}

TEST_F(HpackEncoderStaticTableTest, Status404EncodesAsSingleByte)
{
    auto encoded = encoder_.encode({{":status", "404"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x8d);
}

TEST_F(HpackEncoderStaticTableTest, AcceptEncodingGzipDeflateEncodesAsSingleByte)
{
    // accept-encoding "gzip, deflate" is index 16 → 0x90
    auto encoded = encoder_.encode({{"accept-encoding", "gzip, deflate"}});
    ASSERT_EQ(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x90);
}

// ============================================================================
// Encoder: static-table name match, literal value
// ============================================================================

TEST_F(HpackEncoderStaticTableTest, ContentTypeJsonUsesStaticNameLiteralValue)
{
    // content-type is static index 31. The encoder takes the literal-with-
    // indexing-indexed-name path (6-bit prefix, 0x40 high nibble). First byte
    // becomes 0x40 | 31 = 0x5f.
    auto encoded = encoder_.encode({{"content-type", "application/json"}});
    ASSERT_GE(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x5f);
    // Encoder should add the header to its dynamic table after literal-with-
    // indexing emission.
    EXPECT_GT(encoder_.table_size(), 0u);
}

TEST_F(HpackEncoderStaticTableTest, AcceptStarStarUsesStaticNameLiteralValue)
{
    // accept is static index 19 → first byte 0x40 | 19 = 0x53.
    auto encoded = encoder_.encode({{"accept", "*/*"}});
    ASSERT_GE(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x53);
}

// ============================================================================
// Encoder: brand-new header uses literal-with-indexing-new-name
// ============================================================================

TEST_F(HpackEncoderStaticTableTest, NewHeaderUsesLiteralWithIndexingNewName)
{
    // Brand-new name not in static or dynamic table → first byte is 0x40
    // (literal with incremental indexing, new name) per RFC 7541 §6.2.1.
    auto encoded = encoder_.encode({{"x-completely-new", "value"}});
    ASSERT_GE(encoded.size(), 1u);
    EXPECT_EQ(encoded[0], 0x40);
    EXPECT_GT(encoder_.table_size(), 0u);
}

// ============================================================================
// Encoder: dynamic-table reuse on repeat
// ============================================================================

TEST(HpackEncoderDynamicTable, RepeatHeaderEncodesAsIndexedReference)
{
    http2::hpack_encoder encoder;

    auto first = encoder.encode({{"x-app-token", "abcdefgh"}});
    ASSERT_GE(first.size(), 2u) << "literal-with-indexing should be multi-byte";

    auto second = encoder.encode({{"x-app-token", "abcdefgh"}});
    ASSERT_EQ(second.size(), 1u)
        << "second emission must be a single indexed byte";
    EXPECT_NE((second[0] & 0x80), 0)
        << "indexed representation has the high bit set";
}

TEST(HpackEncoderDynamicTable, RepeatedHeaderUsesNameMatchOnly)
{
    http2::hpack_encoder encoder;

    encoder.encode({{"x-app-token", "first-value"}});
    auto second = encoder.encode({{"x-app-token", "second-value"}});
    // Same name with a different value → encoder uses literal-with-indexing
    // referencing the dynamic-table name. First byte is the 0x40 prefix
    // followed by the dynamic-table name index in 6-bit form.
    ASSERT_GE(second.size(), 1u);
    EXPECT_NE((second[0] & 0x40), 0);
    EXPECT_EQ((second[0] & 0x80), 0);
}

// ============================================================================
// Encoder: eviction during encode under tight max_table_size
// ============================================================================

TEST(HpackEncoderEviction, TightMaxSizeKeepsTableUnderLimit)
{
    constexpr size_t k_max_size = 100;
    http2::hpack_encoder encoder(k_max_size);

    // Each of these headers is well over 32 bytes (the per-entry RFC overhead
    // alone), so inserting all four would blow past 100 bytes and force
    // eviction inside encode().
    std::vector<http2::http_header> headers = {
        {"x-header-one", "valueone"},
        {"x-header-two", "valuetwo"},
        {"x-header-three", "valuethree"},
        {"x-header-four", "valuefour"},
    };

    auto encoded = encoder.encode(headers);
    EXPECT_FALSE(encoded.empty());
    EXPECT_LE(encoder.table_size(), k_max_size);
}

TEST(HpackEncoderEviction, ExtremelySmallMaxSizeStillProducesDecodableBytes)
{
    // A max_table_size below the per-entry RFC overhead (32 bytes) means no
    // entry can ever truly fit. The current implementation does not reject
    // oversized inserts — it admits them and lets current_size_ exceed
    // max_size_ — so this test only verifies that encode/decode keep working
    // end-to-end at the wire level, not that the table stays under its limit.
    http2::hpack_encoder encoder(16);
    http2::hpack_decoder decoder(16);

    auto encoded = encoder.encode({{"x-tag", "v"}});
    ASSERT_FALSE(encoded.empty());

    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, "x-tag");
    EXPECT_EQ(result.value()[0].value, "v");
}

// ============================================================================
// Encoder: set_max_table_size shrinks an already-populated table
// ============================================================================

TEST(HpackEncoderShrink, SetMaxToZeroEvictsAllEntries)
{
    http2::hpack_encoder encoder;
    encoder.encode({
        {"x-a", "1"},
        {"x-b", "2"},
        {"x-c", "3"},
    });
    ASSERT_GT(encoder.table_size(), 0u);

    encoder.set_max_table_size(0);
    EXPECT_EQ(encoder.table_size(), 0u);
}

TEST(HpackEncoderShrink, SetMaxToSmallValueRetainsRecentEntriesOnly)
{
    http2::hpack_encoder encoder;
    encoder.encode({
        {"x-old", "value-old"},
        {"x-mid", "value-mid"},
        {"x-new", "value-new"},
    });
    const size_t populated = encoder.table_size();
    ASSERT_GT(populated, 0u);

    // Shrink to roughly one entry's worth.
    encoder.set_max_table_size(48);
    EXPECT_LE(encoder.table_size(), 48u);
}

// ============================================================================
// Round-trip: integer-length boundary for string-length prefix
// ============================================================================

class HpackLengthBoundaryTest : public ::testing::Test
{
protected:
    http2::hpack_encoder encoder_;
    http2::hpack_decoder decoder_;

    void RoundTripValueOfSize(size_t value_len)
    {
        std::string value(value_len, 'a');
        auto encoded = encoder_.encode({{"x-payload", value}});
        ASSERT_FALSE(encoded.empty());

        auto result = decoder_.decode(as_span(encoded));
        ASSERT_TRUE(result.is_ok())
            << "round-trip failed at value_len=" << value_len;
        ASSERT_EQ(result.value().size(), 1u);
        EXPECT_EQ(result.value()[0].name, "x-payload");
        EXPECT_EQ(result.value()[0].value, value);
    }
};

TEST_F(HpackLengthBoundaryTest, ValueLength126FitsInSevenBitPrefix)
{
    // Length = 126 < 127 (max_prefix for 7-bit) → single-byte length encoding.
    RoundTripValueOfSize(126);
}

TEST_F(HpackLengthBoundaryTest, ValueLength127TriggersContinuationByte)
{
    // Length = 127 == max_prefix → first byte is 0x7F, continuation follows.
    RoundTripValueOfSize(127);
}

TEST_F(HpackLengthBoundaryTest, ValueLength200UsesMultiByteEncoding)
{
    RoundTripValueOfSize(200);
}

TEST_F(HpackLengthBoundaryTest, ValueLength1000SpansMultipleContinuations)
{
    // 1000 = 127 + 873; 873 / 128 = 6 with remainder 105 → 2 continuation
    // bytes plus terminator after the prefix.
    RoundTripValueOfSize(1000);
}

TEST_F(HpackLengthBoundaryTest, ValueLength16384LargePayload)
{
    RoundTripValueOfSize(16384);
}

TEST_F(HpackLengthBoundaryTest, NameLength200RoundTripsViaLiteralName)
{
    std::string long_name = "x-" + std::string(198, 'k');
    auto encoded = encoder_.encode({{long_name, "v"}});
    auto result = decoder_.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].name, long_name);
    EXPECT_EQ(result.value()[0].value, "v");
}

// ============================================================================
// Round-trip: raw byte preservation
// ============================================================================

TEST(HpackBinaryPreservation, ValueWithNulBytesRoundTrips)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    std::string value;
    value.push_back('a');
    value.push_back('\0');
    value.push_back('b');
    value.push_back('\0');
    value.push_back('c');

    auto encoded = encoder.encode({{"x-binary", value}});
    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].value, value);
}

TEST(HpackBinaryPreservation, ValueWithFullByteRangeRoundTrips)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    std::string value;
    value.reserve(256);
    for (int i = 0; i < 256; ++i)
    {
        value.push_back(static_cast<char>(i));
    }

    auto encoded = encoder.encode({{"x-bytes", value}});
    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].value, value);
    EXPECT_EQ(result.value()[0].value.size(), 256u);
}

TEST(HpackBinaryPreservation, ValueWithUtf8RoundTripsAsRawBytes)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    // The byte sequence below is a UTF-8 string; HPACK is byte-oriented so
    // it should arrive bit-identical even though the implementation treats
    // it as opaque bytes.
    const std::string utf8 = "\xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x9a\x80";
    auto encoded = encoder.encode({{"x-i18n", utf8}});
    auto result = decoder.decode(as_span(encoded));
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0].value, utf8);
}

// ============================================================================
// Decoder: error paths
// ============================================================================

class HpackDecoderErrorTest : public ::testing::Test
{
protected:
    http2::hpack_decoder decoder_;
};

TEST_F(HpackDecoderErrorTest, IndexedZeroIsRejected)
{
    // First byte 0x80: indexed (high bit set), 7-bit value = 0 → invalid per
    // RFC 7541 §6.1 (index 0 is not a valid header reference).
    std::vector<uint8_t> bytes = {0x80};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, IndexedHighValueRequiringContinuationButTruncated)
{
    // First byte 0xFF: indexed prefix, value = 127 (max_prefix) signals a
    // continuation byte, but no further byte exists. Should yield error 101.
    std::vector<uint8_t> bytes = {0xFF};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, IndexedReferencingEmptyDynamicTableFails)
{
    // 0xC1 = 0x80 | 0x41 → indexed, value = 65. Static table covers 1..61, so
    // 65 maps to dynamic-table index (65 - 61 - 1) = 3, which does not exist
    // in a freshly constructed decoder → error 107.
    std::vector<uint8_t> bytes = {0xC1};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, LiteralWithIndexingTruncatedNameLength)
{
    // 0x40 (literal with indexing, new name) followed by 0x7F which signals
    // a multi-byte string length but no continuation byte exists.
    std::vector<uint8_t> bytes = {0x40, 0x7F};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, LiteralWithIndexingDeclaredLengthExceedsRemaining)
{
    // 0x40 (literal new name), name length = 5, but only 3 bytes follow →
    // error 104 ("Insufficient data for string value").
    std::vector<uint8_t> bytes = {0x40, 0x05, 'a', 'b', 'c'};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, LiteralWithIndexingMissingValueAfterName)
{
    // 0x40 (literal new name), name length 1, name byte 'k', then EOF before
    // the value-length byte → decoder returns error from decode_string.
    std::vector<uint8_t> bytes = {0x40, 0x01, 'k'};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, IntegerOverflowFromExcessiveContinuationBytes)
{
    // First byte 0xFF: indexed, value seed = 127. Each subsequent 0xFF
    // continues with 7 bits. The decoder accumulates m += 7 and bails out
    // when m >= 64 → error 102. We provide enough continuation bytes to
    // trip the overflow (m starts at 0 so 10 iterations push m to 70).
    std::vector<uint8_t> bytes = {
        0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x01,
    };
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackDecoderErrorTest, LiteralWithoutIndexingTruncatedFails)
{
    // 0x00 (literal without indexing, new name) with a length-5 name but only
    // 2 bytes available afterward.
    std::vector<uint8_t> bytes = {0x00, 0x05, 'x', 'y'};
    auto result = decoder_.decode(as_span(bytes));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Decoder: dynamic-table state across multiple decode calls
// ============================================================================

TEST(HpackDecoderState, MultipleDecodeCallsAccumulateDynamicTable)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    auto first = encoder.encode({{"x-stash", "abc"}});
    auto firstResult = decoder.decode(as_span(first));
    ASSERT_TRUE(firstResult.is_ok());
    const size_t after_first = decoder.table_size();
    EXPECT_GT(after_first, 0u);

    auto second = encoder.encode({{"x-other", "def"}});
    auto secondResult = decoder.decode(as_span(second));
    ASSERT_TRUE(secondResult.is_ok());
    EXPECT_GT(decoder.table_size(), after_first);
}

// ============================================================================
// Encoder/decoder pair: convergence
// ============================================================================

TEST(HpackPairConvergence, RedundantHeadersShrinkSecondEncoding)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder decoder;

    std::vector<http2::http_header> headers = {
        {"x-app", "alpha"},
        {"x-svc", "beta"},
        {"x-env", "prod"},
    };

    auto first_bytes = encoder.encode(headers);
    auto first_result = decoder.decode(as_span(first_bytes));
    ASSERT_TRUE(first_result.is_ok());
    ASSERT_EQ(first_result.value().size(), headers.size());

    auto second_bytes = encoder.encode(headers);
    EXPECT_LT(second_bytes.size(), first_bytes.size())
        << "second emission must benefit from dynamic-table indexing";

    auto second_result = decoder.decode(as_span(second_bytes));
    ASSERT_TRUE(second_result.is_ok());
    ASSERT_EQ(second_result.value().size(), headers.size());
    for (size_t i = 0; i < headers.size(); ++i)
    {
        EXPECT_EQ(second_result.value()[i].name, headers[i].name);
        EXPECT_EQ(second_result.value()[i].value, headers[i].value);
    }

    EXPECT_EQ(encoder.table_size(), decoder.table_size())
        << "encoder and decoder must agree on dynamic table footprint";
}

TEST(HpackPairConvergence, DistinctEncodersProduceIdenticalBytesForFreshState)
{
    http2::hpack_encoder e1;
    http2::hpack_encoder e2;
    const std::vector<http2::http_header> headers = {
        {":method", "GET"},
        {":scheme", "https"},
        {":path", "/"},
        {":status", "200"},
    };

    EXPECT_EQ(e1.encode(headers), e2.encode(headers));
    EXPECT_EQ(e1.table_size(), e2.table_size());
}

// ============================================================================
// Independent decoder instances
// ============================================================================

TEST(HpackDecoderIndependence, TwoDecodersMaintainSeparateState)
{
    http2::hpack_encoder encoder;
    http2::hpack_decoder d1;
    http2::hpack_decoder d2;

    auto encoded = encoder.encode({{"x-only-d1", "value"}});

    auto r1 = d1.decode(as_span(encoded));
    ASSERT_TRUE(r1.is_ok());
    EXPECT_GT(d1.table_size(), 0u);
    EXPECT_EQ(d2.table_size(), 0u)
        << "d2 must not be affected by decoding into d1";
}
