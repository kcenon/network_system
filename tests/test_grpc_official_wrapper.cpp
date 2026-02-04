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
#include <kcenon/network/detail/protocols/grpc/grpc_official_wrapper.h>
#include <kcenon/network/detail/protocols/grpc/frame.h>
#include <kcenon/network/detail/protocols/grpc/status.h>

namespace grpc = kcenon::network::protocols::grpc;

// ============================================================================
// Channel Credentials Config Tests
// ============================================================================

class ChannelCredentialsConfigTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ChannelCredentialsConfigTest, DefaultConstruction)
{
    grpc::channel_credentials_config config;

    EXPECT_FALSE(config.insecure);
    EXPECT_TRUE(config.root_certificates.empty());
    EXPECT_FALSE(config.client_certificate.has_value());
    EXPECT_FALSE(config.client_key.has_value());
}

TEST_F(ChannelCredentialsConfigTest, InsecureConfig)
{
    grpc::channel_credentials_config config;
    config.insecure = true;

    EXPECT_TRUE(config.insecure);
}

TEST_F(ChannelCredentialsConfigTest, TlsConfig)
{
    grpc::channel_credentials_config config;
    config.root_certificates = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----";

    EXPECT_FALSE(config.insecure);
    EXPECT_FALSE(config.root_certificates.empty());
}

TEST_F(ChannelCredentialsConfigTest, MutualTlsConfig)
{
    grpc::channel_credentials_config config;
    config.root_certificates = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----";
    config.client_certificate = "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----";
    config.client_key = "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----";

    EXPECT_FALSE(config.insecure);
    EXPECT_TRUE(config.client_certificate.has_value());
    EXPECT_TRUE(config.client_key.has_value());
}

// ============================================================================
// Status Code Mapping Tests (Non-NETWORK_GRPC_OFFICIAL)
// ============================================================================

class StatusCodeMappingTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StatusCodeMappingTest, StatusCodeValues)
{
    // Verify status code enum values match gRPC spec
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::ok), 0u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::cancelled), 1u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::unknown), 2u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::invalid_argument), 3u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::deadline_exceeded), 4u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::not_found), 5u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::already_exists), 6u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::permission_denied), 7u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::resource_exhausted), 8u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::failed_precondition), 9u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::aborted), 10u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::out_of_range), 11u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::unimplemented), 12u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::internal), 13u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::unavailable), 14u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::data_loss), 15u);
    EXPECT_EQ(static_cast<uint32_t>(grpc::status_code::unauthenticated), 16u);
}

TEST_F(StatusCodeMappingTest, AllStatusCodesHaveStringRepresentation)
{
    // All status codes should have string representations
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::ok).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::cancelled).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::unknown).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::invalid_argument).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::deadline_exceeded).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::not_found).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::already_exists).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::permission_denied).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::resource_exhausted).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::failed_precondition).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::aborted).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::out_of_range).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::unimplemented).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::internal).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::unavailable).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::data_loss).empty());
    EXPECT_FALSE(grpc::status_code_to_string(grpc::status_code::unauthenticated).empty());
}

// ============================================================================
// gRPC Status Tests
// ============================================================================

class GrpcStatusWrapperTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcStatusWrapperTest, OkStatusIsNotError)
{
    grpc::grpc_status status = grpc::grpc_status::ok_status();

    EXPECT_TRUE(status.is_ok());
    EXPECT_FALSE(status.is_error());
    EXPECT_EQ(status.code, grpc::status_code::ok);
}

TEST_F(GrpcStatusWrapperTest, ErrorStatusIsError)
{
    grpc::grpc_status status = grpc::grpc_status::error_status(
        grpc::status_code::internal, "Internal error");

    EXPECT_FALSE(status.is_ok());
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status.code, grpc::status_code::internal);
    EXPECT_EQ(status.message, "Internal error");
}

TEST_F(GrpcStatusWrapperTest, StatusWithDetails)
{
    grpc::grpc_status status(
        grpc::status_code::invalid_argument,
        "Bad request",
        "field 'name' is required");

    EXPECT_TRUE(status.is_error());
    EXPECT_TRUE(status.details.has_value());
    EXPECT_EQ(status.details.value(), "field 'name' is required");
}

TEST_F(GrpcStatusWrapperTest, CodeString)
{
    grpc::grpc_status status(grpc::status_code::not_found);

    EXPECT_EQ(status.code_string(), "NOT_FOUND");
}

// ============================================================================
// gRPC Message Wrapper Tests
// ============================================================================

class GrpcMessageWrapperTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcMessageWrapperTest, MessageSerializationRoundTrip)
{
    std::vector<uint8_t> data = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E};
    grpc::grpc_message original(data, false);

    auto serialized = original.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);

    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().data, original.data);
    EXPECT_EQ(parsed.value().compressed, original.compressed);
}

TEST_F(GrpcMessageWrapperTest, CompressedMessageRoundTrip)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    grpc::grpc_message original(data, true);

    auto serialized = original.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);

    ASSERT_TRUE(parsed.is_ok());
    EXPECT_TRUE(parsed.value().compressed);
    EXPECT_EQ(parsed.value().data, original.data);
}

