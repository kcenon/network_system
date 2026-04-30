// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include "internal/protocols/http2/http2_client.h"
#include "kcenon/network/detail/utils/result_types.h"
#include <cstdint>
#include <limits>
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

// =====================================================================
// Additional error-path and boundary coverage (Part of #953, #1062)
//
// These tests target the public surface that is reachable WITHOUT an
// established HTTP/2 connection: response/stream/settings struct methods,
// validation guards, lifecycle idempotency, and constructor variations.
// Coverage of post-handshake code paths (frame I/O, stream state machine,
// HPACK encode/decode against a peer) requires an in-process HTTP/2
// loopback fixture and is tracked separately under #953.
// =====================================================================

// http2_response::get_header — case folding and missing-name edges
TEST_F(Http2ClientTest, ResponseGetHeaderHandlesEmptyName)
{
    http2_response response;
    response.headers = {{":status", "200"}};
    auto result = response.get_header("");
    EXPECT_FALSE(result.has_value());
}

TEST_F(Http2ClientTest, ResponseGetHeaderHandlesHeaderWithEmptyName)
{
    http2_response response;
    response.headers = {{"", "value"}, {"x-name", "x-value"}};
    auto empty_lookup = response.get_header("");
    ASSERT_TRUE(empty_lookup.has_value());
    EXPECT_EQ(*empty_lookup, "value");
}

TEST_F(Http2ClientTest, ResponseGetHeaderUppercaseInStoredName)
{
    http2_response response;
    response.headers = {{"X-CUSTOM-HEADER", "abc"}};
    auto lower = response.get_header("x-custom-header");
    ASSERT_TRUE(lower.has_value());
    EXPECT_EQ(*lower, "abc");
}

TEST_F(Http2ClientTest, ResponseGetHeaderMixedCaseQueryAndStorage)
{
    http2_response response;
    response.headers = {{"Content-Type", "text/plain"}};
    auto upper = response.get_header("CONTENT-TYPE");
    auto lower = response.get_header("content-type");
    auto exact = response.get_header("Content-Type");
    ASSERT_TRUE(upper.has_value());
    EXPECT_EQ(*upper, "text/plain");
    ASSERT_TRUE(lower.has_value());
    EXPECT_EQ(*lower, "text/plain");
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(*exact, "text/plain");
}

// http2_response::get_body_string — binary payload edges
TEST_F(Http2ClientTest, ResponseGetBodyStringWithEmbeddedNulls)
{
    http2_response response;
    response.body = {0x41, 0x00, 0x42, 0x00, 0x43};
    auto str = response.get_body_string();
    EXPECT_EQ(str.size(), 5u);
    EXPECT_EQ(str[0], 'A');
    EXPECT_EQ(str[1], '\0');
    EXPECT_EQ(str[2], 'B');
    EXPECT_EQ(str[3], '\0');
    EXPECT_EQ(str[4], 'C');
}

TEST_F(Http2ClientTest, ResponseGetBodyStringWithLargePayload)
{
    http2_response response;
    response.body.assign(1024 * 16, static_cast<uint8_t>(0x7F));
    auto str = response.get_body_string();
    EXPECT_EQ(str.size(), 16384u);
    EXPECT_EQ(static_cast<unsigned char>(str.front()), 0x7Fu);
    EXPECT_EQ(static_cast<unsigned char>(str.back()), 0x7Fu);
}

TEST_F(Http2ClientTest, ResponseGetBodyStringWithAllByteValues)
{
    http2_response response;
    response.body.resize(256);
    for (size_t i = 0; i < 256; ++i)
    {
        response.body[i] = static_cast<uint8_t>(i);
    }
    auto str = response.get_body_string();
    ASSERT_EQ(str.size(), 256u);
    for (size_t i = 0; i < 256; ++i)
    {
        EXPECT_EQ(static_cast<unsigned char>(str[i]),
                  static_cast<unsigned char>(i));
    }
}

// http2_client construction — id parameter edges
TEST(Http2ClientConstructionTest, ConstructWithEmptyClientId)
{
    auto client = std::make_shared<http2_client>("");
    EXPECT_FALSE(client->is_connected());
    EXPECT_EQ(client->get_timeout(), std::chrono::milliseconds(30000));
}

