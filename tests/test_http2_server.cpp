// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include "internal/protocols/http2/http2_server.h"
#include "internal/protocols/http2/http2_server_stream.h"
#include "internal/protocols/http2/http2_request.h"
#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/hpack.h"
#include "kcenon/network/detail/utils/result_types.h"

#include <asio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
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

// ============================================================================
// Additional http2_server coverage (Issue #1064, Part of #953)
// ============================================================================
//
// These tests exercise the surface of http2_server / http2_server_connection
// that is reachable WITHOUT a connected HTTP/2 client. Behind-the-handshake
// branches (frame parsing loop, settings exchange, stream-state transitions
// triggered by peer frames) require an in-process HTTP/2 loopback fixture
// that does not yet exist; building it is a separate, larger work item.
//
// The intent of this batch is to lock down the public-API guards, constructor
// behavior, idempotency, multi-instance isolation, and boundary inputs that
// can be verified deterministically with no network I/O.

// ---------------------------------------------------------------------------
// Constructor variations
// ---------------------------------------------------------------------------

TEST(Http2ServerConstruction, EmptyServerId)
{
    auto server = std::make_shared<http2_server>("");
    EXPECT_EQ(server->server_id(), "");
    EXPECT_FALSE(server->is_running());
}

TEST(Http2ServerConstruction, LongServerId)
{
    std::string long_id(512, 'x');
    auto server = std::make_shared<http2_server>(long_id);
    EXPECT_EQ(server->server_id(), long_id);
    EXPECT_FALSE(server->is_running());
}

TEST(Http2ServerConstruction, ServerIdWithSpecialCharacters)
{
    auto server = std::make_shared<http2_server>("server/id-1.0_test:42");
    EXPECT_EQ(server->server_id(), "server/id-1.0_test:42");
}

TEST(Http2ServerConstruction, DefaultEncoderTableSizeMatchesSettings)
{
    // Construction wires encoder/decoder to settings_.header_table_size.
    // We can verify default settings remained 4096 after construction.
    auto server = std::make_shared<http2_server>("encoder-test");
    auto s = server->get_settings();
    EXPECT_EQ(s.header_table_size, 4096u);
}

TEST(Http2ServerConstruction, ManyServersIndependent)
{
    std::vector<std::shared_ptr<http2_server>> servers;
    for (int i = 0; i < 16; ++i)
    {
        servers.push_back(
            std::make_shared<http2_server>("srv-" + std::to_string(i)));
    }

    for (size_t i = 0; i < servers.size(); ++i)
    {
        EXPECT_EQ(servers[i]->server_id(), "srv-" + std::to_string(i));
        EXPECT_FALSE(servers[i]->is_running());
        EXPECT_EQ(servers[i]->active_connections(), 0u);
        EXPECT_EQ(servers[i]->active_streams(), 0u);
    }
}

// ---------------------------------------------------------------------------
// Lifecycle / idempotency
// ---------------------------------------------------------------------------

TEST_F(Http2ServerTest, StopBeforeStartReturnsOk)
{
    EXPECT_FALSE(server_->is_running());
    auto r = server_->stop();
    EXPECT_TRUE(r.is_ok());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTest, StartStopRestartCycle)
{
    auto r1 = server_->start(18100);
    ASSERT_TRUE(r1.is_ok());
    EXPECT_TRUE(server_->is_running());

    auto r2 = server_->stop();
    EXPECT_TRUE(r2.is_ok());
    EXPECT_FALSE(server_->is_running());

    auto r3 = server_->start(18101);
    EXPECT_TRUE(r3.is_ok());
    EXPECT_TRUE(server_->is_running());

    server_->stop();
}

TEST_F(Http2ServerTest, StartThenStartTlsAfterStopUsesNewConfig)
{
    auto r1 = server_->start(18102);
    ASSERT_TRUE(r1.is_ok());
    server_->stop();

    // Now start_tls with a bad cert should fail with bind_failed (or similar
    // construction error) - but should NOT return already_exists.
    tls_config config;
    config.cert_file = "/nonexistent-cert.pem";
    config.key_file = "/nonexistent-key.pem";
    auto r2 = server_->start_tls(18103, config);
    EXPECT_TRUE(r2.is_err());
    EXPECT_NE(r2.error().code, err::common_errors::already_exists);
}

TEST_F(Http2ServerTest, StartTlsWithMissingCertReportsBindFailed)
{
    tls_config config;
    config.cert_file = "/nonexistent-cert.pem";
    config.key_file = "/nonexistent-key.pem";

    auto r = server_->start_tls(18104, config);
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTest, StartTlsWithEmptyCertReportsError)
{
    tls_config config;
    // Both empty
    auto r = server_->start_tls(18105, config);
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTest, StartTwiceOnSamePortFails)
{
    auto r1 = server_->start(18106);
    ASSERT_TRUE(r1.is_ok());

    auto server2 = std::make_shared<http2_server>("second");
    auto r2 = server2->start(18106);
    EXPECT_TRUE(r2.is_err());
    EXPECT_FALSE(server2->is_running());

    server_->stop();
}

// ---------------------------------------------------------------------------
// Handler registration
// ---------------------------------------------------------------------------

TEST_F(Http2ServerTest, SetRequestHandlerNullThenSetReal)
{
    server_->set_request_handler(nullptr);
    bool called = false;
    server_->set_request_handler(
        [&called](http2_server_stream&, const http2_request&) { called = true; });

    // Handler stored without invocation (no client connected).
    EXPECT_FALSE(called);
}

TEST_F(Http2ServerTest, SetErrorHandlerNullThenSetReal)
{
    server_->set_error_handler(nullptr);
    std::string captured;
    server_->set_error_handler(
        [&captured](const std::string& m) { captured = m; });

    EXPECT_TRUE(captured.empty());
}

TEST_F(Http2ServerTest, ReplaceRequestHandlerKeepsServerUsable)
{
    int v = 0;
    server_->set_request_handler(
        [&v](http2_server_stream&, const http2_request&) { v = 1; });
    server_->set_request_handler(
        [&v](http2_server_stream&, const http2_request&) { v = 2; });

    EXPECT_EQ(v, 0);
    EXPECT_FALSE(server_->is_running());
}

// ---------------------------------------------------------------------------
// Settings boundary values
// ---------------------------------------------------------------------------

