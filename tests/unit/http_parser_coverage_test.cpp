// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

// Coverage-expansion tests for src/internal/http_parser.cpp targeting branches
// not reached by tests/unit/http_parser_test.cpp: URL decode truncation,
// query string delimiter edge cases, cookie malformed-pair handling, header
// edge cases, chunked encoding boundary conditions, multipart edge cases,
// and parse_request_line / parse_status_line error paths.
//
// Part of epic #953 (expand unit test coverage from 40% to 80%). Single-file
// sub-issue #1013.

#include "internal/http/http_parser.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace kcenon::network::internal;

namespace {

auto vec_from(std::string_view s) -> std::vector<uint8_t>
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

auto str_from(const std::vector<uint8_t>& bytes) -> std::string
{
    return std::string(bytes.begin(), bytes.end());
}

auto contains(const std::vector<uint8_t>& haystack, std::string_view needle) -> bool
{
    return str_from(haystack).find(needle) != std::string::npos;
}

}  // namespace

// ============================================================================
// url_encode coverage — safe character matrix, high-bit bytes, null byte
// ============================================================================

class HttpParserUrlEncodeCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserUrlEncodeCoverageTest, SafeHyphenPreserved)
{
    EXPECT_EQ(http_parser::url_encode("a-b"), "a-b");
}

TEST_F(HttpParserUrlEncodeCoverageTest, SafeUnderscorePreserved)
{
    EXPECT_EQ(http_parser::url_encode("a_b"), "a_b");
}

TEST_F(HttpParserUrlEncodeCoverageTest, SafeDotPreserved)
{
    EXPECT_EQ(http_parser::url_encode("a.b"), "a.b");
}

TEST_F(HttpParserUrlEncodeCoverageTest, SafeTildePreserved)
{
    EXPECT_EQ(http_parser::url_encode("a~b"), "a~b");
}

TEST_F(HttpParserUrlEncodeCoverageTest, HighBitByteEncoded)
{
    std::string input(1, static_cast<char>(0xFF));
    EXPECT_EQ(http_parser::url_encode(input), "%FF");
}

TEST_F(HttpParserUrlEncodeCoverageTest, NullByteEncoded)
{
    std::string input(1, '\0');
    EXPECT_EQ(http_parser::url_encode(input), "%00");
}

TEST_F(HttpParserUrlEncodeCoverageTest, LowByteEncodedWithLeadingZero)
{
    std::string input(1, '\x01');
    EXPECT_EQ(http_parser::url_encode(input), "%01");
}

TEST_F(HttpParserUrlEncodeCoverageTest, UppercaseHexDigitsInOutput)
{
    std::string input(1, '\xAB');
    auto result = http_parser::url_encode(input);
    EXPECT_EQ(result, "%AB");
    EXPECT_NE(result, "%ab");
}

TEST_F(HttpParserUrlEncodeCoverageTest, UnreservedRfc3986Preserved)
{
    EXPECT_EQ(http_parser::url_encode("abcXYZ0123456789-_.~"),
              "abcXYZ0123456789-_.~");
}

TEST_F(HttpParserUrlEncodeCoverageTest, ReservedCharactersEncoded)
{
    EXPECT_EQ(http_parser::url_encode("!*'();:@&=+$,/?#[]"),
              "%21%2A%27%28%29%3B%3A%40%26%3D%2B%24%2C%2F%3F%23%5B%5D");
}

TEST_F(HttpParserUrlEncodeCoverageTest, Utf8MultibyteByteByByte)
{
    // "가" in UTF-8 is EA B0 80
    std::string input = "\xEA\xB0\x80";
    EXPECT_EQ(http_parser::url_encode(input), "%EA%B0%80");
}

// ============================================================================
// url_decode coverage — truncation, hex case, high-bit and null bytes
// ============================================================================

class HttpParserUrlDecodeCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserUrlDecodeCoverageTest, TruncatedTrailingPercentDropped)
{
    // When input ends with '%' and no two chars follow, the '%' is silently
    // dropped because the `i + 2 < length` guard is false.
    EXPECT_EQ(http_parser::url_decode("foo%"), "foo");
}

TEST_F(HttpParserUrlDecodeCoverageTest, TruncatedPercentOneCharDropped)
{
    // 'foo%A' — only one char after '%', i+2 == length, guard false.
    EXPECT_EQ(http_parser::url_decode("foo%A"), "foo");
}

TEST_F(HttpParserUrlDecodeCoverageTest, LowercaseHexAccepted)
{
    // std::stoi(..., 16) accepts lowercase a-f.
    std::string expected(1, '\xAB');
    EXPECT_EQ(http_parser::url_decode("%ab"), expected);
}

TEST_F(HttpParserUrlDecodeCoverageTest, MixedHexCaseAccepted)
{
    std::string expected(1, '\xAB');
    EXPECT_EQ(http_parser::url_decode("%aB"), expected);
    EXPECT_EQ(http_parser::url_decode("%Ab"), expected);
}

