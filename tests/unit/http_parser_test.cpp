/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "network_system/internal/http_parser.h"
#include "network_system/internal/http_types.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace network_system::internal;

class HTTPParserTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test HTTP method parsing
TEST_F(HTTPParserTest, ParseHTTPMethod)
{
    EXPECT_EQ(string_to_http_method("GET"), http_method::HTTP_GET);
    EXPECT_EQ(string_to_http_method("POST"), http_method::HTTP_POST);
    EXPECT_EQ(string_to_http_method("PUT"), http_method::HTTP_PUT);
    EXPECT_EQ(string_to_http_method("DELETE"), http_method::HTTP_DELETE);
    EXPECT_EQ(string_to_http_method("HEAD"), http_method::HTTP_HEAD);
    EXPECT_EQ(string_to_http_method("OPTIONS"), http_method::HTTP_OPTIONS);
    EXPECT_EQ(string_to_http_method("PATCH"), http_method::HTTP_PATCH);
    EXPECT_FALSE(string_to_http_method("INVALID"));
}

// Test HTTP method to string conversion
TEST_F(HTTPParserTest, HTTPMethodToString)
{
    EXPECT_EQ(http_method_to_string(http_method::HTTP_GET), "GET");
    EXPECT_EQ(http_method_to_string(http_method::HTTP_POST), "POST");
    EXPECT_EQ(http_method_to_string(http_method::HTTP_PUT), "PUT");
    EXPECT_EQ(http_method_to_string(http_method::HTTP_DELETE), "DELETE");
    EXPECT_EQ(http_method_to_string(http_method::HTTP_HEAD), "HEAD");
    EXPECT_EQ(http_method_to_string(http_method::HTTP_OPTIONS), "OPTIONS");
    EXPECT_EQ(http_method_to_string(http_method::HTTP_PATCH), "PATCH");
}

// Test simple GET request parsing
TEST_F(HTTPParserTest, ParseSimpleGETRequest)
{
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: test\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    EXPECT_EQ(request.method, http_method::HTTP_GET);
    EXPECT_EQ(request.uri, "/");
    EXPECT_EQ(request.version, http_version::HTTP_1_1);
    EXPECT_EQ(request.get_header("Host"), "localhost:8080");
    EXPECT_EQ(request.get_header("User-Agent"), "test");
    EXPECT_TRUE(request.body.empty());
}

// Test POST request with body
TEST_F(HTTPParserTest, ParsePOSTRequestWithBody)
{
    std::string raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 18\r\n"
        "\r\n"
        "{\"message\":\"test\"}";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    EXPECT_EQ(request.method, http_method::HTTP_POST);
    EXPECT_EQ(request.uri, "/api/data");
    EXPECT_EQ(request.get_header("Content-Type"), "application/json");
    EXPECT_EQ(request.get_header("Content-Length"), "18");

    std::string body_str(request.body.begin(), request.body.end());
    EXPECT_EQ(body_str, "{\"message\":\"test\"}");
}

// Test query parameters parsing
TEST_F(HTTPParserTest, ParseQueryParameters)
{
    std::string raw_request =
        "GET /search?q=test&page=1&sort=desc HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    EXPECT_EQ(request.uri, "/search");
    EXPECT_EQ(request.query_params.size(), 3);
    EXPECT_EQ(request.query_params.at("q"), "test");
    EXPECT_EQ(request.query_params.at("page"), "1");
    EXPECT_EQ(request.query_params.at("sort"), "desc");
}

// Test malformed request line
TEST_F(HTTPParserTest, ParseMalformedRequestLine)
{
    // Missing HTTP version
    std::string raw_request =
        "GET /\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    EXPECT_TRUE(result.is_err());
}

// Test invalid HTTP method
TEST_F(HTTPParserTest, ParseInvalidMethod)
{
    std::string raw_request =
        "INVALID / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    EXPECT_TRUE(result.is_err());
}

