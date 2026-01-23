# Migration Guide: Unified Interface API

This guide helps you migrate from the legacy protocol-specific interfaces (12+ interfaces) to the new unified interface API (3 core interfaces).

## Overview

### Interface Consolidation

The network_system library has evolved from having many protocol-specific interfaces to a cleaner, unified design:

| Count | Old Design | New Design |
|-------|------------|------------|
| **Client interfaces** | `i_client`, `i_tcp_client`, `i_udp_client`, `i_websocket_client`, `i_quic_client` | `i_connection` |
| **Server interfaces** | `i_server`, `i_tcp_server`, `i_udp_server`, `i_websocket_server`, `i_quic_server` | `i_listener` |
| **Session interface** | `i_session` | `i_connection` (accepted) |
| **Base interface** | `i_network_component` | `i_transport` |

**Result**: 12+ interfaces → 3 core interfaces

### Why This Change?

1. **Reduced cognitive load**: Learn 3 interfaces instead of 12+
2. **Protocol-agnostic code**: Write code that works with any protocol
3. **Simplified testing**: Mock fewer interfaces
4. **Better discoverability**: Clear, minimal API surface

## Quick Migration Reference

### Include Changes

```cpp
// Before
#include <kcenon/network/interfaces/interfaces.h>
#include <kcenon/network/interfaces/i_tcp_client.h>

// After
#include <kcenon/network/unified/unified.h>
#include <kcenon/network/protocol/protocol.h>
```

### Namespace Changes

```cpp
// Before
using namespace kcenon::network::interfaces;

// After
using namespace kcenon::network::unified;
using namespace kcenon::network::protocol;
```

### Type Mappings

| Legacy Type | Unified Type | Notes |
|-------------|--------------|-------|
| `i_client*` | `i_connection*` | Use protocol factory |
| `i_tcp_client*` | `i_connection*` | Via `protocol::tcp::connect()` |
| `i_udp_client*` | `i_connection*` | Via `protocol::udp::connect()` |
| `i_websocket_client*` | `i_connection*` | Via `protocol::websocket::connect()` |
| `i_server*` | `i_listener*` | Use protocol factory |
| `i_tcp_server*` | `i_listener*` | Via `protocol::tcp::listen()` |
| `i_udp_server*` | `i_listener*` | Via `protocol::udp::listen()` |
| `i_websocket_server*` | `i_listener*` | Via `protocol::websocket::listen()` |
| `i_session*` | `i_connection*` | Accepted connections |

## Step-by-Step Migration

### Step 1: Client Migration

#### Before (Legacy)

```cpp
#include <kcenon/network/interfaces/i_tcp_client.h>

void setup_client() {
    // Create client (implementation-specific)
    auto client = create_tcp_client();  // Returns i_tcp_client*

    // Set individual callbacks
    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        std::cout << "Received " << data.size() << " bytes\n";
    });

    client->set_connected_callback([]() {
        std::cout << "Connected!\n";
    });

    client->set_disconnected_callback([]() {
        std::cout << "Disconnected\n";
    });

    client->set_error_callback([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << "\n";
    });

    // Connect
    if (auto result = client->start("localhost", 8080); !result) {
        std::cerr << "Failed to connect\n";
        return;
    }

    // Send data
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    client->send(std::move(data));
}
```

#### After (Unified)

```cpp
#include <kcenon/network/protocol/protocol.h>

void setup_client() {
    using namespace kcenon::network;

    // Create connection via protocol factory
    auto conn = protocol::tcp::connect({"localhost", 8080});

    // Set all callbacks at once (cleaner API)
    conn->set_callbacks({
        .on_connected = []() {
            std::cout << "Connected!\n";
        },
        .on_data = [](std::span<const std::byte> data) {
            std::cout << "Received " << data.size() << " bytes\n";
        },
        .on_disconnected = []() {
            std::cout << "Disconnected\n";
        },
        .on_error = [](std::error_code ec) {
            std::cerr << "Error: " << ec.message() << "\n";
        }
    });

    // Connect (endpoint already specified in factory)
    // Or connect to a different endpoint:
    if (auto result = conn->connect({"remote.host.com", 9000}); !result) {
        std::cerr << "Failed to connect\n";
        return;
    }

    // Send data (uses std::span<const std::byte>)
    std::vector<std::byte> data = {
        std::byte{'H'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}
    };
    conn->send(data);
}
```

### Step 2: Server Migration

#### Before (Legacy)

```cpp
#include <kcenon/network/interfaces/i_tcp_server.h>

void setup_server() {
    auto server = create_tcp_server();  // Returns i_tcp_server*

    server->set_connection_callback([](std::shared_ptr<i_session> session) {
        std::cout << "New connection: " << session->id() << "\n";
    });

    server->set_receive_callback(
        [](std::string_view session_id, const std::vector<uint8_t>& data) {
            std::cout << "Data from " << session_id << "\n";
        });

    server->set_disconnection_callback([](std::string_view session_id) {
        std::cout << "Disconnected: " << session_id << "\n";
    });

    server->set_error_callback(
        [](std::string_view session_id, std::error_code ec) {
            std::cerr << "Error on " << session_id << ": " << ec.message() << "\n";
        });

    if (auto result = server->start(8080); !result) {
        std::cerr << "Failed to start server\n";
        return;
    }
}
```

