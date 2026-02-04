# Facade Pattern Migration Guide

> **Version:** 2.0.0
> **Last Updated:** 2025-02-04
> **Audience:** Developers migrating from direct template usage to Facade API

## Overview

This guide provides step-by-step instructions for migrating from direct protocol template instantiation to the simplified Facade API. The migration is designed to be incremental and backward-compatible.

### Why Migrate?

| Benefit | Description |
|---------|-------------|
| **Simplified Code** | Remove template parameters and protocol tags |
| **Reduced Compile Time** | Fewer template instantiations |
| **Better Readability** | Clear, declarative configuration |
| **Easier Testing** | Protocol-agnostic interfaces for mocking |
| **Future-Proof** | Facade API will be stable across versions |

### Migration Strategy

✅ **Incremental:** Migrate one component at a time
✅ **Backward Compatible:** Old and new APIs coexist
✅ **Low Risk:** No behavior changes, only API simplification

---

## Migration Steps

### Step 1: Identify Direct Template Usage

Look for code patterns like:

```cpp
// Pattern 1: Direct template instantiation
auto client = std::make_shared<messaging_client<tcp_policy>>("client-id");

// Pattern 2: Template with multiple policies
auto server = std::make_shared<messaging_server<tcp_policy, ssl_policy>>("server-id");

// Pattern 3: Custom policy configurations
auto client = std::make_shared<messaging_client<
    tcp_policy,
    custom_buffer_policy,
    custom_thread_policy
>>("client-id");
```

### Step 2: Choose Appropriate Facade

| Protocol | Old Class | Facade |
|----------|-----------|--------|
| TCP | `messaging_client<tcp_policy>` | `tcp_facade` |
| UDP | `messaging_udp_client` | `udp_facade` |
| WebSocket | `messaging_ws_client` | `websocket_facade` |
| HTTP | `http_client` | `http_facade` |
| QUIC | `messaging_quic_client` | `quic_facade` |

### Step 3: Update Includes

Replace direct class includes with facade headers:

**Before:**
```cpp
#include <kcenon/network/core/messaging_client.h>
#include <kcenon/network/policies/tcp_policy.h>
```

**After:**
```cpp
#include <kcenon/network/facade/tcp_facade.h>
```

### Step 4: Update Instantiation Code

Replace template instantiation with facade creation:

**Before:**
```cpp
auto client = std::make_shared<
    messaging_client<tcp_policy>
>("my-client");
```

**After:**
```cpp
tcp_facade facade;
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "my-client"
});
```

### Step 5: Update Configuration

Move initialization parameters to configuration struct:

**Before:**
```cpp
client->set_timeout(std::chrono::seconds(30));
client->enable_ssl(true);
client->set_verify_certificate(true);
```

**After:**
```cpp
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .timeout = std::chrono::seconds(30),
    .use_ssl = true,
    .verify_certificate = true
});
```

### Step 6: Test Incrementally

1. Compile and verify no errors
2. Run existing unit tests
3. Validate functionality is unchanged
4. Check performance metrics (should be identical)

---

## Before/After Examples

### TCP Client Migration

#### Before (Direct Template)

```cpp
#include <kcenon/network/core/messaging_client.h>
#include <kcenon/network/policies/tcp_policy.h>

using namespace kcenon::network;

auto create_client() -> std::shared_ptr<core::messaging_client<policies::tcp_policy>> {
    auto client = std::make_shared<core::messaging_client<policies::tcp_policy>>("my-client");

    // Configuration via method calls
    client->set_connection_timeout(std::chrono::seconds(30));

    // Callback registration
    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        // Handle received data
    });

    // Start connection
    auto result = client->start_client("127.0.0.1", 8080);
    if (!result.is_ok()) {
        throw std::runtime_error(result.error().message);
    }

    return client;
}
```

#### After (Facade API)

```cpp
#include <kcenon/network/facade/tcp_facade.h>

using namespace kcenon::network;

auto create_client() -> std::shared_ptr<interfaces::i_protocol_client> {
    facade::tcp_facade facade;

    // Declarative configuration
    auto client = facade.create_client({
        .host = "127.0.0.1",
        .port = 8080,
        .client_id = "my-client",
        .timeout = std::chrono::seconds(30)
    });

    // Callback registration (unchanged)
    client->set_receive_callback([](const std::vector<uint8_t>& data) {
        // Handle received data
    });

    // Start connection
    auto result = client->start("127.0.0.1", 8080);
    if (result.is_err()) {
        throw std::runtime_error(result.error().message);
    }

    return client;
}
```

**Benefits:**
- ✅ No template parameters
- ✅ Clear configuration struct
- ✅ Unified interface return type
- ✅ Same functionality, cleaner code

