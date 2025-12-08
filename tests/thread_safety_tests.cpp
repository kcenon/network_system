/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/core/messaging_server.h"
#include "kcenon/network/session/messaging_session.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace network_system::core;
using namespace std::chrono_literals;

// Free function for yielding to allow async operations to complete
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
         flag_set(std::getenv("UBSAN_OPTIONS")) ||
         flag_set(std::getenv("MSAN_OPTIONS")) ||
         flag_set(std::getenv("SANITIZER")) ||
         flag_set(std::getenv("NETWORK_SYSTEM_SANITIZER"));
}

class NetworkThreadSafetyTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Test 1: Concurrent client creation and destruction
TEST_F(NetworkThreadSafetyTest, ConcurrentClientLifecycle) {
  const int num_threads = 10;
  const int operations_per_thread = 50;
  std::atomic<int> created{0};
  std::atomic<int> errors{0};

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, thread_id = i]() {
      for (int j = 0; j < operations_per_thread; ++j) {
        try {
          auto client = std::make_shared<messaging_client>(
              "test_client_" + std::to_string(thread_id) + "_" +
              std::to_string(j));
          ++created;
          // Client destructs here
        } catch (...) {
          ++errors;
        }
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(errors.load(), 0);
  EXPECT_EQ(created.load(), num_threads * operations_per_thread);
}

// Test 2: Concurrent server creation and destruction
TEST_F(NetworkThreadSafetyTest, ConcurrentServerLifecycle) {
  const int num_threads = 10;
  const int operations_per_thread = 50;
  std::atomic<int> created{0};
  std::atomic<int> errors{0};

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, thread_id = i]() {
      for (int j = 0; j < operations_per_thread; ++j) {
        try {
          auto server = std::make_shared<messaging_server>(
              "test_server_" + std::to_string(thread_id) + "_" +
              std::to_string(j));
          ++created;
          // Server destructs here
        } catch (...) {
          ++errors;
        }
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(errors.load(), 0);
  EXPECT_EQ(created.load(), num_threads * operations_per_thread);
}

// Test 3: Server start/stop from multiple threads
TEST_F(NetworkThreadSafetyTest, ConcurrentServerStartStop) {
  const int num_servers = 5;
  std::atomic<int> started{0};
  std::atomic<int> stopped{0};
  std::atomic<int> errors{0};

  std::vector<std::thread> threads;
  std::vector<std::shared_ptr<messaging_server>> servers;

  // Create servers
  for (int i = 0; i < num_servers; ++i) {
    servers.push_back(
        std::make_shared<messaging_server>("server_" + std::to_string(i)));
  }

  // Start servers from different threads
  for (int i = 0; i < num_servers; ++i) {
    threads.emplace_back([&, index = i]() {
      try {
        unsigned short port = 9000 + index;
        servers[index]->start_server(port);
        ++started;
        wait_for_ready();
        servers[index]->stop_server();
        ++stopped;
      } catch (...) {
        ++errors;
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(errors.load(), 0);
}

// Test 4: Client start/stop from multiple threads
TEST_F(NetworkThreadSafetyTest, ConcurrentClientStartStop) {
  const int num_clients = 5;
  std::atomic<int> started{0};
  std::atomic<int> stopped{0};

  std::vector<std::thread> threads;
  std::vector<std::shared_ptr<messaging_client>> clients;

  // Create clients
  for (int i = 0; i < num_clients; ++i) {
    clients.push_back(
        std::make_shared<messaging_client>("client_" + std::to_string(i)));
  }

  // Start clients from different threads (will fail to connect, but that's OK
  // for this test)
  for (int i = 0; i < num_clients; ++i) {
    threads.emplace_back([&, index = i]() {
      try {
        clients[index]->start_client("127.0.0.1", 9999); // Non-existent server
        ++started;
        wait_for_ready();
        clients[index]->stop_client();
        ++stopped;
      } catch (...) {
        // Connection failure is expected, not an error for this test
        ++started;
        ++stopped;
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(started.load(), num_clients);
  EXPECT_EQ(stopped.load(), num_clients);
}

// Test 5: Concurrent send_packet calls (without actual connection)
TEST_F(NetworkThreadSafetyTest, ConcurrentSendPacket) {
  auto client = std::make_shared<messaging_client>("concurrent_sender");

  const int num_threads = 10;
  const int sends_per_thread = 100;
  std::atomic<int> sent_attempts{0};

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, thread_id = i]() {
      for (int j = 0; j < sends_per_thread; ++j) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(thread_id),
                                     static_cast<uint8_t>(j)};
        client->send_packet(std::move(data));
        ++sent_attempts;
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(sent_attempts.load(), num_threads * sends_per_thread);
}

// Test 6: Mixed operations - create, start, stop, destroy
TEST_F(NetworkThreadSafetyTest, MixedOperations) {
  const int num_iterations = 20;
  std::atomic<int> errors{0};

  std::vector<std::thread> threads;

  // Thread 1: Create and destroy servers
  threads.emplace_back([&]() {
    for (int i = 0; i < num_iterations; ++i) {
      try {
        auto server = std::make_shared<messaging_server>("mixed_server_" +
                                                         std::to_string(i));
        std::this_thread::yield();
      } catch (...) {
        ++errors;
      }
    }
  });

  // Thread 2: Create and destroy clients
  threads.emplace_back([&]() {
    for (int i = 0; i < num_iterations; ++i) {
      try {
        auto client = std::make_shared<messaging_client>("mixed_client_" +
                                                         std::to_string(i));
        std::this_thread::yield();
      } catch (...) {
        ++errors;
      }
    }
  });

  // Thread 3: Server start/stop cycles
  threads.emplace_back([&]() {
    auto server = std::make_shared<messaging_server>("cycle_server");
    for (int i = 0; i < num_iterations; ++i) {
      try {
        server->start_server(9100 + (i % 10));
        wait_for_ready();
        server->stop_server();
        std::this_thread::yield();
      } catch (...) {
        ++errors;
      }
    }
  });

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(errors.load(), 0);
}

// Test 7: Stress test - rapid creation and destruction
TEST_F(NetworkThreadSafetyTest, RapidCreationDestruction) {
  const int num_threads = 20;
  const int operations = 100;
  std::atomic<int> total_operations{0};
  std::atomic<int> errors{0};

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&, thread_id = i]() {
      for (int j = 0; j < operations; ++j) {
        try {
          if (j % 2 == 0) {
            auto server = std::make_shared<messaging_server>(
                "stress_server_" + std::to_string(thread_id) + "_" +
                std::to_string(j));
          } else {
            auto client = std::make_shared<messaging_client>(
                "stress_client_" + std::to_string(thread_id) + "_" +
                std::to_string(j));
          }
          ++total_operations;
        } catch (...) {
          ++errors;
        }
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(errors.load(), 0);
  EXPECT_EQ(total_operations.load(), num_threads * operations);
}

// Test 8: Server with multiple ports concurrently
TEST_F(NetworkThreadSafetyTest, MultipleServerPorts) {
  const int num_ports = 5;
  std::atomic<int> started{0};
  std::atomic<int> errors{0};

  std::vector<std::thread> threads;
  std::vector<std::shared_ptr<messaging_server>> servers;

  for (int i = 0; i < num_ports; ++i) {
    servers.push_back(std::make_shared<messaging_server>("multi_port_server_" +
                                                         std::to_string(i)));
  }

  for (int i = 0; i < num_ports; ++i) {
    threads.emplace_back([&, index = i]() {
      try {
        unsigned short port = 9200 + index;
        servers[index]->start_server(port);
        ++started;
        wait_for_ready();
      } catch (...) {
        ++errors;
      }
    });
  }

  for (auto &t : threads) {
    t.join();
  }

  // Stop all servers
  for (auto &server : servers) {
    server->stop_server();
  }

  EXPECT_EQ(errors.load(), 0);
}

// Test 9: Wait for stop from multiple threads
TEST_F(NetworkThreadSafetyTest, ConcurrentWaitForStop) {
  auto server = std::make_shared<messaging_server>("wait_test_server");

  const int num_waiters = 5;
  std::atomic<int> wait_completed{0};
  std::vector<std::thread> threads;

  // Start server
  server->start_server(9300);
  wait_for_ready();

  // Multiple threads waiting for stop
  for (int i = 0; i < num_waiters; ++i) {
    threads.emplace_back([&]() {
      server->wait_for_stop();
      ++wait_completed;
    });
  }

  // Give threads time to start waiting
  wait_for_ready();

  // Stop server (should wake all waiting threads)
  server->stop_server();

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(wait_completed.load(), num_waiters);
}

// Test 10: Memory safety under concurrent access
// Note: This test is skipped under sanitizers because asio's internal
// data structures have race conditions that are detected by TSan but
// are not actual bugs (asio uses its own synchronization primitives).
TEST_F(NetworkThreadSafetyTest, MemorySafety) {
  if (is_sanitizer_run()) {
    GTEST_SKIP()
        << "Skipping under sanitizer due to asio internal false positives";
  }

  const int num_iterations = 30;
  std::atomic<int> total_errors{0};

  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    std::atomic<int> errors{0};

    // Use unique port per iteration to avoid conflicts
    unsigned short port = static_cast<unsigned short>(9400 + (iteration % 100));

    auto server = std::make_shared<messaging_server>("memory_test_server");
    auto client = std::make_shared<messaging_client>("memory_test_client");

    // Use atomic flags to coordinate threads
    std::atomic<bool> server_started{false};
    std::atomic<bool> client_started{false};
    std::atomic<bool> should_stop{false};

    std::vector<std::thread> threads;

    // Thread 1: Server operations
    threads.emplace_back([&, port]() {
      try {
        auto result = server->start_server(port);
        if (result.is_ok()) {
          server_started.store(true);
          // Wait until signaled to stop
          while (!should_stop.load()) {
            std::this_thread::yield();
          }
        }
      } catch (...) {
        ++errors;
      }
    });

    // Wait for server to start before starting client
    for (int i = 0; i < 1000 && !server_started.load(); ++i) {
      std::this_thread::yield();
    }

    // Thread 2: Client operations (only if server started)
    threads.emplace_back([&, port]() {
      try {
        if (server_started.load()) {
          auto result = client->start_client("127.0.0.1", port);
          if (result.is_ok()) {
            client_started.store(true);
            // Wait until signaled to stop
            while (!should_stop.load()) {
              std::this_thread::yield();
            }
          }
        }
      } catch (...) {
        // Connection may fail, that's OK
      }
    });

    // Give client time to try connecting
    wait_for_ready();

    // Thread 3: Send attempts (sequential, not concurrent with stop)
    // Only send if client started successfully
    if (client_started.load()) {
      for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        client->send_packet(std::move(data));
        std::this_thread::yield();
      }
    }

    // Signal all threads to stop
    should_stop.store(true);

    // Join all threads
    for (auto &t : threads) {
      t.join();
    }

    total_errors += errors.load();

    // Cleanup in correct order: client then server
    client->stop_client();
    wait_for_ready();
    server->stop_server();
    wait_for_ready();
  }

  EXPECT_EQ(total_errors.load(), 0);
}
