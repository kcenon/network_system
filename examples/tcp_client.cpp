/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Project
All rights reserved.
*****************************************************************************/

/**
 * @file tcp_client.cpp
 * @brief Minimal TCP client using the facade API
 *
 * Demonstrates:
 * - Creating a TCP client via tcp_facade
 * - Connecting to a server
 * - Sending text and binary messages
 * - Receiving responses via callback
 * - Checking connection status
 * - Clean disconnection
 *
 * Pair with the tcp_echo_server example:
 *   1. Start tcp_echo_server
 *   2. Run this tcp_client
 */

#include <kcenon/network/facade/tcp_facade.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network;

int main() {
    std::cout << "=== TCP Client Example ===" << std::endl;

    // Create TCP facade and client
    facade::tcp_facade tcp;
    constexpr uint16_t port = 9000;
    const std::string host = "127.0.0.1";

    auto client = tcp.create_client({
        .host = host,
        .port = port,
        .client_id = "ExampleClient",
    });

    // Set up receive callback to print server responses
    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        std::cout << "[Client] Received: " << message << std::endl;
    });

    // Set up connection and disconnection callbacks
    client->set_connected_callback([]() {
        std::cout << "[Client] Connected to server." << std::endl;
    });

    client->set_disconnected_callback([]() {
        std::cout << "[Client] Disconnected from server." << std::endl;
    });

    // Set up error callback
    client->set_error_callback([](std::error_code ec) {
        std::cerr << "[Client] Error: " << ec.message() << std::endl;
    });

    // Connect to the server
    std::cout << "[Client] Connecting to " << host << ":" << port << "..." << std::endl;

    auto connect_result = client->start(host, port);
    if (connect_result.is_err()) {
        std::cerr << "[Client] Connection failed: " << connect_result.error().message
                  << std::endl;
        return 1;
    }

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check connection status
    if (!client->is_connected()) {
        std::cerr << "[Client] Not connected." << std::endl;
        client->stop();
        return 1;
    }

    // Send text messages
    std::vector<std::string> messages = {
        "Hello, server!",
        "This is a TCP client example.",
        "Goodbye!",
    };

    for (const auto& msg : messages) {
        std::vector<uint8_t> data(msg.begin(), msg.end());
        std::cout << "[Client] Sending: " << msg << std::endl;

        auto send_result = client->send(std::move(data));
        if (send_result.is_err()) {
            std::cerr << "[Client] Send failed: " << send_result.error().message
                      << std::endl;
        }

        // Wait for echo response
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Send binary data
    std::cout << "[Client] Sending binary data..." << std::endl;
    std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04, 0xFF};
    auto binary_result = client->send(std::move(binary_data));
    if (binary_result.is_err()) {
        std::cerr << "[Client] Binary send failed: " << binary_result.error().message
                  << std::endl;
    }

    // Wait for response
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Disconnect
    std::cout << "[Client] Disconnecting..." << std::endl;
    auto stop_result = client->stop();
    if (stop_result.is_err()) {
        std::cerr << "[Client] Stop error: " << stop_result.error().message << std::endl;
    }

    std::cout << "[Client] Done." << std::endl;
    return 0;
}
