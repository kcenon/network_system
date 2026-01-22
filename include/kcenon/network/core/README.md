# Core Module

This module contains the stable core networking primitives and base classes.

## Contents

### Stable APIs

| Header | Description |
|--------|-------------|
| `messaging_client.h` | Base TCP messaging client |
| `messaging_server.h` | Base TCP messaging server |
| `network_context.h` | Shared network I/O context |
| `secure_messaging_client.h` | TLS-secured TCP client |
| `secure_messaging_server.h` | TLS-secured TCP server |

### Compatibility Headers (Deprecated)

The following headers are provided for backward compatibility and will emit
deprecation warnings. Please migrate to the new locations:

| Deprecated Header | New Location |
|-------------------|--------------|
| `http_client.h` | `kcenon/network/http/http_client.h` |
| `http_server.h` | `kcenon/network/http/http_server.h` |
| `messaging_ws_client.h` | `kcenon/network/http/websocket_client.h` |
| `messaging_ws_server.h` | `kcenon/network/http/websocket_server.h` |
| `messaging_quic_client.h` | `kcenon/network/experimental/quic_client.h` |
| `messaging_quic_server.h` | `kcenon/network/experimental/quic_server.h` |
| `reliable_udp_client.h` | `kcenon/network/experimental/reliable_udp_client.h` |

## Stability

All non-deprecated APIs in this module are **stable** and follow semantic versioning.

## Usage

```cpp
#include <kcenon/network/core/messaging_client.h>
#include <kcenon/network/core/messaging_server.h>

using namespace kcenon::network::core;

// TCP Client
auto client = messaging_client::create(client_config{
    .server_address = "localhost",
    .port = 9000
});

// TCP Server
auto server = messaging_server::create(server_config{
    .port = 9000
});
```

## Namespace

Core types are in `kcenon::network::core` namespace:

```cpp
namespace kcenon::network::core {
    class messaging_client;
    class messaging_server;
    class network_context;
    // ... etc
}
```

## Module Organization

The network library is organized into three modules:

```
kcenon/network/
├── core/          # Stable core primitives (TCP, base classes)
├── http/          # Stable HTTP/WebSocket implementations
└── experimental/  # Experimental protocols (QUIC, Reliable UDP)
```
