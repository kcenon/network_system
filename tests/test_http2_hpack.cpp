/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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
#include "internal/protocols/http2/hpack.h"
#include <vector>
#include <string>

using namespace kcenon::network::protocols::http2;

class HpackTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HpackTest, StaticTableLookup)
{
    // Test some known static table entries
    auto header1 = static_table::get(1);
    ASSERT_TRUE(header1.has_value());
    EXPECT_EQ(header1->name, ":authority");
    EXPECT_EQ(header1->value, "");

    auto header2 = static_table::get(2);
    ASSERT_TRUE(header2.has_value());
    EXPECT_EQ(header2->name, ":method");
    EXPECT_EQ(header2->value, "GET");

    auto header8 = static_table::get(8);
    ASSERT_TRUE(header8.has_value());
    EXPECT_EQ(header8->name, ":status");
    EXPECT_EQ(header8->value, "200");

    // Invalid index
    auto header0 = static_table::get(0);
    EXPECT_FALSE(header0.has_value());

    auto header100 = static_table::get(100);
    EXPECT_FALSE(header100.has_value());
}

TEST_F(HpackTest, StaticTableFind)
{
    // Find exact match
    auto index1 = static_table::find(":method", "GET");
    EXPECT_EQ(index1, 2u);

    auto index2 = static_table::find(":status", "200");
    EXPECT_EQ(index2, 8u);

    // Find name only
    auto index3 = static_table::find(":authority");
    EXPECT_EQ(index3, 1u);

    // Not found
    auto index4 = static_table::find("custom-header");
    EXPECT_EQ(index4, 0u);
}

TEST_F(HpackTest, DynamicTableBasicOperations)
{
    dynamic_table table(4096);

    EXPECT_EQ(table.entry_count(), 0u);
    EXPECT_EQ(table.current_size(), 0u);

    // Insert entry
    table.insert("custom-key", "custom-value");
    EXPECT_EQ(table.entry_count(), 1u);
    EXPECT_GT(table.current_size(), 0u);

    // Get entry
    auto header = table.get(0);
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->name, "custom-key");
    EXPECT_EQ(header->value, "custom-value");

    // Find entry
    auto index = table.find("custom-key", "custom-value");
    ASSERT_TRUE(index.has_value());
    EXPECT_EQ(index.value(), 0u);

    // Not found
    auto not_found = table.find("other-key");
    EXPECT_FALSE(not_found.has_value());
}

TEST_F(HpackTest, DynamicTableEviction)
{
    dynamic_table table(100);  // Small size for testing eviction

    // Insert multiple entries
    table.insert("key1", "value1");
    table.insert("key2", "value2");
    table.insert("key3", "value3");
    table.insert("key4", "value4");

    // Some entries should have been evicted
    EXPECT_LE(table.current_size(), 100u);

    // First entry (key1) should be evicted, last entry (key4) should be present
    auto last = table.get(0);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->name, "key4");
}

TEST_F(HpackTest, EncodesIndexedHeader)
{
    hpack_encoder encoder;

    // Encode :method GET (static table index 2)
    std::vector<http_header> headers = {
        {":method", "GET"}
    };

    auto encoded = encoder.encode(headers);

    // Should be indexed representation: 10000010 (0x82)
    ASSERT_FALSE(encoded.empty());
    EXPECT_EQ(encoded[0], 0x82);
}

TEST_F(HpackTest, EncodesLiteralWithIndexing)
{
    hpack_encoder encoder;

    // Encode custom header
    std::vector<http_header> headers = {
        {"custom-key", "custom-value"}
    };

    auto encoded = encoder.encode(headers);

    // Should start with 01 pattern (literal with incremental indexing)
    ASSERT_FALSE(encoded.empty());
    EXPECT_EQ(encoded[0] & 0xC0, 0x40);
}

TEST_F(HpackTest, EncodesAndDecodesStaticHeaders)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> original = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"}
    };

    auto encoded = encoder.encode(original);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();

    ASSERT_EQ(decoded.size(), 3u);
    EXPECT_EQ(decoded[0].name, ":method");
    EXPECT_EQ(decoded[0].value, "GET");
    EXPECT_EQ(decoded[1].name, ":path");
    EXPECT_EQ(decoded[1].value, "/");
    EXPECT_EQ(decoded[2].name, ":scheme");
    EXPECT_EQ(decoded[2].value, "https");
}

