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

#include <gtest/gtest.h>
#include "../framework/system_fixture.h"
#include "../framework/test_helpers.h"
#include <iostream>

namespace network_system::integration_tests {

/**
 * @brief Test suite for network performance measurement
 *
 * Tests performance metrics including:
 * - Connection throughput
 * - Message latency (P50, P95, P99)
 * - Bandwidth utilization
 * - Concurrent connection scalability
 */
class NetworkPerformanceTest : public PerformanceFixture {};
class ConcurrentPerformanceTest : public MultiConnectionFixture {};

// ============================================================================
// Connection Performance Tests
// ============================================================================

TEST_F(ConcurrentPerformanceTest, ConnectionThroughput) {
    // Skip concurrent performance tests in CI due to resource contention
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP() << "Skipping concurrent performance test in CI environment";
    }

    ASSERT_TRUE(StartServer());

    // Measure time to establish multiple connections
    constexpr size_t num_connections = 100;
    CreateClients(num_connections);

    auto duration = this->MeasureDuration([&]() {
        ConnectAllClients();
    });

    // Calculate throughput (connections per second)
    double throughput = (num_connections / duration) * 1000.0;

    std::cout << "Connection throughput: " << throughput << " conn/s\n";

    // Target: > 1000 connections/second (should be much higher for local connections)
    EXPECT_GT(throughput, 100.0);  // Conservative target for testing
}

TEST_F(NetworkPerformanceTest, SingleConnectionLatency) {
    // Skip this test in CI environments due to resource contention
    // Performance tests are not reliable in shared CI runners
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP() << "Skipping latency test in CI environment";
    }

    ASSERT_TRUE(StartServer());

    // Measure latency for single connection
    std::vector<double> latencies;

    for (int i = 0; i < 100; ++i) {
        client_ = std::make_shared<core::messaging_client>("client_" + std::to_string(i));

        auto latency = this->MeasureDuration([&]() {
            ConnectClient();
        });

        latencies.push_back(latency);

        client_->stop_client();
        WaitFor(10);
    }

    auto stats = this->CalculateStats(latencies);

    std::cout << "Connection latency (ms):\n"
              << "  P50: " << stats.p50 << "\n"
              << "  P95: " << stats.p95 << "\n"
              << "  P99: " << stats.p99 << "\n";

    // Target: < 10ms for P50
    EXPECT_LT(stats.p50, 100.0);  // Conservative for CI environments
}

// ============================================================================
// Message Latency Tests
// ============================================================================

TEST_F(NetworkPerformanceTest, SmallMessageLatency) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    std::vector<double> latencies;
    constexpr size_t message_size = 64;

    // Measure latency for 100 small messages
    for (int i = 0; i < 100; ++i) {
        auto message = CreateTestMessage(message_size);

        auto latency = this->MeasureDuration([&]() {
            SendMessage(message);
        });

        latencies.push_back(latency);
        WaitFor(5);
    }

    auto stats = this->CalculateStats(latencies);

    std::cout << "Small message latency (ms):\n"
              << "  P50: " << stats.p50 << "\n"
              << "  P95: " << stats.p95 << "\n"
              << "  P99: " << stats.p99 << "\n";

    // Target: P50 < 1ms, P95 < 10ms
    EXPECT_LT(stats.p50, 10.0);
    EXPECT_LT(stats.p95, 50.0);
}

TEST_F(NetworkPerformanceTest, LargeMessageLatency) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    std::vector<double> latencies;
    constexpr size_t message_size = 64 * 1024;  // 64KB

    // Measure latency for large messages
    for (int i = 0; i < 50; ++i) {
        auto message = CreateTestMessage(message_size);

        auto latency = this->MeasureDuration([&]() {
            SendMessage(message);
        });

        latencies.push_back(latency);
        WaitFor(10);
    }

    auto stats = this->CalculateStats(latencies);

    std::cout << "Large message latency (ms):\n"
              << "  P50: " << stats.p50 << "\n"
              << "  P95: " << stats.p95 << "\n"
              << "  P99: " << stats.p99 << "\n";

    EXPECT_LT(stats.p99, 500.0);
}

// ============================================================================
// Throughput Tests
// ============================================================================

