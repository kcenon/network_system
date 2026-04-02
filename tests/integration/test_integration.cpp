// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_integration.cpp
 * @brief Integration tests for network_system
 *
 * Tests the integration between various components including
 * thread pool, container system, and messaging bridge.
 *
 * @author kcenon
 * @date 2025-09-20

 */

#include <iostream>
#include <cassert>
#include <future>
#include <chrono>

#include "kcenon/network/network_system.h"

using namespace std::chrono_literals;

// Test thread integration
bool test_thread_integration() {
    std::cout << "\n=== Testing Thread Integration ===" << std::endl;

    auto& thread_mgr = kcenon::network::integration::thread_integration_manager::instance();
    auto pool = thread_mgr.get_thread_pool();

    // Test task submission
    auto future = pool->submit([]() {
        std::cout << "✓ Task executed in thread pool" << std::endl;
    });

    future.wait();

    // Test delayed task
    auto start = std::chrono::steady_clock::now();
    auto delayed_future = pool->submit_delayed([]() {
        std::cout << "✓ Delayed task executed" << std::endl;
    }, 100ms);

    delayed_future.wait();
    auto duration = std::chrono::steady_clock::now() - start;

    assert(duration >= 100ms);
    std::cout << "✓ Delay was "
              << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
              << "ms" << std::endl;

    // Test metrics
    auto metrics = thread_mgr.get_metrics();
    std::cout << "✓ Worker threads: " << metrics.worker_threads << std::endl;
    std::cout << "✓ Thread pool is running: " << (metrics.is_running ? "yes" : "no") << std::endl;

    return true;
}

// Test container integration
bool test_container_integration() {
    std::cout << "\n=== Testing Container Integration ===" << std::endl;

    auto& container_mgr = kcenon::network::integration::container_manager::instance();

    // Register a custom container
    auto basic = std::make_shared<kcenon::network::integration::basic_container>();
    container_mgr.register_container("test_container", basic);

    // Test serialization
    std::string test_data = "Hello, Network System!";
    auto bytes = container_mgr.serialize(std::any(test_data));

    assert(!bytes.empty());
    std::cout << "✓ Serialized " << test_data.length() << " chars to "
              << bytes.size() << " bytes" << std::endl;

    // Test deserialization
    auto result = container_mgr.deserialize(bytes);

    assert(result.has_value());
    auto deserialized = std::any_cast<std::string>(result);
    assert(deserialized == test_data);
    std::cout << "✓ Deserialized: \"" << deserialized << "\"" << std::endl;

    // Test container listing
    auto containers = container_mgr.list_containers();
    assert(!containers.empty());
    std::cout << "✓ Registered containers: ";
    for (const auto& name : containers) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    return true;
}

// Test modern API
bool test_modern_api() {
    std::cout << "\n=== Testing Modern API ===" << std::endl;

    // Test direct instantiation
    {
        auto server = std::make_shared<kcenon::network::core::messaging_server>("test_server");
        assert(server != nullptr);
        std::cout << "✓ Server creation works" << std::endl;

        auto client = std::make_shared<kcenon::network::core::messaging_client>("test_client");
        assert(client != nullptr);
        std::cout << "✓ Client creation works" << std::endl;

#ifdef BUILD_MESSAGING_BRIDGE
        auto bridge = std::make_shared<kcenon::network::integration::messaging_bridge>();
        assert(bridge != nullptr);
        std::cout << "✓ Bridge creation works" << std::endl;
#else
        std::cout << "⚠️  Skipping bridge test (BUILD_MESSAGING_BRIDGE=OFF)" << std::endl;
#endif
    }

    return true;
}

// Test messaging bridge integration (only if available)
bool test_messaging_bridge() {
#ifdef BUILD_MESSAGING_BRIDGE
    std::cout << "\n=== Testing Messaging Bridge ===" << std::endl;

    auto bridge = std::make_shared<kcenon::network::integration::messaging_bridge>();

    // Test initialization
    assert(bridge->is_initialized());
    std::cout << "✓ Bridge initialized" << std::endl;

    // Test server creation through bridge
    auto server = bridge->create_server("bridge_server");
    assert(server != nullptr);
    std::cout << "✓ Server created through bridge" << std::endl;

    // Test client creation through bridge
    auto client = bridge->create_client("bridge_client");
    assert(client != nullptr);
    std::cout << "✓ Client created through bridge" << std::endl;

    // Test thread pool interface
    auto pool = bridge->get_thread_pool_interface();
    assert(pool != nullptr);
    std::cout << "✓ Thread pool interface available" << std::endl;

    // Test metrics
    auto metrics = bridge->get_metrics();
    std::cout << "✓ Bridge metrics - connections: " << metrics.connections_active << std::endl;

    return true;
#else
    std::cout << "\n=== Testing Messaging Bridge ===" << std::endl;
    std::cout << "⚠️  Messaging bridge not available (BUILD_MESSAGING_BRIDGE=OFF)" << std::endl;
    return true; // Not a failure, just not available
#endif
}

// Initialize thread and container managers
void initialize_integration() {
    // Initialize thread pool
    auto& thread_mgr = kcenon::network::integration::thread_integration_manager::instance();
    if (!thread_mgr.get_thread_pool()) {
        thread_mgr.set_thread_pool(
            std::make_shared<kcenon::network::integration::basic_thread_pool>()
        );
    }

    // Initialize container manager
    auto& container_mgr = kcenon::network::integration::container_manager::instance();
    if (!container_mgr.get_default_container()) {
        container_mgr.set_default_container(
            std::make_shared<kcenon::network::integration::basic_container>()
        );
    }
}

// Shutdown thread and container managers
void shutdown_integration() {
    auto& thread_mgr = kcenon::network::integration::thread_integration_manager::instance();
    if (auto pool = thread_mgr.get_thread_pool()) {
        if (auto basic = std::dynamic_pointer_cast<kcenon::network::integration::basic_thread_pool>(pool)) {
            basic->stop(true);
        }
    }
}

// Main test runner
int main() {
    std::cout << "=== Network System Integration Tests ===" << std::endl;
    std::cout << "Testing network_system" << std::endl;

    int tests_passed = 0;
    int tests_failed = 0;

    // Initialize the system
    initialize_integration();
    std::cout << "\n✓ Network system initialized" << std::endl;

    // Run tests
    try {
        if (test_thread_integration()) {
            tests_passed++;
        } else {
            tests_failed++;
        }

        if (test_container_integration()) {
            tests_passed++;
        } else {
            tests_failed++;
        }

        if (test_modern_api()) {
            tests_passed++;
        } else {
            tests_failed++;
        }

        if (test_messaging_bridge()) {
            tests_passed++;
        } else {
            tests_failed++;
        }

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        tests_failed++;
    }

    // Shutdown
    shutdown_integration();
    std::cout << "\n✓ Network system shutdown" << std::endl;

    // Results
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "✅ Passed: " << tests_passed << std::endl;
    if (tests_failed > 0) {
        std::cout << "❌ Failed: " << tests_failed << std::endl;
    }
    std::cout << "🎯 Total:  " << (tests_passed + tests_failed) << std::endl;

    return tests_failed > 0 ? 1 : 0;
}