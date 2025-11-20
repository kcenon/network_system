/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/**
 * @file udp_basic_test.cpp
 * @brief Basic unit tests for UDP functionality
 *
 * Tests cover:
 * - Server start/stop
 * - Client start/stop
 * - Basic send/receive
 * - Error conditions
 */

#include "kcenon/network/core/messaging_udp_server.h"
#include "kcenon/network/core/messaging_udp_client.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>

using namespace network_system::core;

// Test utilities
namespace test
{
    std::atomic<int> tests_passed{0};
    std::atomic<int> tests_failed{0};

    void assert_true(bool condition, const std::string& message)
    {
        if (condition)
        {
            std::cout << "[PASS] " << message << "\n";
            tests_passed++;
        }
        else
        {
            std::cerr << "[FAIL] " << message << "\n";
            tests_failed++;
        }
    }

    void print_summary()
    {
        std::cout << "\n=== Test Summary ===\n";
        std::cout << "Passed: " << tests_passed.load() << "\n";
        std::cout << "Failed: " << tests_failed.load() << "\n";
        std::cout << "Total: " << (tests_passed.load() + tests_failed.load()) << "\n";
    }
}

/**
 * @brief Test UDP server start/stop
 */
void test_server_lifecycle()
{
    std::cout << "\n--- Test: Server Lifecycle ---\n";

    auto server = std::make_shared<messaging_udp_server>("TestServer");

    // Test start
    auto result = server->start_server(5556);
    test::assert_true(result.is_ok(), "Server should start successfully");
    test::assert_true(server->is_running(), "Server should be running after start");

    // Test duplicate start
    auto dup_result = server->start_server(5556);
    test::assert_true(!dup_result.is_ok(), "Server should reject duplicate start");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test stop
    auto stop_result = server->stop_server();
    test::assert_true(stop_result.is_ok(), "Server should stop successfully");
    test::assert_true(!server->is_running(), "Server should not be running after stop");
}

/**
 * @brief Test UDP client start/stop
 */
void test_client_lifecycle()
{
    std::cout << "\n--- Test: Client Lifecycle ---\n";

    auto client = std::make_shared<messaging_udp_client>("TestClient");

    // Test start
    auto result = client->start_client("localhost", 5557);
    test::assert_true(result.is_ok(), "Client should start successfully");
    test::assert_true(client->is_running(), "Client should be running after start");

    // Test duplicate start
    auto dup_result = client->start_client("localhost", 5557);
    test::assert_true(!dup_result.is_ok(), "Client should reject duplicate start");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test stop
    auto stop_result = client->stop_client();
    test::assert_true(stop_result.is_ok(), "Client should stop successfully");
    test::assert_true(!client->is_running(), "Client should not be running after stop");
}

/**
 * @brief Test basic send/receive
 */
void test_send_receive()
{
    std::cout << "\n--- Test: Send/Receive ---\n";

    std::atomic<bool> received{false};
    std::atomic<bool> sent{false};

    // Start server
    auto server = std::make_shared<messaging_udp_server>("TestServer");
    server->set_receive_callback(
        [&received](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint&)
        {
            std::string msg(data.begin(), data.end());
            if (msg == "Test message")
            {
                received.store(true);
            }
        });

    auto server_result = server->start_server(5558);
    test::assert_true(server_result.is_ok(), "Server should start");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Start client
    auto client = std::make_shared<messaging_udp_client>("TestClient");
    auto client_result = client->start_client("localhost", 5558);
    test::assert_true(client_result.is_ok(), "Client should start");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Send message
    std::string msg = "Test message";
    std::vector<uint8_t> data(msg.begin(), msg.end());
    client->send_packet(
        std::move(data),
        [&sent](std::error_code ec, std::size_t)
        {
            if (!ec)
            {
                sent.store(true);
            }
        });

    // Wait for send/receive
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    test::assert_true(sent.load(), "Message should be sent");
    test::assert_true(received.load(), "Message should be received");

    // Cleanup
    client->stop_client();
    server->stop_server();
}

/**
 * @brief Test error conditions
 */
void test_error_conditions()
{
    std::cout << "\n--- Test: Error Conditions ---\n";

    auto client = std::make_shared<messaging_udp_client>("TestClient");

    // Test send before start
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto result = client->send_packet(std::move(data), [](std::error_code, std::size_t){});
    test::assert_true(!result.is_ok(), "Send should fail when client not started");

    // Test invalid host
    auto start_result = client->start_client("", 5559);
    test::assert_true(!start_result.is_ok(), "Client should reject empty host");
}

int main()
{
    std::cout << "=== UDP Basic Tests ===\n";

    try
    {
        test_server_lifecycle();
        test_client_lifecycle();
        test_send_receive();
        test_error_conditions();

        test::print_summary();

        return (test::tests_failed.load() == 0) ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Test exception: " << e.what() << "\n";
        return 1;
    }
}