TEST_F(HttpParserUrlDecodeCoverageTest, NullByteDecoded)
{
    std::string expected(1, '\0');
    EXPECT_EQ(http_parser::url_decode("%00"), expected);
}

TEST_F(HttpParserUrlDecodeCoverageTest, HighBitByteDecoded)
{
    std::string expected(1, '\xFF');
    EXPECT_EQ(http_parser::url_decode("%FF"), expected);
}

TEST_F(HttpParserUrlDecodeCoverageTest, PlusAtStart)
{
    EXPECT_EQ(http_parser::url_decode("+foo"), " foo");
}

TEST_F(HttpParserUrlDecodeCoverageTest, PlusAtEnd)
{
    EXPECT_EQ(http_parser::url_decode("foo+"), "foo ");
}

TEST_F(HttpParserUrlDecodeCoverageTest, OnlyPlusSigns)
{
    EXPECT_EQ(http_parser::url_decode("+++"), "   ");
}

TEST_F(HttpParserUrlDecodeCoverageTest, ConsecutivePercentEncoded)
{
    std::string expected = "\x41\x42\x43";  // ABC
    EXPECT_EQ(http_parser::url_decode("%41%42%43"), expected);
}

TEST_F(HttpParserUrlDecodeCoverageTest, RoundTripForEveryUnreservedByte)
{
    for (int b = 0; b < 256; ++b) {
        // Skip '+' because url_decode treats '+' as space (intentional
        // form-encoding behavior), breaking the simple round-trip.
        if (b == '+') continue;
        std::string input(1, static_cast<char>(b));
        auto encoded = http_parser::url_encode(input);
        auto decoded = http_parser::url_decode(encoded);
        EXPECT_EQ(decoded, input) << "byte=0x" << std::hex << b;
    }
}

// ============================================================================
// parse_query_string / build_query_string coverage
// ============================================================================

class HttpParserQueryStringCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserQueryStringCoverageTest, LeadingAmpersandProducesEmptyKey)
{
    auto params = http_parser::parse_query_string("&a=1");
    EXPECT_EQ(params["a"], "1");
    // Leading '&' creates an empty-key pair with no '=' -> params[""] = ""
    auto it = params.find("");
    EXPECT_NE(it, params.end());
}

TEST_F(HttpParserQueryStringCoverageTest, TrailingAmpersandProducesEmptyKey)
{
    auto params = http_parser::parse_query_string("a=1&");
    EXPECT_EQ(params["a"], "1");
    auto it = params.find("");
    EXPECT_NE(it, params.end());
}

TEST_F(HttpParserQueryStringCoverageTest, DoubleAmpersandSkipsEmptyMiddle)
{
    auto params = http_parser::parse_query_string("a=1&&b=2");
    EXPECT_EQ(params["a"], "1");
    EXPECT_EQ(params["b"], "2");
}

TEST_F(HttpParserQueryStringCoverageTest, EmptyKeyWithValue)
{
    auto params = http_parser::parse_query_string("=value");
    EXPECT_EQ(params[""], "value");
}

TEST_F(HttpParserQueryStringCoverageTest, MultipleEqualsKeepsFirstAsSeparator)
{
    auto params = http_parser::parse_query_string("a=b=c");
    EXPECT_EQ(params["a"], "b=c");
}

TEST_F(HttpParserQueryStringCoverageTest, UrlEncodedAmpersandPreservedInValue)
{
    auto params = http_parser::parse_query_string("a=%26other");
    EXPECT_EQ(params["a"], "&other");
}

TEST_F(HttpParserQueryStringCoverageTest, UrlEncodedEqualsPreservedInValue)
{
    auto params = http_parser::parse_query_string("a=%3Dvalue");
    EXPECT_EQ(params["a"], "=value");
}

TEST_F(HttpParserQueryStringCoverageTest, DuplicateKeyLastValueWins)
{
    auto params = http_parser::parse_query_string("k=1&k=2");
    EXPECT_EQ(params["k"], "2");
    EXPECT_EQ(params.size(), 1u);
}

TEST_F(HttpParserQueryStringCoverageTest, EmptyValueAfterEquals)
{
    auto params = http_parser::parse_query_string("a=");
    EXPECT_EQ(params["a"], "");
    EXPECT_EQ(params.size(), 1u);
}

TEST_F(HttpParserQueryStringCoverageTest, OnlyAmpersandsYieldsSingleEmptyKey)
{
    // "&&&" — getline with '&' delimiter produces three empty tokens, each
    // without '=' -> every one inserts/overwrites params[""] = "".
    auto params = http_parser::parse_query_string("&&&");
    EXPECT_EQ(params.size(), 1u);
    EXPECT_EQ(params[""], "");
}

