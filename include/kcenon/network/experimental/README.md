# Experimental Module

This module contains experimental protocol implementations that are still under development.

## Warning

**APIs in this module may change between minor versions without notice.**

These implementations are provided for early adopters and feedback collection.
Do not use in production without understanding the risks.

## Contents

| Header | Description |
|--------|-------------|
| `experimental_api.h` | Macros for experimental API opt-in |
| `quic_client.h` | QUIC protocol client implementation |
| `quic_server.h` | QUIC protocol server implementation |
| `reliable_udp_client.h` | Reliable UDP transport layer |

## Enabling Experimental APIs

To use experimental APIs, you must explicitly opt-in:

```cpp
// Method 1: Define before including
#define NETWORK_USE_EXPERIMENTAL
#include <kcenon/network/experimental/quic_client.h>

// Method 2: Compiler define
// g++ -DNETWORK_USE_EXPERIMENTAL ...
```

Without opt-in, you'll get a compile-time error explaining that the API is experimental.

## Usage

```cpp
#define NETWORK_USE_EXPERIMENTAL
#include <kcenon/network/experimental/quic_client.h>
#include <kcenon/network/experimental/quic_server.h>

using namespace kcenon::network::experimental;

// QUIC Client
auto client = messaging_quic_client::create(quic_client_config{
    .server_address = "localhost",
    .port = 4433
});

// QUIC Server (requires TLS certificates)
auto server = messaging_quic_server::create(quic_server_config{
    .port = 4433,
    .cert_path = "server.crt",
    .key_path = "server.key"
});
```

## Migration from core/

If you were previously using includes from `kcenon/network/core/`:

| Old Path | New Path |
|----------|----------|
| `core/messaging_quic_client.h` | `experimental/quic_client.h` |
| `core/messaging_quic_server.h` | `experimental/quic_server.h` |
| `core/reliable_udp_client.h` | `experimental/reliable_udp_client.h` |

The old paths still work but will emit deprecation warnings and automatically
enable the experimental flag for backward compatibility.

## Namespace

All types are in `kcenon::network::experimental` namespace:

```cpp
namespace kcenon::network::experimental {
    class messaging_quic_client;
    class messaging_quic_server;
    class reliable_udp_client;
    // ... etc
}
```

## Feedback

We welcome feedback on experimental APIs! Please open an issue on GitHub
with your use case, suggestions, or bug reports.

## Promotion Path

Experimental APIs may be promoted to stable status when:
1. The API has stabilized and no breaking changes are expected
2. Sufficient testing and real-world usage has occurred
3. Documentation is complete
4. Performance characteristics are well understood
