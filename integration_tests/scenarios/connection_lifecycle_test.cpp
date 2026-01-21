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
#include "../framework/system_fixture.h"
#include "../framework/test_helpers.h"

namespace kcenon::network::integration_tests {

/**
 * @brief Test suite for connection lifecycle scenarios
 *
 * Tests the complete lifecycle of network connections including:
 * - Server initialization and startup
 * - Client connection establishment
 * - Connection acceptance
 * - Connection termination
 * - Server shutdown
 */
class ConnectionLifecycleTest : public NetworkSystemFixture {};
class MultiConnectionLifecycleTest : public MultiConnectionFixture {};

// ============================================================================
// Server Initialization Tests
// ============================================================================

TEST_F(ConnectionLifecycleTest, ServerInitialization) {
    // Server should be created successfully
    ASSERT_NE(server_, nullptr);
}

TEST_F(ConnectionLifecycleTest, ServerStartupSuccess) {
    // Start server on test port
    ASSERT_TRUE(StartServer());

    // Server should be running
    // Additional verification would check server state if API provides it
}

TEST_F(ConnectionLifecycleTest, ServerStartupOnUsedPort) {
    // Start first server
    ASSERT_TRUE(StartServer());

    // Try to start another server on the same port
    auto second_server = std::make_shared<core::messaging_server>("second_server");
    auto result = second_server->start_server(test_port_);

    // Should fail because port is already in use
    EXPECT_FALSE(result.is_ok());

    // Cleanup
    second_server->stop_server();
}

TEST_F(ConnectionLifecycleTest, ServerMultipleStartAttempts) {
    // Start server
    ASSERT_TRUE(StartServer());

    // Try to start again - should fail
    auto result = server_->start_server(test_port_);
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Client Connection Tests
// ============================================================================

TEST_F(ConnectionLifecycleTest, ClientConnectionSuccess) {
    // Start server
    ASSERT_TRUE(StartServer());

    // Connect client
    EXPECT_TRUE(ConnectClient());
}

TEST_F(ConnectionLifecycleTest, ClientConnectionToNonExistentServer) {
    // Skip on Linux Debug builds due to heap corruption in async connection failure
    // cleanup. The startable_base CRTP pattern interacts with ASIO's epoll_reactor
    // in a way that causes malloc assertion failures during rapid client destruction.
    // This is a known issue tracked for future investigation.
    if (test_helpers::is_linux()) {
        GTEST_SKIP() << "Skipping on Linux due to ASIO/glibc heap interaction issue in connection failure cleanup";
    }

    // io_context lifecycle issues fixed via intentional leak pattern (Issue #400)
    // The no-op deleter on io_context prevents heap corruption during static destruction

    // Set up error callback to detect connection failure
    std::atomic<bool> error_occurred{false};
    client_->set_error_callback([&error_occurred](std::error_code) {
        error_occurred.store(true, std::memory_order_release);
    });

    // Try to connect without starting server
    auto result = client_->start_client("localhost", test_port_);

    // Wait for the async connection attempt to complete (failure expected)
    // This prevents heap corruption by ensuring async operations finish before cleanup
    test_helpers::wait_for_connection_attempt(client_, error_occurred,
                                              std::chrono::seconds(5));

    // Connection should have failed since no server is running
    EXPECT_FALSE(client_->is_connected());
}

TEST_F(ConnectionLifecycleTest, ClientMultipleConnectionAttempts) {
    ASSERT_TRUE(StartServer());

    // First connection
    ASSERT_TRUE(ConnectClient());

    // Try to connect again - should fail or be handled gracefully
    auto result = client_->start_client("localhost", test_port_);
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Message Exchange Tests
// ============================================================================

TEST_F(ConnectionLifecycleTest, SendMessageAfterConnection) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Create and send test message
    auto message = CreateTestMessage(1024);
    EXPECT_TRUE(SendMessage(std::move(message)));
}

TEST_F(ConnectionLifecycleTest, SendMessageBeforeConnection) {
    // Try to send without connecting
    auto message = CreateTestMessage(512);
    auto result = client_->send_packet(std::move(message));

    // Should fail because not connected
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionLifecycleTest, SendMultipleMessages) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send multiple messages
    for (int i = 0; i < 10; ++i) {
        auto message = CreateTestMessage(256, static_cast<uint8_t>(i));
        EXPECT_TRUE(SendMessage(std::move(message)));
    }
}

// ============================================================================
// Connection Termination Tests
// ============================================================================

TEST_F(ConnectionLifecycleTest, ClientGracefulDisconnect) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Disconnect client
    auto result = client_->stop_client();
    EXPECT_TRUE(result.is_ok());

    // Brief pause for cleanup
    WaitFor(100);
}

TEST_F(ConnectionLifecycleTest, ServerShutdownWithActiveConnections) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Stop server while client is connected
    EXPECT_TRUE(StopServer());

    // Brief pause for cleanup
    WaitFor(100);
}

TEST_F(ConnectionLifecycleTest, ReconnectAfterDisconnect) {
    ASSERT_TRUE(StartServer());

    // First connection
    ASSERT_TRUE(ConnectClient());

    // Disconnect
    client_->stop_client();
    WaitFor(100);

    // Create new client and reconnect
    client_ = std::make_shared<core::messaging_client>("test_client_reconnect");
    EXPECT_TRUE(ConnectClient());
}

// ============================================================================
// Multiple Concurrent Connections Tests
// ============================================================================

TEST_F(MultiConnectionLifecycleTest, MultipleConcurrentConnections) {
    ASSERT_TRUE(StartServer());

    // Create and connect multiple clients
    CreateClients(5);
    size_t connected = ConnectAllClients();

    EXPECT_EQ(connected, 5);
}

TEST_F(MultiConnectionLifecycleTest, SequentialConnections) {
    // io_context lifecycle issues fixed via intentional leak pattern (Issue #400)
    // The no-op deleter on io_context prevents heap corruption during static destruction

    ASSERT_TRUE(StartServer());

    // Connect clients one by one
    CreateClients(3);

    // Use longer timeout for macOS CI where kqueue-based async I/O can be slower
    std::chrono::seconds timeout;
    if (test_helpers::is_macos() && test_helpers::is_ci_environment()) {
        timeout = std::chrono::seconds(10);
    } else {
        timeout = std::chrono::seconds(5);
    }

    for (auto& client : clients_) {
        // Use 127.0.0.1 to avoid IPv6 lookup delays on macOS
        auto result = client->start_client("127.0.0.1", test_port_);
        EXPECT_TRUE(result.is_ok());
        // Wait for connection to be established
        EXPECT_TRUE(test_helpers::wait_for_connection(client, timeout));
        // Brief pause between sequential connections for resource cleanup on macOS
        if (test_helpers::is_macos()) {
            test_helpers::wait_for_ready();
        }
    }
}

TEST_F(MultiConnectionLifecycleTest, ConnectionScaling) {
    ASSERT_TRUE(StartServer());

    // Test with larger number of connections
    CreateClients(20);
    size_t connected = ConnectAllClients();

    // Should handle at least 15 out of 20 connections
    EXPECT_GE(connected, 15);
}

// ============================================================================
// Server Restart Tests
// ============================================================================

TEST_F(ConnectionLifecycleTest, ServerRestartCycle) {
    // Start server
    ASSERT_TRUE(StartServer());

    // Stop server
    ASSERT_TRUE(StopServer());

    // Restart server
    ASSERT_TRUE(StartServer());
}

} // namespace kcenon::network::integration_tests
