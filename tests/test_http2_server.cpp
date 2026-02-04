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
#include "internal/protocols/http2/http2_server.h"
#include "internal/protocols/http2/http2_request.h"
#include "kcenon/network/detail/utils/result_types.h"
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::protocols::http2;
namespace err = kcenon::network::error_codes;

// ============================================================================
// http2_request tests
// ============================================================================

class Http2RequestTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
};

TEST_F(Http2RequestTest, GetHeaderReturnsValue)
{
    http2_request request;
    request.headers = {
        {"content-type", "application/json"},
        {"Content-Length", "42"}
    };

    auto ct = request.get_header("content-type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");

    // Case-insensitive lookup
    auto ct2 = request.get_header("Content-Type");
    ASSERT_TRUE(ct2.has_value());
    EXPECT_EQ(*ct2, "application/json");

    auto missing = request.get_header("x-custom-header");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(Http2RequestTest, ContentTypeHelper)
{
    http2_request request;
    request.headers = {{"content-type", "application/json"}};

    auto ct = request.content_type();
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");
}

TEST_F(Http2RequestTest, ContentLengthHelper)
{
    http2_request request;
    request.headers = {{"content-length", "1024"}};

    auto cl = request.content_length();
    ASSERT_TRUE(cl.has_value());
    EXPECT_EQ(*cl, 1024u);
}

TEST_F(Http2RequestTest, ContentLengthInvalid)
{
    http2_request request;
    request.headers = {{"content-length", "not-a-number"}};

    auto cl = request.content_length();
    EXPECT_FALSE(cl.has_value());
}

TEST_F(Http2RequestTest, ContentLengthMissing)
{
    http2_request request;
    auto cl = request.content_length();
    EXPECT_FALSE(cl.has_value());
}

TEST_F(Http2RequestTest, GetBodyString)
{
    http2_request request;
    std::string expected = "Hello, World!";
    request.body = std::vector<uint8_t>(expected.begin(), expected.end());

    EXPECT_EQ(request.get_body_string(), expected);
}

TEST_F(Http2RequestTest, EmptyBodyReturnsEmptyString)
{
    http2_request request;
    EXPECT_EQ(request.get_body_string(), "");
}

TEST_F(Http2RequestTest, IsValidWithAllFields)
{
    http2_request request;
    request.method = "GET";
    request.path = "/api/test";
    request.scheme = "https";
    request.authority = "example.com";

    EXPECT_TRUE(request.is_valid());
}

TEST_F(Http2RequestTest, IsValidWithoutScheme)
{
    http2_request request;
    request.method = "GET";
    request.path = "/api/test";

    EXPECT_FALSE(request.is_valid());
}

TEST_F(Http2RequestTest, IsValidConnectMethod)
{
    http2_request request;
    request.method = "CONNECT";
    request.authority = "example.com:443";

    EXPECT_TRUE(request.is_valid());
}

TEST_F(Http2RequestTest, IsValidConnectWithoutAuthority)
{
    http2_request request;
    request.method = "CONNECT";

    EXPECT_FALSE(request.is_valid());
}

TEST_F(Http2RequestTest, FromHeadersParsePseudoHeaders)
{
    std::vector<http_header> headers = {
        {":method", "POST"},
        {":path", "/api/users"},
        {":scheme", "https"},
        {":authority", "api.example.com"},
        {"content-type", "application/json"},
        {"accept", "application/json"}
    };

    auto request = http2_request::from_headers(headers);

    EXPECT_EQ(request.method, "POST");
    EXPECT_EQ(request.path, "/api/users");
    EXPECT_EQ(request.scheme, "https");
    EXPECT_EQ(request.authority, "api.example.com");
    EXPECT_EQ(request.headers.size(), 2u);
}

TEST_F(Http2RequestTest, FromHeadersIgnoresUnknownPseudoHeaders)
{
    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"},
        {":unknown", "value"},
        {"x-custom", "custom-value"}
    };

    auto request = http2_request::from_headers(headers);

    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.headers.size(), 1u);
    EXPECT_EQ(request.headers[0].name, "x-custom");
}

// ============================================================================
// http2_server tests
// ============================================================================

class Http2ServerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server_ = std::make_shared<http2_server>("test-server");
    }

    void TearDown() override
    {
        if (server_ && server_->is_running()) {
            server_->stop();
        }
    }

    std::shared_ptr<http2_server> server_;
};

