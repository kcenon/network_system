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
#include "internal/protocols/http2/http2_server_stream.h"
#include "internal/protocols/http2/http2_request.h"
#include "kcenon/network/detail/utils/result_types.h"
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::protocols::http2;
using kcenon::network::VoidResult;
using kcenon::network::ok;
using kcenon::network::error_void;
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

TEST(TlsConfigTest, CopyBehavior)
{
    tls_config original;
    original.cert_file = "/cert.pem";
    original.key_file = "/key.pem";
    original.verify_client = true;

    tls_config copy = original;
    EXPECT_EQ(copy.cert_file, "/cert.pem");
    EXPECT_EQ(copy.key_file, "/key.pem");
    EXPECT_TRUE(copy.verify_client);

    copy.cert_file = "/other.pem";
    EXPECT_EQ(original.cert_file, "/cert.pem");
}

// ============================================================================
// Extended http2_request tests
// ============================================================================

TEST_F(Http2RequestTest, DefaultFieldsAreEmpty)
{
    http2_request request;
    EXPECT_TRUE(request.method.empty());
    EXPECT_TRUE(request.path.empty());
    EXPECT_TRUE(request.authority.empty());
    EXPECT_TRUE(request.scheme.empty());
    EXPECT_TRUE(request.headers.empty());
    EXPECT_TRUE(request.body.empty());
}

TEST_F(Http2RequestTest, IsValidEmptyMethodReturnsFalse)
{
    http2_request request;
    EXPECT_FALSE(request.is_valid());
}

TEST_F(Http2RequestTest, IsValidGetWithoutPathReturnsFalse)
{
    http2_request request;
    request.method = "GET";
    request.scheme = "https";
    // path is empty
    EXPECT_FALSE(request.is_valid());
}

TEST_F(Http2RequestTest, IsValidPostWithAllFields)
{
    http2_request request;
    request.method = "POST";
    request.path = "/api/data";
    request.scheme = "https";
    request.authority = "example.com";
    EXPECT_TRUE(request.is_valid());
}

TEST_F(Http2RequestTest, FromHeadersWithEmptyInput)
{
    std::vector<http_header> headers;
    auto request = http2_request::from_headers(headers);

    EXPECT_TRUE(request.method.empty());
    EXPECT_TRUE(request.path.empty());
    EXPECT_TRUE(request.headers.empty());
}

TEST_F(Http2RequestTest, FromHeadersSkipsEmptyNames)
{
    std::vector<http_header> headers = {
        {"", "value"},
        {":method", "GET"},
        {":path", "/"}
    };

    auto request = http2_request::from_headers(headers);
    EXPECT_EQ(request.method, "GET");
    EXPECT_EQ(request.path, "/");
    EXPECT_TRUE(request.headers.empty());
}

TEST_F(Http2RequestTest, FromHeadersWithDuplicateRegularHeaders)
{
    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"},
        {"accept", "text/html"},
        {"accept", "application/json"}
    };

    auto request = http2_request::from_headers(headers);
    EXPECT_EQ(request.headers.size(), 2u);
    EXPECT_EQ(request.headers[0].value, "text/html");
    EXPECT_EQ(request.headers[1].value, "application/json");
}

TEST_F(Http2RequestTest, GetHeaderCaseInsensitiveMixedCase)
{
    http2_request request;
    request.headers = {{"X-Custom-Header", "custom-value"}};

    auto result = request.get_header("x-custom-header");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "custom-value");

    auto result2 = request.get_header("X-CUSTOM-HEADER");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(*result2, "custom-value");
}

TEST_F(Http2RequestTest, ContentLengthNegativeValue)
{
    http2_request request;
    request.headers = {{"content-length", "-1"}};

    // stoull on negative string: implementation-defined, but should not crash
    auto cl = request.content_length();
    // Either returns a value or nullopt; either way no crash
    (void)cl;
}

TEST_F(Http2RequestTest, ContentLengthZero)
{
    http2_request request;
    request.headers = {{"content-length", "0"}};

    auto cl = request.content_length();
    ASSERT_TRUE(cl.has_value());
    EXPECT_EQ(*cl, 0u);
}

// ============================================================================
// Extended http2_server tests
// ============================================================================

TEST_F(Http2ServerTest, DoubleStopIsIdempotent)
{
    auto r1 = server_->start(18090);
    EXPECT_TRUE(r1.is_ok());

    auto r2 = server_->stop();
    EXPECT_TRUE(r2.is_ok());

    auto r3 = server_->stop();
    EXPECT_TRUE(r3.is_ok());
}

