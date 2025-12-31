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
 * @file modern_usage.cpp
 * @brief Example of using network_system with modern API and Result<T> pattern
 *
 * This example demonstrates:
 * - Modern network_system API with Result<T> error handling
 * - Type-safe error checking with VoidResult return types
 * - Integration with thread pool and container systems
 * - Proper error handling and recovery patterns
 *
 * Note: All primary APIs (start_server, start_client, send_packet, etc.)
 * now return Result<T> for type-safe error handling instead of throwing
 * exceptions or using callback-only error reporting.
 *
 * @author kcenon
 * @date 2025-09-20
 * @updated 2025-10-10 - Added Result<T> pattern support
 */

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>

// Include modern network_system API
#include "kcenon/network/network_system.h"

using namespace kcenon::network;
using namespace std::chrono_literals;

/**
 * @brief Modern server using new API
 */
class ModernServer {
public:
    ModernServer(const std::string& id) : server_id_(id) {
        server_ = std::make_shared<core::messaging_server>(server_id_);
        bridge_ = std::make_shared<integration::messaging_bridge>();

        // Set up thread pool interface
        bridge_->set_thread_pool_interface(
            integration::thread_integration_manager::instance().get_thread_pool()
        );

        std::cout << "[Modern Server] Created with ID: " << server_id_ << std::endl;
    }

    bool start(uint16_t port) {
        auto result = server_->start_server(port);
        if (result.is_err()) {
            std::cerr << "[Modern Server] Failed to start: " << result.error().message
                      << " (code: " << result.error().code << ")" << std::endl;
            return false;
        }

        port_ = port;
        std::cout << "[Modern Server] Started on port " << port << std::endl;
        return true;
    }

    bool stop() {
        auto result = server_->stop_server();
        if (result.is_err()) {
            std::cerr << "[Modern Server] Failed to stop: " << result.error().message
                      << " (code: " << result.error().code << ")" << std::endl;
            return false;
        }

        std::cout << "[Modern Server] Stopped" << std::endl;
        return true;
    }

    void enable_async_processing() {
        async_enabled_ = true;
        std::cout << "[Modern Server] Async processing enabled" << std::endl;
    }

    void show_statistics() {
        auto metrics = bridge_->get_metrics();
        auto thread_metrics = integration::thread_integration_manager::instance().get_metrics();

        std::cout << "\n=== Server Statistics ===" << std::endl;
        std::cout << "Network Metrics:" << std::endl;
        std::cout << "  Messages sent: " << metrics.messages_sent << std::endl;
        std::cout << "  Messages received: " << metrics.messages_received << std::endl;
        std::cout << "  Bytes sent: " << metrics.bytes_sent << std::endl;
        std::cout << "  Bytes received: " << metrics.bytes_received << std::endl;
        std::cout << "  Active connections: " << metrics.connections_active << std::endl;

        std::cout << "\nThread Pool Metrics:" << std::endl;
        std::cout << "  Worker threads: " << thread_metrics.worker_threads << std::endl;
        std::cout << "  Pending tasks: " << thread_metrics.pending_tasks << std::endl;
        std::cout << "  Completed tasks: " << thread_metrics.completed_tasks << std::endl;
    }

private:
    void process_message(const std::string& client_id, const std::string& message) {
        std::cout << "[Modern Server] Processing from " << client_id
                  << ": " << message << std::endl;

        if (async_enabled_) {
            // Process asynchronously using thread pool
            auto& thread_mgr = integration::thread_integration_manager::instance();
            auto future = thread_mgr.submit_task([message]() {
                // Simulate complex processing
                std::this_thread::sleep_for(50ms);
                std::cout << "[Async Processor] Completed processing: " << message << std::endl;
            });

            // Don't wait - let it process in background
            futures_.push_back(std::move(future));
        }

        // Use container system for demonstration
        auto& container_mgr = integration::container_manager::instance();
        std::string response = "Processed: " + message;

        // Serialize and deserialize to demonstrate container usage
        auto serialized = container_mgr.serialize(std::any(response));
        auto deserialized = container_mgr.deserialize(serialized);

        if (deserialized.has_value()) {
            std::cout << "[Modern Server] Container processed: "
                      << std::any_cast<std::string>(deserialized) << std::endl;
        }
    }

    std::string server_id_;
    uint16_t port_ = 0;
    bool async_enabled_ = false;
    std::shared_ptr<core::messaging_server> server_;
    std::shared_ptr<integration::messaging_bridge> bridge_;
    std::vector<std::future<void>> futures_;
};

/**
 * @brief Modern client using new API
 */
class ModernClient {
public:
    ModernClient(const std::string& id) : client_id_(id) {
        client_ = std::make_shared<core::messaging_client>(client_id_);
        std::cout << "[Modern Client] Created with ID: " << client_id_ << std::endl;
    }

    bool connect(const std::string& host, uint16_t port) {
        auto result = client_->start_client(host, port);
        if (result.is_err()) {
            std::cerr << "[Modern Client] Failed to connect: " << result.error().message
                      << " (code: " << result.error().code << ")" << std::endl;
            return false;
        }

        std::cout << "[Modern Client] Connecting to " << host << ":" << port << std::endl;
        std::this_thread::sleep_for(200ms);  // Give time to connect
        return true;
    }

