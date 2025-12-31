/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Network System Project
 */

#include <kcenon/network/core/messaging_server.h>
#include <kcenon/network/core/messaging_client.h>
#include <kcenon/network/compatibility.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

using namespace kcenon::network;

int main() {
    std::cout << "=== Network System - Basic Usage Example ===" << std::endl;

    // 1. TCP Server Setup
    std::cout << "\n1. TCP Server Setup:" << std::endl;

    auto server = std::make_shared<core::messaging_server>("BasicServer");
    std::cout << "Server created with ID: BasicServer" << std::endl;

    // Start server on port 8080
    const unsigned short port = 8080;
    auto server_result = server->start_server(port);

    if (server_result.is_err()) {
        std::cerr << "✗ Failed to start server: " << server_result.error().message << std::endl;
        return -1;
    }

    std::cout << "✓ Server started on port " << port << std::endl;

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. TCP Client Setup
    std::cout << "\n2. TCP Client Setup:" << std::endl;

    auto client = std::make_shared<core::messaging_client>("BasicClient");
    std::cout << "Client created with ID: BasicClient" << std::endl;

    // Connect to server
    auto client_result = client->start_client("localhost", port);

    if (client_result.is_err()) {
        std::cerr << "✗ Failed to connect client: " << client_result.error().message << std::endl;
        server->stop_server();
        return -1;
    }

    std::cout << "✓ Client connected to localhost:" << port << std::endl;

    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. Data Transmission
    std::cout << "\n3. Data Transmission:" << std::endl;

    // Send text message
    std::string message = "Hello from Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());

    std::cout << "Sending message: \"" << message << "\"" << std::endl;

    // Use std::move for zero-copy efficiency
    auto send_result = client->send_packet(std::move(data));

    if (send_result.is_err()) {
        std::cerr << "✗ Failed to send message: " << send_result.error().message << std::endl;
    } else {
        std::cout << "✓ Message sent successfully" << std::endl;
    }

    // 4. Connection Status
    std::cout << "\n4. Connection Status:" << std::endl;

    if (client->is_connected()) {
        std::cout << "✓ Client is connected" << std::endl;
    } else {
        std::cout << "✗ Client is disconnected" << std::endl;
    }

    // 5. Binary Data Transmission
    std::cout << "\n5. Binary Data Transmission:" << std::endl;

    std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xFE, 0xFD};
    std::cout << "Sending binary data (" << binary_data.size() << " bytes)" << std::endl;

    auto binary_result = client->send_packet(std::move(binary_data));

    if (binary_result.is_err()) {
        std::cerr << "✗ Failed to send binary data: " << binary_result.error().message << std::endl;
    } else {
        std::cout << "✓ Binary data sent successfully" << std::endl;
    }

    // 6. Cleanup
    std::cout << "\n6. Cleanup:" << std::endl;

    // Stop client
    auto client_stop_result = client->stop_client();
    if (client_stop_result.is_err()) {
        std::cerr << "✗ Failed to stop client: " << client_stop_result.error().message << std::endl;
    } else {
        std::cout << "✓ Client stopped" << std::endl;
    }

    // Stop server
    auto server_stop_result = server->stop_server();
    if (server_stop_result.is_err()) {
        std::cerr << "✗ Failed to stop server: " << server_stop_result.error().message << std::endl;
    } else {
        std::cout << "✓ Server stopped" << std::endl;
    }

    // Summary
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "✓ Server/Client creation" << std::endl;
    std::cout << "✓ TCP connection establishment" << std::endl;
    std::cout << "✓ Text and binary data transmission" << std::endl;
    std::cout << "✓ Result<T> based error handling" << std::endl;
    std::cout << "✓ Zero-copy data transfer with std::move" << std::endl;
    std::cout << "✓ Clean shutdown" << std::endl;

    std::cout << "\n=== Basic Usage Example completed ===" << std::endl;
    return 0;
}