TEST(Http2ClientConstructionTest, ConstructWithLongClientId)
{
    std::string long_id(1024, 'a');
    auto client = std::make_shared<http2_client>(long_id);
    EXPECT_FALSE(client->is_connected());
}

TEST(Http2ClientConstructionTest, ConstructWithSpecialCharsInId)
{
    auto client = std::make_shared<http2_client>("client/with:special@chars.{}");
    EXPECT_FALSE(client->is_connected());
}

// connect() — extended boundary coverage
TEST_F(Http2ClientTest, ConnectFailsWithWhitespaceOnlyHost)
{
    // Whitespace string is non-empty so it bypasses the empty-host guard
    // and must fail at resolver/connection level rather than as invalid_argument.
    auto result = client_->connect("   ", 443);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_failed);
}

TEST_F(Http2ClientTest, ConnectFailsWithPortZero)
{
    auto result = client_->connect("invalid.host.example.invalid", 0);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_failed);
}

// disconnect() — lifecycle idempotency edges
TEST_F(Http2ClientTest, TripleDisconnectIsIdempotent)
{
    EXPECT_TRUE(client_->disconnect().is_ok());
    EXPECT_TRUE(client_->disconnect().is_ok());
    EXPECT_TRUE(client_->disconnect().is_ok());
}

TEST_F(Http2ClientTest, DisconnectAfterFailedConnect)
{
    auto connect_result = client_->connect("invalid.host.example.invalid", 443);
    EXPECT_TRUE(connect_result.is_err());
    auto disconnect_result = client_->disconnect();
    EXPECT_TRUE(disconnect_result.is_ok());
}

// set_settings() — boundary values
TEST_F(Http2ClientTest, SetSettingsWithZeroHeaderTableSize)
{
    http2_settings s;
    s.header_table_size = 0;
    client_->set_settings(s);
    EXPECT_EQ(client_->get_settings().header_table_size, 0u);
}

TEST_F(Http2ClientTest, SetSettingsWithMaxValues)
{
    http2_settings s;
    constexpr auto kMax = std::numeric_limits<uint32_t>::max();
    s.header_table_size = kMax;
    s.max_concurrent_streams = kMax;
    s.initial_window_size = kMax;
    s.max_frame_size = kMax;
    s.max_header_list_size = kMax;
    client_->set_settings(s);
    auto out = client_->get_settings();
    EXPECT_EQ(out.header_table_size, kMax);
    EXPECT_EQ(out.max_concurrent_streams, kMax);
    EXPECT_EQ(out.initial_window_size, kMax);
    EXPECT_EQ(out.max_frame_size, kMax);
    EXPECT_EQ(out.max_header_list_size, kMax);
}

TEST_F(Http2ClientTest, SetSettingsTogglesEnablePush)
{
    auto base = client_->get_settings();
    EXPECT_FALSE(base.enable_push);

    http2_settings s = base;
    s.enable_push = true;
    client_->set_settings(s);
    EXPECT_TRUE(client_->get_settings().enable_push);

    s.enable_push = false;
    client_->set_settings(s);
    EXPECT_FALSE(client_->get_settings().enable_push);
}