TEST_F(Http2ServerTest, SetSettingsZeroValuesAccepted)
{
    http2_settings s;
    s.header_table_size = 0;
    s.max_concurrent_streams = 0;
    s.initial_window_size = 0;
    s.max_frame_size = 0;
    s.max_header_list_size = 0;

    server_->set_settings(s);

    auto out = server_->get_settings();
    EXPECT_EQ(out.header_table_size, 0u);
    EXPECT_EQ(out.max_concurrent_streams, 0u);
    EXPECT_EQ(out.initial_window_size, 0u);
    EXPECT_EQ(out.max_frame_size, 0u);
    EXPECT_EQ(out.max_header_list_size, 0u);
}

TEST_F(Http2ServerTest, SetSettingsMaxValuesAccepted)
{
    http2_settings s;
    s.header_table_size = 0xFFFFFFFFu;
    s.max_concurrent_streams = 0xFFFFFFFFu;
    s.initial_window_size = 0x7FFFFFFFu;
    s.max_frame_size = 0x00FFFFFFu;
    s.max_header_list_size = 0xFFFFFFFFu;

    server_->set_settings(s);

    auto out = server_->get_settings();
    EXPECT_EQ(out.header_table_size, 0xFFFFFFFFu);
    EXPECT_EQ(out.max_concurrent_streams, 0xFFFFFFFFu);
    EXPECT_EQ(out.initial_window_size, 0x7FFFFFFFu);
    EXPECT_EQ(out.max_frame_size, 0x00FFFFFFu);
    EXPECT_EQ(out.max_header_list_size, 0xFFFFFFFFu);
}

TEST_F(Http2ServerTest, SetSettingsEnablePushTogglePersists)
{
    http2_settings s;
    s.enable_push = true;
    server_->set_settings(s);
    EXPECT_TRUE(server_->get_settings().enable_push);

    s.enable_push = false;
    server_->set_settings(s);
    EXPECT_FALSE(server_->get_settings().enable_push);
}

TEST_F(Http2ServerTest, GetSettingsReturnsValueCopy)
{
    auto a = server_->get_settings();
    a.max_frame_size = 99u;

    auto b = server_->get_settings();
    EXPECT_NE(b.max_frame_size, 99u);
}

// ---------------------------------------------------------------------------
// active_connections / active_streams accessors
// ---------------------------------------------------------------------------

TEST_F(Http2ServerTest, ActiveStreamsZeroBeforeStart)
{
    EXPECT_EQ(server_->active_streams(), 0u);
}

TEST_F(Http2ServerTest, ActiveCountsAfterRepeatedStartStop)
{
    for (unsigned short port = 18110; port < 18113; ++port)
    {
        auto r = server_->start(port);
        ASSERT_TRUE(r.is_ok());
        EXPECT_EQ(server_->active_connections(), 0u);
        EXPECT_EQ(server_->active_streams(), 0u);
        server_->stop();
    }
}

// ---------------------------------------------------------------------------
// wait()/stop() interplay
// ---------------------------------------------------------------------------

TEST_F(Http2ServerTest, StopUnblocksWait)
{
    auto r = server_->start(18120);
    ASSERT_TRUE(r.is_ok());

    std::thread waiter([this] { server_->wait(); });
    // Give the waiter time to enter wait().
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server_->stop();
    waiter.join();  // must return
    EXPECT_FALSE(server_->is_running());
}

// ---------------------------------------------------------------------------
// tls_config additional cases
// ---------------------------------------------------------------------------

TEST(TlsConfigTest, EmptyCaFileWithVerifyClientFalse)
{
    tls_config config;
    config.cert_file = "/cert.pem";
    config.key_file = "/key.pem";
    config.ca_file = "";
    config.verify_client = false;

    EXPECT_TRUE(config.ca_file.empty());
    EXPECT_FALSE(config.verify_client);
}

TEST(TlsConfigTest, AssignmentOperator)
{
    tls_config a;
    a.cert_file = "/a-cert.pem";
    a.key_file = "/a-key.pem";

    tls_config b;
    b = a;
    EXPECT_EQ(b.cert_file, "/a-cert.pem");
    EXPECT_EQ(b.key_file, "/a-key.pem");
}

// ---------------------------------------------------------------------------
// http2_settings additional cases
// ---------------------------------------------------------------------------

TEST(Http2SettingsTest, CopyAndModify)
{
    http2_settings a;
    a.max_concurrent_streams = 42;

    http2_settings b = a;
    b.max_concurrent_streams = 84;

    EXPECT_EQ(a.max_concurrent_streams, 42u);
    EXPECT_EQ(b.max_concurrent_streams, 84u);
}

TEST(Http2SettingsTest, EnablePushDefaultIsFalse)
{
    http2_settings s;
    EXPECT_FALSE(s.enable_push);
}

// ---------------------------------------------------------------------------
// http2_server_stream additional boundary cases
// ---------------------------------------------------------------------------

TEST_F(Http2ServerStreamTest, UpdateWindowZeroIsNoop)
{
    int32_t before = stream_->window_size();
    stream_->update_window(0);
    EXPECT_EQ(stream_->window_size(), before);
}

TEST_F(Http2ServerStreamTest, UpdateWindowMultipleAccumulates)
{
    int32_t before = stream_->window_size();
    stream_->update_window(100);
    stream_->update_window(200);
    stream_->update_window(-50);
    EXPECT_EQ(stream_->window_size(), before + 100 + 200 - 50);
}

TEST_F(Http2ServerStreamTest, SendHeadersStatusCodes)
{
    // Verify a range of common status codes serialize without error.
    for (int code : {100, 200, 201, 204, 301, 302, 400, 401, 403, 404, 500, 503})
    {
        std::vector<std::vector<uint8_t>> frames;
        http2_request req;
        req.method = "GET";
        req.path = "/";
        req.scheme = "https";
        auto s = std::make_unique<http2_server_stream>(
            1u, std::move(req), encoder_,
            [&frames](const frame& f) -> VoidResult {
                frames.push_back(f.serialize());
                return ok();
            },
            16384);

        auto r = s->send_headers(code, {});
        EXPECT_TRUE(r.is_ok()) << "status=" << code;
        EXPECT_TRUE(s->headers_sent());
    }
}

TEST_F(Http2ServerStreamTest, SendHeadersWithManyHeaders)
{
    std::vector<http_header> hs;
    for (int i = 0; i < 32; ++i)
    {
        hs.push_back({"x-header-" + std::to_string(i),
                      "v-" + std::to_string(i)});
    }

    auto r = stream_->send_headers(200, hs);
    EXPECT_TRUE(r.is_ok());
    EXPECT_TRUE(stream_->headers_sent());
    EXPECT_EQ(sent_frames_.size(), 1u);
}