TEST_F(HpackTest, EncodesAndDecodesCustomHeaders)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> original = {
        {"custom-key", "custom-value"},
        {"another-key", "another-value"}
    };

    auto encoded = encoder.encode(original);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();

    ASSERT_EQ(decoded.size(), 2u);
    EXPECT_EQ(decoded[0].name, "custom-key");
    EXPECT_EQ(decoded[0].value, "custom-value");
    EXPECT_EQ(decoded[1].name, "another-key");
    EXPECT_EQ(decoded[1].value, "another-value");
}

TEST_F(HpackTest, EncodesAndDecodesMixedHeaders)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> original = {
        {":method", "POST"},
        {":path", "/api/users"},
        {"content-type", "application/json"},
        {"custom-header", "custom-value"}
    };

    auto encoded = encoder.encode(original);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();

    ASSERT_EQ(decoded.size(), 4u);
    EXPECT_EQ(decoded[0].name, ":method");
    EXPECT_EQ(decoded[0].value, "POST");
    EXPECT_EQ(decoded[1].name, ":path");
    EXPECT_EQ(decoded[1].value, "/api/users");
    EXPECT_EQ(decoded[2].name, "content-type");
    EXPECT_EQ(decoded[2].value, "application/json");
    EXPECT_EQ(decoded[3].name, "custom-header");
    EXPECT_EQ(decoded[3].value, "custom-value");
}

TEST_F(HpackTest, DynamicTablePersistsBetweenEncodes)
{
    hpack_encoder encoder;

    // First request with custom header
    std::vector<http_header> headers1 = {
        {"custom-key", "custom-value"}
    };
    auto encoded1 = encoder.encode(headers1);

    // Second request with same header (should use dynamic table)
    std::vector<http_header> headers2 = {
        {"custom-key", "custom-value"}
    };
    auto encoded2 = encoder.encode(headers2);

    // Second encoding should be smaller (indexed from dynamic table)
    EXPECT_LT(encoded2.size(), encoded1.size());
}

TEST_F(HpackTest, DynamicTableSynchronizationBetweenEncoderAndDecoder)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    // First request
    std::vector<http_header> headers1 = {
        {"custom-key1", "value1"}
    };
    auto encoded1 = encoder.encode(headers1);
    auto decoded1 = decoder.decode(encoded1);
    ASSERT_TRUE(decoded1.is_ok());

    // Second request with repeated header
    std::vector<http_header> headers2 = {
        {"custom-key1", "value1"},  // Should be indexed from dynamic table
        {"custom-key2", "value2"}
    };
    auto encoded2 = encoder.encode(headers2);
    auto decoded2_result = decoder.decode(encoded2);

    ASSERT_TRUE(decoded2_result.is_ok());
    auto decoded2 = decoded2_result.value();

    ASSERT_EQ(decoded2.size(), 2u);
    EXPECT_EQ(decoded2[0].name, "custom-key1");
    EXPECT_EQ(decoded2[0].value, "value1");
    EXPECT_EQ(decoded2[1].name, "custom-key2");
    EXPECT_EQ(decoded2[1].value, "value2");
}

TEST_F(HpackTest, RejectsInvalidEncodedData)
{
    hpack_decoder decoder;

    // Empty data
    std::vector<uint8_t> empty;
    auto result1 = decoder.decode(empty);
    EXPECT_TRUE(result1.is_ok());  // Empty is valid
    EXPECT_EQ(result1.value().size(), 0u);

    // Invalid index (0)
    std::vector<uint8_t> invalid_index = {0x80};  // Indexed with index 0
    auto result2 = decoder.decode(invalid_index);
    EXPECT_TRUE(result2.is_err());
}

TEST_F(HpackTest, HandlesLargeHeaders)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    // Create a large value
    std::string large_value(1000, 'x');

    std::vector<http_header> headers = {
        {"large-header", large_value}
    };

    auto encoded = encoder.encode(headers);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();

    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].name, "large-header");
    EXPECT_EQ(decoded[0].value, large_value);
}

// ============================================================
// http_header struct tests
// ============================================================

TEST_F(HpackTest, HttpHeaderSizeCalculation)
{
    // RFC 7541: entry size = name length + value length + 32
    http_header h1("content-type", "text/html");
    EXPECT_EQ(h1.size(), 12 + 9 + 32);

    http_header h2("", "");
    EXPECT_EQ(h2.size(), 32u);

    http_header h3("x", "y");
    EXPECT_EQ(h3.size(), 1 + 1 + 32);
}

