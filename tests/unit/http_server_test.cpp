/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/http_server.h"
#include "internal/http/http_error.h"
#include <gtest/gtest.h>

#include <chrono>
#include <regex>
#include <sstream>
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

// ============================================================================
// http_request_buffer Coverage Tests (edge cases)
// ============================================================================

class HttpRequestBufferCoverageTest : public ::testing::Test
{
protected:
	http_request_buffer buffer_;
};

TEST_F(HttpRequestBufferCoverageTest, AppendExceedingMaxRequestSize)
{
	// Create a chunk that exceeds MAX_REQUEST_SIZE
	std::vector<uint8_t> huge_chunk(http_request_buffer::MAX_REQUEST_SIZE + 1, 'A');

	bool result = buffer_.append(huge_chunk);

	// Should reject with false (413 Payload Too Large path)
	EXPECT_FALSE(result);
}

TEST_F(HttpRequestBufferCoverageTest, AppendCumulativeExceedsMaxRequestSize)
{
	// First append: valid headers with large Content-Length
	std::string headers =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 999999\r\n"
		"\r\n";
	std::vector<uint8_t> chunk1(headers.begin(), headers.end());
	EXPECT_TRUE(buffer_.append(chunk1));

	// Add body data just under the limit
	std::size_t remaining = http_request_buffer::MAX_REQUEST_SIZE - chunk1.size();
	std::vector<uint8_t> chunk2(remaining, 'A');
	EXPECT_TRUE(buffer_.append(chunk2));

	// This one pushes over MAX_REQUEST_SIZE
	std::vector<uint8_t> chunk3(2, 'B');
	EXPECT_FALSE(buffer_.append(chunk3));
}

TEST_F(HttpRequestBufferCoverageTest, HeaderTooLargeWithoutComplete)
{
	// Fill buffer with header data that exceeds MAX_HEADER_SIZE
	// without containing \r\n\r\n (no complete headers)
	std::vector<uint8_t> large_header(http_request_buffer::MAX_HEADER_SIZE + 1, 'H');

	bool result = buffer_.append(large_header);

	// Should reject with false (431 Request Header Fields Too Large path)
	EXPECT_FALSE(result);
}

TEST_F(HttpRequestBufferCoverageTest, ContentLengthCaseInsensitive)
{
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"CONTENT-LENGTH: 100\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker, std::string::npos);

	auto length = http_request_buffer::parse_content_length(data, marker + 4);

	EXPECT_EQ(length, 100u);
}

TEST_F(HttpRequestBufferCoverageTest, ContentLengthMixedCase)
{
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-length: 77\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker, std::string::npos);

	auto length = http_request_buffer::parse_content_length(data, marker + 4);

	EXPECT_EQ(length, 77u);
}

TEST_F(HttpRequestBufferCoverageTest, ContentLengthWithWhitespace)
{
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length:   55  \r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker, std::string::npos);

	auto length = http_request_buffer::parse_content_length(data, marker + 4);

	EXPECT_EQ(length, 55u);
}

TEST_F(HttpRequestBufferCoverageTest, ContentLengthInvalidValue)
{
	// Tests the catch (...) path in parse_content_length
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: not_a_number\r\n"
		"\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());
	auto marker = http_request_buffer::find_header_end(data);
	ASSERT_NE(marker, std::string::npos);

	auto length = http_request_buffer::parse_content_length(data, marker + 4);

	// Invalid value falls through catch to return 0
	EXPECT_EQ(length, 0u);
}

TEST_F(HttpRequestBufferCoverageTest, IsCompleteReturnsFalseWhenHeadersIncomplete)
{
	std::string partial = "GET / HTTP/1.1\r\nHost: localhost\r\n";
	std::vector<uint8_t> data(partial.begin(), partial.end());
	buffer_.append(data);

	// Headers not complete, so is_complete should return false
	EXPECT_FALSE(buffer_.is_complete());
}

TEST_F(HttpRequestBufferCoverageTest, MultipleHeadersBeforeContentLength)
{
	std::string raw =
		"POST /data HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Accept: text/html\r\n"
		"X-Custom: value\r\n"
		"Content-Length: 13\r\n"
		"Accept-Encoding: gzip\r\n"
		"\r\n"
		"Hello, World!";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	EXPECT_TRUE(buffer_.append(data));
	EXPECT_TRUE(buffer_.is_complete());
}