TEST_F(Http2ServerStreamTest, SendDataLargeSingleFrame)
{
    stream_->send_headers(200, {});
    sent_frames_.clear();

    std::vector<uint8_t> data(8192, 0x55);
    auto r = stream_->send_data(data, true);
    EXPECT_TRUE(r.is_ok());
    EXPECT_GE(sent_frames_.size(), 1u);
    EXPECT_EQ(stream_->state(), stream_state::half_closed_local);
}

TEST_F(Http2ServerStreamTest, ResetThenSendHeadersFails)
{
    stream_->reset();
    auto r = stream_->send_headers(200, {});
    EXPECT_TRUE(r.is_err());
}

TEST_F(Http2ServerStreamTest, ResetThenSendDataFails)
{
    stream_->reset();
    auto r = stream_->send_data("data");
    EXPECT_TRUE(r.is_err());
}

TEST(Http2ServerStreamConstruction, StreamIdZero)
{
    auto encoder = std::make_shared<hpack_encoder>(4096);
    http2_request req;
    req.method = "GET";
    req.path = "/";
    req.scheme = "https";

    http2_server_stream s(
        0u, std::move(req), encoder,
        [](const frame&) -> VoidResult { return ok(); });

    EXPECT_EQ(s.stream_id(), 0u);
    EXPECT_EQ(s.state(), stream_state::open);
}

TEST(Http2ServerStreamConstruction, StreamIdMaxUint32)
{
    auto encoder = std::make_shared<hpack_encoder>(4096);
    http2_request req;
    req.method = "GET";
    req.path = "/";
    req.scheme = "https";

    http2_server_stream s(
        0xFFFFFFFFu, std::move(req), encoder,
        [](const frame&) -> VoidResult { return ok(); });

    EXPECT_EQ(s.stream_id(), 0xFFFFFFFFu);
}

TEST(Http2ServerStreamConstruction, CustomMaxFrameSize)
{
    auto encoder = std::make_shared<hpack_encoder>(4096);
    http2_request req;
    req.method = "GET";
    req.path = "/";
    req.scheme = "https";

    // Min HTTP/2 max frame size is 16384.
    http2_server_stream s(
        1u, std::move(req), encoder,
        [](const frame&) -> VoidResult { return ok(); },
        32768);

    EXPECT_EQ(s.stream_id(), 1u);
    EXPECT_EQ(s.window_size(), 65535);
}

// ---------------------------------------------------------------------------
// Multi-instance state isolation
// ---------------------------------------------------------------------------

TEST(Http2ServerIsolation, TwoServersIndependentSettings)
{
    auto a = std::make_shared<http2_server>("a");
    auto b = std::make_shared<http2_server>("b");

    http2_settings sa;
    sa.max_frame_size = 32768;
    a->set_settings(sa);

    http2_settings sb;
    sb.max_frame_size = 65536;
    b->set_settings(sb);

    EXPECT_EQ(a->get_settings().max_frame_size, 32768u);
    EXPECT_EQ(b->get_settings().max_frame_size, 65536u);
}

TEST(Http2ServerIsolation, TwoServersIndependentHandlers)
{
    auto a = std::make_shared<http2_server>("a");
    auto b = std::make_shared<http2_server>("b");

    int seen_a = 0, seen_b = 0;
    a->set_request_handler(
        [&](http2_server_stream&, const http2_request&) { seen_a = 1; });
    b->set_request_handler(
        [&](http2_server_stream&, const http2_request&) { seen_b = 2; });

    EXPECT_EQ(seen_a, 0);
    EXPECT_EQ(seen_b, 0);
}

TEST(Http2ServerIsolation, IndependentRunningState)
{
    auto a = std::make_shared<http2_server>("a");
    auto b = std::make_shared<http2_server>("b");

    auto ra = a->start(18130);
    ASSERT_TRUE(ra.is_ok());

    EXPECT_TRUE(a->is_running());
    EXPECT_FALSE(b->is_running());

    auto rb = b->start(18131);
    ASSERT_TRUE(rb.is_ok());
    EXPECT_TRUE(a->is_running());
    EXPECT_TRUE(b->is_running());

    a->stop();
    EXPECT_FALSE(a->is_running());
    EXPECT_TRUE(b->is_running());

    b->stop();
    EXPECT_FALSE(b->is_running());
}

// ============================================================================
// In-process loopback tests for http2_server_connection
//
// These tests connect a real asio TCP client to the running server on
// 127.0.0.1, write raw bytes (preface, serialized frames) and observe the
// server's response. They exercise the private read/parse/dispatch paths of
// http2_server_connection without mocks.
// ============================================================================

