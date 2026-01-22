# HTTP Module

This module contains stable HTTP and WebSocket protocol implementations.

## Contents

| Header | Description |
|--------|-------------|
| `http_client.h` | HTTP client implementation with SSL/TLS support |
| `http_server.h` | HTTP server implementation with routing and middleware |
| `websocket_client.h` | WebSocket client with automatic reconnection |
| `websocket_server.h` | WebSocket server with session management |

## Stability

All APIs in this module are **stable** and follow semantic versioning:
- Breaking changes only in major version updates
- New features in minor version updates
- Bug fixes in patch version updates

## Usage

```cpp
#include <kcenon/network/http/http_client.h>
#include <kcenon/network/http/websocket_server.h>

using namespace kcenon::network::http;

// HTTP Client
auto client = http_client::create(http_client_config{});
auto response = client->get("https://api.example.com/data");

// WebSocket Server
auto server = messaging_ws_server::create(ws_server_config{
    .port = 8080
});
server->start();
```

## Migration from core/

If you were previously using includes from `kcenon/network/core/`:

| Old Path | New Path |
|----------|----------|
| `core/http_client.h` | `http/http_client.h` |
| `core/http_server.h` | `http/http_server.h` |
| `core/messaging_ws_client.h` | `http/websocket_client.h` |
| `core/messaging_ws_server.h` | `http/websocket_server.h` |

The old paths still work but will emit deprecation warnings.

## Namespace

All types are in `kcenon::network::http` namespace:

```cpp
namespace kcenon::network::http {
    class http_client;
    class http_server;
    class messaging_ws_client;
    class messaging_ws_server;
    // ... etc
}
```
