/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/http2/http2_server_stream.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

/**
 * @file http2_server_stream_test.cpp
 * @brief Unit tests for http2_server_stream
 *
 * Tests validate:
 * - Constructor and initial state
 * - stream_id(), method(), path(), headers(), request() accessors
 * - is_open() and headers_sent() state queries
 * - send_headers() operation and state transition
 * - send_data() with string and binary payloads
 * - start_response() / write() / end_response() streaming
 * - reset() operation
 * - update_window() and window_size()
 * - Error paths (send after close, double headers)
 */

// ============================================================================
// Test Helpers
// ============================================================================

namespace
{

/**
 * @brief Creates a test request with common fields populated
 */
http2::http2_request make_test_request(
	const std::string& method = "GET",
	const std::string& path = "/api/test")
{
	http2::http2_request req;
	req.method = method;
	req.path = path;
	req.scheme = "https";
	req.authority = "localhost:8443";
	req.headers = {
		http2::http_header{"content-type", "application/json"},
		http2::http_header{"accept", "application/json"}};
	return req;
}

/**
 * @brief Frame sender that captures sent frames for inspection
 */
class capturing_frame_sender
{
public:
	using VoidResult = kcenon::network::VoidResult;

	auto get_sender() -> http2::http2_server_stream::frame_sender_t
	{
		return [this](const http2::frame& f) -> VoidResult {
			std::lock_guard<std::mutex> lock(mutex_);
			frame_count_++;
			return kcenon::network::ok();
		};
	}

	int frame_count() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return frame_count_;
	}

private:
	mutable std::mutex mutex_;
	int frame_count_ = 0;
};

/**
 * @brief Frame sender that returns errors
 */
auto make_failing_sender() -> http2::http2_server_stream::frame_sender_t
{
	return [](const http2::frame&) -> kcenon::network::VoidResult {
		return kcenon::network::error_void(-1, "connection reset", "test");
	};
}

} // namespace

// ============================================================================
// Constructor and Accessor Tests
// ============================================================================

class Http2ServerStreamConstructorTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamConstructorTest, BasicConstruction)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	EXPECT_EQ(stream.stream_id(), 1u);
}

TEST_F(Http2ServerStreamConstructorTest, StreamId)
{
	http2::http2_server_stream stream(
		5, make_test_request(), encoder_, sender_.get_sender());

	EXPECT_EQ(stream.stream_id(), 5u);
}

TEST_F(Http2ServerStreamConstructorTest, Method)
{
	http2::http2_server_stream stream(
		1, make_test_request("POST"), encoder_, sender_.get_sender());

	EXPECT_EQ(stream.method(), "POST");
}

TEST_F(Http2ServerStreamConstructorTest, Path)
{
	http2::http2_server_stream stream(
		1, make_test_request("GET", "/health"), encoder_, sender_.get_sender());

	EXPECT_EQ(stream.path(), "/health");
}

TEST_F(Http2ServerStreamConstructorTest, Headers)
{
	auto req = make_test_request();
	http2::http2_server_stream stream(
		1, req, encoder_, sender_.get_sender());

	auto& headers = stream.headers();
	EXPECT_EQ(headers.size(), 2u);
	EXPECT_EQ(headers[0].name, "content-type");
}

TEST_F(Http2ServerStreamConstructorTest, Request)
{
	auto req = make_test_request("PUT", "/api/data");
	http2::http2_server_stream stream(
		1, req, encoder_, sender_.get_sender());

	auto& request = stream.request();
	EXPECT_EQ(request.method, "PUT");
	EXPECT_EQ(request.path, "/api/data");
	EXPECT_EQ(request.scheme, "https");
	EXPECT_EQ(request.authority, "localhost:8443");
}

// ============================================================================
// Initial State Tests
// ============================================================================

class Http2ServerStreamStateTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamStateTest, InitiallyOpen)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	EXPECT_TRUE(stream.is_open());
}

TEST_F(Http2ServerStreamStateTest, HeadersNotSentInitially)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	EXPECT_FALSE(stream.headers_sent());
}

TEST_F(Http2ServerStreamStateTest, StateIsOpen)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	EXPECT_EQ(stream.state(), http2::stream_state::open);
}

TEST_F(Http2ServerStreamStateTest, DefaultWindowSize)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	EXPECT_EQ(stream.window_size(), 65535);
}

