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
#include "kcenon/network/protocols/http2/hpack.h"
#include <vector>
#include <string>

using namespace network_system::protocols::http2;

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
