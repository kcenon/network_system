# Network System Facade API

> **Version:** 2.0.0
> **Last Updated:** 2025-02-04
> **Status:** Complete

## Overview

The Facade API provides a simplified, unified interface for creating protocol clients and servers without exposing template parameters, protocol tags, or implementation details. Each protocol (TCP, UDP, WebSocket, HTTP, QUIC) has a dedicated facade class that follows the same design pattern for consistency.

### What is the Facade Pattern?

The Facade pattern is a structural design pattern that provides a simplified interface to a complex subsystem. In network_system, facades hide:

- Template parameter complexity (protocol tags, policies)
- Internal implementation details (session management, adapters)
- Protocol-specific initialization boilerplate

### Design Goals

| Goal | Description | Benefit |
|------|-------------|---------|
| **Simplicity** | No template parameters or protocol tags | Easier to learn and use |
| **Consistency** | Same API pattern across all protocols | Reduced cognitive load |
| **Type Safety** | Returns standard `i_protocol_client`/`i_protocol_server` interfaces | Protocol-agnostic code |
| **Zero Cost** | No performance overhead | Production-ready performance |

---

## Quick Start

### Before (Direct Template Usage)

```cpp
#include <kcenon/network/core/messaging_client.h>
#include <kcenon/network/policies/tcp_policy.h>

using namespace kcenon::network;

// Complex template instantiation
auto client = std::make_shared<
    core::messaging_client<policies::tcp_policy>
>("my-client");

auto result = client->start_client("127.0.0.1", 8080);
```

### After (Facade API)

```cpp
#include <kcenon/network/facade/tcp_facade.h>

using namespace kcenon::network::facade;

// Simple facade usage
tcp_facade facade;
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "my-client"
});

auto result = client->start();
```

---

## Available Facades

### TCP Facade

**Header:** `<kcenon/network/facade/tcp_facade.h>`

Simplified API for TCP clients and servers with optional SSL/TLS support.

```cpp
tcp_facade facade;

// Plain TCP client
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "my-client",
    .timeout = std::chrono::seconds(30)
});

// Secure TCP client (TLS)
auto secure_client = facade.create_client({
    .host = "example.com",
    .port = 8443,
    .use_ssl = true,
    .verify_certificate = true,
    .ca_cert_path = "/path/to/ca.pem"
});

// TCP server
auto server = facade.create_server({
    .port = 8080,
    .server_id = "my-server"
});

// Secure TCP server (TLS)
auto secure_server = facade.create_server({
    .port = 8443,
    .use_ssl = true,
    .cert_path = "/path/to/cert.pem",
    .key_path = "/path/to/key.pem"
});
```

**Configuration Options:**

| Client Config | Type | Description |
|--------------|------|-------------|
| `host` | `std::string` | Server hostname or IP address (required) |
| `port` | `uint16_t` | Server port number (required) |
| `client_id` | `std::string` | Client identifier (auto-generated if empty) |
| `timeout` | `std::chrono::milliseconds` | Connection timeout (default: 30s) |
| `use_ssl` | `bool` | Enable SSL/TLS (default: false) |
| `ca_cert_path` | `std::optional<std::string>` | CA certificate path for verification |
| `verify_certificate` | `bool` | Verify SSL certificate (default: true) |

| Server Config | Type | Description |
|--------------|------|-------------|
| `port` | `uint16_t` | Port number to listen on (required) |
| `server_id` | `std::string` | Server identifier (auto-generated if empty) |
| `use_ssl` | `bool` | Enable SSL/TLS (default: false) |
| `cert_path` | `std::optional<std::string>` | Server certificate path (required if use_ssl=true) |
| `key_path` | `std::optional<std::string>` | Server private key path (required if use_ssl=true) |
| `tls_version` | `std::optional<std::string>` | TLS version (default: TLS 1.2+) |

---

### UDP Facade

**Header:** `<kcenon/network/facade/udp_facade.h>`

Simplified API for connectionless UDP clients and servers.

```cpp
udp_facade facade;

// UDP client
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 5555,
    .client_id = "my-udp-client"
});

// UDP server
auto server = facade.create_server({
    .port = 5555,
    .server_id = "my-udp-server"
});
```

**Configuration Options:**

| Client Config | Type | Description |
|--------------|------|-------------|
| `host` | `std::string` | Target hostname or IP address (required) |
| `port` | `uint16_t` | Target port number (required) |
| `client_id` | `std::string` | Client identifier (auto-generated if empty) |

| Server Config | Type | Description |
|--------------|------|-------------|
| `port` | `uint16_t` | Port number to listen on (required) |
| `server_id` | `std::string` | Server identifier (auto-generated if empty) |

---

### WebSocket Facade

**Header:** `<kcenon/network/facade/websocket_facade.h>`

Simplified API for WebSocket clients and servers (RFC 6455 compliant).

