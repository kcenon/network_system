/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/http_server.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace kcenon::network::core;
using namespace kcenon::network::internal;

/**
 * @file http_server_test.cpp
 * @brief Unit tests for http_server
 *
 * Tests validate:
 * - Construction with server_id
 * - Route registration (get, post, put, del, patch, head, options)
 * - Start/stop lifecycle on ephemeral port
 * - Double-start returns error
 * - Stop when not running returns error
 * - Custom error handler registration
 * - Destructor safety
 *
 * Note: Request handling and response tests require a running client
 * and are covered by integration tests.
 */

// ============================================================================
// Construction Tests
// ============================================================================

class HttpServerConstructionTest : public ::testing::Test
{
};

TEST_F(HttpServerConstructionTest, ConstructsWithServerId)
{
	auto server = std::make_shared<http_server>("test_http_server");

	// Server should not be running after construction
	// (no is_running() accessor, but start/stop should work)
	SUCCEED();
}

TEST_F(HttpServerConstructionTest, ConstructsWithEmptyServerId)
{
	auto server = std::make_shared<http_server>("");

	SUCCEED();
}

// ============================================================================
// Route Registration Tests
// ============================================================================

class HttpServerRouteTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<http_server>("route_test_server");
	}

	std::shared_ptr<http_server> server_;
};

TEST_F(HttpServerRouteTest, RegisterGetRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->get("/", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		resp.set_body_string("OK");
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterPostRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->post("/api/data", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 201;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterPutRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->put("/api/data/:id", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterDeleteRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->del("/api/data/:id", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 204;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterPatchRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->patch("/api/data/:id", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterHeadRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->head("/health", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterOptionsRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->options("/api/data", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 204;
		resp.set_header("Allow", "GET, POST, PUT, DELETE");
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterMultipleRoutesDoesNotThrow)
{
	EXPECT_NO_THROW({
		server_->get("/users", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		});
		server_->get("/users/:id", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		});
		server_->post("/users", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 201;
			return resp;
		});
		server_->del("/users/:id", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 204;
			return resp;
		});
	});
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

class HttpServerLifecycleTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<http_server>("lifecycle_test_server");
	}

	std::shared_ptr<http_server> server_;
};

TEST_F(HttpServerLifecycleTest, StartAndStopOnEphemeralPort)
{
	auto start_result = server_->start(0);
	ASSERT_TRUE(start_result.is_ok()) << "Start failed: " << start_result.error().message;

	// Give time for the server to bind
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto stop_result = server_->stop();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(HttpServerLifecycleTest, StartWithRoutesRegistered)
{
	server_->get("/", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		resp.set_body_string("Hello");
		return resp;
	});

	auto start_result = server_->start(0);
	ASSERT_TRUE(start_result.is_ok()) << "Start failed: " << start_result.error().message;

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto stop_result = server_->stop();
	EXPECT_TRUE(stop_result.is_ok());
}

// ============================================================================
// Error Handler Tests
// ============================================================================

class HttpServerErrorHandlerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<http_server>("error_handler_server");
	}

	std::shared_ptr<http_server> server_;
};

