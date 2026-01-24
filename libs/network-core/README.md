# network-core

Core interfaces and abstractions for the network_system modularization effort.

## Overview

`network-core` is a header-only library that provides the foundational interfaces and session management abstractions used by all protocol-specific libraries in the network_system ecosystem.

## Purpose

Part of issue #538 (EPIC: Modularize network_system into protocol-specific libraries), this library:

- Defines protocol-agnostic interfaces (`i_client`, `i_server`, `i_session`, `i_network_component`)
- Provides type-erased session management (`session_concept`, `session_model`, `session_handle`)
- Establishes common error handling patterns (`Result<T>`, `VoidResult`)
- Enables protocol libraries to build independently

## Key Components

### 1. Network Component Interfaces

Located in `include/kcenon/network/interfaces/`:

- **`i_network_component.h`**: Base interface for all network components
- **`i_client.h`**: Client-side operations contract
- **`i_server.h`**: Server-side operations contract
- **`i_session.h`**: Individual client session interface

### 2. Session Management (Type Erasure Pattern)

Located in `include/kcenon/network/core/`:

- **`session_concept.h`**: Abstract interface for type-erased sessions
- **`session_model.h`**: Template wrapper that implements `session_concept`
- **`session_handle.h`**: Value-semantic wrapper for sessions
- **`session_traits.h`**: Customization point for session behavior
- **`session_info.h`**: Session metadata and information
- **`session_manager_base.h`**: Base class for session managers

### 3. Error Handling

Located in `include/kcenon/network/utils/`:

- **`result_types.h`**: `Result<T>` and `VoidResult` for error handling

## Usage

### As Part of network_system

When used within the main `network_system` project, simply link to the library:

```cmake
target_link_libraries(your_target PRIVATE kcenon::network-core)
```

### Standalone Build

To build network-core independently:

```bash
cd libs/network-core
cmake -B build
cmake --build build
```

## Dependencies

- **C++20** compiler
- **No external dependencies** (header-only, uses only standard library)

## Integration with Protocol Libraries

Protocol-specific libraries (network-tcp, network-udp, etc.) depend on network-core for:

1. **Interface contracts**: Implement `i_client`, `i_server` interfaces
2. **Session management**: Use `session_handle` for type-erased session storage
3. **Error handling**: Return `Result<T>` from operations

Example dependency graph:

```
network-core (this library)
    ├── network-tcp
    ├── network-udp
    ├── network-websocket
    ├── network-http2
    ├── network-quic
    └── network-grpc
```

## Design Rationale

### Why Header-Only?

- **ABI compatibility**: No binary compatibility concerns across compiler versions
- **Ease of integration**: No separate library build step required
- **Minimal dependencies**: Protocol libraries depend only on headers

### Why Type Erasure?

The `session_concept`/`session_model`/`session_handle` pattern provides:

- **Heterogeneous storage**: Store different session types in the same container
- **Reduced compilation time**: No template instantiation in client code
- **Smaller binary size**: Single implementation instead of per-type instantiation
- **Type recovery**: Original type retrievable via `handle.as<T>()`

## Development Status

- **Current Phase**: Initial extraction from monolithic `network_system`
- **Part of**: Issue #538 (EPIC: Modularize network_system)
- **Related Issues**:
  - Issue #546: Extract network-core library with shared interfaces (this task)
  - Subsequent tasks: Extract protocol-specific libraries

## License

BSD 3-Clause License. See LICENSE file in the root directory.

## Contributing

This library is part of the `network_system` modularization effort. For contribution guidelines, see the main repository README.
