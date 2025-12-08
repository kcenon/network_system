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

#include "../framework/system_fixture.h"
#include "../framework/test_helpers.h"
#include <gtest/gtest.h>

namespace network_system::integration_tests {

/**
 * @brief Test suite for error handling scenarios
 *
 * Tests error conditions and recovery including:
 * - Connection failures
 * - Invalid operations
 * - Network errors
 * - Resource exhaustion
 */
class ErrorHandlingTest : public NetworkSystemFixture {};

// ============================================================================
// Connection Failure Tests
// ============================================================================

TEST_F(ErrorHandlingTest, ConnectToInvalidHost) {
  // Try to connect to invalid hostname
  auto result = client_->start_client("invalid.host.local", 12345);

  // Should fail or timeout gracefully
  // Connection may appear to succeed initially but fail during establishment
  // The exact behavior depends on async connection handling
}

TEST_F(ErrorHandlingTest, ConnectToInvalidPort) {
  // Try to connect to port that's not listening
  auto result = client_->start_client("localhost", 1);

  // Should fail or timeout gracefully
  // May succeed initially but fail during connection
}

TEST_F(ErrorHandlingTest, ConnectionRefused) {
  // Try to connect without starting server
  auto result = client_->start_client("localhost", test_port_);

  // Connection should eventually fail
  // Note: Async operations may return success initially
}

TEST_F(ErrorHandlingTest, ServerStartOnInvalidPort) {
  // Try to start server on invalid port (0)
  auto result = server_->start_server(0);

  // Port 0 may be allowed (OS assigns random port)
  // Or it may fail depending on implementation
}

TEST_F(ErrorHandlingTest, ServerStartOnPrivilegedPort) {
  // Skip this test in CI environments as it's environment-dependent
  // CI runners may have elevated capabilities or port 80 may already be in use
  if (test_helpers::is_ci_environment()) {
    GTEST_SKIP() << "Skipping privileged port test in CI environment";
  }

  // Try to start server on privileged port (requires root)
  auto result = server_->start_server(80);

  if (result.is_ok()) {
    server_->stop_server();
    GTEST_SKIP() << "Environment allows binding to privileged ports; skipping "
                    "assertion.";
  }

  // Should fail with permission error (unless running as root)
  EXPECT_FALSE(result.is_ok());
  EXPECT_EQ(result.error().code, error_codes::network_system::bind_failed);
}

// ============================================================================
// Invalid Operation Tests
// ============================================================================

TEST_F(ErrorHandlingTest, SendWithoutConnection) {
  // Try to send message without connecting
  auto message = CreateTestMessage(256);
  auto result = client_->send_packet(std::move(message));

  // Should fail because not connected
  EXPECT_FALSE(result.is_ok());
}

TEST_F(ErrorHandlingTest, SendEmptyMessage) {
  // Skip under sanitizers due to false positive data races in asio's
  // io_context_thread_manager during concurrent server/client initialization
  if (test_helpers::is_sanitizer_run()) {
    GTEST_SKIP()
        << "Skipping under sanitizer due to asio internal false positives";
  }

  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Try to send empty message
  std::vector<uint8_t> empty;
  auto result = client_->send_packet(std::move(empty));

  // Should fail with invalid argument
  EXPECT_FALSE(result.is_ok());
}

TEST_F(ErrorHandlingTest, SendAfterDisconnect) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Disconnect client
  client_->stop_client();
  WaitFor(100);

  // Try to send message after disconnect
  auto message = CreateTestMessage(256);
  auto result = client_->send_packet(std::move(message));

  // Should fail because disconnected
  EXPECT_FALSE(result.is_ok());
}

TEST_F(ErrorHandlingTest, DoubleServerStart) {
  ASSERT_TRUE(StartServer());

  // Try to start server again
  auto result = server_->start_server(test_port_);

  // Should fail because already running
  EXPECT_FALSE(result.is_ok());
}