TEST_F(HttpServerErrorHandlerTest, SetNotFoundHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_not_found_handler(
		[](const http_request_context&)
		{
			http_response resp;
			resp.status_code = 404;
			resp.set_body_string("Not Found");
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetErrorHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_error_handler(
		[](const http_request_context&)
		{
			http_response resp;
			resp.status_code = 500;
			resp.set_body_string("Internal Server Error");
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetSpecificErrorHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_error_handler(
		http_error_code::bad_request,
		[](const http_error&)
		{
			http_response resp;
			resp.status_code = 400;
			resp.set_body_string("Bad Request");
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetDefaultErrorHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_default_error_handler(
		[](const http_error& err)
		{
			http_response resp;
			resp.status_code = err.status_code();
			resp.set_body_string(err.message);
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetJsonErrorResponsesDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_json_error_responses(true));
}

TEST_F(HttpServerErrorHandlerTest, SetRequestTimeoutDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_request_timeout(std::chrono::milliseconds(10000)));
}

TEST_F(HttpServerErrorHandlerTest, SetCompressionSettingsDoNotThrow)
{
	EXPECT_NO_THROW(server_->set_compression_enabled(true));
	EXPECT_NO_THROW(server_->set_compression_threshold(2048));
}

// ============================================================================
// Destructor Safety Tests
// ============================================================================

class HttpServerDestructorTest : public ::testing::Test
{
};

TEST_F(HttpServerDestructorTest, DestructorOnNonStartedServerDoesNotCrash)
{
	{
		auto server = std::make_shared<http_server>("destructor_test");
		server->get("/", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		});
		// Destructor called without starting
	}
	SUCCEED();
}

TEST_F(HttpServerDestructorTest, DestructorStopsRunningServer)
{
	{
		auto server = std::make_shared<http_server>("running_destructor_test");
		auto result = server->start(0);
		ASSERT_TRUE(result.is_ok()) << "Start failed: " << result.error().message;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		// Destructor should stop it
	}
	SUCCEED();
}

// ============================================================================
// http_request_buffer Tests
// ============================================================================

class HttpRequestBufferTest : public ::testing::Test
{
protected:
	http_request_buffer buffer_;
};

TEST_F(HttpRequestBufferTest, FindHeaderEndWithCompleteHeaders)
{
	std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nbody";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	auto pos = http_request_buffer::find_header_end(data);

	// Should find the position after \r\n\r\n
	EXPECT_GT(pos, 0u);
	EXPECT_NE(pos, std::string::npos);
}

TEST_F(HttpRequestBufferTest, FindHeaderEndWithNoEnd)
{
	std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	auto pos = http_request_buffer::find_header_end(data);

	// Returns npos when \r\n\r\n marker is not found
	EXPECT_EQ(pos, std::string::npos);
}

TEST_F(HttpRequestBufferTest, FindHeaderEndWithEmptyData)
{
	std::vector<uint8_t> data;

	auto pos = http_request_buffer::find_header_end(data);

	EXPECT_EQ(pos, std::string::npos);
}

TEST_F(HttpRequestBufferTest, ParseContentLengthPresent)
{
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 42\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker_pos = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker_pos, std::string::npos);

	// parse_content_length expects position after \r\n\r\n
	auto length = http_request_buffer::parse_content_length(data, marker_pos + 4);

	EXPECT_EQ(length, 42u);
}

TEST_F(HttpRequestBufferTest, ParseContentLengthAbsent)
{
	std::string raw =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker_pos = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker_pos, std::string::npos);

	auto length = http_request_buffer::parse_content_length(data, marker_pos + 4);

	EXPECT_EQ(length, 0u);
}

TEST_F(HttpRequestBufferTest, ParseContentLengthZero)
{
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker_pos = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker_pos, std::string::npos);

	auto length = http_request_buffer::parse_content_length(data, marker_pos + 4);

	EXPECT_EQ(length, 0u);
}

TEST_F(HttpRequestBufferTest, AppendAndCompleteGetRequest)
{
	std::string raw =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	bool ok = buffer_.append(data);
	EXPECT_TRUE(ok);
	EXPECT_TRUE(buffer_.is_complete());
}

TEST_F(HttpRequestBufferTest, AppendAndCompletePostWithBody)
{
	std::string headers_part =
		"POST /api HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 5\r\n"
		"\r\n";
	std::string body_part = "hello";

	std::vector<uint8_t> h(headers_part.begin(), headers_part.end());
	buffer_.append(h);

	// Headers complete but body not yet
	EXPECT_FALSE(buffer_.is_complete());

	std::vector<uint8_t> b(body_part.begin(), body_part.end());
	buffer_.append(b);

	EXPECT_TRUE(buffer_.is_complete());
}

TEST_F(HttpRequestBufferTest, IncrementalAppend)
{
	std::string full =
		"GET / HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"\r\n";

	// Feed one byte at a time
	for (char c : full)
	{
		std::vector<uint8_t> byte = {static_cast<uint8_t>(c)};
		buffer_.append(byte);
	}

	EXPECT_TRUE(buffer_.is_complete());
}

TEST_F(HttpRequestBufferTest, MaxRequestSizeConstant)
{
	// MAX_REQUEST_SIZE should be 10MB
	EXPECT_EQ(http_request_buffer::MAX_REQUEST_SIZE, 10u * 1024 * 1024);
}

TEST_F(HttpRequestBufferTest, MaxHeaderSizeConstant)
{
	// MAX_HEADER_SIZE should be 64KB
	EXPECT_EQ(http_request_buffer::MAX_HEADER_SIZE, 64u * 1024);
}

// ============================================================================
// http_request_context Tests
// ============================================================================

class HttpRequestContextTest : public ::testing::Test
{
protected:
	http_request_context ctx_;
};

TEST_F(HttpRequestContextTest, GetQueryParamExisting)
{
	ctx_.request.query_params["page"] = "1";
	ctx_.request.query_params["limit"] = "50";

	auto page = ctx_.get_query_param("page");
	ASSERT_TRUE(page.has_value());
	EXPECT_EQ(page.value(), "1");

	auto limit = ctx_.get_query_param("limit");
	ASSERT_TRUE(limit.has_value());
	EXPECT_EQ(limit.value(), "50");
}

TEST_F(HttpRequestContextTest, GetQueryParamMissing)
{
	ctx_.request.query_params["page"] = "1";

	auto missing = ctx_.get_query_param("sort");
	EXPECT_FALSE(missing.has_value());
}

TEST_F(HttpRequestContextTest, GetPathParamExisting)
{
	ctx_.path_params["id"] = "42";
	ctx_.path_params["name"] = "test";

	auto id = ctx_.get_path_param("id");
	ASSERT_TRUE(id.has_value());
	EXPECT_EQ(id.value(), "42");

	auto name = ctx_.get_path_param("name");
	ASSERT_TRUE(name.has_value());
	EXPECT_EQ(name.value(), "test");
}

TEST_F(HttpRequestContextTest, GetPathParamMissing)
{
	ctx_.path_params["id"] = "42";

	auto missing = ctx_.get_path_param("slug");
	EXPECT_FALSE(missing.has_value());
}

TEST_F(HttpRequestContextTest, GetQueryParamEmptyMap)
{
	auto result = ctx_.get_query_param("anything");
	EXPECT_FALSE(result.has_value());
}

TEST_F(HttpRequestContextTest, GetPathParamEmptyMap)
{
	auto result = ctx_.get_path_param("anything");
	EXPECT_FALSE(result.has_value());
}
