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

#include "kcenon/network/protocols/quic/varint.h"

namespace network_system::protocols::quic
{
namespace
{

class VarintTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// RFC 9000 Section 16 boundary values
TEST_F(VarintTest, EncodeBoundaryValues)
{
    // 1-byte boundary: 0-63
    EXPECT_EQ(varint::encode(0).size(), 1);
    EXPECT_EQ(varint::encode(63).size(), 1);

    // 2-byte boundary: 64-16383
    EXPECT_EQ(varint::encode(64).size(), 2);
    EXPECT_EQ(varint::encode(16383).size(), 2);

    // 4-byte boundary: 16384-1073741823
    EXPECT_EQ(varint::encode(16384).size(), 4);
    EXPECT_EQ(varint::encode(1073741823).size(), 4);

    // 8-byte boundary: 1073741824-4611686018427387903
    EXPECT_EQ(varint::encode(1073741824).size(), 8);
    EXPECT_EQ(varint::encode(varint_max).size(), 8);
}

TEST_F(VarintTest, EncodeZero)
{
    auto encoded = varint::encode(0);
    ASSERT_EQ(encoded.size(), 1);
    EXPECT_EQ(encoded[0], 0x00);
}

TEST_F(VarintTest, Encode1ByteMax)
{
    auto encoded = varint::encode(63);
    ASSERT_EQ(encoded.size(), 1);
    EXPECT_EQ(encoded[0], 0x3F);
}

TEST_F(VarintTest, Encode2ByteMin)
{
    auto encoded = varint::encode(64);
    ASSERT_EQ(encoded.size(), 2);
    // 64 = 0x40, prefix 0b01 -> first byte = 0x40 | 0x00 = 0x40
    EXPECT_EQ(encoded[0], 0x40);
    EXPECT_EQ(encoded[1], 0x40);
}

TEST_F(VarintTest, Encode2ByteMax)
{
    auto encoded = varint::encode(16383);
    ASSERT_EQ(encoded.size(), 2);
    // 16383 = 0x3FFF, prefix 0b01 -> 0x7F, 0xFF
    EXPECT_EQ(encoded[0], 0x7F);
    EXPECT_EQ(encoded[1], 0xFF);
}

TEST_F(VarintTest, Encode4ByteMin)
{
    auto encoded = varint::encode(16384);
    ASSERT_EQ(encoded.size(), 4);
    // 16384 = 0x4000, prefix 0b10
    EXPECT_EQ(encoded[0], 0x80);
    EXPECT_EQ(encoded[1], 0x00);
    EXPECT_EQ(encoded[2], 0x40);
    EXPECT_EQ(encoded[3], 0x00);
}

TEST_F(VarintTest, Encode4ByteMax)
{
    auto encoded = varint::encode(1073741823);
    ASSERT_EQ(encoded.size(), 4);
    // 1073741823 = 0x3FFFFFFF, prefix 0b10 -> 0xBF, 0xFF, 0xFF, 0xFF
    EXPECT_EQ(encoded[0], 0xBF);
    EXPECT_EQ(encoded[1], 0xFF);
    EXPECT_EQ(encoded[2], 0xFF);
    EXPECT_EQ(encoded[3], 0xFF);
}

TEST_F(VarintTest, Encode8ByteMin)
{
    auto encoded = varint::encode(1073741824);
    ASSERT_EQ(encoded.size(), 8);
    // prefix 0b11
    EXPECT_EQ(encoded[0] & 0xC0, 0xC0);
}

TEST_F(VarintTest, Encode8ByteMax)
{
    auto encoded = varint::encode(varint_max);
    ASSERT_EQ(encoded.size(), 8);
    // varint_max = 0x3FFFFFFFFFFFFFFF, prefix 0b11
    EXPECT_EQ(encoded[0], 0xFF);
    EXPECT_EQ(encoded[1], 0xFF);
    EXPECT_EQ(encoded[2], 0xFF);
    EXPECT_EQ(encoded[3], 0xFF);
    EXPECT_EQ(encoded[4], 0xFF);
    EXPECT_EQ(encoded[5], 0xFF);
    EXPECT_EQ(encoded[6], 0xFF);
    EXPECT_EQ(encoded[7], 0xFF);
}

TEST_F(VarintTest, DecodeEmpty)
{
    std::vector<uint8_t> empty;
    auto result = varint::decode(empty);
    EXPECT_TRUE(result.is_err());
}

TEST_F(VarintTest, DecodeInsufficientData)
{
    // 2-byte encoding but only 1 byte provided
    std::vector<uint8_t> data = {0x40};  // prefix 0b01 indicates 2 bytes
    auto result = varint::decode(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(VarintTest, DecodeZero)
{
    std::vector<uint8_t> data = {0x00};
    auto result = varint::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 0);
    EXPECT_EQ(result.value().second, 1);
}

TEST_F(VarintTest, Decode1ByteMax)
{
    std::vector<uint8_t> data = {0x3F};
    auto result = varint::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 63);
    EXPECT_EQ(result.value().second, 1);
}

TEST_F(VarintTest, Decode2Byte)
{
    std::vector<uint8_t> data = {0x40, 0x40};  // 64
    auto result = varint::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 64);
    EXPECT_EQ(result.value().second, 2);
}

TEST_F(VarintTest, Decode4Byte)
{
    std::vector<uint8_t> data = {0x80, 0x00, 0x40, 0x00};  // 16384
    auto result = varint::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 16384);
    EXPECT_EQ(result.value().second, 4);
}

