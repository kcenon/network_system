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

#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <future>
#include <atomic>

#include "network_system/compatibility.h"
#include "network_system/utils/result_types.h"

// Note: container_system integration temporarily disabled due to missing headers
// #ifdef BUILD_WITH_CONTAINER_SYSTEM
// #include <container.h>
// #include <values/string_value.h>
// #include <values/numeric_value.h>
// using namespace container_module;
// #endif

using namespace network_module;
using namespace network_system;  // For error_codes and Result types
using namespace std::chrono_literals;

// Test fixture for network tests
class NetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for network tests
    }
    
    void TearDown() override {
        // Cleanup
    }
    
    // Helper to find an available port
    unsigned short FindAvailablePort(unsigned short start = 5000) {
        for (unsigned short port = start; port < 65535; ++port) {
            try {
                asio::io_context io_context;
                asio::ip::tcp::acceptor acceptor(io_context);
                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
                acceptor.open(endpoint.protocol());
                acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                acceptor.bind(endpoint);
                acceptor.close();
                return port;
            } catch (...) {
                continue;
            }
        }
        return 0;
    }
};

// ============================================================================
// Messaging Server Tests
// ============================================================================

TEST_F(NetworkTest, ServerConstruction) {
    auto server = std::make_shared<messaging_server>("test_server");
    EXPECT_NE(server, nullptr);
}

TEST_F(NetworkTest, ServerStartStop) {
    auto server = std::make_shared<messaging_server>("test_server");

    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Start server - should succeed
    auto start_result = server->start_server(port);
    EXPECT_TRUE(start_result.is_ok()) << "Server start should succeed: "
                                       << (start_result.is_err() ? start_result.error().message : "");

    // Give it time to start
    std::this_thread::sleep_for(100ms);

    // Stop server - should succeed
    auto stop_result = server->stop_server();
    EXPECT_TRUE(stop_result.is_ok()) << "Server stop should succeed: "
                                      << (stop_result.is_err() ? stop_result.error().message : "");
}

TEST_F(NetworkTest, ServerMultipleStartStop) {
    auto server = std::make_shared<messaging_server>("test_server");

    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Multiple start/stop cycles
    for (int i = 0; i < 3; ++i) {
        auto start_result = server->start_server(port);
        EXPECT_TRUE(start_result.is_ok()) << "Server start cycle " << i << " should succeed: "
                                           << (start_result.is_err() ? start_result.error().message : "");
        std::this_thread::sleep_for(50ms);

        auto stop_result = server->stop_server();
        EXPECT_TRUE(stop_result.is_ok()) << "Server stop cycle " << i << " should succeed: "
                                          << (stop_result.is_err() ? stop_result.error().message : "");
        std::this_thread::sleep_for(50ms);
    }
}

TEST_F(NetworkTest, ServerPortAlreadyInUse) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    auto server1 = std::make_shared<messaging_server>("server1");

    // Start first server - should succeed
    auto result1 = server1->start_server(port);
    EXPECT_TRUE(result1.is_ok()) << "First server should start successfully: "
                                  << (result1.is_err() ? result1.error().message : "");
    std::this_thread::sleep_for(100ms);  // Ensure server is fully started

    // Second server on same port should return error
    auto server2 = std::make_shared<messaging_server>("server2");
    auto result2 = server2->start_server(port);
    EXPECT_TRUE(result2.is_err()) << "Second server should fail to start on same port";

    if (result2.is_err()) {
        EXPECT_EQ(result2.error().code, error_codes::network_system::bind_failed)
            << "Should return bind_failed error code, got: " << result2.error().code;
        EXPECT_FALSE(result2.error().message.empty())
            << "Error message should not be empty";
    }

    // Cleanup first server
    auto stop_result = server1->stop_server();
    EXPECT_TRUE(stop_result.is_ok()) << "Server stop should succeed";
    std::this_thread::sleep_for(50ms);  // Allow port to be released
}

// ============================================================================
// Messaging Client Tests
// ============================================================================

TEST_F(NetworkTest, ClientConstruction) {
    auto client = std::make_shared<messaging_client>("test_client");
    EXPECT_NE(client, nullptr);
}

TEST_F(NetworkTest, ClientConnectToNonExistentServer) {
    auto client = std::make_shared<messaging_client>("test_client");

    // Start client connecting to non-existent server
    // Should succeed in starting (async operation), but connection will fail
    auto start_result = client->start_client("127.0.0.1", 59999); // Unlikely port
    EXPECT_TRUE(start_result.is_ok()) << "Client start should succeed (connection is async): "
                                       << (start_result.is_err() ? start_result.error().message : "");

    // Give it a moment to try connecting (will fail in background)
    std::this_thread::sleep_for(100ms);

    // Stop the client - should succeed
    auto stop_result = client->stop_client();
    EXPECT_TRUE(stop_result.is_ok()) << "Client stop should succeed: "
                                      << (stop_result.is_err() ? stop_result.error().message : "");
}

