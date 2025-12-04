# QUIC Protocol Support

## Overview

network_system provides native QUIC protocol support implementing RFC 9000, RFC 9001, and RFC 9002. The implementation follows the same design philosophy as other protocols in the library: direct RFC implementation with minimal external dependencies.

### Key Features

- **RFC 9000/9001/9002 Compliance**: Full implementation of QUIC transport, TLS 1.3 integration, and congestion control
- **TLS 1.3 Built-in Security**: Mandatory encryption with modern cryptographic standards
- **Stream Multiplexing**: Multiple concurrent streams over a single connection without head-of-line blocking
- **Connection Migration**: Maintain connections across network changes (NAT rebinding support)
- **0-RTT Resumption**: Fast reconnection with early data support
- **Consistent API**: Follows existing `messaging_*_client/server` patterns

## Quick Start

### Server

```cpp
#include "kcenon/network/core/messaging_quic_server.h"

using namespace network_system::core;

int main() {
    auto server = std::make_shared<messaging_quic_server>("my_server");

    // Configure callbacks
    server->set_connection_callback([](auto session) {
        std::cout << "Client connected: " << session->session_id() << "\n";
    });

    server->set_receive_callback([](auto session, const auto& data) {
        std::cout << "Received " << data.size() << " bytes\n";
    });

    // Start server
    if (auto result = server->start_server(4433); result.is_ok()) {
        std::cout << "Server started on port 4433\n";
        server->wait_for_stop();
    }

    return 0;
}
```

### Client

```cpp
#include "kcenon/network/core/messaging_quic_client.h"

using namespace network_system::core;

int main() {
    auto client = std::make_shared<messaging_quic_client>("my_client");

    // Configure callbacks
    client->set_connected_callback([]() {
        std::cout << "Connected to server\n";
    });

    client->set_receive_callback([](const auto& data) {
        std::cout << "Received: " << std::string(data.begin(), data.end()) << "\n";
    });

    // Configure client
    quic_client_config config;
    config.verify_server = false;  // For testing only

    // Connect
    if (auto result = client->start_client("127.0.0.1", 4433, config); result.is_ok()) {
        // Send data
        client->send_packet("Hello, QUIC!");

        // Create additional streams
        if (auto stream = client->create_stream(); stream.is_ok()) {
            client->send_on_stream(stream.value(), {'d', 'a', 't', 'a'});
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
        client->stop_client();
    }

    return 0;
}
```

## Documentation Index

| Document | Description |
|----------|-------------|
| [Architecture](ARCHITECTURE.md) | Implementation architecture and design decisions |
| [API Reference](API_REFERENCE.md) | Complete API documentation |
| [Configuration](CONFIGURATION.md) | Configuration options and tuning |
| [Examples](EXAMPLES.md) | Usage examples and patterns |

## Build Configuration

QUIC support is optional and must be enabled at build time:

```cmake
cmake -DBUILD_QUIC_SUPPORT=ON ..
```

### Dependencies

- OpenSSL 1.1.1+ (for TLS 1.3 cryptography)
- C++20 compatible compiler
- ASIO (header-only networking)

## Comparison with Other Protocols

| Feature | TCP | WebSocket | QUIC |
|---------|-----|-----------|------|
| Transport | TCP | TCP | UDP |
| Encryption | Optional (TLS) | Optional (WSS) | Required (TLS 1.3) |
| Multiplexing | No | No | Yes (Streams) |
| Head-of-line Blocking | Yes | Yes | No |
| Connection Migration | No | No | Yes |
| 0-RTT | No | No | Yes |

## Performance Characteristics

- **Latency**: Lower connection establishment time (1-RTT or 0-RTT)
- **Throughput**: Comparable to TCP with better behavior under packet loss
- **Concurrency**: Multiple streams without blocking each other
- **Recovery**: Fast loss recovery with RACK-TLP algorithm

## Related Issues

- [#245](https://github.com/kcenon/network_system/issues/245) - Epic: QUIC Protocol Support
- [#256](https://github.com/kcenon/network_system/issues/256) - Phase 4.3: Integration Tests and Documentation
