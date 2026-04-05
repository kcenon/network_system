/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Project
All rights reserved.
*****************************************************************************/

/**
 * @file tcp_echo_server.cpp
 * @brief Minimal TCP echo server using the facade API
 * @example tcp_echo_server.cpp
 *
 * @par Category
 * Basic - TCP networking
 *
 * Demonstrates:
 * - Creating a TCP server via tcp_facade
 * - Setting up connection, disconnection, receive, and error callbacks
 * - Session management with thread-safe session tracking
 * - Echoing received data back to the client
 * - Graceful shutdown
 *
 * Run this server, then connect with the tcp_client example or any TCP client:
 *   echo "hello" | nc localhost 9000
 *
 * @see kcenon::network::facade::tcp_facade
 * @see kcenon::network::interfaces::i_protocol_server
 * @see kcenon::network::interfaces::i_session
 */

#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/i_session.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

using namespace kcenon::network;

// Global flag for graceful shutdown
static std::atomic<bool> g_running{true};

void signal_handler(int /*signum*/) {
    g_running.store(false);
}

int main() {
    std::cout << "=== TCP Echo Server Example ===" << std::endl;

    // Register signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Create TCP facade and server
    facade::tcp_facade tcp;
    constexpr uint16_t port = 9000;

    auto server = tcp.create_server({
        .port = port,
        .server_id = "EchoServer",
    });

    // Thread-safe session storage for echoing data back
    std::mutex sessions_mutex;
    std::map<std::string, std::shared_ptr<interfaces::i_session>> sessions;

    // Track new connections
    server->set_connection_callback(
        [&sessions, &sessions_mutex](std::shared_ptr<interfaces::i_session> session) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            std::string id(session->id());
            sessions[id] = session;
            std::cout << "[Server] Client connected: " << id << std::endl;
        });

    // Track disconnections
    server->set_disconnection_callback(
        [&sessions, &sessions_mutex](std::string_view session_id) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions.erase(std::string(session_id));
            std::cout << "[Server] Client disconnected: " << session_id << std::endl;
        });

    // Echo received data back to the sender
    server->set_receive_callback(
        [&sessions, &sessions_mutex](std::string_view session_id,
                                     const std::vector<uint8_t>& data) {
            std::string message(data.begin(), data.end());
            std::cout << "[Server] Received from " << session_id << ": " << message
                      << std::endl;

            // Find the session and echo data back
            std::shared_ptr<interfaces::i_session> session;
            {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                auto it = sessions.find(std::string(session_id));
                if (it != sessions.end()) {
                    session = it->second;
                }
            }

            if (session) {
                auto result = session->send(std::vector<uint8_t>(data));
                if (result.is_err()) {
                    std::cerr << "[Server] Echo failed: " << result.error().message
                              << std::endl;
                }
            }
        });

    // Log errors
    server->set_error_callback(
        [](std::string_view session_id, std::error_code ec) {
            std::cerr << "[Server] Error on " << session_id << ": " << ec.message()
                      << std::endl;
        });

    // Start the server
    auto result = server->start(port);
    if (result.is_err()) {
        std::cerr << "Failed to start server: " << result.error().message << std::endl;
        return 1;
    }

    std::cout << "[Server] Listening on port " << port << std::endl;
    std::cout << "[Server] Press Ctrl+C to stop." << std::endl;

    // Main loop: wait until shutdown signal
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Graceful shutdown
    std::cout << "\n[Server] Shutting down..." << std::endl;
    auto stop_result = server->stop();
    if (stop_result.is_err()) {
        std::cerr << "[Server] Stop error: " << stop_result.error().message << std::endl;
    }

    std::cout << "[Server] Stopped." << std::endl;
    return 0;
}