// ============================================================================
// Client-Server Connection Tests
// ============================================================================

TEST_F(NetworkTest, ClientServerBasicConnection) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Start server
    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully: "
                                       << (server_start.is_err() ? server_start.error().message : "");

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Create and connect client
    auto client = std::make_shared<messaging_client>("test_client");
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok()) << "Client should start successfully: "
                                       << (client_start.is_err() ? client_start.error().message : "");

    // Give it time to connect
    std::this_thread::sleep_for(100ms);

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

TEST_F(NetworkTest, MultipleClientsConnection) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Start server
    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Connect multiple clients
    const int client_count = 5;
    std::vector<std::shared_ptr<messaging_client>> clients;

    for (int i = 0; i < client_count; ++i) {
        auto client = std::make_shared<messaging_client>(
            "client_" + std::to_string(i));
        auto client_start = client->start_client("127.0.0.1", port);
        EXPECT_TRUE(client_start.is_ok()) << "Client " << i << " should start successfully: "
                                           << (client_start.is_err() ? client_start.error().message : "");
        clients.push_back(client);
    }

    // Let them all connect
    std::this_thread::sleep_for(200ms);

    // Disconnect all clients
    for (size_t i = 0; i < clients.size(); ++i) {
        auto client_stop = clients[i]->stop_client();
        EXPECT_TRUE(client_stop.is_ok()) << "Client " << i << " stop should succeed";
    }

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

// ============================================================================
// Message Transfer Tests
// ============================================================================

#ifdef BUILD_WITH_CONTAINER_SYSTEM
TEST_F(NetworkTest, BasicMessageTransfer) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Start server
    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully: "
                                       << (server_start.is_err() ? server_start.error().message : "");

    // Give server time to start
    std::this_thread::sleep_for(100ms);

    // Create client
    auto client = std::make_shared<messaging_client>("test_client");
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok()) << "Client should start successfully: "
                                       << (client_start.is_err() ? client_start.error().message : "");

    // Give time to connect
    std::this_thread::sleep_for(100ms);

    // Create a message
    auto message = std::make_shared<value_container>();
    message->add(std::make_shared<string_value>("type", "test_message"));
    message->add(std::make_shared<string_value>("content", "Hello, Server!"));
    message->add(std::make_shared<int_value>("sequence", 1));

    // Serialize message to bytes
    auto serialized = message->serialize_array();

    // Send message
    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_ok()) << "Message send should succeed: "
                                      << (send_result.is_err() ? send_result.error().message : "");

    // Give time for message to be sent
    std::this_thread::sleep_for(100ms);

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

TEST_F(NetworkTest, LargeMessageTransfer) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Start server
    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully: "
                                       << (server_start.is_err() ? server_start.error().message : "");

    std::this_thread::sleep_for(100ms);

    // Create client
    auto client = std::make_shared<messaging_client>("test_client");
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok()) << "Client should start successfully: "
                                       << (client_start.is_err() ? client_start.error().message : "");

    std::this_thread::sleep_for(100ms);

    // Create a large message
    auto message = std::make_shared<value_container>();
    message->add(std::make_shared<string_value>("type", "large_message"));

    // Add 1MB of data
    std::string large_data(1024 * 1024, 'X');
    message->add(std::make_shared<string_value>("data", large_data));

    // Serialize and send
    auto serialized = message->serialize_array();
    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_ok()) << "Large message send should succeed: "
                                      << (send_result.is_err() ? send_result.error().message : "");

    std::this_thread::sleep_for(200ms);

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

TEST_F(NetworkTest, MultipleMessageTransfer) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    // Start server
    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully: "
                                       << (server_start.is_err() ? server_start.error().message : "");

    std::this_thread::sleep_for(100ms);

    // Create client
    auto client = std::make_shared<messaging_client>("test_client");
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok()) << "Client should start successfully: "
                                       << (client_start.is_err() ? client_start.error().message : "");

    std::this_thread::sleep_for(100ms);

    // Send multiple messages
    const int message_count = 10;
    for (int i = 0; i < message_count; ++i) {
        auto message = std::make_shared<value_container>();
        message->add(std::make_shared<string_value>("type", "sequence_message"));
        message->add(std::make_shared<int_value>("sequence", i));
        message->add(std::make_shared<string_value>("data", "Message " + std::to_string(i)));

        auto serialized = message->serialize_array();
        auto send_result = client->send_packet(std::move(serialized));
        EXPECT_TRUE(send_result.is_ok()) << "Message " << i << " send should succeed: "
                                          << (send_result.is_err() ? send_result.error().message : "");

        std::this_thread::sleep_for(10ms);
    }

    std::this_thread::sleep_for(100ms);

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}
#endif // BUILD_WITH_CONTAINER_SYSTEM

// ============================================================================
// Stress Tests
// ============================================================================