#### After (Unified)

```cpp
#include <kcenon/network/protocol/protocol.h>

void setup_server() {
    using namespace kcenon::network;

    // Create listener via protocol factory
    auto listener = protocol::tcp::listen(8080);

    // Set all callbacks at once
    listener->set_callbacks({
        .on_accept = [](std::string_view conn_id) {
            std::cout << "New connection: " << conn_id << "\n";
        },
        .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
            std::cout << "Data from " << conn_id << "\n";
        },
        .on_disconnect = [](std::string_view conn_id) {
            std::cout << "Disconnected: " << conn_id << "\n";
        },
        .on_error = [](std::string_view conn_id, std::error_code ec) {
            std::cerr << "Error on " << conn_id << ": " << ec.message() << "\n";
        }
    });

    // Start (already bound in factory, or specify endpoint)
    if (auto result = listener->start({"::", 8080}); !result) {
        std::cerr << "Failed to start listener\n";
        return;
    }
}
```

### Step 3: Protocol-Agnostic Code

The unified API enables writing protocol-agnostic code:

```cpp
#include <kcenon/network/unified/unified.h>

// Works with TCP, UDP, WebSocket, QUIC - any protocol!
void process_connection(std::unique_ptr<unified::i_connection> conn) {
    conn->set_callbacks({
        .on_data = [](std::span<const std::byte> data) {
            // Process data regardless of protocol
        }
    });

    // Send works the same for all protocols
    std::vector<std::byte> response = prepare_response();
    conn->send(response);
}

// Accept connections from any protocol listener
void run_server(std::unique_ptr<unified::i_listener> listener) {
    listener->set_accept_callback([](std::unique_ptr<unified::i_connection> conn) {
        process_connection(std::move(conn));
    });

    listener->start(8080);
}
```

## Callback Migration

### Data Callback Changes

```cpp
// Before: std::vector<uint8_t>
void on_data(const std::vector<uint8_t>& data);

// After: std::span<const std::byte>
void on_data(std::span<const std::byte> data);

// Conversion if needed:
std::vector<uint8_t> vec(data.size());
std::memcpy(vec.data(), data.data(), data.size());
```

### Callback Structure

```cpp
// Before: Individual setters
client->set_receive_callback(...);
client->set_connected_callback(...);
client->set_disconnected_callback(...);
client->set_error_callback(...);

// After: Single callback structure
conn->set_callbacks({
    .on_connected = ...,
    .on_data = ...,
    .on_disconnected = ...,
    .on_error = ...
});
```

## Protocol Factory Usage

### TCP

```cpp
using namespace kcenon::network::protocol;

// Client
auto client = tcp::connect({"localhost", 8080});
auto secure_client = tcp::connect_secure({"localhost", 443}, tls_config);

// Server
auto server = tcp::listen(8080);
auto secure_server = tcp::listen_secure(8080, tls_config);
```

### UDP

```cpp
using namespace kcenon::network::protocol;

// Client (connected mode)
auto client = udp::connect({"localhost", 5555});

// Server
auto server = udp::listen(5555);
```

### WebSocket

```cpp
using namespace kcenon::network::protocol;

// Client (URL-based)
auto client = websocket::connect("ws://localhost:8080/ws");
auto secure_client = websocket::connect("wss://example.com/ws");

// Server
auto server = websocket::listen(8080, "/ws");
```

## Compatibility Header

For gradual migration, use the compatibility header:

```cpp
#include <kcenon/network/compat/legacy_aliases.h>

using namespace kcenon::network::compat;

// These aliases compile but produce deprecation warnings
i_client_compat* client;      // Warning: use unified::i_connection
i_server_compat* server;      // Warning: use unified::i_listener
i_session_compat* session;    // Warning: use unified::i_connection
```

## Common Migration Patterns

### Pattern 1: Factory Function Migration

```cpp
// Before: Implementation-specific factory
std::unique_ptr<i_tcp_client> create_tcp_client();

// After: Protocol factory
auto conn = protocol::tcp::connect(endpoint);
```

### Pattern 2: Interface Parameter Migration

```cpp
// Before: Protocol-specific interface
void process(i_tcp_client* client);

// After: Unified interface
void process(i_connection* conn);
// Or better:
void process(std::unique_ptr<i_connection> conn);
```

### Pattern 3: Connection Storage Migration

```cpp
// Before: Storing protocol-specific pointers
std::map<std::string, std::shared_ptr<i_session>> sessions_;

// After: Unified connection storage
std::map<std::string, std::unique_ptr<i_connection>> connections_;
```

## Breaking Changes

1. **Callback signatures**: Use `std::span<const std::byte>` instead of `std::vector<uint8_t>`
2. **Ownership**: `std::unique_ptr` instead of `std::shared_ptr` for connections
3. **Factory functions**: Protocol-specific creation via namespace factories
4. **Callback setup**: Single `set_callbacks()` instead of individual setters

## Timeline

- **Current release**: Unified API available, legacy API deprecated
- **Next major release**: Legacy interfaces may be removed
- **Recommendation**: Migrate new code immediately, refactor existing code gradually

## Getting Help

- Check the [API Reference](../API_REFERENCE.md) for unified interface details
- See [examples/unified/](../../examples/unified/) for complete examples
- Report issues at the project repository
