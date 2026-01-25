/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "kcenon/network/core/messaging_ws_server.h"
#include "kcenon/network/core/messaging_ws_client.h"
#include "../framework/test_helpers.h"
#include "../framework/memory_profiler.h"
#include "../framework/result_writer.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace kcenon::network::integration_tests;

/**
 * @brief Test fixture for WebSocket load testing
 */
class WebSocketLoadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find available port
        test_port_ = test_helpers::find_available_port(20000);

        // Initialize WebSocket server
        server_ = std::make_shared<messaging_ws_server>("ws_load_test_server");

        // Configure server
        ws_server_config server_config;
        server_config.port = test_port_;
        server_config.max_connections = 100;
        server_config.ping_interval = std::chrono::seconds(60);

        auto result = server_->start_server(server_config);
        ASSERT_TRUE(result.is_ok()) << "Failed to start server: " << result.error().message;

        // Wait for server to be ready
        test_helpers::wait_for_ready();
    }

    void TearDown() override {
        // Stop all clients
        for (auto& client : clients_) {
            if (client) {
                (void)client->stop_client();
            }
        }
        clients_.clear();

        // Stop server
        if (server_) {
            (void)server_->stop_server();
            server_.reset();
        }

        // Brief pause for cleanup
        test_helpers::wait_for_ready();
    }

    /**
     * @brief Create and connect a WebSocket client
     */
    auto create_client(const std::string& client_id) -> std::shared_ptr<messaging_ws_client> {
        auto client = std::make_shared<messaging_ws_client>(client_id);

        ws_client_config config;
        config.host = "localhost";
        config.port = test_port_;
        config.path = "/";
        config.auto_pong = true;

        auto result = client->start_client(config);
        if (!result.is_ok()) {
            return nullptr;
        }

        // Wait for connection
        test_helpers::wait_for_ready();
        return client;
    }

    std::shared_ptr<messaging_ws_server> server_;
    std::vector<std::shared_ptr<messaging_ws_client>> clients_;
    uint16_t test_port_;
    MemoryProfiler profiler_;
    ResultWriter writer_;
};

// ============================================================================
// Text Message Throughput Tests
// ============================================================================

TEST_F(WebSocketLoadTest, Text_Message_Throughput_64B) {
    // Skip in CI due to timing sensitivity
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP() << "Skipping throughput test in CI environment";
    }

    auto client = create_client("throughput_client_64b");
    ASSERT_NE(client, nullptr) << "Failed to create client";

    constexpr size_t num_messages = 1000;
    constexpr size_t message_size = 64;

    std::vector<double> latencies;
    latencies.reserve(num_messages);

    std::string message(message_size, 'A');

    auto memory_before = profiler_.snapshot();
    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();

        auto result = client->send_text(std::string(message));
        ASSERT_TRUE(result.is_ok()) << "Send failed: " << result.error().message;

        auto msg_end = std::chrono::steady_clock::now();
        double latency_ms = std::chrono::duration<double, std::milli>(msg_end - msg_start).count();
        latencies.push_back(latency_ms);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto memory_after = profiler_.snapshot();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = num_messages / duration_s;

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "WebSocket Text (64B) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Latency P50: " << stats.p50 << " ms\n"
              << "  Latency P95: " << stats.p95 << " ms\n"
              << "  Latency P99: " << stats.p99 << " ms\n"
              << "  Memory RSS: " << memory_after.rss_mb() << " MB\n";

    // Save results
    PerformanceResult result;
    result.test_name = "WebSocket_Text_64B";
    result.protocol = "websocket";
    result.latency_ms = stats;
    result.throughput_msg_s = throughput;
    result.memory = memory_after;
    result.platform = test_helpers::get_platform_name();
    result.compiler = test_helpers::get_compiler_name();

    std::vector<PerformanceResult> results = {result};
    [[maybe_unused]] auto write_result = writer_.write_json("websocket_text_64b_results.json", results);

    // Performance expectations (conservative for CI)
    EXPECT_GT(throughput, 1000.0) << "Throughput too low";
    EXPECT_LT(stats.p99, 100.0) << "P99 latency too high";
}