TEST(NetworkStressTest, RapidConnectionDisconnection) {
    // Helper to find available port
    auto findPort = []() -> unsigned short {
        for (unsigned short port = 5000; port < 65535; ++port) {
            try {
                asio::io_context io_context;
                asio::ip::tcp::acceptor acceptor(io_context);
                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
                acceptor.open(endpoint.protocol());
                acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                acceptor.bind(endpoint);
                acceptor.close();
                return port;
            } catch (...) {
                continue;
            }
        }
        return 0;
    };

    auto port = findPort();
    ASSERT_NE(port, 0) << "No available port found";

    auto server = std::make_shared<messaging_server>("stress_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    std::this_thread::sleep_for(100ms);

    // Rapid connect/disconnect cycles
    const int cycles = 10;
    for (int i = 0; i < cycles; ++i) {
        auto client = std::make_shared<messaging_client>("rapid_client_" + std::to_string(i));
        auto client_start = client->start_client("127.0.0.1", port);
        EXPECT_TRUE(client_start.is_ok()) << "Client " << i << " should start successfully";
        std::this_thread::sleep_for(20ms);
        auto client_stop = client->stop_client();
        EXPECT_TRUE(client_stop.is_ok()) << "Client " << i << " should stop successfully";
    }

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server should stop successfully";
}

TEST(NetworkStressTest, ConcurrentClients) {
    // Helper to find available port
    auto findPort = []() -> unsigned short {
        for (unsigned short port = 6000; port < 65535; ++port) {
            try {
                asio::io_context io_context;
                asio::ip::tcp::acceptor acceptor(io_context);
                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
                acceptor.open(endpoint.protocol());
                acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                acceptor.bind(endpoint);
                acceptor.close();
                return port;
            } catch (...) {
                continue;
            }
        }
        return 0;
    };

    auto port = findPort();
    ASSERT_NE(port, 0) << "No available port found";

    auto server = std::make_shared<messaging_server>("concurrent_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    std::this_thread::sleep_for(100ms);

    const int num_threads = 5;
    const int clients_per_thread = 2;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([port, t, clients_per_thread]() {
            for (int c = 0; c < clients_per_thread; ++c) {
                auto client = std::make_shared<messaging_client>(
                    "thread_" + std::to_string(t) + "_client_" + std::to_string(c));

                auto client_start = client->start_client("127.0.0.1", port);
                EXPECT_TRUE(client_start.is_ok()) << "Client should start successfully";
                std::this_thread::sleep_for(50ms);

#ifdef BUILD_WITH_CONTAINER_SYSTEM
                // Send a message
                auto message = std::make_shared<value_container>();
                message->add(std::make_shared<string_value>("thread", std::to_string(t)));
                message->add(std::make_shared<string_value>("client", std::to_string(c)));

                auto serialized = message->serialize_array();
                auto send_result = client->send_packet(std::move(serialized));
                EXPECT_TRUE(send_result.is_ok()) << "Message send should succeed";
#endif

                std::this_thread::sleep_for(50ms);
                auto client_stop = client->stop_client();
                EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server should stop successfully";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(NetworkTest, SendWithoutConnection) {
    auto client = std::make_shared<messaging_client>("disconnected_client");

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    // Create a message
    auto message = std::make_shared<value_container>();
    message->add(std::make_shared<string_value>("test", "data"));

    auto serialized = message->serialize_array();

    // Send should return error when not connected
    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_err()) << "Send without connection should return error";

    if (send_result.is_err()) {
        EXPECT_EQ(send_result.error().code, error_codes::network_system::connection_closed)
            << "Should return connection_closed error code";
    }
#else
    // Without container_system, just verify client can be created
    EXPECT_NE(client, nullptr);
#endif
}

TEST_F(NetworkTest, ServerStopWhileClientsConnected) {
    auto port = FindAvailablePort();
    ASSERT_NE(port, 0) << "No available port found";

    auto server = std::make_shared<messaging_server>("stopping_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    std::this_thread::sleep_for(100ms);

    // Connect clients
    std::vector<std::shared_ptr<messaging_client>> clients;
    for (int i = 0; i < 3; ++i) {
        auto client = std::make_shared<messaging_client>("client_" + std::to_string(i));
        auto client_start = client->start_client("127.0.0.1", port);
        EXPECT_TRUE(client_start.is_ok()) << "Client " << i << " should start successfully";
        clients.push_back(client);
    }

    std::this_thread::sleep_for(100ms);

    // Stop server while clients are connected - should still succeed
    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server should stop successfully even with connected clients";

    std::this_thread::sleep_for(100ms);

    // Stop clients - may succeed or fail depending on connection state
    for (size_t i = 0; i < clients.size(); ++i) {
        auto client_stop = clients[i]->stop_client();
        // Client stop might fail if connection was already closed by server
        // but we don't strictly require it to succeed in this test
        if (client_stop.is_err()) {
            // Log but don't fail the test - this is expected behavior
            EXPECT_NE(client_stop.error().message, "") << "Client " << i << " stop returned error (expected)";
        }
    }
}

// Main function for running tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}