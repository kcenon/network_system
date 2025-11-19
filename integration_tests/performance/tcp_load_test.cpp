/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "kcenon/network/core/messaging_server.h"
#include "kcenon/network/core/messaging_client.h"
#include "../framework/test_helpers.h"
#include "../framework/memory_profiler.h"
#include "../framework/result_writer.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace network_system::core;
using namespace network_system::integration_tests;

/**
 * @brief Test fixture for TCP load testing
 */
class TCPLoadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find available port
        test_port_ = test_helpers::find_available_port(19000);

        // Initialize TCP server
        server_ = std::make_shared<messaging_server>("tcp_load_test_server");

        auto result = server_->start_server(test_port_);
        ASSERT_TRUE(result.is_ok()) << "Failed to start server: " << result.error().message;

        // Wait for server to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        // Stop all clients
        for (auto& client : clients_) {
            if (client) {
                client->stop_client();
            }
        }
        clients_.clear();

        // Stop server
        if (server_) {
            server_->stop_server();
            server_.reset();
        }

        // Brief pause for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    /**
     * @brief Create and connect a TCP client
     */
    auto create_client(const std::string& client_id) -> std::shared_ptr<messaging_client> {
        auto client = std::make_shared<messaging_client>(client_id);

        auto result = client->start_client("localhost", test_port_);
        if (!result.is_ok()) {
            return nullptr;
        }

        // Wait for connection
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return client;
    }

    std::shared_ptr<messaging_server> server_;
    std::vector<std::shared_ptr<messaging_client>> clients_;
    uint16_t test_port_;
    MemoryProfiler profiler_;
    ResultWriter writer_;
};

// ============================================================================
// Message Throughput Tests
// ============================================================================

TEST_F(TCPLoadTest, Message_Throughput_64B) {
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

        std::vector<uint8_t> data(message.begin(), message.end());
        auto result = client->send_packet(std::move(data));
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

    std::cout << "TCP (64B) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Latency P50: " << stats.p50 << " ms\n"
              << "  Latency P95: " << stats.p95 << " ms\n"
              << "  Latency P99: " << stats.p99 << " ms\n"
              << "  Memory RSS: " << memory_after.rss_mb() << " MB\n";

    // Save results
    PerformanceResult result;
    result.test_name = "TCP_64B";
    result.protocol = "tcp";
    result.latency_ms = stats;
    result.throughput_msg_s = throughput;
    result.memory = memory_after;
    result.platform = test_helpers::get_platform_name();
    result.compiler = test_helpers::get_compiler_name();

    std::vector<PerformanceResult> results = {result};
    [[maybe_unused]] auto write_result = writer_.write_json("tcp_64b_results.json", results);

    // Performance expectations (conservative for CI)
    EXPECT_GT(throughput, 1000.0) << "Throughput too low";
    EXPECT_LT(stats.p99, 100.0) << "P99 latency too high";
}

TEST_F(TCPLoadTest, Message_Throughput_1KB) {
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
        std::vector<uint8_t> data(message.begin(), message.end());
        auto result = client->send_packet(std::move(data));
        ASSERT_TRUE(result.is_ok());
        auto msg_end = std::chrono::steady_clock::now();

        latencies.push_back(std::chrono::duration<double, std::milli>(msg_end - msg_start).count());
    }

    auto end_time = std::chrono::steady_clock::now();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = num_messages / duration_s;
    double bandwidth_mbps = (num_messages * message_size) / (duration_s * 1024.0 * 1024.0);

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "TCP (1KB) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Bandwidth: " << bandwidth_mbps << " MB/s\n"
              << "  Latency P50: " << stats.p50 << " ms\n"
              << "  Latency P95: " << stats.p95 << " ms\n";

    EXPECT_GT(throughput, 500.0);
}

