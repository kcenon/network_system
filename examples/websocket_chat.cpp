/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Project
All rights reserved.
*****************************************************************************/

/**
 * @file websocket_chat.cpp
 * @brief WebSocket chat server and client in a single program
 *
 * Demonstrates:
 * - Creating a WebSocket server via websocket_facade
 * - Creating a WebSocket client via websocket_facade
 * - Broadcasting messages from one client to all connected sessions
 * - Session tracking on the server side
 * - Full-duplex communication over WebSocket (RFC 6455)
 *
 * This example starts a server and two clients in separate threads,
 * simulating a simple chat room.
 */

#include <kcenon/network/facade/websocket_facade.h>
#include <kcenon/network/interfaces/i_session.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network;

// Shared state
static std::atomic<bool> g_server_ready{false};
static std::atomic<bool> g_done{false};

/**
 * @brief Run a WebSocket chat server that broadcasts messages to all clients
 */
void run_server() {
    facade::websocket_facade ws;
    constexpr uint16_t port = 9001;

    auto server = ws.create_server({
        .port = port,
        .path = "/chat",
        .server_id = "ChatServer",
    });

    // Track connected sessions
    std::mutex sessions_mutex;
    std::map<std::string, std::shared_ptr<interfaces::i_session>> sessions;

    server->set_connection_callback(
        [&sessions, &sessions_mutex](std::shared_ptr<interfaces::i_session> session) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            std::string id(session->id());
            sessions[id] = session;
            std::cout << "[Server] User joined: " << id << " ("
                      << sessions.size() << " online)" << std::endl;
        });

    server->set_disconnection_callback(
        [&sessions, &sessions_mutex](std::string_view session_id) {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions.erase(std::string(session_id));
            std::cout << "[Server] User left: " << session_id << " ("
                      << sessions.size() << " online)" << std::endl;
        });

    // Broadcast received messages to all connected clients
    server->set_receive_callback(
        [&sessions, &sessions_mutex](std::string_view sender_id,
                                     const std::vector<uint8_t>& data) {
            std::string message(data.begin(), data.end());
            std::cout << "[Server] " << sender_id << " says: " << message << std::endl;

            // Broadcast to all sessions
            std::string broadcast = std::string(sender_id) + ": " + message;
            std::vector<uint8_t> broadcast_data(broadcast.begin(), broadcast.end());

            std::lock_guard<std::mutex> lock(sessions_mutex);
            for (auto& [id, session] : sessions) {
                session->send(std::vector<uint8_t>(broadcast_data));
            }
        });

    server->set_error_callback(
        [](std::string_view session_id, std::error_code ec) {
            std::cerr << "[Server] Error on " << session_id << ": " << ec.message()
                      << std::endl;
        });

    auto result = server->start(port);
    if (result.is_err()) {
        std::cerr << "[Server] Failed to start: " << result.error().message << std::endl;
        return;
    }

    std::cout << "[Server] Chat server running on ws://localhost:" << port << "/chat"
              << std::endl;
    g_server_ready.store(true);

    // Wait until chat is done
    while (!g_done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server->stop();
    std::cout << "[Server] Stopped." << std::endl;
}

/**
 * @brief Run a WebSocket chat client that sends messages
 */
void run_client(const std::string& name, const std::vector<std::string>& messages) {
    // Wait for server to be ready
    while (!g_server_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    facade::websocket_facade ws;
    auto client = ws.create_client({
        .client_id = name,
    });

    // Print received broadcast messages
    client->set_receive_callback([&name](const std::vector<uint8_t>& data) {
        std::string msg(data.begin(), data.end());
        std::cout << "  [" << name << " received] " << msg << std::endl;
    });

    client->set_error_callback([&name](std::error_code ec) {
        std::cerr << "  [" << name << "] Error: " << ec.message() << std::endl;
    });

    // Connect to the chat server
    auto connect_result = client->start("127.0.0.1", 9001);
    if (connect_result.is_err()) {
        std::cerr << "  [" << name << "] Connection failed: "
                  << connect_result.error().message << std::endl;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Send messages with delays
    for (const auto& msg : messages) {
        std::vector<uint8_t> data(msg.begin(), msg.end());
        auto send_result = client->send(std::move(data));
        if (send_result.is_err()) {
            std::cerr << "  [" << name << "] Send failed: "
                      << send_result.error().message << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    client->stop();
}

int main() {
    std::cout << "=== WebSocket Chat Example ===" << std::endl;

    try {
        // Start server
        std::thread server_thread(run_server);

        // Start two clients with different messages
        std::thread client1(run_client, "Alice",
                            std::vector<std::string>{"Hello everyone!", "How are you?"});
        std::thread client2(run_client, "Bob",
                            std::vector<std::string>{"Hi Alice!", "I am fine, thanks!"});

        // Wait for clients to finish
        client1.join();
        client2.join();

        // Signal server to stop
        g_done.store(true);
        server_thread.join();

        std::cout << "\n=== Chat example completed ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
