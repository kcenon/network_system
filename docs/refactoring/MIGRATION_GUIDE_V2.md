# Migration Guide: network_system v1.x → v2.0

> **Version**: 2.0.0
> **Issue**: #610 (Part of Epic #577)
> **Breaking Changes**: Yes - Core implementation headers moved to internal
> **Migration Difficulty**: Low to Medium

## Overview

network_system v2.0 introduces a **facade-first API** that significantly simplifies usage by hiding implementation details. This guide helps you migrate from direct implementation usage to the new facade API.

### What Changed

| Change | Impact |
|--------|--------|
| **Reduced public headers** | 153 → 35 headers (77% reduction) |
| **Facade-based API** | Primary way to create clients/servers |
| **Internal implementations** | Moved to `src/internal/` |
| **Cleaner includes** | Only include what you need |

### Benefits

✅ **Simpler API**: No template parameters or protocol tags
✅ **Faster compilation**: Fewer header dependencies
✅ **Easier upgrades**: Implementation changes don't affect your code
✅ **Better docs**: Clear public API surface

---

## Quick Migration Reference

| Old (v1.x) | New (v2.0) |
|------------|------------|
| `#include "kcenon/network/core/messaging_client.h"` | `#include "kcenon/network/facade/tcp_facade.h"` |
| `messaging_client<...> client(...)` | `tcp_facade facade; facade.create_client(...)` |
| Direct template instantiation | Factory method with config struct |

---

## Detailed Migration Examples

### 1. TCP Client Migration

#### Before (v1.x)
```cpp
#include "kcenon/network/core/messaging_client.h"
#include "kcenon/network/protocol/tcp_tag.h"

// Complex template instantiation
using tcp_client = kcenon::network::messaging_client<
    kcenon::network::protocol::tcp_tag,
    kcenon::network::tls_policy::no_tls
>;

auto client = std::make_shared<tcp_client>(
    "my-client",
    "127.0.0.1",
    8080,
    std::chrono::seconds(30)
);

client->start();
```

#### After (v2.0) ✅
```cpp
#include "kcenon/network/facade/tcp_facade.h"

using namespace kcenon::network::facade;

// Simple factory with clear configuration
tcp_facade facade;
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 8080,
    .client_id = "my-client",
    .timeout = std::chrono::seconds(30)
});

client->start();
```

**Changes**:
- No template parameters needed
- Clear configuration struct
- Same `IProtocolClient` interface
- Cleaner, more readable code

---

### 2. Secure TCP Client Migration

#### Before (v1.x)
```cpp
#include "kcenon/network/core/secure_messaging_client.h"

auto secure_client = std::make_shared<
    kcenon::network::secure_messaging_client
>(
    "secure-client",
    "example.com",
    8443,
    std::chrono::seconds(30),
    true  // verify certificate
);

secure_client->start();
```

#### After (v2.0) ✅
```cpp
#include "kcenon/network/facade/tcp_facade.h"

tcp_facade facade;
auto secure_client = facade.create_client({
    .host = "example.com",
    .port = 8443,
    .client_id = "secure-client",
    .timeout = std::chrono::seconds(30),
    .use_ssl = true,                    // Enable SSL/TLS
    .verify_certificate = true,
    .ca_cert_path = "/path/to/ca.pem"  // Optional
});

secure_client->start();
```

**Benefits**:
- Same facade for both plain and secure clients
- Self-documenting configuration
- Optional SSL/TLS parameters

---

### 3. TCP Server Migration

#### Before (v1.x)
```cpp
#include "kcenon/network/core/messaging_server.h"

auto server = std::make_shared<
    kcenon::network::messaging_server<
        kcenon::network::protocol::tcp_tag,
        kcenon::network::tls_policy::no_tls
    >
>("my-server", 8080);

server->start();
```

#### After (v2.0) ✅
```cpp
#include "kcenon/network/facade/tcp_facade.h"

tcp_facade facade;
auto server = facade.create_server({
    .port = 8080,
    .server_id = "my-server"
});

server->start();
```

