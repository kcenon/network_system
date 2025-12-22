// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file test_compatibility.cpp
 * @brief Enhanced compatibility tests for network_system
 *
 * Tests comprehensive compatibility between legacy messaging_system
 * code and the new network_system implementation.
 *
 * @author kcenon
 * @date 2025-09-20

 */

#include <kcenon/common/config/feature_flags.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "kcenon/network/compatibility.h"

using namespace std::chrono_literals;

// Free function for yielding to allow async operations to complete
inline void wait_for_ready() {
    for (int i = 0; i < 1000; ++i) {
        std::this_thread::yield();
    }
}


// Test results tracking
struct TestResults {
  int passed = 0;
  int failed = 0;
  std::vector<std::string> failures;

  void record_pass(const std::string &test_name) {
    passed++;
    std::cout << "✅ " << test_name << std::endl;
  }

  void record_fail(const std::string &test_name, const std::string &reason) {
    failed++;
    failures.push_back(test_name + ": " + reason);
    std::cout << "❌ " << test_name << " - " << reason << std::endl;
  }

  void print_summary() {
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (!failures.empty()) {
      std::cout << "\nFailures:" << std::endl;
      for (const auto &failure : failures) {
        std::cout << "  - " << failure << std::endl;
      }
    }
  }
};

// Test legacy namespace aliases
void test_legacy_namespaces(TestResults &results) {
  std::cout << "\n=== Testing Legacy Namespaces ===" << std::endl;

  // Test network_module namespace
  try {
    auto server = network_module::create_server("test_server");
    if (server) {
      results.record_pass("network_module::create_server");
    } else {
      results.record_fail("network_module::create_server", "returned nullptr");
    }

    auto client = network_module::create_client("test_client");
    if (client) {
      results.record_pass("network_module::create_client");
    } else {
      results.record_fail("network_module::create_client", "returned nullptr");
    }

#ifdef BUILD_MESSAGING_BRIDGE
    auto bridge = network_module::create_bridge();
    if (bridge) {
      results.record_pass("network_module::create_bridge");
    } else {
      results.record_fail("network_module::create_bridge", "returned nullptr");
    }
#else
    std::cout << "⚠️  Skipping create_bridge (BUILD_MESSAGING_BRIDGE=OFF)"
              << std::endl;
#endif
  } catch (const std::exception &e) {
    results.record_fail("network_module namespace", e.what());
  }

  // Test messaging namespace
  try {
    auto server = messaging::create_server("msg_server");
    if (server) {
      results.record_pass("messaging::create_server");
    } else {
      results.record_fail("messaging::create_server", "returned nullptr");
    }
  } catch (const std::exception &e) {
    results.record_fail("messaging namespace", e.what());
  }
}

// Test type aliases
void test_type_aliases(TestResults &results) {
  std::cout << "\n=== Testing Type Aliases ===" << std::endl;

  // Test that types are correctly aliased
  try {
    // Server type
    [[maybe_unused]] network_module::messaging_server *server_ptr = nullptr;
    results.record_pass("network_module::messaging_server type");

    // Client type
    [[maybe_unused]] network_module::messaging_client *client_ptr = nullptr;
    results.record_pass("network_module::messaging_client type");

    // Session type
    [[maybe_unused]] network_module::messaging_session *session_ptr = nullptr;
    results.record_pass("network_module::messaging_session type");

#ifdef BUILD_MESSAGING_BRIDGE
    // Bridge type
    [[maybe_unused]] network_module::messaging_bridge *bridge_ptr = nullptr;
    results.record_pass("network_module::messaging_bridge type");
#endif

    // Thread pool type
    [[maybe_unused]] network_module::thread_pool_interface *pool_ptr = nullptr;
    results.record_pass("network_module::thread_pool_interface type");

    // Container type
    [[maybe_unused]] network_module::container_interface *container_ptr =
        nullptr;
    results.record_pass("network_module::container_interface type");

  } catch (const std::exception &e) {
    results.record_fail("Type aliases", e.what());
  }
}

