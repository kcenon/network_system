/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/http2/http2_request.h"
#include <gtest/gtest.h>

using namespace kcenon::network::protocols::http2;

/**
 * @file http2_request_test.cpp
 * @brief Unit tests for http2_request struct
 *
 * Tests validate:
 * - is_valid() for normal requests and CONNECT method
 * - get_header() case-insensitive lookup
 * - content_type() typed accessor
 * - content_length() parsing and error handling
 * - get_body_string() conversion
 * - from_headers() static factory method
 */

// ============================================================================
// is_valid() Tests
// ============================================================================

class Http2RequestValidationTest : public ::testing::Test
{
};

TEST_F(Http2RequestValidationTest, EmptyMethodIsInvalid)
{
	http2_request req;

	EXPECT_FALSE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, GetWithSchemeAndPathIsValid)
{
	http2_request req;
	req.method = "GET";
	req.scheme = "https";
	req.path = "/index.html";

	EXPECT_TRUE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, PostWithSchemeAndPathIsValid)
{
	http2_request req;
	req.method = "POST";
	req.scheme = "https";
	req.path = "/api/data";

	EXPECT_TRUE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, GetWithoutSchemeIsInvalid)
{
	http2_request req;
	req.method = "GET";
	req.path = "/index.html";

	EXPECT_FALSE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, GetWithoutPathIsInvalid)
{
	http2_request req;
	req.method = "GET";
	req.scheme = "https";

	EXPECT_FALSE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, ConnectWithAuthorityIsValid)
{
	http2_request req;
	req.method = "CONNECT";
	req.authority = "proxy.example.com:443";

	EXPECT_TRUE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, ConnectWithoutAuthorityIsInvalid)
{
	http2_request req;
	req.method = "CONNECT";

	EXPECT_FALSE(req.is_valid());
}

TEST_F(Http2RequestValidationTest, ConnectDoesNotRequireSchemeOrPath)
{
	http2_request req;
	req.method = "CONNECT";
	req.authority = "example.com:80";
	// No scheme or path needed for CONNECT

	EXPECT_TRUE(req.is_valid());
}

// ============================================================================
// get_header() Tests
// ============================================================================

class Http2RequestHeaderTest : public ::testing::Test
{
protected:
	http2_request req_;

	void SetUp() override
	{
		req_.method = "GET";
		req_.scheme = "https";
		req_.path = "/";
		req_.headers = {
			{"content-type", "application/json"},
			{"Authorization", "Bearer token123"},
			{"X-Custom-Header", "custom-value"},
		};
	}
};

TEST_F(Http2RequestHeaderTest, FindsExactMatchHeader)
{
	auto result = req_.get_header("content-type");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "application/json");
}

TEST_F(Http2RequestHeaderTest, FindsCaseInsensitiveHeader)
{
	auto result = req_.get_header("CONTENT-TYPE");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "application/json");
}

TEST_F(Http2RequestHeaderTest, FindsMixedCaseHeader)
{
	auto result = req_.get_header("authorization");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "Bearer token123");
}

TEST_F(Http2RequestHeaderTest, ReturnsNulloptForMissingHeader)
{
	auto result = req_.get_header("X-Missing-Header");

	EXPECT_FALSE(result.has_value());
}

TEST_F(Http2RequestHeaderTest, ReturnsNulloptForEmptyHeaders)
{
	http2_request empty_req;
	auto result = empty_req.get_header("content-type");

	EXPECT_FALSE(result.has_value());
}

// ============================================================================
// content_type() Tests
// ============================================================================

class Http2RequestContentTypeTest : public ::testing::Test
{
};

TEST_F(Http2RequestContentTypeTest, ReturnsContentTypeWhenPresent)
{
	http2_request req;
	req.headers = {{"content-type", "text/html"}};

	auto result = req.content_type();

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "text/html");
}

TEST_F(Http2RequestContentTypeTest, ReturnsNulloptWhenMissing)
{
	http2_request req;
	req.headers = {{"accept", "text/html"}};

	auto result = req.content_type();

	EXPECT_FALSE(result.has_value());
}

// ============================================================================
// content_length() Tests
// ============================================================================

class Http2RequestContentLengthTest : public ::testing::Test
{
};

TEST_F(Http2RequestContentLengthTest, ParsesValidContentLength)
{
	http2_request req;
	req.headers = {{"content-length", "1024"}};

	auto result = req.content_length();

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), 1024);
}

TEST_F(Http2RequestContentLengthTest, ParsesZeroContentLength)
{
	http2_request req;
	req.headers = {{"content-length", "0"}};

	auto result = req.content_length();

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), 0);
}

TEST_F(Http2RequestContentLengthTest, ReturnsNulloptForMissingHeader)
{
	http2_request req;

	auto result = req.content_length();

	EXPECT_FALSE(result.has_value());
}

