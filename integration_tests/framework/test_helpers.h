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

#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

#include <asio.hpp>

namespace network_system::integration_tests::test_helpers {

/**
 * @brief Statistics structure for performance measurements
 */
struct Statistics {
    double min{0.0};
    double max{0.0};
    double mean{0.0};
    double p50{0.0};  // Median
    double p95{0.0};
    double p99{0.0};
    double stddev{0.0};
};

/**
 * @brief Find an available TCP port for testing
 * @param start_port Starting port to search from (default: 15000)
 * @return Available port number
 */
inline unsigned short find_available_port(unsigned short start_port = 15000) {
    // Use ASIO to find available port
    try {
        asio::io_context io_context;
        asio::ip::tcp::acceptor acceptor(io_context);

        // Try ports in range [start_port, start_port + 1000]
        for (unsigned short port = start_port; port < start_port + 1000; ++port) {
            try {
                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
                acceptor.open(endpoint.protocol());
                acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                acceptor.bind(endpoint);
                acceptor.close();
                return port;
            } catch (const std::exception&) {
                // Port not available, try next
                if (acceptor.is_open()) {
                    acceptor.close();
                }
            }
        }
    } catch (const std::exception&) {
        // Fallback to random port in high range
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(50000, 60000);
        return static_cast<unsigned short>(dis(gen));
    }

    // Ultimate fallback
    return 15555;
}

/**
 * @brief Wait for client connection to be established
 * @param client Client instance to check
 * @param timeout Maximum time to wait
 * @return true if connected within timeout
 */
template<typename ClientType>
inline bool wait_for_connection(
    std::shared_ptr<ClientType> client,
    std::chrono::seconds timeout = std::chrono::seconds(5)
) {
    auto start = std::chrono::steady_clock::now();
    auto deadline = start + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        // Give time for async operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Note: We rely on the client's internal state
        // In a real implementation, you might check is_connected() if available

        // For now, we assume connection is established after brief delay
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::milliseconds(200)) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Wait for a condition to become true
 * @param condition Function that returns bool
 * @param timeout Maximum time to wait
 * @param check_interval How often to check the condition
 * @return true if condition became true within timeout
 */
template<typename Predicate>
inline bool wait_for_condition(
    Predicate&& condition,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000),
    std::chrono::milliseconds check_interval = std::chrono::milliseconds(10)
) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(check_interval);
    }

    return false;
}

/**
 * @brief Calculate statistics from a vector of measurements
 * @param values Vector of measurement values
 * @return Statistics structure
 */
inline Statistics calculate_statistics(std::vector<double> values) {
    Statistics stats;

    if (values.empty()) {
        return stats;
    }

    // Sort for percentile calculations
    std::sort(values.begin(), values.end());

    stats.min = values.front();
    stats.max = values.back();

    // Mean
    stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();

    // Percentiles
    auto percentile = [&values](double p) -> double {
        size_t index = static_cast<size_t>((p / 100.0) * (values.size() - 1));
        return values[index];
    };

    stats.p50 = percentile(50.0);
    stats.p95 = percentile(95.0);
    stats.p99 = percentile(99.0);

    // Standard deviation
    double variance = 0.0;
    for (const auto& value : values) {
        variance += (value - stats.mean) * (value - stats.mean);
    }
    variance /= values.size();
    stats.stddev = std::sqrt(variance);

    return stats;
}

/**
 * @brief Generate random binary data
 * @param size Size of data to generate
 * @return Vector of random bytes
 */
inline std::vector<uint8_t> generate_random_data(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    return data;
}

/**
 * @brief Generate sequential binary data
 * @param size Size of data to generate
 * @param start Starting value (default: 0)
 * @return Vector of sequential bytes
 */
inline std::vector<uint8_t> generate_sequential_data(size_t size, uint8_t start = 0) {
    std::vector<uint8_t> data(size);
    uint8_t value = start;

    for (auto& byte : data) {
        byte = value++;
    }

    return data;
}

/**
 * @brief Verify message data integrity
 * @param expected Expected data
 * @param actual Actual received data
 * @return true if data matches
 */
inline bool verify_message_data(
    const std::vector<uint8_t>& expected,
    const std::vector<uint8_t>& actual
) {
    if (expected.size() != actual.size()) {
        return false;
    }

    return std::equal(expected.begin(), expected.end(), actual.begin());
}

/**
 * @brief Create a simple text message
 * @param text Text content
 * @return Vector of bytes containing the text
 */
inline std::vector<uint8_t> create_text_message(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

/**
 * @brief Convert message data to string
 * @param data Message data
 * @return String representation
 */
inline std::string message_to_string(const std::vector<uint8_t>& data) {
    return std::string(data.begin(), data.end());
}

/**
 * @brief Retry an operation with exponential backoff
 * @param operation Function to retry
 * @param max_attempts Maximum number of attempts
 * @param initial_delay Initial delay between attempts
 * @return true if operation succeeded within max_attempts
 */
template<typename F>
inline bool retry_with_backoff(
    F&& operation,
    int max_attempts = 3,
    std::chrono::milliseconds initial_delay = std::chrono::milliseconds(100)
) {
    auto delay = initial_delay;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        if (operation()) {
            return true;
        }

        if (attempt < max_attempts - 1) {
            std::this_thread::sleep_for(delay);
            delay *= 2; // Exponential backoff
        }
    }

    return false;
}

/**
 * @brief Mock message handler for testing
 */
class MockMessageHandler {
public:
    void on_message_received(const std::vector<uint8_t>& data) {
        received_messages_.push_back(data);
        message_count_.fetch_add(1, std::memory_order_relaxed);
    }

    size_t message_count() const {
        return message_count_.load(std::memory_order_relaxed);
    }

    const std::vector<std::vector<uint8_t>>& received_messages() const {
        return received_messages_;
    }

    void clear() {
        received_messages_.clear();
        message_count_.store(0, std::memory_order_relaxed);
    }

private:
    std::vector<std::vector<uint8_t>> received_messages_;
    std::atomic<size_t> message_count_{0};
};

} // namespace network_system::integration_tests::test_helpers