namespace
{

constexpr std::string_view kHttp2Preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

class Http2ServerLoopbackTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server_ = std::make_shared<http2_server>("loopback-test");
        server_->set_error_handler(
            [this](const std::string& msg) {
                std::lock_guard<std::mutex> lock(errors_mutex_);
                errors_.push_back(msg);
            });
    }

    void TearDown() override
    {
        if (server_ && server_->is_running()) {
            server_->stop();
        }
    }

    // Probe a free ephemeral port via a temporary acceptor bound to port
    // 0. The probe is closed before returning, so a TOCTOU race is
    // possible; the start_server() loop retries to mitigate it. This is
    // dramatically more robust than hardcoded port offsets on shared CI
    // runners (Ubuntu / macOS GitHub runners).
    static unsigned short probe_free_port()
    {
        try {
            asio::io_context probe_io;
            asio::ip::tcp::acceptor probe(
                probe_io,
                asio::ip::tcp::endpoint(
                    asio::ip::make_address_v4("127.0.0.1"), 0));
            unsigned short port = probe.local_endpoint().port();
            std::error_code ec;
            probe.close(ec);
            return port;
        } catch (...) {
            return 0;
        }
    }

    // Start the server on a fresh ephemeral port. Retries the probe loop
    // for the rare case where the kernel reassigns the probed port to
    // another process between probe close and start().
    unsigned short start_server()
    {
        for (int attempt = 0; attempt < 10; ++attempt) {
            unsigned short port = probe_free_port();
            if (port == 0) continue;
            auto result = server_->start(port);
            if (result.is_ok()) {
                return port;
            }
        }
        ADD_FAILURE() << "Could not start loopback server on any port";
        return 0;
    }

    // Connect to the server and return an open socket.
    asio::ip::tcp::socket connect_client(
        asio::io_context& io, unsigned short port)
    {
        asio::ip::tcp::socket sock(io);
        std::error_code ec;
        // Allow a short retry window for the acceptor to be ready.
        for (int attempt = 0; attempt < 50; ++attempt) {
            sock.connect(asio::ip::tcp::endpoint(
                             asio::ip::make_address_v4("127.0.0.1"), port),
                         ec);
            if (!ec) {
                return sock;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ADD_FAILURE() << "client connect failed: " << ec.message();
        return sock;
    }

    static void write_preface(asio::ip::tcp::socket& sock)
    {
        std::error_code ec;
        asio::write(sock, asio::buffer(kHttp2Preface.data(),
                                       kHttp2Preface.size()), ec);
        ASSERT_FALSE(ec) << "preface write failed: " << ec.message();
    }

    static void write_bytes(asio::ip::tcp::socket& sock,
                            const std::vector<uint8_t>& bytes)
    {
        std::error_code ec;
        asio::write(sock, asio::buffer(bytes), ec);
        ASSERT_FALSE(ec) << "frame write failed: " << ec.message();
    }

    static void write_frame(asio::ip::tcp::socket& sock, const frame& f)
    {
        write_bytes(sock, f.serialize());
    }

    // Read at most `max_bytes` from the socket within timeout. Stops at EOF.
    static std::vector<uint8_t> read_available(
        asio::ip::tcp::socket& sock,
        std::size_t max_bytes,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(500))
    {
        std::vector<uint8_t> result;
        result.reserve(max_bytes);
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        sock.non_blocking(true);
        std::array<uint8_t, 1024> buf{};
        while (std::chrono::steady_clock::now() < deadline
               && result.size() < max_bytes) {
            std::error_code ec;
            std::size_t n = sock.read_some(asio::buffer(buf), ec);
            if (ec == asio::error::would_block || ec == asio::error::try_again) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (ec) {
                break; // EOF or other terminal error
            }
            if (n == 0) {
                break;
            }
            result.insert(result.end(), buf.begin(), buf.begin() + n);
        }
        sock.non_blocking(false);
        return result;
    }

    // Look for a frame of the given type in a raw byte stream. Returns the
    // start offset of the frame header, or std::nullopt if not found.
    static std::optional<std::size_t> find_frame(
        const std::vector<uint8_t>& bytes, frame_type type)
    {
        std::size_t off = 0;
        while (off + 9 <= bytes.size()) {
            uint32_t length = (static_cast<uint32_t>(bytes[off]) << 16) |
                              (static_cast<uint32_t>(bytes[off + 1]) << 8) |
                              static_cast<uint32_t>(bytes[off + 2]);
            auto t = static_cast<frame_type>(bytes[off + 3]);
            if (t == type) {
                return off;
            }
            off += 9 + length;
        }
        return std::nullopt;
    }

    bool wait_until_no_errors_or(std::chrono::milliseconds timeout,
                                 std::function<bool()> predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return predicate();
    }

    std::shared_ptr<http2_server> server_;
    std::mutex errors_mutex_;
    std::vector<std::string> errors_;
};

} // namespace

// ============================================================================
// HTTP2ServerPreface_* — connection preface validation
// ============================================================================

TEST_F(Http2ServerLoopbackTest, HTTP2ServerPreface_InvalidBytes)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);

    // Send 24 bytes of garbage that match preface length but not content.
    std::vector<uint8_t> bad_preface(24, 'X');
    write_bytes(sock, bad_preface);

    // Server should detect invalid preface, call error handler, and close.
    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(1000),
        [this]() {
            std::lock_guard<std::mutex> lock(errors_mutex_);
            return !errors_.empty();
        }));

    std::lock_guard<std::mutex> lock(errors_mutex_);
    bool found_invalid = false;
    for (const auto& e : errors_) {
        if (e.find("Invalid connection preface") != std::string::npos) {
            found_invalid = true;
        }
    }
    EXPECT_TRUE(found_invalid) << "expected 'Invalid connection preface' error";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerPreface_TooFewBytes)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);

    // Send only 10 bytes of the 24-byte preface, then close.
    std::vector<uint8_t> partial(kHttp2Preface.begin(),
                                 kHttp2Preface.begin() + 10);
    write_bytes(sock, partial);
    std::error_code ec;
    sock.shutdown(asio::ip::tcp::socket::shutdown_send, ec);

    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(1000),
        [this]() {
            std::lock_guard<std::mutex> lock(errors_mutex_);
            for (const auto& e : errors_) {
                if (e.find("Failed to read connection preface")
                    != std::string::npos) {
                    return true;
                }
            }
            return false;
        }));

    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerPreface_ClientCleanShutdown)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);

    // Close immediately without sending any data.
    std::error_code ec;
    sock.close(ec);

    // The server itself must remain running regardless of how the client
    // disconnects; this is a state-only assertion that does not require
    // synchronization with the connection-handling I/O thread.
    EXPECT_TRUE(server_->is_running());
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerPreface_ValidPrefaceAcceptsSettings)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);

    // Server should send its own SETTINGS frame after preface.
    auto bytes = read_available(sock, 1024,
                                std::chrono::milliseconds(1000));
    auto settings_off = find_frame(bytes, frame_type::settings);
    EXPECT_TRUE(settings_off.has_value())
        << "expected SETTINGS frame from server after valid preface";

    std::error_code ec;
    sock.close(ec);
}

// ============================================================================
// HTTP2ServerErrorPath_* — frame parse errors and protocol error responses
// ============================================================================