TEST_F(Http2ServerTest, StartTlsAlreadyRunningFails)
{
    auto r1 = server_->start(18091);
    EXPECT_TRUE(r1.is_ok());

    tls_config config;
    config.cert_file = "/nonexistent.pem";
    config.key_file = "/nonexistent.pem";
    auto r2 = server_->start_tls(18092, config);
    EXPECT_TRUE(r2.is_err());
    EXPECT_EQ(r2.error().code, err::common_errors::already_exists);

    server_->stop();
}

TEST_F(Http2ServerTest, SetSettingsMultipleTimes)
{
    http2_settings s1;
    s1.header_table_size = 2048;
    server_->set_settings(s1);
    EXPECT_EQ(server_->get_settings().header_table_size, 2048u);

    http2_settings s2;
    s2.header_table_size = 16384;
    s2.max_concurrent_streams = 500;
    server_->set_settings(s2);
    EXPECT_EQ(server_->get_settings().header_table_size, 16384u);
    EXPECT_EQ(server_->get_settings().max_concurrent_streams, 500u);
}

TEST_F(Http2ServerTest, ServerIdPreservedAfterStart)
{
    EXPECT_EQ(server_->server_id(), "test-server");

    auto r = server_->start(18093);
    EXPECT_TRUE(r.is_ok());
    EXPECT_EQ(server_->server_id(), "test-server");

    server_->stop();
}

TEST_F(Http2ServerTest, ActiveCountsAfterStop)
{
    server_->start(18094);
    server_->stop();

    EXPECT_EQ(server_->active_connections(), 0u);
    EXPECT_EQ(server_->active_streams(), 0u);
}

TEST(Http2ServerConstruction, MultipleServersIndependent)
{
    auto server1 = std::make_shared<http2_server>("server-1");
    auto server2 = std::make_shared<http2_server>("server-2");

    EXPECT_EQ(server1->server_id(), "server-1");
    EXPECT_EQ(server2->server_id(), "server-2");

    http2_settings s;
    s.max_concurrent_streams = 50;
    server1->set_settings(s);

    EXPECT_EQ(server1->get_settings().max_concurrent_streams, 50u);
    EXPECT_EQ(server2->get_settings().max_concurrent_streams, 100u);
}

// ============================================================================
// http2_server_stream tests
// ============================================================================

class Http2ServerStreamTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        encoder_ = std::make_shared<hpack_encoder>(4096);
        sent_frames_.clear();
        sender_error_ = false;

        http2_request req;
        req.method = "GET";
        req.path = "/api/test";
        req.scheme = "https";
        req.authority = "example.com";
        req.headers = {{"accept", "application/json"}};

        stream_ = std::make_unique<http2_server_stream>(
            1, std::move(req), encoder_,
            [this](const frame& f) -> VoidResult {
                if (sender_error_)
                {
                    return error_void(
                        err::network_system::send_failed,
                        "Simulated send failure",
                        "test");
                }
                sent_frames_.push_back(f.serialize());
                return ok();
            },
            16384);
    }

    std::shared_ptr<hpack_encoder> encoder_;
    std::unique_ptr<http2_server_stream> stream_;
    std::vector<std::vector<uint8_t>> sent_frames_;
    bool sender_error_ = false;
};

// Accessor tests
TEST_F(Http2ServerStreamTest, StreamIdReturnsCorrectValue)
{
    EXPECT_EQ(stream_->stream_id(), 1u);
}

TEST_F(Http2ServerStreamTest, MethodReturnsRequestMethod)
{
    EXPECT_EQ(stream_->method(), "GET");
}

TEST_F(Http2ServerStreamTest, PathReturnsRequestPath)
{
    EXPECT_EQ(stream_->path(), "/api/test");
}

TEST_F(Http2ServerStreamTest, HeadersReturnsRequestHeaders)
{
    auto& headers = stream_->headers();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, "accept");
    EXPECT_EQ(headers[0].value, "application/json");
}

TEST_F(Http2ServerStreamTest, RequestReturnsFullRequest)
{
    auto& req = stream_->request();
    EXPECT_EQ(req.method, "GET");
    EXPECT_EQ(req.path, "/api/test");
    EXPECT_EQ(req.scheme, "https");
    EXPECT_EQ(req.authority, "example.com");
}

// State tests
TEST_F(Http2ServerStreamTest, InitialStateIsOpen)
{
    EXPECT_EQ(stream_->state(), stream_state::open);
    EXPECT_TRUE(stream_->is_open());
    EXPECT_FALSE(stream_->headers_sent());
}

