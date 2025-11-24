# NET-102: Enhance HTTP Server Error Handling

| Field | Value |
|-------|-------|
| **ID** | NET-102 |
| **Title** | Enhance HTTP Server Error Handling |
| **Category** | CORE |
| **Priority** | HIGH |
| **Status** | DONE |
| **Est. Duration** | 2-3 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (Problem Statement)

### Current Problem
HTTP server error handling is incomplete:
- Responses to invalid requests are generic
- Parse errors lack detailed information
- Timeout handling is insufficient
- Error response format is inconsistent

### Goal
Build a robust and consistent HTTP error handling system:
- RFC 7231 compliant error responses
- Structured error messages (JSON)
- Detailed error logging
- Client-friendly error information

### Affected Files
- `include/network_system/protocols/http_server.h`
- `src/protocols/http_server.cpp`
- `include/network_system/protocols/http_parser.h`
- `src/protocols/http_parser.cpp`

---

## How (Implementation Plan)

### Implementation Plan

#### Step 1: Define Error Types
```cpp
enum class http_error_code {
    // Client Errors (4xx)
    bad_request = 400,
    unauthorized = 401,
    forbidden = 403,
    not_found = 404,
    method_not_allowed = 405,
    request_timeout = 408,
    payload_too_large = 413,
    uri_too_long = 414,
    unsupported_media_type = 415,
    too_many_requests = 429,

    // Server Errors (5xx)
    internal_server_error = 500,
    not_implemented = 501,
    bad_gateway = 502,
    service_unavailable = 503,
    gateway_timeout = 504
};

struct http_error {
    http_error_code code;
    std::string message;
    std::string detail;
    std::string request_id;
};
```

#### Step 2: Implement Error Response Builder
```cpp
class http_error_response {
public:
    static auto build_json_error(const http_error& error) -> http_response;
    static auto build_html_error(const http_error& error) -> http_response;

private:
    static auto get_status_text(http_error_code code) -> std::string_view;
};
```

#### Step 3: Add Error Handlers
```cpp
class http_server {
public:
    // Custom error handler registration
    using error_handler = std::function<http_response(const http_error&)>;
    auto set_error_handler(http_error_code code, error_handler handler) -> void;
    auto set_default_error_handler(error_handler handler) -> void;

private:
    auto handle_parse_error(const parse_error& e) -> http_response;
    auto handle_timeout_error() -> http_response;
    auto handle_internal_error(const std::exception& e) -> http_response;
};
```

#### Step 4: Enhance Parser Error Reporting
```cpp
struct parse_error {
    enum class type {
        invalid_method,
        invalid_uri,
        invalid_version,
        invalid_header,
        incomplete_body,
        body_too_large
    };

    type error_type;
    size_t line_number;
    size_t column_number;
    std::string context;
};
```

### Code Changes

1. **http_error.h** - Define error types and response builder
2. **http_server.h** - Add error handler registration interface
3. **http_parser.h** - Add detailed parsing error information
4. **http_server.cpp** - Implement error handling logic

---

## Test (Verification Plan)

### Unit Tests
```cpp
TEST(HttpErrorResponse, BuildsJsonError) {
    http_error error{
        .code = http_error_code::bad_request,
        .message = "Invalid JSON",
        .detail = "Unexpected token at position 42",
        .request_id = "req-123"
    };

    auto response = http_error_response::build_json_error(error);

    EXPECT_EQ(response.status_code(), 400);
    EXPECT_EQ(response.header("Content-Type"), "application/json");
    EXPECT_TRUE(response.body().find("Invalid JSON") != std::string::npos);
}

TEST(HttpParser, ReportsDetailedErrors) {
    http_parser parser;

    auto result = parser.parse("INVALID /path HTTP/1.1\r\n");

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error_type, parse_error::type::invalid_method);
    EXPECT_EQ(result.error().line_number, 1);
}

TEST(HttpServer, UsesCustomErrorHandler) {
    http_server server;

    bool handler_called = false;
    server.set_error_handler(http_error_code::not_found,
        [&](const http_error& e) {
            handler_called = true;
            return http_error_response::build_json_error(e);
        });

    // Simulate 404
    auto response = server.handle_request(create_request("GET", "/nonexistent"));

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(response.status_code(), 404);
}
```

### Integration Tests
```cpp
TEST(HttpServer, ReturnsProperErrorResponse) {
    http_server server;
    server.start(8080);

    // Test malformed request
    http_client client;
    auto response = client.send_raw("BADMETHOD / HTTP/1.1\r\n\r\n");

    EXPECT_EQ(response.status_code(), 400);
    auto body = json::parse(response.body());
    EXPECT_TRUE(body.contains("error"));
    EXPECT_TRUE(body.contains("message"));
}

TEST(HttpServer, HandlesTimeout) {
    http_server server;
    server.set_request_timeout(std::chrono::seconds(1));
    server.start(8080);

    // Send incomplete request and wait
    auto socket = create_tcp_socket("localhost", 8080);
    socket.send("GET / HTTP/1.1\r\n");  // No terminating \r\n\r\n

    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto response = socket.receive();
    EXPECT_EQ(parse_status_code(response), 408);  // Request Timeout
}
```

### Manual Verification
1. Send invalid HTTP method → Verify 400 response
2. Request non-existent path → Verify 404 response
3. Send large payload → Verify 413 response
4. Trigger request timeout → Verify 408 response

---

## Acceptance Criteria

- [x] Generate appropriate responses for all HTTP error codes
- [x] Support JSON format error responses (RFC 7807)
- [x] Include detailed information for parsing errors
- [x] Enable custom error handler registration
- [x] Handle timeout errors appropriately
- [x] Error logging working
- [x] All tests passing

---

## Notes

- Security: Do not expose sensitive information in error messages
- Performance: Minimize error response generation overhead
- Consider RFC 7807 (Problem Details for HTTP APIs) format
