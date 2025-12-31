// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
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
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
#include "kcenon/network/compatibility.h"

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

// Test compatibility API
bool test_compatibility_api() {
    std::cout << "\n=== Testing Compatibility API ===" << std::endl;

    // Test namespace aliases
    {
        auto server = network_module::create_server("test_server");
        assert(server != nullptr);
        std::cout << "✓ Legacy server creation works" << std::endl;

        auto client = network_module::create_client("test_client");
        assert(client != nullptr);
        std::cout << "✓ Legacy client creation works" << std::endl;

#ifdef BUILD_MESSAGING_BRIDGE
        auto bridge = network_module::create_bridge();
        assert(bridge != nullptr);
        std::cout << "✓ Legacy bridge creation works" << std::endl;
#else
        std::cout << "⚠️  Skipping bridge test (BUILD_MESSAGING_BRIDGE=OFF)" << std::endl;
#endif
    }

    // Test feature detection
    std::cout << "✓ Container support: "
              << (kcenon::network::compat::has_container_support() ? "yes" : "no") << std::endl;
    std::cout << "✓ Thread support: "
              << (kcenon::network::compat::has_thread_support() ? "yes" : "no") << std::endl;

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

// Main test runner
int main() {
    std::cout << "=== Network System Integration Tests ===" << std::endl;
    std::cout << "Testing network_system" << std::endl;

    int tests_passed = 0;
    int tests_failed = 0;

    // Initialize the system
    kcenon::network::compat::initialize();
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

        if (test_compatibility_api()) {
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
    kcenon::network::compat::shutdown();
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