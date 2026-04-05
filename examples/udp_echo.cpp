/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Project
All rights reserved.
*****************************************************************************/

/**
 * @file udp_echo.cpp
 * @brief Minimal UDP echo server and client
 * @example udp_echo.cpp
 *
 * @par Category
 * Basic - UDP networking
 *
 * Demonstrates:
 * - Creating a UDP server via udp_facade
 * - Creating a UDP client via udp_facade
 * - Datagram-based communication (message boundaries preserved)
 * - Session tracking on the server side
 * - Error handling with Result<T>
 *
 * The server echoes each received datagram back to its sender.
 * The client sends a series of messages and prints the echoed responses.
 *
 * @see kcenon::network::facade::udp_facade
 * @see kcenon::network::interfaces::i_protocol_client
 * @see kcenon::network::interfaces::i_protocol_server
 */

#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/interfaces/i_session.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace kcenon::network;

static std::atomic<bool> g_done{false};

void run_server() {
    facade::udp_facade udp;
    constexpr uint16_t port = 9003;

    auto server = udp.create_server({
        .port = port,
        .server_id = "UdpEchoServer",
    });

    std::mutex sessions_mutex;
    std::unordered_map<std::string, std::shared_ptr<interfaces::i_session>> sessions;

    server->set_connection_callback(
        [&sessions, &sessions_mutex](std::shared_ptr<interfaces::i_session> session) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions[std::string(session->id())] = session;
            std::cout << "[Server] New peer: " << session->id() << std::endl;
        });

    server->set_disconnection_callback(
        [&sessions, &sessions_mutex](std::string_view session_id) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions.erase(std::string(session_id));
        });

    server->set_receive_callback(
        [&sessions, &sessions_mutex](std::string_view session_id,
                                     const std::vector<uint8_t>& data) {
            std::string msg(data.begin(), data.end());
            std::cout << "[Server] From " << session_id << ": " << msg << std::endl;

            // Echo back
            std::shared_ptr<interfaces::i_session> session;
            {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                auto it = sessions.find(std::string(session_id));
                if (it != sessions.end()) {
                    session = it->second;
                }
            }
            if (session) {
                std::string echo = "Echo: " + msg;
                std::vector<uint8_t> echo_data(echo.begin(), echo.end());
                session->send(std::move(echo_data));
            }
        });

    server->set_error_callback(
        [](std::string_view session_id, std::error_code ec) {
            std::cerr << "[Server] Error on " << session_id << ": " << ec.message()
                      << std::endl;
        });

    std::cout << "[Server] UDP echo server on port " << port << std::endl;

    while (!g_done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[Server] Stopped." << std::endl;
}

void run_client() {
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    facade::udp_facade udp;
    auto client = udp.create_client({
        .host = "127.0.0.1",
        .port = 9003,
        .client_id = "UdpClient",
    });

    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        std::string msg(data.begin(), data.end());
        std::cout << "[Client] Received: " << msg << std::endl;
    });

    client->set_error_callback([](std::error_code ec) {
        std::cerr << "[Client] Error: " << ec.message() << std::endl;
    });

    // Send messages
    std::vector<std::string> messages = {
        "Hello UDP!",
        "Datagrams preserve boundaries",
        "Fast and lightweight",
    };

    for (const auto& msg : messages) {
        std::vector<uint8_t> data(msg.begin(), msg.end());
        std::cout << "[Client] Sending: " << msg << std::endl;

        auto result = client->send(std::move(data));
        if (result.is_err()) {
            std::cerr << "[Client] Send failed: " << result.error().message << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Wait for final responses
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    g_done.store(true);
}

int main() {
    std::cout << "=== UDP Echo Example ===" << std::endl;

    try {
        std::thread server_thread(run_server);
        std::thread client_thread(run_client);

        client_thread.join();
        server_thread.join();

        std::cout << "\n=== UDP echo example completed ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
