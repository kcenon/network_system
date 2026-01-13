/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/protocols/http2/http2_server.h"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace kcenon::network::protocols::http2;

namespace
{
    std::atomic<bool> g_running{true};

    void signal_handler(int /*signal*/)
    {
        g_running = false;
    }
}

int main()
{
    std::cout << "=== HTTP/2 Server Example ===" << std::endl;
    std::cout << "Note: This example uses h2c (HTTP/2 cleartext) for simplicity." << std::endl;
    std::cout << "      For production, use start_tls() with proper certificates." << std::endl;
    std::cout << std::endl;

    // Setup signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Create HTTP/2 server instance
    auto server = std::make_shared<http2_server>("http2-example-server");

    // Configure HTTP/2 settings (optional)
    http2_settings settings;
    settings.max_concurrent_streams = 100;
    settings.initial_window_size = 65535;
    settings.max_frame_size = 16384;
    server->set_settings(settings);

    // Set error handler for debugging
    server->set_error_handler([](const std::string& error_msg)
    {
        std::cerr << "[ERROR] " << error_msg << std::endl;
    });

    // Set request handler
    server->set_request_handler([](http2_server_stream& stream,
                                   const http2_request& request)
    {
        std::cout << "[REQUEST] " << request.method << " " << request.path << std::endl;

        // Route handling
        if (request.method == "GET" && request.path == "/")
        {
            // Home page
            std::string body = R"({
    "message": "Welcome to HTTP/2 Server",
    "protocol": "h2c",
    "endpoints": [
        "GET /",
        "GET /api/health",
        "GET /api/echo",
        "POST /api/data"
    ]
})";
            stream.send_headers(200, {
                {"content-type", "application/json"},
                {"server", "network-system-http2"}
            });
            stream.send_data(body, true);
        }
        else if (request.method == "GET" && request.path == "/api/health")
        {
            // Health check endpoint
            stream.send_headers(200, {
                {"content-type", "application/json"}
            });
            stream.send_data(R"({"status": "healthy"})", true);
        }
        else if (request.method == "GET" && request.path == "/api/echo")
        {
            // Echo query parameters
            std::string response = R"({"path": ")" + request.path + R"(", "authority": ")" +
                                   request.authority + R"("})";
            stream.send_headers(200, {
                {"content-type", "application/json"}
            });
            stream.send_data(response, true);
        }
        else if (request.method == "POST" && request.path == "/api/data")
        {
            // Echo POST body
            auto content_type = request.content_type();

            std::string response = R"({"received_bytes": )" +
                                   std::to_string(request.body.size()) + R"(, "content_type": ")" +
                                   content_type.value_or("unknown") + R"("})";

            stream.send_headers(200, {
                {"content-type", "application/json"}
            });
            stream.send_data(response, true);
        }
        else
        {
            // 404 Not Found
            stream.send_headers(404, {
                {"content-type", "application/json"}
            });
            stream.send_data(R"({"error": "Not Found"})", true);
        }
    });

    // Start server on port 8080 (h2c - cleartext HTTP/2)
    const unsigned short port = 8080;
    std::cout << "Starting HTTP/2 server on port " << port << "..." << std::endl;

    auto result = server->start(port);
    if (result.is_err())
    {
        std::cerr << "Failed to start server: " << result.error().message << std::endl;
        return 1;
    }

    std::cout << "Server started successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Available endpoints:" << std::endl;
    std::cout << "  GET  http://localhost:" << port << "/" << std::endl;
    std::cout << "  GET  http://localhost:" << port << "/api/health" << std::endl;
    std::cout << "  GET  http://localhost:" << port << "/api/echo" << std::endl;
    std::cout << "  POST http://localhost:" << port << "/api/data" << std::endl;
    std::cout << std::endl;
    std::cout << "Test with curl (requires h2c support):" << std::endl;
    std::cout << "  curl --http2-prior-knowledge http://localhost:" << port << "/" << std::endl;
    std::cout << "  curl --http2-prior-knowledge http://localhost:" << port << "/api/health" << std::endl;
    std::cout << "  curl --http2-prior-knowledge -X POST -d '{\"test\": 123}' http://localhost:"
              << port << "/api/data" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;

    // Wait for shutdown signal
    while (g_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down server..." << std::endl;
    server->stop();
    std::cout << "Server stopped." << std::endl;

    return 0;
}