---

### 4. UDP Client/Server Migration

#### Before (v1.x)
```cpp
#include "kcenon/network/core/messaging_udp_client.h"
#include "kcenon/network/core/messaging_udp_server.h"

auto udp_client = std::make_shared<
    kcenon::network::messaging_udp_client
>("udp-client", "127.0.0.1", 9090);

auto udp_server = std::make_shared<
    kcenon::network::messaging_udp_server
>("udp-server", 9090);
```

#### After (v2.0) ✅
```cpp
#include "kcenon/network/facade/udp_facade.h"

udp_facade facade;

auto udp_client = facade.create_client({
    .host = "127.0.0.1",
    .port = 9090,
    .client_id = "udp-client"
});

auto udp_server = facade.create_server({
    .port = 9090,
    .server_id = "udp-server"
});
```

---

### 5. HTTP Client/Server Migration

#### Before (v1.x)
```cpp
#include "kcenon/network/http/http_client.h"
#include "kcenon/network/http/http_server.h"

auto http_client = std::make_shared<
    kcenon::network::http::http_client
>("http-client", "api.example.com", 80);

auto http_server = std::make_shared<
    kcenon::network::http::http_server
>(8080);
```

#### After (v2.0) ✅
```cpp
#include "kcenon/network/facade/http_facade.h"

http_facade facade;

auto http_client = facade.create_client({
    .host = "api.example.com",
    .port = 80,
    .client_id = "http-client"
});

auto http_server = facade.create_server({
    .port = 8080
});
```

---

### 6. WebSocket Migration

#### Before (v1.x)
```cpp
#include "kcenon/network/http/websocket_client.h"
#include "kcenon/network/http/websocket_server.h"

auto ws_client = std::make_shared<
    kcenon::network::http::websocket_client
>("ws://localhost:8080/socket");

auto ws_server = std::make_shared<
    kcenon::network::http::websocket_server
>(8080, "/socket");
```

#### After (v2.0) ✅
```cpp
#include "kcenon/network/facade/websocket_facade.h"

websocket_facade facade;

auto ws_client = facade.create_client({
    .host = "localhost",
    .port = 8080,
    .path = "/socket",
    .client_id = "ws-client"
});

auto ws_server = facade.create_server({
    .port = 8080,
    .endpoint = "/socket"
});
```

---

## Migration Strategies

### Strategy 1: Immediate Migration (Recommended)

**Best for**: New projects, actively maintained code

```cpp
// Replace old includes with facade includes
- #include "kcenon/network/core/messaging_client.h"
+ #include "kcenon/network/facade/tcp_facade.h"

// Update instantiation code
- auto client = std::make_shared<messaging_client<tcp_tag, no_tls>>(...);
+ tcp_facade facade;
+ auto client = facade.create_client({...});
```

**Pros**:
- Clean code
- Future-proof
- Better compile times

**Cons**:
- Requires code changes

---

### Strategy 2: Gradual Migration with Compatibility Mode

**Best for**: Large codebases, legacy systems

Enable internal header access during transition:

```cmake
# CMakeLists.txt
set(NETWORK_SYSTEM_EXPOSE_INTERNALS ON)
```

This allows old code to continue working:

```cpp
// Old code still works (temporarily)
#include "kcenon/network/core/messaging_client.h"
// ⚠️ Deprecated: Will be removed in v3.0

// New code uses facades
#include "kcenon/network/facade/tcp_facade.h"
```

**Pros**:
- No immediate code changes required
- Migrate incrementally
- Test both old and new code paths

**Cons**:
- Longer compile times
- Must eventually migrate
- Deprecation warnings

---

### Strategy 3: Advanced Access (Not Recommended)

**For**: Library developers, advanced users

Explicitly include internal headers:

