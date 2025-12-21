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
#include <kcenon/network/protocols/grpc/frame.h>
#include <kcenon/network/protocols/grpc/status.h>

namespace grpc = kcenon::network::protocols::grpc;

// grpc_message tests
class GrpcMessageTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcMessageTest, DefaultConstruction)
{
    grpc::grpc_message msg;
    EXPECT_FALSE(msg.compressed);
    EXPECT_TRUE(msg.empty());
    EXPECT_EQ(msg.size(), 0u);
}

TEST_F(GrpcMessageTest, ConstructWithData)
{
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    grpc::grpc_message msg(data, false);

    EXPECT_FALSE(msg.compressed);
    EXPECT_FALSE(msg.empty());
    EXPECT_EQ(msg.size(), 5u);
    EXPECT_EQ(msg.data, data);
}

TEST_F(GrpcMessageTest, ConstructWithCompression)
{
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    grpc::grpc_message msg(data, true);

    EXPECT_TRUE(msg.compressed);
    EXPECT_EQ(msg.size(), 5u);
}

TEST_F(GrpcMessageTest, SerializeUncompressed)
{
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    grpc::grpc_message msg(data, false);

    auto serialized = msg.serialize();

    // Check header: 1 byte compressed flag + 4 bytes length
    EXPECT_EQ(serialized.size(), 10u);  // 5 header + 5 data
    EXPECT_EQ(serialized[0], 0);        // Not compressed
    EXPECT_EQ(serialized[1], 0);        // Length MSB
    EXPECT_EQ(serialized[2], 0);
    EXPECT_EQ(serialized[3], 0);
    EXPECT_EQ(serialized[4], 5);        // Length LSB

    // Check data
    for (size_t i = 0; i < 5; ++i)
    {
        EXPECT_EQ(serialized[5 + i], data[i]);
    }
}

TEST_F(GrpcMessageTest, SerializeCompressed)
{
    std::vector<uint8_t> data = {1, 2, 3};
    grpc::grpc_message msg(data, true);

    auto serialized = msg.serialize();

    EXPECT_EQ(serialized[0], 1);  // Compressed flag
}

TEST_F(GrpcMessageTest, ParseValidMessage)
{
    // Create valid gRPC message: compressed=0, length=3, data=[1,2,3]
    std::vector<uint8_t> raw = {0, 0, 0, 0, 3, 1, 2, 3};

    auto result = grpc::grpc_message::parse(raw);

    ASSERT_TRUE(result.is_ok());
    EXPECT_FALSE(result.value().compressed);
    EXPECT_EQ(result.value().size(), 3u);
    EXPECT_EQ(result.value().data[0], 1);
    EXPECT_EQ(result.value().data[1], 2);
    EXPECT_EQ(result.value().data[2], 3);
}

TEST_F(GrpcMessageTest, ParseCompressedMessage)
{
    // Create compressed gRPC message
    std::vector<uint8_t> raw = {1, 0, 0, 0, 2, 10, 20};

    auto result = grpc::grpc_message::parse(raw);

    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().compressed);
    EXPECT_EQ(result.value().size(), 2u);
}

TEST_F(GrpcMessageTest, ParseTooShort)
{
    std::vector<uint8_t> raw = {0, 0, 0};  // Only 3 bytes, need at least 5

    auto result = grpc::grpc_message::parse(raw);

    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcMessageTest, ParseInsufficientData)
{
    // Header says length=10, but only 3 bytes of data
    std::vector<uint8_t> raw = {0, 0, 0, 0, 10, 1, 2, 3};

    auto result = grpc::grpc_message::parse(raw);

    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcMessageTest, RoundTrip)
{
    std::vector<uint8_t> original_data = {10, 20, 30, 40, 50, 60, 70, 80};
    grpc::grpc_message original(original_data, false);

    auto serialized = original.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);

    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().compressed, original.compressed);
    EXPECT_EQ(parsed.value().data, original.data);
}

TEST_F(GrpcMessageTest, RoundTripCompressed)
{
    std::vector<uint8_t> original_data = {100, 200};
    grpc::grpc_message original(original_data, true);

    auto serialized = original.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);

    ASSERT_TRUE(parsed.is_ok());
    EXPECT_TRUE(parsed.value().compressed);
    EXPECT_EQ(parsed.value().data, original.data);
}

TEST_F(GrpcMessageTest, SerializedSize)
{
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    grpc::grpc_message msg(data, false);

    EXPECT_EQ(msg.serialized_size(), 10u);  // 5 header + 5 data
}

TEST_F(GrpcMessageTest, EmptyMessage)
{
    grpc::grpc_message msg;

    auto serialized = msg.serialize();
    EXPECT_EQ(serialized.size(), 5u);  // Just header

    auto parsed = grpc::grpc_message::parse(serialized);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_TRUE(parsed.value().empty());
}

// grpc_status tests
class GrpcStatusTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcStatusTest, DefaultConstruction)
{
    grpc::grpc_status status;

    EXPECT_EQ(status.code, grpc::status_code::ok);
    EXPECT_TRUE(status.message.empty());
    EXPECT_FALSE(status.details.has_value());
    EXPECT_TRUE(status.is_ok());
    EXPECT_FALSE(status.is_error());
}

