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
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace network_system::core;
using namespace std::chrono_literals;

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
        std::this_thread::sleep_for(100ms);
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
        std::this_thread::sleep_for(50ms);
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
        std::this_thread::sleep_for(10ms);
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
        std::this_thread::sleep_for(10ms);
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
        std::this_thread::sleep_for(20ms);
        server->stop_server();
        std::this_thread::sleep_for(10ms);
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
        std::this_thread::sleep_for(100ms);
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
  std::this_thread::sleep_for(100ms);

  // Multiple threads waiting for stop
  for (int i = 0; i < num_waiters; ++i) {
    threads.emplace_back([&]() {
      server->wait_for_stop();
      ++wait_completed;
    });
  }

  // Give threads time to start waiting
  std::this_thread::sleep_for(100ms);

  // Stop server (should wake all waiting threads)
  server->stop_server();

  for (auto &t : threads) {
    t.join();
  }

  EXPECT_EQ(wait_completed.load(), num_waiters);
}

// Test 10: Memory safety under concurrent access
TEST_F(NetworkThreadSafetyTest, MemorySafety) {
  const int num_iterations = 30;
  std::atomic<int> total_errors{0};

  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    auto server = std::make_shared<messaging_server>("memory_test_server");
    auto client = std::make_shared<messaging_client>("memory_test_client");

    // Thread 1: Server operations
    threads.emplace_back([&]() {
      try {
        server->start_server(9400);
        std::this_thread::sleep_for(50ms);
        server->stop_server();
      } catch (...) {
        ++errors;
      }
    });

    // Thread 2: Client operations
    threads.emplace_back([&]() {
      try {
        client->start_client("127.0.0.1", 9400);
        std::this_thread::sleep_for(50ms);
        client->stop_client();
      } catch (...) {
        // Connection may fail, that's OK
      }
    });

    // Thread 3: Send attempts
    threads.emplace_back([&]() {
      try {
        for (int i = 0; i < 10; ++i) {
          std::vector<uint8_t> data = {0x01, 0x02, 0x03};
          client->send_packet(std::move(data));
          std::this_thread::sleep_for(5ms);
        }
      } catch (...) {
        // Expected if not connected
      }
    });

    for (auto &t : threads) {
      t.join();
    }

    total_errors += errors.load();

    // Ensure all async operations complete before next iteration
    // Stop client and server explicitly before destroying
    client->stop_client();
    server->stop_server();

    // Give time for cleanup
    std::this_thread::sleep_for(10ms);
  }

  EXPECT_EQ(total_errors.load(), 0);
}
