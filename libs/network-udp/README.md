# network-udp

UDP protocol implementation library for network_system.

## Overview

The `network-udp` library provides a complete UDP protocol implementation with both connectionless datagram communication and reliable UDP variants. It implements the unified network interfaces from `network-core`.

## Features

- **Connectionless UDP**: Standard UDP datagram communication
- **Messaging Clients/Servers**: High-level messaging abstractions
- **Secure Communication**: DTLS support for secure UDP
- **Unified Interface**: Implements `i_connection` and `i_listener` from network-core
- **Reliable UDP**: Experimental reliable delivery on top of UDP
- **Async I/O**: ASIO-based asynchronous operations

## Components

### Core Socket
- `udp_socket`: Low-level UDP socket wrapper with async operations

### Messaging Layer
- `messaging_udp_client`: UDP client with message framing
- `messaging_udp_server`: UDP server with message framing
- `secure_messaging_udp_client`: DTLS-secured UDP client
- `secure_messaging_udp_server`: DTLS-secured UDP server

### Unified Adapters
- `udp_connection_adapter`: Adapts UDP client to `i_connection` interface
- `udp_listener_adapter`: Adapts UDP server to `i_listener` interface

### Experimental
- `reliable_udp_client`: Reliable delivery protocol on UDP (experimental)

## Usage

### Simple UDP Client

```cpp
#include "network_udp/protocol/udp.h"

auto conn = network_udp::protocol::udp::connect({"localhost", 5555});
conn->set_callbacks({
    .on_data = [](std::span<const std::byte> data) {
        // Handle received datagram
    }
});
conn->send(data);
```

### Simple UDP Server

```cpp
#include "network_udp/protocol/udp.h"

auto listener = network_udp::protocol::udp::listen(5555);
listener->set_callbacks({
    .on_accept = [](std::string_view conn_id) {
        std::cout << "New endpoint: " << conn_id << "\n";
    },
    .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
        // Handle received datagram from conn_id
    }
});
```

## Dependencies

- **ASIO**: Async I/O library (header-only)
- **OpenSSL**: For DTLS support
- **network-core**: Core interfaces and utilities

## Building

### Standalone Build

```bash
cd libs/network-udp
cmake -B build
cmake --build build
```

### As Part of network_system

The library is automatically built when building the main network_system project:

```bash
cmake -B build
cmake --build build
```

## UDP Characteristics

### Connectionless
- Each datagram is independent
- No connection setup or teardown
- Suitable for real-time, low-latency applications

### No Guaranteed Delivery
- Packets may be lost, duplicated, or reordered
- Applications must handle these scenarios
- Consider using the experimental `reliable_udp_client` for reliability

### Message Boundaries
- Each receive corresponds to one send operation
- No stream fragmentation like TCP

## Thread Safety

All public APIs are thread-safe. Callbacks are invoked on ASIO worker threads, so ensure your callback implementations are thread-safe if they share data.

## License

BSD 3-Clause License. See LICENSE file in the project root.
