/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file legacy_client_example.cpp
 * @brief Example showing legacy (deprecated) client interface usage
 *
 * This file demonstrates the OLD way of creating network clients.
 * It is provided for reference only - new code should use the unified API.
 *
 * @see unified_client_example.cpp for the new, recommended approach
 */

// Note: These includes are for reference only
// #include <kcenon/network/interfaces/i_client.h>
// #include <kcenon/network/interfaces/i_tcp_client.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

// This example shows the LEGACY patterns (deprecated)
// Do NOT use this in new code

namespace legacy_example {

/**
 * LEGACY PATTERN: Protocol-specific interface usage
 *
 * Problems with this approach:
 * 1. Must learn different interfaces for each protocol
 * 2. Code is tightly coupled to specific protocol
 * 3. Changing protocols requires significant refactoring
 * 4. More interfaces to test and maintain
 */

// Simulated legacy interface (for documentation purposes)
class i_client {
public:
    virtual ~i_client() = default;

    using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
    using connected_callback_t = std::function<void()>;
    using disconnected_callback_t = std::function<void()>;
    using error_callback_t = std::function<void(std::error_code)>;

    virtual bool start(std::string_view host, uint16_t port) = 0;
    virtual bool stop() = 0;
    virtual bool send(std::vector<uint8_t>&& data) = 0;
    virtual bool is_connected() const = 0;

    virtual void set_receive_callback(receive_callback_t callback) = 0;
    virtual void set_connected_callback(connected_callback_t callback) = 0;
    virtual void set_disconnected_callback(disconnected_callback_t callback) = 0;
    virtual void set_error_callback(error_callback_t callback) = 0;
};

/**
 * @brief Legacy client setup example
 *
 * This demonstrates the OLD way of setting up a client:
 * - Individual callback setters
 * - Protocol-specific interface
 * - std::vector<uint8_t> for data
 */
void setup_legacy_client(std::unique_ptr<i_client> client) {
    // OLD: Set callbacks individually (verbose)
    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        std::cout << "[Legacy] Received " << data.size() << " bytes\n";
    });

    client->set_connected_callback([]() {
        std::cout << "[Legacy] Connected!\n";
    });

    client->set_disconnected_callback([]() {
        std::cout << "[Legacy] Disconnected\n";
    });

    client->set_error_callback([](std::error_code ec) {
        std::cerr << "[Legacy] Error: " << ec.message() << "\n";
    });

    // OLD: Start with separate host and port
    if (!client->start("localhost", 8080)) {
        std::cerr << "[Legacy] Failed to connect\n";
        return;
    }

    // OLD: Send using std::vector<uint8_t>
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    client->send(std::move(data));
}

}  // namespace legacy_example

int main() {
    std::cout << "=== Legacy Client Example ===\n";
    std::cout << "This file demonstrates the DEPRECATED legacy API.\n";
    std::cout << "See unified_client_example.cpp for the new approach.\n\n";

    std::cout << "Legacy issues:\n";
    std::cout << "  1. Multiple protocol-specific interfaces to learn\n";
    std::cout << "  2. Verbose individual callback setters\n";
    std::cout << "  3. Protocol-coupled code\n";
    std::cout << "  4. std::vector<uint8_t> instead of std::span<std::byte>\n";

    return 0;
}