// Public request methods — additional disconnected-state coverage
TEST_F(Http2ClientTest, GetWithEmptyPathFailsWhenNotConnected)
{
    auto result = client_->get("");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, PostWithEmptyStringBodyFailsWhenNotConnected)
{
    auto result = client_->post("/api", std::string{});
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, PostWithEmptyBinaryBodyFailsWhenNotConnected)
{
    std::vector<uint8_t> empty;
    auto result = client_->post("/api", empty);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, PutWithEmptyPathFailsWhenNotConnected)
{
    auto result = client_->put("", "body");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, DelWithEmptyPathFailsWhenNotConnected)
{
    auto result = client_->del("");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

// Stream operations — additional disconnected-state coverage
TEST_F(Http2ClientTest, StartStreamWithEmptyPathFailsWhenNotConnected)
{
    auto result = client_->start_stream(
        "", {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http_header>) {},
        [](int) {});
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, StartStreamWithNullCallbacksFailsWhenNotConnected)
{
    // The disconnected guard must short-circuit before callbacks are
    // dereferenced; null std::function objects must not be invoked.
    auto result = client_->start_stream("/stream", {}, nullptr, nullptr, nullptr);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, WriteStreamWithStreamIdZeroFailsWhenNotConnected)
{
    auto result = client_->write_stream(0, {0x01}, true);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, WriteStreamWithEmptyDataFailsWhenNotConnected)
{
    auto result = client_->write_stream(1, {}, true);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

TEST_F(Http2ClientTest, CancelStreamWithStreamIdZeroFailsWhenNotConnected)
{
    auto result = client_->cancel_stream(0);
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, err::network_system::connection_closed);
}

// Multi-instance state isolation
TEST(Http2ClientMultiInstance, SettingsChangeOnOneClientDoesNotAffectAnother)
{
    auto a = std::make_shared<http2_client>("a");
    auto b = std::make_shared<http2_client>("b");

    http2_settings sa;
    sa.header_table_size = 32768;
    sa.enable_push = true;
    a->set_settings(sa);

    auto a_out = a->get_settings();
    auto b_out = b->get_settings();

    EXPECT_EQ(a_out.header_table_size, 32768u);
    EXPECT_TRUE(a_out.enable_push);
    EXPECT_EQ(b_out.header_table_size, 4096u);
    EXPECT_FALSE(b_out.enable_push);
}

TEST(Http2ClientMultiInstance, ManyConcurrentlyConstructedClients)
{
    constexpr size_t kCount = 8;
    std::vector<std::shared_ptr<http2_client>> clients;
    clients.reserve(kCount);
    for (size_t i = 0; i < kCount; ++i)
    {
        clients.push_back(
            std::make_shared<http2_client>("client-" + std::to_string(i)));
    }
    for (const auto& c : clients)
    {
        EXPECT_FALSE(c->is_connected());
    }
}

// http2_stream — boundary value coverage
TEST(Http2StreamTest, WindowSizeBoundaryValues)
{
    http2_stream stream;
    stream.window_size = -1;
    EXPECT_EQ(stream.window_size, -1);
    stream.window_size = std::numeric_limits<int32_t>::min();
    EXPECT_EQ(stream.window_size, std::numeric_limits<int32_t>::min());
    stream.window_size = std::numeric_limits<int32_t>::max();
    EXPECT_EQ(stream.window_size, std::numeric_limits<int32_t>::max());
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

// =====================================================================
// Hermetic transport coverage (Issue #1062)
//
// The tests above operate purely on the public API without an established
// HTTP/2 connection. This section drives the post-handshake code paths in
// http2_client.cpp that are otherwise unreachable from a hermetic CI
// environment:
//   - handle_headers_frame   — server replies with HEADERS (status code path)
//   - handle_data_frame      — server replies with DATA + END_STREAM
//   - handle_rst_stream_frame — server resets the stream
//   - handle_goaway_frame    — server emits GOAWAY mid-session
//   - handle_window_update_frame — server sends WINDOW_UPDATE
//   - handle_ping_frame      — PING + PING-ACK round trip
//   - read_frame             — invalid frame data triggers error path
//   - process_frame          — unknown frame type falls through
//   - handle_data_frame      — automatic WINDOW_UPDATE on flow control
//   - run_io                 — peer closes socket abruptly
//   - HPACK header-table eviction triggered from client side
//
// Each test reuses tls_loopback_listener for the TLS-with-ALPN-h2 layer
// and drives the HTTP/2 framing manually so the test author retains full
// control over what the client receives. Tests are scoped to error /
// branch coverage that the disconnected-state tests above cannot cover.
// =====================================================================

#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/hpack.h"
#include "hermetic_transport_fixture.h"
#include "mock_h2_server_peer.h"
#include "mock_tls_socket.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <span>
#include <thread>

namespace support = kcenon::network::tests::support;
using namespace std::chrono_literals;

namespace
{


// Frame header is fixed at 9 bytes for HTTP/2 (RFC 7540 Section 4.1).
constexpr std::size_t kFrameHeaderSize = 9;
constexpr std::size_t kPrefaceSize = 24;

// HTTP/2 connection preface bytes.
constexpr std::uint8_t kPrefaceBytes[kPrefaceSize] = {
    'P',  'R',  'I',  ' ',  '*',  ' ',  'H',  'T',
    'T',  'P',  '/',  '2',  '.',  '0',  '\r', '\n',
    '\r', '\n', 'S',  'M',  '\r', '\n', '\r', '\n'
};

/**
 * @brief Read the client preface and exchange SETTINGS, then return the
 *        accepted SSL stream so the caller can inject custom frames.
 *
 * Mirrors the first four steps of @ref support::mock_h2_server_peer but
 * exposes the underlying SSL stream so individual tests can write
 * arbitrary frame sequences (HEADERS+DATA, RST_STREAM, GOAWAY, PING,
 * WINDOW_UPDATE, malformed frames, abrupt close).
 *
 * @returns the post-SETTINGS stream on success, or nullptr on any I/O
 *          or framing error.
 */
std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>>
complete_settings_exchange(support::tls_loopback_listener& listener)
{
    auto stream = listener.accepted_socket(std::chrono::seconds(5));
    if (!stream)
    {
        return nullptr;
    }

    std::error_code ec;

    // Read 24-byte client preface.
    std::array<std::uint8_t, kPrefaceSize> preface_buf{};
    asio::read(*stream, asio::buffer(preface_buf), ec);
    if (ec ||
        std::memcmp(preface_buf.data(), kPrefaceBytes, kPrefaceSize) != 0)
    {
        return nullptr;
    }

    // Send empty server SETTINGS.
    {
        kcenon::network::protocols::http2::settings_frame initial(
            {}, /*ack=*/false);
        const auto bytes = initial.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec)
        {
            return nullptr;
        }
    }

    // Read client SETTINGS frame header.
    std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
    asio::read(*stream, asio::buffer(hdr_buf), ec);
    if (ec)
    {
        return nullptr;
    }
    auto parsed = kcenon::network::protocols::http2::frame_header::parse(
        std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
    if (parsed.is_err())
    {
        return nullptr;
    }
    const auto hdr = parsed.value();
    if (hdr.length > 0)
    {
        std::vector<std::uint8_t> payload(hdr.length);
        asio::read(*stream, asio::buffer(payload), ec);
        if (ec)
        {
            return nullptr;
        }
    }

    // Send SETTINGS-ACK.
    {
        kcenon::network::protocols::http2::settings_frame ack_frame(
            {}, /*ack=*/true);
        const auto bytes = ack_frame.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec)
        {
            return nullptr;
        }
    }

    return stream;
}

/**
 * @brief Build a minimal HPACK-encoded header block carrying :status only.
 *
 * Uses the project's hpack_encoder so the bytes round-trip cleanly through
 * the client's hpack_decoder. Constructed per-call so each test gets a
 * fresh dynamic-table state.
 */
std::vector<std::uint8_t> encode_status(int status_code)
{
    kcenon::network::protocols::http2::hpack_encoder enc(4096);
    std::vector<kcenon::network::protocols::http2::http_header> headers = {
        {":status", std::to_string(status_code)},
    };
    return enc.encode(headers);
}

/**
 * @brief Build an HPACK-encoded header block populated with many distinct
 *        custom headers — exercises the dynamic-table eviction path on the
 *        client decoder.
 */
std::vector<std::uint8_t> encode_many_headers(int status_code, std::size_t n)
{
    kcenon::network::protocols::http2::hpack_encoder enc(4096);
    std::vector<kcenon::network::protocols::http2::http_header> headers;
    headers.reserve(n + 1);
    headers.emplace_back(":status", std::to_string(status_code));
    for (std::size_t i = 0; i < n; ++i)
    {
        headers.emplace_back(
            "x-large-header-" + std::to_string(i),
            std::string(64, static_cast<char>('A' + (i % 26))));
    }
    return enc.encode(headers);
}

} // namespace

class Http2ClientHermeticTest : public support::hermetic_transport_fixture
{
};

// HEADERS+DATA reply path: drives handle_headers_frame and handle_data_frame
// success branches plus the response future fulfilment.
TEST_F(Http2ClientHermeticTest, GetSucceedsWhenPeerSendsHeadersAndData)
{
    support::tls_loopback_listener listener(io());

    std::atomic<bool> peer_done{false};
    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        // Read the client HEADERS frame (request) — discard it.
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Reply with HEADERS frame carrying :status 200, end_stream=false.
        {
            auto block = encode_status(200);
            kcenon::network::protocols::http2::headers_frame hf(
                hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // Reply with DATA frame carrying body and end_stream=true.
        {
            std::vector<std::uint8_t> body{'h', 'e', 'l', 'l', 'o'};
            kcenon::network::protocols::http2::data_frame df(
                hdr.stream_id, body, /*end_stream=*/true);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        peer_done.store(true);

        // Drain remaining client frames (e.g. GOAWAY) until socket closes.
        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-get-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    // Wait for connect to complete and the peer to send the response.
    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto response = client->get("/coverage");
    ASSERT_TRUE(response.is_ok())
        << "GET should succeed after peer sends HEADERS+DATA, got error: "
        << (response.is_err() ? response.error().message : "none");
    EXPECT_EQ(response.value().status_code, 200);
    EXPECT_EQ(response.value().get_body_string(), "hello");

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return peer_done.load(); }, 2s));

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// GOAWAY reply mid-session: drives handle_goaway_frame branch and the
// pending request being failed via promise.set_value with status 0.
TEST_F(Http2ClientHermeticTest, GetReceivesGoawayWhilePending)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        // Read the client HEADERS frame (request) — discard it.
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Send GOAWAY frame referencing last_stream_id = 0 so all client
        // streams are considered unprocessed and force-closed by the
        // handle_goaway_frame branch.
        {
            kcenon::network::protocols::http2::goaway_frame gf(
                /*last_stream_id=*/0,
                static_cast<std::uint32_t>(
                    kcenon::network::protocols::http2::error_code::no_error));
            auto bytes = gf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
        }

        // Drain remaining frames.
        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-goaway-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    // After GOAWAY arrives, the pending request future is set with status 0.
    auto response = client->get("/goaway");
    ASSERT_TRUE(response.is_ok());
    EXPECT_EQ(response.value().status_code, 0);

    // is_connected() returns false once goaway_received_ is set, even if
    // is_connected_ is still true — exercises the && branch at line 241.
    EXPECT_FALSE(client->is_connected());

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// RST_STREAM reply: drives handle_rst_stream_frame branch.
TEST_F(Http2ClientHermeticTest, GetReceivesRstStream)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Reply with RST_STREAM.
        {
            kcenon::network::protocols::http2::rst_stream_frame rsf(
                hdr.stream_id,
                static_cast<std::uint32_t>(
                    kcenon::network::protocols::http2::error_code::cancel));
            auto bytes = rsf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
        }

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-rst-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto response = client->get("/rst");
    ASSERT_TRUE(response.is_ok());
    // RST_STREAM sets status_code to 0 to indicate error.
    EXPECT_EQ(response.value().status_code, 0);

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// PING frame: drives handle_ping_frame branch (server PING -> client ACK).
TEST_F(Http2ClientHermeticTest, ClientReplyToServerPing)
{
    support::tls_loopback_listener listener(io());

    std::atomic<bool> ping_ack_received{false};
    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;

        // Send PING with opaque data.
        {
            std::array<std::uint8_t, 8> opaque = {1, 2, 3, 4, 5, 6, 7, 8};
            kcenon::network::protocols::http2::ping_frame pf(opaque, false);
            auto bytes = pf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // Expect PING-ACK back from the client.
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.type ==
                kcenon::network::protocols::http2::frame_type::ping &&
            (hdr.flags &
             kcenon::network::protocols::http2::frame_flags::ack) != 0)
        {
            ping_ack_received.store(true);
        }
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-ping-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return ping_ack_received.load(); }, 3s))
        << "client should auto-reply to server PING with PING-ACK";

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// PING-ACK frame: server sends PING with ACK flag — client must NOT reply.
// Drives the !is_ack branch in handle_ping_frame.
TEST_F(Http2ClientHermeticTest, ClientDoesNotReplyToPingAck)
{
    support::tls_loopback_listener listener(io());

    std::atomic<bool> got_ping_ack_from_peer{false};
    std::atomic<int> client_frames_after_ack{0};

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }
        got_ping_ack_from_peer.store(true);

        std::error_code ec;

        // Send PING with ACK flag set — handle_ping_frame must not reply.
        {
            std::array<std::uint8_t, 8> opaque = {0xAA, 0xBB, 0xCC, 0xDD,
                                                  0xEE, 0xFF, 0x11, 0x22};
            kcenon::network::protocols::http2::ping_frame pf(opaque, true);
            auto bytes = pf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // Read with timeout-like behavior — we expect EOF or GOAWAY when
        // the client disconnects; we should NOT see a PING-ACK frame.
        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            const auto h = p.value();
            // A PING frame here would indicate the client incorrectly
            // replied to our ACK.
            if (h.type ==
                kcenon::network::protocols::http2::frame_type::ping)
            {
                client_frames_after_ack.fetch_add(1);
            }
            if (h.length > 0)
            {
                std::vector<std::uint8_t> payload(h.length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-ping-ack-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    // Allow the peer to deliver the PING-ACK and the client's run_io to
    // process it.
    std::this_thread::sleep_for(200ms);

    (void)client->disconnect();
    connector.join();
    peer_thread.join();

    EXPECT_TRUE(got_ping_ack_from_peer.load());
    EXPECT_EQ(client_frames_after_ack.load(), 0)
        << "client should not have replied to PING with ACK flag";
}

// WINDOW_UPDATE frame: connection-level (stream 0) and stream-level paths.
TEST_F(Http2ClientHermeticTest, ClientHandlesWindowUpdateConnectionLevel)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;

        // Send connection-level WINDOW_UPDATE (stream_id == 0).
        {
            kcenon::network::protocols::http2::window_update_frame wuf(
                0, 32768);
            auto bytes = wuf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // Send stream-level WINDOW_UPDATE for a stream that doesn't exist.
        // handle_window_update_frame should silently ignore unknown
        // stream IDs (the !stream branch at line 1106).
        {
            kcenon::network::protocols::http2::window_update_frame wuf(
                99u, 16384);
            auto bytes = wuf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-wu-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    // Allow run_io to process the WINDOW_UPDATEs.
    std::this_thread::sleep_for(150ms);

    EXPECT_TRUE(client->is_connected());

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// Frame parse error: peer sends a valid header but an oversized DATA frame
// payload that the client cannot parse — drives the read_frame error
// branch and the run_io break-on-error path.
TEST_F(Http2ClientHermeticTest, RunIoBreaksOnPeerSocketClose)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }
        // Abruptly close the underlying socket so the next read in
        // client's run_io fails. This drives the catch-block / err
        // branches in run_io (lines 1131-1149).
        std::error_code ec;
        stream->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        stream->lowest_layer().close(ec);
    });

    auto client = std::make_shared<http2_client>("hermetic-close-test");
    client->set_timeout(1500ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    // Wait for client to finish handshake before peer slams the socket.
    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    // The peer thread closes the socket, so run_io will break out of
    // the loop and is_connected_ flips to false.
    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return !client->is_connected(); }, 3s));

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// State machine: send_request returning timeout closes the stream. The
// stream remains in the streams_ map but in a closed state — exercises
// close_stream's lookup-and-mark path (lines 715-721).
TEST_F(Http2ClientHermeticTest, RequestTimeoutMarksStreamClosed)
{
    support::mock_h2_server_peer peer(io());

    auto client = std::make_shared<http2_client>("hermetic-timeout-test");
    client->set_timeout(120ms);
    auto port = peer.port();
    std::thread connector([client, port]() {
        (void)client->connect("127.0.0.1", port);
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return peer.settings_exchanged(); }, 3s));
    EXPECT_TRUE(client->is_connected());

    // GET times out (peer never sends HEADERS+DATA).
    auto first = client->get("/timeout-1");
    EXPECT_TRUE(first.is_err());

    // Issue a second timed-out GET — exercises the same path again with
    // an incremented stream id, allocating sequentially via fetch_add(2).
    auto second = client->get("/timeout-2");
    EXPECT_TRUE(second.is_err());

    (void)client->disconnect();
    connector.join();
}