TEST_F(GrpcMessageWrapperTest, LargeMessageSerialization)
{
    // Test with a larger payload (1KB)
    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = static_cast<uint8_t>(i % 256);
    }

    grpc::grpc_message original(data, false);
    auto serialized = original.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);

    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().data.size(), 1024u);
    EXPECT_EQ(parsed.value().data, original.data);
}

TEST_F(GrpcMessageWrapperTest, SerializedSizeCalculation)
{
    std::vector<uint8_t> data(100);
    grpc::grpc_message msg(data, false);

    // Header is 5 bytes (1 byte compression flag + 4 bytes length)
    EXPECT_EQ(msg.serialized_size(), 105u);
}

// ============================================================================
// gRPC Trailers Tests
// ============================================================================

class GrpcTrailersWrapperTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcTrailersWrapperTest, ConvertToStatus)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::permission_denied;
    trailers.status_message = "Access denied";

    auto status = trailers.to_status();

    EXPECT_EQ(status.code, grpc::status_code::permission_denied);
    EXPECT_EQ(status.message, "Access denied");
    EXPECT_FALSE(status.details.has_value());
}

TEST_F(GrpcTrailersWrapperTest, ConvertToStatusWithDetails)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::resource_exhausted;
    trailers.status_message = "Rate limit exceeded";
    trailers.status_details = "retry after 60 seconds";

    auto status = trailers.to_status();

    EXPECT_EQ(status.code, grpc::status_code::resource_exhausted);
    EXPECT_TRUE(status.details.has_value());
    EXPECT_EQ(status.details.value(), "retry after 60 seconds");
}

// ============================================================================
// Timeout Parsing Tests
// ============================================================================

class TimeoutParsingWrapperTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TimeoutParsingWrapperTest, ParseAndFormat)
{
    // Test round-trip for hours
    EXPECT_EQ(grpc::format_timeout(grpc::parse_timeout("2H")), "2H");

    // Test round-trip for minutes
    EXPECT_EQ(grpc::format_timeout(grpc::parse_timeout("30M")), "30M");

    // Test round-trip for seconds
    EXPECT_EQ(grpc::format_timeout(grpc::parse_timeout("15S")), "15S");

    // Test round-trip for milliseconds
    EXPECT_EQ(grpc::format_timeout(grpc::parse_timeout("250m")), "250m");
}

TEST_F(TimeoutParsingWrapperTest, InvalidTimeoutReturnsZero)
{
    EXPECT_EQ(grpc::parse_timeout(""), 0u);
    EXPECT_EQ(grpc::parse_timeout("invalid"), 0u);
    EXPECT_EQ(grpc::parse_timeout("10X"), 0u);
    EXPECT_EQ(grpc::parse_timeout("-5S"), 0u);
}

// ============================================================================
// Integration Readiness Tests
// ============================================================================

class IntegrationReadinessTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IntegrationReadinessTest, StatusCodeCompatibility)
{
    // Verify our status codes are compatible with gRPC protocol
    // These are the standard gRPC status codes
    std::vector<std::pair<grpc::status_code, std::string>> expected = {
        {grpc::status_code::ok, "OK"},
        {grpc::status_code::cancelled, "CANCELLED"},
        {grpc::status_code::unknown, "UNKNOWN"},
        {grpc::status_code::invalid_argument, "INVALID_ARGUMENT"},
        {grpc::status_code::deadline_exceeded, "DEADLINE_EXCEEDED"},
        {grpc::status_code::not_found, "NOT_FOUND"},
        {grpc::status_code::already_exists, "ALREADY_EXISTS"},
        {grpc::status_code::permission_denied, "PERMISSION_DENIED"},
        {grpc::status_code::resource_exhausted, "RESOURCE_EXHAUSTED"},
        {grpc::status_code::failed_precondition, "FAILED_PRECONDITION"},
        {grpc::status_code::aborted, "ABORTED"},
        {grpc::status_code::out_of_range, "OUT_OF_RANGE"},
        {grpc::status_code::unimplemented, "UNIMPLEMENTED"},
        {grpc::status_code::internal, "INTERNAL"},
        {grpc::status_code::unavailable, "UNAVAILABLE"},
        {grpc::status_code::data_loss, "DATA_LOSS"},
        {grpc::status_code::unauthenticated, "UNAUTHENTICATED"},
    };

    for (const auto& [code, name] : expected)
    {
        EXPECT_EQ(grpc::status_code_to_string(code), name)
            << "Status code " << static_cast<int>(code) << " should be " << name;
    }
}

TEST_F(IntegrationReadinessTest, MessageFrameFormat)
{
    // Verify message frame format matches gRPC wire protocol
    // Format: [1 byte compression flag][4 bytes big-endian length][payload]
    std::vector<uint8_t> payload = {0x08, 0x96, 0x01};  // Example protobuf data
    grpc::grpc_message msg(payload, false);

    auto frame = msg.serialize();

    // Check header
    EXPECT_EQ(frame[0], 0);  // Not compressed
    EXPECT_EQ(frame[1], 0);  // Length MSB
    EXPECT_EQ(frame[2], 0);
    EXPECT_EQ(frame[3], 0);
    EXPECT_EQ(frame[4], 3);  // Length LSB = 3 bytes

    // Check payload
    EXPECT_EQ(frame[5], 0x08);
    EXPECT_EQ(frame[6], 0x96);
    EXPECT_EQ(frame[7], 0x01);
}