TEST_F(GrpcStatusTest, ConstructWithCode)
{
    grpc::grpc_status status(grpc::status_code::not_found);

    EXPECT_EQ(status.code, grpc::status_code::not_found);
    EXPECT_TRUE(status.is_error());
    EXPECT_FALSE(status.is_ok());
}

TEST_F(GrpcStatusTest, ConstructWithCodeAndMessage)
{
    grpc::grpc_status status(grpc::status_code::invalid_argument, "Bad input");

    EXPECT_EQ(status.code, grpc::status_code::invalid_argument);
    EXPECT_EQ(status.message, "Bad input");
    EXPECT_TRUE(status.is_error());
}

TEST_F(GrpcStatusTest, ConstructWithDetails)
{
    grpc::grpc_status status(grpc::status_code::internal, "Error", "detail info");

    EXPECT_EQ(status.code, grpc::status_code::internal);
    EXPECT_EQ(status.message, "Error");
    EXPECT_TRUE(status.details.has_value());
    EXPECT_EQ(status.details.value(), "detail info");
}

TEST_F(GrpcStatusTest, OkStatus)
{
    auto status = grpc::grpc_status::ok_status();

    EXPECT_TRUE(status.is_ok());
    EXPECT_EQ(status.code, grpc::status_code::ok);
}

TEST_F(GrpcStatusTest, ErrorStatus)
{
    auto status = grpc::grpc_status::error_status(
        grpc::status_code::deadline_exceeded,
        "Timeout");

    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status.code, grpc::status_code::deadline_exceeded);
    EXPECT_EQ(status.message, "Timeout");
}

TEST_F(GrpcStatusTest, CodeString)
{
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::ok), "OK");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::cancelled), "CANCELLED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unknown), "UNKNOWN");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::invalid_argument), "INVALID_ARGUMENT");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::deadline_exceeded), "DEADLINE_EXCEEDED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::not_found), "NOT_FOUND");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::already_exists), "ALREADY_EXISTS");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::permission_denied), "PERMISSION_DENIED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::resource_exhausted), "RESOURCE_EXHAUSTED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::failed_precondition), "FAILED_PRECONDITION");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::aborted), "ABORTED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::out_of_range), "OUT_OF_RANGE");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unimplemented), "UNIMPLEMENTED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::internal), "INTERNAL");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unavailable), "UNAVAILABLE");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::data_loss), "DATA_LOSS");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unauthenticated), "UNAUTHENTICATED");
}

// Timeout parsing tests
class GrpcTimeoutTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcTimeoutTest, ParseHours)
{
    EXPECT_EQ(grpc::parse_timeout("1H"), 3600000u);
    EXPECT_EQ(grpc::parse_timeout("2H"), 7200000u);
}

TEST_F(GrpcTimeoutTest, ParseMinutes)
{
    EXPECT_EQ(grpc::parse_timeout("1M"), 60000u);
    EXPECT_EQ(grpc::parse_timeout("30M"), 1800000u);
}

TEST_F(GrpcTimeoutTest, ParseSeconds)
{
    EXPECT_EQ(grpc::parse_timeout("1S"), 1000u);
    EXPECT_EQ(grpc::parse_timeout("10S"), 10000u);
}

TEST_F(GrpcTimeoutTest, ParseMilliseconds)
{
    EXPECT_EQ(grpc::parse_timeout("100m"), 100u);
    EXPECT_EQ(grpc::parse_timeout("1000m"), 1000u);
}

TEST_F(GrpcTimeoutTest, ParseMicroseconds)
{
    EXPECT_EQ(grpc::parse_timeout("1000u"), 1u);
    EXPECT_EQ(grpc::parse_timeout("5000u"), 5u);
}

TEST_F(GrpcTimeoutTest, ParseNanoseconds)
{
    EXPECT_EQ(grpc::parse_timeout("1000000n"), 1u);
}

TEST_F(GrpcTimeoutTest, ParseInvalid)
{
    EXPECT_EQ(grpc::parse_timeout(""), 0u);
    EXPECT_EQ(grpc::parse_timeout("abc"), 0u);
    EXPECT_EQ(grpc::parse_timeout("10x"), 0u);
}

TEST_F(GrpcTimeoutTest, FormatTimeout)
{
    EXPECT_EQ(grpc::format_timeout(3600000), "1H");
    EXPECT_EQ(grpc::format_timeout(60000), "1M");
    EXPECT_EQ(grpc::format_timeout(1000), "1S");
    EXPECT_EQ(grpc::format_timeout(500), "500m");
    EXPECT_EQ(grpc::format_timeout(0), "0m");
}

// grpc_trailers tests
class GrpcTrailersTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcTrailersTest, ToStatus)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::internal;
    trailers.status_message = "Server error";

    auto status = trailers.to_status();

    EXPECT_EQ(status.code, grpc::status_code::internal);
    EXPECT_EQ(status.message, "Server error");
    EXPECT_FALSE(status.details.has_value());
}

TEST_F(GrpcTrailersTest, ToStatusWithDetails)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::unavailable;
    trailers.status_message = "Service down";
    trailers.status_details = "binary details";

    auto status = trailers.to_status();

    EXPECT_EQ(status.code, grpc::status_code::unavailable);
    EXPECT_TRUE(status.details.has_value());
    EXPECT_EQ(status.details.value(), "binary details");
}
