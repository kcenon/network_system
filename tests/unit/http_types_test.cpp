/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/http_types.h"
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace kcenon::network::internal;

/**
 * @file http_types_test.cpp
 * @brief Unit tests for HTTP types, cookie, and helper functions
 *
 * Tests validate:
 * - cookie::to_header_value() with all attribute combinations
 * - http_response::set_cookie() builder method
 * - http_method_to_string() for CONNECT and TRACE
 * - string_to_http_method() case-insensitive parsing
 * - string_to_http_version() alternative format ("HTTP/2")
 * - get_status_message() comprehensive coverage (1xx-5xx + unknown)
 * - http_request and http_response default construction values
 * - http_request get_header/set_header case-insensitive behavior
 * - http_response get_header/set_header case-insensitive behavior
 * - multipart_file struct defaults
 * - cookie struct defaults
 */

// ============================================================================
// cookie::to_header_value() Tests
// ============================================================================

class CookieToHeaderValueTest : public ::testing::Test
{
};

TEST_F(CookieToHeaderValueTest, NameAndValueOnly)
{
	cookie c;
	c.name = "session";
	c.value = "abc123";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "session=abc123");
}

TEST_F(CookieToHeaderValueTest, WithPath)
{
	cookie c;
	c.name = "token";
	c.value = "xyz";
	c.path = "/api";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "token=xyz; Path=/api");
}

TEST_F(CookieToHeaderValueTest, WithDomain)
{
	cookie c;
	c.name = "lang";
	c.value = "en";
	c.domain = ".example.com";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "lang=en; Domain=.example.com");
}

TEST_F(CookieToHeaderValueTest, WithExpires)
{
	cookie c;
	c.name = "pref";
	c.value = "dark";
	c.expires = "Thu, 01 Jan 2026 00:00:00 GMT";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "pref=dark; Expires=Thu, 01 Jan 2026 00:00:00 GMT");
}

TEST_F(CookieToHeaderValueTest, WithMaxAgeZero)
{
	cookie c;
	c.name = "old";
	c.value = "delete";
	c.max_age = 0;

	auto result = c.to_header_value();

	EXPECT_EQ(result, "old=delete; Max-Age=0");
}

TEST_F(CookieToHeaderValueTest, WithMaxAgePositive)
{
	cookie c;
	c.name = "session";
	c.value = "abc";
	c.max_age = 3600;

	auto result = c.to_header_value();

	EXPECT_EQ(result, "session=abc; Max-Age=3600");
}

TEST_F(CookieToHeaderValueTest, SessionCookieOmitsMaxAge)
{
	cookie c;
	c.name = "temp";
	c.value = "val";
	// max_age defaults to -1 (session cookie)

	auto result = c.to_header_value();

	EXPECT_EQ(result, "temp=val");
	EXPECT_EQ(result.find("Max-Age"), std::string::npos);
}

TEST_F(CookieToHeaderValueTest, WithHttpOnly)
{
	cookie c;
	c.name = "sid";
	c.value = "secret";
	c.http_only = true;

	auto result = c.to_header_value();

	EXPECT_EQ(result, "sid=secret; HttpOnly");
}

TEST_F(CookieToHeaderValueTest, WithSecure)
{
	cookie c;
	c.name = "sid";
	c.value = "secret";
	c.secure = true;

	auto result = c.to_header_value();

	EXPECT_EQ(result, "sid=secret; Secure");
}

TEST_F(CookieToHeaderValueTest, WithSameSiteStrict)
{
	cookie c;
	c.name = "csrf";
	c.value = "token123";
	c.same_site = "Strict";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "csrf=token123; SameSite=Strict");
}

TEST_F(CookieToHeaderValueTest, WithSameSiteLax)
{
	cookie c;
	c.name = "pref";
	c.value = "val";
	c.same_site = "Lax";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "pref=val; SameSite=Lax");
}

