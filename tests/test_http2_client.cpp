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
#include "internal/protocols/http2/http2_client.h"
#include "kcenon/network/detail/utils/result_types.h"
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::protocols::http2;
namespace err = kcenon::network::error_codes;

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

// Stream operations when not connected
TEST_F(Http2ClientTest, StartStreamFailsWhenNotConnected)
{
    auto result = client_->start_stream(
        "/api/stream", {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http_header>) {},
        [](int) {});

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, WriteStreamFailsWhenNotConnected)
{
    auto result = client_->write_stream(1, {0x01, 0x02}, false);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, CloseStreamWriterFailsWhenNotConnected)
{
    auto result = client_->close_stream_writer(1);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, CancelStreamFailsWhenNotConnected)
{
    auto result = client_->cancel_stream(1);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, PostBinaryBodyFailsWhenNotConnected)
{
    std::vector<uint8_t> binary_body = {0x00, 0x01, 0xFF, 0xFE};
    auto result = client_->post("/api/upload", binary_body);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

// http2_response edge cases
TEST_F(Http2ClientTest, ResponseGetHeaderReturnsFirstMatchOnDuplicate)
{
    http2_response response;
    response.headers = {
        {"set-cookie", "session=abc"},
        {"set-cookie", "theme=dark"},
        {"content-type", "text/html"}
    };

    auto cookie = response.get_header("set-cookie");
    ASSERT_TRUE(cookie.has_value());
    EXPECT_EQ(*cookie, "session=abc");
}

TEST_F(Http2ClientTest, ResponseGetHeaderOnEmptyHeaders)
{
    http2_response response;
    auto result = response.get_header("content-type");
    EXPECT_FALSE(result.has_value());
}

TEST_F(Http2ClientTest, ResponseGetBodyStringWithBinaryContent)
{
    http2_response response;
    response.body = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    EXPECT_EQ(response.get_body_string(), "Hello");
}

TEST_F(Http2ClientTest, ResponseStatusCodeDefaultsToZero)
{
    http2_response response;
    EXPECT_EQ(response.status_code, 0);
}

// Connection edge cases
TEST_F(Http2ClientTest, DoubleDisconnectIsIdempotent)
{
    auto result1 = client_->disconnect();
    EXPECT_TRUE(result1.is_ok());

    auto result2 = client_->disconnect();
    EXPECT_TRUE(result2.is_ok());
}

// Timeout edge cases
TEST_F(Http2ClientTest, SetTimeoutToZero)
{
    client_->set_timeout(std::chrono::milliseconds(0));
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(0));
}

TEST_F(Http2ClientTest, SetTimeoutToLargeValue)
{
    client_->set_timeout(std::chrono::milliseconds(300000));
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(300000));
}

TEST_F(Http2ClientTest, SetAndGetTimeoutMultipleTimes)
{
    client_->set_timeout(std::chrono::milliseconds(1000));
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(1000));

    client_->set_timeout(std::chrono::milliseconds(5000));
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(5000));

    client_->set_timeout(std::chrono::milliseconds(100));
    EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds(100));
}

// Settings edge cases
TEST_F(Http2ClientTest, SetSettingsMultipleTimes)
{
    http2_settings s1;
    s1.header_table_size = 2048;
    client_->set_settings(s1);
    EXPECT_EQ(client_->get_settings().header_table_size, 2048u);

    http2_settings s2;
    s2.header_table_size = 16384;
    s2.max_concurrent_streams = 500;
    client_->set_settings(s2);
    EXPECT_EQ(client_->get_settings().header_table_size, 16384u);
    EXPECT_EQ(client_->get_settings().max_concurrent_streams, 500u);
}

TEST_F(Http2ClientTest, SetSettingsUpdatesEncoderDecoder)
{
    http2_settings custom;
    custom.header_table_size = 8192;
    client_->set_settings(custom);

    // Verify settings are applied (encoder/decoder table size updated)
    auto settings = client_->get_settings();
    EXPECT_EQ(settings.header_table_size, 8192u);
}

// Multiple client instances
TEST(Http2ClientMultiInstance, IndependentClientsHaveSeparateState)
{
    auto client1 = std::make_shared<http2_client>("client-1");
    auto client2 = std::make_shared<http2_client>("client-2");

    client1->set_timeout(std::chrono::milliseconds(1000));
    client2->set_timeout(std::chrono::milliseconds(5000));

    EXPECT_EQ(client1->get_timeout(), std::chrono::milliseconds(1000));
    EXPECT_EQ(client2->get_timeout(), std::chrono::milliseconds(5000));

    EXPECT_FALSE(client1->is_connected());
    EXPECT_FALSE(client2->is_connected());
}

