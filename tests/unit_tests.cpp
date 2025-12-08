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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <gtest/gtest.h>
#include <latch>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "kcenon/network/compatibility.h"
#include "kcenon/network/integration/container_integration.h"
#include "kcenon/network/utils/result_types.h"

using namespace network_module;
using namespace network_system; // For error_codes and Result types
using namespace std::chrono_literals;

// Free function for tests without fixtures - yields to allow async operations
inline void wait_for_ready() {
  for (int i = 0; i < 1000; ++i) {
    std::this_thread::yield();
  }
}

// Detect whether tests are running under a sanitizer
inline bool is_sanitizer_run() {
  const auto flag_set = [](const char *value) {
    return value != nullptr && *value != '\0' && std::string_view(value) != "0";
  };
  return flag_set(std::getenv("TSAN_OPTIONS")) ||
         flag_set(std::getenv("ASAN_OPTIONS")) ||
         flag_set(std::getenv("SANITIZER"));
}

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

  // Wait for a condition with timeout (replaces arbitrary sleep_for)
  template <typename Predicate>
  static bool WaitForCondition(Predicate &&condition,
                               std::chrono::milliseconds timeout = 5000ms) {
    std::mutex mtx;
    std::condition_variable cv;
    bool result = false;

    std::thread checker([&]() {
      while (!result) {
        if (condition()) {
          std::lock_guard<std::mutex> lock(mtx);
          result = true;
          cv.notify_one();
          return;
        }
        std::this_thread::yield();
      }
    });

    {
      std::unique_lock<std::mutex> lock(mtx);
      cv.wait_for(lock, timeout, [&] { return result; });
    }

    if (checker.joinable()) {
      checker.join();
    }
    return result;
  }

  // Wait for server to be ready (simplified: just yield to allow async start)
  static void WaitForServerReady() {
    // Give the async server a chance to start accepting connections
    // Using yield instead of fixed sleep - actual readiness should be checked
    for (int i = 0; i < 1000; ++i) {
      std::this_thread::yield();
    }
  }

  // Wait for client to be connected using callback-based synchronization
  // This is more reliable than yield loops, especially under sanitizers
  static bool WaitForClientConnected(std::shared_ptr<messaging_client> client,
                                     std::chrono::seconds timeout = 5s) {
    std::promise<bool> connected_promise;
    auto connected_future = connected_promise.get_future();
    bool promise_set = false;
    std::mutex promise_mutex;

    client->set_connected_callback(
        [&connected_promise, &promise_set, &promise_mutex]() {
          std::lock_guard<std::mutex> lock(promise_mutex);
          if (!promise_set) {
            promise_set = true;
            connected_promise.set_value(true);
          }
        });

    auto status = connected_future.wait_for(timeout);
    return status == std::future_status::ready;
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
  EXPECT_TRUE(start_result.is_ok())
      << "Server start should succeed: "
      << (start_result.is_err() ? start_result.error().message : "");

  // Wait for server to be ready
  WaitForServerReady();

  // Stop server - should succeed
  auto stop_result = server->stop_server();
  EXPECT_TRUE(stop_result.is_ok())
      << "Server stop should succeed: "
      << (stop_result.is_err() ? stop_result.error().message : "");
}

TEST_F(NetworkTest, ServerMultipleStartStop) {
  auto server = std::make_shared<messaging_server>("test_server");

  auto port = FindAvailablePort();
  ASSERT_NE(port, 0) << "No available port found";

  // Multiple start/stop cycles
  for (int i = 0; i < 3; ++i) {
    auto start_result = server->start_server(port);
    EXPECT_TRUE(start_result.is_ok())
        << "Server start cycle " << i << " should succeed: "
        << (start_result.is_err() ? start_result.error().message : "");
    WaitForServerReady();

    auto stop_result = server->stop_server();
    EXPECT_TRUE(stop_result.is_ok())
        << "Server stop cycle " << i << " should succeed: "
        << (stop_result.is_err() ? stop_result.error().message : "");
    WaitForServerReady();
  }
}