namespace
{

// Build an HPACK-encoded header block for a GET request.
std::vector<uint8_t> encode_get_headers(hpack_encoder& encoder,
                                        const std::string& path)
{
    std::vector<http_header> headers = {
        {":method", "GET"},
        {":scheme", "http"},
        {":path", path},
        {":authority", "localhost"}
    };
    return encoder.encode(headers);
}

} // namespace

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_HpackCompressionError)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);

    // Drain the server's initial SETTINGS frame.
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // HEADERS frame with a deliberately corrupt HPACK header block. The
    // 0x80 prefix indicates an indexed header field with an invalid index
    // (0 is reserved in HPACK), which triggers a decode error and the
    // server's COMPRESSION_ERROR GOAWAY path.
    std::vector<uint8_t> bad_hpack = {0x80, 0x80, 0x80, 0x80};
    headers_frame f(/*stream_id*/ 1, bad_hpack,
                    /*end_stream*/ true, /*end_headers*/ true);
    write_frame(sock, f);

    // Expect a GOAWAY frame from the server.
    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    auto goaway_off = find_frame(bytes, frame_type::goaway);
    EXPECT_TRUE(goaway_off.has_value())
        << "expected GOAWAY after corrupt HPACK block";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_SettingsAckSent)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Send a non-ACK SETTINGS frame exercising every setting_identifier
    // branch.
    std::vector<setting_parameter> params = {
        {static_cast<uint16_t>(setting_identifier::header_table_size), 8192},
        {static_cast<uint16_t>(setting_identifier::enable_push), 0},
        {static_cast<uint16_t>(setting_identifier::max_concurrent_streams),
         50},
        {static_cast<uint16_t>(setting_identifier::initial_window_size),
         131072},
        {static_cast<uint16_t>(setting_identifier::max_frame_size), 32768},
        {static_cast<uint16_t>(setting_identifier::max_header_list_size),
         16384},
    };
    settings_frame f(params, /*ack*/ false);
    write_frame(sock, f);

    // Server must respond with a SETTINGS ACK frame (length 0, ACK flag).
    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    bool found_ack = false;
    std::size_t off = 0;
    while (off + 9 <= bytes.size()) {
        uint32_t length = (static_cast<uint32_t>(bytes[off]) << 16) |
                          (static_cast<uint32_t>(bytes[off + 1]) << 8) |
                          static_cast<uint32_t>(bytes[off + 2]);
        auto t = static_cast<frame_type>(bytes[off + 3]);
        uint8_t flags = bytes[off + 4];
        if (t == frame_type::settings && length == 0
            && (flags & frame_flags::ack) != 0) {
            found_ack = true;
            break;
        }
        off += 9 + length;
    }
    EXPECT_TRUE(found_ack) << "expected SETTINGS ACK from server";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_SettingsAckFromClientIgnored)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Send only a SETTINGS ACK; the server's handle_settings_frame must
    // early-return without sending anything in response.
    settings_frame ack({}, /*ack*/ true);
    write_frame(sock, ack);

    // No new bytes from the server beyond the initial SETTINGS already drained.
    auto bytes = read_available(sock, 64, std::chrono::milliseconds(200));
    EXPECT_TRUE(bytes.empty())
        << "server must not respond to a SETTINGS ACK";

    EXPECT_TRUE(server_->is_running());
    {
        std::lock_guard<std::mutex> lock(errors_mutex_);
        EXPECT_TRUE(errors_.empty()) << "no errors expected for SETTINGS ACK";
    }

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_PingFrameAcked)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    std::array<uint8_t, 8> opaque = {1, 2, 3, 4, 5, 6, 7, 8};
    ping_frame ping(opaque, /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    bool found_ping_ack = false;
    std::size_t off = 0;
    while (off + 9 <= bytes.size()) {
        uint32_t length = (static_cast<uint32_t>(bytes[off]) << 16) |
                          (static_cast<uint32_t>(bytes[off + 1]) << 8) |
                          static_cast<uint32_t>(bytes[off + 2]);
        auto t = static_cast<frame_type>(bytes[off + 3]);
        uint8_t flags = bytes[off + 4];
        if (t == frame_type::ping && (flags & frame_flags::ack) != 0
            && length == 8) {
            found_ping_ack = true;
            for (std::size_t i = 0; i < 8; ++i) {
                EXPECT_EQ(bytes[off + 9 + i], opaque[i]);
            }
            break;
        }
        off += 9 + length;
    }
    EXPECT_TRUE(found_ping_ack) << "expected PING ACK from server";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_PingAckIgnored)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    ping_frame ping_ack(std::array<uint8_t, 8>{}, /*ack*/ true);
    write_frame(sock, ping_ack);

    auto bytes = read_available(sock, 64, std::chrono::milliseconds(200));
    EXPECT_FALSE(find_frame(bytes, frame_type::ping).has_value());

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_GoawayFromClientStops)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    goaway_frame g(/*last_stream_id*/ 0,
                   static_cast<uint32_t>(error_code::no_error));
    write_frame(sock, g);

    // After processing GOAWAY the server closes its connection socket. We
    // synchronize on that close: read until the socket reports EOF or no
    // more data within the deadline. This avoids a fixed sleep and is
    // resilient to scheduling jitter on macOS / Windows CI.
    (void)read_available(sock, 4096, std::chrono::milliseconds(1000));

    EXPECT_TRUE(server_->is_running());

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_UnknownFrameTypeIgnored)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // CONTINUATION (0x9) is unsupported by process_frame's switch and falls
    // into the default branch; the server must keep reading.
    std::vector<uint8_t> raw_frame = {
        0x00, 0x00, 0x00,        // length 0
        0x09,                    // type CONTINUATION
        0x04,                    // END_HEADERS
        0x00, 0x00, 0x00, 0x01   // stream_id 1
    };
    write_bytes(sock, raw_frame);

    ping_frame ping(std::array<uint8_t, 8>{0xa, 0xb, 0xc, 0xd,
                                           0xe, 0xf, 0x1, 0x2},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value())
        << "server should still process frames after an unknown type";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_ZeroLengthFrame)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Zero-length SETTINGS ACK exercises the length==0 branch where
    // read_frame_header parses the frame directly without a payload read.
    std::vector<uint8_t> raw_frame = {
        0x00, 0x00, 0x00,        // length 0
        0x04,                    // type SETTINGS
        0x01,                    // ACK flag
        0x00, 0x00, 0x00, 0x00   // stream_id 0
    };
    write_bytes(sock, raw_frame);

    ping_frame ping(std::array<uint8_t, 8>{0x11, 0x22, 0x33, 0x44,
                                           0x55, 0x66, 0x77, 0x88},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value())
        << "server must keep reading after a zero-length frame";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_RstStreamFrame)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/abc");
    headers_frame hdrs(/*stream_id*/ 3, block,
                       /*end_stream*/ false, /*end_headers*/ true);
    write_frame(sock, hdrs);

    rst_stream_frame rst(/*stream_id*/ 3,
                         static_cast<uint32_t>(error_code::cancel));
    write_frame(sock, rst);

    ping_frame ping(std::array<uint8_t, 8>{0xaa, 0xbb, 0xcc, 0xdd,
                                           0xee, 0xff, 0x00, 0x11},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value());

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_RequestHandlerException)
{
    server_->set_request_handler(
        [](http2_server_stream& /*stream*/,
           const http2_request& /*req*/) {
            throw std::runtime_error("test handler failure");
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/throws");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ true, /*end_headers*/ true);
    write_frame(sock, hdrs);

    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(1000),
        [this]() {
            std::lock_guard<std::mutex> lock(errors_mutex_);
            for (const auto& e : errors_) {
                if (e.find("Request handler exception")
                    != std::string::npos) {
                    return true;
                }
            }
            return false;
        }))
        << "expected the error handler to receive a 'Request handler "
           "exception' notification";

    // close_stream(stream_id) must run after the catch block (line 968 in
    // http2_server.cpp), so the failed request must NOT leak the stream
    // record. Wait briefly for the post-catch close_stream to take effect.
    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(500),
        [this]() { return server_->active_streams() == 0; }))
        << "stream must be closed even when the handler throws";

    std::error_code ec;
    sock.close(ec);
}