TEST_F(HpackTest, HttpHeaderEquality)
{
    http_header h1("content-type", "text/html");
    http_header h2("content-type", "text/html");
    http_header h3("content-type", "text/plain");
    http_header h4("accept", "text/html");

    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_NE(h1, h4);
}

TEST_F(HpackTest, HttpHeaderDefaultConstructor)
{
    http_header h;
    EXPECT_TRUE(h.name.empty());
    EXPECT_TRUE(h.value.empty());
    EXPECT_EQ(h.size(), 32u);
}

// ============================================================
// Static Table Extended Tests
// ============================================================

TEST_F(HpackTest, StaticTableBoundaryIndex61)
{
    auto header61 = static_table::get(61);
    ASSERT_TRUE(header61.has_value());
    EXPECT_EQ(header61->name, "www-authenticate");
    EXPECT_EQ(header61->value, "");
}

TEST_F(HpackTest, StaticTableOutOfBounds62)
{
    auto header62 = static_table::get(62);
    EXPECT_FALSE(header62.has_value());
}

TEST_F(HpackTest, StaticTableSize)
{
    EXPECT_EQ(static_table::size(), 61u);
}

TEST_F(HpackTest, StaticTableFindStatusCodes)
{
    EXPECT_EQ(static_table::find(":status", "204"), 9u);
    EXPECT_EQ(static_table::find(":status", "206"), 10u);
    EXPECT_EQ(static_table::find(":status", "304"), 11u);
    EXPECT_EQ(static_table::find(":status", "400"), 12u);
    EXPECT_EQ(static_table::find(":status", "404"), 13u);
    EXPECT_EQ(static_table::find(":status", "500"), 14u);
}

TEST_F(HpackTest, StaticTableFindCommonHeaders)
{
    EXPECT_EQ(static_table::find("accept-encoding", "gzip, deflate"), 16u);
    EXPECT_EQ(static_table::find("content-type"), 31u);
    EXPECT_EQ(static_table::find("cookie"), 32u);
    EXPECT_EQ(static_table::find("user-agent"), 58u);
}

TEST_F(HpackTest, StaticTableFindNameOnlyReturnsFirstMatch)
{
    // :status appears at indices 8-14; name-only search should return first
    auto idx = static_table::find(":status");
    EXPECT_EQ(idx, 8u);

    // :method appears at indices 2-3
    auto idx2 = static_table::find(":method");
    EXPECT_EQ(idx2, 2u);
}

// ============================================================
// Dynamic Table Extended Tests
// ============================================================

TEST_F(HpackTest, DynamicTableClear)
{
    dynamic_table table(4096);
    table.insert("key1", "value1");
    table.insert("key2", "value2");
    EXPECT_GT(table.entry_count(), 0u);
    EXPECT_GT(table.current_size(), 0u);

    table.clear();
    EXPECT_EQ(table.entry_count(), 0u);
    EXPECT_EQ(table.current_size(), 0u);
}

TEST_F(HpackTest, DynamicTableSetMaxSizeShrinks)
{
    dynamic_table table(4096);
    table.insert("key1", "value1");
    table.insert("key2", "value2");
    table.insert("key3", "value3");

    size_t count_before = table.entry_count();
    EXPECT_EQ(count_before, 3u);

    // Shrink to very small size - should evict entries
    table.set_max_size(50);
    EXPECT_LE(table.current_size(), 50u);
    EXPECT_LT(table.entry_count(), count_before);
}

TEST_F(HpackTest, DynamicTableMaxSize)
{
    dynamic_table table(2048);
    EXPECT_EQ(table.max_size(), 2048u);

    table.set_max_size(1024);
    EXPECT_EQ(table.max_size(), 1024u);
}

TEST_F(HpackTest, DynamicTableGetOutOfBounds)
{
    dynamic_table table(4096);
    table.insert("key1", "value1");

    auto result = table.get(1);  // Only index 0 exists
    EXPECT_FALSE(result.has_value());

    auto result2 = table.get(100);
    EXPECT_FALSE(result2.has_value());
}

