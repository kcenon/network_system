/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "network_system/core/http_server.h"
#include "network_system/core/http_client.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

using namespace network_system::core;
using namespace network_system::internal;

class HTTPIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        server_port_ = 8081;  // Use non-standard port for testing
    }

    void TearDown() override
    {
        if (server_)
        {
            server_->stop();
        }
        // Give server time to clean up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Helper to start server on a background thread
    void StartServerAsync(std::shared_ptr<http_server> server)
    {
        server_ = server;
        server_thread_ = std::thread([this]() {
            auto result = server_->start(server_port_);
            ASSERT_TRUE(result.is_ok()) << "Server failed to start: " << result.error().message;
        });

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void StopServer()
    {
        if (server_)
        {
            server_->stop();
        }
        if (server_thread_.joinable())
        {
            server_thread_.join();
        }
    }

    std::shared_ptr<http_server> server_;
    std::thread server_thread_;
    unsigned short server_port_;
};

// Test basic GET request
TEST_F(HTTPIntegrationTest, BasicGETRequest)
{
    auto server = std::make_shared<http_server>("test_server");

    std::atomic<bool> handler_called{false};

    server->get("/", [&handler_called](const http_request_context& ctx) {
        handler_called = true;
        http_response response;
        response.status_code = 200;
        response.set_body_string("Hello, World!");
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    StartServerAsync(server);

    // Create client and send request
    auto client = std::make_shared<http_client>();
    auto result = client->get("http://localhost:" + std::to_string(server_port_) + "/");

    ASSERT_TRUE(result.is_ok()) << "GET request failed: " << result.error().message;
    const auto& response = result.value();

    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.get_body_string(), "Hello, World!");
    EXPECT_TRUE(handler_called);

    StopServer();
}

// Test POST request with body
TEST_F(HTTPIntegrationTest, POSTRequestWithBody)
{
    auto server = std::make_shared<http_server>("test_server");

    std::atomic<bool> handler_called{false};
    std::string received_body;

    server->post("/api/echo", [&handler_called, &received_body](const http_request_context& ctx) {
        handler_called = true;
        received_body = ctx.request.get_body_string();

        http_response response;
        response.status_code = 200;
        response.set_body_string("Echo: " + received_body);
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    StartServerAsync(server);

    // Create client and send POST request
    auto client = std::make_shared<http_client>();
    std::string test_data = "Test message";
    auto result = client->post("http://localhost:" + std::to_string(server_port_) + "/api/echo", test_data);

    ASSERT_TRUE(result.is_ok()) << "POST request failed: " << result.error().message;
    const auto& response = result.value();

    EXPECT_EQ(response.status_code, 200);
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(received_body, test_data);
    EXPECT_EQ(response.get_body_string(), "Echo: " + test_data);

    StopServer();
}

// Test query parameters
TEST_F(HTTPIntegrationTest, QueryParameters)
{
    auto server = std::make_shared<http_server>("test_server");

    server->get("/search", [](const http_request_context& ctx) {
        auto query = ctx.get_query_param("q").value_or("none");
        auto page = ctx.get_query_param("page").value_or("1");

        http_response response;
        response.status_code = 200;
        response.set_body_string("Query: " + query + ", Page: " + page);
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    StartServerAsync(server);

    auto client = std::make_shared<http_client>();
    auto result = client->get("http://localhost:" + std::to_string(server_port_) + "/search?q=test&page=2");

    ASSERT_TRUE(result.is_ok());
    const auto& response = result.value();

    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.get_body_string(), "Query: test, Page: 2");

    StopServer();
}

// Test path parameters
TEST_F(HTTPIntegrationTest, PathParameters)
{
    auto server = std::make_shared<http_server>("test_server");

    server->get("/users/:id", [](const http_request_context& ctx) {
        auto user_id = ctx.get_path_param("id").value_or("unknown");

        http_response response;
        response.status_code = 200;
        response.set_body_string("User ID: " + user_id);
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    StartServerAsync(server);

    auto client = std::make_shared<http_client>();
    auto result = client->get("http://localhost:" + std::to_string(server_port_) + "/users/123");

    ASSERT_TRUE(result.is_ok());
    const auto& response = result.value();

    EXPECT_EQ(response.status_code, 200);
    EXPECT_EQ(response.get_body_string(), "User ID: 123");

    StopServer();
}

// Test 404 Not Found
TEST_F(HTTPIntegrationTest, NotFoundError)
{
    auto server = std::make_shared<http_server>("test_server");

    server->get("/exists", [](const http_request_context&) {
        http_response response;
        response.status_code = 200;
        response.set_body_string("Found");
        return response;
    });

    StartServerAsync(server);

    auto client = std::make_shared<http_client>();
    auto result = client->get("http://localhost:" + std::to_string(server_port_) + "/nonexistent");

    ASSERT_TRUE(result.is_ok());
    const auto& response = result.value();

    EXPECT_EQ(response.status_code, 404);

    StopServer();
}

// Test large payload
TEST_F(HTTPIntegrationTest, LargePayload)
{
    auto server = std::make_shared<http_server>("test_server");

    std::atomic<bool> handler_called{false};
    std::size_t received_size = 0;

    server->post("/upload", [&handler_called, &received_size](const http_request_context& ctx) {
        handler_called = true;
        received_size = ctx.request.body.size();

        http_response response;
        response.status_code = 200;
        response.set_body_string("Received " + std::to_string(received_size) + " bytes");
        return response;
    });

    StartServerAsync(server);

    // Send 100KB payload
    std::string large_data(100 * 1024, 'x');
    auto client = std::make_shared<http_client>();
    auto result = client->post("http://localhost:" + std::to_string(server_port_) + "/upload", large_data);

    ASSERT_TRUE(result.is_ok()) << "Large payload request failed: " << result.error().message;
    const auto& response = result.value();

    EXPECT_EQ(response.status_code, 200);
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(received_size, large_data.size());

    StopServer();
}

// Test custom headers
TEST_F(HTTPIntegrationTest, CustomHeaders)
{
    auto server = std::make_shared<http_server>("test_server");

    std::string received_auth_header;

    server->get("/protected", [&received_auth_header](const http_request_context& ctx) {
        received_auth_header = ctx.request.get_header("Authorization").value_or("");

        http_response response;
        if (received_auth_header == "Bearer secret-token") {
            response.status_code = 200;
            response.set_body_string("Authorized");
        } else {
            response.status_code = 401;
            response.set_body_string("Unauthorized");
        }
        return response;
    });

    StartServerAsync(server);

    auto client = std::make_shared<http_client>();

    // First request without auth header
    auto result1 = client->get("http://localhost:" + std::to_string(server_port_) + "/protected");
    ASSERT_TRUE(result1.is_ok());
    EXPECT_EQ(result1.value().status_code, 401);

    // Second request with auth header
    auto result2 = client->get("http://localhost:" + std::to_string(server_port_) + "/protected",
                               {}, {{"Authorization", "Bearer secret-token"}});
    ASSERT_TRUE(result2.is_ok());
    EXPECT_EQ(result2.value().status_code, 200);
    EXPECT_EQ(result2.value().get_body_string(), "Authorized");

    StopServer();
}

// Test handler exception handling
TEST_F(HTTPIntegrationTest, HandlerException)
{
    auto server = std::make_shared<http_server>("test_server");

    server->get("/crash", [](const http_request_context&) -> http_response {
        throw std::runtime_error("Simulated crash");
    });

    StartServerAsync(server);

    auto client = std::make_shared<http_client>();
    auto result = client->get("http://localhost:" + std::to_string(server_port_) + "/crash");

    ASSERT_TRUE(result.is_ok());
    const auto& response = result.value();

    // Server should return 500 Internal Server Error
    EXPECT_EQ(response.status_code, 500);

    StopServer();
}

// Test multiple concurrent requests
TEST_F(HTTPIntegrationTest, ConcurrentRequests)
{
    auto server = std::make_shared<http_server>("test_server");

    std::atomic<int> request_count{0};

    server->get("/count", [&request_count](const http_request_context&) {
        request_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work

        http_response response;
        response.status_code = 200;
        response.set_body_string("OK");
        return response;
    });

    StartServerAsync(server);

    // Launch 10 concurrent requests
    const int num_requests = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_requests; ++i) {
        threads.emplace_back([this, &success_count]() {
            auto client = std::make_shared<http_client>();
            auto result = client->get("http://localhost:" + std::to_string(server_port_) + "/count");
            if (result.is_ok() && result.value().status_code == 200) {
                success_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count, num_requests);
    EXPECT_EQ(request_count, num_requests);

    StopServer();
}

// Main function
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