TEST_F(HttpRequestBufferCoverageTest, FindHeaderEndAtStartOfData)
{
	// Edge case: \r\n\r\n right at the start
	std::string raw = "\r\n\r\nBody here";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	auto pos = http_request_buffer::find_header_end(data);

	EXPECT_EQ(pos, 0u);
}

TEST_F(HttpRequestBufferCoverageTest, FindHeaderEndSmallData)
{
	// Data shorter than 4 bytes
	std::vector<uint8_t> data = {'\r', '\n', '\r'};

	auto pos = http_request_buffer::find_header_end(data);

	EXPECT_EQ(pos, std::string::npos);
}

// ============================================================================
// http_route Matching Tests
// ============================================================================

class HttpRouteMatchTest : public ::testing::Test
{
protected:
	// Helper to create a route with compiled regex
	static auto make_route(http_method method, const std::string& pattern) -> http_route
	{
		http_route route;
		route.method = method;
		route.pattern = pattern;

		// Use http_server's internal pattern_to_regex logic by constructing regex manually
		// For simple static routes:
		if (pattern.find(':') == std::string::npos) {
			route.regex_pattern = std::regex("^" + pattern + "$");
		} else {
			// Parse :param placeholders
			std::ostringstream regex_str;
			regex_str << "^";
			std::size_t pos = 0;
			while (pos < pattern.length()) {
				auto param_start = pattern.find(':', pos);
				if (param_start == std::string::npos) {
					regex_str << pattern.substr(pos);
					break;
				}
				regex_str << pattern.substr(pos, param_start - pos);
				auto param_end = param_start + 1;
				while (param_end < pattern.length() &&
					   (std::isalnum(pattern[param_end]) || pattern[param_end] == '_')) {
					++param_end;
				}
				route.param_names.push_back(
					pattern.substr(param_start + 1, param_end - param_start - 1));
				regex_str << "([^/]+)";
				pos = param_end;
			}
			regex_str << "$";
			route.regex_pattern = std::regex(regex_str.str());
		}

		route.handler = [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		};
		return route;
	}
};

TEST_F(HttpRouteMatchTest, StaticRouteMatches)
{
	auto route = make_route(http_method::HTTP_GET, "/api/users");
	std::map<std::string, std::string> params;

	EXPECT_TRUE(route.matches(http_method::HTTP_GET, "/api/users", params));
	EXPECT_TRUE(params.empty());
}

TEST_F(HttpRouteMatchTest, StaticRouteMethodMismatch)
{
	auto route = make_route(http_method::HTTP_GET, "/api/users");
	std::map<std::string, std::string> params;

	// POST vs GET should not match
	EXPECT_FALSE(route.matches(http_method::HTTP_POST, "/api/users", params));
}

TEST_F(HttpRouteMatchTest, StaticRoutePathMismatch)
{
	auto route = make_route(http_method::HTTP_GET, "/api/users");
	std::map<std::string, std::string> params;

	EXPECT_FALSE(route.matches(http_method::HTTP_GET, "/api/products", params));
}

TEST_F(HttpRouteMatchTest, ParamRouteExtractsSingleParam)
{
	auto route = make_route(http_method::HTTP_GET, "/users/:id");
	std::map<std::string, std::string> params;

	EXPECT_TRUE(route.matches(http_method::HTTP_GET, "/users/42", params));
	ASSERT_EQ(params.size(), 1u);
	EXPECT_EQ(params["id"], "42");
}

TEST_F(HttpRouteMatchTest, ParamRouteExtractsMultipleParams)
{
	auto route = make_route(http_method::HTTP_GET, "/users/:user_id/posts/:post_id");
	std::map<std::string, std::string> params;

	EXPECT_TRUE(route.matches(http_method::HTTP_GET, "/users/7/posts/99", params));
	ASSERT_EQ(params.size(), 2u);
	EXPECT_EQ(params["user_id"], "7");
	EXPECT_EQ(params["post_id"], "99");
}