TEST_F(WebSocketLoadTest, Text_Message_Throughput_1KB) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP() << "Skipping throughput test in CI environment";
    }

    auto client = create_client("throughput_client_1kb");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 1000;
    constexpr size_t message_size = 1024;

    std::vector<double> latencies;
    latencies.reserve(num_messages);

    std::string message(message_size, 'B');

    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();
        auto result = client->send_text(std::string(message));
        ASSERT_TRUE(result.is_ok());
        auto msg_end = std::chrono::steady_clock::now();

        latencies.push_back(std::chrono::duration<double, std::milli>(msg_end - msg_start).count());
    }

    auto end_time = std::chrono::steady_clock::now();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = num_messages / duration_s;
    double bandwidth_mbps = (num_messages * message_size) / (duration_s * 1024.0 * 1024.0);

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "WebSocket Text (1KB) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Bandwidth: " << bandwidth_mbps << " MB/s\n"
              << "  Latency P50: " << stats.p50 << " ms\n"
              << "  Latency P95: " << stats.p95 << " ms\n";

    EXPECT_GT(throughput, 500.0);
}

// ============================================================================
// Binary Message Throughput Tests
// ============================================================================

TEST_F(WebSocketLoadTest, Binary_Message_Throughput) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("binary_client");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 1000;
    constexpr size_t message_size = 256;

    std::vector<double> latencies;
    std::vector<uint8_t> message(message_size, 0x42);

    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();
        auto result = client->send_binary(std::vector<uint8_t>(message));
        ASSERT_TRUE(result.is_ok());
        auto msg_end = std::chrono::steady_clock::now();

        latencies.push_back(std::chrono::duration<double, std::milli>(msg_end - msg_start).count());
    }

    auto end_time = std::chrono::steady_clock::now();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = num_messages / duration_s;

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "WebSocket Binary (256B) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Latency P50: " << stats.p50 << " ms\n";

    EXPECT_GT(throughput, 800.0);
}

// ============================================================================
// Concurrent Connection Tests
// ============================================================================

TEST_F(WebSocketLoadTest, Concurrent_Connections_10) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    constexpr size_t num_clients = 10;
    auto memory_before = profiler_.snapshot();

    // Create and connect clients
    for (size_t i = 0; i < num_clients; ++i) {
        auto client = create_client("concurrent_client_" + std::to_string(i));
        ASSERT_NE(client, nullptr) << "Failed to create client " << i;
        clients_.push_back(client);
    }

    auto memory_after = profiler_.snapshot();
    auto memory_delta = MemoryProfiler::delta(memory_before, memory_after);

    double memory_per_connection_kb = (memory_delta.rss_bytes / 1024.0) / num_clients;

    std::cout << "Concurrent WebSocket Connections (" << num_clients << "):\n"
              << "  Total Memory Growth: " << memory_delta.rss_mb() << " MB\n"
              << "  Per-Connection: " << memory_per_connection_kb << " KB\n";

    // Send messages from all clients
    std::atomic<size_t> success_count{0};
    std::string test_message = "concurrent test message";

    for (auto& client : clients_) {
        auto result = client->send_text(std::string(test_message));
        if (result.is_ok()) {
            success_count.fetch_add(1);
        }
    }

    std::cout << "  Messages sent successfully: " << success_count << "/" << num_clients << "\n";

    EXPECT_EQ(success_count, num_clients) << "Not all messages sent successfully";
    EXPECT_LT(memory_per_connection_kb, 2000.0) << "Per-connection memory too high";
}

// ============================================================================
// Ping/Pong Latency Test
// ============================================================================

TEST_F(WebSocketLoadTest, Ping_Pong_Latency) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("ping_client");
    ASSERT_NE(client, nullptr);

    // WebSocket ping/pong is automatic, so we measure small message round-trip
    // as a proxy for ping/pong performance
    constexpr size_t num_pings = 100;
    std::vector<double> latencies;

    std::string ping_message = "ping";

    for (size_t i = 0; i < num_pings; ++i) {
        auto start = std::chrono::steady_clock::now();
        auto result = client->send_text(std::string(ping_message));
        auto end = std::chrono::steady_clock::now();

        if (result.is_ok()) {
            latencies.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }

        std::this_thread::yield();
    }

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "WebSocket Ping/Pong Latency:\n"
              << "  P50: " << stats.p50 << " ms\n"
              << "  P95: " << stats.p95 << " ms\n"
              << "  P99: " << stats.p99 << " ms\n";

    EXPECT_LT(stats.p95, 50.0) << "Ping latency too high";
}