TEST_F(HttpParserQueryStringCoverageTest, BuildEmptyMapReturnsEmptyString)
{
    std::map<std::string, std::string> params;
    EXPECT_EQ(http_parser::build_query_string(params), "");
}

TEST_F(HttpParserQueryStringCoverageTest, BuildEncodesKeyWithSpace)
{
    std::map<std::string, std::string> params = {{"a b", "1"}};
    EXPECT_EQ(http_parser::build_query_string(params), "a%20b=1");
}

TEST_F(HttpParserQueryStringCoverageTest, BuildEncodesValueWithSpecialChars)
{
    std::map<std::string, std::string> params = {{"k", "hello world&"}};
    EXPECT_EQ(http_parser::build_query_string(params), "k=hello%20world%26");
}

TEST_F(HttpParserQueryStringCoverageTest, BuildOmitsEqualsForEmptyValue)
{
    std::map<std::string, std::string> params = {{"a", ""}};
    // Current implementation emits "a" with no '=' when value is empty.
    EXPECT_EQ(http_parser::build_query_string(params), "a");
}

TEST_F(HttpParserQueryStringCoverageTest, BuildMultipleParamsJoinedByAmpersand)
{
    std::map<std::string, std::string> params = {{"a", "1"}, {"b", "2"}};
    auto result = http_parser::build_query_string(params);
    // std::map iterates in key order -> "a=1&b=2"
    EXPECT_EQ(result, "a=1&b=2");
}

TEST_F(HttpParserQueryStringCoverageTest, RoundTripWithEncodableChars)
{
    std::map<std::string, std::string> original = {
        {"name", "John Doe"},
        {"email", "john@example.com"},
        {"tags", "a&b&c"}};
    auto built = http_parser::build_query_string(original);
    auto parsed = http_parser::parse_query_string(built);
    EXPECT_EQ(parsed, original);
}

// ============================================================================
// parse_request_line error branches (exercised via parse_request)
// ============================================================================

class HttpParserRequestLineCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserRequestLineCoverageTest, NoSpacesReturnsError)
{
    auto result = http_parser::parse_request("GETPATHVERSION\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestLineCoverageTest, MissingVersionReturnsError)
{
    auto result = http_parser::parse_request("GET /path\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestLineCoverageTest, UnknownMethodReturnsError)
{
    auto result = http_parser::parse_request("FOOBAR /path HTTP/1.1\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestLineCoverageTest, UnknownVersionReturnsError)
{
    auto result = http_parser::parse_request("GET /path HTTP/9.9\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserRequestLineCoverageTest, UriWithMultipleQuestionMarksSplitsAtFirst)
{
    auto result = http_parser::parse_request("GET /p?a=1?b=2 HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().uri, "/p");
    // parse_query_string treats "a=1?b=2" as single token with '=' at pos 1.
    EXPECT_EQ(result.value().query_params["a"], "1?b=2");
}

TEST_F(HttpParserRequestLineCoverageTest, UriWithTrailingQuestionMarkEmptyQuery)
{
    auto result = http_parser::parse_request("GET /path? HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().uri, "/path");
    EXPECT_TRUE(result.value().query_params.empty());
}

TEST_F(HttpParserRequestLineCoverageTest, UriWithOnlyQuestionMark)
{
    auto result = http_parser::parse_request("GET ? HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().uri, "");
    EXPECT_TRUE(result.value().query_params.empty());
}

TEST_F(HttpParserRequestLineCoverageTest, EmptyQueryKeyPreserved)
{
    auto result = http_parser::parse_request("GET /path?=v HTTP/1.1\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().query_params[""], "v");
}

// ============================================================================
// parse_status_line error branches (exercised via parse_response)
// ============================================================================

class HttpParserStatusLineCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserStatusLineCoverageTest, NonNumericStatusCodeReturnsError)
{
    auto result = http_parser::parse_response("HTTP/1.1 ABC OK\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserStatusLineCoverageTest, NoSpacesInStatusLineReturnsError)
{
    auto result = http_parser::parse_response("HTTP/1.1\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserStatusLineCoverageTest, UnknownVersionReturnsError)
{
    auto result = http_parser::parse_response("HTTP/9.9 200 OK\r\n\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserStatusLineCoverageTest, StatusCodeZeroAccepted)
{
    auto result = http_parser::parse_response("HTTP/1.1 0 Bogus\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 0);
}

TEST_F(HttpParserStatusLineCoverageTest, ThreeDigitStatusCodeWithoutMessageUsesCanonical)
{
    auto result = http_parser::parse_response("HTTP/1.1 200\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 200);
    EXPECT_FALSE(result.value().status_message.empty());
}

TEST_F(HttpParserStatusLineCoverageTest, StatusMessageWithMultipleWordsPreserved)
{
    auto result = http_parser::parse_response(
        "HTTP/1.1 200 OK All Good\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 200);
    EXPECT_EQ(result.value().status_message, "OK All Good");
}

TEST_F(HttpParserStatusLineCoverageTest, LargeStatusCodeStillParses)
{
    auto result = http_parser::parse_response("HTTP/1.1 99999 X\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 99999);
}

TEST_F(HttpParserStatusLineCoverageTest, EmptyStatusMessageFallsBackToCanonical)
{
    auto result = http_parser::parse_response("HTTP/1.1 404 \r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 404);
    // When status message is trimmed-empty, the default canonical text is used.
    EXPECT_FALSE(result.value().status_message.empty());
}

TEST_F(HttpParserStatusLineCoverageTest, InformationalStatus1xx)
{
    auto result = http_parser::parse_response("HTTP/1.1 100 Continue\r\n\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 100);
    EXPECT_EQ(result.value().status_message, "Continue");
}

// ============================================================================
// parse_headers edge cases (exercised via parse_request)
// ============================================================================

class HttpParserHeadersCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserHeadersCoverageTest, HeaderWithMultipleColonsKeepsValueAsIs)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X-Forward: proto: https\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers["X-Forward"], "proto: https");
}

TEST_F(HttpParserHeadersCoverageTest, HeaderWithEmptyValueAfterColon)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X-Empty:\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers["X-Empty"], "");
}

TEST_F(HttpParserHeadersCoverageTest, HeaderWithValueSpaces)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X-Spaces:    hello   \r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers["X-Spaces"], "hello");
}

TEST_F(HttpParserHeadersCoverageTest, HeaderWithOnlyColonEmptyNameAndValue)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        ":\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    auto it = result.value().headers.find("");
    EXPECT_NE(it, result.value().headers.end());
}

TEST_F(HttpParserHeadersCoverageTest, HeaderWithoutColonReturnsError)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "NotAnHeaderLine\r\n"
        "\r\n");
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserHeadersCoverageTest, DuplicateHeaderLastWinsInMap)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X-Dup: first\r\n"
        "X-Dup: second\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers["X-Dup"], "second");
}

TEST_F(HttpParserHeadersCoverageTest, ManyHeadersParsedInOrder)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "A: 1\r\n"
        "B: 2\r\n"
        "C: 3\r\n"
        "D: 4\r\n"
        "E: 5\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers.size(), 5u);
}

TEST_F(HttpParserHeadersCoverageTest, HeaderFollowedByOnlyHeadersNoBody)
{
    // No "\r\n\r\n" terminator -- all remaining data treated as headers block.
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X: y\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().headers["X"], "y");
    EXPECT_TRUE(result.value().body.empty());
}

// ============================================================================
// parse_cookies edge cases
// ============================================================================

class HttpParserCookieCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserCookieCoverageTest, PairWithoutEqualsIgnored)
{
    http_request request;
    request.headers["Cookie"] = "just_name";
    http_parser::parse_cookies(request);
    EXPECT_TRUE(request.cookies.empty());
}

TEST_F(HttpParserCookieCoverageTest, ValueWithEqualsSignKeptPastFirstEquals)
{
    http_request request;
    request.headers["Cookie"] = "a=b=c";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["a"], "b=c");
}

TEST_F(HttpParserCookieCoverageTest, LeadingSemicolonSkipsEmpty)
{
    http_request request;
    request.headers["Cookie"] = "; a=b";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["a"], "b");
    EXPECT_EQ(request.cookies.size(), 1u);
}

TEST_F(HttpParserCookieCoverageTest, TrailingSemicolonIgnoresEmpty)
{
    http_request request;
    request.headers["Cookie"] = "a=b;";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["a"], "b");
    EXPECT_EQ(request.cookies.size(), 1u);
}

TEST_F(HttpParserCookieCoverageTest, DoubleSemicolonSkipsEmptyBetween)
{
    http_request request;
    request.headers["Cookie"] = "a=b;; c=d";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["a"], "b");
    EXPECT_EQ(request.cookies["c"], "d");
    EXPECT_EQ(request.cookies.size(), 2u);
}

TEST_F(HttpParserCookieCoverageTest, TabWhitespaceStripped)
{
    http_request request;
    request.headers["Cookie"] = "a=b;\tc=d";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["a"], "b");
    EXPECT_EQ(request.cookies["c"], "d");
}

TEST_F(HttpParserCookieCoverageTest, QuotedValueNotStripped)
{
    http_request request;
    request.headers["Cookie"] = "a=\"quoted\"";
    http_parser::parse_cookies(request);
    // parse_cookies does not strip surrounding quotes in value.
    EXPECT_EQ(request.cookies["a"], "\"quoted\"");
}

TEST_F(HttpParserCookieCoverageTest, EmptyValueAfterEquals)
{
    http_request request;
    request.headers["Cookie"] = "a=";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["a"], "");
}

TEST_F(HttpParserCookieCoverageTest, EmptyNameBeforeEqualsIgnored)
{
    http_request request;
    request.headers["Cookie"] = "=value";
    http_parser::parse_cookies(request);
    EXPECT_TRUE(request.cookies.empty());
}

TEST_F(HttpParserCookieCoverageTest, MultipleNameValuePairsParsed)
{
    http_request request;
    request.headers["Cookie"] = "sid=abc; pref=dark; lang=en-US";
    http_parser::parse_cookies(request);
    EXPECT_EQ(request.cookies["sid"], "abc");
    EXPECT_EQ(request.cookies["pref"], "dark");
    EXPECT_EQ(request.cookies["lang"], "en-US");
    EXPECT_EQ(request.cookies.size(), 3u);
}

// ============================================================================
// parse_multipart_form_data edge cases
// ============================================================================

class HttpParserMultipartCoverageTest : public ::testing::Test
{
protected:
    static auto make_request(const std::string& body,
                              const std::string& content_type) -> http_request
    {
        http_request req;
        req.headers["Content-Type"] = content_type;
        req.body = vec_from(body);
        return req;
    }
};

TEST_F(HttpParserMultipartCoverageTest, PartWithoutContentDispositionSkipped)
{
    std::string body =
        "--BOUNDARY\r\n"
        "X-Custom: value\r\n"
        "\r\n"
        "data without disposition\r\n"
        "--BOUNDARY--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=BOUNDARY");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(req.form_data.empty());
    EXPECT_TRUE(req.files.empty());
}

TEST_F(HttpParserMultipartCoverageTest, BoundaryWithQuotesStripped)
{
    std::string body =
        "--ABC\r\n"
        "Content-Disposition: form-data; name=\"field\"\r\n"
        "\r\n"
        "value\r\n"
        "--ABC--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=\"ABC\"");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(req.form_data["field"], "value");
}

TEST_F(HttpParserMultipartCoverageTest, EmptyBodyYieldsNoFormData)
{
    auto req = make_request("", "multipart/form-data; boundary=X");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(req.form_data.empty());
    EXPECT_TRUE(req.files.empty());
}

TEST_F(HttpParserMultipartCoverageTest, BodyWithoutAnyBoundaryYieldsNoFormData)
{
    auto req = make_request("garbage with no boundary markers",
                            "multipart/form-data; boundary=X");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(req.form_data.empty());
}

TEST_F(HttpParserMultipartCoverageTest, EmptyFieldValueAccepted)
{
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"empty\"\r\n"
        "\r\n"
        "\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    auto it = req.form_data.find("empty");
    ASSERT_NE(it, req.form_data.end());
    EXPECT_EQ(it->second, "");
}

TEST_F(HttpParserMultipartCoverageTest, FileUploadWithoutContentTypeUsesDefault)
{
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"a.bin\"\r\n"
        "\r\n"
        "raw bytes\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    ASSERT_EQ(req.files.count("f"), 1u);
    EXPECT_EQ(req.files["f"].filename, "a.bin");
    EXPECT_EQ(req.files["f"].content_type, "application/octet-stream");
    EXPECT_EQ(str_from(req.files["f"].content), "raw bytes");
}

TEST_F(HttpParserMultipartCoverageTest, FileUploadWithExplicitContentType)
{
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"f\"; filename=\"data.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(req.files["f"].content_type, "text/plain");
}

TEST_F(HttpParserMultipartCoverageTest, MixedFieldsAndFilesParsed)
{
    std::string body =
        "--B\r\n"
        "Content-Disposition: form-data; name=\"text\"\r\n"
        "\r\n"
        "value1\r\n"
        "--B\r\n"
        "Content-Disposition: form-data; name=\"upload\"; filename=\"a.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file content\r\n"
        "--B--\r\n";
    auto req = make_request(body, "multipart/form-data; boundary=B");
    auto result = http_parser::parse_multipart_form_data(req);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(req.form_data["text"], "value1");
    EXPECT_EQ(req.files["upload"].filename, "a.txt");
    EXPECT_EQ(str_from(req.files["upload"].content), "file content");
}

// ============================================================================
// serialize_chunked_response boundary conditions
// ============================================================================

class HttpParserChunkedCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserChunkedCoverageTest, EmptyBodyProducesTerminatorOnly)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    EXPECT_NE(s.find("Transfer-Encoding: chunked"), std::string::npos);
    // With an empty body, the only chunk is the zero-size terminator.
    EXPECT_NE(s.rfind("0\r\n\r\n"), std::string::npos);
    // There must not be any non-terminator chunks.
    EXPECT_EQ(s.find("\r\n\r\n") + 4, s.find("0\r\n\r\n"));
}

