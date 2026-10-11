---
doc_id: "NET-GUID-018"
doc_title: "Quick Start Guide"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# Quick Start Guide

[English / 한국어](QUICK_START.kr.md)

These examples target current main installed from source. Follow the [build guide](BUILD.md) first.
Use the CMakeLists.txt in the [README](../../README.md), saving each program as main.cpp in a separate consumer directory.
The published v0.1.1 does not necessarily contain every current API.

## Core server

Starts a server on port 8080; press Enter to stop.

```cpp
#include <kcenon/network/network_system.h>
#include <iostream>
#include <memory>

int main() {
    auto server = std::make_shared<kcenon::network::core::messaging_server>("MyServer");

    auto result = server->start_server(8080);
    if (result.is_err()) {
        const auto& err = result.error();
        std::cerr << "Failed to start: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
        return -1;
    }

    std::cout << "Server running on port 8080; press Enter to stop." << std::endl;
    std::cin.get();
    auto stopped = server->stop_server();
    return stopped.is_err() ? 1 : 0;
}
```

## Client

Start the receiving server below first. Connecting is asynchronous: this client waits up to five seconds for readiness before sending, and stops on Enter.

```cpp
#include <kcenon/network/network_system.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

int main() {
    auto client = std::make_shared<kcenon::network::core::messaging_client>("MyClient");

    auto result = client->start_client("127.0.0.1", 8080);
    if (result.is_err()) {
        const auto& err = result.error();
        std::cerr << "Failed to connect: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
        return -1;
    }

    // start_client initiates an asynchronous connection. Wait before sending.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->is_connected() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!client->is_connected()) {
        std::cerr << "Connection timed out\n";
        (void)client->stop_client();
        return 1;
    }

    // Send message (std::move avoids extra copies)
    std::string message = "Hello, Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());

    auto send_result = client->send_packet(std::move(data));
    if (send_result.is_err()) {
        const auto& err = send_result.error();
        std::cerr << "Failed to send: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
    }

    std::cout << "Press Enter to stop the client." << std::endl;
    std::cin.get();
    auto stopped = client->stop_client();
    return send_result.is_err() || stopped.is_err() ? 1 : 0;
}
```

## Receiving facade server

Run this instead of the core server to print the number of received bytes. Both servers use port 8080; run only one at a time.

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

int main() {
    kcenon::network::facade::tcp_facade tcp;
    auto created = tcp.create_server({.port = 8080});
    if (created.is_err()) {
        std::cerr << created.error().message << '\n';
        return 1;
    }
    auto server = created.value();
    server->set_receive_callback([](auto session_id, const auto& data) {
        std::cout << "Received " << data.size() << " bytes from "
                  << session_id << std::endl;
    });
    auto started = server->start(8080);
    if (started.is_err()) {
        std::cerr << started.error().message << '\n';
        return 1;
    }
    std::cout << "Listening on port 8080; press Enter to stop." << std::endl;
    std::cin.get();
    auto stopped = server->stop();
    return stopped.is_err() ? 1 : 0;
}
```

[Protocol, tracing, and thread-pool examples](USAGE_EXAMPLES.md) | [Troubleshooting](TROUBLESHOOTING.md)