---

### Secure TCP Server Migration

#### Before (Direct Template with SSL)

```cpp
#include <kcenon/network/core/secure_messaging_server.h>
#include <kcenon/network/policies/ssl_policy.h>

using namespace kcenon::network;

auto create_secure_server() {
    auto server = std::make_shared<core::secure_messaging_server>("my-server");

    // SSL configuration via multiple calls
    server->set_certificate_file("/path/to/cert.pem");
    server->set_private_key_file("/path/to/key.pem");
    server->set_tls_version("TLS1.3");

    // Callback registration
    server->set_receive_callback([](auto session_id, const auto& data) {
        // Handle received data from session
    });

    server->set_connected_callback([](auto session_id) {
        std::cout << "Client connected: " << session_id << "\n";
    });

    // Start server
    auto result = server->start_server(8443);
    if (!result.is_ok()) {
        throw std::runtime_error(result.error().message);
    }

    return server;
}
```

#### After (Facade API with SSL)

```cpp
#include <kcenon/network/facade/tcp_facade.h>

using namespace kcenon::network;

auto create_secure_server() -> std::shared_ptr<interfaces::i_protocol_server> {
    facade::tcp_facade facade;

    // Declarative SSL configuration
    auto server = facade.create_server({
        .port = 8443,
        .server_id = "my-server",
        .use_ssl = true,
        .cert_path = "/path/to/cert.pem",
        .key_path = "/path/to/key.pem",
        .tls_version = "TLS1.3"
    });

    // Callback registration (unchanged)
    server->set_receive_callback([](auto session_id, const auto& data) {
        // Handle received data from session
    });

    server->set_connected_callback([](auto session_id) {
        std::cout << "Client connected: " << session_id << "\n";
    });

    // Start server
    auto result = server->start(8443);
    if (result.is_err()) {
        throw std::runtime_error(result.error().message);
    }

    return server;
}
```

**Benefits:**
- ✅ All SSL config in one place
- ✅ No separate `secure_messaging_server` class needed
- ✅ SSL enabled via `use_ssl` flag
- ✅ Same security, simpler setup

---

### UDP Client/Server Migration

#### Before (Direct Class)

```cpp
#include <kcenon/network/udp/messaging_udp_client.h>
#include <kcenon/network/udp/messaging_udp_server.h>

using namespace kcenon::network;

auto create_udp_components() {
    // Client
    auto client = std::make_shared<udp::messaging_udp_client>("udp-client");
    client->start_client("127.0.0.1", 5555);

    // Server
    auto server = std::make_shared<udp::messaging_udp_server>("udp-server");
    server->start_server(5555);

    return std::make_pair(client, server);
}
```

#### After (Facade API)

```cpp
#include <kcenon/network/facade/udp_facade.h>

using namespace kcenon::network;

auto create_udp_components() {
    facade::udp_facade facade;

    // Client
    auto client = facade.create_client({
        .host = "127.0.0.1",
        .port = 5555,
        .client_id = "udp-client"
    });
    client->start("127.0.0.1", 5555);

    // Server
    auto server = facade.create_server({
        .port = 5555,
        .server_id = "udp-server"
    });
    server->start(5555);

    return std::make_pair(client, server);
}
```

**Benefits:**
- ✅ Consistent API with TCP/WebSocket/etc.
- ✅ Protocol-agnostic return types
- ✅ Easier to switch protocols

---

### WebSocket Migration

#### Before (Direct Class)

```cpp
#include <kcenon/network/websocket/messaging_ws_client.h>

using namespace kcenon::network;

auto create_websocket_client() {
    auto client = std::make_shared<websocket::messaging_ws_client>("ws-client");

    // WebSocket-specific configuration
    client->set_ping_interval(std::chrono::seconds(30));
    client->set_auto_reconnect(true);

    // Start connection
    client->start_client("ws://127.0.0.1:8080/ws");

    return client;
}
```

#### After (Facade API)

```cpp
#include <kcenon/network/facade/websocket_facade.h>

using namespace kcenon::network;

auto create_websocket_client() -> std::shared_ptr<interfaces::i_protocol_client> {
    facade::websocket_facade facade;

    // Declarative configuration
    auto client = facade.create_client({
        .client_id = "ws-client",
        .ping_interval = std::chrono::seconds(30)
    });

    // Start connection
    client->start("127.0.0.1", 8080);

    return client;
}
```

**Note:** For WebSocket-specific features (text frames, fragmentation), cast to `i_websocket_client` or use direct classes.

---

### QUIC Migration

#### Before (Direct Class)