TEST_F(HttpParserChunkedCoverageTest, BodyExactlyChunkSizeProducesSingleChunk)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.body.assign(8192u, 'A');

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    // 8192 in hex is "2000"
    EXPECT_NE(s.find("2000\r\n"), std::string::npos);
    EXPECT_NE(s.rfind("0\r\n\r\n"), std::string::npos);
    // There should be no 0x2000-and-something-bigger encoding: no two chunks.
    EXPECT_EQ(s.find("2000\r\n"), s.rfind("2000\r\n"));
}

TEST_F(HttpParserChunkedCoverageTest, BodyExceedingChunkSizeSplitsIntoMultiple)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.body.assign(10000u, 'X');

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    // First chunk: 8192 bytes -> "2000"
    EXPECT_NE(s.find("2000\r\n"), std::string::npos);
    // Second chunk: 10000 - 8192 = 1808 = 0x710
    EXPECT_NE(s.find("710\r\n"), std::string::npos);
    EXPECT_NE(s.rfind("0\r\n\r\n"), std::string::npos);
}

TEST_F(HttpParserChunkedCoverageTest, LowercaseContentLengthHeaderRemoved)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.headers["content-length"] = "42";  // lowercase variant
    resp.body = vec_from("abc");

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    EXPECT_EQ(s.find("content-length"), std::string::npos);
    EXPECT_NE(s.find("Transfer-Encoding: chunked"), std::string::npos);
}