// A second fixture without an installed error_handler. Verifies the
// dangerous branch where the handler is null and the request handler
// throws: the connection's catch block must swallow silently, the stream
// must still be closed, and the server must remain alive.
class Http2ServerLoopbackNoErrorHandlerTest : public Http2ServerLoopbackTest
{
protected:
    void SetUp() override
    {
        // Construct without installing the error_handler that the parent
        // fixture provides.
        server_ = std::make_shared<http2_server>("loopback-no-eh");
    }
};

TEST_F(Http2ServerLoopbackNoErrorHandlerTest,
       HTTP2ServerErrorPath_RequestHandlerExceptionWithoutErrorHandler)
{
    server_->set_request_handler(
        [](http2_server_stream& /*stream*/,
           const http2_request& /*req*/) {
            throw std::runtime_error("silent failure");
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/silent-throw");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ true, /*end_headers*/ true);
    write_frame(sock, hdrs);

    // We have no error_handler to synchronize on, so probe the post-catch
    // close_stream side-effect via active_streams. The deadline lets the
    // throw + catch + close_stream sequence finish on the I/O thread.
    bool stream_closed = false;
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(1000);
    while (std::chrono::steady_clock::now() < deadline) {
        if (server_->active_streams() == 0) {
            stream_closed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(stream_closed)
        << "stream must close even with a null error_handler";
    EXPECT_TRUE(server_->is_running())
        << "server must survive a swallowed handler exception";

    std::error_code ec;
    sock.close(ec);
}

// ============================================================================
// HTTP2ServerStreamState_* — stream state transitions and dispatch
// ============================================================================

TEST_F(Http2ServerLoopbackTest, HTTP2ServerStreamState_HeadersEndStreamDispatches)
{
    std::atomic<bool> handler_called{false};
    std::string captured_path;
    std::string captured_method;
    std::mutex capture_mutex;

    server_->set_request_handler(
        [&](http2_server_stream& /*stream*/,
            const http2_request& req) {
            std::lock_guard<std::mutex> lock(capture_mutex);
            handler_called = true;
            captured_path = req.path;
            captured_method = req.method;
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/dispatch");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ true, /*end_headers*/ true);
    write_frame(sock, hdrs);

    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(1000),
        [&]() { return handler_called.load(); }))
        << "request_handler should be invoked once HEADERS with END_STREAM "
           "arrives";

    {
        std::lock_guard<std::mutex> lock(capture_mutex);
        EXPECT_EQ(captured_method, "GET");
        EXPECT_EQ(captured_path, "/dispatch");
    }

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerStreamState_HeadersEndHeadersWithoutEndStream)
{
    std::atomic<bool> handler_called{false};
    server_->set_request_handler(
        [&](http2_server_stream&, const http2_request&) {
            handler_called = true;
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/no-end-stream");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ false, /*end_headers*/ true);
    write_frame(sock, hdrs);

    // Synchronize on a PING-ACK round-trip after the HEADERS write: when
    // we observe the PING ACK on the wire, the HEADERS frame has already
    // passed through the server's process_frame() switch and the headers
    // path. The handler must not have been invoked because END_STREAM was
    // not set on the HEADERS frame.
    ping_frame ping(std::array<uint8_t, 8>{1, 2, 3, 4, 5, 6, 7, 8},
                    /*ack*/ false);
    write_frame(sock, ping);
    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value());

    EXPECT_FALSE(handler_called.load())
        << "handler must not run until END_STREAM is observed";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerStreamState_DataEndStreamDispatchesRequest)
{
    // Distinct signal from HTTP2ServerFlowControl_DataFrameTriggersWindowUpdate:
    // here we verify the dispatch path (handler_called + method/path/body
    // captured), not the WINDOW_UPDATE side effect. A regression in either
    // path produces a different failure than the flow-control test.
    std::atomic<bool> handler_called{false};
    std::string captured_method;
    std::string captured_path;
    std::vector<uint8_t> captured_body;
    std::mutex capture_mutex;

    server_->set_request_handler(
        [&](http2_server_stream& /*stream*/, const http2_request& req) {
            std::lock_guard<std::mutex> lock(capture_mutex);
            handler_called = true;
            captured_method = req.method;
            captured_path = req.path;
            captured_body = req.body;
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Open the stream with HEADERS only (no END_STREAM).
    hpack_encoder enc(4096);
    std::vector<http_header> req_headers = {
        {":method", "POST"},
        {":scheme", "http"},
        {":path", "/upload"},
        {":authority", "localhost"},
        {"content-type", "text/plain"}
    };
    auto block = enc.encode(req_headers);
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ false, /*end_headers*/ true);
    write_frame(sock, hdrs);

    // Body in a single DATA frame with END_STREAM.
    std::vector<uint8_t> body = {'h', 'i', '!', '\n'};
    data_frame body_frame(/*stream_id*/ 1, body,
                          /*end_stream*/ true, /*padded*/ false);
    write_frame(sock, body_frame);

    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(1000),
        [&]() { return handler_called.load(); }))
        << "DATA with END_STREAM must trigger dispatch_request";

    {
        std::lock_guard<std::mutex> lock(capture_mutex);
        EXPECT_EQ(captured_method, "POST");
        EXPECT_EQ(captured_path, "/upload");
        EXPECT_EQ(captured_body, body);
    }

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerStreamState_DataFrameOnUnknownStreamSendsRst)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Send a DATA frame on a stream id that has never been opened.
    data_frame df(/*stream_id*/ 7, std::vector<uint8_t>{1, 2, 3, 4},
                  /*end_stream*/ false, /*padded*/ false);
    write_frame(sock, df);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    auto rst_off = find_frame(bytes, frame_type::rst_stream);
    EXPECT_TRUE(rst_off.has_value())
        << "expected RST_STREAM after DATA on unknown stream";

    if (rst_off) {
        std::size_t off = *rst_off;
        // stream id at offset+5..+8
        uint32_t sid = (static_cast<uint32_t>(bytes[off + 5]) << 24)
                       | (static_cast<uint32_t>(bytes[off + 6]) << 16)
                       | (static_cast<uint32_t>(bytes[off + 7]) << 8)
                       | static_cast<uint32_t>(bytes[off + 8]);
        sid &= 0x7FFFFFFFu;
        EXPECT_EQ(sid, 7u);
    }

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerStreamState_DispatchWithoutHandler)
{
    // No request handler set. dispatch_request must early-return when the
    // server receives a complete HEADERS frame.
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/no-handler");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ true, /*end_headers*/ true);
    write_frame(sock, hdrs);

    // Verify the connection survives by following up with a PING.
    ping_frame ping(std::array<uint8_t, 8>{9, 8, 7, 6, 5, 4, 3, 2},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value())
        << "server must remain healthy when dispatch has no handler";

    std::error_code ec;
    sock.close(ec);
}

// ============================================================================
// HTTP2ServerFlowControl_* — WINDOW_UPDATE handling
// ============================================================================

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerFlowControl_WindowUpdateConnection)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Connection-level WINDOW_UPDATE (stream_id 0). Must be accepted
    // silently; we verify with a follow-up PING ACK.
    window_update_frame wu(/*stream_id*/ 0, /*increment*/ 4096);
    write_frame(sock, wu);

    ping_frame ping(std::array<uint8_t, 8>{0, 1, 2, 3, 4, 5, 6, 7},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(500));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value());
    {
        std::lock_guard<std::mutex> lock(errors_mutex_);
        EXPECT_TRUE(errors_.empty());
    }

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerFlowControl_WindowUpdateStream)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/wu");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ false, /*end_headers*/ true);
    write_frame(sock, hdrs);

    // Stream-level WINDOW_UPDATE on the open stream.
    window_update_frame wu(/*stream_id*/ 1, /*increment*/ 8192);
    write_frame(sock, wu);

    ping_frame ping(std::array<uint8_t, 8>{1, 1, 1, 1, 1, 1, 1, 1},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(500));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value());

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerFlowControl_WindowUpdateUnknownStream)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // WINDOW_UPDATE for a stream that has never been opened should be
    // ignored (no map lookup hit), and the connection must remain alive.
    window_update_frame wu(/*stream_id*/ 99, /*increment*/ 4096);
    write_frame(sock, wu);

    ping_frame ping(std::array<uint8_t, 8>{2, 2, 2, 2, 2, 2, 2, 2},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(500));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value());

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerFlowControl_DataFrameTriggersWindowUpdate)
{
    // Distinct signal from HTTP2ServerStreamState_DataEndStreamDispatchesRequest:
    // here END_STREAM is intentionally NOT set, so dispatch must NOT fire.
    // The assertion targets the WINDOW_UPDATE emission counts, not the
    // request-handler invocation. A regression that breaks dispatch will
    // surface in the other test; a regression that breaks WINDOW_UPDATE
    // emission surfaces here.
    std::atomic<bool> dispatched{false};
    server_->set_request_handler(
        [&](http2_server_stream&, const http2_request&) {
            dispatched = true;
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Open the stream, then send DATA without END_STREAM so the body is
    // accumulated and the server emits WINDOW_UPDATE frames.
    hpack_encoder enc(4096);
    std::vector<http_header> req_headers = {
        {":method", "POST"},
        {":scheme", "http"},
        {":path", "/wu-data"},
        {":authority", "localhost"}
    };
    auto block = enc.encode(req_headers);
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ false, /*end_headers*/ true);
    write_frame(sock, hdrs);

    std::vector<uint8_t> chunk(64, 'x');
    data_frame df(/*stream_id*/ 1, chunk,
                  /*end_stream*/ false, /*padded*/ false);
    write_frame(sock, df);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(800));

    // Expect at least two WINDOW_UPDATE frames on the wire: one for stream
    // 0 (connection) and one for stream 1.
    int connection_wu_count = 0;
    int stream_wu_count = 0;
    std::size_t off = 0;
    while (off + 9 <= bytes.size()) {
        uint32_t length = (static_cast<uint32_t>(bytes[off]) << 16) |
                          (static_cast<uint32_t>(bytes[off + 1]) << 8) |
                          static_cast<uint32_t>(bytes[off + 2]);
        auto t = static_cast<frame_type>(bytes[off + 3]);
        uint32_t sid = (static_cast<uint32_t>(bytes[off + 5]) << 24) |
                       (static_cast<uint32_t>(bytes[off + 6]) << 16) |
                       (static_cast<uint32_t>(bytes[off + 7]) << 8) |
                       static_cast<uint32_t>(bytes[off + 8]);
        sid &= 0x7FFFFFFFu;
        if (t == frame_type::window_update) {
            if (sid == 0) ++connection_wu_count;
            else if (sid == 1) ++stream_wu_count;
        }
        off += 9 + length;
    }

    EXPECT_GE(connection_wu_count, 1)
        << "expected at least one connection-level WINDOW_UPDATE";
    EXPECT_GE(stream_wu_count, 1)
        << "expected at least one stream-level WINDOW_UPDATE";

    EXPECT_FALSE(dispatched.load())
        << "without END_STREAM the request handler must not run yet";

    std::error_code ec;
    sock.close(ec);
}