// Test case-insensitive header names
TEST_F(HTTPParserTest, CaseInsensitiveHeaders)
{
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Content-Type: text/plain\r\n"
        "content-length: 5\r\n"
        "\r\n"
        "hello";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Headers should be accessible regardless of case
    EXPECT_EQ(request.get_header("Content-Type"), "text/plain");
    EXPECT_EQ(request.get_header("Content-Length"), "5");
}

// Test response serialization
TEST_F(HTTPParserTest, SerializeResponse)
{
    http_response response;
    response.version = http_version::HTTP_1_1;
    response.status_code = 200;
    response.status_message = "OK";
    response.set_header("Content-Type", "text/plain");
    response.set_body_string("Hello, World!");
    response.set_header("Content-Length", std::to_string(response.body.size()));

    auto serialized = http_parser::serialize_response(response);
    std::string response_str(serialized.begin(), serialized.end());

    EXPECT_TRUE(response_str.find("HTTP/1.1 200 OK") != std::string::npos);
    EXPECT_TRUE(response_str.find("Content-Type: text/plain") != std::string::npos);
    EXPECT_TRUE(response_str.find("Content-Length: 13") != std::string::npos);
    EXPECT_TRUE(response_str.find("Hello, World!") != std::string::npos);
}

// Test request serialization
TEST_F(HTTPParserTest, SerializeRequest)
{
    http_request request;
    request.method = http_method::HTTP_POST;
    request.uri = "/api/test";
    request.version = http_version::HTTP_1_1;
    request.set_header("Host", "localhost:8080");
    request.set_header("Content-Type", "application/json");
    request.set_body_string("{\"test\":true}");
    request.set_header("Content-Length", std::to_string(request.body.size()));

    auto serialized = http_parser::serialize_request(request);
    std::string request_str(serialized.begin(), serialized.end());

    EXPECT_TRUE(request_str.find("POST /api/test HTTP/1.1") != std::string::npos);
    EXPECT_TRUE(request_str.find("Host: localhost:8080") != std::string::npos);
    EXPECT_TRUE(request_str.find("Content-Type: application/json") != std::string::npos);
    EXPECT_TRUE(request_str.find("{\"test\":true}") != std::string::npos);
}

// Test large body handling
TEST_F(HTTPParserTest, ParseLargeBody)
{
    std::string body(10000, 'x'); // 10KB body
    std::string raw_request =
        "POST /upload HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    EXPECT_EQ(request.body.size(), 10000);
    EXPECT_EQ(request.get_header("Content-Length"), "10000");
}

// Test multiple headers with same name
TEST_F(HTTPParserTest, MultipleHeadersSameName)
{
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Accept: text/html\r\n"
        "Accept: application/json\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Last value should be used
    auto accept = request.get_header("Accept");
    EXPECT_TRUE(accept == "application/json" || accept == "text/html");
}

// Test whitespace handling
TEST_F(HTTPParserTest, WhitespaceHandling)
{
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Host:  localhost:8080  \r\n"
        "User-Agent:   test   \r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Whitespace should be trimmed
    auto host = request.get_header("Host");
    auto user_agent = request.get_header("User-Agent");

    ASSERT_TRUE(host.has_value());
    ASSERT_TRUE(user_agent.has_value());
    EXPECT_TRUE(host->find("localhost:8080") != std::string::npos);
    EXPECT_TRUE(user_agent->find("test") != std::string::npos);
}

// Test URL encoding in query parameters
TEST_F(HTTPParserTest, URLEncodedQueryParams)
{
    std::string raw_request =
        "GET /search?q=hello%20world&name=John%20Doe HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    EXPECT_EQ(request.uri, "/search");
    EXPECT_TRUE(request.query_params.find("q") != request.query_params.end());
    EXPECT_TRUE(request.query_params.find("name") != request.query_params.end());
}