TEST_F(HttpParserChunkedCoverageTest, MixedCaseContentLengthHeaderRemoved)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.headers["Content-Length"] = "42";
    resp.body = vec_from("abc");

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    EXPECT_EQ(s.find("Content-Length"), std::string::npos);
}

TEST_F(HttpParserChunkedCoverageTest, SetCookiePreservedInChunkedResponse)
{
    http_response resp;
    resp.version = http_version::HTTP_1_1;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    cookie c;
    c.name = "sid";
    c.value = "abc";
    resp.set_cookies.push_back(c);
    resp.body = vec_from("x");

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    EXPECT_NE(s.find("Set-Cookie"), std::string::npos);
    EXPECT_NE(s.find("sid"), std::string::npos);
}

TEST_F(HttpParserChunkedCoverageTest, Http10UseChunkedFlagFallsBackToPlain)
{
    http_response resp;
    resp.version = http_version::HTTP_1_0;
    resp.status_code = 200;
    resp.status_message = "OK";
    resp.use_chunked_encoding = true;
    resp.body = vec_from("hello");

    auto bytes = http_parser::serialize_response(resp);
    auto s = str_from(bytes);
    // HTTP/1.0 + use_chunked_encoding -> plain serializer, no
    // Transfer-Encoding header emitted.
    EXPECT_EQ(s.find("Transfer-Encoding"), std::string::npos);
    EXPECT_NE(s.find("hello"), std::string::npos);
}

// ============================================================================
// Flow-level coverage (byte vector parity, binary bodies, no-separator)
// ============================================================================

class HttpParserFlowCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserFlowCoverageTest, ParseRequestByteVectorMatchesStringView)
{
    std::string raw =
        "POST /api HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"k\":\"v\"}";
    auto vec_result = http_parser::parse_request(vec_from(raw));
    auto sv_result = http_parser::parse_request(std::string_view(raw));
    ASSERT_TRUE(vec_result.is_ok());
    ASSERT_TRUE(sv_result.is_ok());
    EXPECT_EQ(vec_result.value().uri, sv_result.value().uri);
    EXPECT_EQ(vec_result.value().headers, sv_result.value().headers);
    EXPECT_EQ(vec_result.value().body, sv_result.value().body);
}

TEST_F(HttpParserFlowCoverageTest, ParseResponseByteVectorMatchesStringView)
{
    std::string raw =
        "HTTP/1.1 201 Created\r\n"
        "Location: /new\r\n"
        "\r\n";
    auto vec_result = http_parser::parse_response(vec_from(raw));
    auto sv_result = http_parser::parse_response(std::string_view(raw));
    ASSERT_TRUE(vec_result.is_ok());
    ASSERT_TRUE(sv_result.is_ok());
    EXPECT_EQ(vec_result.value().status_code, sv_result.value().status_code);
    EXPECT_EQ(vec_result.value().status_message,
              sv_result.value().status_message);
}

TEST_F(HttpParserFlowCoverageTest, RequestWithBinaryBodyPreserved)
{
    std::vector<uint8_t> body_bytes = {
        0x00, 0x01, 0xFF, 0xFE, 0x7F, 0x80, 0x00, 0xAA};

    std::string header =
        "POST /upload HTTP/1.1\r\n"
        "Content-Length: 8\r\n"
        "\r\n";
    std::vector<uint8_t> raw(header.begin(), header.end());
    raw.insert(raw.end(), body_bytes.begin(), body_bytes.end());

    auto result = http_parser::parse_request(raw);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().body, body_bytes);
}

