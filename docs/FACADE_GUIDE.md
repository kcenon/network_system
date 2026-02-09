# Facade API Guide

> **Language:** **English**

A comprehensive guide to the Facade v2.0 API layer, which provides simplified, high-level interfaces for each network protocol in the `network_system` library.

## Table of Contents

- [Facade Pattern Overview](#facade-pattern-overview)
- [TCP Facade](#tcp-facade)
- [UDP Facade](#udp-facade)
- [HTTP Facade](#http-facade)
- [WebSocket Facade](#websocket-facade)
- [QUIC Facade](#quic-facade)
- [Common Patterns Across Facades](#common-patterns-across-facades)
- [Migration from Low-Level API](#migration-from-low-level-api)

## Facade Pattern Overview

### Why Facades Exist

The `network_system` library provides powerful, low-level protocol implementations with extensive template parameters, protocol tags, and TLS policies. While flexible, this complexity creates a steep learning curve for common use cases. The facade layer eliminates this complexity by providing:

- **No template parameters**: Concrete classes instead of template-heavy instantiation
- **Unified API**: Same `create_client()` / `create_server()` pattern across all protocols
- **Automatic TLS handling**: SSL/TLS configuration via simple boolean flags
- **Zero-cost abstraction**: No runtime overhead compared to direct instantiation

### Architecture

```
┌──────────────────────────────────────────────────┐
│                 User Application                 │
├──────────────────────────────────────────────────┤
│  tcp_facade  udp_facade  http_facade  ws_facade  │  ← Facade Layer
│                      quic_facade                  │
├──────────────────────────────────────────────────┤
│           i_protocol_client / i_protocol_server   │  ← Unified Interfaces
├──────────────────────────────────────────────────┤
│    messaging_client    messaging_udp_client       │
│    messaging_server    messaging_udp_server       │  ← Protocol Implementations
│    http_client/server  messaging_ws_client/server │
│    messaging_quic_client / messaging_quic_server  │
└──────────────────────────────────────────────────┘
```

### When to Use Facade vs Direct Protocol API

| Scenario | Recommended |
|----------|-------------|
| Simple client/server setup | **Facade** |
| Standard protocol usage | **Facade** |
| Custom template configurations | Direct API |
| Protocol-specific advanced features (e.g., QUIC multi-stream) | Direct API |
| Connection pooling (TCP) | **Facade** (`create_connection_pool`) |
| Rapid prototyping | **Facade** |

### Common Facade Interface

All facades share the same structural pattern:

```cpp
class <protocol>_facade {
public:
    struct client_config { /* protocol-specific options */ };
    struct server_config { /* protocol-specific options */ };

    auto create_client(const client_config&) -> std::shared_ptr<i_protocol_client>;
    auto create_server(const server_config&) -> std::shared_ptr<i_protocol_server>;
};
```

### Header Files

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/facade/http_facade.h>
#include <kcenon/network/facade/websocket_facade.h>
#include <kcenon/network/facade/quic_facade.h>
```

All facades reside in the `kcenon::network::facade` namespace.

---

## TCP Facade

The `tcp_facade` provides simplified TCP client/server creation with optional SSL/TLS support and connection pooling.

**Header:** `kcenon/network/facade/tcp_facade.h`

### Client Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | `std::string` | (required) | Server hostname or IP address |
| `port` | `uint16_t` | `0` | Server port number |
| `client_id` | `std::string` | auto-generated | Client identifier |
| `timeout` | `std::chrono::milliseconds` | `30s` | Connection timeout |
| `use_ssl` | `bool` | `false` | Enable SSL/TLS encryption |
| `ca_cert_path` | `std::optional<std::string>` | `nullopt` | CA certificate file path |
| `verify_certificate` | `bool` | `true` | Verify server SSL certificate |

### Server Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `port` | `uint16_t` | `0` | Port to listen on |
| `server_id` | `std::string` | auto-generated | Server identifier |
| `use_ssl` | `bool` | `false` | Enable SSL/TLS encryption |
| `cert_path` | `std::optional<std::string>` | `nullopt` | Server certificate file (required if `use_ssl=true`) |
| `key_path` | `std::optional<std::string>` | `nullopt` | Server private key file (required if `use_ssl=true`) |
| `tls_version` | `std::optional<std::string>` | `nullopt` | TLS protocol version (default: TLS 1.2+) |

### Connection Pool Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | `std::string` | (required) | Server hostname or IP address |
| `port` | `uint16_t` | `0` | Server port number |
| `pool_size` | `size_t` | `10` | Number of connections to maintain |

### Example: Echo Server

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>
#include <vector>

using namespace kcenon::network::facade;

int main()
{
    tcp_facade facade;

    // Create server
    auto server = facade.create_server({.port = 8080});

    // Echo received data back to the client
    server->set_receive_callback(
        [&](std::string_view session_id, const std::vector<uint8_t>& data) {
            // In a real implementation, look up the session and send data back
            std::cout << "Received " << data.size() << " bytes from "
                      << session_id << std::endl;
        });

    server->set_connection_callback(
        [](std::shared_ptr<kcenon::network::interfaces::i_session> session) {
            std::cout << "Client connected: " << session->id() << std::endl;
        });

    // Start listening
    if (auto result = server->start(8080); !result)
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Echo server running on port 8080" << std::endl;
    // Block until stopped
    return 0;
}
```

### Example: TCP Client with SSL

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <iostream>

using namespace kcenon::network;

int main()
{
    facade::tcp_facade facade;

    auto client = facade.create_client({
        .host = "example.com",
        .port = 8443,
        .client_id = "secure-client",
        .use_ssl = true,
        .verify_certificate = true
    });

    // Use observer pattern for event handling
    auto observer = std::make_shared<interfaces::callback_adapter>();
    observer->on_connected([]() {
        std::cout << "Connected to server" << std::endl;
    }).on_receive([](std::span<const uint8_t> data) {
        std::cout << "Received " << data.size() << " bytes" << std::endl;
    }).on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    client->set_observer(observer);

    if (auto result = client->start("example.com", 8443); !result)
    {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    // Send data
    std::vector<uint8_t> payload = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    client->send(std::move(payload));

    client->stop();
    return 0;
}
```

### Example: Connection Pool

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

using namespace kcenon::network::facade;

int main()
{
    tcp_facade facade;

    auto pool = facade.create_connection_pool({
        .host = "127.0.0.1",
        .port = 5555,
        .pool_size = 10
    });

    if (auto result = pool->initialize(); !result)
    {
        std::cerr << "Pool initialization failed" << std::endl;
        return 1;
    }

    // Acquire a connection from the pool
    auto client = pool->acquire();

    // Use the connection
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    // client->send_packet(data);

    // Release back to pool when done
    pool->release(std::move(client));

    std::cout << "Active connections: " << pool->active_count()
              << "/" << pool->pool_size() << std::endl;

    return 0;
}
```

---

## UDP Facade

The `udp_facade` provides simplified UDP datagram client/server creation.

**Header:** `kcenon/network/facade/udp_facade.h`

### Client Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | `std::string` | (required) | Target hostname or IP address |
| `port` | `uint16_t` | `0` | Target port number |
| `client_id` | `std::string` | auto-generated | Client identifier |

### Server Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `port` | `uint16_t` | `0` | Port to listen on |
| `server_id` | `std::string` | auto-generated | Server identifier |

### Example: Simple Discovery Protocol

```cpp
#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <iostream>

using namespace kcenon::network;

// Discovery server: listens for broadcast queries and responds
int main()
{
    facade::udp_facade facade;

    // Create UDP server for discovery
    auto server = facade.create_server({
        .port = 9999,
        .server_id = "discovery-server"
    });

    server->set_receive_callback(
        [](std::string_view session_id, const std::vector<uint8_t>& data) {
            std::cout << "Discovery query from " << session_id
                      << " (" << data.size() << " bytes)" << std::endl;
        });

    if (auto result = server->start(9999); !result)
    {
        std::cerr << "Failed to start discovery server" << std::endl;
        return 1;
    }

    std::cout << "Discovery server listening on port 9999" << std::endl;
    return 0;
}
```

### Example: UDP Client

```cpp
#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <iostream>

using namespace kcenon::network;

int main()
{
    facade::udp_facade facade;

    auto client = facade.create_client({
        .host = "127.0.0.1",
        .port = 9999,
        .client_id = "discovery-client"
    });

    auto observer = std::make_shared<interfaces::callback_adapter>();
    observer->on_receive([](std::span<const uint8_t> data) {
        std::cout << "Response: " << data.size() << " bytes" << std::endl;
    });

    client->set_observer(observer);

    if (auto result = client->start("127.0.0.1", 9999); !result)
    {
        std::cerr << "Failed to start UDP client" << std::endl;
        return 1;
    }

    // Send discovery query
    std::vector<uint8_t> query = {'D', 'I', 'S', 'C', 'O', 'V', 'E', 'R'};
    client->send(std::move(query));

    client->stop();
    return 0;
}
```

> **Note:** UDP is connectionless. The `start()` method sets the default target endpoint rather than establishing a persistent connection. Message boundaries are preserved (each `send()` produces one datagram).

---

## HTTP Facade

The `http_facade` provides simplified HTTP/1.1 client/server creation through the unified protocol interface.

**Header:** `kcenon/network/facade/http_facade.h`

### Client Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `client_id` | `std::string` | auto-generated | Client identifier |
| `timeout` | `std::chrono::milliseconds` | `30s` | Request timeout |
| `use_ssl` | `bool` | `false` | Enable HTTPS |
| `path` | `std::string` | `"/"` | HTTP path |

### Server Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `port` | `uint16_t` | `0` | Port to listen on |
| `server_id` | `std::string` | auto-generated | Server identifier |

### Example: HTTP Server

```cpp
#include <kcenon/network/facade/http_facade.h>
#include <iostream>
#include <string>

using namespace kcenon::network;

int main()
{
    facade::http_facade facade;

    auto server = facade.create_server({
        .port = 8080,
        .server_id = "api-server"
    });

    // Handle incoming HTTP requests
    server->set_receive_callback(
        [](std::string_view session_id, const std::vector<uint8_t>& data) {
            std::string body(data.begin(), data.end());
            std::cout << "Request from " << session_id << ": " << body << std::endl;
        });

    server->set_connection_callback(
        [](std::shared_ptr<interfaces::i_session> session) {
            // Send response
            std::string response = R"({"status": "ok"})";
            std::vector<uint8_t> response_data(response.begin(), response.end());
            session->send(std::move(response_data));
        });

    if (auto result = server->start(8080); !result)
    {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return 1;
    }

    std::cout << "HTTP server running on port 8080" << std::endl;
    return 0;
}
```

### Example: HTTP Client

```cpp
#include <kcenon/network/facade/http_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <iostream>

using namespace kcenon::network;

int main()
{
    facade::http_facade facade;

    auto client = facade.create_client({
        .client_id = "api-client",
        .timeout = std::chrono::seconds(10),
        .path = "/api/data"
    });

    auto observer = std::make_shared<interfaces::callback_adapter>();
    observer->on_receive([](std::span<const uint8_t> data) {
        std::string response(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout << "Response: " << response << std::endl;
    }).on_error([](std::error_code ec) {
        std::cerr << "HTTP error: " << ec.message() << std::endl;
    });

    client->set_observer(observer);

    // Connect to server
    if (auto result = client->start("localhost", 8080); !result)
    {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    // Send POST request with binary data
    std::string body = R"({"key": "value"})";
    std::vector<uint8_t> payload(body.begin(), body.end());
    client->send(std::move(payload));

    client->stop();
    return 0;
}
```

> **Protocol-Specific Notes:**
> - `start()` sets the base URL (host:port) for subsequent requests
> - `send()` performs an HTTP POST request with the provided binary data
> - Received response body is delivered via the receive callback/observer

---

## WebSocket Facade

The `websocket_facade` provides simplified WebSocket client/server creation with automatic ping/pong keepalive.

**Header:** `kcenon/network/facade/websocket_facade.h`

### Client Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `client_id` | `std::string` | auto-generated | Client identifier |
| `ping_interval` | `std::chrono::milliseconds` | `30s` | Ping keepalive interval |

### Server Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `port` | `uint16_t` | `0` | Port to listen on |
| `path` | `std::string` | `"/"` | WebSocket endpoint path |
| `server_id` | `std::string` | auto-generated | Server identifier |

### Example: Chat Server

```cpp
#include <kcenon/network/facade/websocket_facade.h>
#include <iostream>
#include <mutex>
#include <unordered_map>

using namespace kcenon::network;

int main()
{
    facade::websocket_facade facade;

    auto server = facade.create_server({
        .port = 8080,
        .path = "/chat",
        .server_id = "chat-server"
    });

    // Track connected sessions
    std::mutex sessions_mutex;
    std::unordered_map<std::string, std::shared_ptr<interfaces::i_session>> sessions;

    server->set_connection_callback(
        [&](std::shared_ptr<interfaces::i_session> session) {
            std::lock_guard lock(sessions_mutex);
            sessions[std::string(session->id())] = session;
            std::cout << "User joined: " << session->id() << std::endl;
        });

    server->set_disconnection_callback(
        [&](std::string_view session_id) {
            std::lock_guard lock(sessions_mutex);
            sessions.erase(std::string(session_id));
            std::cout << "User left: " << session_id << std::endl;
        });

    // Broadcast received messages to all connected clients
    server->set_receive_callback(
        [&](std::string_view sender_id, const std::vector<uint8_t>& data) {
            std::lock_guard lock(sessions_mutex);
            for (auto& [id, session] : sessions)
            {
                if (id != sender_id && session->is_connected())
                {
                    std::vector<uint8_t> copy(data);
                    session->send(std::move(copy));
                }
            }
        });

    if (auto result = server->start(8080); !result)
    {
        std::cerr << "Failed to start chat server" << std::endl;
        return 1;
    }

    std::cout << "Chat server running on ws://localhost:8080/chat" << std::endl;
    return 0;
}
```

### Example: WebSocket Client

```cpp
#include <kcenon/network/facade/websocket_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <iostream>

using namespace kcenon::network;

int main()
{
    facade::websocket_facade facade;

    auto client = facade.create_client({
        .client_id = "chat-user-1",
        .ping_interval = std::chrono::seconds(15)
    });

    auto observer = std::make_shared<interfaces::callback_adapter>();
    observer->on_connected([]() {
        std::cout << "Connected to chat server" << std::endl;
    }).on_receive([](std::span<const uint8_t> data) {
        std::string message(reinterpret_cast<const char*>(data.data()), data.size());
        std::cout << "Message: " << message << std::endl;
    }).on_disconnected([](std::optional<std::string_view> reason) {
        std::cout << "Disconnected";
        if (reason) std::cout << ": " << *reason;
        std::cout << std::endl;
    });

    client->set_observer(observer);

    if (auto result = client->start("localhost", 8080); !result)
    {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }

    // Send a chat message
    std::string msg = "Hello, everyone!";
    std::vector<uint8_t> payload(msg.begin(), msg.end());
    client->send(std::move(payload));

    client->stop();
    return 0;
}
```

> **Protocol-Specific Notes:**
> - `send()` transmits data as binary WebSocket frames via the unified interface
> - For text messages or WebSocket-specific features, cast to `i_websocket_client`/`i_websocket_server`
> - The `path` field in server config must start with `'/'`
> - Automatic ping/pong keepalive prevents connection timeouts

---

## QUIC Facade

The `quic_facade` provides simplified QUIC client/server creation with built-in TLS 1.3, ALPN negotiation, and optional 0-RTT support.

**Header:** `kcenon/network/facade/quic_facade.h`

### Client Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | `std::string` | (required) | Server hostname or IP address |
| `port` | `uint16_t` | `0` | Server port number |
| `client_id` | `std::string` | auto-generated | Client identifier |
| `ca_cert_path` | `std::optional<std::string>` | `nullopt` | CA certificate for server verification |
| `client_cert_path` | `std::optional<std::string>` | `nullopt` | Client certificate for mutual TLS |
| `client_key_path` | `std::optional<std::string>` | `nullopt` | Client private key for mutual TLS |
| `verify_server` | `bool` | `true` | Verify server certificate |
| `alpn` | `std::string` | (required) | ALPN protocol identifier (e.g., `"h3"`) |
| `max_idle_timeout_ms` | `uint64_t` | `30000` | Maximum idle timeout in milliseconds |
| `enable_0rtt` | `bool` | `false` | Enable 0-RTT early data |

### Server Configuration

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `port` | `uint16_t` | `0` | Port to listen on |
| `server_id` | `std::string` | auto-generated | Server identifier |
| `cert_path` | `std::string` | (required) | Server certificate file (PEM) |
| `key_path` | `std::string` | (required) | Server private key file (PEM) |
| `ca_cert_path` | `std::optional<std::string>` | `nullopt` | CA certificate for client verification |
| `require_client_cert` | `bool` | `false` | Require mutual TLS |
| `alpn` | `std::string` | (required) | ALPN protocol identifier |
| `max_idle_timeout_ms` | `uint64_t` | `30000` | Maximum idle timeout in milliseconds |
| `max_connections` | `size_t` | `10000` | Maximum concurrent connections |

### Example: QUIC Server

```cpp
#include <kcenon/network/facade/quic_facade.h>
#include <iostream>

using namespace kcenon::network;

int main()
{
    facade::quic_facade facade;

    auto server = facade.create_server({
        .port = 4433,
        .server_id = "quic-server",
        .cert_path = "/path/to/cert.pem",
        .key_path = "/path/to/key.pem",
        .alpn = "h3",
        .max_idle_timeout_ms = 60000,
        .max_connections = 5000
    });

    server->set_connection_callback(
        [](std::shared_ptr<interfaces::i_session> session) {
            std::cout << "QUIC client connected: " << session->id() << std::endl;
        });

    server->set_receive_callback(
        [](std::string_view session_id, const std::vector<uint8_t>& data) {
            std::cout << "Received " << data.size() << " bytes from "
                      << session_id << std::endl;
        });

    if (auto result = server->start(4433); !result)
    {
        std::cerr << "Failed to start QUIC server" << std::endl;
        return 1;
    }

    std::cout << "QUIC server running on port 4433" << std::endl;
    return 0;
}
```

### Example: QUIC Client with 0-RTT

```cpp
#include <kcenon/network/facade/quic_facade.h>
#include <kcenon/network/interfaces/connection_observer.h>
#include <iostream>

using namespace kcenon::network;

int main()
{
    facade::quic_facade facade;

    auto client = facade.create_client({
        .host = "127.0.0.1",
        .port = 4433,
        .client_id = "quic-client",
        .ca_cert_path = "/path/to/ca.pem",
        .alpn = "h3",
        .max_idle_timeout_ms = 60000,
        .enable_0rtt = true
    });

    auto observer = std::make_shared<interfaces::callback_adapter>();
    observer->on_connected([]() {
        std::cout << "QUIC connection established" << std::endl;
    }).on_receive([](std::span<const uint8_t> data) {
        std::cout << "Received " << data.size() << " bytes" << std::endl;
    }).on_error([](std::error_code ec) {
        std::cerr << "QUIC error: " << ec.message() << std::endl;
    });

    client->set_observer(observer);

    if (auto result = client->start("127.0.0.1", 4433); !result)
    {
        std::cerr << "QUIC connection failed" << std::endl;
        return 1;
    }

    // Send data over QUIC
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
    client->send(std::move(payload));

    client->stop();
    return 0;
}
```

> **Protocol-Specific Notes:**
> - QUIC always uses TLS 1.3 encryption (no plaintext mode)
> - Server requires `cert_path` and `key_path` (mandatory)
> - For QUIC-specific features (multi-stream, stream multiplexing, connection migration), use `messaging_quic_client`/`messaging_quic_server` directly instead of the facade

---

## Common Patterns Across Facades

### Error Handling with Result\<T\>

All facade methods that can fail return `VoidResult` (alias for `Result<void>`). The `Result<T>` type provides a type-safe alternative to exceptions:

```cpp
auto client = facade.create_client({.host = "127.0.0.1", .port = 8080});

// Check result with bool conversion
if (auto result = client->start("127.0.0.1", 8080); !result)
{
    // Handle error - result contains error_info
    std::cerr << "Failed to start client" << std::endl;
    return 1;
}

// Send also returns VoidResult
if (auto result = client->send(std::move(data)); !result)
{
    std::cerr << "Send failed" << std::endl;
}
```

### Observer Pattern (Recommended)

The `connection_observer` interface is the recommended approach for handling client events. It centralizes all event handling in one place:

```cpp
// Option 1: Implement connection_observer directly
class my_observer : public kcenon::network::interfaces::connection_observer
{
public:
    void on_receive(std::span<const uint8_t> data) override
    {
        // Handle received data
    }

    void on_connected() override
    {
        // Handle connection established
    }

    void on_disconnected(std::optional<std::string_view> reason) override
    {
        // Handle disconnection
    }

    void on_error(std::error_code ec) override
    {
        // Handle error
    }
};

auto observer = std::make_shared<my_observer>();
client->set_observer(observer);
```

### Callback Adapter (Fluent API)

For simpler cases, use `callback_adapter` with a fluent builder pattern:

```cpp
auto adapter = std::make_shared<interfaces::callback_adapter>();
adapter->on_connected([]() {
    std::cout << "Connected" << std::endl;
}).on_receive([](std::span<const uint8_t> data) {
    // Process data
}).on_disconnected([](std::optional<std::string_view> reason) {
    std::cout << "Disconnected" << std::endl;
}).on_error([](std::error_code ec) {
    std::cerr << "Error: " << ec.message() << std::endl;
});

client->set_observer(adapter);
```

### Null Observer

When you only need some events, extend `null_connection_observer`:

```cpp
class receive_only_observer : public interfaces::null_connection_observer
{
public:
    void on_receive(std::span<const uint8_t> data) override
    {
        // Only handle received data; all other events are no-ops
    }
};
```

### Server Callback Pattern

Servers use a separate callback model for managing multiple client sessions:

```cpp
// Connection events
server->set_connection_callback([](std::shared_ptr<i_session> session) {
    std::cout << "New client: " << session->id() << std::endl;
    // session->send() to send data to this specific client
});

// Disconnection events
server->set_disconnection_callback([](std::string_view session_id) {
    std::cout << "Client left: " << session_id << std::endl;
});

// Data received from any client
server->set_receive_callback([](std::string_view session_id,
                                const std::vector<uint8_t>& data) {
    // session_id identifies which client sent the data
});

// Error on any session
server->set_error_callback([](std::string_view session_id,
                              std::error_code error) {
    std::cerr << "Error on " << session_id << ": "
              << error.message() << std::endl;
});
```

### Client Lifecycle

All protocol clients follow the same lifecycle:

```
  create_client()   start()        send()/receive      stop()
  ─────────────► [Created] ────► [Connected] ────► [Stopped]
                     │                │                  ▲
                     │                └── on_error ──────┘
                     └─── (invalid config) ──► throws
```

1. **Create**: `facade.create_client(config)` - validates config, returns `shared_ptr`
2. **Start**: `client->start(host, port)` - connects to server
3. **Communicate**: `client->send()` / observer `on_receive()`
4. **Stop**: `client->stop()` - graceful disconnection

### Server Lifecycle

```
  create_server()   start(port)    accept/receive      stop()
  ─────────────► [Created] ────► [Listening] ────► [Stopped]
                     │                │                  ▲
                     │                └── on_error ──────┘
                     └─── (invalid config) ──► throws
```

1. **Create**: `facade.create_server(config)` - validates config, returns `shared_ptr`
2. **Start**: `server->start(port)` - begins listening
3. **Serve**: Accept connections, handle data via callbacks
4. **Stop**: `server->stop()` - closes all connections gracefully

### TLS Configuration

Protocols that support TLS follow a consistent pattern:

| Protocol | TLS Support | Configuration |
|----------|-------------|---------------|
| TCP | Optional (`use_ssl`) | `cert_path`, `key_path`, `ca_cert_path` |
| HTTP | Optional (`use_ssl`) | Via `client_config.use_ssl` |
| WebSocket | Not in facade | Use direct API for WSS |
| QUIC | Always on (TLS 1.3) | `cert_path`, `key_path` (server required) |
| UDP | Not supported | Use TCP/QUIC for encrypted transport |

### Thread Safety

All facades and the objects they create are thread-safe:

- Facade methods (`create_client`, `create_server`) can be called concurrently
- Client/server `start()`, `stop()`, `send()` are thread-safe
- Callbacks and observer methods may be invoked from I/O threads
- Multiple `send()` operations are serialized internally

---

## Migration from Low-Level API

### When to Switch from Facade to Direct API

The facade layer covers common use cases. Consider using the direct API when you need:

| Feature | Facade | Direct API |
|---------|--------|------------|
| Basic client/server | Yes | Yes |
| SSL/TLS | Yes (TCP, QUIC) | Yes |
| Connection pooling | Yes (TCP) | Yes |
| Multi-stream (QUIC) | No | Yes |
| Stream multiplexing | No | Yes |
| Custom template policies | No | Yes |
| Advanced QUIC features | No | Yes |
| WebSocket text frames | No | Yes |
| Custom protocol tags | No | Yes |

### How Facades Map to Internal Types

| Facade | Plain Client | SSL Client | Plain Server | SSL Server |
|--------|-------------|------------|-------------|------------|
| `tcp_facade` | `messaging_client` | `secure_messaging_client` | `messaging_server` | `secure_messaging_server` |
| `udp_facade` | `messaging_udp_client` | — | `messaging_udp_server` | — |
| `http_facade` | `http_client` | — | `http_server` | — |
| `websocket_facade` | `messaging_ws_client` | — | `messaging_ws_server` | — |
| `quic_facade` | `messaging_quic_client` | (always TLS) | `messaging_quic_server` | (always TLS) |

### Migration Example

**Before (Direct API):**

```cpp
// Complex template instantiation
auto client = std::make_shared<messaging_client>(
    /* ... template parameters, protocol tags, TLS policy ... */
);
// Manual setup...
```

**After (Facade):**

```cpp
tcp_facade facade;
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .use_ssl = true
});
// Ready to use immediately
```

### Gradual Migration with callback_adapter

If your codebase uses legacy callbacks, the `callback_adapter` enables gradual migration:

```cpp
// Legacy callback style
client->set_receive_callback([](const std::vector<uint8_t>& data) { /* ... */ });

// New observer style (recommended)
auto adapter = std::make_shared<callback_adapter>();
adapter->on_receive([](std::span<const uint8_t> data) { /* ... */ });
client->set_observer(adapter);
```

The `callback_adapter` wraps `std::function` callbacks into the `connection_observer` interface, allowing incremental migration without rewriting all event handlers at once.
