/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/http2/http2_client.h"
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

/**
 * @file http2_client_test.cpp
 * @brief Unit tests for http2_client
 *
 * Tests validate:
 * - stream_state enum values
 * - http2_response struct (header lookup, body string conversion)
 * - http2_stream struct (default state, move semantics)
 * - http2_settings struct (default values)
 * - http2_client construction and initial state
 * - Timeout get/set
 * - Settings get/set
 * - Error paths (operations while disconnected)
 */

// ============================================================================
// stream_state Enum Tests
// ============================================================================

class StreamStateEnumTest : public ::testing::Test
{
};

TEST_F(StreamStateEnumTest, DistinctValues)
{
	EXPECT_NE(http2::stream_state::idle, http2::stream_state::open);
	EXPECT_NE(http2::stream_state::open, http2::stream_state::half_closed_local);
	EXPECT_NE(http2::stream_state::half_closed_local,
			  http2::stream_state::half_closed_remote);
	EXPECT_NE(http2::stream_state::half_closed_remote, http2::stream_state::closed);
}

TEST_F(StreamStateEnumTest, DefaultIsIdle)
{
	http2::stream_state state{};
	EXPECT_EQ(state, http2::stream_state::idle);
}

// ============================================================================
// http2_response Tests
// ============================================================================

class Http2ResponseTest : public ::testing::Test
{
protected:
	http2::http2_response response_;
};

TEST_F(Http2ResponseTest, DefaultStatusCode)
{
	EXPECT_EQ(response_.status_code, 0);
}

TEST_F(Http2ResponseTest, EmptyHeaders)
{
	EXPECT_TRUE(response_.headers.empty());
}

TEST_F(Http2ResponseTest, EmptyBody)
{
	EXPECT_TRUE(response_.body.empty());
}

TEST_F(Http2ResponseTest, GetBodyString)
{
	response_.body = {'H', 'e', 'l', 'l', 'o'};
	EXPECT_EQ(response_.get_body_string(), "Hello");
}

TEST_F(Http2ResponseTest, GetBodyStringEmpty)
{
	EXPECT_EQ(response_.get_body_string(), "");
}

TEST_F(Http2ResponseTest, GetHeaderFound)
{
	response_.headers = {
		http2::http_header{"content-type", "application/json"},
		http2::http_header{"x-request-id", "abc-123"}};

	auto ct = response_.get_header("content-type");
	ASSERT_TRUE(ct.has_value());
	EXPECT_EQ(*ct, "application/json");
}

TEST_F(Http2ResponseTest, GetHeaderNotFound)
{
	response_.headers = {
		http2::http_header{"content-type", "text/html"}};

	auto missing = response_.get_header("x-nonexistent");
	EXPECT_FALSE(missing.has_value());
}

// ============================================================================
// http2_stream Tests
// ============================================================================

class Http2StreamTest : public ::testing::Test
{
};

TEST_F(Http2StreamTest, DefaultValues)
{
	http2::http2_stream stream;
	EXPECT_EQ(stream.stream_id, 0u);
	EXPECT_EQ(stream.state, http2::stream_state::idle);
	EXPECT_TRUE(stream.request_headers.empty());
	EXPECT_TRUE(stream.response_headers.empty());
	EXPECT_TRUE(stream.request_body.empty());
	EXPECT_TRUE(stream.response_body.empty());
	EXPECT_EQ(stream.window_size, 65535);
	EXPECT_FALSE(stream.headers_complete);
	EXPECT_FALSE(stream.body_complete);
	EXPECT_FALSE(stream.is_streaming);
}

TEST_F(Http2StreamTest, MoveConstruction)
{
	http2::http2_stream original;
	original.stream_id = 5;
	original.state = http2::stream_state::open;
	original.window_size = 32768;

	http2::http2_stream moved(std::move(original));
	EXPECT_EQ(moved.stream_id, 5u);
	EXPECT_EQ(moved.state, http2::stream_state::open);
	EXPECT_EQ(moved.window_size, 32768);
}

TEST_F(Http2StreamTest, MoveAssignment)
{
	http2::http2_stream original;
	original.stream_id = 7;
	original.state = http2::stream_state::half_closed_local;

	http2::http2_stream target;
	target = std::move(original);
	EXPECT_EQ(target.stream_id, 7u);
	EXPECT_EQ(target.state, http2::stream_state::half_closed_local);
}

