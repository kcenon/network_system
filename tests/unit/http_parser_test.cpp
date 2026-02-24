/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/http_parser.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace kcenon::network::internal;

/**
 * @file http_parser_test.cpp
 * @brief Unit tests for http_parser
 *
 * Tests validate:
 * - URL encoding and decoding
 * - Query string parsing and building
 * - HTTP request parsing (string_view and byte vector)
 * - HTTP response parsing (string_view and byte vector)
 * - Request and response serialization
 * - Chunked transfer encoding serialization
 * - Cookie header parsing
 * - Multipart form-data parsing
 * - Round-trip (serialize -> parse) consistency
 * - Error handling for malformed inputs
 */

// ============================================================================
// URL Encoding Tests
// ============================================================================

class HttpParserUrlEncodeTest : public ::testing::Test
{
};

TEST_F(HttpParserUrlEncodeTest, AlphanumericPassThrough)
{
	auto result = http_parser::url_encode("hello123");
	EXPECT_EQ(result, "hello123");
}

TEST_F(HttpParserUrlEncodeTest, SafeCharactersPassThrough)
{
	auto result = http_parser::url_encode("hello-world_test.v1~draft");
	EXPECT_EQ(result, "hello-world_test.v1~draft");
}

TEST_F(HttpParserUrlEncodeTest, SpacesEncodedAsPercent20)
{
	auto result = http_parser::url_encode("hello world");
	EXPECT_EQ(result, "hello%20world");
}

TEST_F(HttpParserUrlEncodeTest, SpecialCharactersEncoded)
{
	auto result = http_parser::url_encode("key=value&foo=bar");
	EXPECT_EQ(result, "key%3Dvalue%26foo%3Dbar");
}

TEST_F(HttpParserUrlEncodeTest, EmptyStringReturnsEmpty)
{
	auto result = http_parser::url_encode("");
	EXPECT_TRUE(result.empty());
}

TEST_F(HttpParserUrlEncodeTest, SlashEncoded)
{
	auto result = http_parser::url_encode("/path/to/resource");
	EXPECT_EQ(result, "%2Fpath%2Fto%2Fresource");
}

// ============================================================================
// URL Decoding Tests
// ============================================================================

class HttpParserUrlDecodeTest : public ::testing::Test
{
};

TEST_F(HttpParserUrlDecodeTest, PercentEncodedDecoded)
{
	auto result = http_parser::url_decode("hello%20world");
	EXPECT_EQ(result, "hello world");
}

TEST_F(HttpParserUrlDecodeTest, PlusDecodedAsSpace)
{
	auto result = http_parser::url_decode("hello+world");
	EXPECT_EQ(result, "hello world");
}

TEST_F(HttpParserUrlDecodeTest, MixedEncodedAndPlain)
{
	auto result = http_parser::url_decode("key%3Dvalue%26foo%3Dbar");
	EXPECT_EQ(result, "key=value&foo=bar");
}

TEST_F(HttpParserUrlDecodeTest, EmptyStringReturnsEmpty)
{
	auto result = http_parser::url_decode("");
	EXPECT_TRUE(result.empty());
}

TEST_F(HttpParserUrlDecodeTest, NoEncodingPassThrough)
{
	auto result = http_parser::url_decode("plaintext");
	EXPECT_EQ(result, "plaintext");
}

TEST_F(HttpParserUrlDecodeTest, UrlEncodeDecodeRoundTrip)
{
	std::string original = "hello world & foo=bar/baz?q=1";
	auto encoded = http_parser::url_encode(original);
	auto decoded = http_parser::url_decode(encoded);
	EXPECT_EQ(decoded, original);
}

// ============================================================================
// Query String Tests
// ============================================================================

class HttpParserQueryStringTest : public ::testing::Test
{
};

TEST_F(HttpParserQueryStringTest, ParseSimpleQueryString)
{
	auto result = http_parser::parse_query_string("key=value");
	ASSERT_EQ(result.size(), 1u);
	EXPECT_EQ(result["key"], "value");
}

TEST_F(HttpParserQueryStringTest, ParseMultipleParams)
{
	auto result = http_parser::parse_query_string("name=alice&age=30&city=seoul");
	ASSERT_EQ(result.size(), 3u);
	EXPECT_EQ(result["name"], "alice");
	EXPECT_EQ(result["age"], "30");
	EXPECT_EQ(result["city"], "seoul");
}