TEST_F(VarintTest, Decode8Byte)
{
    std::vector<uint8_t> data = {0xC0, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00};
    auto result = varint::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 1073741824);
    EXPECT_EQ(result.value().second, 8);
}

TEST_F(VarintTest, RoundTripAllBoundaries)
{
    std::vector<uint64_t> test_values = {
        0, 1, 62, 63,                  // 1-byte boundaries
        64, 65, 16382, 16383,          // 2-byte boundaries
        16384, 16385, 1073741822, 1073741823,  // 4-byte boundaries
        1073741824, 1073741825, varint_max - 1, varint_max  // 8-byte boundaries
    };

    for (uint64_t value : test_values)
    {
        auto encoded = varint::encode(value);
        auto decoded = varint::decode(encoded);

        ASSERT_TRUE(decoded.is_ok()) << "Failed to decode value: " << value;
        EXPECT_EQ(decoded.value().first, value) << "Round-trip failed for: " << value;
        EXPECT_EQ(decoded.value().second, encoded.size())
            << "Consumed bytes mismatch for: " << value;
    }
}

TEST_F(VarintTest, EncodedLengthConstexpr)
{
    static_assert(varint::encoded_length(0) == 1);
    static_assert(varint::encoded_length(63) == 1);
    static_assert(varint::encoded_length(64) == 2);
    static_assert(varint::encoded_length(16383) == 2);
    static_assert(varint::encoded_length(16384) == 4);
    static_assert(varint::encoded_length(1073741823) == 4);
    static_assert(varint::encoded_length(1073741824) == 8);
    static_assert(varint::encoded_length(varint_max) == 8);
}

TEST_F(VarintTest, LengthFromPrefixConstexpr)
{
    static_assert(varint::length_from_prefix(0x00) == 1);  // 0b00
    static_assert(varint::length_from_prefix(0x3F) == 1);  // 0b00
    static_assert(varint::length_from_prefix(0x40) == 2);  // 0b01
    static_assert(varint::length_from_prefix(0x7F) == 2);  // 0b01
    static_assert(varint::length_from_prefix(0x80) == 4);  // 0b10
    static_assert(varint::length_from_prefix(0xBF) == 4);  // 0b10
    static_assert(varint::length_from_prefix(0xC0) == 8);  // 0b11
    static_assert(varint::length_from_prefix(0xFF) == 8);  // 0b11
}

TEST_F(VarintTest, IsValidConstexpr)
{
    static_assert(varint::is_valid(0));
    static_assert(varint::is_valid(varint_max));
    static_assert(!varint::is_valid(varint_max + 1));
}

TEST_F(VarintTest, EncodeWithLength_ValidLengths)
{
    // Encode small value with larger length
    auto result = varint::encode_with_length(10, 2);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 2);

    // Verify it decodes correctly
    auto decoded = varint::decode(result.value());
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value().first, 10);
}