TEST_F(NetworkPerformanceTest, MessageThroughput) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    constexpr size_t num_messages = 1000;
    constexpr size_t message_size = 256;

    auto duration = this->MeasureDuration([&]() {
        for (size_t i = 0; i < num_messages; ++i) {
            auto message = CreateTestMessage(message_size);
            SendMessage(message);
        }
    });

    // Calculate throughput (messages per second)
    double throughput = (num_messages / duration) * 1000.0;

    std::cout << "Message throughput: " << throughput << " msg/s\n";

    // Target: > 10,000 messages/second
    EXPECT_GT(throughput, 1000.0);  // Conservative target
}

TEST_F(NetworkPerformanceTest, BandwidthUtilization) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    constexpr size_t num_messages = 100;
    constexpr size_t message_size = 10 * 1024;  // 10KB
    constexpr size_t total_bytes = num_messages * message_size;

    auto duration = this->MeasureDuration([&]() {
        for (size_t i = 0; i < num_messages; ++i) {
            auto message = CreateTestMessage(message_size);
            SendMessage(message);
        }
    });

    // Calculate bandwidth (MB/s)
    double bandwidth_mbps = (total_bytes / (1024.0 * 1024.0)) / (duration / 1000.0);

    std::cout << "Bandwidth: " << bandwidth_mbps << " MB/s\n";

    // Should achieve reasonable bandwidth
    EXPECT_GT(bandwidth_mbps, 1.0);
}

// ============================================================================
// Scalability Tests
// ============================================================================

TEST_F(ConcurrentPerformanceTest, ConcurrentConnectionScalability) {
    // Skip concurrent performance tests in CI due to resource contention
    if (test_helpers::is_ci_environment()) {
        GTEST_SKIP() << "Skipping concurrent scalability test in CI environment";
    }

    ASSERT_TRUE(StartServer());

    // Test with increasing number of connections
    std::vector<size_t> connection_counts = {10, 50, 100};

    for (auto count : connection_counts) {
        // Clean up previous clients
        for (auto& client : clients_) {
            if (client) {
                client->stop_client();
            }
        }
        clients_.clear();

        CreateClients(count);

        auto duration = this->MeasureDuration([&]() {
            ConnectAllClients();
        });

        double throughput = (count / duration) * 1000.0;

        std::cout << "Connections: " << count
                  << ", Throughput: " << throughput << " conn/s\n";

        // Should handle connections efficiently
        EXPECT_GT(throughput, 50.0);

        WaitFor(100);
    }
}

TEST_F(ConcurrentPerformanceTest, ConcurrentMessageSending) {
    ASSERT_TRUE(StartServer());

    constexpr size_t num_clients = 20;
    CreateClients(num_clients);
    size_t connected = ConnectAllClients();

    ASSERT_GE(connected, num_clients / 2);

    // Each client sends messages concurrently
    constexpr size_t messages_per_client = 50;

    auto duration = this->MeasureDuration([&]() {
        for (auto& client : clients_) {
            for (size_t i = 0; i < messages_per_client; ++i) {
                auto message = CreateTestMessage(128);
                client->send_packet(message);
            }
        }
    });

    double total_messages = connected * messages_per_client;
    double throughput = (total_messages / duration) * 1000.0;

    std::cout << "Concurrent message throughput: " << throughput << " msg/s\n";

    EXPECT_GT(throughput, 500.0);
}

// ============================================================================
// Load Tests
// ============================================================================

TEST_F(NetworkPerformanceTest, SustainedLoad) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send messages continuously for a period
    constexpr auto test_duration = std::chrono::seconds(2);
    auto start = std::chrono::steady_clock::now();
    size_t message_count = 0;

    while (std::chrono::steady_clock::now() - start < test_duration) {
        auto message = CreateTestMessage(512);
        if (SendMessage(message)) {
            ++message_count;
        }
    }

    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    double throughput = (message_count / static_cast<double>(actual_duration)) * 1000.0;

    std::cout << "Sustained load throughput: " << throughput << " msg/s\n";
    std::cout << "Total messages sent: " << message_count << "\n";

    EXPECT_GT(message_count, 100);
}

TEST_F(NetworkPerformanceTest, BurstLoad) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send burst of messages
    constexpr size_t burst_size = 500;

    auto duration = this->MeasureDuration([&]() {
        for (size_t i = 0; i < burst_size; ++i) {
            auto message = CreateTestMessage(256);
            SendMessage(message);
        }
    });

    double throughput = (burst_size / duration) * 1000.0;

    std::cout << "Burst load throughput: " << throughput << " msg/s\n";

    EXPECT_GT(throughput, 1000.0);
}

} // namespace network_system::integration_tests