// Test feature detection macros
void test_feature_detection(TestResults &results) {
  std::cout << "\n=== Testing Feature Detection ===" << std::endl;

  // Test has_container_support
  bool container_support = network_system::compat::has_container_support();
  std::cout << "Container support: " << (container_support ? "YES" : "NO")
            << std::endl;

#if KCENON_WITH_CONTAINER_SYSTEM
  if (container_support) {
    results.record_pass("Container support detection");
  } else {
    results.record_fail("Container support detection",
                        "Should be true with KCENON_WITH_CONTAINER_SYSTEM");
  }
#else
  if (!container_support) {
    results.record_pass("Container support detection");
  } else {
    results.record_fail("Container support detection",
                        "Should be false without KCENON_WITH_CONTAINER_SYSTEM");
  }
#endif

  // Test has_thread_support
  bool thread_support = network_system::compat::has_thread_support();
  std::cout << "Thread support: " << (thread_support ? "YES" : "NO")
            << std::endl;

#if KCENON_WITH_THREAD_SYSTEM
  if (thread_support) {
    results.record_pass("Thread support detection");
  } else {
    results.record_fail("Thread support detection",
                        "Should be true with KCENON_WITH_THREAD_SYSTEM");
  }
#else
  if (!thread_support) {
    results.record_pass("Thread support detection");
  } else {
    results.record_fail("Thread support detection",
                        "Should be false without KCENON_WITH_THREAD_SYSTEM");
  }
#endif
}

// Test initialization and shutdown
void test_init_shutdown(TestResults &results) {
  std::cout << "\n=== Testing Init/Shutdown ===" << std::endl;

  try {
    // Initialize
    network_system::compat::initialize();
    results.record_pass("network_system::compat::initialize");

    // Verify thread pool is initialized
    auto &thread_mgr = network_module::thread_integration_manager::instance();
    auto pool = thread_mgr.get_thread_pool();
    if (pool) {
      results.record_pass("Thread pool initialized");
    } else {
      results.record_fail("Thread pool initialized", "Pool is null");
    }

    // Verify container manager is initialized
    auto &container_mgr = network_module::container_manager::instance();
    auto container = container_mgr.get_default_container();
    if (container) {
      results.record_pass("Container manager initialized");
    } else {
      results.record_fail("Container manager initialized", "Container is null");
    }

    // Shutdown
    network_system::compat::shutdown();
    results.record_pass("network_system::compat::shutdown");

  } catch (const std::exception &e) {
    results.record_fail("Init/Shutdown", e.what());
  }
}

// Test cross-compatibility
void test_cross_compatibility(TestResults &results) {
  std::cout << "\n=== Testing Cross-Compatibility ===" << std::endl;

  try {
    // Create objects using different namespaces
    auto legacy_server = network_module::create_server("legacy");
    auto modern_server =
        std::make_shared<kcenon::network::core::messaging_server>("modern");

    if (legacy_server && modern_server) {
      results.record_pass("Mixed namespace object creation");
    } else {
      results.record_fail("Mixed namespace object creation",
                          "One or both objects null");
    }

#ifdef BUILD_MESSAGING_BRIDGE
    // Test that legacy bridge works with modern objects
    auto legacy_bridge = network_module::create_bridge();
    auto modern_client = legacy_bridge->create_client("bridge_client");

    if (modern_client) {
      results.record_pass("Legacy bridge creates modern client");
    } else {
      results.record_fail("Legacy bridge creates modern client",
                          "Client is null");
    }
#else
    std::cout << "⚠️  Skipping legacy bridge test (BUILD_MESSAGING_BRIDGE=OFF)"
              << std::endl;
#endif

  } catch (const std::exception &e) {
    results.record_fail("Cross-compatibility", e.what());
  }
}

// Test actual message passing
void test_message_passing(TestResults &results) {
  std::cout << "\n=== Testing Message Passing ===" << std::endl;

  try {
    // Create server using legacy API
    auto server = network_module::create_server("compat_server");
    server->start_server(7070);

    // Wait for server to start
    wait_for_ready();

    // Create client using legacy API
    auto client = network_module::create_client("compat_client");
    client->start_client("127.0.0.1", 7070);

    // Wait for connection
    wait_for_ready();

    // Send test message
    std::string test_msg = "Compatibility test message";
    std::vector<uint8_t> data(test_msg.begin(), test_msg.end());
    client->send_packet(std::move(data));

    results.record_pass("Legacy API message send");

    // Wait for message processing
    wait_for_ready();

    // Stop client and server
    client->stop_client();
    server->stop_server();

    results.record_pass("Legacy API cleanup");

  } catch (const std::exception &e) {
    results.record_fail("Message passing", e.what());
  }
}

