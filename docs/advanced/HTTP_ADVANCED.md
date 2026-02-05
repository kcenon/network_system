# HTTP Server Advanced Features Guide

**Last Updated**: 2025-11-24
**Version**: 0.1.0.0

This document provides comprehensive guidance on using advanced features of the network_system HTTP server and client.

---

## Table of Contents

- [Overview](#overview)
- [Route Configuration](#route-configuration)
- [Error Handling](#error-handling)
- [Response Compression](#response-compression)
- [Request Timeout](#request-timeout)
- [HTTP Client Usage](#http-client-usage)
- [Performance Considerations](#performance-considerations)
- [Troubleshooting](#troubleshooting)

---

## Overview

The network_system HTTP server is built on top of the messaging_server infrastructure, providing HTTP/1.1 protocol support with modern C++ APIs.

**Key Features**:
- HTTP/1.1 protocol support
- Pattern-based routing with path parameters
- Query parameter parsing
- Custom error handlers
- Response compression (gzip, deflate)
- Request timeout configuration
- Thread-safe design

**Namespace**: `network_system::core`

---

## Route Configuration

### Basic Routing

```cpp
#include "kcenon/network/core/http_server.h"

using namespace network_system::core;
using namespace network_system::internal;

auto server = std::make_shared<http_server>("my_server");

// GET request
server->get("/", [](const http_request_context& ctx) {
    http_response response;
    response.status_code = 200;
    response.set_body_string("Hello, World!");
    response.set_header("Content-Type", "text/plain");
    return response;
});

// POST request
server->post("/api/data", [](const http_request_context& ctx) {
    auto body = ctx.request.get_body_string();

    http_response response;
    response.status_code = 201;
    response.set_body_string("Created: " + body);
    response.set_header("Content-Type", "text/plain");
    return response;
});
```

### Path Parameters

Use `:param_name` syntax to capture path segments:

```cpp
// Single parameter
server->get("/users/:id", [](const http_request_context& ctx) {
    auto user_id = ctx.get_path_param("id").value_or("unknown");

    http_response response;
    response.status_code = 200;
    response.set_body_string("User ID: " + user_id);
    response.set_header("Content-Type", "text/plain");
    return response;
});

// Multiple parameters
server->get("/products/:category/:id", [](const http_request_context& ctx) {
    auto category = ctx.get_path_param("category").value_or("");
    auto id = ctx.get_path_param("id").value_or("");

    http_response response;
    response.status_code = 200;
    response.set_body_string("Category: " + category + ", ID: " + id);
    return response;
});
```

### Query Parameters

Access query parameters from the request:

```cpp
server->get("/search", [](const http_request_context& ctx) {
    auto query = ctx.get_query_param("q").value_or("");
    auto page = ctx.get_query_param("page").value_or("1");
    auto limit = ctx.get_query_param("limit").value_or("10");

    http_response response;
    response.status_code = 200;
    response.set_body_string(
        "Search: " + query +
        ", Page: " + page +
        ", Limit: " + limit
    );
    return response;
});
```

### Supported HTTP Methods

| Method | Registration Function | Description |
|--------|----------------------|-------------|
| GET | `server->get()` | Retrieve resources |
| POST | `server->post()` | Create resources |
| PUT | `server->put()` | Update resources |
| DELETE | `server->del()` | Delete resources |
| PATCH | `server->patch()` | Partial update |
| HEAD | `server->head()` | Get headers only |
| OPTIONS | `server->options()` | Get allowed methods |

---

## Error Handling

### Custom 404 Handler

```cpp
server->set_not_found_handler([](const http_request_context& ctx) {
    http_response response;
    response.status_code = 404;
    response.set_body_string(R"({"error": "Resource not found", "path": ")" +
                             ctx.request.path + R"("})");
    response.set_header("Content-Type", "application/json");
    return response;
});
```

### Custom 500 Handler

```cpp
server->set_error_handler([](const http_request_context& ctx) {
    http_response response;
    response.status_code = 500;
    response.set_body_string(R"({"error": "Internal server error"})");
    response.set_header("Content-Type", "application/json");
    return response;
});
```

### Specific Error Code Handlers

```cpp
using namespace network_system::internal;

// Handle specific error codes
server->set_error_handler(http_error_code::bad_request,
    [](const http_error& error) {
        http_response response;
        response.status_code = 400;
        response.set_body_string(
            R"({"error": "Bad Request", "message": ")" +
            error.message + R"("})"
        );
        response.set_header("Content-Type", "application/json");
        return response;
    });

server->set_error_handler(http_error_code::method_not_allowed,
    [](const http_error& error) {
        http_response response;
        response.status_code = 405;
        response.set_body_string(R"({"error": "Method not allowed"})");
        response.set_header("Content-Type", "application/json");
        return response;
    });
```

### Default Error Handler

Set a fallback handler for all unhandled error codes:

```cpp
server->set_default_error_handler([](const http_error& error) {
    http_response response;
    response.status_code = error.status_code;
    response.set_body_string(
        R"({"error": ")" + error.message +
        R"(", "code": )" + std::to_string(error.status_code) + "}"
    );
    response.set_header("Content-Type", "application/json");
    return response;
});
```

### JSON Error Responses

Enable JSON format for all error responses:

```cpp
// Enable JSON error responses
server->set_json_error_responses(true);

// Errors will automatically be formatted as JSON:
// {"error": "Not Found", "status": 404, "path": "/unknown"}
```

---

## Response Compression

### Enabling Compression

```cpp
// Enable automatic response compression
server->set_compression_enabled(true);

// Set minimum size for compression (default: 1024 bytes)
server->set_compression_threshold(512);  // Compress responses > 512 bytes
```

### How Compression Works

1. Server checks the `Accept-Encoding` header from the request
2. If response size exceeds threshold, compression is applied
3. Supported algorithms: gzip, deflate
4. `Content-Encoding` header is automatically set

### Client-Side

The HTTP client automatically handles compressed responses:

```cpp
auto client = std::make_shared<http_client>();

std::map<std::string, std::string> headers = {
    {"Accept-Encoding", "gzip, deflate"}
};

auto response = client->get("http://localhost:8080/large-data", {}, headers);
// Response is automatically decompressed
```

---

## Request Timeout

### Setting Server Timeout

```cpp
using namespace std::chrono;

// Set request timeout to 60 seconds
server->set_request_timeout(seconds(60));

// For long-running operations
server->set_request_timeout(minutes(5));
```

### Client Timeout

```cpp
using namespace std::chrono;

// Create client with custom timeout
auto client = std::make_shared<http_client>(seconds(30));

// Or set timeout after creation
client->set_timeout(seconds(45));

// Get current timeout
auto timeout = client->get_timeout();
```

---

## HTTP Client Usage

### Basic Requests

```cpp
#include "kcenon/network/core/http_client.h"

using namespace network_system::core;

auto client = std::make_shared<http_client>();

// Simple GET
auto response = client->get("http://localhost:8080/api/users");
if (response.is_ok()) {
    std::cout << "Status: " << response.value().status_code << "\n";
    std::cout << "Body: " << response.value().get_body_string() << "\n";
}

// GET with query parameters
std::map<std::string, std::string> params = {
    {"page", "1"},
    {"limit", "10"}
};
auto search_result = client->get("http://localhost:8080/search", params);

// GET with custom headers
std::map<std::string, std::string> headers = {
    {"Authorization", "Bearer token123"},
    {"Accept", "application/json"}
};
auto auth_result = client->get("http://localhost:8080/protected", {}, headers);
```

### POST Requests

```cpp
// POST with string body
std::string json_body = R"({"name": "John", "email": "john@example.com"})";
std::map<std::string, std::string> headers = {
    {"Content-Type", "application/json"}
};

auto post_result = client->post(
    "http://localhost:8080/api/users",
    json_body,
    headers
);

// POST with binary body
std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0x03};
headers["Content-Type"] = "application/octet-stream";

auto binary_result = client->post(
    "http://localhost:8080/upload",
    binary_data,
    headers
);
```

### Other Methods

```cpp
// PUT
auto put_result = client->put(
    "http://localhost:8080/api/users/123",
    R"({"name": "Jane"})",
    {{"Content-Type", "application/json"}}
);

// DELETE
auto delete_result = client->del("http://localhost:8080/api/users/123");

// PATCH
auto patch_result = client->patch(
    "http://localhost:8080/api/users/123",
    R"({"email": "new@example.com"})",
    {{"Content-Type", "application/json"}}
);

// HEAD
auto head_result = client->head("http://localhost:8080/api/users");
```

### Error Handling

```cpp
auto response = client->get("http://localhost:8080/api/resource");

if (response.is_ok()) {
    auto& resp = response.value();

    if (resp.status_code == 200) {
        // Success
        process_data(resp.get_body_string());
    } else if (resp.status_code == 404) {
        // Not found
        handle_not_found();
    } else {
        // Other status codes
        log_error("Unexpected status: " + std::to_string(resp.status_code));
    }
} else {
    // Network error (connection failed, timeout, etc.)
    log_error("Request failed: " + response.error().message);
}
```

---

## Performance Considerations

### Server Configuration Tips

1. **Thread Safety**: Route handlers should be thread-safe as they may be called concurrently.

2. **Keep Handlers Fast**: Long-running operations should be offloaded to background threads.

3. **Use Compression Wisely**: Set appropriate threshold to avoid compressing small responses.

4. **Timeout Configuration**: Set timeouts based on expected operation duration.

### Concurrent Request Handling

The HTTP server handles requests concurrently using the underlying messaging_server's thread pool:

```cpp
// Handlers are called from multiple threads
server->get("/compute", [](const http_request_context& ctx) {
    // This handler may run concurrently on different threads
    // Ensure thread-safe access to shared resources

    std::lock_guard<std::mutex> lock(shared_mutex);
    // ... access shared data

    http_response response;
    response.status_code = 200;
    return response;
});
```

### Request Size Limits

The server has built-in limits to prevent denial-of-service attacks:

| Limit | Default Value | Description |
|-------|---------------|-------------|
| MAX_REQUEST_SIZE | 10 MB | Maximum total request size |
| MAX_HEADER_SIZE | 64 KB | Maximum headers size |

---

## Troubleshooting

### Common Issues

**Issue**: Server fails to start

```cpp
auto result = server->start(8080);
if (result.is_err()) {
    std::cerr << "Failed to start: " << result.error().message << "\n";
    // Common causes:
    // - Port already in use
    // - Insufficient permissions for ports < 1024
}
```

**Issue**: Client connection timeout

```cpp
// Increase timeout for slow servers
client->set_timeout(std::chrono::seconds(60));

// Check for network issues
auto response = client->get("http://example.com/api");
if (response.is_err()) {
    if (response.error().code == error_codes::network_system::connection_timeout) {
        // Handle timeout specifically
    }
}
```

**Issue**: Route not matching

- Ensure path parameters use `:` prefix (e.g., `/users/:id`)
- Check for trailing slashes - `/api/users` and `/api/users/` are different
- Verify HTTP method matches the registered handler

### Debug Logging

Enable verbose logging to troubleshoot issues:

```cpp
// Handlers can include logging
server->get("/debug", [](const http_request_context& ctx) {
    std::cout << "Request: " << ctx.request.method_str() << " "
              << ctx.request.path << "\n";
    std::cout << "Headers:\n";
    for (const auto& [key, value] : ctx.request.headers) {
        std::cout << "  " << key << ": " << value << "\n";
    }

    http_response response;
    response.status_code = 200;
    return response;
});
```

---

## Related Documentation

- [API Reference](API_REFERENCE.md) - Complete API documentation
- [TLS Setup Guide](TLS_SETUP_GUIDE.md) - Secure HTTP configuration
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions
- [Operations Guide](OPERATIONS.md) - Production deployment

---

**Maintainer**: Network System Team
**Contact**: Use issue tracker for questions