TEST_F(HpackTest, DynamicTableFIFOOrdering)
{
    dynamic_table table(4096);
    table.insert("first", "1");
    table.insert("second", "2");
    table.insert("third", "3");

    // Most recent (third) should be at index 0
    auto h0 = table.get(0);
    ASSERT_TRUE(h0.has_value());
    EXPECT_EQ(h0->name, "third");

    auto h1 = table.get(1);
    ASSERT_TRUE(h1.has_value());
    EXPECT_EQ(h1->name, "second");

    auto h2 = table.get(2);
    ASSERT_TRUE(h2.has_value());
    EXPECT_EQ(h2->name, "first");
}

TEST_F(HpackTest, DynamicTableFindNameOnly)
{
    dynamic_table table(4096);
    table.insert("content-type", "text/html");
    table.insert("content-type", "application/json");

    // Name-only search should find the most recent (index 0)
    auto idx = table.find("content-type");
    ASSERT_TRUE(idx.has_value());
    EXPECT_EQ(idx.value(), 0u);
}

TEST_F(HpackTest, DynamicTableEntrySize)
{
    dynamic_table table(4096);

    // "key" (3) + "value" (5) + 32 = 40 bytes per RFC 7541
    table.insert("key", "value");
    EXPECT_EQ(table.current_size(), 40u);

    table.insert("k2", "v2");
    // "k2" (2) + "v2" (2) + 32 = 36
    EXPECT_EQ(table.current_size(), 40u + 36u);
}

TEST_F(HpackTest, DynamicTableSetMaxSizeToZero)
{
    dynamic_table table(4096);
    table.insert("key1", "value1");
    table.insert("key2", "value2");

    table.set_max_size(0);
    EXPECT_EQ(table.entry_count(), 0u);
    EXPECT_EQ(table.current_size(), 0u);
}

// ============================================================
// Encoder Extended Tests
// ============================================================

TEST_F(HpackTest, EncoderTableSize)
{
    hpack_encoder encoder;
    EXPECT_EQ(encoder.table_size(), 0u);

    std::vector<http_header> headers = {{"custom-key", "custom-value"}};
    encoder.encode(headers);

    // After encoding, the header should be in the dynamic table
    EXPECT_GT(encoder.table_size(), 0u);
}

TEST_F(HpackTest, EncoderSetMaxTableSize)
{
    hpack_encoder encoder(4096);

    // Insert a bunch of headers
    for (int i = 0; i < 20; ++i)
    {
        std::vector<http_header> h = {
            {"key-" + std::to_string(i), "value-" + std::to_string(i)}
        };
        encoder.encode(h);
    }

    size_t size_before = encoder.table_size();

    // Shrink table
    encoder.set_max_table_size(100);
    EXPECT_LE(encoder.table_size(), 100u);
    EXPECT_LT(encoder.table_size(), size_before);
}

TEST_F(HpackTest, EncoderUsesStaticTableNameIndex)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    // "content-type" is in static table (index 31) but with empty value
    // So encoding "content-type: application/json" should use name index
    std::vector<http_header> headers = {
        {"content-type", "application/json"}
    };

    auto encoded = encoder.encode(headers);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].name, "content-type");
    EXPECT_EQ(decoded[0].value, "application/json");
}

TEST_F(HpackTest, EncoderUsesDynamicTableNameIndex)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    // First: add "x-custom" to dynamic table
    std::vector<http_header> headers1 = {{"x-custom", "value1"}};
    auto encoded1 = encoder.encode(headers1);
    auto decoded1 = decoder.decode(encoded1);
    ASSERT_TRUE(decoded1.is_ok());

    // Second: same name but different value - should use dynamic table name index
    std::vector<http_header> headers2 = {{"x-custom", "value2"}};
    auto encoded2 = encoder.encode(headers2);
    auto decoded2 = decoder.decode(encoded2);

    ASSERT_TRUE(decoded2.is_ok());
    ASSERT_EQ(decoded2.value().size(), 1u);
    EXPECT_EQ(decoded2.value()[0].name, "x-custom");
    EXPECT_EQ(decoded2.value()[0].value, "value2");
}

TEST_F(HpackTest, EncodesMultipleStaticIndexedHeaders)
{
    hpack_encoder encoder;

    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"},
        {":status", "200"}
    };

    auto encoded = encoder.encode(headers);

    // All should be indexed: 0x82, 0x84, 0x87, 0x88
    ASSERT_GE(encoded.size(), 4u);
    EXPECT_EQ(encoded[0], 0x82);  // :method GET
    EXPECT_EQ(encoded[1], 0x84);  // :path /
    EXPECT_EQ(encoded[2], 0x87);  // :scheme https
    EXPECT_EQ(encoded[3], 0x88);  // :status 200
}