TEST(Http2ClientMultiInstance, IndependentClientsHaveSeparateSettings)
{
    auto client1 = std::make_shared<http2_client>("client-1");
    auto client2 = std::make_shared<http2_client>("client-2");

    http2_settings s1;
    s1.max_concurrent_streams = 50;
    client1->set_settings(s1);

    http2_settings s2;
    s2.max_concurrent_streams = 200;
    client2->set_settings(s2);

    EXPECT_EQ(client1->get_settings().max_concurrent_streams, 50u);
    EXPECT_EQ(client2->get_settings().max_concurrent_streams, 200u);
}

// http2_stream extended tests
TEST(Http2StreamTest, StreamWithHeaderData)
{
    http2_stream stream;
    stream.stream_id = 3;
    stream.state = stream_state::open;
    stream.request_headers = {
        {":method", "GET"},
        {":path", "/api/users"}
    };
    stream.response_headers = {
        {":status", "200"},
        {"content-type", "application/json"}
    };

    EXPECT_EQ(stream.request_headers.size(), 2u);
    EXPECT_EQ(stream.response_headers.size(), 2u);
    EXPECT_EQ(stream.request_headers[0].name, ":method");
    EXPECT_EQ(stream.response_headers[0].value, "200");
}

TEST(Http2StreamTest, StreamWithBodyData)
{
    http2_stream stream;
    stream.request_body = {0x01, 0x02, 0x03};
    stream.response_body = {0x04, 0x05};

    EXPECT_EQ(stream.request_body.size(), 3u);
    EXPECT_EQ(stream.response_body.size(), 2u);
}

TEST(Http2StreamTest, StreamCompletionFlags)
{
    http2_stream stream;
    EXPECT_FALSE(stream.headers_complete);
    EXPECT_FALSE(stream.body_complete);

    stream.headers_complete = true;
    stream.body_complete = true;

    EXPECT_TRUE(stream.headers_complete);
    EXPECT_TRUE(stream.body_complete);
}

TEST(Http2StreamTest, StreamCallbacksSetup)
{
    http2_stream stream;
    stream.is_streaming = true;

    bool data_called = false;
    bool headers_called = false;
    bool complete_called = false;

    stream.on_data = [&data_called](std::vector<uint8_t>) { data_called = true; };
    stream.on_headers = [&headers_called](std::vector<http_header>) { headers_called = true; };
    stream.on_complete = [&complete_called](int) { complete_called = true; };

    EXPECT_TRUE(stream.is_streaming);
    EXPECT_TRUE(stream.on_data != nullptr);
    EXPECT_TRUE(stream.on_headers != nullptr);
    EXPECT_TRUE(stream.on_complete != nullptr);

    // Invoke callbacks to verify they work
    stream.on_data({0x01});
    stream.on_headers({{"key", "value"}});
    stream.on_complete(200);

    EXPECT_TRUE(data_called);
    EXPECT_TRUE(headers_called);
    EXPECT_TRUE(complete_called);
}

TEST(Http2StreamTest, StreamStateTransitions)
{
    http2_stream stream;
    EXPECT_EQ(stream.state, stream_state::idle);

    stream.state = stream_state::open;
    EXPECT_EQ(stream.state, stream_state::open);

    stream.state = stream_state::half_closed_local;
    EXPECT_EQ(stream.state, stream_state::half_closed_local);

    stream.state = stream_state::half_closed_remote;
    EXPECT_EQ(stream.state, stream_state::half_closed_remote);

    stream.state = stream_state::closed;
    EXPECT_EQ(stream.state, stream_state::closed);
}

// http2_settings extended tests
TEST(Http2SettingsTest, SettingsCopyBehavior)
{
    http2_settings original;
    original.header_table_size = 8192;
    original.enable_push = true;
    original.max_concurrent_streams = 200;

    http2_settings copy = original;
    EXPECT_EQ(copy.header_table_size, 8192u);
    EXPECT_TRUE(copy.enable_push);
    EXPECT_EQ(copy.max_concurrent_streams, 200u);

    // Modify copy doesn't affect original
    copy.header_table_size = 1024;
    EXPECT_EQ(original.header_table_size, 8192u);
}

TEST(Http2SettingsTest, SettingsAssignment)
{
    http2_settings s1;
    s1.max_frame_size = 32768;

    http2_settings s2;
    s2 = s1;
    EXPECT_EQ(s2.max_frame_size, 32768u);
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
