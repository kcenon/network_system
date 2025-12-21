/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "result_writer.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace kcenon::network::integration_tests {

auto ResultWriter::write_json(const std::string& path,
                               const std::vector<PerformanceResult>& results) const -> bool {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "{\n";
    file << "  \"timestamp\": \"" << get_timestamp() << "\",\n";
    file << "  \"results\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];

        file << "    {\n";
        file << "      \"test_name\": \"" << r.test_name << "\",\n";
        file << "      \"protocol\": \"" << r.protocol << "\",\n";
        file << "      \"latency_ms\": {\n";
        file << "        \"min\": " << r.latency_ms.min << ",\n";
        file << "        \"max\": " << r.latency_ms.max << ",\n";
        file << "        \"mean\": " << r.latency_ms.mean << ",\n";
        file << "        \"p50\": " << r.latency_ms.p50 << ",\n";
        file << "        \"p95\": " << r.latency_ms.p95 << ",\n";
        file << "        \"p99\": " << r.latency_ms.p99 << ",\n";
        file << "        \"stddev\": " << r.latency_ms.stddev << "\n";
        file << "      },\n";
        file << "      \"throughput_msg_s\": " << r.throughput_msg_s << ",\n";
        file << "      \"bandwidth_mbps\": " << r.bandwidth_mbps << ",\n";
        file << "      \"memory\": {\n";
        file << "        \"rss_mb\": " << r.memory.rss_mb() << ",\n";
        file << "        \"heap_mb\": " << r.memory.heap_mb() << ",\n";
        file << "        \"vm_mb\": " << r.memory.vm_mb() << "\n";
        file << "      },\n";
        file << "      \"platform\": \"" << r.platform << "\",\n";
        file << "      \"compiler\": \"" << r.compiler << "\"\n";
        file << "    }";

        if (i < results.size() - 1) {
            file << ",";
        }
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    return true;
}

auto ResultWriter::write_csv(const std::string& path,
                              const std::vector<PerformanceResult>& results) const -> bool {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    // Write CSV header
    file << "test_name,protocol,";
    file << "latency_min_ms,latency_max_ms,latency_mean_ms,";
    file << "latency_p50_ms,latency_p95_ms,latency_p99_ms,latency_stddev_ms,";
    file << "throughput_msg_s,bandwidth_mbps,";
    file << "memory_rss_mb,memory_heap_mb,memory_vm_mb,";
    file << "platform,compiler\n";

    // Write data rows
    for (const auto& r : results) {
        file << escape_csv(r.test_name) << ",";
        file << escape_csv(r.protocol) << ",";
        file << r.latency_ms.min << ",";
        file << r.latency_ms.max << ",";
        file << r.latency_ms.mean << ",";
        file << r.latency_ms.p50 << ",";
        file << r.latency_ms.p95 << ",";
        file << r.latency_ms.p99 << ",";
        file << r.latency_ms.stddev << ",";
        file << r.throughput_msg_s << ",";
        file << r.bandwidth_mbps << ",";
        file << r.memory.rss_mb() << ",";
        file << r.memory.heap_mb() << ",";
        file << r.memory.vm_mb() << ",";
        file << escape_csv(r.platform) << ",";
        file << escape_csv(r.compiler) << "\n";
    }

    return true;
}

auto ResultWriter::get_timestamp() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

auto ResultWriter::escape_csv(const std::string& str) -> std::string {
    // If string contains comma, quote, or newline, wrap in quotes and escape quotes
    if (str.find_first_of(",\"\n") != std::string::npos) {
        std::string escaped = "\"";
        for (char c : str) {
            if (c == '"') {
                escaped += "\"\"";  // Escape quote with double quote
            } else {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
    }
    return str;
}

} // namespace kcenon::network::integration_tests
