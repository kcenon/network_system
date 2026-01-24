# network-core

Core interfaces and abstractions for the network_system protocol libraries.

## Purpose

`network-core` provides the foundational layer that all protocol-specific libraries (TCP, UDP, WebSocket, QUIC, HTTP/2, gRPC) depend on. It defines:

- **Core Interfaces**: Base abstractions for clients, servers, and network components
- **Session Management**: Type-erased session handling with unified interfaces
- **Result Types**: Consistent error handling across all protocols
- **Utilities**: Buffer pools, connection state management, lifecycle helpers
- **Concepts**: C++20 concepts for compile-time type validation
- **Metrics & Tracing**: Unified observability infrastructure
- **Integration**: Adapters for logging, threading, and monitoring systems

## Benefits

1. **Reduced Compilation Time**: Protocol libraries compile only against core interfaces, not each other
2. **Clearer Abstractions**: Explicitly defined contracts for network operations
3. **Unified Observability**: Common metrics, tracing, and logging across all protocols
4. **Extensibility**: New protocols only need to implement core interfaces
5. **Reduced Code Duplication**: Shared utilities used once across all protocols
6. **Binary Size**: Single implementation of shared components

## Directory Structure

```
network-core/
├── include/kcenon/network-core/
│   ├── interfaces/          # Core network interfaces (i_client, i_server, etc.)
│   ├── core/                # Session management abstractions
│   ├── unified/             # Protocol-agnostic unified interfaces
│   ├── utils/               # Utilities (buffer_pool, result_types, etc.)
│   ├── concepts/            # C++20 concepts for type validation
│   ├── config/              # Feature flags and configuration
│   ├── metrics/             # Metrics framework
│   ├── tracing/             # Distributed tracing (OpenTelemetry compatible)
│   ├── integration/         # Integration adapters (logging, threading, monitoring)
│   └── protocol/            # Protocol tags and selection
├── src/                     # Implementation files
│   ├── metrics/
│   ├── tracing/
│   ├── integration/
│   └── utils/
├── CMakeLists.txt
└── README.md (this file)
```

## Dependencies

### Required
- C++20 compiler
- CMake 3.21+
- C++ Standard Library
- Threads library

### Optional
- `common_system` (via `KCENON_WITH_COMMON_SYSTEM` flag)
- OpenTelemetry C++ SDK (via `KCENON_WITH_OPENTELEMETRY` flag)

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install . --prefix /usr/local
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `ON` | Build shared libraries |
| `KCENON_WITH_COMMON_SYSTEM` | `OFF` | Enable integration with common_system |
| `KCENON_WITH_OPENTELEMETRY` | `OFF` | Enable OpenTelemetry tracing support |
| `KCENON_NETWORK_CORE_BUILD_TESTS` | `ON` | Build unit tests |

## Usage in CMake Projects

```cmake
find_package(network-core REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE kcenon::network-core)
```

## API Overview

### Core Interfaces

```cpp
#include <kcenon/network-core/interfaces/i_client.h>
#include <kcenon/network-core/interfaces/i_server.h>

// All protocol clients implement i_client
// All protocol servers implement i_server
```

### Session Management

```cpp
#include <kcenon/network-core/core/session_handle.h>

// Type-erased session handle
session_handle handle = /* ... */;
handle.send(data);
handle.close();
```

### Result Types

```cpp
#include <kcenon/network-core/utils/result_types.h>

auto result = some_operation();
if (result) {
    // Success
    auto value = result.value();
} else {
    // Error
    auto error = result.error();
}
```

### Metrics

```cpp
#include <kcenon/network-core/metrics/network_metrics.h>

// Report metrics
metrics::report_connection_established(endpoint);
metrics::report_bytes_sent(count);
```

### Tracing

```cpp
#include <kcenon/network-core/tracing/trace_context.h>
#include <kcenon/network-core/tracing/span.h>

// Distributed tracing (W3C Trace Context)
auto ctx = trace_context::current();
auto span = ctx.start_span("operation_name");
// ... perform operation ...
span.end();
```

## Protocol Library Integration

Protocol-specific libraries (e.g., `network-tcp`, `network-quic`) depend on `network-core`:

```cmake
# network-tcp/CMakeLists.txt
find_package(network-core REQUIRED)

add_library(network-tcp src/tcp_client.cpp src/tcp_server.cpp)
target_link_libraries(network-tcp PUBLIC kcenon::network-core)
```

```cpp
// network-tcp/tcp_client.h
#include <kcenon/network-core/interfaces/i_client.h>

class tcp_client : public i_client {
    // Implement interface methods
};
```

## Migration from Monolithic network_system

1. Update include paths:
   ```cpp
   // Old
   #include <kcenon/network/interfaces/i_client.h>

   // New
   #include <kcenon/network-core/interfaces/i_client.h>
   ```

2. Link against `network-core`:
   ```cmake
   target_link_libraries(my_app PRIVATE kcenon::network-core)
   ```

3. Protocol-specific code moves to separate libraries:
   - TCP → `network-tcp`
   - UDP → `network-udp`
   - WebSocket → `network-websocket`
   - QUIC → `network-quic`
   - HTTP/2 → `network-http2`
   - gRPC → `network-grpc`

## Version

Current version: 1.0.0

## License

See LICENSE file in the parent repository.

## Contributing

Part of the network_system modularization effort (Epic #538).
