# API Quick Reference -- network_system

Cheat-sheet for the most common `network_system` APIs.
For full details see [API_REFERENCE.md](API_REFERENCE.md) and the Doxygen-generated docs.

---

## Header and Namespace

```cpp
// Facade API (recommended)
#include <kcenon/network/facade/tcp_facade.h>
#include <kcenon/network/facade/udp_facade.h>
#include <kcenon/network/facade/websocket_facade.h>
#include <kcenon/network/facade/http_facade.h>

using namespace kcenon::network::facade;
using namespace kcenon::network::interfaces;
```

---

## Facade API

All facades follow the same pattern: `create_client(config)` / `create_server(config)`.

### TCP

```cpp
tcp_facade facade;

auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "my-client",
    .timeout = std::chrono::seconds(30),
    .use_ssl = false
});

auto server = facade.create_server({
    .port = 8080,
    .use_ssl = false
});
```

### UDP

```cpp
udp_facade facade;

auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 5555,
    .client_id = "udp-client"
});

auto server = facade.create_server({
    .port = 5555,
    .server_id = "udp-server"
});
```

### WebSocket

```cpp
websocket_facade facade;

auto client = facade.create_client({
    .client_id = "ws-client",
    .ping_interval = std::chrono::seconds(30)
});

auto server = facade.create_server({
    .port = 9090,
    .path = "/ws",
    .server_id = "ws-server"
});
```

### HTTP

```cpp
http_facade facade;

auto client = facade.create_client({
    .client_id = "http-client",
    .timeout = std::chrono::seconds(10),
    .use_ssl = false,
    .path = "/"
});

auto server = facade.create_server({
    .port = 8080,
    .server_id = "http-server"
});
```

---

## Protocol Creation (factory functions)

For advanced use without facades:

```cpp
#include <kcenon/network/detail/protocol/tcp.h>
#include <kcenon/network/detail/protocol/udp.h>
#include <kcenon/network/detail/protocol/websocket.h>

// Protocol tags carry compile-time metadata
// tcp_protocol::name, tcp_protocol::is_connection_oriented, tcp_protocol::is_reliable
```

---

## Client Lifecycle

```cpp
// Start (connect)
auto result = client->start("127.0.0.1", 8080);  // VoidResult

// Send data
std::vector<uint8_t> data = {0x01, 0x02};
client->send(std::move(data));                     // VoidResult

// Check state
client->is_connected();                            // bool
client->is_running();                              // bool

// Stop
client->stop();                                    // VoidResult
```

---

## Session Management

Sessions represent individual client connections on the server side.

```cpp
// Session interface (i_session)
session->id();                      // string_view -- unique session ID
session->is_connected();            // bool
session->send(std::move(data));     // VoidResult
session->close();                   // VoidResult
```

### Callbacks (simple)

```cpp
// Client callbacks
client->set_on_data([](const std::vector<uint8_t>& data) { /* ... */ });
client->set_on_connected([] { /* ... */ });
client->set_on_disconnected([] { /* ... */ });
client->set_on_error([](std::error_code ec) { /* ... */ });

// Server callbacks
server->set_on_data([](auto session, const auto& data) { /* ... */ });
server->set_on_connected([](auto session) { /* ... */ });
server->set_on_disconnected([](auto session) { /* ... */ });
```

### Observer Pattern (recommended)

```cpp
class my_observer : public connection_observer {
    void on_connected(std::shared_ptr<i_session> s) override;
    void on_disconnected(std::shared_ptr<i_session> s) override;
    void on_data(std::shared_ptr<i_session> s,
                 const std::vector<uint8_t>& data) override;
    void on_error(std::error_code ec) override;
};

auto obs = std::make_shared<my_observer>();
client->set_observer(obs);
server->set_observer(obs);
```

---

## TLS Policies

### Via Facade Config

```cpp
// Secure client
auto client = tcp_facade.create_client({
    .host = "example.com",
    .port = 8443,
    .use_ssl = true,
    .ca_cert_path = "/path/to/ca.pem",
    .verify_ssl = true
});

// Secure server
auto server = tcp_facade.create_server({
    .port = 8443,
    .use_ssl = true,
    .cert_path = "/path/to/cert.pem",
    .key_path = "/path/to/key.pem"
});
```

### Via Unified Templates (advanced)

```cpp
#include <kcenon/network/detail/protocol/tcp.h>

// Template with TLS policy
unified_messaging_client<tcp_protocol, tls_enabled_policy> client;
unified_messaging_server<tcp_protocol, tls_enabled_policy> server;
```

---

## Connection Pool

```cpp
#include "internal/core/connection_pool.h"

auto pool = std::make_shared<kcenon::network::core::connection_pool>(
    "localhost",                          // host
    8080,                                 // port
    10,                                   // pool_size
    std::chrono::seconds(30)              // acquire_timeout
);

auto result = pool->initialize();         // VoidResult

// Acquire a connection
auto client = pool->acquire();            // blocks if all in use
client->send(data);

// Release back to pool
pool->release(std::move(client));
```

---

## Unified Messaging (template API)

For direct template usage without facades:

```cpp
#include <kcenon/network/detail/session/messaging_session.h>

// unified_messaging_client<Protocol, TlsPolicy>
// unified_messaging_server<Protocol, TlsPolicy>

// Plain TCP
unified_messaging_client<tcp_protocol, tls_disabled_policy> client;
client.start("127.0.0.1", 8080);

// TLS TCP
unified_messaging_client<tcp_protocol, tls_enabled_policy> secure_client;
secure_client.start("example.com", 8443);
```

---

## Supported Protocols

| Protocol | Facade | Connection-oriented | Reliable | TLS support |
|---|---|---|---|---|
| TCP | `tcp_facade` | Yes | Yes | Yes |
| UDP | `udp_facade` | No | No | DTLS |
| WebSocket | `websocket_facade` | Yes | Yes | WSS |
| HTTP/1.1 | `http_facade` | Yes | Yes | HTTPS |
| QUIC | `quic_facade` | Yes | Yes | Built-in |

---

## Quick Recipe

```cpp
#include <kcenon/network/facade/tcp_facade.h>
using namespace kcenon::network::facade;

// Create server
tcp_facade facade;
auto server = facade.create_server({.port = 8080});
server->set_on_data([](auto session, const auto& data) {
    session->send(std::vector<uint8_t>(data.begin(), data.end()));
});
server->start();

// Create client
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "test"
});
client->set_on_data([](const auto& data) {
    // handle echo response
});
client->start("127.0.0.1", 8080);

std::string msg = "ping";
client->send(std::vector<uint8_t>(msg.begin(), msg.end()));
```