TEST_F(Http2RequestContentLengthTest, ReturnsNulloptForInvalidValue)
{
	http2_request req;
	req.headers = {{"content-length", "not-a-number"}};

	auto result = req.content_length();

	EXPECT_FALSE(result.has_value());
}

TEST_F(Http2RequestContentLengthTest, ReturnsNulloptForEmptyValue)
{
	http2_request req;
	req.headers = {{"content-length", ""}};

	auto result = req.content_length();

	EXPECT_FALSE(result.has_value());
}

// ============================================================================
// get_body_string() Tests
// ============================================================================

class Http2RequestBodyTest : public ::testing::Test
{
};

TEST_F(Http2RequestBodyTest, ConvertsBodyToString)
{
	http2_request req;
	std::string text = "Hello, World!";
	req.body.assign(text.begin(), text.end());

	EXPECT_EQ(req.get_body_string(), "Hello, World!");
}

TEST_F(Http2RequestBodyTest, EmptyBodyReturnsEmptyString)
{
	http2_request req;

	EXPECT_EQ(req.get_body_string(), "");
}

TEST_F(Http2RequestBodyTest, ConvertsJsonBody)
{
	http2_request req;
	std::string json = R"({"key":"value"})";
	req.body.assign(json.begin(), json.end());

	EXPECT_EQ(req.get_body_string(), json);
}

// ============================================================================
// from_headers() Static Factory Tests
// ============================================================================

class Http2RequestFromHeadersTest : public ::testing::Test
{
};

TEST_F(Http2RequestFromHeadersTest, SeparatesPseudoHeaders)
{
	std::vector<http_header> headers = {
		{":method", "GET"},
		{":path", "/api/users"},
		{":scheme", "https"},
		{":authority", "example.com"},
		{"accept", "application/json"},
		{"user-agent", "TestClient/1.0"},
	};

	auto req = http2_request::from_headers(headers);

	EXPECT_EQ(req.method, "GET");
	EXPECT_EQ(req.path, "/api/users");
	EXPECT_EQ(req.scheme, "https");
	EXPECT_EQ(req.authority, "example.com");
	ASSERT_EQ(req.headers.size(), 2);
	EXPECT_EQ(req.headers[0].name, "accept");
	EXPECT_EQ(req.headers[1].name, "user-agent");
}

TEST_F(Http2RequestFromHeadersTest, HandlesEmptyHeaderList)
{
	std::vector<http_header> headers;

	auto req = http2_request::from_headers(headers);

	EXPECT_TRUE(req.method.empty());
	EXPECT_TRUE(req.path.empty());
	EXPECT_TRUE(req.scheme.empty());
	EXPECT_TRUE(req.authority.empty());
	EXPECT_TRUE(req.headers.empty());
}

TEST_F(Http2RequestFromHeadersTest, IgnoresUnknownPseudoHeaders)
{
	std::vector<http_header> headers = {
		{":method", "POST"},
		{":unknown-pseudo", "some-value"},
		{"content-type", "text/plain"},
	};

	auto req = http2_request::from_headers(headers);

	EXPECT_EQ(req.method, "POST");
	ASSERT_EQ(req.headers.size(), 1);
	EXPECT_EQ(req.headers[0].name, "content-type");
}

TEST_F(Http2RequestFromHeadersTest, SkipsEmptyNameHeaders)
{
	std::vector<http_header> headers = {
		{":method", "GET"},
		{"", "empty-name-value"},
		{"accept", "*/*"},
	};

	auto req = http2_request::from_headers(headers);

	EXPECT_EQ(req.method, "GET");
	ASSERT_EQ(req.headers.size(), 1);
	EXPECT_EQ(req.headers[0].name, "accept");
}

TEST_F(Http2RequestFromHeadersTest, OnlyPseudoHeadersNoRegularHeaders)
{
	std::vector<http_header> headers = {
		{":method", "DELETE"},
		{":path", "/api/resource/123"},
		{":scheme", "https"},
	};

	auto req = http2_request::from_headers(headers);

	EXPECT_EQ(req.method, "DELETE");
	EXPECT_EQ(req.path, "/api/resource/123");
	EXPECT_TRUE(req.headers.empty());
}

TEST_F(Http2RequestFromHeadersTest, ConnectRequestFromHeaders)
{
	std::vector<http_header> headers = {
		{":method", "CONNECT"},
		{":authority", "proxy.example.com:8080"},
	};

	auto req = http2_request::from_headers(headers);

	EXPECT_EQ(req.method, "CONNECT");
	EXPECT_EQ(req.authority, "proxy.example.com:8080");
	EXPECT_TRUE(req.is_valid());
}

TEST_F(Http2RequestFromHeadersTest, BuiltRequestIsValid)
{
	std::vector<http_header> headers = {
		{":method", "GET"},
		{":path", "/"},
		{":scheme", "https"},
	};

	auto req = http2_request::from_headers(headers);

	EXPECT_TRUE(req.is_valid());
}