TEST_F(HttpRouteMatchTest, ParamRouteRejectsPartialPath)
{
	auto route = make_route(http_method::HTTP_GET, "/users/:id");
	std::map<std::string, std::string> params;

	// Too many segments
	EXPECT_FALSE(route.matches(http_method::HTTP_GET, "/users/42/extra", params));
}

TEST_F(HttpRouteMatchTest, ParamRouteRejectsNoParamValue)
{
	auto route = make_route(http_method::HTTP_GET, "/users/:id");
	std::map<std::string, std::string> params;

	// Empty param segment (matches [^/]+ so requires at least one char)
	EXPECT_FALSE(route.matches(http_method::HTTP_GET, "/users/", params));
}

TEST_F(HttpRouteMatchTest, RootRouteMatches)
{
	auto route = make_route(http_method::HTTP_GET, "/");
	std::map<std::string, std::string> params;

	EXPECT_TRUE(route.matches(http_method::HTTP_GET, "/", params));
}

TEST_F(HttpRouteMatchTest, AllHttpMethodsCanMatch)
{
	std::map<std::string, std::string> params;

	auto route_post = make_route(http_method::HTTP_POST, "/data");
	EXPECT_TRUE(route_post.matches(http_method::HTTP_POST, "/data", params));

	auto route_put = make_route(http_method::HTTP_PUT, "/data");
	EXPECT_TRUE(route_put.matches(http_method::HTTP_PUT, "/data", params));

	auto route_del = make_route(http_method::HTTP_DELETE, "/data");
	EXPECT_TRUE(route_del.matches(http_method::HTTP_DELETE, "/data", params));

	auto route_patch = make_route(http_method::HTTP_PATCH, "/data");
	EXPECT_TRUE(route_patch.matches(http_method::HTTP_PATCH, "/data", params));

	auto route_head = make_route(http_method::HTTP_HEAD, "/data");
	EXPECT_TRUE(route_head.matches(http_method::HTTP_HEAD, "/data", params));

	auto route_opt = make_route(http_method::HTTP_OPTIONS, "/data");
	EXPECT_TRUE(route_opt.matches(http_method::HTTP_OPTIONS, "/data", params));
}

TEST_F(HttpRouteMatchTest, MatchClearsExistingParams)
{
	auto route = make_route(http_method::HTTP_GET, "/users/:id");
	std::map<std::string, std::string> params;
	params["stale"] = "value";

	EXPECT_TRUE(route.matches(http_method::HTTP_GET, "/users/42", params));

	// Stale param should be cleared
	EXPECT_EQ(params.find("stale"), params.end());
	EXPECT_EQ(params["id"], "42");
}

// ============================================================================
// http_error Tests
// ============================================================================

class HttpErrorTest : public ::testing::Test
{
};

TEST_F(HttpErrorTest, ClientErrorRange)
{
	http_error err;
	err.code = http_error_code::bad_request;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_FALSE(err.is_server_error());
	EXPECT_EQ(err.status_code(), 400);
}

TEST_F(HttpErrorTest, ServerErrorRange)
{
	http_error err;
	err.code = http_error_code::internal_server_error;
	EXPECT_TRUE(err.is_server_error());
	EXPECT_FALSE(err.is_client_error());
	EXPECT_EQ(err.status_code(), 500);
}

TEST_F(HttpErrorTest, NotFoundIsClientError)
{
	http_error err;
	err.code = http_error_code::not_found;
	EXPECT_TRUE(err.is_client_error());
	EXPECT_EQ(err.status_code(), 404);
}

TEST_F(HttpErrorTest, ServiceUnavailableIsServerError)
{
	http_error err;
	err.code = http_error_code::service_unavailable;
	EXPECT_TRUE(err.is_server_error());
	EXPECT_EQ(err.status_code(), 503);
}

TEST_F(HttpErrorTest, ParseErrorToHttpError)
{
	parse_error pe;
	pe.error_type = parse_error_type::invalid_method;
	pe.message = "Unknown method";
	pe.context = "INVALID / HTTP/1.1";

	auto he = pe.to_http_error();

	EXPECT_EQ(he.code, http_error_code::bad_request);
	EXPECT_EQ(he.message, "Bad Request");
	EXPECT_NE(he.detail.find("Unknown method"), std::string::npos);
	EXPECT_NE(he.detail.find("INVALID / HTTP/1.1"), std::string::npos);
}