TEST_F(HttpParserFlowCoverageTest, ResponseWithBinaryBodyPreserved)
{
    std::vector<uint8_t> body_bytes = {0xCA, 0xFE, 0xBA, 0xBE};

    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 4\r\n"
        "\r\n";
    std::vector<uint8_t> raw(header.begin(), header.end());
    raw.insert(raw.end(), body_bytes.begin(), body_bytes.end());

    auto result = http_parser::parse_response(raw);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().body, body_bytes);
}

TEST_F(HttpParserFlowCoverageTest, EmptyStringReturnsError)
{
    auto req = http_parser::parse_request(std::string_view(""));
    EXPECT_TRUE(req.is_err());
    auto resp = http_parser::parse_response(std::string_view(""));
    EXPECT_TRUE(resp.is_err());
}

TEST_F(HttpParserFlowCoverageTest, EmptyByteVectorReturnsError)
{
    std::vector<uint8_t> empty;
    auto req = http_parser::parse_request(empty);
    EXPECT_TRUE(req.is_err());
    auto resp = http_parser::parse_response(empty);
    EXPECT_TRUE(resp.is_err());
}

TEST_F(HttpParserFlowCoverageTest, LfOnlyLineTerminatorNotAccepted)
{
    // split_line looks for "\r\n"; a line with only "\n" won't split, so the
    // whole blob becomes the request line, which then lacks a version field.
    auto result = http_parser::parse_request(std::string_view(
        "GET / HTTP/1.1\nHost: x\n\n"));
    EXPECT_TRUE(result.is_err());
}

TEST_F(HttpParserFlowCoverageTest, ResponseWithoutHeadersBodyOk)
{
    auto result = http_parser::parse_response("HTTP/1.1 204 No Content\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().status_code, 204);
    EXPECT_TRUE(result.value().body.empty());
}

TEST_F(HttpParserFlowCoverageTest, RequestWithHeaderSeparatorButNoBody)
{
    auto result = http_parser::parse_request(
        "GET / HTTP/1.1\r\n"
        "X: y\r\n"
        "\r\n");
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().body.empty());
    EXPECT_EQ(result.value().headers["X"], "y");
}

// ============================================================================
// Round-trip coverage (serialize then re-parse)
// ============================================================================

class HttpParserRoundTripCoverageTest : public ::testing::Test
{
};

TEST_F(HttpParserRoundTripCoverageTest, RequestNoHeadersNoBody)
{
    http_request original;
    original.method = http_method::HTTP_GET;
    original.uri = "/";
    original.version = http_version::HTTP_1_1;

    auto bytes = http_parser::serialize_request(original);
    auto parsed = http_parser::parse_request(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().method, original.method);
    EXPECT_EQ(parsed.value().uri, original.uri);
    EXPECT_EQ(parsed.value().version, original.version);
    EXPECT_TRUE(parsed.value().body.empty());
}

TEST_F(HttpParserRoundTripCoverageTest, RequestWithManyHeadersRoundTrip)
{
    http_request original;
    original.method = http_method::HTTP_POST;
    original.uri = "/api";
    original.version = http_version::HTTP_1_1;
    original.headers["Content-Type"] = "application/json";
    original.headers["Accept"] = "*/*";
    original.headers["X-Custom-A"] = "valueA";
    original.headers["X-Custom-B"] = "valueB";
    original.body = vec_from("{}");

    auto bytes = http_parser::serialize_request(original);
    auto parsed = http_parser::parse_request(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().uri, original.uri);
    EXPECT_EQ(parsed.value().headers, original.headers);
    EXPECT_EQ(parsed.value().body, original.body);
}

TEST_F(HttpParserRoundTripCoverageTest, ResponseInformationalStatusRoundTrip)
{
    http_response original;
    original.version = http_version::HTTP_1_1;
    original.status_code = 100;
    original.status_message = "Continue";

    auto bytes = http_parser::serialize_response(original);
    auto parsed = http_parser::parse_response(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().status_code, 100);
    EXPECT_EQ(parsed.value().status_message, "Continue");
}

TEST_F(HttpParserRoundTripCoverageTest, ResponseStatusMessageWithMultipleWords)
{
    http_response original;
    original.version = http_version::HTTP_1_1;
    original.status_code = 200;
    original.status_message = "OK Very Fine";

    auto bytes = http_parser::serialize_response(original);
    auto parsed = http_parser::parse_response(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().status_code, 200);
    EXPECT_EQ(parsed.value().status_message, "OK Very Fine");
}

TEST_F(HttpParserRoundTripCoverageTest, QueryParamsSurviveRoundTrip)
{
    http_request original;
    original.method = http_method::HTTP_GET;
    original.uri = "/search";
    original.version = http_version::HTTP_1_1;
    original.query_params["q"] = "hello world";
    original.query_params["n"] = "10";

    auto bytes = http_parser::serialize_request(original);
    auto parsed = http_parser::parse_request(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().query_params, original.query_params);
}