// ============================================================
// Decoder Extended Tests
// ============================================================

TEST_F(HpackTest, DecoderTableSize)
{
    hpack_decoder decoder;
    EXPECT_EQ(decoder.table_size(), 0u);
}

TEST_F(HpackTest, DecoderSetMaxTableSize)
{
    hpack_decoder decoder(4096);
    decoder.set_max_table_size(2048);
    // Setting max table size should work without error
    EXPECT_EQ(decoder.table_size(), 0u);
}

TEST_F(HpackTest, DecoderRejectsOutOfRangeStaticIndex)
{
    hpack_decoder decoder;

    // Index 62+ doesn't exist in static table and no dynamic entries
    // Indexed representation with index 62: need multi-byte encoding
    // 7-bit prefix: 0x80 | 62 = 0xBE
    std::vector<uint8_t> data = {0xBE};
    auto result = decoder.decode(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackTest, DecoderRejectsOutOfRangeDynamicIndex)
{
    hpack_decoder decoder;

    // Index 70 (way beyond static table and empty dynamic table)
    // 7-bit prefix: 70 < 127, so 0x80 | 70 = 0xC6
    std::vector<uint8_t> data = {0xC6};
    auto result = decoder.decode(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackTest, DecoderHandlesLiteralWithoutIndexing)
{
    // Literal without indexing: 0000xxxx pattern
    // 0x00 (new name), length 3 "foo", length 3 "bar"
    std::vector<uint8_t> data = {
        0x00,                    // Literal without indexing, new name
        0x03, 'f', 'o', 'o',    // Name: "foo"
        0x03, 'b', 'a', 'r'     // Value: "bar"
    };

    hpack_decoder decoder;
    auto result = decoder.decode(data);

    ASSERT_TRUE(result.is_ok());
    auto headers = result.value();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, "foo");
    EXPECT_EQ(headers[0].value, "bar");

    // Should NOT be added to dynamic table
    EXPECT_EQ(decoder.table_size(), 0u);
}

TEST_F(HpackTest, DecoderHandlesLiteralWithIndexingNewName)
{
    // Literal with incremental indexing: 01xxxxxx pattern
    // 0x40 (new name), length 4 "test", length 5 "value"
    std::vector<uint8_t> data = {
        0x40,                          // Literal with indexing, new name
        0x04, 't', 'e', 's', 't',     // Name: "test"
        0x05, 'v', 'a', 'l', 'u', 'e' // Value: "value"
    };

    hpack_decoder decoder;
    auto result = decoder.decode(data);

    ASSERT_TRUE(result.is_ok());
    auto headers = result.value();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, "test");
    EXPECT_EQ(headers[0].value, "value");

    // Should be added to dynamic table
    EXPECT_GT(decoder.table_size(), 0u);
}

TEST_F(HpackTest, DecoderHandlesLiteralWithIndexingIndexedName)
{
    // Literal with indexing, name from static table index 31 (content-type)
    // 6-bit prefix max = 63, so index 31 fits in single byte: 0x40 | 31 = 0x5F
    std::vector<uint8_t> data = {
        0x5F,                                                  // Literal with indexing, name index 31
        0x09, 't', 'e', 'x', 't', '/', 'h', 't', 'm', 'l'   // Value: "text/html" (9 bytes)
    };

    hpack_decoder decoder;
    auto result = decoder.decode(data);

    ASSERT_TRUE(result.is_ok());
    auto headers = result.value();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, "content-type");
    EXPECT_EQ(headers[0].value, "text/html");
}

TEST_F(HpackTest, DecoderHandlesEmptyHeaderValue)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> headers = {
        {":authority", ""}
    };

    auto encoded = encoder.encode(headers);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(decoded[0].name, ":authority");
    EXPECT_EQ(decoded[0].value, "");
}

TEST_F(HpackTest, DecoderHandlesMultipleHeadersWithSameName)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> headers = {
        {"set-cookie", "id=abc"},
        {"set-cookie", "lang=en"}
    };

    auto encoded = encoder.encode(headers);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();
    ASSERT_EQ(decoded.size(), 2u);
    EXPECT_EQ(decoded[0].name, "set-cookie");
    EXPECT_EQ(decoded[0].value, "id=abc");
    EXPECT_EQ(decoded[1].name, "set-cookie");
    EXPECT_EQ(decoded[1].value, "lang=en");
}