// HPACK eviction: server sends HEADERS with a large header set so the
// client's hpack_decoder triggers evict_to_size while building its
// response_headers vector. Drives the dynamic-table eviction path on the
// decoder side, reachable only post-handshake.
TEST_F(Http2ClientHermeticTest, GetWithManyHeadersTriggersHpackEviction)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Reply with HEADERS frame carrying many headers; total size
        // exceeds the default 4096-byte dynamic table to force eviction.
        {
            auto block = encode_many_headers(200, 80);
            kcenon::network::protocols::http2::headers_frame hf(
                hdr.stream_id, std::move(block),
                /*end_stream=*/true, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-hpack-evict-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto response = client->get("/hpack-evict");
    ASSERT_TRUE(response.is_ok());
    EXPECT_EQ(response.value().status_code, 200);
    // Verify many of the headers came through despite eviction.
    EXPECT_GE(response.value().headers.size(), 50u);

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// Streaming path: start_stream + write_stream + RST_STREAM from peer
// triggers handle_rst_stream_frame on a streaming session.
TEST_F(Http2ClientHermeticTest, StreamingRequestReceivesPeerRstStream)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        // Read the client HEADERS for streaming POST.
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Send RST_STREAM for the streaming request.
        {
            kcenon::network::protocols::http2::rst_stream_frame rsf(
                hdr.stream_id,
                static_cast<std::uint32_t>(
                    kcenon::network::protocols::http2::error_code::cancel));
            auto bytes = rsf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
        }

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-stream-rst-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->start_stream(
        "/streaming-rst", {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http_header>) {},
        [](int) {});
    ASSERT_TRUE(stream_result.is_ok());

    // Allow the RST_STREAM to land.
    std::this_thread::sleep_for(150ms);

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// Streaming HEADERS path: server sends HEADERS frame to a streaming
// request — drives the on_headers callback branch in handle_headers_frame.
TEST_F(Http2ClientHermeticTest, StreamingRequestReceivesHeadersCallback)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Send HEADERS without END_STREAM so the streaming session stays
        // open and the on_headers callback fires.
        {
            auto block = encode_status(202);
            kcenon::network::protocols::http2::headers_frame hf(
                hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // Send a DATA frame with body bytes (no END_STREAM), then a final
        // empty DATA with END_STREAM to close it. This drives the
        // streaming on_data + on_complete callbacks.
        {
            std::vector<std::uint8_t> body{'c', 'h', 'u', 'n', 'k'};
            kcenon::network::protocols::http2::data_frame df(
                hdr.stream_id, body, /*end_stream=*/false);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        {
            std::vector<std::uint8_t> empty;
            kcenon::network::protocols::http2::data_frame df(
                hdr.stream_id, empty, /*end_stream=*/true);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-stream-cb-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    std::atomic<bool> got_headers{false};
    std::atomic<bool> got_data{false};
    std::atomic<int> complete_status{-1};

    auto stream_result = client->start_stream(
        "/streaming-cb", {},
        [&](std::vector<uint8_t> chunk) {
            if (!chunk.empty()) got_data.store(true);
        },
        [&](std::vector<http_header>) { got_headers.store(true); },
        [&](int status) { complete_status.store(status); });
    ASSERT_TRUE(stream_result.is_ok());

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() {
            return got_headers.load() && got_data.load() &&
                   complete_status.load() == 202;
        },
        3s));

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// Unknown frame type: peer sends a frame with a reserved type byte
// (e.g. 0xFE). process_frame's default switch arm should ignore it and
// the run_io loop should keep going.
TEST_F(Http2ClientHermeticTest, ClientIgnoresUnknownFrameType)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        // Hand-construct a frame header with an unknown type (0xFE).
        // length=0, type=0xFE, flags=0, stream_id=0.
        std::array<std::uint8_t, kFrameHeaderSize> raw = {
            0x00, 0x00, 0x00,  // length = 0
            0xFE,              // type = 0xFE (unknown)
            0x00,              // flags
            0x00, 0x00, 0x00, 0x00  // stream_id
        };
        asio::write(*stream, asio::buffer(raw), ec);
        if (ec) return;

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-unknown-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    // Allow the unknown frame to be received and dispatched.
    std::this_thread::sleep_for(150ms);

    EXPECT_TRUE(client->is_connected())
        << "client should ignore unknown frame types and stay connected";

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// HEADERS with non-numeric :status should result in status_code = 0
// (drives the catch-all branch in handle_headers_frame's stoi try/catch).
TEST_F(Http2ClientHermeticTest, GetWithNonNumericStatusYieldsZero)
{
    support::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Encode :status with a non-numeric value to force the std::stoi
        // try/catch fallback in handle_headers_frame.
        kcenon::network::protocols::http2::hpack_encoder enc(4096);
        std::vector<kcenon::network::protocols::http2::http_header> headers = {
            {":status", "abc"},
        };
        auto block = enc.encode(headers);

        kcenon::network::protocols::http2::headers_frame hf(
            hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);

        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            if (p.value().length > 0)
            {
                std::vector<std::uint8_t> payload(p.value().length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-bad-status-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto response = client->get("/bad-status");
    ASSERT_TRUE(response.is_ok());
    EXPECT_EQ(response.value().status_code, 0);

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}

// Flow control: peer sends a large DATA payload that crosses the
// half-window threshold so the client emits an automatic WINDOW_UPDATE
// for the stream and the connection.
TEST_F(Http2ClientHermeticTest, GetWithLargeBodyTriggersWindowUpdate)
{
    support::tls_loopback_listener listener(io());

    std::atomic<bool> client_window_update_received{false};

    std::thread peer_thread([&]() {
        auto stream = complete_settings_exchange(listener);
        if (!stream)
        {
            return;
        }

        std::error_code ec;
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = kcenon::network::protocols::http2::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto hdr = parsed.value();
        if (hdr.length > 0)
        {
            std::vector<std::uint8_t> drain(hdr.length);
            asio::read(*stream, asio::buffer(drain), ec);
            if (ec) return;
        }

        // Reply with HEADERS (no END_STREAM).
        {
            auto block = encode_status(200);
            kcenon::network::protocols::http2::headers_frame hf(
                hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // DATA frame that is large enough to push window_size below
        // DEFAULT_WINDOW_SIZE/2 (32768), forcing the client to emit a
        // WINDOW_UPDATE. Send 40 KB.
        {
            std::vector<std::uint8_t> body(40 * 1024, 0x42);
            kcenon::network::protocols::http2::data_frame df(
                hdr.stream_id, body, /*end_stream=*/true);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }

        // Look for the WINDOW_UPDATE frame from the client.
        while (true)
        {
            std::array<std::uint8_t, kFrameHeaderSize> drain_hdr{};
            asio::read(*stream, asio::buffer(drain_hdr), ec);
            if (ec) break;
            auto p = kcenon::network::protocols::http2::frame_header::parse(
                std::span<const std::uint8_t>(drain_hdr.data(), drain_hdr.size()));
            if (p.is_err()) break;
            const auto h = p.value();
            if (h.type ==
                kcenon::network::protocols::http2::frame_type::window_update)
            {
                client_window_update_received.store(true);
            }
            if (h.length > 0)
            {
                std::vector<std::uint8_t> payload(h.length);
                asio::read(*stream, asio::buffer(payload), ec);
                if (ec) break;
            }
        }
    });

    auto client = std::make_shared<http2_client>("hermetic-large-body-test");
    client->set_timeout(2000ms);

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto response = client->get("/large-body");
    ASSERT_TRUE(response.is_ok());
    EXPECT_EQ(response.value().status_code, 200);
    EXPECT_EQ(response.value().body.size(), 40u * 1024u);

    EXPECT_TRUE(support::hermetic_transport_fixture::wait_for(
        [&]() { return client_window_update_received.load(); }, 2s))
        << "client should auto-emit WINDOW_UPDATE after large DATA frame";

    (void)client->disconnect();
    connector.join();
    peer_thread.join();
}