TEST_F(HttpParserQueryStringTest, ParseUrlEncodedValues)
{
	auto result = http_parser::parse_query_string("q=hello+world&tag=c%2B%2B");
	EXPECT_EQ(result["q"], "hello world");
	EXPECT_EQ(result["tag"], "c++");
}

TEST_F(HttpParserQueryStringTest, ParseParameterWithoutValue)
{
	auto result = http_parser::parse_query_string("flag&key=value");
	ASSERT_EQ(result.size(), 2u);
	EXPECT_EQ(result["flag"], "");
	EXPECT_EQ(result["key"], "value");
}

TEST_F(HttpParserQueryStringTest, ParseEmptyString)
{
	auto result = http_parser::parse_query_string("");
	EXPECT_TRUE(result.empty());
}

TEST_F(HttpParserQueryStringTest, BuildSimpleQueryString)
{
	std::map<std::string, std::string> params = {{"key", "value"}};
	auto result = http_parser::build_query_string(params);
	EXPECT_EQ(result, "key=value");
}

TEST_F(HttpParserQueryStringTest, BuildMultipleParams)
{
	std::map<std::string, std::string> params = {{"a", "1"}, {"b", "2"}};
	auto result = http_parser::build_query_string(params);
	// std::map is ordered, so a comes before b
	EXPECT_EQ(result, "a=1&b=2");
}

TEST_F(HttpParserQueryStringTest, BuildQueryStringEncodesSpecialChars)
{
	std::map<std::string, std::string> params = {{"q", "hello world"}};
	auto result = http_parser::build_query_string(params);
	EXPECT_EQ(result, "q=hello%20world");
}

TEST_F(HttpParserQueryStringTest, QueryStringRoundTrip)
{
	std::map<std::string, std::string> original = {{"name", "alice"}, {"city", "seoul"}};
	auto query = http_parser::build_query_string(original);
	auto parsed = http_parser::parse_query_string(query);
	EXPECT_EQ(parsed, original);
}

// ============================================================================
// HTTP Request Parsing Tests
// ============================================================================

class HttpParserRequestTest : public ::testing::Test
{
};

TEST_F(HttpParserRequestTest, ParseSimpleGetRequest)
{
	std::string raw = "GET /index.html HTTP/1.1\r\n"
	                  "Host: example.com\r\n"
	                  "\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& req = result.value();
	EXPECT_EQ(req.method, http_method::HTTP_GET);
	EXPECT_EQ(req.uri, "/index.html");
	EXPECT_EQ(req.version, http_version::HTTP_1_1);
	EXPECT_EQ(req.headers.at("Host"), "example.com");
	EXPECT_TRUE(req.body.empty());
}

TEST_F(HttpParserRequestTest, ParsePostRequestWithBody)
{
	std::string raw = "POST /api/users HTTP/1.1\r\n"
	                  "Host: example.com\r\n"
	                  "Content-Type: application/json\r\n"
	                  "Content-Length: 27\r\n"
	                  "\r\n"
	                  "{\"name\":\"alice\",\"age\":30}";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& req = result.value();
	EXPECT_EQ(req.method, http_method::HTTP_POST);
	EXPECT_EQ(req.uri, "/api/users");
	EXPECT_EQ(req.headers.at("Content-Type"), "application/json");
	EXPECT_EQ(req.get_body_string(), "{\"name\":\"alice\",\"age\":30}");
}

TEST_F(HttpParserRequestTest, ParseRequestWithQueryParams)
{
	std::string raw = "GET /search?q=hello+world&page=1 HTTP/1.1\r\n"
	                  "Host: example.com\r\n"
	                  "\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& req = result.value();
	EXPECT_EQ(req.uri, "/search");
	EXPECT_EQ(req.query_params.at("q"), "hello world");
	EXPECT_EQ(req.query_params.at("page"), "1");
}

TEST_F(HttpParserRequestTest, ParseRequestFromByteVector)
{
	std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	auto result = http_parser::parse_request(data);
	ASSERT_TRUE(result.is_ok());

	const auto& req = result.value();
	EXPECT_EQ(req.method, http_method::HTTP_GET);
	EXPECT_EQ(req.uri, "/");
}