TEST_F(CookieToHeaderValueTest, WithSameSiteNone)
{
	cookie c;
	c.name = "track";
	c.value = "id";
	c.same_site = "None";
	c.secure = true;

	auto result = c.to_header_value();

	EXPECT_EQ(result, "track=id; Secure; SameSite=None");
}

TEST_F(CookieToHeaderValueTest, AllAttributesCombined)
{
	cookie c;
	c.name = "session_id";
	c.value = "abc123";
	c.path = "/";
	c.domain = ".example.com";
	c.expires = "Thu, 01 Jan 2026 00:00:00 GMT";
	c.max_age = 86400;
	c.http_only = true;
	c.secure = true;
	c.same_site = "Strict";

	auto result = c.to_header_value();

	EXPECT_EQ(result,
			  "session_id=abc123; Path=/; Domain=.example.com; "
			  "Expires=Thu, 01 Jan 2026 00:00:00 GMT; Max-Age=86400; "
			  "HttpOnly; Secure; SameSite=Strict");
}

TEST_F(CookieToHeaderValueTest, EmptyValue)
{
	cookie c;
	c.name = "cleared";
	c.value = "";

	auto result = c.to_header_value();

	EXPECT_EQ(result, "cleared=");
}

// ============================================================================
// cookie Struct Default Tests
// ============================================================================

class CookieDefaultsTest : public ::testing::Test
{
};

TEST_F(CookieDefaultsTest, DefaultValues)
{
	cookie c;

	EXPECT_TRUE(c.name.empty());
	EXPECT_TRUE(c.value.empty());
	EXPECT_TRUE(c.path.empty());
	EXPECT_TRUE(c.domain.empty());
	EXPECT_TRUE(c.expires.empty());
	EXPECT_EQ(c.max_age, -1);
	EXPECT_FALSE(c.secure);
	EXPECT_FALSE(c.http_only);
	EXPECT_TRUE(c.same_site.empty());
}

// ============================================================================
// http_response::set_cookie() Tests
// ============================================================================

class HttpResponseSetCookieTest : public ::testing::Test
{
};

TEST_F(HttpResponseSetCookieTest, DefaultParameters)
{
	http_response resp;
	resp.set_cookie("sid", "abc123");

	ASSERT_EQ(resp.set_cookies.size(), 1);
	const auto& c = resp.set_cookies[0];
	EXPECT_EQ(c.name, "sid");
	EXPECT_EQ(c.value, "abc123");
	EXPECT_EQ(c.path, "/");
	EXPECT_EQ(c.max_age, -1);
	EXPECT_TRUE(c.http_only);
	EXPECT_FALSE(c.secure);
	EXPECT_TRUE(c.same_site.empty());
}

TEST_F(HttpResponseSetCookieTest, CustomParameters)
{
	http_response resp;
	resp.set_cookie("token", "xyz", "/api", 7200, false, true, "None");

	ASSERT_EQ(resp.set_cookies.size(), 1);
	const auto& c = resp.set_cookies[0];
	EXPECT_EQ(c.name, "token");
	EXPECT_EQ(c.value, "xyz");
	EXPECT_EQ(c.path, "/api");
	EXPECT_EQ(c.max_age, 7200);
	EXPECT_FALSE(c.http_only);
	EXPECT_TRUE(c.secure);
	EXPECT_EQ(c.same_site, "None");
}

TEST_F(HttpResponseSetCookieTest, MultipleCookies)
{
	http_response resp;
	resp.set_cookie("a", "1");
	resp.set_cookie("b", "2");
	resp.set_cookie("c", "3");

	EXPECT_EQ(resp.set_cookies.size(), 3);
	EXPECT_EQ(resp.set_cookies[0].name, "a");
	EXPECT_EQ(resp.set_cookies[1].name, "b");
	EXPECT_EQ(resp.set_cookies[2].name, "c");
}

// ============================================================================
// http_method_to_string() Tests (CONNECT, TRACE)
// ============================================================================