TEST_F(Http2ServerTest, ConstructWithServerId)
{
    EXPECT_EQ(server_->server_id(), "test-server");
}

TEST_F(Http2ServerTest, InitiallyNotRunning)
{
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTest, InitialActiveConnectionsIsZero)
{
    EXPECT_EQ(server_->active_connections(), 0u);
}

TEST_F(Http2ServerTest, InitialActiveStreamsIsZero)
{
    EXPECT_EQ(server_->active_streams(), 0u);
}

TEST_F(Http2ServerTest, DefaultSettings)
{
    auto settings = server_->get_settings();
    EXPECT_EQ(settings.header_table_size, 4096u);
    EXPECT_FALSE(settings.enable_push);
    EXPECT_EQ(settings.max_concurrent_streams, 100u);
    EXPECT_EQ(settings.initial_window_size, 65535u);
    EXPECT_EQ(settings.max_frame_size, 16384u);
}

TEST_F(Http2ServerTest, SetSettings)
{
    http2_settings settings;
    settings.header_table_size = 8192;
    settings.max_concurrent_streams = 200;
    settings.initial_window_size = 131072;
    settings.max_frame_size = 32768;

    server_->set_settings(settings);

    auto result = server_->get_settings();
    EXPECT_EQ(result.header_table_size, 8192u);
    EXPECT_EQ(result.max_concurrent_streams, 200u);
    EXPECT_EQ(result.initial_window_size, 131072u);
    EXPECT_EQ(result.max_frame_size, 32768u);
}

TEST_F(Http2ServerTest, SetRequestHandler)
{
    bool handler_set = false;
    server_->set_request_handler([&handler_set](http2_server_stream& /*stream*/,
                                                 const http2_request& /*request*/) {
        handler_set = true;
    });

    // Handler is set but not called yet
    EXPECT_FALSE(handler_set);
}

TEST_F(Http2ServerTest, SetErrorHandler)
{
    std::string error_message;
    server_->set_error_handler([&error_message](const std::string& msg) {
        error_message = msg;
    });

    // Error handler is set but not triggered
    EXPECT_TRUE(error_message.empty());
}

TEST_F(Http2ServerTest, StartWithoutTLS)
{
    // Start server on a high port to avoid permission issues
    auto result = server_->start(18080);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(server_->is_running());

    server_->stop();
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTest, StopNotRunningServerSucceeds)
{
    EXPECT_FALSE(server_->is_running());
    auto result = server_->stop();
    EXPECT_TRUE(result.is_ok());
}

TEST_F(Http2ServerTest, StartAlreadyRunningFails)
{
    auto result1 = server_->start(18081);
    EXPECT_TRUE(result1.is_ok());
    EXPECT_TRUE(server_->is_running());

    auto result2 = server_->start(18082);
    EXPECT_TRUE(result2.is_err());

    server_->stop();
}

TEST_F(Http2ServerTest, ActiveConnectionsAfterStart)
{
    server_->start(18083);
    EXPECT_EQ(server_->active_connections(), 0u);
    server_->stop();
}

// ============================================================================
// http2_settings tests
// ============================================================================

TEST(Http2SettingsTest, DefaultValues)
{
    http2_settings settings;
    EXPECT_EQ(settings.header_table_size, 4096u);
    EXPECT_FALSE(settings.enable_push);
    EXPECT_EQ(settings.max_concurrent_streams, 100u);
    EXPECT_EQ(settings.initial_window_size, 65535u);
    EXPECT_EQ(settings.max_frame_size, 16384u);
    EXPECT_EQ(settings.max_header_list_size, 8192u);
}

// ============================================================================
// tls_config tests
// ============================================================================

TEST(TlsConfigTest, DefaultValues)
{
    tls_config config;
    EXPECT_TRUE(config.cert_file.empty());
    EXPECT_TRUE(config.key_file.empty());
    EXPECT_TRUE(config.ca_file.empty());
    EXPECT_FALSE(config.verify_client);
}

TEST(TlsConfigTest, SetValues)
{
    tls_config config;
    config.cert_file = "/path/to/cert.pem";
    config.key_file = "/path/to/key.pem";
    config.ca_file = "/path/to/ca.pem";
    config.verify_client = true;

    EXPECT_EQ(config.cert_file, "/path/to/cert.pem");
    EXPECT_EQ(config.key_file, "/path/to/key.pem");
    EXPECT_EQ(config.ca_file, "/path/to/ca.pem");
    EXPECT_TRUE(config.verify_client);
}
