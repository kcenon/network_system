# Migration Guide: Monolithic to Modular Architecture

This guide helps you migrate from the monolithic `network_system` to the new modular architecture introduced in v2.0.

## Overview

The network_system has been refactored from a single monolithic library into protocol-specific modules:

| Before (Monolithic) | After (Modular) |
|---------------------|-----------------|
| Single `network_system` library | Multiple protocol-specific libraries |
| Link everything | Link only what you need |
| ~50MB binary (all protocols) | ~8MB binary (TCP only) |
| Full rebuild on any change | Incremental builds per protocol |

## Quick Migration

### Minimal Change (Use Umbrella Package)

If you want minimal migration effort, use the `network-all` umbrella package:

**Before:**
```cmake
find_package(network_system REQUIRED)
target_link_libraries(my_app PRIVATE network_system::network_system)
```

**After:**
```cmake
find_package(network-all REQUIRED)
target_link_libraries(my_app PRIVATE kcenon::network-all)
```

Your existing code should work without changes.

### Optimized Migration (Selective Linking)

For reduced binary size and faster builds, link only required protocols:

**Before:**
```cmake
target_link_libraries(my_app PRIVATE network_system::network_system)
```

**After:**
```cmake
# Determine which protocols you actually use
find_package(network-tcp REQUIRED)  # For TCP client/server
find_package(network-udp REQUIRED)  # For UDP (if used)

target_link_libraries(my_app PRIVATE
    kcenon::network-tcp
    kcenon::network-udp
)
```

## Library Selection Guide

Choose the libraries based on your protocol usage:

| If you use... | Link to... |
|---------------|------------|
| TCP client/server | `kcenon::network-tcp` |
| UDP datagrams | `kcenon::network-udp` |
| WebSocket | `kcenon::network-websocket` (includes TCP) |
| HTTP/2 | `kcenon::network-http2` (includes WebSocket, TCP) |
| QUIC | `kcenon::network-quic` (includes UDP) |
| gRPC | `kcenon::network-grpc` (includes QUIC, UDP) |
| Everything | `kcenon::network-all` |

## Include Path Changes

### Option 1: Umbrella Header (Recommended for Full Usage)

```cpp
// Single include for all protocols
#include <network_all/network.h>
```

### Option 2: Protocol-Specific Includes

```cpp
// Before (monolithic)
#include <kcenon/network/core/messaging_server.h>
#include <kcenon/network/protocols/tcp_socket.h>

// After (modular) - protocol-specific headers
#include <network_tcp/protocol/tcp.h>
#include <network_udp/protocol/udp.h>
#include <network_websocket/websocket.h>
#include <network_http2/http2.h>
#include <network_quic/quic.h>
#include <network_grpc/grpc.h>
```

## CMake Integration

### find_package Support

Each library supports `find_package()` after installation:

```cmake
# Individual packages
find_package(network-core REQUIRED)
find_package(network-tcp REQUIRED)
find_package(network-udp REQUIRED)
find_package(network-websocket REQUIRED)
find_package(network-http2 REQUIRED)
find_package(network-quic REQUIRED)
find_package(network-grpc REQUIRED)

# Or all at once
find_package(network-all REQUIRED)
```

### add_subdirectory Support

For source-based inclusion:

```cmake
# Add specific libraries
add_subdirectory(external/network_system/libs/network-tcp)
target_link_libraries(my_app PRIVATE kcenon::network-tcp)

# Or add all via umbrella
add_subdirectory(external/network_system/libs/network-all)
target_link_libraries(my_app PRIVATE kcenon::network-all)
```

### FetchContent Support

```cmake
include(FetchContent)

FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG        v2.0.0
)
FetchContent_MakeAvailable(network_system)

# Link to specific libraries or network-all
target_link_libraries(my_app PRIVATE kcenon::network-tcp)
```

## Compile-Time Protocol Detection

Check if a protocol is available at compile time:

```cpp
#include <network_all/network.h>

// Using constexpr booleans
using namespace kcenon::network;

static_assert(all::protocols::tcp, "TCP protocol required");
static_assert(all::protocols::quic, "QUIC protocol required");

// Using preprocessor macros
#if NETWORK_ALL_HAS_WEBSOCKET
    // WebSocket-specific code
#endif

#if NETWORK_ALL_HAS_GRPC
    // gRPC-specific code
#endif
```