TEST_F(HttpParserRequestTest, ParseRequestWithMultipleHeaders)
{
	std::string raw = "PUT /resource HTTP/1.1\r\n"
	                  "Host: example.com\r\n"
	                  "Content-Type: text/plain\r\n"
	                  "Accept: */*\r\n"
	                  "Authorization: Bearer token123\r\n"
	                  "\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& req = result.value();
	EXPECT_EQ(req.method, http_method::HTTP_PUT);
	EXPECT_EQ(req.headers.size(), 4u);
	EXPECT_EQ(req.headers.at("Authorization"), "Bearer token123");
}

TEST_F(HttpParserRequestTest, ParseRequestHTTP10)
{
	std::string raw = "GET / HTTP/1.0\r\n"
	                  "Host: example.com\r\n"
	                  "\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().version, http_version::HTTP_1_0);
}

TEST_F(HttpParserRequestTest, ParseDeleteRequest)
{
	std::string raw = "DELETE /api/users/42 HTTP/1.1\r\n"
	                  "Host: example.com\r\n"
	                  "\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().method, http_method::HTTP_DELETE);
	EXPECT_EQ(result.value().uri, "/api/users/42");
}

TEST_F(HttpParserRequestTest, ParseHeadersWithoutBody)
{
	std::string raw = "GET / HTTP/1.1\r\n"
	                  "Host: example.com\r\n"
	                  "Accept: text/html\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& req = result.value();
	EXPECT_EQ(req.headers.at("Host"), "example.com");
	EXPECT_EQ(req.headers.at("Accept"), "text/html");
	EXPECT_TRUE(req.body.empty());
}

// ---- Error Cases ----

TEST_F(HttpParserRequestTest, EmptyDataReturnsError)
{
	auto result = http_parser::parse_request(std::string_view(""));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestTest, InvalidRequestLineReturnsError)
{
	auto result = http_parser::parse_request(std::string_view("INVALIDLINE\r\n\r\n"));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestTest, InvalidMethodReturnsError)
{
	auto result = http_parser::parse_request(std::string_view("FOOBAR / HTTP/1.1\r\n\r\n"));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestTest, MissingVersionReturnsError)
{
	auto result = http_parser::parse_request(std::string_view("GET /path\r\n\r\n"));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestTest, InvalidHeaderLineReturnsError)
{
	std::string raw = "GET / HTTP/1.1\r\n"
	                  "InvalidHeaderNoColon\r\n"
	                  "\r\n";

	auto result = http_parser::parse_request(std::string_view(raw));
	ASSERT_TRUE(result.is_err());
}

// ============================================================================
// HTTP Response Parsing Tests
// ============================================================================

class HttpParserResponseTest : public ::testing::Test
{
};

