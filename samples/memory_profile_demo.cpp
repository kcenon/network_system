// BSD 3-Clause License
//
// Copyright (c) 2021-2025, Network System Project
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
 * @file memory_profile_demo.cpp
 * @brief Memory profiling demonstration for Network System
 *
 * This demo shows how to profile memory usage patterns including:
 * - Connection memory overhead
 * - Message throughput memory
 * - Long-running stability
 */

#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/i_session.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <cstring>
#include <map>
#include <mutex>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#elif defined(__linux__)
#include <fstream>
#include <unistd.h>
#endif

using namespace kcenon::network;

/**
 * @brief Get current RSS (Resident Set Size) in bytes
 */
size_t get_current_rss() {
#if defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return info.resident_size;
    }
    return 0;
#elif defined(__linux__)
    std::ifstream statm("/proc/self/statm");
    size_t size, rss;
    statm >> size >> rss;
    return rss * static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
    return 0;
#endif
}

/**
 * @brief Get current virtual memory size in bytes
 */
size_t get_virtual_memory() {
#if defined(__APPLE__)
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return info.virtual_size;
    }
    return 0;
#elif defined(__linux__)
    std::ifstream statm("/proc/self/statm");
    size_t size;
    statm >> size;
    return size * static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
    return 0;
#endif
}

/**
 * @brief Print memory statistics with label
 */
void print_memory_stats(const std::string& label) {
    size_t rss = get_current_rss();
    size_t vms = get_virtual_memory();
    std::cout << std::left << std::setw(40) << ("[" + label + "]")
              << "RSS: " << std::setw(8) << (rss / (1024 * 1024)) << " MB"
              << "  VMS: " << std::setw(8) << (vms / (1024 * 1024)) << " MB"
              << std::endl;
}

/**
 * @brief Profile connection memory overhead
 */
void profile_connections(int num_connections) {
    std::cout << "\n========================================\n";
    std::cout << "  Profiling Connection Memory\n";
    std::cout << "  Target: " << num_connections << " connections\n";
    std::cout << "========================================\n\n";

    size_t baseline_rss = get_current_rss();
    print_memory_stats("Before server start");

    facade::tcp_facade tcp;
    auto server = tcp.create_server({
        .port = 5555,
        .server_id = "profile-server"
    });

    auto start_result = server->start(5555);

    if (start_result.is_err()) {
        std::cerr << "Failed to start server: " << start_result.error().message << std::endl;
        return;
    }

    print_memory_stats("After server start");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<std::shared_ptr<interfaces::i_protocol_client>> clients;
    clients.reserve(static_cast<size_t>(num_connections));

    size_t before_clients_rss = get_current_rss();

    for (int i = 0; i < num_connections; ++i) {
        auto client = tcp.create_client({
            .client_id = "client-" + std::to_string(i)
        });
        auto result = client->start("localhost", 5555);

        if (result.is_ok()) {
            clients.push_back(std::move(client));
        } else {
            std::cerr << "Failed to connect client " << i << std::endl;
        }

        if ((i + 1) % 50 == 0) {
            print_memory_stats("After " + std::to_string(i + 1) + " connections");
        }
    }

    size_t after_clients_rss = get_current_rss();
    size_t total_client_memory = after_clients_rss - before_clients_rss;

    std::cout << "\n--- Connection Memory Summary ---\n";
    std::cout << "Connections created: " << clients.size() << "\n";
    std::cout << "Total memory for clients: " << (total_client_memory / 1024) << " KB\n";
    if (!clients.empty()) {
        std::cout << "Memory per connection: ~"
                  << (total_client_memory / clients.size() / 1024) << " KB\n";
    }

    // Cleanup
    std::cout << "\n--- Cleanup ---\n";
    for (auto& client : clients) {
        (void)client->stop();
    }
    clients.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    print_memory_stats("After cleanup");

    (void)server->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    print_memory_stats("After server stop");

    size_t final_rss = get_current_rss();
    std::cout << "\nMemory delta from baseline: "
              << static_cast<long long>(final_rss - baseline_rss) / 1024 << " KB\n";
}

/**
 * @brief Profile message throughput memory
 */