```cpp
websocket_facade facade;

// WebSocket client
auto client = facade.create_client({
    .client_id = "my-ws-client",
    .ping_interval = std::chrono::seconds(30)
});

// WebSocket server
auto server = facade.create_server({
    .port = 8080,
    .path = "/ws",
    .server_id = "my-ws-server"
});
```

**Configuration Options:**

| Client Config | Type | Description |
|--------------|------|-------------|
| `client_id` | `std::string` | Client identifier (auto-generated if empty) |
| `ping_interval` | `std::chrono::milliseconds` | Ping interval (default: 30s) |

| Server Config | Type | Description |
|--------------|------|-------------|
| `port` | `uint16_t` | Port number to listen on (required) |
| `path` | `std::string` | WebSocket path (default: "/") |
| `server_id` | `std::string` | Server identifier (auto-generated if empty) |

**Note:** For text messages or WebSocket-specific features (fragmentation, extensions), cast to `i_websocket_client`/`i_websocket_server` or use direct classes.

---

### HTTP Facade

**Header:** `<kcenon/network/facade/http_facade.h>`

Simplified API for HTTP/1.1 clients and servers.

```cpp
http_facade facade;

// HTTP client
auto client = facade.create_client({
    .client_id = "my-http-client",
    .timeout = std::chrono::seconds(10),
    .use_ssl = false,
    .path = "/"
});

// HTTP server
auto server = facade.create_server({
    .port = 8080,
    .server_id = "my-http-server"
});
```

**Configuration Options:**

| Client Config | Type | Description |
|--------------|------|-------------|
| `client_id` | `std::string` | Client identifier (auto-generated if empty) |
| `timeout` | `std::chrono::milliseconds` | Request timeout (default: 30s) |
| `use_ssl` | `bool` | Enable HTTPS (default: false) |
| `path` | `std::string` | HTTP path (default: "/") |

| Server Config | Type | Description |
|--------------|------|-------------|
| `port` | `uint16_t` | Port number to listen on (required) |
| `server_id` | `std::string` | Server identifier (auto-generated if empty) |

**Note:** For advanced HTTP features (routing, cookies, multipart), use direct classes.

---

### QUIC Facade

**Header:** `<kcenon/network/facade/quic_facade.h>`

Simplified API for QUIC clients and servers (RFC 9000/9001/9002 compliant).

```cpp
quic_facade facade;

// QUIC client
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 4433,
    .client_id = "my-quic-client",
    .alpn = "h3",
    .verify_server = true,
    .enable_0rtt = false
});

// QUIC server
auto server = facade.create_server({
    .port = 4433,
    .server_id = "my-quic-server",
    .cert_path = "/path/to/cert.pem",
    .key_path = "/path/to/key.pem",
    .alpn = "h3",
    .max_connections = 10000
});
```

**Configuration Options:**

| Client Config | Type | Description |
|--------------|------|-------------|
| `host` | `std::string` | Server hostname or IP address (required) |
| `port` | `uint16_t` | Server port number (required) |
| `client_id` | `std::string` | Client identifier (auto-generated if empty) |
| `ca_cert_path` | `std::optional<std::string>` | CA certificate path for verification |
| `client_cert_path` | `std::optional<std::string>` | Client certificate for mutual TLS |
| `client_key_path` | `std::optional<std::string>` | Client private key for mutual TLS |
| `verify_server` | `bool` | Verify server certificate (default: true) |
| `alpn` | `std::string` | ALPN protocol identifier (e.g., "h3") |
| `max_idle_timeout_ms` | `uint64_t` | Maximum idle timeout (default: 30000ms) |
| `enable_0rtt` | `bool` | Enable 0-RTT early data (default: false) |

| Server Config | Type | Description |
|--------------|------|-------------|
| `port` | `uint16_t` | Port number to listen on (required) |
| `server_id` | `std::string` | Server identifier (auto-generated if empty) |
| `cert_path` | `std::string` | Server certificate path (required) |
| `key_path` | `std::string` | Server private key path (required) |
| `ca_cert_path` | `std::optional<std::string>` | CA certificate for client verification |
| `require_client_cert` | `bool` | Require client certificate (default: false) |
| `alpn` | `std::string` | ALPN protocol identifier |
| `max_idle_timeout_ms` | `uint64_t` | Maximum idle timeout (default: 30000ms) |
| `max_connections` | `size_t` | Maximum concurrent connections (default: 10000) |

**Note:** For QUIC-specific features (multi-stream, 0-RTT, connection migration), use direct classes.

---

## Common Interface

All facades return implementations of the unified protocol interfaces:

### `i_protocol_client`

```cpp
class i_protocol_client {
public:
    virtual auto start(std::string_view host, uint16_t port) -> Result<void> = 0;
    virtual auto stop() -> Result<void> = 0;
    virtual auto send(std::vector<uint8_t> data) -> Result<void> = 0;
    virtual auto is_running() const -> bool = 0;

    virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;
    virtual auto set_connected_callback(connected_callback_t callback) -> void = 0;
    virtual auto set_disconnected_callback(disconnected_callback_t callback) -> void = 0;
    virtual auto set_error_callback(error_callback_t callback) -> void = 0;
};
```

