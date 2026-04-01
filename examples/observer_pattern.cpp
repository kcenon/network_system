/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Project
All rights reserved.
*****************************************************************************/

/**
 * @file observer_pattern.cpp
 * @brief Demonstrates the connection_observer and callback_adapter patterns
 *
 * Demonstrates:
 * - Implementing connection_observer for unified event handling
 * - Using null_connection_observer for partial event handling
 * - Using callback_adapter for functional-style event handling
 * - Setting an observer on a TCP client
 *
 * These patterns provide alternatives to individual callback setters
 * and centralize event handling in a single object.
 */

#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <kcenon/network/interfaces/i_session.h>

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network;

/**
 * @brief Custom observer that handles all connection events
 *
 * This demonstrates the recommended pattern for handling client events.
 * All events go through a single object, making it easy to maintain state.
 */
class chat_observer : public interfaces::connection_observer {
public:
    explicit chat_observer(const std::string& name) : name_(name) {}

    auto on_connected() -> void override {
        std::cout << "[" << name_ << "] Connected to server." << std::endl;
    }

    auto on_disconnected(std::optional<std::string_view> reason) -> void override {
        std::cout << "[" << name_ << "] Disconnected";
        if (reason.has_value()) {
            std::cout << " (reason: " << reason.value() << ")";
        }
        std::cout << std::endl;
    }

    auto on_receive(std::span<const uint8_t> data) -> void override {
        std::string message(data.begin(), data.end());
        std::cout << "[" << name_ << "] Received: " << message << std::endl;
        ++message_count_;
    }

    auto on_error(std::error_code ec) -> void override {
        std::cerr << "[" << name_ << "] Error: " << ec.message() << std::endl;
    }

    [[nodiscard]] auto message_count() const -> int { return message_count_; }

private:
    std::string name_;
    int message_count_ = 0;
};

/**
 * @brief Partial observer that only handles receive events
 *
 * Inherits from null_connection_observer so unneeded methods are no-ops.
 */
class receive_only_observer : public interfaces::null_connection_observer {
public:
    auto on_receive(std::span<const uint8_t> data) -> void override {
        std::string message(data.begin(), data.end());
        std::cout << "[ReceiveOnly] Got: " << message << std::endl;
    }
};

int main() {
    std::cout << "=== Observer Pattern Example ===" << std::endl;

    // --- Start a local echo server ---
    facade::tcp_facade tcp;
    constexpr uint16_t port = 9004;

    auto server = tcp.create_server({
        .port = port,
        .server_id = "ObserverTestServer",
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
    std::cout << "Echo server running on port " << port << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // --- Example 1: Full connection_observer ---
    std::cout << "\n--- Example 1: Full connection_observer ---" << std::endl;
    {
        auto client = tcp.create_client({
            .host = "127.0.0.1",
            .port = port,
            .client_id = "ObserverClient",
        });

        auto observer = std::make_shared<chat_observer>("FullObserver");
        client->set_observer(observer);

        auto result = client->start("127.0.0.1", port);
        if (result.is_ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            std::string msg = "Hello via observer!";
            std::vector<uint8_t> data(msg.begin(), msg.end());
            client->send(std::move(data));

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout << "Messages received: " << observer->message_count() << std::endl;
        }

        client->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // --- Example 2: callback_adapter (functional style) ---
    std::cout << "\n--- Example 2: callback_adapter ---" << std::endl;
    {
        auto client = tcp.create_client({
            .host = "127.0.0.1",
            .port = port,
            .client_id = "AdapterClient",
        });

        auto adapter = std::make_shared<interfaces::callback_adapter>();
        adapter->on_connected([]() {
                   std::cout << "[Adapter] Connected!" << std::endl;
               })
            .on_receive([](std::span<const uint8_t> data) {
                std::string msg(data.begin(), data.end());
                std::cout << "[Adapter] Received: " << msg << std::endl;
            })
            .on_disconnected([](std::optional<std::string_view> reason) {
                std::cout << "[Adapter] Disconnected." << std::endl;
            })
            .on_error([](std::error_code ec) {
                std::cerr << "[Adapter] Error: " << ec.message() << std::endl;
            });

        client->set_observer(adapter);

        auto result = client->start("127.0.0.1", port);
        if (result.is_ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            std::string msg = "Hello via callback_adapter!";
            std::vector<uint8_t> data(msg.begin(), msg.end());
            client->send(std::move(data));

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        client->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // --- Cleanup ---
    server->stop();
    std::cout << "\n=== Observer pattern example completed ===" << std::endl;

    return 0;
}