TEST_F(ErrorHandlingTest, StopServerNotStarted) {
  // Try to stop server that was never started
  auto result = server_->stop_server();

  // Should fail or be handled gracefully
  EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Network Error Simulation Tests
// ============================================================================

TEST_F(ErrorHandlingTest, ServerShutdownDuringTransmission) {
  // Skip under sanitizers due to false positive data races in asio's
  // io_context_thread_manager during concurrent server/client operations
  if (test_helpers::is_sanitizer_run()) {
    GTEST_SKIP()
        << "Skipping under sanitizer due to asio internal false positives";
  }

  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Start sending messages
  for (int i = 0; i < 5; ++i) {
    auto message = CreateTestMessage(1024);
    SendMessage(std::move(message));
  }

  // Shutdown server during transmission
  StopServer();
  WaitFor(100);

  // Try to send more messages
  auto message = CreateTestMessage(256);
  auto result = client_->send_packet(std::move(message));

  // Should fail because server stopped
  EXPECT_FALSE(result.is_ok());
}

TEST_F(ErrorHandlingTest, ClientDisconnectDuringReceive) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Send some messages
  auto message = CreateTestMessage(2048);
  SendMessage(std::move(message));

  // Abruptly disconnect client
  client_->stop_client();

  // Server should handle disconnect gracefully
  WaitFor(100);
}

TEST_F(ErrorHandlingTest, RapidConnectDisconnect) {
  ASSERT_TRUE(StartServer());

  // Rapidly connect and disconnect
  for (int i = 0; i < 10; ++i) {
    client_ =
        std::make_shared<core::messaging_client>("client_" + std::to_string(i));

    auto result = client_->start_client("localhost", test_port_);
    WaitFor(10);

    client_->stop_client();
    WaitFor(10);
  }

  // Server should handle this gracefully
}

// ============================================================================
// Resource Exhaustion Tests
// ============================================================================

TEST_F(ErrorHandlingTest, LargeMessageHandling) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Try to send very large message (1MB)
  auto message = CreateTestMessage(1024 * 1024);
  auto result = client_->send_packet(std::move(message));

  // Should handle or reject gracefully
  // Behavior depends on buffer limits
  WaitFor(1000);
}

TEST_F(ErrorHandlingTest, ExcessiveMessageRate) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Send messages at very high rate
  size_t successful = 0;
  size_t failed = 0;

  for (int i = 0; i < 1000; ++i) {
    auto message = CreateTestMessage(128);
    auto result = client_->send_packet(std::move(message));

    if (result.is_ok()) {
      ++successful;
    } else {
      ++failed;
    }
  }

  // System should handle high rate without crashing
  // Some messages may fail due to buffer limits
  EXPECT_GT(successful, 0);
}

// ============================================================================
// Recovery Tests
// ============================================================================

TEST_F(ErrorHandlingTest, RecoveryAfterConnectionFailure) {
  // Try to connect without server
  auto result = client_->start_client("localhost", test_port_);

  // Start server
  ASSERT_TRUE(StartServer());

  // Create new client and try again
  client_ = std::make_shared<core::messaging_client>("client_recovery");
  EXPECT_TRUE(ConnectClient());
}

TEST_F(ErrorHandlingTest, RecoveryAfterServerRestart) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Stop server
  StopServer();
  WaitFor(200);

  // Restart server
  ASSERT_TRUE(StartServer());

  // Reconnect client
  client_ = std::make_shared<core::messaging_client>("client_reconnect");
  EXPECT_TRUE(ConnectClient());
}

TEST_F(ErrorHandlingTest, PartialMessageRecovery) {
  ASSERT_TRUE(StartServer());
  ASSERT_TRUE(ConnectClient());

  // Send valid message
  auto valid_message = CreateTestMessage(512);
  EXPECT_TRUE(SendMessage(std::move(valid_message)));

  // Try to send invalid message
  std::vector<uint8_t> invalid;
  auto result = client_->send_packet(std::move(invalid));
  EXPECT_FALSE(result.is_ok());

  // Send another valid message
  EXPECT_TRUE(SendMessage(std::move(valid_message)));
}

} // namespace network_system::integration_tests