// ============================================================
// Huffman Stub Tests
// ============================================================

TEST_F(HpackTest, HuffmanEncodeStub)
{
    auto encoded = huffman::encode("hello");
    // Stub returns raw bytes
    ASSERT_EQ(encoded.size(), 5u);
    EXPECT_EQ(encoded[0], 'h');
    EXPECT_EQ(encoded[4], 'o');
}

TEST_F(HpackTest, HuffmanDecodeStub)
{
    std::vector<uint8_t> data = {'w', 'o', 'r', 'l', 'd'};
    auto result = huffman::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), "world");
}

TEST_F(HpackTest, HuffmanEncodedSizeStub)
{
    EXPECT_EQ(huffman::encoded_size("test"), 4u);
    EXPECT_EQ(huffman::encoded_size(""), 0u);
    EXPECT_EQ(huffman::encoded_size("hello world"), 11u);
}

TEST_F(HpackTest, HuffmanEmptyInput)
{
    auto encoded = huffman::encode("");
    EXPECT_TRUE(encoded.empty());

    std::vector<uint8_t> empty;
    auto decoded = huffman::decode(empty);
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_TRUE(decoded.value().empty());
}

// ============================================================
// Encoder-Decoder Integration Tests
// ============================================================

TEST_F(HpackTest, RoundTripWithPathVariations)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    // :path "/" is static index 4, :path "/index.html" is static index 5
    // Other paths need literal encoding
    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/index.html"},
        {":scheme", "http"},
        {":authority", "example.com"}
    };

    auto encoded = encoder.encode(headers);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();
    ASSERT_EQ(decoded.size(), 4u);
    EXPECT_EQ(decoded[0], headers[0]);
    EXPECT_EQ(decoded[1], headers[1]);
    EXPECT_EQ(decoded[2], headers[2]);
    EXPECT_EQ(decoded[3].name, ":authority");
    EXPECT_EQ(decoded[3].value, "example.com");
}

TEST_F(HpackTest, RoundTripMultipleRequests)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    // Request 1
    std::vector<http_header> req1 = {
        {":method", "GET"},
        {":path", "/"},
        {"accept", "text/html"}
    };
    auto enc1 = encoder.encode(req1);
    auto dec1 = decoder.decode(enc1);
    ASSERT_TRUE(dec1.is_ok());
    EXPECT_EQ(dec1.value().size(), 3u);

    // Request 2 (shares some headers via dynamic table)
    std::vector<http_header> req2 = {
        {":method", "GET"},
        {":path", "/style.css"},
        {"accept", "text/css"}
    };
    auto enc2 = encoder.encode(req2);
    auto dec2 = decoder.decode(enc2);
    ASSERT_TRUE(dec2.is_ok());
    EXPECT_EQ(dec2.value().size(), 3u);
    EXPECT_EQ(dec2.value()[1].value, "/style.css");
    EXPECT_EQ(dec2.value()[2].value, "text/css");

    // Request 3 (more dynamic table reuse)
    std::vector<http_header> req3 = {
        {":method", "POST"},
        {":path", "/api/data"},
        {"content-type", "application/json"},
        {"accept", "application/json"}
    };
    auto enc3 = encoder.encode(req3);
    auto dec3 = decoder.decode(enc3);
    ASSERT_TRUE(dec3.is_ok());
    EXPECT_EQ(dec3.value().size(), 4u);
}

TEST_F(HpackTest, RoundTripResponseHeaders)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> headers = {
        {":status", "200"},
        {"content-type", "text/html; charset=utf-8"},
        {"content-length", "1234"},
        {"cache-control", "max-age=3600"},
        {"server", "network_system/1.0"}
    };

    auto encoded = encoder.encode(headers);
    auto decoded_result = decoder.decode(encoded);

    ASSERT_TRUE(decoded_result.is_ok());
    auto decoded = decoded_result.value();
    ASSERT_EQ(decoded.size(), 5u);

    for (size_t i = 0; i < headers.size(); ++i)
    {
        EXPECT_EQ(decoded[i].name, headers[i].name);
        EXPECT_EQ(decoded[i].value, headers[i].value);
    }
}