// ============================================================================
// Additional gap-closure tests requested in the reviewer checklist
// ============================================================================

// A separate fixture that does NOT install an error_handler. Used to confirm
// the http2_server_connection's `if (error_handler_)` guard does not segfault
// when the handler is unset and a failure occurs.
class Http2ServerLoopbackNoHandlerTest : public Http2ServerLoopbackTest
{
protected:
    void SetUp() override
    {
        // Construct the server but skip installing the error_handler that
        // the parent fixture provides. This exercises the null-handler
        // guard branches inside read_connection_preface.
        server_ = std::make_shared<http2_server>("loopback-no-handler");
    }
};

TEST_F(Http2ServerLoopbackNoHandlerTest, HTTP2ServerPreface_NullErrorHandler)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);

    // 24 garbage bytes trigger the "Invalid connection preface" branch.
    // With no error_handler installed, the server must not crash.
    std::vector<uint8_t> bad_preface(24, 'Z');
    write_bytes(sock, bad_preface);

    // Drain until the server closes the socket (deadline-bounded; no sleep).
    (void)read_available(sock, 1024, std::chrono::milliseconds(500));

    EXPECT_TRUE(server_->is_running())
        << "server must remain alive when error_handler is null";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest, HTTP2ServerErrorPath_MalformedFramePayload)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Build a SETTINGS frame whose declared length (5) is not a multiple of
    // the 6-byte setting parameter size, so frame::parse rejects the
    // payload and read_frame_payload's "Failed to parse frame" branch
    // executes.
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x05,        // length = 5 (invalid for SETTINGS)
        0x04,                    // type SETTINGS
        0x00,                    // no flags (not ACK)
        0x00, 0x00, 0x00, 0x00,  // stream_id 0
        0x00, 0x01,              // partial setting id
        0x00, 0x00, 0x10         // partial value (only 3 of 4 bytes)
    };
    write_bytes(sock, raw);

    EXPECT_TRUE(wait_until_no_errors_or(
        std::chrono::milliseconds(1000),
        [this]() {
            std::lock_guard<std::mutex> lock(errors_mutex_);
            for (const auto& e : errors_) {
                if (e.find("Failed to parse frame") != std::string::npos) {
                    return true;
                }
            }
            return false;
        }))
        << "expected the 'Failed to parse frame' error path to fire";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerErrorPath_UnknownSettingIdentifierIgnored)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // SETTINGS frame containing identifiers outside the enum's named
    // values. handle_settings_frame's switch has no default branch, so
    // these must fall through silently and the server must still ACK.
    std::vector<setting_parameter> params = {
        {/*identifier*/ 0x42, /*value*/ 1},
        {/*identifier*/ 0x99, /*value*/ 2},
    };
    settings_frame f(params, /*ack*/ false);
    write_frame(sock, f);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));

    bool found_ack = false;
    std::size_t off = 0;
    while (off + 9 <= bytes.size()) {
        uint32_t length = (static_cast<uint32_t>(bytes[off]) << 16) |
                          (static_cast<uint32_t>(bytes[off + 1]) << 8) |
                          static_cast<uint32_t>(bytes[off + 2]);
        auto t = static_cast<frame_type>(bytes[off + 3]);
        uint8_t flags = bytes[off + 4];
        if (t == frame_type::settings && length == 0
            && (flags & frame_flags::ack) != 0) {
            found_ack = true;
            break;
        }
        off += 9 + length;
    }
    EXPECT_TRUE(found_ack)
        << "server must still ACK SETTINGS containing unknown identifiers";

    {
        std::lock_guard<std::mutex> lock(errors_mutex_);
        EXPECT_TRUE(errors_.empty())
            << "unknown identifiers must not produce errors";
    }

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerStreamState_HeadersWithBothFlagsFalse)
{
    std::atomic<bool> handler_called{false};
    server_->set_request_handler(
        [&](http2_server_stream&, const http2_request&) {
            handler_called = true;
        });

    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // HEADERS with neither END_STREAM nor END_HEADERS. The handle_headers_frame
    // logic enters neither branch (no dispatch, no headers_complete=true),
    // and the connection must remain alive. We verify with a PING-ACK.
    //
    // The headers_frame constructor only exposes end_stream and end_headers;
    // to clear END_HEADERS we have to build the raw frame bytes by hand.
    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/no-flags");
    std::vector<uint8_t> raw;
    raw.resize(9 + block.size());
    // length (24 bits)
    raw[0] = static_cast<uint8_t>((block.size() >> 16) & 0xFF);
    raw[1] = static_cast<uint8_t>((block.size() >> 8) & 0xFF);
    raw[2] = static_cast<uint8_t>(block.size() & 0xFF);
    raw[3] = static_cast<uint8_t>(frame_type::headers);  // type
    raw[4] = 0x00;                                       // no flags
    raw[5] = 0x00;
    raw[6] = 0x00;
    raw[7] = 0x00;
    raw[8] = 0x01;                                       // stream id 1
    std::copy(block.begin(), block.end(), raw.begin() + 9);
    write_bytes(sock, raw);

    ping_frame ping(std::array<uint8_t, 8>{0xA5, 0x5A, 0xA5, 0x5A,
                                           0xA5, 0x5A, 0xA5, 0x5A},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value())
        << "server must keep reading after HEADERS with no flags set";

    EXPECT_FALSE(handler_called.load())
        << "handler must not run when neither END_STREAM nor END_HEADERS "
           "is set";

    std::error_code ec;
    sock.close(ec);
}

