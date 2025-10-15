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

#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"
#include "network_system/utils/result_types.h"
#include "test_helpers.h"

namespace network_system::integration_tests {

/**
 * @brief Base fixture for network system integration tests
 *
 * Provides common setup/teardown and helper methods for testing
 * server-client interactions, connection lifecycle, and message exchange.
 */
class NetworkSystemFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Find available port for testing
        test_port_ = test_helpers::find_available_port();

        // Initialize server
        server_ = std::make_shared<core::messaging_server>("test_server");

        // Initialize client
        client_ = std::make_shared<core::messaging_client>("test_client");
    }

    void TearDown() override {
        // Stop client first
        if (client_) {
            client_->stop_client();
            client_.reset();
        }

        // Then stop server
        if (server_) {
            server_->stop_server();
            server_.reset();
        }

        // Brief pause to ensure cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /**
     * @brief Start the test server on the configured port
     * @return true if server started successfully
     */
    bool StartServer() {
        auto result = server_->start_server(test_port_);
        if (!result.is_ok()) {
            return false;
        }

        // Wait for server to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    }

    /**
     * @brief Stop the test server
     * @return true if server stopped successfully
     */
    bool StopServer() {
        auto result = server_->stop_server();
        return result.is_ok();
    }

    /**
     * @brief Connect client to the test server
     * @return true if client connected successfully
     */
    bool ConnectClient() {
        auto result = client_->start_client("localhost", test_port_);
        if (!result.is_ok()) {
            return false;
        }

        // Wait for connection to establish
        return test_helpers::wait_for_connection(client_, std::chrono::seconds(5));
    }

    /**
     * @brief Send a message from client to server
     * @param data Message data to send
     * @return true if message sent successfully
     */
    bool SendMessage(const std::vector<uint8_t>& data) {
        auto result = client_->send_packet(data);
        return result.is_ok();
    }

    /**
     * @brief Create test message data
     * @param size Size of the message
     * @param pattern Fill pattern (default: sequential bytes)
     * @return Vector of test data
     */
    std::vector<uint8_t> CreateTestMessage(size_t size, uint8_t pattern = 0) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = pattern == 0 ? static_cast<uint8_t>(i % 256) : pattern;
        }
        return data;
    }

    /**
     * @brief Wait for a specific duration
     * @param ms Milliseconds to wait
     */
    void WaitFor(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

protected:
    std::shared_ptr<core::messaging_server> server_;
    std::shared_ptr<core::messaging_client> client_;
    unsigned short test_port_{0};
};

/**
 * @brief Fixture for performance testing
 */
class PerformanceFixture : public NetworkSystemFixture {
protected:
    void SetUp() override {
        if (test_helpers::is_sanitizer_run()) {
            GTEST_SKIP() << "Skipping performance-sensitive test under sanitizer instrumentation";
        }
        NetworkSystemFixture::SetUp();
    }

    /**
     * @brief Measure operation duration
     * @param operation Function to measure
     * @return Duration in milliseconds
     */
    template<typename F>
    double MeasureDuration(F&& operation) {
        auto start = std::chrono::high_resolution_clock::now();
        operation();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    /**
     * @brief Calculate statistics from measurements
     * @param measurements Vector of measurement values
     * @return Statistics (min, max, mean, p50, p95, p99)
     */
    test_helpers::Statistics CalculateStats(const std::vector<double>& measurements) {
        return test_helpers::calculate_statistics(measurements);
    }
};

/**
 * @brief Fixture for multiple concurrent connections
 * Inherits from PerformanceFixture to allow performance measurements
 */
class MultiConnectionFixture : public PerformanceFixture {
protected:
    void SetUp() override {
        PerformanceFixture::SetUp();
    }

    void TearDown() override {
        // Stop all clients
        for (auto& client : clients_) {
            if (client) {
                client->stop_client();
            }
        }
        clients_.clear();

        NetworkSystemFixture::TearDown();
    }

    /**
     * @brief Create multiple clients
     * @param count Number of clients to create
     */
    void CreateClients(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            auto client = std::make_shared<core::messaging_client>(
                "test_client_" + std::to_string(i)
            );
            clients_.push_back(client);
        }
    }

    /**
     * @brief Connect all created clients to the server
     * @return Number of successfully connected clients
     */
    size_t ConnectAllClients() {
        size_t connected = 0;
        for (auto& client : clients_) {
            auto result = client->start_client("localhost", test_port_);
            if (result.is_ok()) {
                if (test_helpers::wait_for_connection(client, std::chrono::seconds(5))) {
                    ++connected;
                }
            }
        }
        return connected;
    }

protected:
    std::vector<std::shared_ptr<core::messaging_client>> clients_;
};

} // namespace network_system::integration_tests