TEST_F(Http2ServerStreamTest, DefaultWindowSize)
{
    EXPECT_EQ(stream_->window_size(), 65535);
}

TEST_F(Http2ServerStreamTest, UpdateWindowIncreasesSize)
{
    stream_->update_window(1000);
    EXPECT_EQ(stream_->window_size(), 65535 + 1000);
}

TEST_F(Http2ServerStreamTest, UpdateWindowDecreases)
{
    stream_->update_window(-100);
    EXPECT_EQ(stream_->window_size(), 65535 - 100);
}

// send_headers tests
TEST_F(Http2ServerStreamTest, SendHeadersSuccess)
{
    auto result = stream_->send_headers(200, {{"content-type", "text/plain"}});

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(stream_->headers_sent());
    EXPECT_TRUE(stream_->is_open());
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, SendHeadersWithEndStream)
{
    auto result = stream_->send_headers(204, {}, true);

    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(stream_->headers_sent());
    EXPECT_EQ(stream_->state(), stream_state::half_closed_local);
    EXPECT_FALSE(stream_->is_open());
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, SendHeadersDuplicateCallFails)
{
    auto r1 = stream_->send_headers(200, {});
    EXPECT_TRUE(r1.is_ok());

    auto r2 = stream_->send_headers(200, {});
    EXPECT_TRUE(r2.is_err());
    EXPECT_EQ(r2.error().code, err::common_errors::invalid_argument);
}

TEST_F(Http2ServerStreamTest, SendHeadersAfterClosedStreamFails)
{
    stream_->send_headers(200, {}, true);
    // State is now half_closed_local

    // Create new stream that starts closed
    http2_request req;
    req.method = "GET";
    req.path = "/";
    req.scheme = "https";
    auto closed_stream = std::make_unique<http2_server_stream>(
        3, std::move(req), encoder_,
        [this](const frame& f) -> VoidResult {
            sent_frames_.push_back(f.serialize());
            return ok();
        },
        16384);

    // Send headers with end_stream to close
    closed_stream->send_headers(200, {}, true);
    // Now try sending again
    auto result = closed_stream->send_headers(200, {});
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2ServerStreamTest, SendHeadersWithSendError)
{
    sender_error_ = true;
    auto result = stream_->send_headers(200, {});

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::send_failed);
}

// send_data tests
TEST_F(Http2ServerStreamTest, SendDataBeforeHeadersFails)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto result = stream_->send_data(data);

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::common_errors::invalid_argument);
}

TEST_F(Http2ServerStreamTest, SendDataSuccess)
{
    stream_->send_headers(200, {{"content-type", "text/plain"}});
    sent_frames_.clear();

    std::string body = "Hello, World!";
    std::vector<uint8_t> data(body.begin(), body.end());
    auto result = stream_->send_data(data, true);

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(stream_->state(), stream_state::half_closed_local);
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, SendDataStringVariant)
{
    stream_->send_headers(200, {});
    sent_frames_.clear();

    auto result = stream_->send_data("Hello!", true);

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, SendDataEmptyWithEndStream)
{
    stream_->send_headers(200, {});
    sent_frames_.clear();

    auto result = stream_->send_data(std::vector<uint8_t>{}, true);

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(stream_->state(), stream_state::half_closed_local);
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, SendDataAfterClosedStreamFails)
{
    stream_->send_headers(200, {}, true);

    auto result = stream_->send_data("data");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::common_errors::invalid_argument);
}

TEST_F(Http2ServerStreamTest, SendDataFrameSplitting)
{
    // Create stream with small max_frame_size to test splitting
    http2_request req;
    req.method = "POST";
    req.path = "/upload";
    req.scheme = "https";

    std::vector<std::vector<uint8_t>> small_frames;
    auto small_stream = std::make_unique<http2_server_stream>(
        5, std::move(req), encoder_,
        [&small_frames](const frame& f) -> VoidResult {
            small_frames.push_back(f.serialize());
            return ok();
        },
        10);  // max_frame_size = 10 bytes

    small_stream->send_headers(200, {});
    small_frames.clear();

    // Send 25 bytes of data — should split into 3 frames (10 + 10 + 5)
    std::vector<uint8_t> data(25, 0xAA);
    auto result = small_stream->send_data(data, true);

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(small_frames.size(), 3u);
}

TEST_F(Http2ServerStreamTest, SendDataWithSendError)
{
    stream_->send_headers(200, {});
    sender_error_ = true;

    auto result = stream_->send_data("data");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::send_failed);
}