Available macros:
- `NETWORK_ALL_HAS_TCP`
- `NETWORK_ALL_HAS_UDP`
- `NETWORK_ALL_HAS_WEBSOCKET`
- `NETWORK_ALL_HAS_HTTP2`
- `NETWORK_ALL_HAS_QUIC`
- `NETWORK_ALL_HAS_GRPC`

## Dependency Graph

Understanding dependencies helps optimize linking:

```
                    network-core (header-only)
                    /         |         \
                   /          |          \
            network-tcp    network-udp    (ASIO, OpenSSL)
                  |                |
                  v                v
         network-websocket   network-quic
                  |                |
                  v                v
            network-http2    network-grpc
```

**Key Points:**
- `network-core` is header-only (no binary)
- `network-websocket` automatically pulls in `network-tcp`
- `network-quic` automatically pulls in `network-udp`
- Each library manages its own dependencies transitively

## Code Changes

### API Compatibility

The public API remains backward compatible. Existing code using:
- `messaging_server` / `messaging_client`
- `secure_messaging_server` / `secure_messaging_client`
- Protocol-specific classes

...will continue to work without modification.

### Namespace Changes

No namespace changes required. All code remains in `kcenon::network::*` namespace.

### Factory Functions

New factory functions are available for cleaner protocol instantiation:

```cpp
// New modular factory functions
auto tcp_conn = protocol::tcp::connect({"localhost", 8080});
auto tcp_listener = protocol::tcp::listen(8080);

auto udp_conn = protocol::udp::connect({"localhost", 9090});
auto udp_listener = protocol::udp::listen(9090);

// These work alongside existing messaging_server/client APIs
```

## Build Performance

### Expected Improvements

| Metric | Monolithic | Modular (TCP only) | Improvement |
|--------|------------|-------------------|-------------|
| Full build | ~5 min | ~1 min | 80% faster |
| Incremental build | ~2 min | ~15 sec | 88% faster |
| Binary size | ~50 MB | ~8 MB | 84% smaller |
| Link time | ~30 sec | ~5 sec | 83% faster |

### Building Specific Libraries

```bash
# Build only TCP library
cmake --build build --target network-tcp

# Build only what you need
cmake --build build --target network-tcp network-udp
```

## Troubleshooting

### Missing Protocol

**Error:**
```
fatal error: network_quic/quic.h: No such file or directory
```

**Solution:** Add the required library to your CMake:
```cmake
find_package(network-quic REQUIRED)
target_link_libraries(my_app PRIVATE kcenon::network-quic)
```

### Link Errors

**Error:**
```
undefined reference to `kcenon::network::tcp_socket::connect'
```

**Solution:** Ensure you link the correct library:
```cmake
target_link_libraries(my_app PRIVATE kcenon::network-tcp)
```

### Header Not Found After Migration

**Error:**
```
fatal error: kcenon/network/protocols/tcp_socket.h: No such file or directory
```

**Solution:** Update to new include paths:
```cpp
// Old
#include <kcenon/network/protocols/tcp_socket.h>

// New (option 1 - umbrella)
#include <network_all/network.h>

// New (option 2 - specific)
#include <network_tcp/internal/tcp_socket.h>
```

## Migration Checklist

- [ ] Update CMakeLists.txt to use new library names
- [ ] Replace `network_system::network_system` with `kcenon::network-*`
- [ ] Optionally update include paths to protocol-specific headers
- [ ] Test build and run existing test suite
- [ ] Verify binary size reduction (if using selective linking)

## Getting Help

- **Issues:** [GitHub Issues](https://github.com/kcenon/network_system/issues)
- **Discussions:** [GitHub Discussions](https://github.com/kcenon/network_system/discussions)
- **Library READMEs:** See individual `libs/*/README.md` files

## Related Documentation

- [Main README](../README.md)
- [Architecture Guide](ARCHITECTURE.md)
- [API Reference](API_REFERENCE.md)
- [Build Guide](guides/BUILD.md)