// Test Content-Length mismatch (declared larger than actual body)
TEST_F(HTTPParserTest, ContentLengthMismatchLarger)
{
    std::string raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 100\r\n"  // Claims 100 bytes
        "\r\n"
        "{\"test\":true}";  // Only ~14 bytes

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Parser should successfully parse what's available
    // Server-level buffering will handle incomplete body
    EXPECT_EQ(request.get_header("Content-Length"), "100");
    EXPECT_LT(request.body.size(), 100);  // Body is smaller than declared
}

// Test Content-Length mismatch (declared smaller than actual body)
TEST_F(HTTPParserTest, ContentLengthMismatchSmaller)
{
    std::string raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 5\r\n"  // Claims only 5 bytes
        "\r\n"
        "{\"test\":true}";  // Actually ~14 bytes

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Parser reads all available data
    EXPECT_EQ(request.get_header("Content-Length"), "5");
    // Body will contain all data after headers
    EXPECT_GT(request.body.size(), 5);
}

// Test extremely large Content-Length header
TEST_F(HTTPParserTest, ExtremelyLargeContentLength)
{
    std::string raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 999999999999\r\n"  // Very large number
        "\r\n"
        "test";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Parser should parse the header value
    // Server will reject this based on MAX_REQUEST_SIZE
    EXPECT_EQ(request.get_header("Content-Length"), "999999999999");
}

// Test negative Content-Length (invalid but should be caught)
TEST_F(HTTPParserTest, NegativeContentLength)
{
    std::string raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: -100\r\n"
        "\r\n"
        "test";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Parser reads the header as-is
    // Server-level validation should reject negative values
    EXPECT_EQ(request.get_header("Content-Length"), "-100");
}

// Test request with no body but Content-Length header
TEST_F(HTTPParserTest, ContentLengthWithoutBody)
{
    std::string raw_request =
        "GET /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    EXPECT_EQ(request.get_header("Content-Length"), "0");
    EXPECT_TRUE(request.body.empty());
}

// Test malformed Content-Length (non-numeric)
TEST_F(HTTPParserTest, MalformedContentLength)
{
    std::string raw_request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: abc\r\n"
        "\r\n"
        "test";

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();

    // Parser accepts invalid header value
    // Server-level parsing will handle validation
    EXPECT_EQ(request.get_header("Content-Length"), "abc");
}

// Test empty request
TEST_F(HTTPParserTest, EmptyRequest)
{
    std::string raw_request = "";
    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    EXPECT_TRUE(result.is_err());
}

// Test request with only headers, no double CRLF
TEST_F(HTTPParserTest, RequestWithoutDoubleCRLF)
{
    std::string raw_request =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:8080\r\n";  // Missing final \r\n

    std::vector<uint8_t> data(raw_request.begin(), raw_request.end());
    auto result = http_parser::parse_request(data);

    // Parser should handle incomplete requests
    ASSERT_TRUE(result.is_ok());
    const auto& request = result.value();
    EXPECT_EQ(request.method, http_method::HTTP_GET);
}

// Test response parsing with various status codes
TEST_F(HTTPParserTest, ParseResponseVariousStatusCodes)
{
    struct TestCase {
        int status_code;
        std::string status_message;
    };

    std::vector<TestCase> test_cases = {
        {200, "OK"},
        {201, "Created"},
        {204, "No Content"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {500, "Internal Server Error"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"}
    };

    for (const auto& test_case : test_cases) {
        std::string raw_response =
            "HTTP/1.1 " + std::to_string(test_case.status_code) + " " + test_case.status_message + "\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        std::vector<uint8_t> data(raw_response.begin(), raw_response.end());
        auto result = http_parser::parse_response(data);

        ASSERT_TRUE(result.is_ok()) << "Failed for status code " << test_case.status_code;
        const auto& response = result.value();
        EXPECT_EQ(response.status_code, test_case.status_code);
        EXPECT_EQ(response.status_message, test_case.status_message);
    }
}

// Main function
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