class HttpMethodToStringTest : public ::testing::Test
{
};

TEST_F(HttpMethodToStringTest, ConnectMethod)
{
	EXPECT_EQ(http_method_to_string(http_method::HTTP_CONNECT), "CONNECT");
}

TEST_F(HttpMethodToStringTest, TraceMethod)
{
	EXPECT_EQ(http_method_to_string(http_method::HTTP_TRACE), "TRACE");
}

// ============================================================================
// string_to_http_method() Case-Insensitive Tests
// ============================================================================

class StringToHttpMethodTest : public ::testing::Test
{
};

TEST_F(StringToHttpMethodTest, LowercaseGet)
{
	auto result = string_to_http_method("get");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_GET);
}

TEST_F(StringToHttpMethodTest, MixedCasePost)
{
	auto result = string_to_http_method("Post");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_POST);
}

TEST_F(StringToHttpMethodTest, LowercaseDelete)
{
	auto result = string_to_http_method("delete");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_DELETE);
}

TEST_F(StringToHttpMethodTest, ConnectUppercase)
{
	auto result = string_to_http_method("CONNECT");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_CONNECT);
}

TEST_F(StringToHttpMethodTest, TraceLowercase)
{
	auto result = string_to_http_method("trace");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_TRACE);
}

TEST_F(StringToHttpMethodTest, PatchMixedCase)
{
	auto result = string_to_http_method("pAtCh");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_PATCH);
}

TEST_F(StringToHttpMethodTest, OptionsMixedCase)
{
	auto result = string_to_http_method("Options");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_OPTIONS);
}

TEST_F(StringToHttpMethodTest, HeadMixedCase)
{
	auto result = string_to_http_method("hEaD");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_HEAD);
}

TEST_F(StringToHttpMethodTest, PutLowercase)
{
	auto result = string_to_http_method("put");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_method::HTTP_PUT);
}

TEST_F(StringToHttpMethodTest, InvalidMethodReturnsError)
{
	auto result = string_to_http_method("INVALID");

	EXPECT_FALSE(result.is_ok());
}

TEST_F(StringToHttpMethodTest, EmptyStringReturnsError)
{
	auto result = string_to_http_method("");

	EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// string_to_http_version() Tests
// ============================================================================

class StringToHttpVersionTest : public ::testing::Test
{
};

TEST_F(StringToHttpVersionTest, Http10)
{
	auto result = string_to_http_version("HTTP/1.0");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_version::HTTP_1_0);
}

TEST_F(StringToHttpVersionTest, Http11)
{
	auto result = string_to_http_version("HTTP/1.1");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_version::HTTP_1_1);
}

TEST_F(StringToHttpVersionTest, Http20Full)
{
	auto result = string_to_http_version("HTTP/2.0");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_version::HTTP_2_0);
}

TEST_F(StringToHttpVersionTest, Http2Short)
{
	auto result = string_to_http_version("HTTP/2");

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), http_version::HTTP_2_0);
}

TEST_F(StringToHttpVersionTest, InvalidVersionReturnsError)
{
	auto result = string_to_http_version("HTTP/3.0");

	EXPECT_FALSE(result.is_ok());
}

