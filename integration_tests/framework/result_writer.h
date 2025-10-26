/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "memory_profiler.h"
#include "test_helpers.h"

#include <string>
#include <vector>

namespace network_system::integration_tests {

/**
 * @struct PerformanceResult
 * @brief Container for performance test results
 */
struct PerformanceResult {
    std::string test_name;          // Name of the test
    std::string protocol;           // "tcp", "udp", "websocket"
    test_helpers::Statistics latency_ms;  // Latency statistics (P50, P95, P99, etc.)
    double throughput_msg_s{0.0};   // Throughput in messages per second
    double bandwidth_mbps{0.0};     // Bandwidth in megabytes per second
    MemoryMetrics memory;           // Memory usage metrics
    std::string timestamp;          // ISO 8601 timestamp
    std::string platform;           // Platform identifier (e.g., "ubuntu-22.04")
    std::string compiler;           // Compiler info (e.g., "gcc-11")
};

/**
 * @class ResultWriter
 * @brief Writes performance results to JSON and CSV formats
 *
 * Provides serialization of performance test results for analysis
 * and baseline tracking.
 *
 * Usage:
 * @code
 * ResultWriter writer;
 * std::vector<PerformanceResult> results = run_tests();
 * writer.write_json("results.json", results);
 * writer.write_csv("results.csv", results);
 * @endcode
 */
class ResultWriter {
public:
    /**
     * @brief Write results to JSON file
     * @param path Output file path
     * @param results Vector of performance results
     * @return true if write succeeded, false otherwise
     */
    [[nodiscard]] auto write_json(const std::string& path,
                                   const std::vector<PerformanceResult>& results) const -> bool;

    /**
     * @brief Write results to CSV file
     * @param path Output file path
     * @param results Vector of performance results
     * @return true if write succeeded, false otherwise
     */
    [[nodiscard]] auto write_csv(const std::string& path,
                                  const std::vector<PerformanceResult>& results) const -> bool;

private:
    /**
     * @brief Get current ISO 8601 timestamp
     */
    [[nodiscard]] static auto get_timestamp() -> std::string;

    /**
     * @brief Escape string for CSV format
     */
    [[nodiscard]] static auto escape_csv(const std::string& str) -> std::string;
};

} // namespace network_system::integration_tests