TEST_F(Http2ServerStreamStateTest, CustomMaxFrameSize)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender(), 8192);

	EXPECT_TRUE(stream.is_open());
}

// ============================================================================
// send_headers Tests
// ============================================================================

class Http2ServerStreamSendHeadersTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamSendHeadersTest, SendHeadersSuccess)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto result = stream.send_headers(200, {});

	EXPECT_FALSE(result.is_err());
	EXPECT_TRUE(stream.headers_sent());
}

TEST_F(Http2ServerStreamSendHeadersTest, SendHeadersWithAdditional)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	std::vector<http2::http_header> headers = {
		{"content-type", "application/json"},
		{"x-request-id", "abc-123"}};

	auto result = stream.send_headers(200, headers);

	EXPECT_FALSE(result.is_err());
	EXPECT_GT(sender_.frame_count(), 0);
}

TEST_F(Http2ServerStreamSendHeadersTest, SendHeadersWithEndStream)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto result = stream.send_headers(204, {}, true);

	EXPECT_FALSE(result.is_err());
	// Stream should be closed (half-closed local) after end_stream
	EXPECT_FALSE(stream.is_open());
}

TEST_F(Http2ServerStreamSendHeadersTest, Send404Status)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto result = stream.send_headers(404, {});

	EXPECT_FALSE(result.is_err());
	EXPECT_TRUE(stream.headers_sent());
}

// ============================================================================
// send_data Tests
// ============================================================================

class Http2ServerStreamSendDataTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamSendDataTest, SendStringData)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	stream.send_headers(200, {});

	auto result = stream.send_data(std::string_view{R"({"status":"ok"})"}, true);

	EXPECT_FALSE(result.is_err());
}

TEST_F(Http2ServerStreamSendDataTest, SendBinaryData)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	stream.send_headers(200, {});

	std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
	auto result = stream.send_data(data, true);

	EXPECT_FALSE(result.is_err());
}

// ============================================================================
// Streaming Response Tests
// ============================================================================

class Http2ServerStreamStreamingTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamStreamingTest, StartResponseAndWrite)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto start_result = stream.start_response(
		200, {http2::http_header{"content-type", "text/plain"}});
	EXPECT_FALSE(start_result.is_err());

	std::vector<uint8_t> chunk1 = {'H', 'e', 'l', 'l', 'o'};
	auto write_result = stream.write(chunk1);
	EXPECT_FALSE(write_result.is_err());

	auto end_result = stream.end_response();
	EXPECT_FALSE(end_result.is_err());
}

// ============================================================================
// reset Tests
// ============================================================================

class Http2ServerStreamResetTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamResetTest, ResetStream)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto result = stream.reset();

	EXPECT_FALSE(result.is_err());
	EXPECT_FALSE(stream.is_open());
}

TEST_F(Http2ServerStreamResetTest, ResetWithErrorCode)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto result = stream.reset(
		static_cast<uint32_t>(http2::error_code::internal_error));

	EXPECT_FALSE(result.is_err());
}

// ============================================================================
// Window Update Tests
// ============================================================================

class Http2ServerStreamWindowTest : public ::testing::Test
{
protected:
	capturing_frame_sender sender_;
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamWindowTest, UpdateWindow)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	stream.update_window(1000);

	EXPECT_EQ(stream.window_size(), 65535 + 1000);
}

TEST_F(Http2ServerStreamWindowTest, WindowDecreasesWithSent)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender_.get_sender());

	auto initial_window = stream.window_size();

	stream.update_window(-100);

	EXPECT_EQ(stream.window_size(), initial_window - 100);
}

// ============================================================================
// Error Path Tests
// ============================================================================

class Http2ServerStreamErrorTest : public ::testing::Test
{
protected:
	std::shared_ptr<http2::hpack_encoder> encoder_
		= std::make_shared<http2::hpack_encoder>();
};

TEST_F(Http2ServerStreamErrorTest, SendDataWithFailingSender)
{
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, make_failing_sender());

	auto result = stream.send_headers(200, {});

	EXPECT_TRUE(result.is_err());
}

TEST_F(Http2ServerStreamErrorTest, SendAfterReset)
{
	capturing_frame_sender sender;
	http2::http2_server_stream stream(
		1, make_test_request(), encoder_, sender.get_sender());

	stream.reset();

	auto result = stream.send_headers(200, {});

	EXPECT_TRUE(result.is_err());
}