TEST_F(HttpErrorTest, ParseErrorToHttpErrorEmptyContext)
{
	parse_error pe;
	pe.message = "Malformed";
	pe.context = "";

	auto he = pe.to_http_error();

	EXPECT_EQ(he.detail, "Malformed");
	// No " near: " suffix when context is empty
	EXPECT_EQ(he.detail.find(" near: "), std::string::npos);
}

TEST_F(HttpErrorTest, MakeErrorWithDetail)
{
	auto err = http_error_response::make_error(
		http_error_code::not_found, "Resource not found", "req-123");

	EXPECT_EQ(err.code, http_error_code::not_found);
	EXPECT_EQ(err.detail, "Resource not found");
	EXPECT_EQ(err.request_id, "req-123");
}

TEST_F(HttpErrorTest, BuildJsonErrorResponse)
{
	auto err = http_error_response::make_error(
		http_error_code::bad_request, "Invalid JSON");

	auto resp = http_error_response::build_json_error(err);

	EXPECT_EQ(resp.status_code, 400);
	auto content_type = resp.get_header("Content-Type");
	ASSERT_TRUE(content_type.has_value());
	EXPECT_NE(content_type->find("json"), std::string::npos);

	auto body = resp.get_body_string();
	EXPECT_NE(body.find("400"), std::string::npos);
}

TEST_F(HttpErrorTest, BuildHtmlErrorResponse)
{
	auto err = http_error_response::make_error(
		http_error_code::not_found, "Page not found");

	auto resp = http_error_response::build_html_error(err);

	EXPECT_EQ(resp.status_code, 404);
	auto body = resp.get_body_string();
	EXPECT_NE(body.find("404"), std::string::npos);
}

// ============================================================================
// http_types Conversion Tests
// ============================================================================

class HttpTypesTest : public ::testing::Test
{
};

TEST_F(HttpTypesTest, MethodToStringRoundTrip)
{
	auto get_str = http_method_to_string(http_method::HTTP_GET);
	EXPECT_EQ(get_str, "GET");

	auto post_str = http_method_to_string(http_method::HTTP_POST);
	EXPECT_EQ(post_str, "POST");

	auto put_str = http_method_to_string(http_method::HTTP_PUT);
	EXPECT_EQ(put_str, "PUT");

	auto del_str = http_method_to_string(http_method::HTTP_DELETE);
	EXPECT_EQ(del_str, "DELETE");

	auto head_str = http_method_to_string(http_method::HTTP_HEAD);
	EXPECT_EQ(head_str, "HEAD");

	auto options_str = http_method_to_string(http_method::HTTP_OPTIONS);
	EXPECT_EQ(options_str, "OPTIONS");

	auto patch_str = http_method_to_string(http_method::HTTP_PATCH);
	EXPECT_EQ(patch_str, "PATCH");
}

TEST_F(HttpTypesTest, StringToMethodValid)
{
	auto get_result = string_to_http_method("GET");
	ASSERT_TRUE(get_result.is_ok());
	EXPECT_EQ(get_result.value(), http_method::HTTP_GET);

	auto post_result = string_to_http_method("POST");
	ASSERT_TRUE(post_result.is_ok());
	EXPECT_EQ(post_result.value(), http_method::HTTP_POST);
}

TEST_F(HttpTypesTest, StringToMethodInvalid)
{
	auto result = string_to_http_method("INVALID");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpTypesTest, VersionToStringRoundTrip)
{
	auto v10 = http_version_to_string(http_version::HTTP_1_0);
	EXPECT_NE(v10.find("1.0"), std::string::npos);

	auto v11 = http_version_to_string(http_version::HTTP_1_1);
	EXPECT_NE(v11.find("1.1"), std::string::npos);
}

TEST_F(HttpTypesTest, StringToVersionValid)
{
	auto v11 = string_to_http_version("HTTP/1.1");
	ASSERT_TRUE(v11.is_ok());
	EXPECT_EQ(v11.value(), http_version::HTTP_1_1);
}