```cpp
#include <kcenon/network/quic/messaging_quic_client.h>

using namespace kcenon::network;

auto create_quic_client() {
    auto client = std::make_shared<quic::messaging_quic_client>("quic-client");

    // QUIC configuration
    client->set_alpn("h3");
    client->set_max_idle_timeout(30000);
    client->enable_0rtt(false);

    // TLS configuration
    client->set_verify_server(true);
    client->set_ca_cert_path("/path/to/ca.pem");

    // Start connection
    client->start_client("127.0.0.1", 4433);

    return client;
}
```

#### After (Facade API)

```cpp
#include <kcenon/network/facade/quic_facade.h>

using namespace kcenon::network;

auto create_quic_client() -> std::shared_ptr<interfaces::i_protocol_client> {
    facade::quic_facade facade;

    // Declarative QUIC + TLS configuration
    auto client = facade.create_client({
        .host = "127.0.0.1",
        .port = 4433,
        .client_id = "quic-client",
        .ca_cert_path = "/path/to/ca.pem",
        .verify_server = true,
        .alpn = "h3",
        .max_idle_timeout_ms = 30000,
        .enable_0rtt = false
    });

    // Start connection
    client->start("127.0.0.1", 4433);

    return client;
}
```

**Note:** For QUIC-specific features (multi-stream, stream priorities), use direct classes.

---

## Common Migration Patterns

### Pattern 1: Factory Functions

**Before:**
```cpp
template <typename Policy>
auto create_client(const std::string& id, const std::string& host, uint16_t port) {
    auto client = std::make_shared<messaging_client<Policy>>(id);
    client->start_client(host, port);
    return client;
}

// Usage
auto tcp_client = create_client<tcp_policy>("client", "127.0.0.1", 8080);
auto udp_client = create_client<udp_policy>("client", "127.0.0.1", 5555);
```

**After:**
```cpp
auto create_tcp_client(const std::string& id, const std::string& host, uint16_t port) {
    tcp_facade facade;
    auto client = facade.create_client({.host = host, .port = port, .client_id = id});
    client->start(host, port);
    return client;
}

auto create_udp_client(const std::string& id, const std::string& host, uint16_t port) {
    udp_facade facade;
    auto client = facade.create_client({.host = host, .port = port, .client_id = id});
    client->start(host, port);
    return client;
}
```

**Or with unified interface:**
```cpp
enum class protocol { tcp, udp, websocket };

auto create_client(protocol proto, const std::string& id,
                   const std::string& host, uint16_t port)
    -> std::shared_ptr<interfaces::i_protocol_client>
{
    std::shared_ptr<interfaces::i_protocol_client> client;

    switch (proto) {
    case protocol::tcp: {
        tcp_facade facade;
        client = facade.create_client({.host = host, .port = port, .client_id = id});
        break;
    }
    case protocol::udp: {
        udp_facade facade;
        client = facade.create_client({.host = host, .port = port, .client_id = id});
        break;
    }
    case protocol::websocket: {
        websocket_facade facade;
        client = facade.create_client({.client_id = id});
        break;
    }
    }

    client->start(host, port);
    return client;
}
```

### Pattern 2: Configuration Builder

**Before:**
```cpp
class ClientBuilder {
    std::string host_;
    uint16_t port_;
    std::chrono::milliseconds timeout_ = std::chrono::seconds(30);
    bool use_ssl_ = false;

public:
    auto host(std::string h) -> ClientBuilder& { host_ = std::move(h); return *this; }
    auto port(uint16_t p) -> ClientBuilder& { port_ = p; return *this; }
    auto timeout(auto t) -> ClientBuilder& { timeout_ = t; return *this; }
    auto ssl(bool s) -> ClientBuilder& { use_ssl_ = s; return *this; }

    auto build() {
        auto client = use_ssl_
            ? std::make_shared<secure_messaging_client>("client")
            : std::make_shared<messaging_client<tcp_policy>>("client");

        // Configure client...
        return client;
    }
};

// Usage
auto client = ClientBuilder()
    .host("127.0.0.1")
    .port(8080)
    .timeout(std::chrono::seconds(10))
    .ssl(true)
    .build();
```

**After:**
```cpp
// Facade config struct is already a builder pattern
tcp_facade facade;
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .timeout = std::chrono::seconds(10),
    .use_ssl = true
});

// Or with a helper function
auto build_client(auto&&... args) {
    tcp_facade facade;
    return facade.create_client(tcp_facade::client_config{
        std::forward<decltype(args)>(args)...
    });
}
```

### Pattern 3: Protocol Abstraction