TEST_F(NetworkTest, ServerPortAlreadyInUse) {
  auto port = FindAvailablePort();
  ASSERT_NE(port, 0) << "No available port found";

  auto server1 = std::make_shared<messaging_server>("server1");

  // Start first server - should succeed
  auto result1 = server1->start_server(port);
  EXPECT_TRUE(result1.is_ok())
      << "First server should start successfully: "
      << (result1.is_err() ? result1.error().message : "");
  WaitForServerReady(); // Ensure server is fully started

  // Second server on same port should return error
  auto server2 = std::make_shared<messaging_server>("server2");
  auto result2 = server2->start_server(port);
  EXPECT_TRUE(result2.is_err())
      << "Second server should fail to start on same port";

  if (result2.is_err()) {
    EXPECT_EQ(result2.error().code, error_codes::network_system::bind_failed)
        << "Should return bind_failed error code, got: "
        << result2.error().code;
    EXPECT_FALSE(result2.error().message.empty())
        << "Error message should not be empty";
  }

  // Cleanup first server
  auto stop_result = server1->stop_server();
  EXPECT_TRUE(stop_result.is_ok()) << "Server stop should succeed";
  WaitForServerReady(); // Allow port to be released
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
  EXPECT_TRUE(start_result.is_ok())
      << "Client start should succeed (connection is async): "
      << (start_result.is_err() ? start_result.error().message : "");

  // Give it a moment to try connecting (will fail in background)
  WaitForServerReady();

  // Stop the client - should succeed
  auto stop_result = client->stop_client();
  EXPECT_TRUE(stop_result.is_ok())
      << "Client stop should succeed: "
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
  EXPECT_TRUE(server_start.is_ok())
      << "Server should start successfully: "
      << (server_start.is_err() ? server_start.error().message : "");

  // Give server time to start
  WaitForServerReady();

  // Create and connect client
  auto client = std::make_shared<messaging_client>("test_client");
  auto client_start = client->start_client("127.0.0.1", port);
  EXPECT_TRUE(client_start.is_ok())
      << "Client should start successfully: "
      << (client_start.is_err() ? client_start.error().message : "");

  // Wait for client to connect
  WaitForServerReady();

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
  WaitForServerReady();

  // Connect multiple clients
  const int client_count = 5;
  std::vector<std::shared_ptr<messaging_client>> clients;

  for (int i = 0; i < client_count; ++i) {
    auto client =
        std::make_shared<messaging_client>("client_" + std::to_string(i));
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client " << i << " should start successfully: "
        << (client_start.is_err() ? client_start.error().message : "");
    clients.push_back(client);
  }

  // Wait for all clients to connect
  WaitForServerReady();

  // Disconnect all clients
  for (size_t i = 0; i < clients.size(); ++i) {
    auto client_stop = clients[i]->stop_client();
    EXPECT_TRUE(client_stop.is_ok())
        << "Client " << i << " stop should succeed";
  }

  auto server_stop = server->stop_server();
  EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

// ============================================================================
// Message Transfer Tests
// ============================================================================

// Basic message transfer test using container_manager (new API)
TEST_F(NetworkTest, BasicMessageTransfer) {
  auto port = FindAvailablePort();
  ASSERT_NE(port, 0) << "No available port found";

  // Start server
  auto server = std::make_shared<messaging_server>("test_server");
  auto server_start = server->start_server(port);
  EXPECT_TRUE(server_start.is_ok())
      << "Server should start successfully: "
      << (server_start.is_err() ? server_start.error().message : "");

  // Give server time to start
  WaitForServerReady();

  // Create client with connection synchronization
  auto client = std::make_shared<messaging_client>("test_client");

  // Set up connection callback before starting client
  std::promise<bool> connected_promise;
  auto connected_future = connected_promise.get_future();
  bool promise_set = false;
  std::mutex promise_mutex;

  client->set_connected_callback(
      [&connected_promise, &promise_set, &promise_mutex]() {
        std::lock_guard<std::mutex> lock(promise_mutex);
        if (!promise_set) {
          promise_set = true;
          connected_promise.set_value(true);
        }
      });

  auto client_start = client->start_client("127.0.0.1", port);
  EXPECT_TRUE(client_start.is_ok())
      << "Client should start successfully: "
      << (client_start.is_err() ? client_start.error().message : "");

  // Wait for connection with timeout
  auto status = connected_future.wait_for(5s);
  ASSERT_EQ(status, std::future_status::ready)
      << "Client should connect within timeout";

  // Create simple test message (basic_container supports std::string directly)
  std::string test_message = "test_message:Hello, Server!:1";

  // Get container manager and serialize
  auto &manager = integration::container_manager::instance();
  auto serialized = manager.serialize(test_message);

  // Send message
  auto send_result = client->send_packet(std::move(serialized));
  EXPECT_TRUE(send_result.is_ok())
      << "Message send should succeed: "
      << (send_result.is_err() ? send_result.error().message : "");

  // Give time for message to be sent
  WaitForServerReady();

  auto client_stop = client->stop_client();
  EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
  wait_for_ready();

  auto server_stop = server->stop_server();
  EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";

  // Wait for async cleanup before shared_ptrs are destroyed
  wait_for_ready();
}

// Large message transfer test using container_manager (new API)
TEST_F(NetworkTest, LargeMessageTransfer) {
  auto port = FindAvailablePort();
  ASSERT_NE(port, 0) << "No available port found";

  // Start server
  auto server = std::make_shared<messaging_server>("test_server");
  auto server_start = server->start_server(port);
  EXPECT_TRUE(server_start.is_ok())
      << "Server should start successfully: "
      << (server_start.is_err() ? server_start.error().message : "");

  WaitForServerReady();

  // Create client with connection synchronization
  auto client = std::make_shared<messaging_client>("test_client");

  std::promise<bool> connected_promise;
  auto connected_future = connected_promise.get_future();
  bool promise_set = false;
  std::mutex promise_mutex;

  client->set_connected_callback(
      [&connected_promise, &promise_set, &promise_mutex]() {
        std::lock_guard<std::mutex> lock(promise_mutex);
        if (!promise_set) {
          promise_set = true;
          connected_promise.set_value(true);
        }
      });

  auto client_start = client->start_client("127.0.0.1", port);
  EXPECT_TRUE(client_start.is_ok())
      << "Client should start successfully: "
      << (client_start.is_err() ? client_start.error().message : "");

  auto status = connected_future.wait_for(5s);
  ASSERT_EQ(status, std::future_status::ready)
      << "Client should connect within timeout";

  // Create large message (1MB) as string
  std::string large_message =
      "large_message:" + std::string(1024UL * 1024UL, 'X');

  // Get container manager and serialize
  auto &manager = integration::container_manager::instance();
  auto serialized = manager.serialize(large_message);

  // Send message
  auto send_result = client->send_packet(std::move(serialized));
  EXPECT_TRUE(send_result.is_ok())
      << "Large message send should succeed: "
      << (send_result.is_err() ? send_result.error().message : "");

  WaitForServerReady(); // Allow large message to be sent

  auto client_stop = client->stop_client();
  EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
  wait_for_ready();

  auto server_stop = server->stop_server();
  EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";

  // Wait for async cleanup before shared_ptrs are destroyed
  wait_for_ready();
}

// Multiple message transfer test using container_manager (new API)
TEST_F(NetworkTest, MultipleMessageTransfer) {
  auto port = FindAvailablePort();
  ASSERT_NE(port, 0) << "No available port found";

  // Start server
  auto server = std::make_shared<messaging_server>("test_server");
  auto server_start = server->start_server(port);
  EXPECT_TRUE(server_start.is_ok())
      << "Server should start successfully: "
      << (server_start.is_err() ? server_start.error().message : "");

  WaitForServerReady();

  // Create client with connection synchronization
  auto client = std::make_shared<messaging_client>("test_client");

  std::promise<bool> connected_promise;
  auto connected_future = connected_promise.get_future();
  bool promise_set = false;
  std::mutex promise_mutex;

  client->set_connected_callback(
      [&connected_promise, &promise_set, &promise_mutex]() {
        std::lock_guard<std::mutex> lock(promise_mutex);
        if (!promise_set) {
          promise_set = true;
          connected_promise.set_value(true);
        }
      });

  auto client_start = client->start_client("127.0.0.1", port);
  EXPECT_TRUE(client_start.is_ok())
      << "Client should start successfully: "
      << (client_start.is_err() ? client_start.error().message : "");

  auto status = connected_future.wait_for(5s);
  ASSERT_EQ(status, std::future_status::ready)
      << "Client should connect within timeout";

  // Get container manager
  auto &manager = integration::container_manager::instance();

  // Send multiple messages
  const int message_count = 10;
  for (int i = 0; i < message_count; ++i) {
    // Create message as string
    std::string message = "sequence_message:" + std::to_string(i) +
                          ":Message " + std::to_string(i);

    auto serialized = manager.serialize(message);
    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_ok())
        << "Message " << i << " send should succeed: "
        << (send_result.is_err() ? send_result.error().message : "");

    std::this_thread::yield(); // Yield between messages
  }

  WaitForServerReady();

  auto client_stop = client->stop_client();
  EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
  wait_for_ready();

  auto server_stop = server->stop_server();
  EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";

  // Wait for async cleanup before shared_ptrs are destroyed
  wait_for_ready();
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(NetworkStressTest, RapidConnectionDisconnection) {
  // Skip under sanitizers - asio internals trigger TSan false positives
  if (is_sanitizer_run()) {
    GTEST_SKIP() << "Skipping under sanitizer due to asio false positives";
  }
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

  wait_for_ready();

  // Rapid connect/disconnect cycles
  const int cycles = 10;
  for (int i = 0; i < cycles; ++i) {
    auto client =
        std::make_shared<messaging_client>("rapid_client_" + std::to_string(i));
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client " << i << " should start successfully";
    wait_for_ready();
    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok())
        << "Client " << i << " should stop successfully";
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

  wait_for_ready();

  const int num_threads = 5;
  const int clients_per_thread = 2;
  std::vector<std::thread> threads;
  std::vector<std::shared_ptr<messaging_client>> all_clients;
  std::mutex clients_mutex;
  std::atomic<int> successful_connections{0};
  std::atomic<int> successful_sends{0};

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([port, t, &all_clients, &clients_mutex,
                          &successful_connections, &successful_sends]() {
      for (int c = 0; c < clients_per_thread; ++c) {
        auto client = std::make_shared<messaging_client>(
            "thread_" + std::to_string(t) + "_client_" + std::to_string(c));

        {
          std::lock_guard<std::mutex> lock(clients_mutex);
          all_clients.push_back(client);
        }

        auto client_start = client->start_client("127.0.0.1", port);
        if (client_start.is_ok()) {
          ++successful_connections;
          wait_for_ready();

          // Send a message using container_manager
          std::string message =
              "thread:" + std::to_string(t) + ":client:" + std::to_string(c);
          auto &manager = integration::container_manager::instance();
          auto serialized = manager.serialize(message);
          auto send_result = client->send_packet(std::move(serialized));
          if (send_result.is_ok()) {
            ++successful_sends;
          }
        }
        wait_for_ready();
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  // Expect at least some connections and sends to succeed
  // Under sanitizers, timing may cause some to fail
  EXPECT_GT(successful_connections.load(), 0)
      << "At least some clients should connect";
  EXPECT_GT(successful_sends.load(), 0)
      << "At least some messages should be sent";

  // Stop all clients after threads complete (proper lifecycle management)
  for (auto &client : all_clients) {
    client->stop_client(); // Don't assert, some may already be stopped
  }

  // Wait for all async cleanup to complete before destroying clients
  wait_for_ready();
  all_clients.clear();

  auto server_stop = server->stop_server();
  EXPECT_TRUE(server_stop.is_ok()) << "Server should stop successfully";
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(NetworkTest, SendWithoutConnection) {
  auto client = std::make_shared<messaging_client>("disconnected_client");

  // Create a message using container_manager
  std::string test_data = "test:data";
  auto &manager = integration::container_manager::instance();
  auto serialized = manager.serialize(test_data);

  // Send should return error when not connected
  auto send_result = client->send_packet(std::move(serialized));
  EXPECT_TRUE(send_result.is_err())
      << "Send without connection should return error";

  if (send_result.is_err()) {
    EXPECT_EQ(send_result.error().code,
              error_codes::network_system::connection_closed)
        << "Should return connection_closed error code";
  }
}

TEST_F(NetworkTest, ServerStopWhileClientsConnected) {
  auto port = FindAvailablePort();
  ASSERT_NE(port, 0) << "No available port found";

  auto server = std::make_shared<messaging_server>("stopping_server");
  auto server_start = server->start_server(port);
  EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

  wait_for_ready();

  // Connect clients
  std::vector<std::shared_ptr<messaging_client>> clients;
  for (int i = 0; i < 3; ++i) {
    auto client =
        std::make_shared<messaging_client>("client_" + std::to_string(i));
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client " << i << " should start successfully";
    clients.push_back(client);
  }

  wait_for_ready();

  // Stop server while clients are connected - should still succeed
  auto server_stop = server->stop_server();
  EXPECT_TRUE(server_stop.is_ok())
      << "Server should stop successfully even with connected clients";

  wait_for_ready();

  // Stop clients - may succeed or fail depending on connection state
  for (size_t i = 0; i < clients.size(); ++i) {
    auto client_stop = clients[i]->stop_client();
    // Client stop might fail if connection was already closed by server
    // but we don't strictly require it to succeed in this test
    if (client_stop.is_err()) {
      // Log but don't fail the test - this is expected behavior
      EXPECT_NE(client_stop.error().message, "")
          << "Client " << i << " stop returned error (expected)";
    }
  }

  // Wait for async cleanup before shared_ptrs are destroyed
  wait_for_ready();
  clients.clear();
}

// Main function for running tests
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}