TEST_F(Http2ServerLoopbackTest,
       HTTP2ServerFlowControl_DataFrameEmptyPayloadNoWindowUpdate)
{
    auto port = start_server();
    ASSERT_NE(port, 0);

    asio::io_context io;
    auto sock = connect_client(io, port);
    write_preface(sock);
    (void)read_available(sock, 1024, std::chrono::milliseconds(300));

    // Open a stream first.
    hpack_encoder enc(4096);
    auto block = encode_get_headers(enc, "/empty-data");
    headers_frame hdrs(/*stream_id*/ 1, block,
                       /*end_stream*/ false, /*end_headers*/ true);
    write_frame(sock, hdrs);

    // Empty DATA frame (no END_STREAM): the data_size > 0 guard suppresses
    // WINDOW_UPDATE emission, and dispatch is not triggered.
    data_frame df(/*stream_id*/ 1, std::vector<uint8_t>{},
                  /*end_stream*/ false, /*padded*/ false);
    write_frame(sock, df);

    // Synchronize via a PING-ACK so we know the empty DATA has been
    // processed.
    ping_frame ping(std::array<uint8_t, 8>{0x10, 0x20, 0x30, 0x40,
                                           0x50, 0x60, 0x70, 0x80},
                    /*ack*/ false);
    write_frame(sock, ping);

    auto bytes = read_available(sock, 4096, std::chrono::milliseconds(1000));
    EXPECT_TRUE(find_frame(bytes, frame_type::ping).has_value());

    // No WINDOW_UPDATE frames should have been emitted for the empty DATA.
    EXPECT_FALSE(find_frame(bytes, frame_type::window_update).has_value())
        << "server must not emit WINDOW_UPDATE for an empty DATA frame";

    std::error_code ec;
    sock.close(ec);
}