// Test container integration through legacy API
void test_legacy_container_integration(TestResults &results) {
  std::cout << "\n=== Testing Legacy Container Integration ===" << std::endl;

  try {
    auto &container_mgr = network_module::container_manager::instance();

    // Create and register container using legacy namespace
    auto container = std::make_shared<network_module::basic_container>();
    container_mgr.register_container("legacy_test", container);

    // Test serialization
    std::string data = "Legacy container test";
    auto serialized = container->serialize(std::any(data));

    if (!serialized.empty()) {
      results.record_pass("Legacy container serialization");
    } else {
      results.record_fail("Legacy container serialization", "Empty result");
    }

    // Test deserialization
    auto deserialized = container->deserialize(serialized);
    if (deserialized.has_value()) {
      auto result = std::any_cast<std::string>(deserialized);
      if (result == data) {
        results.record_pass("Legacy container deserialization");
      } else {
        results.record_fail("Legacy container deserialization",
                            "Data mismatch");
      }
    } else {
      results.record_fail("Legacy container deserialization", "No value");
    }

    // List containers
    auto containers = container_mgr.list_containers();
    if (!containers.empty()) {
      results.record_pass("Legacy container listing");
    } else {
      results.record_fail("Legacy container listing", "Empty list");
    }

  } catch (const std::exception &e) {
    results.record_fail("Legacy container integration", e.what());
  }
}

// Test thread pool through legacy API
void test_legacy_thread_integration(TestResults &results) {
  std::cout << "\n=== Testing Legacy Thread Integration ===" << std::endl;

  try {
    auto &thread_mgr = network_module::thread_integration_manager::instance();

    // Submit task using legacy namespace
    bool task_executed = false;
    auto future =
        thread_mgr.submit_task([&task_executed]() { task_executed = true; });

    future.wait();

    if (task_executed) {
      results.record_pass("Legacy thread task execution");
    } else {
      results.record_fail("Legacy thread task execution", "Task not executed");
    }

    // Test delayed task
    auto start = std::chrono::steady_clock::now();
    auto delayed = thread_mgr.submit_delayed_task([]() {}, 100ms);
    delayed.wait();
    auto duration = std::chrono::steady_clock::now() - start;

    if (duration >= 100ms) {
      results.record_pass("Legacy thread delayed task");
    } else {
      results.record_fail("Legacy thread delayed task", "Delay too short");
    }

    // Get metrics
    auto metrics = thread_mgr.get_metrics();
    if (metrics.worker_threads > 0) {
      results.record_pass("Legacy thread metrics");
    } else {
      results.record_fail("Legacy thread metrics", "No worker threads");
    }

  } catch (const std::exception &e) {
    results.record_fail("Legacy thread integration", e.what());
  }
}

int main() {
  std::cout << "=== Network System Compatibility Tests ===" << std::endl;
  std::cout << "Build: "
#if KCENON_WITH_CONTAINER_SYSTEM
            << "Container+"
#endif
#if KCENON_WITH_THREAD_SYSTEM
            << "Thread+"
#endif
#if KCENON_WITH_LOGGER_SYSTEM
            << "Logger+"
#endif
            << "Core" << std::endl;

  TestResults results;

  // Initialize system
  network_system::compat::initialize();

  // Run all tests
  test_legacy_namespaces(results);
  test_type_aliases(results);
  test_feature_detection(results);
  test_init_shutdown(results);
  test_cross_compatibility(results);
  test_message_passing(results);
  test_legacy_container_integration(results);
  test_legacy_thread_integration(results);

  // Cleanup
  network_system::compat::shutdown();

  // Print results
  results.print_summary();

  return results.failed > 0 ? 1 : 0;
}