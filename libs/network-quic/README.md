# network-quic

QUIC protocol implementation for the network_system library.

## Overview

`network-quic` provides a complete QUIC protocol implementation (RFC 9000) including:

- QUIC connection management
- TLS 1.3 integration (RFC 9001)
- Stream multiplexing
- Flow control and congestion control
- Connection migration support
- 0-RTT early data (optional)
- Path MTU Discovery

## Features

- **RFC 9000 Compliant**: Full implementation of the QUIC protocol specification
- **TLS 1.3 Built-in**: Secure connections with mandatory encryption
- **Multiplexing**: Multiple concurrent streams over single connection
- **Flow Control**: Per-stream and connection-level flow control
- **Congestion Control**: Built-in congestion control mechanisms
- **Both Client and Server**: Full support for both QUIC client and server

## Dependencies

- **network-core**: For interfaces and result types
- **network-udp**: For underlying UDP transport
- **OpenSSL**: For TLS 1.3 support

## Usage

### As Part of network_system

When building as part of the main network_system project, the library is automatically integrated:

```cmake
# In your CMakeLists.txt
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::network-quic)
```

### Standalone Build

```cmake
# In your CMakeLists.txt
find_package(network-quic REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::network-quic)
```

### Example Code

#### Client Usage

```cpp
#include <network_quic/quic.h>

// Create QUIC client with configuration
quic_config cfg;
cfg.server_name = "example.com";
cfg.alpn_protocols = {"h3"};

auto client = protocol::quic::create_connection(cfg, "my-quic-client");

// Set callbacks
client->set_callbacks({
    .on_connected = []() { std::cout << "Connected!\n"; },
    .on_data = [](std::span<const std::byte> data) {
        // Handle received data
    },
    .on_error = [](const error_info& err) {
        std::cerr << "Error: " << err.message << "\n";
    }
});

// Connect to server
client->connect({"example.com", 443});

// Send data
std::vector<std::byte> data = /* ... */;
client->send(data);

// Disconnect
client->disconnect();
```

#### Server Usage

```cpp
#include <network_quic/quic.h>

// Create QUIC server with TLS configuration
quic_config cfg;
cfg.cert_file = "/path/to/cert.pem";
cfg.key_file = "/path/to/key.pem";
cfg.alpn_protocols = {"h3"};

auto server = protocol::quic::create_listener(cfg, "my-quic-server");

// Set callbacks
server->set_callbacks({
    .on_accept = [](std::string_view conn_id) {
        std::cout << "New connection: " << conn_id << "\n";
    },
    .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
        // Handle received data from conn_id
    },
    .on_disconnect = [](std::string_view conn_id) {
        std::cout << "Disconnected: " << conn_id << "\n";
    }
});

// Start listening on port 443
server->start(443);

// Wait for shutdown signal
server->wait();

// Stop server
server->stop();
```

## Library Structure

```
network-quic/
├── include/
│   └── network_quic/
│       ├── quic.h                    # Main public header
│       ├── quic_client.h             # Client implementation
│       ├── quic_server.h             # Server implementation
│       ├── quic_connection.h         # Connection management
│       └── internal/
│           ├── stream.h              # QUIC stream management
│           └── crypto.h              # QUIC crypto utilities
├── src/
│   ├── quic_client.cpp
│   ├── quic_server.cpp
│   └── quic_connection.cpp
├── tests/
│   └── (test files)
├── CMakeLists.txt
├── network-quic-config.cmake.in
└── README.md
```

## API Overview

### Factory Functions

- `create_connection()` - Create QUIC client connection
- `connect()` - Create and connect QUIC client
- `create_listener()` - Create QUIC server listener
- `listen()` - Create and start QUIC server

### quic_config

Configuration options for QUIC connections:

- `server_name` - Server name for TLS SNI
- `cert_file` - Path to certificate file (server)
- `key_file` - Path to private key file (server)
- `alpn_protocols` - ALPN protocols (e.g., "h3")
- `idle_timeout` - Maximum idle timeout
- `max_bidi_streams` - Maximum bidirectional streams
- `max_uni_streams` - Maximum unidirectional streams
- `initial_max_data` - Connection-level flow control
- `enable_early_data` - Enable 0-RTT
- `enable_pmtud` - Enable Path MTU Discovery

### Connection Interface

Following the unified `i_connection` interface:

- `connect()` - Initiate QUIC handshake
- `disconnect()` - Close connection
- `send()` - Send data on default stream
- `is_connected()` - Check connection status
- `set_callbacks()` - Set event callbacks

### Listener Interface

Following the unified `i_listener` interface:

- `start()` - Start accepting connections
- `stop()` - Stop listener
- `send()` - Send data to specific connection
- `broadcast()` - Send to all connections
- `set_callbacks()` - Set event callbacks

## License

BSD 3-Clause License. See LICENSE file for details.
