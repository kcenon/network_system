# Unified API Guide

> **Language:** **English**

A comprehensive guide to the Unified API interface layer, which provides protocol-agnostic abstractions for connections, listeners, and transports in the `network_system` library.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Foundation Types](#foundation-types)
- [i\_transport Interface](#i_transport-interface)
- [i\_connection Interface](#i_connection-interface)
- [i\_listener Interface](#i_listener-interface)
- [Protocol-Agnostic Code Examples](#protocol-agnostic-code-examples)
- [Protocol Support Matrix](#protocol-support-matrix)
- [Extending with Custom Protocols](#extending-with-custom-protocols)

## Architecture Overview

### Purpose

The Unified API layer enables writing **protocol-agnostic networking code**. Through three core interfaces (`i_transport`, `i_connection`, `i_listener`), the same application code can work across TCP, UDP, WebSocket, QUIC, and other protocols without modification.

### Interface Hierarchy

```
                        ┌──────────────┐
                        │  i_transport  │  ← Data transfer abstraction
                        │  (base)       │     send(), is_connected(), id()
                        └──────┬───────┘
                               │ inherits
                        ┌──────┴───────┐
                        │ i_connection  │  ← Connection lifecycle
                        │ (active)      │     connect(), close(), set_callbacks()
                        └──────────────┘

                        ┌──────────────┐
                        │  i_listener   │  ← Server-side listener (standalone)
                        │  (passive)    │     start(), stop(), broadcast()
                        └──────────────┘
```

### Relationship Between Interfaces

| Interface | Role | When to Use |
|-----------|------|-------------|
| `i_transport` | Minimal send/receive contract | Code that only needs to send/receive data |
| `i_connection` | Full connection lifecycle | Code that manages connection state (connect, close) |
| `i_listener` | Server-side connection acceptance | Code that accepts incoming connections |

### How It Fits in the Architecture

```
┌──────────────────────────────────────────────────────┐
│                   User Application                    │
├──────────────────────────────────────────────────────┤
│     Facade Layer (tcp_facade, udp_facade, ...)        │  ← Simplified creation
├──────────────────────────────────────────────────────┤
│     Protocol Interfaces (i_protocol_client/server)    │  ← Protocol callbacks
├──────────────────────────────────────────────────────┤
│     Unified API (i_transport, i_connection,           │  ← Protocol-agnostic
│                   i_listener)                         │     abstractions
├──────────────────────────────────────────────────────┤
│     Protocol Implementations (TCP, UDP, WS, QUIC)     │  ← Concrete protocols
└──────────────────────────────────────────────────────┘
```

The Unified API sits below the Facade layer and above concrete protocol implementations, providing the **protocol abstraction boundary** that enables write-once networking code.

### Header Files

```cpp
#include <kcenon/network/detail/unified/types.h>         // Foundation types
#include <kcenon/network/detail/unified/i_transport.h>    // Base transport
#include <kcenon/network/detail/unified/i_connection.h>   // Connection lifecycle
#include <kcenon/network/detail/unified/i_listener.h>     // Server-side listener
```

All Unified API types reside in the `kcenon::network::unified` namespace.

---

## Foundation Types

**Header:** `kcenon/network/detail/unified/types.h`

The foundation types provide the building blocks used across all unified interfaces.

### endpoint_info

Represents a network endpoint as either a host:port combination or a full URL.

```cpp
struct endpoint_info {
    std::string host;    // Hostname, IP address, or full URL
    uint16_t port = 0;   // Port number (0 if embedded in URL)

    endpoint_info() = default;
    endpoint_info(const std::string& h, uint16_t p);      // Host/port
    endpoint_info(const char* h, uint16_t p);              // Host/port (C-string)
    explicit endpoint_info(const std::string& url);         // URL-only
    explicit endpoint_info(const char* url);                // URL-only (C-string)

    [[nodiscard]] auto is_valid() const noexcept -> bool;   // Non-empty host
    [[nodiscard]] auto to_string() const -> std::string;    // "host:port" or URL
    auto operator==(const endpoint_info&) const noexcept -> bool;
    auto operator!=(const endpoint_info&) const noexcept -> bool;
};
```

**Usage:**

```cpp
using namespace kcenon::network::unified;

// Socket-style endpoints
endpoint_info tcp_ep{"192.168.1.1", 8080};
endpoint_info local_ep{"localhost", 3000};

// URL-style endpoints (WebSocket, HTTP)
endpoint_info ws_ep{"wss://example.com/ws"};

// Validation
if (tcp_ep.is_valid()) {
    std::cout << tcp_ep.to_string() << std::endl; // "192.168.1.1:8080"
}
```

### connection_callbacks

Groups all callback functions for client-side connection events.

| Callback | Signature | Description |
|----------|-----------|-------------|
| `on_connected` | `void()` | Connection established |
| `on_data` | `void(std::span<const std::byte>)` | Data received (raw bytes) |
| `on_disconnected` | `void()` | Connection closed |
| `on_error` | `void(std::error_code)` | Error occurred |

**Usage:**

```cpp
connection_callbacks cbs;
cbs.on_connected = []() {
    std::cout << "Connected!" << std::endl;
};
cbs.on_data = [](std::span<const std::byte> data) {
    // Process received data
    std::cout << "Received " << data.size() << " bytes" << std::endl;
};
cbs.on_disconnected = []() {
    std::cout << "Disconnected!" << std::endl;
};
cbs.on_error = [](std::error_code ec) {
    std::cerr << "Error: " << ec.message() << std::endl;
};
```

### listener_callbacks

Groups all callback functions for server-side listener events.

| Callback | Signature | Description |
|----------|-----------|-------------|
| `on_accept` | `void(std::string_view)` | New connection accepted (connection ID) |
| `on_data` | `void(std::string_view, std::span<const std::byte>)` | Data from connection (conn ID, data) |
| `on_disconnect` | `void(std::string_view)` | Connection closed (connection ID) |
| `on_error` | `void(std::string_view, std::error_code)` | Error on connection (conn ID, error) |

### connection_options

Configuration parameters for fine-tuning connection behavior.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `connect_timeout` | `std::chrono::milliseconds` | `0` (none) | Connection timeout duration |
| `read_timeout` | `std::chrono::milliseconds` | `0` (none) | Read operation timeout |
| `write_timeout` | `std::chrono::milliseconds` | `0` (none) | Write operation timeout |
| `keep_alive` | `bool` | `false` | Enable TCP keep-alive |
| `no_delay` | `bool` | `false` | Disable Nagle's algorithm |

---

## i_transport Interface

**Header:** `kcenon/network/detail/unified/i_transport.h`

The `i_transport` interface is the **base abstraction for data transfer**. It provides the minimal contract for sending data and querying connection state, without any lifecycle management (connect/close). This allows code that only needs to transfer data to work with the simplest possible interface.

### Method Reference

| Method | Signature | Description |
|--------|-----------|-------------|
| `send` | `send(std::span<const std::byte>) -> VoidResult` | Send raw byte data |
| `send` | `send(std::vector<uint8_t>&&) -> VoidResult` | Send from uint8_t vector (convenience) |
| `is_connected` | `is_connected() const noexcept -> bool` | Check connection status |
| `id` | `id() const noexcept -> std::string_view` | Get unique transport ID |
| `remote_endpoint` | `remote_endpoint() const noexcept -> endpoint_info` | Get remote peer info |
| `local_endpoint` | `local_endpoint() const noexcept -> endpoint_info` | Get local binding info |

### Design Notes

- **Non-copyable, movable**: Interfaces are used via `unique_ptr`/`shared_ptr`
- **Thread-safe**: All methods use appropriate synchronization
- **Protocol-agnostic**: The same `send()` call works for TCP streams, UDP datagrams, WebSocket frames, and QUIC streams

### Example: Protocol-Agnostic Send Function

```cpp
#include <kcenon/network/detail/unified/i_transport.h>
#include <iostream>
#include <span>

using namespace kcenon::network::unified;

// This function works with ANY protocol
void send_message(i_transport& transport, std::span<const std::byte> data)
{
    if (!transport.is_connected())
    {
        std::cerr << "Transport " << transport.id() << " not connected"
                  << std::endl;
        return;
    }

    if (auto result = transport.send(data); !result)
    {
        std::cerr << "Send failed on " << transport.id() << std::endl;
    }
}

// Log transport info
void log_transport_info(const i_transport& transport)
{
    std::cout << "Transport: " << transport.id() << std::endl;
    std::cout << "  Remote: " << transport.remote_endpoint().to_string() << std::endl;
    std::cout << "  Local:  " << transport.local_endpoint().to_string() << std::endl;
    std::cout << "  Status: " << (transport.is_connected() ? "connected" : "disconnected")
              << std::endl;
}
```

---

## i_connection Interface

**Header:** `kcenon/network/detail/unified/i_connection.h`

The `i_connection` interface extends `i_transport` with **connection lifecycle management**. It represents both client-initiated connections and server-accepted connections.

### Method Reference

#### Inherited from i_transport

| Method | Signature | Description |
|--------|-----------|-------------|
| `send` | `send(std::span<const std::byte>) -> VoidResult` | Send raw byte data |
| `send` | `send(std::vector<uint8_t>&&) -> VoidResult` | Send from uint8_t vector |
| `is_connected` | `is_connected() const noexcept -> bool` | Check connection status |
| `id` | `id() const noexcept -> std::string_view` | Get unique connection ID |
| `remote_endpoint` | `remote_endpoint() const noexcept -> endpoint_info` | Get remote peer info |
| `local_endpoint` | `local_endpoint() const noexcept -> endpoint_info` | Get local binding info |

#### Connection Lifecycle

| Method | Signature | Description |
|--------|-----------|-------------|
| `connect` | `connect(const endpoint_info&) -> VoidResult` | Connect via host/port |
| `connect` | `connect(std::string_view url) -> VoidResult` | Connect via URL |
| `close` | `close() noexcept -> void` | Graceful close |
| `wait_for_stop` | `wait_for_stop() -> void` | Block until fully stopped |

#### Configuration

| Method | Signature | Description |
|--------|-----------|-------------|
| `set_callbacks` | `set_callbacks(connection_callbacks) -> void` | Set all event callbacks |
| `set_options` | `set_options(connection_options) -> void` | Set connection options |
| `set_timeout` | `set_timeout(std::chrono::milliseconds) -> void` | Set connect timeout |

#### State Query

| Method | Signature | Description |
|--------|-----------|-------------|
| `is_connecting` | `is_connecting() const noexcept -> bool` | Check if connect in progress |

### Connection State Machine

```
                    ┌────────────┐
                    │  Created    │  ← Initial state after construction
                    └─────┬──────┘
                          │ connect()
                    ┌─────▼──────┐
         ┌──────── │ Connecting  │ ──────── error ─────┐
         │         └─────┬──────┘                      │
         │               │ success                     │
         │         ┌─────▼──────┐                      │
         │         │ Connected   │ ← Ready for send/   │
         │         │             │   receive            │
         │         └─────┬──────┘                      │
         │               │ close() or                  │
         │               │ remote close / error        │
         │         ┌─────▼──────┐                      │
         │         │  Closing    │                      │
         │         └─────┬──────┘                      │
         │               │                             │
         │         ┌─────▼──────┐                      │
         └────────►│  Closed     │◄────────────────────┘
                   └────────────┘
```

**State transitions and corresponding queries:**

| State | `is_connected()` | `is_connecting()` | Allowed Operations |
|-------|:-:|:-:|---|
| Created | `false` | `false` | `connect()`, `set_callbacks()`, `set_options()` |
| Connecting | `false` | `true` | `close()` (cancel) |
| Connected | `true` | `false` | `send()`, `close()` |
| Closing | `false` | `false` | `wait_for_stop()` |
| Closed | `false` | `false` | (terminal state) |

### Example: Client Connection with Callbacks

```cpp
#include <kcenon/network/detail/unified/i_connection.h>
#include <iostream>

using namespace kcenon::network::unified;

void run_client(i_connection& conn)
{
    // Configure callbacks before connecting
    conn.set_callbacks({
        .on_connected = []() {
            std::cout << "Connected to server!" << std::endl;
        },
        .on_data = [](std::span<const std::byte> data) {
            std::cout << "Received " << data.size() << " bytes" << std::endl;
        },
        .on_disconnected = []() {
            std::cout << "Disconnected from server" << std::endl;
        },
        .on_error = [](std::error_code ec) {
            std::cerr << "Error: " << ec.message() << std::endl;
        }
    });

    // Set connection options
    conn.set_options({
        .connect_timeout = std::chrono::seconds(10),
        .read_timeout = std::chrono::seconds(30),
        .keep_alive = true,
        .no_delay = true
    });

    // Connect to remote endpoint
    if (auto result = conn.connect(endpoint_info{"example.com", 8080}); !result)
    {
        std::cerr << "Connection failed" << std::endl;
        return;
    }

    // Send data
    std::vector<uint8_t> payload = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    conn.send(std::move(payload));

    // Close gracefully
    conn.close();
    conn.wait_for_stop();
}
```

### Example: URL-Based Connection (WebSocket)

```cpp
void connect_websocket(i_connection& conn)
{
    conn.set_callbacks({
        .on_connected = []() {
            std::cout << "WebSocket connected" << std::endl;
        },
        .on_data = [](std::span<const std::byte> data) {
            // Process WebSocket message
        }
    });

    // URL-based connect for WebSocket protocol
    if (auto result = conn.connect("wss://example.com/ws"); !result)
    {
        std::cerr << "WebSocket connection failed" << std::endl;
        return;
    }
}
```

---

## i_listener Interface

**Header:** `kcenon/network/detail/unified/i_listener.h`

The `i_listener` interface provides **server-side connection acceptance and management**. It is a standalone interface (does not inherit from `i_transport`) that manages multiple client connections.

### Method Reference

#### Listener Lifecycle

| Method | Signature | Description |
|--------|-----------|-------------|
| `start` | `start(const endpoint_info&) -> VoidResult` | Start listening on address |
| `start` | `start(uint16_t port) -> VoidResult` | Start listening on port (all interfaces) |
| `stop` | `stop() noexcept -> void` | Stop and close all connections |
| `wait_for_stop` | `wait_for_stop() -> void` | Block until fully stopped |

#### Configuration

| Method | Signature | Description |
|--------|-----------|-------------|
| `set_callbacks` | `set_callbacks(listener_callbacks) -> void` | Set all event callbacks |
| `set_accept_callback` | `set_accept_callback(accept_callback_t) -> void` | Set accept callback with connection ownership |

#### State Query

| Method | Signature | Description |
|--------|-----------|-------------|
| `is_listening` | `is_listening() const noexcept -> bool` | Check if actively listening |
| `local_endpoint` | `local_endpoint() const noexcept -> endpoint_info` | Get bound address |
| `connection_count` | `connection_count() const noexcept -> size_t` | Count active connections |

#### Connection Management

| Method | Signature | Description |
|--------|-----------|-------------|
| `send_to` | `send_to(string_view conn_id, span<const byte>) -> VoidResult` | Send to specific client |
| `broadcast` | `broadcast(span<const byte>) -> VoidResult` | Send to all clients |
| `close_connection` | `close_connection(string_view conn_id) noexcept -> void` | Close specific client |

### Accept Callback Types

The listener provides two callback mechanisms for handling new connections:

**1. Lightweight callback (via `listener_callbacks`):**
```cpp
listener_callbacks cbs;
cbs.on_accept = [](std::string_view conn_id) {
    // Notified of connection ID only
    // Use send_to(conn_id, ...) to communicate
};
```

**2. Full ownership callback (via `set_accept_callback`):**
```cpp
using accept_callback_t = std::function<void(std::unique_ptr<i_connection>)>;

listener->set_accept_callback([](std::unique_ptr<i_connection> conn) {
    // Full ownership of the connection object
    // Can set callbacks, configure options, etc.
});
```

### Example: Echo Server

```cpp
#include <kcenon/network/detail/unified/i_listener.h>
#include <iostream>

using namespace kcenon::network::unified;

void run_echo_server(i_listener& listener)
{
    // Set listener callbacks
    listener.set_callbacks({
        .on_accept = [](std::string_view conn_id) {
            std::cout << "New connection: " << conn_id << std::endl;
        },
        .on_data = [&listener](std::string_view conn_id,
                               std::span<const std::byte> data) {
            // Echo data back to the sender
            listener.send_to(conn_id, data);
        },
        .on_disconnect = [](std::string_view conn_id) {
            std::cout << "Disconnected: " << conn_id << std::endl;
        },
        .on_error = [](std::string_view conn_id, std::error_code ec) {
            std::cerr << "Error on " << conn_id << ": "
                      << ec.message() << std::endl;
        }
    });

    // Start listening on port 8080 (all interfaces)
    if (auto result = listener.start(8080); !result)
    {
        std::cerr << "Failed to start listener" << std::endl;
        return;
    }

    std::cout << "Echo server listening on "
              << listener.local_endpoint().to_string() << std::endl;
    std::cout << "Active connections: " << listener.connection_count() << std::endl;

    // Wait until stopped externally
    listener.wait_for_stop();
}
```

### Example: Broadcast Server

```cpp
void run_chat_server(i_listener& listener)
{
    listener.set_callbacks({
        .on_accept = [](std::string_view conn_id) {
            std::cout << "User joined: " << conn_id << std::endl;
        },
        .on_data = [&listener](std::string_view sender_id,
                               std::span<const std::byte> data) {
            // Broadcast message to all connected clients
            listener.broadcast(data);
        },
        .on_disconnect = [](std::string_view conn_id) {
            std::cout << "User left: " << conn_id << std::endl;
        }
    });

    listener.start(endpoint_info{"0.0.0.0", 9000});

    std::cout << "Chat server running on port 9000" << std::endl;
    listener.wait_for_stop();
}
```

### Example: Connection Ownership via Accept Callback

```cpp
#include <map>
#include <mutex>

void run_server_with_connection_ownership(i_listener& listener)
{
    std::mutex connections_mutex;
    std::map<std::string, std::unique_ptr<i_connection>> connections;

    // Use accept callback for full connection ownership
    listener.set_accept_callback(
        [&](std::unique_ptr<i_connection> conn) {
            auto id = std::string(conn->id());
            std::cout << "Accepted connection: " << id << std::endl;

            // Configure the accepted connection individually
            conn->set_callbacks({
                .on_data = [id](std::span<const std::byte> data) {
                    std::cout << "Data from " << id << ": "
                              << data.size() << " bytes" << std::endl;
                },
                .on_disconnected = [id]() {
                    std::cout << "Connection " << id << " closed" << std::endl;
                }
            });

            std::lock_guard lock(connections_mutex);
            connections[id] = std::move(conn);
        });

    listener.start(8080);
    listener.wait_for_stop();
}
```

---

## Protocol-Agnostic Code Examples

### Generic Message Sender

```cpp
#include <kcenon/network/detail/unified/i_transport.h>
#include <iostream>
#include <string>

using namespace kcenon::network::unified;

// Works with TCP, UDP, WebSocket, QUIC - any protocol
void send_text(i_transport& transport, const std::string& message)
{
    std::vector<uint8_t> data(message.begin(), message.end());
    if (auto result = transport.send(std::move(data)); !result)
    {
        std::cerr << "Failed to send to " << transport.id() << std::endl;
    }
}

// Generic connection handler - works with any protocol
void handle_connection(i_connection& conn, const endpoint_info& target)
{
    conn.set_callbacks({
        .on_connected = [&conn]() {
            std::cout << "Connected: " << conn.id() << " -> "
                      << conn.remote_endpoint().to_string() << std::endl;
        },
        .on_data = [](std::span<const std::byte> data) {
            std::cout << "Received " << data.size() << " bytes" << std::endl;
        },
        .on_disconnected = [&conn]() {
            std::cout << "Disconnected: " << conn.id() << std::endl;
        },
        .on_error = [](std::error_code ec) {
            std::cerr << "Error: " << ec.message() << std::endl;
        }
    });

    conn.set_options({
        .connect_timeout = std::chrono::seconds(10),
        .keep_alive = true
    });

    if (auto result = conn.connect(target); !result)
    {
        std::cerr << "Connection failed" << std::endl;
        return;
    }

    send_text(conn, "Hello from unified API!");
    conn.close();
}
```

### Generic Server Handler

```cpp
// Protocol-agnostic server that works with any listener implementation
void run_generic_server(i_listener& listener, uint16_t port)
{
    listener.set_callbacks({
        .on_accept = [](std::string_view conn_id) {
            std::cout << "Client connected: " << conn_id << std::endl;
        },
        .on_data = [&listener](std::string_view conn_id,
                               std::span<const std::byte> data) {
            std::cout << "Received " << data.size() << " bytes from "
                      << conn_id << std::endl;

            // Echo back to sender
            listener.send_to(conn_id, data);
        },
        .on_disconnect = [](std::string_view conn_id) {
            std::cout << "Client disconnected: " << conn_id << std::endl;
        },
        .on_error = [](std::string_view conn_id, std::error_code ec) {
            std::cerr << "Error on " << conn_id << ": "
                      << ec.message() << std::endl;
        }
    });

    if (auto result = listener.start(port); !result)
    {
        std::cerr << "Failed to start server on port " << port << std::endl;
        return;
    }

    std::cout << "Server running on "
              << listener.local_endpoint().to_string() << std::endl;

    listener.wait_for_stop();
}
```

### Switching Protocols at Runtime

```cpp
// The same function works regardless of protocol
void communicate(i_connection& conn, const endpoint_info& target,
                 const std::string& message)
{
    conn.set_callbacks({
        .on_data = [](std::span<const std::byte> data) {
            std::cout << "Response: " << data.size() << " bytes" << std::endl;
        }
    });

    if (auto result = conn.connect(target); result)
    {
        std::vector<uint8_t> payload(message.begin(), message.end());
        conn.send(std::move(payload));
        conn.close();
        conn.wait_for_stop();
    }
}

// Usage: same function, different protocols
// auto tcp_conn = create_tcp_connection();
// auto quic_conn = create_quic_connection();
//
// communicate(*tcp_conn, {"server.com", 8080}, "Hello via TCP!");
// communicate(*quic_conn, {"server.com", 4433}, "Hello via QUIC!");
```

---

## Protocol Support Matrix

| Protocol | i_transport | i_connection | i_listener | Data Model | TLS Support |
|----------|:-:|:-:|:-:|---|---|
| **TCP** | Yes | Yes | Yes | Stream (byte-oriented) | Optional (TLS 1.2+) |
| **UDP** | Yes | Partial | Yes | Datagram (message boundaries) | N/A |
| **WebSocket** | Yes | Yes | Yes | Frame (message boundaries) | Optional (WSS) |
| **QUIC** | Yes | Yes | Yes | Stream + datagram | Always (TLS 1.3) |
| **HTTP/1.1** | Yes | Yes | Yes | Request/response | Optional (HTTPS) |
| **TLS** | Yes | Yes | Yes | Stream (wraps TCP) | Built-in |

### Protocol-Specific Behaviors

| Behavior | TCP | UDP | WebSocket | QUIC |
|----------|-----|-----|-----------|------|
| `connect()` | TCP handshake | Set target endpoint | HTTP upgrade + WS handshake | QUIC handshake |
| `send()` | Byte stream (may fragment) | Single datagram (preserves boundaries) | Binary frame (preserves boundaries) | Stream data |
| `close()` | FIN/ACK exchange | Stop receiving | Close frame | CONNECTION_CLOSE |
| `is_connected()` | TCP connection state | Target endpoint set | WS connection state | QUIC connection state |
| Keep-alive | TCP keepalive option | N/A | Ping/pong frames | Built-in |

### Data Type Notes

The Unified API uses `std::span<const std::byte>` for data parameters, while the Facade/Protocol layers use `std::vector<uint8_t>`. The `i_transport` interface provides both overloads:

```cpp
// Raw byte span (preferred for zero-copy)
auto send(std::span<const std::byte> data) -> VoidResult;

// uint8_t vector (convenience, compatible with older code)
auto send(std::vector<uint8_t>&& data) -> VoidResult;
```

---

## Extending with Custom Protocols

### Implementing i_transport

To add support for a new protocol, implement the three core interfaces starting with `i_transport`:

```cpp
#include <kcenon/network/detail/unified/i_transport.h>

namespace my_protocol {

class my_transport : public kcenon::network::unified::i_transport {
public:
    // Data transfer
    auto send(std::span<const std::byte> data) -> VoidResult override
    {
        // Protocol-specific send implementation
    }

    auto send(std::vector<uint8_t>&& data) -> VoidResult override
    {
        // Convert and delegate to span-based send
        auto bytes = std::as_bytes(std::span{data});
        return send(bytes);
    }

    // State queries
    auto is_connected() const noexcept -> bool override
    {
        return connected_.load();
    }

    auto id() const noexcept -> std::string_view override
    {
        return id_;
    }

    auto remote_endpoint() const noexcept -> endpoint_info override
    {
        return remote_;
    }

    auto local_endpoint() const noexcept -> endpoint_info override
    {
        return local_;
    }

private:
    std::atomic<bool> connected_{false};
    std::string id_;
    endpoint_info remote_;
    endpoint_info local_;
};

} // namespace my_protocol
```

### Implementing i_connection

Extend your transport with connection lifecycle:

```cpp
class my_connection : public kcenon::network::unified::i_connection {
public:
    // Connection lifecycle
    auto connect(const endpoint_info& endpoint) -> VoidResult override
    {
        // Protocol-specific connection logic
        // Invoke callbacks_.on_connected() on success
    }

    auto connect(std::string_view url) -> VoidResult override
    {
        // Parse URL and delegate to endpoint-based connect
    }

    auto close() noexcept -> void override
    {
        // Graceful shutdown
        // Invoke callbacks_.on_disconnected()
    }

    // Configuration
    auto set_callbacks(connection_callbacks callbacks) -> void override
    {
        callbacks_ = std::move(callbacks);
    }

    auto set_options(connection_options options) -> void override
    {
        options_ = options;
    }

    auto set_timeout(std::chrono::milliseconds timeout) -> void override
    {
        options_.connect_timeout = timeout;
    }

    // State queries
    auto is_connecting() const noexcept -> bool override
    {
        return connecting_.load();
    }

    auto wait_for_stop() -> void override
    {
        // Block until fully closed
    }

    // Inherited from i_transport - must implement
    auto send(std::span<const std::byte> data) -> VoidResult override { /* ... */ }
    auto send(std::vector<uint8_t>&& data) -> VoidResult override { /* ... */ }
    auto is_connected() const noexcept -> bool override { /* ... */ }
    auto id() const noexcept -> std::string_view override { /* ... */ }
    auto remote_endpoint() const noexcept -> endpoint_info override { /* ... */ }
    auto local_endpoint() const noexcept -> endpoint_info override { /* ... */ }

private:
    connection_callbacks callbacks_;
    connection_options options_;
    std::atomic<bool> connecting_{false};
};
```

### Implementing i_listener

Provide server-side acceptance:

```cpp
class my_listener : public kcenon::network::unified::i_listener {
public:
    // Listener lifecycle
    auto start(const endpoint_info& bind_address) -> VoidResult override
    {
        // Bind and begin accepting connections
    }

    auto start(uint16_t port) -> VoidResult override
    {
        return start(endpoint_info{"0.0.0.0", port});
    }

    auto stop() noexcept -> void override
    {
        // Stop accepting, close all connections
    }

    // Configuration
    auto set_callbacks(listener_callbacks callbacks) -> void override
    {
        callbacks_ = std::move(callbacks);
    }

    auto set_accept_callback(accept_callback_t callback) -> void override
    {
        accept_callback_ = std::move(callback);
    }

    // State queries
    auto is_listening() const noexcept -> bool override { /* ... */ }
    auto local_endpoint() const noexcept -> endpoint_info override { /* ... */ }
    auto connection_count() const noexcept -> size_t override { /* ... */ }

    // Connection management
    auto send_to(std::string_view connection_id,
                 std::span<const std::byte> data) -> VoidResult override { /* ... */ }
    auto broadcast(std::span<const std::byte> data) -> VoidResult override { /* ... */ }
    auto close_connection(std::string_view connection_id) noexcept -> void override { /* ... */ }
    auto wait_for_stop() -> void override { /* ... */ }

private:
    listener_callbacks callbacks_;
    accept_callback_t accept_callback_;
};
```

### Implementation Contracts

When implementing the unified interfaces, ensure the following contracts are met:

| Contract | Requirement |
|----------|-------------|
| **Thread safety** | All public methods must be thread-safe |
| **Callback thread** | Callbacks may be invoked from I/O threads |
| **ID uniqueness** | `id()` must return a unique value within the application |
| **State consistency** | `is_connected()` must accurately reflect connection state |
| **Graceful close** | `close()` must complete pending sends before disconnecting |
| **Destructor safety** | Destructor must call `close()` if still connected |
| **Error propagation** | Failed operations return error via `VoidResult`, not exceptions |
| **Idempotent close** | `close()` and `stop()` must be safe to call multiple times |