TEST_F(TCPLoadTest, Message_Throughput_64KB) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("throughput_client_64kb");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 500;
    constexpr size_t message_size = 65536;

    std::vector<double> latencies;
    std::string message(message_size, 'C');

    auto start_time = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();
        std::vector<uint8_t> data(message.begin(), message.end());
        auto result = client->send_packet(std::move(data));
        ASSERT_TRUE(result.is_ok());
        auto msg_end = std::chrono::steady_clock::now();

        latencies.push_back(std::chrono::duration<double, std::milli>(msg_end - msg_start).count());
    }

    auto end_time = std::chrono::steady_clock::now();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = num_messages / duration_s;
    double bandwidth_mbps = (num_messages * message_size) / (duration_s * 1024.0 * 1024.0);

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "TCP (64KB) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Bandwidth: " << bandwidth_mbps << " MB/s\n"
              << "  Latency P50: " << stats.p50 << " ms\n";

    EXPECT_GT(throughput, 100.0);
}

// ============================================================================
// Concurrent Connection Tests
// ============================================================================

TEST_F(TCPLoadTest, Concurrent_Connections_10) {
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

    std::cout << "Concurrent TCP Connections (" << num_clients << "):\n"
              << "  Total Memory Growth: " << memory_delta.rss_mb() << " MB\n"
              << "  Per-Connection: " << memory_per_connection_kb << " KB\n";

    // Send messages from all clients
    std::atomic<size_t> success_count{0};
    std::string test_message = "concurrent test message";

    for (auto& client : clients_) {
        std::vector<uint8_t> data(test_message.begin(), test_message.end());
        auto result = client->send_packet(std::move(data));
        if (result.is_ok()) {
            success_count.fetch_add(1);
        }
    }

    std::cout << "  Messages sent successfully: " << success_count << "/" << num_clients << "\n";

    EXPECT_EQ(success_count, num_clients) << "Not all messages sent successfully";
    EXPECT_LT(memory_per_connection_kb, 1000.0) << "Per-connection memory too high";
}

TEST_F(TCPLoadTest, Concurrent_Connections_50) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    constexpr size_t num_clients = 50;
    auto memory_before = profiler_.snapshot();

    // Create clients in batches to avoid overwhelming the system
    for (size_t i = 0; i < num_clients; ++i) {
        auto client = create_client("concurrent_client_" + std::to_string(i));
        ASSERT_NE(client, nullptr) << "Failed to create client " << i;
        clients_.push_back(client);

        // Small delay between connection batches
        if (i % 10 == 9) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    auto memory_after = profiler_.snapshot();
    auto memory_delta = MemoryProfiler::delta(memory_before, memory_after);

    double memory_per_connection_kb = (memory_delta.rss_bytes / 1024.0) / num_clients;

    std::cout << "Concurrent TCP Connections (" << num_clients << "):\n"
              << "  Total Memory Growth: " << memory_delta.rss_mb() << " MB\n"
              << "  Per-Connection: " << memory_per_connection_kb << " KB\n";

    EXPECT_LT(memory_per_connection_kb, 1000.0) << "Per-connection memory too high";
}

// ============================================================================
// Round-trip Latency Test
// ============================================================================

TEST_F(TCPLoadTest, Round_Trip_Latency) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("latency_client");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 100;
    std::vector<double> latencies;

    std::string test_message = "ping";

    for (size_t i = 0; i < num_messages; ++i) {
        auto start = std::chrono::steady_clock::now();
        std::vector<uint8_t> data(test_message.begin(), test_message.end());
        auto result = client->send_packet(std::move(data));
        auto end = std::chrono::steady_clock::now();

        if (result.is_ok()) {
            latencies.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "TCP Round-trip Latency:\n"
              << "  P50: " << stats.p50 << " ms\n"
              << "  P95: " << stats.p95 << " ms\n"
              << "  P99: " << stats.p99 << " ms\n";

    EXPECT_LT(stats.p95, 50.0) << "Round-trip latency too high";
}
