/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file unified_server_example.cpp
 * @brief Example showing the new unified server (listener) interface usage
 *
 * This file demonstrates the NEW, RECOMMENDED way of creating network servers
 * using the unified interface API.
 *
 * Benefits:
 * - Single interface (i_listener) for all protocols
 * - Protocol selection via factory functions
 * - Unified callback structure
 * - Accepted connections use same i_connection interface
 */

#include <kcenon/network/protocol/protocol.h>
#include <kcenon/network/unified/unified.h>

#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <string>

using namespace kcenon::network;

/**
 * @brief Protocol-agnostic server setup
 *
 * This function works with ANY protocol - TCP, UDP, WebSocket, QUIC.
 * The unified interface enables truly protocol-agnostic server code.
 */
void setup_server(std::unique_ptr<unified::i_listener> listener) {
    // NEW: Set all callbacks at once using aggregate initialization
    listener->set_callbacks({
        .on_accept = [](std::string_view conn_id) {
            std::cout << "[Server] New connection: " << conn_id << "\n";
        },
        .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
            std::cout << "[Server] Received " << data.size()
                      << " bytes from " << conn_id << "\n";

            // Process data...
        },
        .on_disconnect = [](std::string_view conn_id) {
            std::cout << "[Server] Disconnected: " << conn_id << "\n";
        },
        .on_error = [](std::string_view conn_id, std::error_code ec) {
            std::cerr << "[Server] Error on " << conn_id
                      << ": " << ec.message() << "\n";
        }
    });

    // NEW: Start listening
    if (auto result = listener->start(8080); !result) {
        std::cerr << "[Server] Failed to start\n";
        return;
    }

    std::cout << "[Server] Listening on port 8080\n";
}

/**
 * @brief TCP server example using unified API
 */
void tcp_server_example() {
    std::cout << "\n=== TCP Server (Unified API) ===\n";

    // NEW: Create listener via protocol factory
    auto listener = protocol::tcp::listen(8080);
    setup_server(std::move(listener));
}

/**
 * @brief UDP server example using unified API
 */
void udp_server_example() {
    std::cout << "\n=== UDP Server (Unified API) ===\n";

    // Same interface, different protocol factory
    auto listener = protocol::udp::listen(5555);
    setup_server(std::move(listener));
}

/**
 * @brief WebSocket server example using unified API
 */
void websocket_server_example() {
    std::cout << "\n=== WebSocket Server (Unified API) ===\n";

    // WebSocket listener
    auto listener = protocol::websocket::listen(8080, "/ws");
    setup_server(std::move(listener));
}

/**
 * @brief Advanced example: Managing accepted connections
 *
 * Shows how to get ownership of accepted connections for
 * custom management.
 */
class ConnectionManager {
public:
    void start(std::unique_ptr<unified::i_listener> listener) {
        listener_ = std::move(listener);

        // Use accept callback to get connection ownership
        listener_->set_accept_callback(
            [this](std::unique_ptr<unified::i_connection> conn) {
                handle_new_connection(std::move(conn));
            });

        listener_->start(8080);
    }

    void broadcast(std::span<const std::byte> data) {
        for (auto& [id, conn] : connections_) {
            conn->send(data);
        }
    }

private:
    void handle_new_connection(std::unique_ptr<unified::i_connection> conn) {
        // Get connection ID from endpoint info
        auto endpoint = conn->remote_endpoint();
        std::string id = std::string(endpoint.host) + ":" +
                         std::to_string(endpoint.port);

        std::cout << "Managing connection: " << id << "\n";

        // Set up connection-specific callbacks
        conn->set_callbacks({
            .on_data = [id](std::span<const std::byte> data) {
                std::cout << "Data from " << id << "\n";
            },
            .on_disconnected = [this, id]() {
                connections_.erase(id);
                std::cout << "Removed connection: " << id << "\n";
            }
        });

        // Store connection
        connections_[id] = std::move(conn);
    }

    std::unique_ptr<unified::i_listener> listener_;
    std::map<std::string, std::unique_ptr<unified::i_connection>> connections_;
};

/**
 * @brief Example of protocol selection at runtime
 */
std::unique_ptr<unified::i_listener> create_listener(
    const std::string& protocol,
    uint16_t port) {

    if (protocol == "tcp") {
        return protocol::tcp::listen(port);
    } else if (protocol == "udp") {
        return protocol::udp::listen(port);
    } else if (protocol == "websocket") {
        return protocol::websocket::listen(port, "/");
    }

    throw std::invalid_argument("Unknown protocol: " + protocol);
}

int main() {
    std::cout << "=== Unified Server Example ===\n";
    std::cout << "This file demonstrates the NEW unified API for servers.\n\n";

    std::cout << "Benefits of unified server API:\n";
    std::cout << "  1. Single i_listener interface for all protocols\n";
    std::cout << "  2. Accepted connections are i_connection (same as clients)\n";
    std::cout << "  3. Protocol selection via factory functions\n";
    std::cout << "  4. Cleaner callback setup with aggregate initialization\n";
    std::cout << "  5. Easy connection management with ownership\n";

    std::cout << "\nCode patterns shown:\n";
    std::cout << "  - protocol::tcp::listen(port)\n";
    std::cout << "  - protocol::udp::listen(port)\n";
    std::cout << "  - protocol::websocket::listen(port, path)\n";
    std::cout << "  - listener->set_callbacks({...})\n";
    std::cout << "  - listener->set_accept_callback(callback)\n";
    std::cout << "  - listener->start(port)\n";
    std::cout << "  - listener->broadcast(data)\n";

    return 0;
}
