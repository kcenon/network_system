/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "network_system/core/messaging_udp_server.h"
#include "network_system/core/messaging_udp_client.h"
#include "../framework/test_helpers.h"
#include "../framework/memory_profiler.h"
#include "../framework/result_writer.h"

#include <chrono>
#include <thread>
#include <vector>
#include <atomic>

using namespace network_system::core;
using namespace network_system::integration_tests;

/**
 * @brief Test fixture for UDP load testing
 */
class UDPLoadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find available port
        test_port_ = test_helpers::find_available_port(18000);

        // Initialize UDP server
        server_ = std::make_shared<messaging_udp_server>("udp_load_test_server");

        // Configure server
        udp_server_config server_config;
        server_config.port = test_port_;

        auto result = server_->start_server(server_config);
        ASSERT_TRUE(result.is_ok()) << "Failed to start server: " << result.error().message;

        // Wait for server to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        // Stop all clients
        for (auto& client : clients_) {
            if (client) {
                client->disconnect();
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
     * @brief Create and start a UDP client
     */
    auto create_client(const std::string& client_id) -> std::shared_ptr<messaging_udp_client> {
        auto client = std::make_shared<messaging_udp_client>(client_id);

        udp_client_config config;
        config.host = "localhost";
        config.port = test_port_;

        auto result = client->start_client(config);
        if (!result.is_ok()) {
            return nullptr;
        }

        // Brief pause for initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return client;
    }

    std::shared_ptr<messaging_udp_server> server_;
    std::vector<std::shared_ptr<messaging_udp_client>> clients_;
    uint16_t test_port_;
    MemoryProfiler profiler_;
    ResultWriter writer_;
};

// ============================================================================
// Message Throughput Tests
// ============================================================================

TEST_F(UDPLoadTest, Message_Throughput_64B) {
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

    size_t sent_count = 0;
    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();

        auto result = client->send_message(std::string(message));
        if (result.is_ok()) {
            sent_count++;
            auto msg_end = std::chrono::steady_clock::now();
            double latency_ms = std::chrono::duration<double, std::milli>(msg_end - msg_start).count();
            latencies.push_back(latency_ms);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto memory_after = profiler_.snapshot();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = sent_count / duration_s;
    double success_rate = (static_cast<double>(sent_count) / num_messages) * 100.0;

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "UDP (64B) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Success Rate: " << success_rate << "%\n"
              << "  Latency P50: " << stats.p50 << " ms\n"
              << "  Latency P95: " << stats.p95 << " ms\n"
              << "  Latency P99: " << stats.p99 << " ms\n"
              << "  Memory RSS: " << memory_after.rss_mb() << " MB\n";

    // Save results
    PerformanceResult result;
    result.test_name = "UDP_64B";
    result.protocol = "udp";
    result.latency_ms = stats;
    result.throughput_msg_s = throughput;
    result.memory = memory_after;
    result.platform = test_helpers::get_platform_name();
    result.compiler = test_helpers::get_compiler_name();

    std::vector<PerformanceResult> results = {result};
    writer_.write_json("udp_64b_results.json", results);

    // Performance expectations (UDP is typically faster than TCP)
    EXPECT_GT(throughput, 2000.0) << "Throughput too low";
    EXPECT_GT(success_rate, 95.0) << "Success rate too low (packet loss)";
    EXPECT_LT(stats.p99, 50.0) << "P99 latency too high";
}

TEST_F(UDPLoadTest, Message_Throughput_512B) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP() << "Skipping throughput test in CI environment";
    }

    auto client = create_client("throughput_client_512b");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 1000;
    constexpr size_t message_size = 512;

    std::vector<double> latencies;
    latencies.reserve(num_messages);

    std::string message(message_size, 'B');

    auto start_time = std::chrono::steady_clock::now();

    size_t sent_count = 0;
    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();
        auto result = client->send_message(std::string(message));
        if (result.is_ok()) {
            sent_count++;
            auto msg_end = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration<double, std::milli>(msg_end - msg_start).count());
        }
    }

    auto end_time = std::chrono::steady_clock::now();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = sent_count / duration_s;
    double bandwidth_mbps = (sent_count * message_size) / (duration_s * 1024.0 * 1024.0);
    double success_rate = (static_cast<double>(sent_count) / num_messages) * 100.0;

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "UDP (512B) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Bandwidth: " << bandwidth_mbps << " MB/s\n"
              << "  Success Rate: " << success_rate << "%\n"
              << "  Latency P50: " << stats.p50 << " ms\n"
              << "  Latency P95: " << stats.p95 << " ms\n";

    EXPECT_GT(throughput, 1000.0);
    EXPECT_GT(success_rate, 95.0);
}

TEST_F(UDPLoadTest, Message_Throughput_1KB) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("throughput_client_1kb");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 1000;
    constexpr size_t message_size = 1024;

    std::vector<double> latencies;
    std::string message(message_size, 'C');

    auto start_time = std::chrono::steady_clock::now();

    size_t sent_count = 0;
    for (size_t i = 0; i < num_messages; ++i) {
        auto msg_start = std::chrono::steady_clock::now();
        auto result = client->send_message(std::string(message));
        if (result.is_ok()) {
            sent_count++;
            auto msg_end = std::chrono::steady_clock::now();
            latencies.push_back(std::chrono::duration<double, std::milli>(msg_end - msg_start).count());
        }
    }

    auto end_time = std::chrono::steady_clock::now();

    double duration_s = std::chrono::duration<double>(end_time - start_time).count();
    double throughput = sent_count / duration_s;
    double bandwidth_mbps = (sent_count * message_size) / (duration_s * 1024.0 * 1024.0);
    double success_rate = (static_cast<double>(sent_count) / num_messages) * 100.0;

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "UDP (1KB) Performance:\n"
              << "  Throughput: " << throughput << " msg/s\n"
              << "  Bandwidth: " << bandwidth_mbps << " MB/s\n"
              << "  Success Rate: " << success_rate << "%\n"
              << "  Latency P50: " << stats.p50 << " ms\n";

    EXPECT_GT(throughput, 800.0);
    EXPECT_GT(success_rate, 90.0);  // Lower expectation for larger packets
}

