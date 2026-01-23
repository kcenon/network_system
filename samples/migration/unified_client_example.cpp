/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file unified_client_example.cpp
 * @brief Example showing the new unified client interface usage
 *
 * This file demonstrates the NEW, RECOMMENDED way of creating network clients
 * using the unified interface API.
 *
 * Benefits:
 * - Single interface (i_connection) for all protocols
 * - Protocol selection via factory functions
 * - Cleaner callback setup
 * - Protocol-agnostic business logic
 *
 * @see legacy_client_example.cpp for comparison with old approach
 */

#include <kcenon/network/protocol/protocol.h>
#include <kcenon/network/unified/unified.h>

#include <cstddef>
#include <iostream>
#include <span>
#include <vector>

using namespace kcenon::network;

/**
 * @brief Protocol-agnostic connection handler
 *
 * This function works with ANY protocol - TCP, UDP, WebSocket, QUIC.
 * The unified interface enables truly protocol-agnostic code.
 */
void handle_connection(std::unique_ptr<unified::i_connection> conn) {
    // NEW: Set all callbacks at once using aggregate initialization
    conn->set_callbacks({
        .on_connected = []() {
            std::cout << "[Unified] Connected!\n";
        },
        .on_data = [](std::span<const std::byte> data) {
            std::cout << "[Unified] Received " << data.size() << " bytes\n";

            // Easy to work with modern C++ types
            std::vector<std::byte> buffer(data.begin(), data.end());
            // Process buffer...
        },
        .on_disconnected = []() {
            std::cout << "[Unified] Disconnected\n";
        },
        .on_error = [](std::error_code ec) {
            std::cerr << "[Unified] Error: " << ec.message() << "\n";
        }
    });

    // NEW: Connect using endpoint_info
    if (auto result = conn->connect({"localhost", 8080}); !result) {
        std::cerr << "[Unified] Failed to connect\n";
        return;
    }

    // NEW: Send using std::span<const std::byte>
    std::vector<std::byte> data = {
        std::byte{'H'}, std::byte{'e'}, std::byte{'l'},
        std::byte{'l'}, std::byte{'o'}
    };
    conn->send(data);
}

/**
 * @brief TCP client example using unified API
 */
void tcp_example() {
    std::cout << "\n=== TCP Client (Unified API) ===\n";

    // NEW: Create connection via protocol factory
    auto conn = protocol::tcp::connect({"localhost", 8080});
    handle_connection(std::move(conn));
}

/**
 * @brief UDP client example using unified API
 */
void udp_example() {
    std::cout << "\n=== UDP Client (Unified API) ===\n";

    // Same interface, different protocol factory
    auto conn = protocol::udp::connect({"localhost", 5555});
    handle_connection(std::move(conn));
}

/**
 * @brief WebSocket client example using unified API
 */
void websocket_example() {
    std::cout << "\n=== WebSocket Client (Unified API) ===\n";

    // WebSocket uses URL-based creation
    auto conn = protocol::websocket::connect("ws://localhost:8080/ws");

    // Can also use the same handle_connection function!
    handle_connection(std::move(conn));
}

/**
 * @brief Example of protocol selection at runtime
 */
std::unique_ptr<unified::i_connection> create_connection(
    const std::string& protocol,
    const unified::endpoint_info& endpoint) {

    if (protocol == "tcp") {
        return protocol::tcp::connect(endpoint);
    } else if (protocol == "udp") {
        return protocol::udp::connect(endpoint);
    } else if (protocol == "websocket") {
        // Convert endpoint to URL for WebSocket
        std::string url = "ws://" + std::string(endpoint.host) + ":" +
                          std::to_string(endpoint.port);
        return protocol::websocket::connect(url);
    }

    throw std::invalid_argument("Unknown protocol: " + protocol);
}

int main() {
    std::cout << "=== Unified Client Example ===\n";
    std::cout << "This file demonstrates the NEW unified API.\n\n";

    std::cout << "Benefits of unified API:\n";
    std::cout << "  1. Single i_connection interface for all protocols\n";
    std::cout << "  2. Protocol selection via factory functions\n";
    std::cout << "  3. Cleaner callback setup with aggregate initialization\n";
    std::cout << "  4. std::span<std::byte> for modern, efficient data handling\n";
    std::cout << "  5. Protocol-agnostic business logic\n";

    // Note: These examples create connections but won't actually connect
    // without a running server. They demonstrate the API patterns.

    std::cout << "\nCode patterns shown:\n";
    std::cout << "  - protocol::tcp::connect(endpoint)\n";
    std::cout << "  - protocol::udp::connect(endpoint)\n";
    std::cout << "  - protocol::websocket::connect(url)\n";
    std::cout << "  - conn->set_callbacks({...})\n";
    std::cout << "  - conn->connect(endpoint)\n";
    std::cout << "  - conn->send(data)\n";

    return 0;
}
