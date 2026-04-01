/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Project
All rights reserved.
*****************************************************************************/

/**
 * @file connection_pool.cpp
 * @brief TCP connection pool usage example
 *
 * Demonstrates:
 * - Creating a connection pool via tcp_facade
 * - Acquiring and releasing connections
 * - Monitoring pool utilization
 * - Concurrent access from multiple threads
 * - Proper error handling with Result<T>
 *
 * The example starts a local echo server, then exercises the pool
 * with single-threaded and multi-threaded workloads.
 */

#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/i_session.h>
#include "internal/core/connection_pool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network;

int main() {
    std::cout << "=== Connection Pool Example ===" << std::endl;

    // --- Set up a local echo server for testing ---
    facade::tcp_facade tcp;
    constexpr uint16_t port = 9002;

    auto server = tcp.create_server({
        .port = port,
        .server_id = "PoolTestServer",
    });

    std::mutex sessions_mutex;
    std::map<std::string, std::shared_ptr<interfaces::i_session>> sessions;

    server->set_connection_callback(
        [&sessions, &sessions_mutex](std::shared_ptr<interfaces::i_session> session) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions[std::string(session->id())] = session;
        });

    server->set_disconnection_callback(
        [&sessions, &sessions_mutex](std::string_view session_id) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions.erase(std::string(session_id));
        });

    server->set_receive_callback(
        [&sessions, &sessions_mutex](std::string_view session_id,
                                     const std::vector<uint8_t>& data) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            auto it = sessions.find(std::string(session_id));
            if (it != sessions.end()) {
                it->second->send(std::vector<uint8_t>(data));
            }
        });

    auto server_result = server->start(port);
    if (server_result.is_err()) {
        std::cerr << "Server start failed: " << server_result.error().message << std::endl;
        return 1;
    }
    std::cout << "Echo server started on port " << port << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Create and initialize the connection pool ---
    constexpr size_t pool_size = 5;

    auto pool = tcp.create_connection_pool({
        .host = "127.0.0.1",
        .port = port,
        .pool_size = pool_size,
    });

    auto init_result = pool->initialize();
    if (init_result.is_err()) {
        std::cerr << "Pool init failed: " << init_result.error().message << std::endl;
        server->stop();
        return 1;
    }
    std::cout << "Connection pool initialized: " << pool->pool_size() << " connections"
              << std::endl;

    // --- Single-threaded usage ---
    std::cout << "\n--- Single-threaded usage ---" << std::endl;
    {
        auto client = pool->acquire();
        std::cout << "Acquired connection. Active: " << pool->active_count() << "/"
                  << pool->pool_size() << std::endl;

        std::vector<uint8_t> data = {'p', 'i', 'n', 'g'};
        auto result = client->send_packet(std::move(data));
        if (result.is_ok()) {
            std::cout << "Sent 'ping' successfully" << std::endl;
        } else {
            std::cerr << "Send failed: " << result.error().message << std::endl;
        }

        pool->release(std::move(client));
        std::cout << "Released connection. Active: " << pool->active_count() << "/"
                  << pool->pool_size() << std::endl;
    }

    // --- Multi-threaded usage ---
    std::cout << "\n--- Multi-threaded usage ---" << std::endl;
    {
        constexpr int num_workers = 4;
        constexpr int requests_per_worker = 10;

        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        std::vector<std::future<void>> futures;

        auto start_time = std::chrono::steady_clock::now();

        for (int w = 0; w < num_workers; ++w) {
            futures.push_back(
                std::async(std::launch::async, [&pool, w, &success_count, &failure_count]() {
                    for (int i = 0; i < requests_per_worker; ++i) {
                        auto client = pool->acquire();

                        std::vector<uint8_t> data = {
                            static_cast<uint8_t>('A' + w),
                            static_cast<uint8_t>('0' + i),
                        };

                        auto result = client->send_packet(std::move(data));
                        if (result.is_ok()) {
                            success_count.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            failure_count.fetch_add(1, std::memory_order_relaxed);
                        }

                        pool->release(std::move(client));
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                }));
        }

        for (auto& f : futures) {
            f.wait();
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time)
                .count();

        int total = num_workers * requests_per_worker;
        std::cout << "Completed " << success_count.load() << "/" << total
                  << " requests in " << duration_ms << " ms" << std::endl;
        std::cout << "Failed: " << failure_count.load() << std::endl;
        if (duration_ms > 0) {
            std::cout << "Throughput: " << (total * 1000 / duration_ms) << " req/s"
                      << std::endl;
        }
    }

    // --- Cleanup ---
    std::cout << "\nStopping server..." << std::endl;
    server->stop();
    std::cout << "=== Connection pool example completed ===" << std::endl;

    return 0;
}