TEST_F(StringToHttpVersionTest, EmptyStringReturnsError)
{
	auto result = string_to_http_version("");

	EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// get_status_message() Comprehensive Tests
// ============================================================================

class GetStatusMessageTest : public ::testing::Test
{
};

TEST_F(GetStatusMessageTest, Informational1xx)
{
	EXPECT_EQ(get_status_message(100), "Continue");
	EXPECT_EQ(get_status_message(101), "Switching Protocols");
}

TEST_F(GetStatusMessageTest, Success2xx)
{
	EXPECT_EQ(get_status_message(200), "OK");
	EXPECT_EQ(get_status_message(201), "Created");
	EXPECT_EQ(get_status_message(202), "Accepted");
	EXPECT_EQ(get_status_message(203), "Non-Authoritative Information");
	EXPECT_EQ(get_status_message(204), "No Content");
	EXPECT_EQ(get_status_message(205), "Reset Content");
	EXPECT_EQ(get_status_message(206), "Partial Content");
}

TEST_F(GetStatusMessageTest, Redirection3xx)
{
	EXPECT_EQ(get_status_message(300), "Multiple Choices");
	EXPECT_EQ(get_status_message(301), "Moved Permanently");
	EXPECT_EQ(get_status_message(302), "Found");
	EXPECT_EQ(get_status_message(303), "See Other");
	EXPECT_EQ(get_status_message(304), "Not Modified");
	EXPECT_EQ(get_status_message(307), "Temporary Redirect");
	EXPECT_EQ(get_status_message(308), "Permanent Redirect");
}

TEST_F(GetStatusMessageTest, ClientError4xx)
{
	EXPECT_EQ(get_status_message(400), "Bad Request");
	EXPECT_EQ(get_status_message(401), "Unauthorized");
	EXPECT_EQ(get_status_message(402), "Payment Required");
	EXPECT_EQ(get_status_message(403), "Forbidden");
	EXPECT_EQ(get_status_message(404), "Not Found");
	EXPECT_EQ(get_status_message(405), "Method Not Allowed");
	EXPECT_EQ(get_status_message(406), "Not Acceptable");
	EXPECT_EQ(get_status_message(407), "Proxy Authentication Required");
	EXPECT_EQ(get_status_message(408), "Request Timeout");
	EXPECT_EQ(get_status_message(409), "Conflict");
	EXPECT_EQ(get_status_message(410), "Gone");
	EXPECT_EQ(get_status_message(411), "Length Required");
	EXPECT_EQ(get_status_message(412), "Precondition Failed");
	EXPECT_EQ(get_status_message(413), "Payload Too Large");
	EXPECT_EQ(get_status_message(414), "URI Too Long");
	EXPECT_EQ(get_status_message(415), "Unsupported Media Type");
	EXPECT_EQ(get_status_message(416), "Range Not Satisfiable");
	EXPECT_EQ(get_status_message(417), "Expectation Failed");
	EXPECT_EQ(get_status_message(429), "Too Many Requests");
}

TEST_F(GetStatusMessageTest, ServerError5xx)
{
	EXPECT_EQ(get_status_message(500), "Internal Server Error");
	EXPECT_EQ(get_status_message(501), "Not Implemented");
	EXPECT_EQ(get_status_message(502), "Bad Gateway");
	EXPECT_EQ(get_status_message(503), "Service Unavailable");
	EXPECT_EQ(get_status_message(504), "Gateway Timeout");
	EXPECT_EQ(get_status_message(505), "HTTP Version Not Supported");
}

TEST_F(GetStatusMessageTest, UnknownStatusCode)
{
	EXPECT_EQ(get_status_message(0), "Unknown");
	EXPECT_EQ(get_status_message(999), "Unknown");
	EXPECT_EQ(get_status_message(418), "Unknown");
}

// ============================================================================
// http_request Default and Method Tests
// ============================================================================

class HttpRequestTest : public ::testing::Test
{
};

TEST_F(HttpRequestTest, DefaultValues)
{
	http_request req;

	EXPECT_EQ(req.method, http_method::HTTP_GET);
	EXPECT_TRUE(req.uri.empty());
	EXPECT_EQ(req.version, http_version::HTTP_1_1);
	EXPECT_TRUE(req.headers.empty());
	EXPECT_TRUE(req.body.empty());
	EXPECT_TRUE(req.query_params.empty());
	EXPECT_TRUE(req.cookies.empty());
	EXPECT_TRUE(req.form_data.empty());
	EXPECT_TRUE(req.files.empty());
}

TEST_F(HttpRequestTest, GetHeaderCaseInsensitive)
{
	http_request req;
	req.headers["Content-Type"] = "application/json";

	auto result1 = req.get_header("content-type");
	auto result2 = req.get_header("CONTENT-TYPE");
	auto result3 = req.get_header("Content-Type");

	ASSERT_TRUE(result1.has_value());
	ASSERT_TRUE(result2.has_value());
	ASSERT_TRUE(result3.has_value());
	EXPECT_EQ(result1.value(), "application/json");
	EXPECT_EQ(result2.value(), "application/json");
	EXPECT_EQ(result3.value(), "application/json");
}

TEST_F(HttpRequestTest, GetHeaderMissing)
{
	http_request req;

	auto result = req.get_header("X-Missing");

	EXPECT_FALSE(result.has_value());
}

TEST_F(HttpRequestTest, SetHeaderReplacesExisting)
{
	http_request req;
	req.headers["Content-Type"] = "text/plain";

	req.set_header("content-type", "application/json");

	// Old key should be removed, new key added
	auto result = req.get_header("content-type");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "application/json");
}

