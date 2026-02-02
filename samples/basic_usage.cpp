/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Network System Project
 *
 * This example demonstrates the facade API for creating TCP clients and servers.
 * The facade pattern provides a simplified interface compared to direct instantiation
 * of protocol implementation classes.
 */

#include <kcenon/network/facade/tcp_facade.h>
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

    // Create TCP facade and server using factory method
    facade::tcp_facade tcp;
    const unsigned short port = 8080;

    auto server = tcp.create_server({
        .port = port,
        .server_id = "BasicServer"
    });
    std::cout << "Server created with ID: BasicServer" << std::endl;

    // Start server (port already set in config)
    auto server_result = server->start(port);

    if (server_result.is_err()) {
        std::cerr << "✗ Failed to start server" << std::endl;
        return -1;
    }

    std::cout << "✓ Server started on port " << port << std::endl;

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. TCP Client Setup
    std::cout << "\n2. TCP Client Setup:" << std::endl;

    // Create TCP client using facade factory method
    auto client = tcp.create_client({
        .host = "localhost",
        .port = port,
        .client_id = "BasicClient"
    });
    std::cout << "Client created with ID: BasicClient" << std::endl;

    // Connect to server
    auto client_result = client->start("localhost", port);

    if (client_result.is_err()) {
        std::cerr << "✗ Failed to connect client" << std::endl;
        server->stop();
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
    auto send_result = client->send(std::move(data));

    if (send_result.is_err()) {
        std::cerr << "✗ Failed to send message" << std::endl;
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

    auto binary_result = client->send(std::move(binary_data));

    if (binary_result.is_err()) {
        std::cerr << "✗ Failed to send binary data" << std::endl;
    } else {
        std::cout << "✓ Binary data sent successfully" << std::endl;
    }

    // 6. Cleanup
    std::cout << "\n6. Cleanup:" << std::endl;

    // Stop client
    auto client_stop_result = client->stop();
    if (client_stop_result.is_err()) {
        std::cerr << "✗ Failed to stop client" << std::endl;
    } else {
        std::cout << "✓ Client stopped" << std::endl;
    }

    // Stop server
    auto server_stop_result = server->stop();
    if (server_stop_result.is_err()) {
        std::cerr << "✗ Failed to stop server" << std::endl;
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