TEST_F(HttpParserResponseTest, ParseSimple200Response)
{
	std::string raw = "HTTP/1.1 200 OK\r\n"
	                  "Content-Type: text/html\r\n"
	                  "\r\n"
	                  "<html>Hello</html>";

	auto result = http_parser::parse_response(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& resp = result.value();
	EXPECT_EQ(resp.version, http_version::HTTP_1_1);
	EXPECT_EQ(resp.status_code, 200);
	EXPECT_EQ(resp.status_message, "OK");
	EXPECT_EQ(resp.headers.at("Content-Type"), "text/html");
	EXPECT_EQ(resp.get_body_string(), "<html>Hello</html>");
}

TEST_F(HttpParserResponseTest, Parse404Response)
{
	std::string raw = "HTTP/1.1 404 Not Found\r\n"
	                  "\r\n";

	auto result = http_parser::parse_response(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& resp = result.value();
	EXPECT_EQ(resp.status_code, 404);
	EXPECT_EQ(resp.status_message, "Not Found");
}

TEST_F(HttpParserResponseTest, ParseResponseWithMultipleHeaders)
{
	std::string raw = "HTTP/1.1 200 OK\r\n"
	                  "Content-Type: application/json\r\n"
	                  "Content-Length: 13\r\n"
	                  "X-Request-Id: abc-123\r\n"
	                  "\r\n"
	                  "{\"status\":\"ok\"}";

	auto result = http_parser::parse_response(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& resp = result.value();
	EXPECT_EQ(resp.headers.size(), 3u);
	EXPECT_EQ(resp.headers.at("X-Request-Id"), "abc-123");
}

TEST_F(HttpParserResponseTest, ParseResponseFromByteVector)
{
	std::string raw = "HTTP/1.1 204 No Content\r\n\r\n";
	std::vector<uint8_t> data(raw.begin(), raw.end());

	auto result = http_parser::parse_response(data);
	ASSERT_TRUE(result.is_ok());

	const auto& resp = result.value();
	EXPECT_EQ(resp.status_code, 204);
	EXPECT_TRUE(resp.body.empty());
}

TEST_F(HttpParserResponseTest, ParseHTTP10Response)
{
	std::string raw = "HTTP/1.0 200 OK\r\n\r\n";

	auto result = http_parser::parse_response(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().version, http_version::HTTP_1_0);
}

TEST_F(HttpParserResponseTest, ParseStatusLineWithoutMessage)
{
	std::string raw = "HTTP/1.1 200\r\n\r\n";

	auto result = http_parser::parse_response(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().status_code, 200);
	// Status message should default to "OK"
	EXPECT_FALSE(result.value().status_message.empty());
}

TEST_F(HttpParserResponseTest, ParseResponseHeadersWithoutBody)
{
	std::string raw = "HTTP/1.1 301 Moved Permanently\r\n"
	                  "Location: https://example.com\r\n";

	auto result = http_parser::parse_response(std::string_view(raw));
	ASSERT_TRUE(result.is_ok());

	const auto& resp = result.value();
	EXPECT_EQ(resp.status_code, 301);
	EXPECT_EQ(resp.headers.at("Location"), "https://example.com");
}

// ---- Error Cases ----

TEST_F(HttpParserResponseTest, EmptyDataReturnsError)
{
	auto result = http_parser::parse_response(std::string_view(""));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserResponseTest, InvalidStatusLineReturnsError)
{
	auto result = http_parser::parse_response(std::string_view("INVALID\r\n\r\n"));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserResponseTest, InvalidStatusCodeReturnsError)
{
	auto result = http_parser::parse_response(std::string_view("HTTP/1.1 abc OK\r\n\r\n"));
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserResponseTest, InvalidVersionReturnsError)
{
	auto result = http_parser::parse_response(std::string_view("HTTP/3.0 200 OK\r\n\r\n"));
	ASSERT_TRUE(result.is_err());
}

// ============================================================================
// Serialization Tests
// ============================================================================

class HttpParserSerializeTest : public ::testing::Test
{
};

TEST_F(HttpParserSerializeTest, SerializeSimpleGetRequest)
{
	http_request req;
	req.method = http_method::HTTP_GET;
	req.uri = "/index.html";
	req.version = http_version::HTTP_1_1;
	req.headers["Host"] = "example.com";

	auto bytes = http_parser::serialize_request(req);
	std::string result(bytes.begin(), bytes.end());

	EXPECT_NE(result.find("GET /index.html HTTP/1.1\r\n"), std::string::npos);
	EXPECT_NE(result.find("Host: example.com\r\n"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, SerializePostRequestWithBody)
{
	http_request req;
	req.method = http_method::HTTP_POST;
	req.uri = "/api/data";
	req.version = http_version::HTTP_1_1;
	req.headers["Content-Type"] = "application/json";
	req.set_body_string("{\"key\":\"value\"}");

	auto bytes = http_parser::serialize_request(req);
	std::string result(bytes.begin(), bytes.end());

	EXPECT_NE(result.find("POST /api/data HTTP/1.1\r\n"), std::string::npos);
	EXPECT_NE(result.find("{\"key\":\"value\"}"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, SerializeRequestWithQueryParams)
{
	http_request req;
	req.method = http_method::HTTP_GET;
	req.uri = "/search";
	req.version = http_version::HTTP_1_1;
	req.query_params["q"] = "test";
	req.query_params["page"] = "1";

	auto bytes = http_parser::serialize_request(req);
	std::string result(bytes.begin(), bytes.end());

	// Query params should appear in the request line
	EXPECT_NE(result.find("/search?"), std::string::npos);
	EXPECT_NE(result.find("q=test"), std::string::npos);
	EXPECT_NE(result.find("page=1"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, SerializeSimple200Response)
{
	http_response resp;
	resp.version = http_version::HTTP_1_1;
	resp.status_code = 200;
	resp.status_message = "OK";
	resp.headers["Content-Type"] = "text/plain";

	auto bytes = http_parser::serialize_response(resp);
	std::string result(bytes.begin(), bytes.end());

	EXPECT_NE(result.find("HTTP/1.1 200 OK\r\n"), std::string::npos);
	EXPECT_NE(result.find("Content-Type: text/plain\r\n"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, SerializeResponseWithBody)
{
	http_response resp;
	resp.version = http_version::HTTP_1_1;
	resp.status_code = 200;
	resp.status_message = "OK";
	resp.set_body_string("Hello, World!");

	auto bytes = http_parser::serialize_response(resp);
	std::string result(bytes.begin(), bytes.end());

	EXPECT_NE(result.find("Hello, World!"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, SerializeChunkedResponse)
{
	http_response resp;
	resp.version = http_version::HTTP_1_1;
	resp.status_code = 200;
	resp.status_message = "OK";
	resp.use_chunked_encoding = true;
	resp.set_body_string("Chunked body content");

	auto bytes = http_parser::serialize_response(resp);
	std::string result(bytes.begin(), bytes.end());

	// Should contain Transfer-Encoding: chunked header
	EXPECT_NE(result.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
	// Should end with final chunk marker "0\r\n\r\n"
	EXPECT_NE(result.find("0\r\n\r\n"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, ChunkedResponseRemovesContentLength)
{
	http_response resp;
	resp.version = http_version::HTTP_1_1;
	resp.status_code = 200;
	resp.status_message = "OK";
	resp.use_chunked_encoding = true;
	resp.headers["Content-Length"] = "100";
	resp.set_body_string("data");

	auto bytes = http_parser::serialize_response(resp);
	std::string result(bytes.begin(), bytes.end());

	// Content-Length should be stripped for chunked encoding
	EXPECT_EQ(result.find("Content-Length"), std::string::npos);
	EXPECT_NE(result.find("Transfer-Encoding: chunked"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, ChunkedEncodingOnlyForHTTP11)
{
	http_response resp;
	resp.version = http_version::HTTP_1_0;
	resp.status_code = 200;
	resp.status_message = "OK";
	resp.use_chunked_encoding = true;
	resp.set_body_string("data");

	auto bytes = http_parser::serialize_response(resp);
	std::string result(bytes.begin(), bytes.end());

	// HTTP/1.0 should not use chunked encoding
	EXPECT_EQ(result.find("Transfer-Encoding: chunked"), std::string::npos);
}

TEST_F(HttpParserSerializeTest, SerializeResponseWithSetCookie)
{
	http_response resp;
	resp.version = http_version::HTTP_1_1;
	resp.status_code = 200;
	resp.status_message = "OK";
	resp.set_cookie("session_id", "abc123", "/", 3600, true, true, "Strict");

	auto bytes = http_parser::serialize_response(resp);
	std::string result(bytes.begin(), bytes.end());

	EXPECT_NE(result.find("Set-Cookie: "), std::string::npos);
	EXPECT_NE(result.find("session_id=abc123"), std::string::npos);
}

// ============================================================================
// Round-Trip Tests
// ============================================================================

class HttpParserRoundTripTest : public ::testing::Test
{
};

TEST_F(HttpParserRoundTripTest, RequestSerializeAndParse)
{
	http_request original;
	original.method = http_method::HTTP_POST;
	original.uri = "/api/data";
	original.version = http_version::HTTP_1_1;
	original.headers["Host"] = "example.com";
	original.headers["Content-Type"] = "text/plain";
	original.set_body_string("Hello, World!");

	auto bytes = http_parser::serialize_request(original);
	auto result = http_parser::parse_request(bytes);
	ASSERT_TRUE(result.is_ok());

	const auto& parsed = result.value();
	EXPECT_EQ(parsed.method, original.method);
	EXPECT_EQ(parsed.uri, original.uri);
	EXPECT_EQ(parsed.version, original.version);
	EXPECT_EQ(parsed.get_body_string(), "Hello, World!");
	EXPECT_EQ(parsed.headers.at("Host"), "example.com");
}

TEST_F(HttpParserRoundTripTest, ResponseSerializeAndParse)
{
	http_response original;
	original.version = http_version::HTTP_1_1;
	original.status_code = 200;
	original.status_message = "OK";
	original.headers["Content-Type"] = "application/json";
	original.set_body_string("{\"status\":\"ok\"}");

	auto bytes = http_parser::serialize_response(original);
	auto result = http_parser::parse_response(bytes);
	ASSERT_TRUE(result.is_ok());

	const auto& parsed = result.value();
	EXPECT_EQ(parsed.status_code, original.status_code);
	EXPECT_EQ(parsed.status_message, original.status_message);
	EXPECT_EQ(parsed.version, original.version);
	EXPECT_EQ(parsed.get_body_string(), "{\"status\":\"ok\"}");
}

TEST_F(HttpParserRoundTripTest, RequestWithQueryParamsRoundTrip)
{
	http_request original;
	original.method = http_method::HTTP_GET;
	original.uri = "/search";
	original.version = http_version::HTTP_1_1;
	original.query_params["q"] = "test";
	original.query_params["limit"] = "10";

	auto bytes = http_parser::serialize_request(original);
	auto result = http_parser::parse_request(bytes);
	ASSERT_TRUE(result.is_ok());

	const auto& parsed = result.value();
	EXPECT_EQ(parsed.uri, "/search");
	EXPECT_EQ(parsed.query_params.at("q"), "test");
	EXPECT_EQ(parsed.query_params.at("limit"), "10");
}

// ============================================================================
// Cookie Parsing Tests
// ============================================================================

class HttpParserCookieTest : public ::testing::Test
{
};

TEST_F(HttpParserCookieTest, ParseSingleCookie)
{
	http_request req;
	req.headers["Cookie"] = "session_id=abc123";

	http_parser::parse_cookies(req);

	ASSERT_EQ(req.cookies.size(), 1u);
	EXPECT_EQ(req.cookies["session_id"], "abc123");
}

TEST_F(HttpParserCookieTest, ParseMultipleCookies)
{
	http_request req;
	req.headers["Cookie"] = "session_id=abc123; user=alice; theme=dark";

	http_parser::parse_cookies(req);

	ASSERT_EQ(req.cookies.size(), 3u);
	EXPECT_EQ(req.cookies["session_id"], "abc123");
	EXPECT_EQ(req.cookies["user"], "alice");
	EXPECT_EQ(req.cookies["theme"], "dark");
}

TEST_F(HttpParserCookieTest, ParseCookiesWithExtraWhitespace)
{
	http_request req;
	req.headers["Cookie"] = "  key1=val1 ;  key2=val2  ";

	http_parser::parse_cookies(req);

	EXPECT_EQ(req.cookies["key1"], "val1");
	EXPECT_EQ(req.cookies["key2"], "val2");
}

TEST_F(HttpParserCookieTest, NoCookieHeaderDoesNothing)
{
	http_request req;
	// No Cookie header set

	http_parser::parse_cookies(req);

	EXPECT_TRUE(req.cookies.empty());
}

TEST_F(HttpParserCookieTest, EmptyCookieHeader)
{
	http_request req;
	req.headers["Cookie"] = "";

	http_parser::parse_cookies(req);

	EXPECT_TRUE(req.cookies.empty());
}

// ============================================================================
// Multipart Form Data Tests
// ============================================================================

class HttpParserMultipartTest : public ::testing::Test
{
};

TEST_F(HttpParserMultipartTest, ParseSimpleFormField)
{
	http_request req;
	req.headers["Content-Type"] = "multipart/form-data; boundary=----boundary123";

	std::string body = "------boundary123\r\n"
	                   "Content-Disposition: form-data; name=\"field1\"\r\n"
	                   "\r\n"
	                   "value1\r\n"
	                   "------boundary123--\r\n";
	req.body.assign(body.begin(), body.end());

	auto result = http_parser::parse_multipart_form_data(req);
	ASSERT_TRUE(result.is_ok());

	ASSERT_EQ(req.form_data.size(), 1u);
	EXPECT_EQ(req.form_data["field1"], "value1");
}

TEST_F(HttpParserMultipartTest, ParseFileUpload)
{
	http_request req;
	req.headers["Content-Type"] = "multipart/form-data; boundary=----boundary123";

	std::string body = "------boundary123\r\n"
	                   "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
	                   "Content-Type: text/plain\r\n"
	                   "\r\n"
	                   "file content here\r\n"
	                   "------boundary123--\r\n";
	req.body.assign(body.begin(), body.end());

	auto result = http_parser::parse_multipart_form_data(req);
	ASSERT_TRUE(result.is_ok());

	ASSERT_EQ(req.files.size(), 1u);
	ASSERT_TRUE(req.files.count("file") > 0);

	const auto& file = req.files.at("file");
	EXPECT_EQ(file.field_name, "file");
	EXPECT_EQ(file.filename, "test.txt");
	EXPECT_EQ(file.content_type, "text/plain");

	std::string file_content(file.content.begin(), file.content.end());
	EXPECT_EQ(file_content, "file content here");
}

TEST_F(HttpParserMultipartTest, ParseMultipleFieldsAndFile)
{
	http_request req;
	req.headers["Content-Type"] = "multipart/form-data; boundary=----boundary123";

	std::string body = "------boundary123\r\n"
	                   "Content-Disposition: form-data; name=\"name\"\r\n"
	                   "\r\n"
	                   "alice\r\n"
	                   "------boundary123\r\n"
	                   "Content-Disposition: form-data; name=\"avatar\"; filename=\"photo.png\"\r\n"
	                   "Content-Type: image/png\r\n"
	                   "\r\n"
	                   "PNG_DATA\r\n"
	                   "------boundary123--\r\n";
	req.body.assign(body.begin(), body.end());

	auto result = http_parser::parse_multipart_form_data(req);
	ASSERT_TRUE(result.is_ok());

	EXPECT_EQ(req.form_data["name"], "alice");
	ASSERT_TRUE(req.files.count("avatar") > 0);
	EXPECT_EQ(req.files.at("avatar").filename, "photo.png");
	EXPECT_EQ(req.files.at("avatar").content_type, "image/png");
}

TEST_F(HttpParserMultipartTest, MissingContentTypeReturnsError)
{
	http_request req;
	// No Content-Type header

	auto result = http_parser::parse_multipart_form_data(req);
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpParserMultipartTest, MissingBoundaryReturnsError)
{
	http_request req;
	req.headers["Content-Type"] = "multipart/form-data";
	// No boundary parameter

	auto result = http_parser::parse_multipart_form_data(req);
	ASSERT_TRUE(result.is_err());
}

// ============================================================================
// HTTP Types Helper Tests
// ============================================================================

class HttpTypesTest : public ::testing::Test
{
};

TEST_F(HttpTypesTest, HttpMethodToStringRoundTrip)
{
	auto str = http_method_to_string(http_method::HTTP_GET);
	EXPECT_EQ(str, "GET");

	auto result = string_to_http_method("GET");
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_GET);
}

TEST_F(HttpTypesTest, AllMethodsConvertCorrectly)
{
	EXPECT_EQ(http_method_to_string(http_method::HTTP_POST), "POST");
	EXPECT_EQ(http_method_to_string(http_method::HTTP_PUT), "PUT");
	EXPECT_EQ(http_method_to_string(http_method::HTTP_DELETE), "DELETE");
	EXPECT_EQ(http_method_to_string(http_method::HTTP_HEAD), "HEAD");
	EXPECT_EQ(http_method_to_string(http_method::HTTP_OPTIONS), "OPTIONS");
	EXPECT_EQ(http_method_to_string(http_method::HTTP_PATCH), "PATCH");
}

TEST_F(HttpTypesTest, InvalidMethodStringReturnsError)
{
	auto result = string_to_http_method("INVALID");
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpTypesTest, HttpVersionToStringRoundTrip)
{
	EXPECT_EQ(http_version_to_string(http_version::HTTP_1_0), "HTTP/1.0");
	EXPECT_EQ(http_version_to_string(http_version::HTTP_1_1), "HTTP/1.1");
	EXPECT_EQ(http_version_to_string(http_version::HTTP_2_0), "HTTP/2.0");

	auto result = string_to_http_version("HTTP/1.1");
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_version::HTTP_1_1);
}

TEST_F(HttpTypesTest, InvalidVersionStringReturnsError)
{
	auto result = string_to_http_version("HTTP/3.0");
	ASSERT_TRUE(result.is_err());
}

TEST_F(HttpTypesTest, GetStatusMessageForCommonCodes)
{
	EXPECT_EQ(get_status_message(200), "OK");
	EXPECT_EQ(get_status_message(404), "Not Found");
	EXPECT_EQ(get_status_message(500), "Internal Server Error");
}

TEST_F(HttpTypesTest, RequestBodyStringHelpers)
{
	http_request req;
	req.set_body_string("test body");
	EXPECT_EQ(req.get_body_string(), "test body");
	EXPECT_EQ(req.body.size(), 9u);
}

TEST_F(HttpTypesTest, ResponseBodyStringHelpers)
{
	http_response resp;
	resp.set_body_string("response body");
	EXPECT_EQ(resp.get_body_string(), "response body");
}
