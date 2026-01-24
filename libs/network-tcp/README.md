# network-tcp

TCP protocol implementation for the network_system modularization effort.

## Overview

`network-tcp` provides a complete TCP implementation including plain and SSL/TLS support, implementing the unified network interfaces defined in the main project.

## Purpose

Part of issue #554 (Extract TCP protocol into standalone library), this library:

- Implements TCP protocol with async I/O using ASIO
- Provides both plain TCP and secure (SSL/TLS) socket implementations
- Implements unified network interfaces (i_connection, i_listener)
- Enables independent TCP functionality

## Key Components

### 1. Core TCP Sockets

Located in `include/network_tcp/internal/`:

- **`tcp_socket.h/cpp`**: Lightweight wrapper around ASIO TCP socket with async read/write
- **`secure_tcp_socket.h/cpp`**: SSL/TLS-enabled TCP socket using ASIO SSL support

### 2. Unified Interface Adapters

Located in `include/network_tcp/unified/adapters/`:

- **`tcp_connection_adapter.h/cpp`**: Implements i_connection for TCP clients
- **`tcp_listener_adapter.h/cpp`**: Implements i_listener for TCP servers

### 3. Protocol Factory Functions

Located in `include/network_tcp/protocol/`:

- **`tcp.h/cpp`**: Factory functions for creating TCP connections and listeners

## Usage

### As Part of network_system

When used within the main `network_system` project:

```cmake
target_link_libraries(your_target PRIVATE kcenon::network-tcp)
```

### Basic TCP Client

```cpp
#include "network_tcp/protocol/tcp.h"

// Create and connect
auto conn = protocol::tcp::connect({"localhost", 8080});

// Set callbacks
conn->set_callbacks({
    .on_connected = []() {
        std::cout << "Connected!\n";
    },
    .on_data = [](std::span<const std::byte> data) {
        // Process received data
    },
    .on_disconnected = []() {
        std::cout << "Disconnected\n";
    }
});

// Send data
std::vector<std::byte> message = /* ... */;
conn->send(message);
```

### Basic TCP Server

```cpp
#include "network_tcp/protocol/tcp.h"

// Create and start listener
auto listener = protocol::tcp::listen(8080);

// Set callbacks
listener->set_callbacks({
    .on_accept = [](std::string_view conn_id) {
        std::cout << "New connection: " << conn_id << "\n";
    },
    .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
        // Process data from client
    }
});

// Listener is now accepting connections
```

### SSL/TLS Support

SSL/TLS is handled transparently by the secure_tcp_socket implementation. Configuration is done through the unified_messaging_client/server interfaces from the main project.

## Dependencies

### Required
- **C++20** compiler
- **ASIO** (header-only library for async I/O)
- **OpenSSL** (for SSL/TLS support)
- **Main project** (for unified interfaces and messaging client/server)

### Optional
- **network-core** (when built as part of modularized system)

## Architecture

```
network-tcp
├── Protocol Layer (tcp.h)
│   └── Factory functions for connections and listeners
├── Adapter Layer (tcp_connection_adapter, tcp_listener_adapter)
│   └── Implements unified interfaces (i_connection, i_listener)
└── Socket Layer (tcp_socket, secure_tcp_socket)
    └── ASIO socket wrappers with async I/O
```

### Dependencies Graph

```
network-tcp
├── ASIO (async I/O)
├── OpenSSL (SSL/TLS)
└── Main project
    ├── Unified interfaces (i_connection, i_listener, types)
    ├── Core components (unified_messaging_client, unified_messaging_server)
    └── Common definitions (common_defs.h)
```

## Features

### Thread Safety
- All public methods are thread-safe
- ASIO operations serialized through io_context
- Callbacks invoked on ASIO worker threads

### Async I/O
- Non-blocking operations using ASIO
- Efficient event-driven architecture
- Automatic reconnection support (via adapters)

### Security
- SSL/TLS support via OpenSSL
- Certificate validation
- Configurable cipher suites

### Backpressure Management
- Automatic flow control
- Configurable buffer sizes
- High/low water mark notifications

## Building

### Standalone Build

```bash
cd libs/network-tcp
cmake -B build
cmake --build build
```

### As Part of network_system

The library is automatically built when building the main project.

## Development Status

- **Current Phase**: Initial extraction from monolithic `network_system`
- **Part of**: Issue #554 (Extract TCP protocol library)
- **Related Issues**:
  - Issue #538: EPIC - Modularize network_system
  - Issue #546: Extract network-core library

## License

BSD 3-Clause License. See LICENSE file in the root directory.

## Contributing

This library is part of the `network_system` modularization effort. For contribution guidelines, see the main repository README.
