/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/core/http_client.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace kcenon::network::core;
using namespace kcenon::network::internal;

void print_response(const http_response& response)
{
    std::cout << "Status: " << response.status_code << " " << response.status_message << std::endl;
    std::cout << "Headers:" << std::endl;
    for (const auto& [name, value] : response.headers)
    {
        std::cout << "  " << name << ": " << value << std::endl;
    }
    std::cout << "Body (" << response.body.size() << " bytes):" << std::endl;
    std::cout << response.get_body_string() << std::endl;
}

int main()
{
    std::cout << "=== Simple HTTP Client Demo ===" << std::endl;

    // Create HTTP client
    auto client = std::make_shared<http_client>();

    // Wait a moment for server to be ready (if running locally)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Test 1: GET request to root
    std::cout << "\n1. GET request to /" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    auto response1 = client->get("http://localhost:8080/");
    if (response1.is_ok())
    {
        print_response(response1.value());
    }
    else
    {
        std::cerr << "Request failed: " << response1.error().message << std::endl;
    }

    // Test 2: GET request with query parameters
    std::cout << "\n2. GET request with query parameters" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    std::map<std::string, std::string> query_params = {
        {"name", "Alice"}
    };

    auto response2 = client->get("http://localhost:8080/api/hello", query_params);
    if (response2.is_ok())
    {
        print_response(response2.value());
    }
    else
    {
        std::cerr << "Request failed: " << response2.error().message << std::endl;
    }

    // Test 3: GET request with path parameter
    std::cout << "\n3. GET request with path parameter" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    auto response3 = client->get("http://localhost:8080/users/42");
    if (response3.is_ok())
    {
        print_response(response3.value());
    }
    else
    {
        std::cerr << "Request failed: " << response3.error().message << std::endl;
    }

    // Test 4: POST request
    std::cout << "\n4. POST request" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    std::string post_body = "Hello from HTTP client!";
    auto response4 = client->post("http://localhost:8080/api/echo", post_body);
    if (response4.is_ok())
    {
        print_response(response4.value());
    }
    else
    {
        std::cerr << "Request failed: " << response4.error().message << std::endl;
    }

    // Test 5: 404 Not Found
    std::cout << "\n5. Testing 404 Not Found" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    auto response5 = client->get("http://localhost:8080/nonexistent");
    if (response5.is_ok())
    {
        print_response(response5.value());
    }
    else
    {
        std::cerr << "Request failed: " << response5.error().message << std::endl;
    }

    std::cout << "\n=== Demo complete ===" << std::endl;

    return 0;
}