// Streaming response pattern tests
TEST_F(Http2ServerStreamTest, StreamingResponsePattern)
{
    // start_response (headers without end_stream)
    auto r1 = stream_->start_response(200, {{"content-type", "text/event-stream"}});
    EXPECT_TRUE(r1.is_ok());
    EXPECT_TRUE(stream_->headers_sent());
    EXPECT_TRUE(stream_->is_open());

    // write chunks
    auto r2 = stream_->write({0x01, 0x02, 0x03});
    EXPECT_TRUE(r2.is_ok());
    EXPECT_TRUE(stream_->is_open());

    auto r3 = stream_->write({0x04, 0x05});
    EXPECT_TRUE(r3.is_ok());
    EXPECT_TRUE(stream_->is_open());

    // end_response
    auto r4 = stream_->end_response();
    EXPECT_TRUE(r4.is_ok());
    EXPECT_EQ(stream_->state(), stream_state::half_closed_local);
    EXPECT_FALSE(stream_->is_open());

    // headers(1) + write(2) + end_response(1) = 4 frames
    EXPECT_EQ(sent_frames_.size(), 4u);
}

TEST_F(Http2ServerStreamTest, WriteAfterEndResponseFails)
{
    stream_->start_response(200, {});
    stream_->end_response();

    auto result = stream_->write({0x01});
    EXPECT_TRUE(result.is_err());
}

// reset tests
TEST_F(Http2ServerStreamTest, ResetSendsRstStreamFrame)
{
    auto result = stream_->reset();

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(stream_->state(), stream_state::closed);
    EXPECT_FALSE(stream_->is_open());
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, ResetWithCustomErrorCode)
{
    auto result = stream_->reset(static_cast<uint32_t>(error_code::refused_stream));

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(stream_->state(), stream_state::closed);
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, ResetAlreadyClosedIsNoop)
{
    stream_->reset();
    sent_frames_.clear();

    auto result = stream_->reset();
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(sent_frames_.size(), 0u);
}

TEST_F(Http2ServerStreamTest, ResetWithSendError)
{
    sender_error_ = true;
    auto result = stream_->reset();

    // reset still transitions to closed even on send error
    EXPECT_EQ(stream_->state(), stream_state::closed);
}

// Full lifecycle test
TEST_F(Http2ServerStreamTest, FullResponseLifecycle)
{
    // 1. Initial state
    EXPECT_EQ(stream_->state(), stream_state::open);
    EXPECT_FALSE(stream_->headers_sent());

    // 2. Send headers
    auto r1 = stream_->send_headers(200, {
        {"content-type", "application/json"},
        {"x-request-id", "abc-123"}
    });
    EXPECT_TRUE(r1.is_ok());
    EXPECT_TRUE(stream_->headers_sent());
    EXPECT_EQ(stream_->state(), stream_state::open);

    // 3. Send body with end_stream
    auto r2 = stream_->send_data(R"({"status":"ok"})", true);
    EXPECT_TRUE(r2.is_ok());
    EXPECT_EQ(stream_->state(), stream_state::half_closed_local);

    // 4. Further sends should fail
    auto r3 = stream_->send_data("more data");
    EXPECT_TRUE(r3.is_err());

    // Total: 1 HEADERS + 1 DATA = 2 frames
    EXPECT_EQ(sent_frames_.size(), 2u);
}

// Server stream with POST request
TEST(Http2ServerStreamPostTest, StreamWithPostBody)
{
    auto encoder = std::make_shared<hpack_encoder>(4096);
    std::vector<std::vector<uint8_t>> frames;

    http2_request req;
    req.method = "POST";
    req.path = "/api/users";
    req.scheme = "https";
    req.authority = "api.example.com";
    req.headers = {
        {"content-type", "application/json"},
        {"content-length", "15"}
    };
    std::string body_str = R"({"name":"test"})";
    req.body = std::vector<uint8_t>(body_str.begin(), body_str.end());

    http2_server_stream stream(
        3, std::move(req), encoder,
        [&frames](const frame& f) -> VoidResult {
            frames.push_back(f.serialize());
            return ok();
        });

    EXPECT_EQ(stream.stream_id(), 3u);
    EXPECT_EQ(stream.method(), "POST");
    EXPECT_EQ(stream.path(), "/api/users");
    EXPECT_EQ(stream.request().get_body_string(), R"({"name":"test"})");

    auto ct = stream.request().content_type();
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");

    auto cl = stream.request().content_length();
    ASSERT_TRUE(cl.has_value());
    EXPECT_EQ(*cl, 15u);
}