**Before:**
```cpp
template <typename ClientType>
class ConnectionManager {
    std::shared_ptr<ClientType> client_;

public:
    explicit ConnectionManager(std::shared_ptr<ClientType> client)
        : client_(std::move(client)) {}

    void send_data(const std::vector<uint8_t>& data) {
        client_->send_packet(data);
    }
};

// Usage requires template parameter
ConnectionManager<messaging_client<tcp_policy>> tcp_mgr(tcp_client);
ConnectionManager<messaging_udp_client> udp_mgr(udp_client);
```

**After:**
```cpp
// No template needed - use common interface
class ConnectionManager {
    std::shared_ptr<interfaces::i_protocol_client> client_;

public:
    explicit ConnectionManager(std::shared_ptr<interfaces::i_protocol_client> client)
        : client_(std::move(client)) {}

    void send_data(const std::vector<uint8_t>& data) {
        client_->send(data);  // Unified interface method
    }
};

// Usage is protocol-agnostic
tcp_facade tcp;
auto tcp_client = tcp.create_client({...});
ConnectionManager tcp_mgr(tcp_client);

udp_facade udp;
auto udp_client = udp.create_client({...});
ConnectionManager udp_mgr(udp_client);  // Same class!
```

---

## Troubleshooting

### Issue 1: Compile Errors After Migration

**Symptom:**
```
error: no matching function for call to 'start_client'
```

**Cause:** Method names changed in unified interface.

**Solution:**
- `start_client()` → `start()`
- `start_server()` → `start()`
- `send_packet()` → `send()`
- `stop_client()` → `stop()`
- `stop_server()` → `stop()`

### Issue 2: Missing Protocol-Specific Features

**Symptom:**
```
error: 'class i_protocol_client' has no member named 'create_stream'
```

**Cause:** QUIC/WebSocket-specific features not in unified interface.

**Solution:** Cast to protocol-specific interface or use direct classes:

```cpp
// Option 1: Cast to protocol-specific interface
auto quic_client = std::dynamic_pointer_cast<interfaces::i_quic_client>(client);
if (quic_client) {
    auto stream = quic_client->create_stream();
}

// Option 2: Use direct class
#include <kcenon/network/quic/messaging_quic_client.h>
auto quic_client = std::make_shared<quic::messaging_quic_client>("client");
auto stream = quic_client->create_stream();
```

### Issue 3: Performance Regression

**Symptom:** Slower performance after migration.

**Cause:** Unlikely - facade is zero-cost abstraction. Check:
- Debug builds instead of Release
- Missing compiler optimizations
- Changed configuration (timeout, buffer sizes, etc.)

**Solution:**
```bash
# Verify optimization level
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Run benchmarks
./build/benchmarks/network_benchmarks
```

### Issue 4: Linking Errors

**Symptom:**
```
undefined reference to `tcp_facade::create_client'
```

**Cause:** Missing library linkage.

**Solution:** Add facade library to CMakeLists.txt:

```cmake
target_link_libraries(my_app PRIVATE
    kcenon::network-core
    kcenon::network-tcp    # For tcp_facade
    kcenon::network-udp    # For udp_facade
    # ... other protocols as needed
)
```

---

## Checklist

Use this checklist to track migration progress:

### Code Changes
- [ ] Updated `#include` directives
- [ ] Replaced template instantiations with facade calls
- [ ] Migrated configuration to config structs
- [ ] Updated method names (`start_client` → `start`, etc.)
- [ ] Replaced protocol-specific types with `i_protocol_client`/`i_protocol_server`

### Testing
- [ ] Compiled without warnings
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Performance benchmarks show no regression
- [ ] Manual testing of client/server communication

### Documentation
- [ ] Updated code comments
- [ ] Updated README/documentation
- [ ] Added migration notes to CHANGELOG
- [ ] Updated API examples

### Validation
- [ ] Code review completed
- [ ] CI/CD pipeline passes
- [ ] No new sanitizer warnings (ASAN, TSAN, UBSAN)
- [ ] Memory profiling shows no leaks

---

## Next Steps

After successful migration:

1. **Review Protocol-Specific Features**: Identify areas where direct classes are still needed
2. **Update Documentation**: Ensure all examples use Facade API
3. **Train Team**: Share migration experience with team members
4. **Monitor Production**: Watch for any unexpected behavior changes

---

## Getting Help

If you encounter issues during migration:

1. Check [Troubleshooting](#troubleshooting) section above
2. Review [Facade API Documentation](README.md)
3. Search [GitHub Issues](https://github.com/kcenon/network_system/issues)
4. Ask on [GitHub Discussions](https://github.com/kcenon/network_system/discussions)

---

**Last Updated:** 2025-02-04
**Maintainer:** kcenon@naver.com