// ============================================================================
// http2_settings Tests
// ============================================================================

class Http2SettingsTest : public ::testing::Test
{
};

TEST_F(Http2SettingsTest, DefaultValues)
{
	http2::http2_settings settings;
	EXPECT_EQ(settings.header_table_size, 4096u);
	EXPECT_FALSE(settings.enable_push);
	EXPECT_EQ(settings.max_concurrent_streams, 100u);
	EXPECT_EQ(settings.initial_window_size, 65535u);
	EXPECT_EQ(settings.max_frame_size, 16384u);
	EXPECT_EQ(settings.max_header_list_size, 8192u);
}

TEST_F(Http2SettingsTest, CustomValues)
{
	http2::http2_settings settings;
	settings.header_table_size = 8192;
	settings.enable_push = true;
	settings.max_concurrent_streams = 200;
	settings.initial_window_size = 131070;
	settings.max_frame_size = 32768;
	settings.max_header_list_size = 16384;

	EXPECT_EQ(settings.header_table_size, 8192u);
	EXPECT_TRUE(settings.enable_push);
	EXPECT_EQ(settings.max_concurrent_streams, 200u);
	EXPECT_EQ(settings.initial_window_size, 131070u);
	EXPECT_EQ(settings.max_frame_size, 32768u);
	EXPECT_EQ(settings.max_header_list_size, 16384u);
}

// ============================================================================
// http2_client Construction and State Tests
// ============================================================================

class Http2ClientTest : public ::testing::Test
{
protected:
	std::shared_ptr<http2::http2_client> client_
		= std::make_shared<http2::http2_client>("test-client");
};

TEST_F(Http2ClientTest, InitiallyDisconnected)
{
	EXPECT_FALSE(client_->is_connected());
}

TEST_F(Http2ClientTest, DefaultTimeout)
{
	EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds{30000});
}

TEST_F(Http2ClientTest, SetTimeout)
{
	client_->set_timeout(std::chrono::milliseconds{5000});
	EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds{5000});
}

TEST_F(Http2ClientTest, SetTimeoutZero)
{
	client_->set_timeout(std::chrono::milliseconds{0});
	EXPECT_EQ(client_->get_timeout(), std::chrono::milliseconds{0});
}

TEST_F(Http2ClientTest, GetSettings)
{
	auto settings = client_->get_settings();
	EXPECT_EQ(settings.header_table_size, 4096u);
	EXPECT_EQ(settings.max_concurrent_streams, 100u);
}

TEST_F(Http2ClientTest, SetSettings)
{
	http2::http2_settings custom;
	custom.max_concurrent_streams = 50;
	custom.max_frame_size = 32768;
	client_->set_settings(custom);

	auto retrieved = client_->get_settings();
	EXPECT_EQ(retrieved.max_concurrent_streams, 50u);
	EXPECT_EQ(retrieved.max_frame_size, 32768u);
}

// ============================================================================
// Error Path Tests (disconnected state)
// ============================================================================

class Http2ClientDisconnectedTest : public ::testing::Test
{
protected:
	std::shared_ptr<http2::http2_client> client_
		= std::make_shared<http2::http2_client>("disconnected-client");
};

TEST_F(Http2ClientDisconnectedTest, GetWhileDisconnected)
{
	auto result = client_->get("/test");
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, PostWhileDisconnected)
{
	auto result = client_->post("/test", "body");
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, PutWhileDisconnected)
{
	auto result = client_->put("/test", "body");
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, DeleteWhileDisconnected)
{
	auto result = client_->del("/test");
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, DisconnectWhileDisconnected)
{
	// Should not crash or throw
	auto result = client_->disconnect();
	// May succeed (no-op) or return error; either is acceptable
	(void)result;
}

TEST_F(Http2ClientDisconnectedTest, PostBinaryWhileDisconnected)
{
	std::vector<uint8_t> body = {0x01, 0x02, 0x03};
	auto result = client_->post("/test", body);
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, WriteStreamWhileDisconnected)
{
	std::vector<uint8_t> data = {0x01};
	auto result = client_->write_stream(1, data);
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, CancelStreamWhileDisconnected)
{
	auto result = client_->cancel_stream(1);
	EXPECT_FALSE(result);
}

TEST_F(Http2ClientDisconnectedTest, CloseStreamWriterWhileDisconnected)
{
	auto result = client_->close_stream_writer(1);
	EXPECT_FALSE(result);
}