TEST_F(VarintTest, EncodeWithLength_InvalidLength)
{
    auto result = varint::encode_with_length(10, 3);
    EXPECT_TRUE(result.is_err());

    result = varint::encode_with_length(10, 5);
    EXPECT_TRUE(result.is_err());
}

TEST_F(VarintTest, EncodeWithLength_ValueTooLarge)
{
    // Try to encode value that exceeds varint_max
    auto result = varint::encode_with_length(varint_max + 1, 8);
    EXPECT_TRUE(result.is_err());
}

TEST_F(VarintTest, EncodeWithLength_AutoUpgrade)
{
    // Request 1-byte encoding for value that needs 2 bytes
    auto result = varint::encode_with_length(100, 1);
    ASSERT_TRUE(result.is_ok());
    // Should automatically upgrade to 2 bytes
    EXPECT_EQ(result.value().size(), 2);

    auto decoded = varint::decode(result.value());
    ASSERT_TRUE(decoded.is_ok());
    EXPECT_EQ(decoded.value().first, 100);
}

TEST_F(VarintTest, DecodeWithExtraData)
{
    // Buffer with extra data after the encoded value
    std::vector<uint8_t> data = {0x25, 0xFF, 0xFF, 0xFF};  // 37 (1-byte) + extra
    auto result = varint::decode(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, 37);
    EXPECT_EQ(result.value().second, 1);  // Only consumed 1 byte
}

TEST_F(VarintTest, RFC9000ExampleValues)
{
    // RFC 9000 Appendix A.1 examples
    // Example: 37 encoded as single byte
    {
        auto encoded = varint::encode(37);
        ASSERT_EQ(encoded.size(), 1);
        EXPECT_EQ(encoded[0], 0x25);
    }

    // Example: 15293 encoded as two bytes (0x7bbd)
    {
        auto encoded = varint::encode(15293);
        ASSERT_EQ(encoded.size(), 2);
        EXPECT_EQ(encoded[0], 0x7b);
        EXPECT_EQ(encoded[1], 0xbd);
    }

    // Example: 494878333 encoded as four bytes (0x9d7f3e7d)
    {
        auto encoded = varint::encode(494878333);
        ASSERT_EQ(encoded.size(), 4);
        EXPECT_EQ(encoded[0], 0x9d);
        EXPECT_EQ(encoded[1], 0x7f);
        EXPECT_EQ(encoded[2], 0x3e);
        EXPECT_EQ(encoded[3], 0x7d);
    }

    // Example: 151288809941952652 encoded as eight bytes (0xc2197c5eff14e88c)
    {
        auto encoded = varint::encode(151288809941952652ULL);
        ASSERT_EQ(encoded.size(), 8);
        EXPECT_EQ(encoded[0], 0xc2);
        EXPECT_EQ(encoded[1], 0x19);
        EXPECT_EQ(encoded[2], 0x7c);
        EXPECT_EQ(encoded[3], 0x5e);
        EXPECT_EQ(encoded[4], 0xff);
        EXPECT_EQ(encoded[5], 0x14);
        EXPECT_EQ(encoded[6], 0xe8);
        EXPECT_EQ(encoded[7], 0x8c);
    }
}

TEST_F(VarintTest, DecodeRFC9000Examples)
{
    // Decode the RFC 9000 examples
    {
        std::vector<uint8_t> data = {0x25};
        auto result = varint::decode(data);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first, 37);
    }

    {
        std::vector<uint8_t> data = {0x7b, 0xbd};
        auto result = varint::decode(data);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first, 15293);
    }

    {
        std::vector<uint8_t> data = {0x9d, 0x7f, 0x3e, 0x7d};
        auto result = varint::decode(data);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first, 494878333);
    }

    {
        std::vector<uint8_t> data = {0xc2, 0x19, 0x7c, 0x5e, 0xff, 0x14, 0xe8, 0x8c};
        auto result = varint::decode(data);
        ASSERT_TRUE(result.is_ok());
        EXPECT_EQ(result.value().first, 151288809941952652ULL);
    }
}

}  // namespace
}  // namespace network_system::protocols::quic