TEST_F(HttpRequestTest, SetBodyStringAndGetBodyString)
{
	http_request req;
	req.set_body_string("Hello, World!");

	EXPECT_EQ(req.get_body_string(), "Hello, World!");
	EXPECT_EQ(req.body.size(), 13);
}

TEST_F(HttpRequestTest, EmptyBodyString)
{
	http_request req;

	EXPECT_EQ(req.get_body_string(), "");
}

// ============================================================================
// http_response Default and Method Tests
// ============================================================================

class HttpResponseTest : public ::testing::Test
{
};

TEST_F(HttpResponseTest, DefaultValues)
{
	http_response resp;

	EXPECT_EQ(resp.status_code, 200);
	EXPECT_EQ(resp.status_message, "OK");
	EXPECT_EQ(resp.version, http_version::HTTP_1_1);
	EXPECT_TRUE(resp.headers.empty());
	EXPECT_TRUE(resp.body.empty());
	EXPECT_TRUE(resp.set_cookies.empty());
	EXPECT_FALSE(resp.use_chunked_encoding);
}

TEST_F(HttpResponseTest, GetHeaderCaseInsensitive)
{
	http_response resp;
	resp.headers["X-Custom-Header"] = "custom-value";

	auto result1 = resp.get_header("x-custom-header");
	auto result2 = resp.get_header("X-CUSTOM-HEADER");

	ASSERT_TRUE(result1.has_value());
	ASSERT_TRUE(result2.has_value());
	EXPECT_EQ(result1.value(), "custom-value");
	EXPECT_EQ(result2.value(), "custom-value");
}

TEST_F(HttpResponseTest, GetHeaderMissing)
{
	http_response resp;

	auto result = resp.get_header("X-Missing");

	EXPECT_FALSE(result.has_value());
}

TEST_F(HttpResponseTest, SetHeaderReplacesExisting)
{
	http_response resp;
	resp.headers["Content-Length"] = "100";

	resp.set_header("content-length", "200");

	auto result = resp.get_header("content-length");
	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result.value(), "200");
}

TEST_F(HttpResponseTest, SetBodyStringAndGetBodyString)
{
	http_response resp;
	resp.set_body_string("{\"status\":\"ok\"}");

	EXPECT_EQ(resp.get_body_string(), "{\"status\":\"ok\"}");
}

TEST_F(HttpResponseTest, EmptyBodyString)
{
	http_response resp;

	EXPECT_EQ(resp.get_body_string(), "");
}

// ============================================================================
// multipart_file Struct Default Tests
// ============================================================================

class MultipartFileTest : public ::testing::Test
{
};

TEST_F(MultipartFileTest, DefaultValues)
{
	multipart_file file;

	EXPECT_TRUE(file.field_name.empty());
	EXPECT_TRUE(file.filename.empty());
	EXPECT_TRUE(file.content_type.empty());
	EXPECT_TRUE(file.content.empty());
}