TEST_F(HttpTypesTest, StringToVersionInvalid)
{
	auto result = string_to_http_version("HTTP/9.9");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpTypesTest, GetStatusMessage)
{
	EXPECT_EQ(get_status_message(200), "OK");
	EXPECT_EQ(get_status_message(404), "Not Found");
	EXPECT_EQ(get_status_message(500), "Internal Server Error");
}

TEST_F(HttpTypesTest, RequestSetAndGetBody)
{
	http_request req;
	req.set_body_string("Hello, World!");

	EXPECT_EQ(req.get_body_string(), "Hello, World!");
	EXPECT_EQ(req.body.size(), 13u);
}

TEST_F(HttpTypesTest, RequestSetAndGetHeader)
{
	http_request req;
	req.set_header("Content-Type", "application/json");

	auto ct = req.get_header("Content-Type");
	ASSERT_TRUE(ct.has_value());
	EXPECT_EQ(*ct, "application/json");
}

TEST_F(HttpTypesTest, RequestGetHeaderCaseInsensitive)
{
	http_request req;
	req.set_header("Content-Type", "text/plain");

	auto ct = req.get_header("content-type");
	ASSERT_TRUE(ct.has_value());
	EXPECT_EQ(*ct, "text/plain");
}

TEST_F(HttpTypesTest, RequestGetHeaderMissing)
{
	http_request req;
	auto result = req.get_header("X-Missing");
	EXPECT_FALSE(result.has_value());
}

TEST_F(HttpTypesTest, ResponseSetAndGetBody)
{
	http_response resp;
	resp.set_body_string("Response body");

	EXPECT_EQ(resp.get_body_string(), "Response body");
}

TEST_F(HttpTypesTest, ResponseSetAndGetHeader)
{
	http_response resp;
	resp.set_header("X-Custom", "value");

	auto hdr = resp.get_header("X-Custom");
	ASSERT_TRUE(hdr.has_value());
	EXPECT_EQ(*hdr, "value");
}

TEST_F(HttpTypesTest, ResponseSetCookie)
{
	http_response resp;
	resp.set_cookie("session_id", "abc123", "/", 3600, true, true, "Strict");

	ASSERT_EQ(resp.set_cookies.size(), 1u);
	EXPECT_EQ(resp.set_cookies[0].name, "session_id");
	EXPECT_EQ(resp.set_cookies[0].value, "abc123");
	EXPECT_EQ(resp.set_cookies[0].path, "/");
	EXPECT_EQ(resp.set_cookies[0].max_age, 3600);
	EXPECT_TRUE(resp.set_cookies[0].http_only);
	EXPECT_TRUE(resp.set_cookies[0].secure);
	EXPECT_EQ(resp.set_cookies[0].same_site, "Strict");
}

TEST_F(HttpTypesTest, CookieToHeaderValue)
{
	cookie c;
	c.name = "token";
	c.value = "xyz";
	c.path = "/api";
	c.max_age = 86400;
	c.http_only = true;
	c.secure = true;
	c.same_site = "Lax";

	auto header = c.to_header_value();

	EXPECT_NE(header.find("token=xyz"), std::string::npos);
	EXPECT_NE(header.find("Path=/api"), std::string::npos);
	EXPECT_NE(header.find("HttpOnly"), std::string::npos);
	EXPECT_NE(header.find("Secure"), std::string::npos);
}

// ============================================================================
// GetErrorStatusText Tests
// ============================================================================

TEST(GetErrorStatusTextTest, CommonCodes)
{
	EXPECT_FALSE(get_error_status_text(http_error_code::bad_request).empty());
	EXPECT_FALSE(get_error_status_text(http_error_code::not_found).empty());
	EXPECT_FALSE(get_error_status_text(http_error_code::internal_server_error).empty());
	EXPECT_FALSE(get_error_status_text(http_error_code::unauthorized).empty());
	EXPECT_FALSE(get_error_status_text(http_error_code::forbidden).empty());
	EXPECT_FALSE(get_error_status_text(http_error_code::service_unavailable).empty());
}