```cpp
// Explicitly opt-in to internal headers
#include "src/internal/tcp/messaging_client.h"  // Not recommended

// Better: Use internal namespace hint
#include "kcenon/network/internal/tcp/messaging_client.h"  // Future option
```

**Warning**: Internal headers may change without notice. Use at your own risk.

---

## Interface Compatibility

The return types from facades implement the same interfaces as before:

```cpp
// Before
std::shared_ptr<messaging_client<tcp_tag, no_tls>> client;

// After
std::shared_ptr<interfaces::i_protocol_client> client;

// Both support the same operations
client->start();
client->stop();
client->send(data);
client->set_received_callback([](auto data) { /* ... */ });
```

**No changes needed** if you were coding to interfaces (recommended).

---

## Common Migration Patterns

### Pattern 1: Client Creation and Configuration

```cpp
// Old: Scattered configuration
messaging_client<tcp_tag, no_tls> client("id", "host", 8080, timeout);
client.set_reconnect_policy(policy);
client.set_retry_count(3);

// New: Centralized configuration
tcp_facade::client_config config{
    .host = "host",
    .port = 8080,
    .client_id = "id",
    .timeout = timeout,
    .reconnect_on_failure = true,
    .max_retries = 3
};
auto client = tcp_facade{}.create_client(config);
```

### Pattern 2: Factory Pattern

```cpp
// Old: Manual factory
template<typename ProtocolTag, typename TlsPolicy>
auto create_client(...) {
    return std::make_shared<messaging_client<ProtocolTag, TlsPolicy>>(...);
}

// New: Built-in factory
auto create_tcp_client(const std::string& host, uint16_t port) {
    return tcp_facade{}.create_client({.host = host, .port = port});
}
```

### Pattern 3: Testing

```cpp
// Old: Mock concrete class
class mock_messaging_client : public messaging_client<tcp_tag, no_tls> { /*...*/ };

// New: Mock interface (cleaner)
class mock_protocol_client : public interfaces::i_protocol_client { /*...*/ };

// Inject via interface
void my_function(std::shared_ptr<interfaces::i_protocol_client> client);
```

---

## Troubleshooting

### Error: "Cannot find messaging_client.h"

**Cause**: Header moved to internal.

**Solution**: Use facade API:
```cpp
- #include "kcenon/network/core/messaging_client.h"
+ #include "kcenon/network/facade/tcp_facade.h"
```

### Error: "Incomplete type in template instantiation"

**Cause**: Implementation details now hidden.

**Solution**: Use facade factories instead of direct instantiation.

### Warning: "Deprecated header"

**Cause**: Using compatibility mode during transition.

**Solution**: Migrate to facade API to remove warnings.

---

## Checklist

Use this checklist to track migration progress:

- [ ] Inventory all direct includes of `core/*` headers
- [ ] Replace with appropriate facade includes
- [ ] Update client/server instantiation to use factories
- [ ] Update configuration code to use config structs
- [ ] Compile and fix any errors
- [ ] Run tests to verify behavior
- [ ] Remove `NETWORK_SYSTEM_EXPOSE_INTERNALS` flag
- [ ] Update documentation/comments
- [ ] Review deprecation warnings

---

## Getting Help

- **Documentation**: See facade header comments for detailed API documentation
- **Examples**: Check `examples/` directory for updated usage examples
- **Issues**: Report migration issues at https://github.com/kcenon/network_system/issues
- **Questions**: Tag issue #610 for migration-related questions

---

## Timeline

| Version | Status | Migration Action |
|---------|--------|------------------|
| **v1.x** | Current | No changes needed |
| **v2.0-beta** | Q1 2026 | Test migration with compatibility mode |
| **v2.0** | Q2 2026 | Migrate to facade API (recommended) |
| **v2.x** | Ongoing | Compatibility mode supported with deprecation warnings |
| **v3.0** | 2027 | Compatibility mode removed, facade-only API |

---

**Prepared by**: Claude Code
**Last Updated**: 2026-02-02
**Related**: Issue #610, Epic #577