// ============================================================================
// Concurrent Client Tests
// ============================================================================

TEST_F(UDPLoadTest, Concurrent_Clients_10) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    constexpr size_t num_clients = 10;
    auto memory_before = profiler_.snapshot();

    // Create clients
    for (size_t i = 0; i < num_clients; ++i) {
        auto client = create_client("concurrent_client_" + std::to_string(i));
        ASSERT_NE(client, nullptr) << "Failed to create client " << i;
        clients_.push_back(client);
    }

    auto memory_after = profiler_.snapshot();
    auto memory_delta = MemoryProfiler::delta(memory_before, memory_after);

    double memory_per_client_kb = (memory_delta.rss_bytes / 1024.0) / num_clients;

    std::cout << "Concurrent UDP Clients (" << num_clients << "):\n"
              << "  Total Memory Growth: " << memory_delta.rss_mb() << " MB\n"
              << "  Per-Client: " << memory_per_client_kb << " KB\n";

    // Send messages from all clients
    std::atomic<size_t> success_count{0};
    std::string test_message = "concurrent test message";

    for (auto& client : clients_) {
        auto result = client->send_message(std::string(test_message));
        if (result.is_ok()) {
            success_count.fetch_add(1);
        }
    }

    std::cout << "  Messages sent successfully: " << success_count << "/" << num_clients << "\n";

    double success_rate = (static_cast<double>(success_count) / num_clients) * 100.0;
    EXPECT_GT(success_rate, 90.0) << "Too many send failures";
    EXPECT_LT(memory_per_client_kb, 500.0) << "Per-client memory too high";
}

TEST_F(UDPLoadTest, Concurrent_Clients_50) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    constexpr size_t num_clients = 50;
    auto memory_before = profiler_.snapshot();

    // Create clients
    for (size_t i = 0; i < num_clients; ++i) {
        auto client = create_client("concurrent_client_" + std::to_string(i));
        ASSERT_NE(client, nullptr) << "Failed to create client " << i;
        clients_.push_back(client);
    }

    auto memory_after = profiler_.snapshot();
    auto memory_delta = MemoryProfiler::delta(memory_before, memory_after);

    double memory_per_client_kb = (memory_delta.rss_bytes / 1024.0) / num_clients;

    std::cout << "Concurrent UDP Clients (" << num_clients << "):\n"
              << "  Total Memory Growth: " << memory_delta.rss_mb() << " MB\n"
              << "  Per-Client: " << memory_per_client_kb << " KB\n";

    EXPECT_LT(memory_per_client_kb, 500.0) << "Per-client memory too high";
}

// ============================================================================
// Latency Test
// ============================================================================

TEST_F(UDPLoadTest, Send_Latency) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("latency_client");
    ASSERT_NE(client, nullptr);

    constexpr size_t num_messages = 100;
    std::vector<double> latencies;

    std::string test_message = "latency test";

    for (size_t i = 0; i < num_messages; ++i) {
        auto start = std::chrono::steady_clock::now();
        auto result = client->send_message(std::string(test_message));
        auto end = std::chrono::steady_clock::now();

        if (result.is_ok()) {
            latencies.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto stats = test_helpers::calculate_statistics(latencies);

    std::cout << "UDP Send Latency:\n"
              << "  P50: " << stats.p50 << " ms\n"
              << "  P95: " << stats.p95 << " ms\n"
              << "  P99: " << stats.p99 << " ms\n";

    // UDP should be faster than TCP
    EXPECT_LT(stats.p95, 30.0) << "Send latency too high";
}

// ============================================================================
// Burst Send Test
// ============================================================================

TEST_F(UDPLoadTest, Burst_Send_Performance) {
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP();
    }

    auto client = create_client("burst_client");
    ASSERT_NE(client, nullptr);

    constexpr size_t burst_size = 100;
    constexpr size_t num_bursts = 10;
    constexpr size_t message_size = 256;

    std::string message(message_size, 'D');

    std::vector<double> burst_latencies;

    for (size_t burst = 0; burst < num_bursts; ++burst) {
        auto burst_start = std::chrono::steady_clock::now();

        size_t sent_in_burst = 0;
        for (size_t i = 0; i < burst_size; ++i) {
            auto result = client->send_message(std::string(message));
            if (result.is_ok()) {
                sent_in_burst++;
            }
        }

        auto burst_end = std::chrono::steady_clock::now();
        double burst_duration_ms = std::chrono::duration<double, std::milli>(burst_end - burst_start).count();
        burst_latencies.push_back(burst_duration_ms);

        // Brief pause between bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto stats = test_helpers::calculate_statistics(burst_latencies);

    std::cout << "UDP Burst Send Performance (" << burst_size << " messages/burst):\n"
              << "  Burst Duration P50: " << stats.p50 << " ms\n"
              << "  Burst Duration P95: " << stats.p95 << " ms\n"
              << "  Burst Duration P99: " << stats.p99 << " ms\n";

    EXPECT_LT(stats.p95, 200.0) << "Burst send too slow";
}