TEST_F(HttpParserRoundTripCoverageTest, CommaSeparatedHeaderValueKept)
{
    http_request original;
    original.method = http_method::HTTP_GET;
    original.uri = "/";
    original.version = http_version::HTTP_1_1;
    original.headers["Accept-Language"] = "en-US,en;q=0.9,fr;q=0.8";

    auto bytes = http_parser::serialize_request(original);
    auto parsed = http_parser::parse_request(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().headers["Accept-Language"],
              "en-US,en;q=0.9,fr;q=0.8");
}

TEST_F(HttpParserRoundTripCoverageTest, ColonInHeaderValueSurvives)
{
    http_request original;
    original.method = http_method::HTTP_GET;
    original.uri = "/";
    original.version = http_version::HTTP_1_1;
    original.headers["X-Forwarded"] = "proto: https";

    auto bytes = http_parser::serialize_request(original);
    auto parsed = http_parser::parse_request(bytes);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().headers["X-Forwarded"], "proto: https");
}

// ============================================================================
// serialize_request / serialize_response byte-level invariants
// ============================================================================

class HttpParserSerializeInvariantTest : public ::testing::Test
{
};

TEST_F(HttpParserSerializeInvariantTest, RequestStartsWithMethod)
{
    http_request r;
    r.method = http_method::HTTP_GET;
    r.uri = "/";
    r.version = http_version::HTTP_1_1;
    auto bytes = http_parser::serialize_request(r);
    auto s = str_from(bytes);
    EXPECT_EQ(s.substr(0, 4), "GET ");
}

TEST_F(HttpParserSerializeInvariantTest, ResponseStartsWithVersion)
{
    http_response r;
    r.version = http_version::HTTP_1_1;
    r.status_code = 200;
    r.status_message = "OK";
    auto bytes = http_parser::serialize_response(r);
    auto s = str_from(bytes);
    EXPECT_EQ(s.substr(0, 9), "HTTP/1.1 ");
}

TEST_F(HttpParserSerializeInvariantTest, HeadersTerminatedByDoubleCrlf)
{
    http_request r;
    r.method = http_method::HTTP_GET;
    r.uri = "/";
    r.version = http_version::HTTP_1_1;
    r.headers["X"] = "y";
    auto bytes = http_parser::serialize_request(r);
    auto s = str_from(bytes);
    EXPECT_NE(s.find("\r\n\r\n"), std::string::npos);
}

TEST_F(HttpParserSerializeInvariantTest, QueryParamsAppendedAfterQuestionMark)
{
    http_request r;
    r.method = http_method::HTTP_GET;
    r.uri = "/search";
    r.version = http_version::HTTP_1_1;
    r.query_params["q"] = "cpp";

    auto bytes = http_parser::serialize_request(r);
    auto s = str_from(bytes);
    EXPECT_NE(s.find("/search?q=cpp"), std::string::npos);
}

TEST_F(HttpParserSerializeInvariantTest, BodyAppendedAfterHeaderSeparator)
{
    http_request r;
    r.method = http_method::HTTP_POST;
    r.uri = "/data";
    r.version = http_version::HTTP_1_1;
    r.body = vec_from("payload");

    auto bytes = http_parser::serialize_request(r);
    auto s = str_from(bytes);
    auto sep_pos = s.find("\r\n\r\n");
    ASSERT_NE(sep_pos, std::string::npos);
    EXPECT_EQ(s.substr(sep_pos + 4), "payload");
}

TEST_F(HttpParserSerializeInvariantTest, ResponseWithoutBodyEndsAtSeparator)
{
    http_response r;
    r.version = http_version::HTTP_1_1;
    r.status_code = 204;
    r.status_message = "No Content";

    auto bytes = http_parser::serialize_response(r);
    auto s = str_from(bytes);
    // Must end with \r\n\r\n and nothing after.
    EXPECT_TRUE(s.size() >= 4);
    EXPECT_EQ(s.substr(s.size() - 4), "\r\n\r\n");
}

TEST_F(HttpParserSerializeInvariantTest, SetCookieEmittedInSerializeResponse)
{
    http_response r;
    r.version = http_version::HTTP_1_1;
    r.status_code = 200;
    r.status_message = "OK";
    cookie c;
    c.name = "sid";
    c.value = "xyz";
    r.set_cookies.push_back(c);

    auto bytes = http_parser::serialize_response(r);
    EXPECT_TRUE(contains(bytes, "Set-Cookie"));
    EXPECT_TRUE(contains(bytes, "sid"));
    EXPECT_TRUE(contains(bytes, "xyz"));
}

TEST_F(HttpParserSerializeInvariantTest, RequestSerializationEndsCorrectlyWithNoBody)
{
    http_request r;
    r.method = http_method::HTTP_HEAD;
    r.uri = "/ping";
    r.version = http_version::HTTP_1_1;

    auto bytes = http_parser::serialize_request(r);
    auto s = str_from(bytes);
    EXPECT_EQ(s.substr(s.size() - 4), "\r\n\r\n");
}
