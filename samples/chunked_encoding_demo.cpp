/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "network_system/core/http_server.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

using namespace network_system::core;
using namespace network_system::internal;

int main()
{
    std::cout << "=== Chunked Transfer Encoding Demo ===" << std::endl;

    // Create HTTP server
    auto server = std::make_shared<http_server>("chunked_demo_server");

    // Small response (< 8KB) - will use Content-Length
    server->get("/small", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        std::string body = "This is a small response. ";
        body += "Content-Length header will be used instead of chunked encoding.";

        response.set_body_string(body);
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    // Large response (> 8KB) - will use chunked encoding
    server->get("/large", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        // Create a large body (>8KB)
        std::string body;
        body.reserve(10000);

        for (int i = 0; i < 500; ++i)
        {
            body += "This is line " + std::to_string(i) + " of a large response. ";
        }

        response.set_body_string(body);
        response.set_header("Content-Type", "text/plain");

        std::cout << "Generating large response: " << body.size() << " bytes" << std::endl;

        return response;
    });

    // Very large JSON response
    server->get("/api/large-data", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        // Generate a large JSON-like response
        std::string json = "{\n  \"data\": [\n";

        for (int i = 0; i < 1000; ++i)
        {
            json += "    {\"id\": " + std::to_string(i) +
                    ", \"name\": \"Item " + std::to_string(i) +
                    "\", \"description\": \"This is a description for item " +
                    std::to_string(i) + "\"}";
            if (i < 999) json += ",";
            json += "\n";
        }

        json += "  ]\n}";

        response.set_body_string(json);
        response.set_header("Content-Type", "application/json");

        std::cout << "Generating large JSON: " << json.size() << " bytes" << std::endl;

        return response;
    });

    // Info endpoint to check encoding method
    server->get("/", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        std::string body = R"(
=== Chunked Transfer Encoding Demo ===

Test endpoints:
1. GET /small - Small response (~100 bytes) - uses Content-Length
2. GET /large - Large response (~10KB) - uses Transfer-Encoding: chunked
3. GET /api/large-data - Very large JSON (~50KB) - uses Transfer-Encoding: chunked

To test chunked encoding:
  curl -v http://localhost:8080/large

Look for the 'Transfer-Encoding: chunked' header in the response.

For small responses:
  curl -v http://localhost:8080/small

Look for the 'Content-Length' header instead.
)";

        response.set_body_string(body);
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    // Start server on port 8080
    std::cout << "Starting HTTP server on port 8080..." << std::endl;
    auto result = server->start(8080);
    if (result.is_err())
    {
        std::cerr << "Failed to start server: " << result.error().message << std::endl;
        return 1;
    }

    std::cout << "Server started successfully!" << std::endl;
    std::cout << "Visit http://localhost:8080/ for endpoint information" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;

    // Wait for server to stop
    server->wait_for_stop();

    return 0;
}
