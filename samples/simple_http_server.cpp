/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <kcenon/network/facade/http_facade.h>
#include <kcenon/network/http/http_server.h>
#include <iostream>
#include <chrono>
#include <thread>

using namespace kcenon::network;

int main()
{
    std::cout << "=== Simple HTTP Server Demo ===" << std::endl;

    // Create HTTP server using facade
    facade::http_facade http;
    auto server = http.create_server({
        .server_id = "simple_http_server"
    });

    // Register routes
    server->get("/", [](const core::http_request_context&)
    {
        internal::http_response response;
        response.status_code = 200;
        response.set_body_string("Hello, World! Welcome to NetworkSystem HTTP Server.");
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    server->get("/api/hello", [](const core::http_request_context& ctx)
    {
        internal::http_response response;
        response.status_code = 200;

        auto name = ctx.get_query_param("name").value_or("Guest");
        response.set_body_string("Hello, " + name + "!");
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    server->get("/users/:id", [](const core::http_request_context& ctx)
    {
        internal::http_response response;
        response.status_code = 200;

        auto user_id = ctx.get_path_param("id").value_or("unknown");
        response.set_body_string("User ID: " + user_id);
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    server->post("/api/echo", [](const core::http_request_context& ctx)
    {
        internal::http_response response;
        response.status_code = 200;

        auto body = ctx.request.get_body_string();
        response.set_body_string("Echo: " + body);
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
    std::cout << "Try these URLs:" << std::endl;
    std::cout << "  http://localhost:8080/" << std::endl;
    std::cout << "  http://localhost:8080/api/hello?name=John" << std::endl;
    std::cout << "  http://localhost:8080/users/123" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;

    // Wait for server to stop
    server->wait_for_stop();

    return 0;
}