    size_t send_batch(const std::vector<std::string>& messages) {
        std::cout << "[Modern Client] Sending batch of " << messages.size()
                  << " messages" << std::endl;

        size_t successful = 0;
        for (const auto& msg : messages) {
            std::vector<uint8_t> data(msg.begin(), msg.end());
            auto result = client_->send_packet(data);

            if (result.is_err()) {
                std::cerr << "[Modern Client] Failed to send message: " << result.error().message
                          << " (code: " << result.error().code << ")" << std::endl;
            } else {
                successful++;
            }

            std::this_thread::sleep_for(50ms);
        }

        std::cout << "[Modern Client] Successfully sent " << successful << "/" << messages.size()
                  << " messages" << std::endl;
        return successful;
    }

    void send_async(const std::string& message) {
        auto& thread_mgr = integration::thread_integration_manager::instance();
        thread_mgr.submit_task([this, message]() {
            std::vector<uint8_t> data(message.begin(), message.end());
            auto result = client_->send_packet(data);

            if (result.is_err()) {
                std::cerr << "[Modern Client] Async send failed: " << result.error().message
                          << " (code: " << result.error().code << ")" << std::endl;
            } else {
                std::cout << "[Modern Client] Async sent: " << message << std::endl;
            }
        });
    }

    bool disconnect() {
        auto result = client_->stop_client();
        if (result.is_err()) {
            std::cerr << "[Modern Client] Failed to disconnect: " << result.error().message
                      << " (code: " << result.error().code << ")" << std::endl;
            return false;
        }

        std::cout << "[Modern Client] Disconnected" << std::endl;
        return true;
    }

private:
    std::string client_id_;
    std::shared_ptr<core::messaging_client> client_;
};

/**
 * @brief Demonstrate advanced features
 */
void demonstrate_advanced_features() {
    std::cout << "\n=== Advanced Features Demo ===" << std::endl;

    // Custom container with serialization
    auto custom_container = std::make_shared<integration::basic_container>();

    // Set custom serializer for complex types
    custom_container->set_serializer([](const std::any& data) {
        std::vector<uint8_t> result;
        if (data.type() == typeid(std::vector<int>)) {
            auto vec = std::any_cast<std::vector<int>>(data);
            result.reserve(vec.size() * sizeof(int));
            for (int val : vec) {
                auto bytes = reinterpret_cast<const uint8_t*>(&val);
                result.insert(result.end(), bytes, bytes + sizeof(int));
            }
        }
        return result;
    });

    // Register custom container
    integration::container_manager::instance().register_container(
        "custom_vector_serializer", custom_container
    );

    // Test custom serialization
    std::vector<int> test_data = {1, 2, 3, 4, 5};
    auto serialized = custom_container->serialize(std::any(test_data));
    std::cout << "Custom serialized " << test_data.size()
              << " integers to " << serialized.size() << " bytes" << std::endl;

    // Thread pool advanced usage
    auto& thread_mgr = integration::thread_integration_manager::instance();
    std::vector<std::future<void>> tasks;

    // Submit multiple delayed tasks
    for (int i = 1; i <= 3; ++i) {
        auto future = thread_mgr.submit_delayed_task(
            [i]() {
                std::cout << "[Delayed Task " << i << "] Executed after delay" << std::endl;
            },
            std::chrono::milliseconds(i * 100)
        );
        tasks.push_back(std::move(future));
    }

    // Wait for all tasks
    for (auto& future : tasks) {
        future.wait();
    }

    std::cout << "All advanced features demonstrated successfully" << std::endl;
}

/**
 * @brief Main function demonstrating modern usage
 */
int main(int argc, char* argv[]) {
    std::cout << "=== Modern Network System Usage Demo ===" << std::endl;
    std::cout << "Demonstrating the new API with all integration features" << std::endl;

    // Initialize using modern API
    kcenon::network::compat::initialize();
    std::cout << "\n✓ Network system initialized" << std::endl;

    try {
        // Create modern server
        ModernServer server("modern_server_001");
        server.enable_async_processing();

        if (!server.start(9090)) {
            std::cerr << "Failed to start server, aborting demo" << std::endl;
            return 1;
        }

        // Allow server to start
        std::this_thread::sleep_for(500ms);

        // Create modern client
        ModernClient client("modern_client_001");

        if (!client.connect("127.0.0.1", 9090)) {
            std::cerr << "Failed to connect client, stopping server" << std::endl;
            server.stop();
            return 1;
        }

        // Send batch messages
        std::vector<std::string> batch = {
            "Modern message 1",
            "Modern message 2",
            "Modern message 3"
        };
        size_t sent = client.send_batch(batch);

        if (sent < batch.size()) {
            std::cout << "[Warning] Not all messages were sent successfully" << std::endl;
        }

        // Send async messages
        for (int i = 1; i <= 3; ++i) {
            client.send_async("Async message " + std::to_string(i));
        }

        // Wait for async operations
        std::this_thread::sleep_for(500ms);

        // Show server statistics
        server.show_statistics();

        // Disconnect
        if (!client.disconnect()) {
            std::cout << "[Warning] Client disconnection had issues" << std::endl;
        }

        // Demonstrate advanced features
        demonstrate_advanced_features();

        // Stop server
        if (!server.stop()) {
            std::cout << "[Warning] Server shutdown had issues" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // Shutdown
    kcenon::network::compat::shutdown();
    std::cout << "\n✓ Network system shutdown complete" << std::endl;

    std::cout << "\n=== Modern Usage Demo Complete ===" << std::endl;
    std::cout << "All modern features working perfectly!" << std::endl;

    return 0;
}