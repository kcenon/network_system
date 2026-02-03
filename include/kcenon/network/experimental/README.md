# Experimental Module (Migrated)

## Status: Moved to Internal

The experimental headers have been moved to `src/internal/experimental/` as part of the header simplification effort (Issue #577).

## Migration Guide

If you were using experimental APIs directly, update your includes:

| Old Path | New Path |
|----------|----------|
| `kcenon/network/experimental/quic_client.h` | `internal/experimental/quic_client.h` |
| `kcenon/network/experimental/quic_server.h` | `internal/experimental/quic_server.h` |
| `kcenon/network/experimental/reliable_udp_client.h` | `internal/experimental/reliable_udp_client.h` |
| `kcenon/network/experimental/experimental_api.h` | `internal/experimental/experimental_api.h` |

## Recommended Approach

Instead of using internal headers directly, prefer the facade APIs:

```cpp
#include <kcenon/network/facade/quic_facade.h>

using namespace kcenon::network::facade;

// Create QUIC client via facade
quic_facade facade;
auto client = facade.create_client({
    .host = "localhost",
    .port = 4433,
    .client_id = "my-client"
});

// Create QUIC server via facade
auto server = facade.create_server({
    .port = 4433,
    .cert_path = "server.crt",
    .key_path = "server.key"
});
```

## Why This Change?

This change is part of EPIC #577 to:
1. Reduce public header count (goal: < 40 files)
2. Hide internal implementation details
3. Promote facade-based API usage
4. Improve compilation times

## See Also

- `kcenon/network/facade/quic_facade.h` - Recommended facade API
- `kcenon/network/interfaces/i_quic_client.h` - Public interface
- `kcenon/network/interfaces/i_quic_server.h` - Public interface
