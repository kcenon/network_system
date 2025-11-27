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
#include "kcenon/network/protocols/http2/http2_client.h"
#include <memory>
#include <string>
#include <vector>

using namespace network_system::protocols::http2;
namespace err = network_system::error_codes;

class Http2ClientTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        client_ = std::make_shared<http2_client>("test-client");
    }

    void TearDown() override
    {
        if (client_ && client_->is_connected())
        {
            client_->disconnect();
        }
    }

    std::shared_ptr<http2_client> client_;
};

// Unit tests for http2_response
TEST_F(Http2ClientTest, ResponseGetHeaderReturnsValue)
{
    http2_response response;
    response.headers = {
        {":status", "200"},
        {"content-type", "application/json"},
        {"Content-Length", "42"}
    };

    auto status = response.get_header(":status");
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "200");

    // Case-insensitive lookup
    auto ct = response.get_header("Content-Type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");

    auto missing = response.get_header("x-custom-header");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(Http2ClientTest, ResponseGetBodyStringConvertsCorrectly)
{
    http2_response response;
    std::string expected = "Hello, HTTP/2!";
    response.body = std::vector<uint8_t>(expected.begin(), expected.end());

    EXPECT_EQ(response.get_body_string(), expected);
}

TEST_F(Http2ClientTest, ResponseEmptyBodyReturnsEmptyString)
{
    http2_response response;
    EXPECT_EQ(response.get_body_string(), "");
}

// Unit tests for http2_client construction
TEST_F(Http2ClientTest, ConstructsWithClientId)
{
    auto client = std::make_shared<http2_client>("my-client-id");
    EXPECT_FALSE(client->is_connected());
}

TEST_F(Http2ClientTest, DefaultTimeoutIs30Seconds)
{
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(30000));
}

TEST_F(Http2ClientTest, SetTimeoutUpdatesValue)
{
    client_->set_timeout(std::chrono::milliseconds(5000));
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(5000));
}

TEST_F(Http2ClientTest, DefaultSettingsAreCorrect)
{
    auto settings = client_->get_settings();

    EXPECT_EQ(settings.header_table_size, 4096u);
    EXPECT_FALSE(settings.enable_push);
    EXPECT_EQ(settings.max_concurrent_streams, 100u);
    EXPECT_EQ(settings.initial_window_size, 65535u);
    EXPECT_EQ(settings.max_frame_size, 16384u);
    EXPECT_EQ(settings.max_header_list_size, 8192u);
}

TEST_F(Http2ClientTest, SetSettingsUpdatesValues)
{
    http2_settings custom;
    custom.header_table_size = 8192;
    custom.enable_push = true;
    custom.max_concurrent_streams = 200;
    custom.initial_window_size = 131070;
    custom.max_frame_size = 32768;
    custom.max_header_list_size = 16384;

    client_->set_settings(custom);
    auto updated = client_->get_settings();

    EXPECT_EQ(updated.header_table_size, 8192u);
    EXPECT_TRUE(updated.enable_push);
    EXPECT_EQ(updated.max_concurrent_streams, 200u);
    EXPECT_EQ(updated.initial_window_size, 131070u);
    EXPECT_EQ(updated.max_frame_size, 32768u);
    EXPECT_EQ(updated.max_header_list_size, 16384u);
}

// Connection tests
TEST_F(Http2ClientTest, IsConnectedReturnsFalseBeforeConnect)
{
    EXPECT_FALSE(client_->is_connected());
}

TEST_F(Http2ClientTest, ConnectFailsWithEmptyHost)
{
    auto result = client_->connect("", 443);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::common_errors::invalid_argument);
}

TEST_F(Http2ClientTest, ConnectFailsWithInvalidHost)
{
    auto result = client_->connect("invalid.host.that.does.not.exist.example", 443);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_failed);
}

TEST_F(Http2ClientTest, DisconnectSucceedsWhenNotConnected)
{
    auto result = client_->disconnect();
    EXPECT_TRUE(result.is_ok());
}

// Request tests when not connected
TEST_F(Http2ClientTest, GetFailsWhenNotConnected)
{
    auto result = client_->get("/api/test");

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, PostFailsWhenNotConnected)
{
    auto result = client_->post("/api/test", "body");

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, PutFailsWhenNotConnected)
{
    auto result = client_->put("/api/test", "body");

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, DeleteFailsWhenNotConnected)
{
    auto result = client_->del("/api/test");

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

// Stream state tests
TEST(Http2StreamStateTest, StreamStateEnumValues)
{
    EXPECT_EQ(static_cast<int>(stream_state::idle), 0);
    EXPECT_EQ(static_cast<int>(stream_state::open), 1);
    EXPECT_EQ(static_cast<int>(stream_state::half_closed_local), 2);
    EXPECT_EQ(static_cast<int>(stream_state::half_closed_remote), 3);
    EXPECT_EQ(static_cast<int>(stream_state::closed), 4);
}

// http2_stream tests
TEST(Http2StreamTest, DefaultStreamState)
{
    http2_stream stream;

    EXPECT_EQ(stream.stream_id, 0u);
    EXPECT_EQ(stream.state, stream_state::idle);
    EXPECT_EQ(stream.window_size, 65535);
    EXPECT_FALSE(stream.headers_complete);
    EXPECT_FALSE(stream.body_complete);
    EXPECT_TRUE(stream.request_headers.empty());
    EXPECT_TRUE(stream.response_headers.empty());
    EXPECT_TRUE(stream.request_body.empty());
    EXPECT_TRUE(stream.response_body.empty());
}

TEST(Http2StreamTest, StreamIsMovable)
{
    http2_stream stream1;
    stream1.stream_id = 5;
    stream1.state = stream_state::open;
    stream1.window_size = 12345;

    http2_stream stream2 = std::move(stream1);

    EXPECT_EQ(stream2.stream_id, 5u);
    EXPECT_EQ(stream2.state, stream_state::open);
    EXPECT_EQ(stream2.window_size, 12345);
}

// http2_settings tests
TEST(Http2SettingsTest, DefaultSettings)
{
    http2_settings settings;

    EXPECT_EQ(settings.header_table_size, 4096u);
    EXPECT_FALSE(settings.enable_push);
    EXPECT_EQ(settings.max_concurrent_streams, 100u);
    EXPECT_EQ(settings.initial_window_size, 65535u);
    EXPECT_EQ(settings.max_frame_size, 16384u);
    EXPECT_EQ(settings.max_header_list_size, 8192u);
}

// Integration tests (requires network access, may be skipped in CI)
class Http2ClientIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        client_ = std::make_shared<http2_client>("integration-test-client");
    }

    void TearDown() override
    {
        if (client_ && client_->is_connected())
        {
            client_->disconnect();
        }
    }

    std::shared_ptr<http2_client> client_;
};

// This test requires network access to httpbin.org
// Skip in CI environment by checking for SKIP_NETWORK_TESTS env var
TEST_F(Http2ClientIntegrationTest, DISABLED_ConnectToHttpbin)
{
    auto result = client_->connect("nghttp2.org", 443);

    if (result.is_ok())
    {
        EXPECT_TRUE(client_->is_connected());

        auto response = client_->get("/");
        if (response.is_ok())
        {
            EXPECT_GE(response.value().status_code, 200);
            EXPECT_LT(response.value().status_code, 400);
        }

        client_->disconnect();
        EXPECT_FALSE(client_->is_connected());
    }
    else
    {
        // Network may not be available, skip gracefully
        GTEST_SKIP() << "Network not available: " << result.error().message;
    }
}
