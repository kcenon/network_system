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
#include <cstdlib>
#include <string_view>
#include <memory>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <atomic>
#include <cstdio>

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
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        // Check if client is actually connected
        if (client && client->is_connected()) {
            return true;
        }

        // Give time for async operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
 * @brief Detect whether tests are running in a CI environment
 * @return true when common CI environment variables are present
 */
inline bool is_ci_environment() {
    const auto flag_set = [](const char* value) {
        return value != nullptr && *value != '\0' && std::string_view(value) != "0";
    };

    return flag_set(std::getenv("CI")) ||
           flag_set(std::getenv("GITHUB_ACTIONS")) ||
           flag_set(std::getenv("NETWORK_SYSTEM_CI"));
}

/**
 * @brief Detect whether tests are running under a sanitizer (ASan, TSan, etc.)
 * @return true when sanitizer environment variables are present
 */
inline bool is_sanitizer_run() {
    const auto flag_set = [](const char* value) {
        return value != nullptr && *value != '\0' && std::string_view(value) != "0";
    };

    return flag_set(std::getenv("TSAN_OPTIONS")) ||
           flag_set(std::getenv("ASAN_OPTIONS")) ||
           flag_set(std::getenv("UBSAN_OPTIONS")) ||
           flag_set(std::getenv("MSAN_OPTIONS")) ||
           flag_set(std::getenv("SANITIZER")) ||
           flag_set(std::getenv("NETWORK_SYSTEM_SANITIZER"));
}

/**
 * @brief Get platform identifier
 * @return Platform name string (e.g., "ubuntu-22.04", "macos-13", "windows-2022")
 */
inline std::string get_platform_name() {
#if defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#else
    return "unknown";
#endif
}

/**
 * @brief Get compiler identifier
 * @return Compiler name and version string
 */
inline std::string get_compiler_name() {
#if defined(__clang__)
    return "clang-" + std::to_string(__clang_major__);
#elif defined(__GNUC__)
    return "gcc-" + std::to_string(__GNUC__);
#elif defined(_MSC_VER)
    return "msvc-" + std::to_string(_MSC_VER / 100);
#else
    return "unknown";
#endif
}

/**
 * @brief Watchdog that aborts a test if it runs longer than the given timeout.
 */
class ScopedTestTimeout {
public:
    ScopedTestTimeout(std::chrono::milliseconds timeout, std::string context)
        : cancel_{false}, watchdog_([timeout, ctx = std::move(context), this]() {
              const auto deadline = std::chrono::steady_clock::now() + timeout;
              while (!cancel_.load(std::memory_order_acquire)) {
                  if (std::chrono::steady_clock::now() >= deadline) {
                      std::fprintf(stderr,
                                   "[network_system][timeout] %s exceeded %lld ms, aborting to avoid hang\n",
                                   ctx.c_str(),
                                   static_cast<long long>(timeout.count()));
                      std::fflush(stderr);
                      std::abort();
                  }
                  std::this_thread::sleep_for(std::chrono::milliseconds(10));
              }
          }) {}

    ScopedTestTimeout(const ScopedTestTimeout&) = delete;
    ScopedTestTimeout& operator=(const ScopedTestTimeout&) = delete;

    ScopedTestTimeout(ScopedTestTimeout&& other) noexcept
        : cancel_(other.cancel_.load(std::memory_order_relaxed)),
          watchdog_(std::move(other.watchdog_)) {
        other.cancel_.store(true, std::memory_order_relaxed);
    }

    ScopedTestTimeout& operator=(ScopedTestTimeout&& other) noexcept {
        if (this != &other) {
            cancel_.store(true, std::memory_order_release);
            if (watchdog_.joinable()) {
                watchdog_.join();
            }
            cancel_.store(other.cancel_.load(std::memory_order_acquire), std::memory_order_release);
            watchdog_ = std::move(other.watchdog_);
            other.cancel_.store(true, std::memory_order_release);
        }
        return *this;
    }

    ~ScopedTestTimeout() {
        cancel_.store(true, std::memory_order_release);
        if (watchdog_.joinable()) {
            watchdog_.join();
        }
    }

private:
    std::atomic<bool> cancel_;
    std::thread watchdog_;
};

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