void profile_message_throughput(int num_messages) {
    std::cout << "\n========================================\n";
    std::cout << "  Profiling Message Memory\n";
    std::cout << "  Target: " << num_messages << " messages\n";
    std::cout << "========================================\n\n";

    size_t baseline_rss = get_current_rss();
    print_memory_stats("Before start");

    facade::tcp_facade tcp;
    auto server = tcp.create_server({
        .port = 5556,
        .server_id = "profile-server"
    });

    auto server_result = server->start(5556);

    if (server_result.is_err()) {
        std::cerr << "Failed to start server\n";
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto client = tcp.create_client({
        .client_id = "profile-client"
    });
    auto client_result = client->start("localhost", 5556);

    if (client_result.is_err()) {
        std::cerr << "Failed to connect client\n";
        (void)server->stop();
        return;
    }

    print_memory_stats("Before messages");

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < num_messages; ++i) {
        std::vector<uint8_t> msg(1024, static_cast<uint8_t>(i % 256));
        (void)client->send(std::move(msg));

        if ((i + 1) % 5000 == 0) {
            print_memory_stats("After " + std::to_string(i + 1) + " messages");
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    print_memory_stats("After all messages");

    std::cout << "\n--- Message Throughput Summary ---\n";
    std::cout << "Messages sent: " << num_messages << "\n";
    std::cout << "Total time: " << duration.count() << " ms\n";
    std::cout << "Throughput: " << (num_messages * 1000 / std::max(1LL, static_cast<long long>(duration.count())))
              << " msg/s\n";

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::seconds(1));
    print_memory_stats("After processing");

    // Cleanup
    (void)client->stop();
    (void)server->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    print_memory_stats("After cleanup");

    size_t final_rss = get_current_rss();
    std::cout << "\nMemory delta from baseline: "
              << static_cast<long long>(final_rss - baseline_rss) / 1024 << " KB\n";
}

/**
 * @brief Profile long-running stability
 */
void profile_long_running(int duration_seconds) {
    std::cout << "\n========================================\n";
    std::cout << "  Profiling Long-Running Stability\n";
    std::cout << "  Duration: " << duration_seconds << " seconds\n";
    std::cout << "========================================\n\n";

    size_t baseline_rss = get_current_rss();
    size_t peak_rss = baseline_rss;
    print_memory_stats("Baseline");

    facade::tcp_facade tcp;
    auto server = tcp.create_server({
        .port = 5557,
        .server_id = "profile-server"
    });

    auto server_result = server->start(5557);

    if (server_result.is_err()) {
        std::cerr << "Failed to start server\n";
        return;
    }

    print_memory_stats("After server start");

    auto start_time = std::chrono::steady_clock::now();
    int iterations = 0;
    int successful_cycles = 0;

    while (std::chrono::steady_clock::now() - start_time <
           std::chrono::seconds(duration_seconds)) {

        // Create client, send message, disconnect
        auto client = tcp.create_client({
            .client_id = "temp-client"
        });
        auto result = client->start("localhost", 5557);

        if (result.is_ok()) {
            (void)client->send({'t', 'e', 's', 't'});
            (void)client->stop();
            successful_cycles++;
        }

        iterations++;

        size_t current_rss = get_current_rss();
        if (current_rss > peak_rss) {
            peak_rss = current_rss;
        }

        if (iterations % 100 == 0) {
            print_memory_stats("After " + std::to_string(iterations) + " cycles");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n--- Long-Running Summary ---\n";
    std::cout << "Total iterations: " << iterations << "\n";
    std::cout << "Successful cycles: " << successful_cycles << "\n";
    std::cout << "Peak RSS: " << (peak_rss / (1024 * 1024)) << " MB\n";

    (void)server->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    print_memory_stats("Final");

    size_t final_rss = get_current_rss();
    std::cout << "\nMemory delta from baseline: "
              << static_cast<long long>(final_rss - baseline_rss) / 1024 << " KB\n";
    std::cout << "Memory leak indicator: "
              << (final_rss > baseline_rss + 1024 * 1024 ? "POSSIBLE LEAK" : "OK")
              << "\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [mode]\n\n";
    std::cout << "Modes:\n";
    std::cout << "  connections  - Profile connection memory overhead\n";
    std::cout << "  messages     - Profile message throughput memory\n";
    std::cout << "  stability    - Profile long-running stability\n";
    std::cout << "  all          - Run all profiles (default)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " connections\n";
    std::cout << "  " << program_name << " all\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  Network System Memory Profiling Demo\n";
    std::cout << "========================================\n";

    std::string mode = "all";
    if (argc > 1) {
        if (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        mode = argv[1];
    }

    print_memory_stats("Program start");

    if (mode == "connections" || mode == "all") {
        profile_connections(200);
    }

    if (mode == "messages" || mode == "all") {
        profile_message_throughput(10000);
    }

    if (mode == "stability" || mode == "all") {
        profile_long_running(10);
    }

    std::cout << "\n========================================\n";
    std::cout << "  Profiling Complete\n";
    std::cout << "========================================\n";

    return 0;
}