### `i_protocol_server`

```cpp
class i_protocol_server {
public:
    virtual auto start(uint16_t port) -> Result<void> = 0;
    virtual auto stop() -> Result<void> = 0;
    virtual auto send(session_id_t session_id, std::vector<uint8_t> data) -> Result<void> = 0;
    virtual auto is_running() const -> bool = 0;

    virtual auto set_receive_callback(server_receive_callback_t callback) -> void = 0;
    virtual auto set_connected_callback(session_callback_t callback) -> void = 0;
    virtual auto set_disconnected_callback(session_callback_t callback) -> void = 0;
    virtual auto set_error_callback(server_error_callback_t callback) -> void = 0;
};
```

### Protocol-Agnostic Usage

```cpp
void configure_client(std::shared_ptr<interfaces::i_protocol_client> client) {
    // Works with any protocol client created via facade
    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        std::cout << "Received " << data.size() << " bytes\n";
    });

    client->set_connected_callback([]() {
        std::cout << "Connected!\n";
    });

    client->set_disconnected_callback([]() {
        std::cout << "Disconnected!\n";
    });
}

// Can be used with any protocol
tcp_facade tcp;
auto tcp_client = tcp.create_client({.host = "127.0.0.1", .port = 8080});
configure_client(tcp_client);

udp_facade udp;
auto udp_client = udp.create_client({.host = "127.0.0.1", .port = 5555});
configure_client(udp_client);
```

---

## When to Use Direct Classes

While facades simplify common use cases, use direct protocol classes when you need:

### TCP-Specific Features
- **Connection pooling**: `tcp_facade::create_connection_pool()`
- **Advanced TLS configuration**: Custom cipher suites, certificate pinning
- **Session-level control**: Direct access to `messaging_session`

### WebSocket-Specific Features
- **Text message framing**: Send/receive text frames explicitly
- **Custom extensions**: WebSocket protocol extensions
- **Fragmentation control**: Manual frame fragmentation

### HTTP-Specific Features
- **Routing**: Request routing and path matching
- **Cookies**: Cookie parsing and management
- **Multipart forms**: File upload handling
- **Custom headers**: Fine-grained header control

### QUIC-Specific Features
- **Multi-stream**: Create and manage multiple streams per connection
- **Stream prioritization**: Set stream priority and dependencies
- **0-RTT resumption**: Advanced 0-RTT session resumption
- **Connection migration**: Handle IP address changes

### Example: Direct QUIC Usage

```cpp
#include <kcenon/network/core/messaging_quic_client.h>

auto client = std::make_shared<core::messaging_quic_client>("my-client");

// Create multiple streams
auto stream1 = client->create_stream();
auto stream2 = client->create_stream();

// Send data on specific streams
client->send_on_stream(stream1, data1);
client->send_on_stream(stream2, data2);

// Set stream priorities
client->set_stream_priority(stream1, 100);
client->set_stream_priority(stream2, 50);
```

---

## Complete Example

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/facade/udp_facade.h>
#include <iostream>
#include <thread>

using namespace kcenon::network;

int main() {
    // Create TCP server
    facade::tcp_facade tcp;
    auto server = tcp.create_server({.port = 8080});

    server->set_receive_callback([&](auto session_id, const auto& data) {
        std::cout << "TCP received " << data.size() << " bytes from "
                  << session_id << "\n";
        // Echo back
        server->send(session_id, data);
    });

    auto result = server->start(8080);
    if (result.is_err()) {
        std::cerr << "Failed to start server: "
                  << result.error().message << "\n";
        return -1;
    }

    // Create TCP client
    auto client = tcp.create_client({
        .host = "127.0.0.1",
        .port = 8080,
        .timeout = std::chrono::seconds(5)
    });

    client->set_receive_callback([](const auto& data) {
        std::cout << "TCP client received " << data.size() << " bytes\n";
    });

    result = client->start("127.0.0.1", 8080);
    if (result.is_err()) {
        std::cerr << "Failed to connect: "
                  << result.error().message << "\n";
        return -1;
    }

    // Send test message
    std::string message = "Hello, Facade API!";
    std::vector<uint8_t> data(message.begin(), message.end());

    result = client->send(std::move(data));
    if (result.is_err()) {
        std::cerr << "Failed to send: "
                  << result.error().message << "\n";
        return -1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    client->stop();
    server->stop();

    return 0;
}
```

---

## Migration Guide

For detailed instructions on migrating from direct template usage to facades, see:

📖 **[Facade Migration Guide](migration-guide.md)**

---

## See Also

- [Architecture Documentation](../ARCHITECTURE.md) - System architecture overview
- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [Protocol Guides](../guides/) - Protocol-specific usage guides
- [Interface Documentation](../../include/kcenon/network/interfaces/) - Interface definitions

---

**Last Updated:** 2025-02-04
**Maintainer:** kcenon@naver.com
