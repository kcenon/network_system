---
doc_id: "NET-PROJ-004"
doc_title: "Network System Project Structure"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "PROJ"
---

# Network System Project Structure

> **SSOT**: This document is the single source of truth for **Network System Project Structure**.

**Last Updated**: 2025-11-15

This document provides a comprehensive guide to the project directory structure, file organization, and module descriptions.

---

## Table of Contents

- [Directory Overview](#directory-overview)
- [libs/ vs src/ Boundary](#libs-vs-src-boundary)
- [Decision Tree: Where Does New Code Go?](#decision-tree-where-does-new-code-go)
- [Core Components](#core-components)
- [Source Organization](#source-organization)
- [Test Organization](#test-organization)
- [Documentation Structure](#documentation-structure)
- [Build Artifacts](#build-artifacts)
- [Module Dependencies](#module-dependencies)

---

## Directory Overview

```
network_system/
├── 📁 include/network_system/     # Public header files (API)
├── 📁 src/                        # Implementation files
├── 📁 examples/                   # Usage examples
├── 📁 tests/                      # Test suites
├── 📁 benchmarks/                 # Performance benchmarks
├── 📁 integration_tests/          # Integration test framework
├── 📁 docs/                       # Documentation
├── 📁 cmake/                      # CMake modules
├── 📁 .github/workflows/          # CI/CD workflows
├── 📄 CMakeLists.txt              # Root build configuration
├── 📄 .clang-format               # Code formatting rules
├── 📄 .gitignore                  # Git ignore rules
├── 📄 LICENSE                     # BSD 3-Clause License
├── 📄 README.md                   # Main documentation
├── 📄 README.kr.md                # Korean documentation
└── 📄 BASELINE.md                 # Performance baseline
```

---

## libs/ vs src/ Boundary

`network_system` ships its protocol implementations in **two parallel locations**.
The split is intentional and the rule below is the single source of truth:

| Location | Target name(s) | Purpose | Stability | Consumed by |
|----------|----------------|---------|-----------|-------------|
| `libs/network-*/` | `kcenon::network::tcp`, `kcenon::network::udp`, `kcenon::network::websocket`, `kcenon::network::http2`, `kcenon::network::quic`, `kcenon::network::grpc`, `kcenon::network::core`, `kcenon::network::all` | Public, modular, ABI-stable protocol libraries. Each module has its own `CMakeLists.txt`, public headers under `include/network_<protocol>/`, and is independently consumable via `find_package`. | Public ABI. Breaking changes follow semver. | Downstream projects link the protocol(s) they need (`kcenon::network::tcp` etc.) directly. |
| `src/` | `network_system::network_system` (umbrella) | Internal implementation behind the umbrella target. Protocol entry-point factories (`src/protocol/`), heavy protocol implementations (`src/protocols/`), facade layer (`src/facade/`), adapters, sessions, integrations, tracing, and internal utilities live here. | Internal. Symbols are reorganized as needed; no API guarantees. | The umbrella `network_system::network_system` target — used by examples, tests, and benchmarks inside this repo, and by consumers that want every protocol in one library. |

**Rule of thumb:**

- Need a **standalone, minimal-dependency library** for one protocol? → Add it under `libs/network-<protocol>/` and expose headers in `include/network_<protocol>/`.
- Need to **wire pieces together** (factory, facade, adapter, session, tracing, integration) behind the umbrella build? → Add it under `src/`.

> Migration history: TCP and UDP have completed the migration into `libs/network-tcp/` and
> `libs/network-udp/`. WebSocket, QUIC, HTTP/2, and gRPC currently live in **both** the
> umbrella `src/` tree (via `src/protocol/` and `src/protocols/`) and the modular
> `libs/network-*/` packages. The umbrella tree is kept compiling for backward compatibility
> until `libs/` parity is complete.

### `src/protocol/` vs `src/protocols/`

These two directories look identical at a glance but serve different roles. Both are
currently active and compiled by the root `CMakeLists.txt`:

| Directory | Layout | Purpose | What goes here |
|-----------|--------|---------|----------------|
| `src/protocol/` (singular) | Flat `.cpp` files: `tcp.cpp`, `udp.cpp`, `quic.cpp`, `websocket.cpp` | **Thin protocol entry points** that satisfy the unified factory API (`kcenon::network::protocol::<proto>::connect()`, `listen()`, `create_connection()`, `create_listener()`). Each file is small (~50-150 lines) and forwards to adapters in `src/unified/adapters/`. | One file per protocol that exposes the unified factory functions defined in `include/kcenon/network/detail/protocol/<proto>.h`. |
| `src/protocols/` (plural) | Per-protocol subdirectories: `grpc/`, `http2/`, `quic/` | **Full protocol implementations** — frame parsers, state machines, congestion controllers, packet protection, HPACK, etc. Files are large (typically 5-50 KB each) and contain the protocol logic itself. | Implementation details for protocols that need their own subtree of supporting source files. |

In short: `src/protocol/` answers *"how do I create a connection of this protocol?"*, and
`src/protocols/` answers *"how does this protocol actually work on the wire?"*.

> Both directories are current as of 2026-05. Neither is legacy. Renaming is out of scope
> for this document; if the singular/plural pair becomes a stumbling block, raise a
> follow-up issue rather than fixing it ad-hoc.

---

## Decision Tree: Where Does New Code Go?

Use this tree when adding a new piece of code. Pick the first branch that matches.

1. **Implementing a brand-new protocol** (e.g., MQTT, WebTransport)?
   - Create `libs/network-<protocol>/` with its own `CMakeLists.txt`, `include/network_<protocol>/`, and `src/`.
   - If the umbrella build needs to expose it as well, add a thin entry-point file under `src/protocol/<protocol>.cpp` that forwards to the modular library.

2. **Adding to an existing protocol's wire-level implementation** (frame parser, congestion controller, codec, state machine)?
   - Modular library (`libs/network-<protocol>/`) is the canonical home if the protocol has been migrated there.
   - Otherwise, add it under `src/protocols/<protocol>/` next to its peers.
   - Avoid duplicating the same source file in both `libs/` and `src/protocols/`. If both must temporarily exist, mirror them and add a comment in the root `CMakeLists.txt` indicating the migration status.

3. **Adding a new factory entry point** (`connect()`, `listen()`, `create_connection()` for an existing protocol)?
   - Edit `src/protocol/<protocol>.cpp` and the corresponding header in `include/kcenon/network/detail/protocol/<protocol>.h`.

4. **Adding a facade** (high-level convenience wrapper over a protocol)?
   - `src/facade/<protocol>_facade.cpp` plus `include/kcenon/network/facade/<protocol>_facade.h`.

5. **Adding an adapter** (bridging a legacy class to the unified `i_connection` / `i_listener` interfaces)?
   - `src/adapters/<thing>_adapter.cpp`.

6. **Adding a session, internal helper, or integration** (thread/container/logger bridge)?
   - `src/session/`, `src/internal/`, or `src/integration/` respectively.

7. **Adding a public header that downstream consumers should include**?
   - Modern API: `include/kcenon/network/<area>/<header>.h` (preferred).
   - Legacy compatibility API: `include/network_system/<area>/<header>.h` (only when extending the legacy surface).

8. **Adding a test, benchmark, or example**?
   - `tests/unit/`, `tests/integration/`, `benchmarks/`, or `examples/` per the conventions in those directories' READMEs.

### Worked examples

- **"Add a new HTTP/3 frame type"** — Modify `src/protocols/http3/frame.cpp` (creating the directory if HTTP/3 is being introduced), and mirror to `libs/network-http3/` once the modular library exists.
- **"Add a `connect_with_retry()` helper for TCP"** — Edit `src/protocol/tcp.cpp` (forwarding entry points). The implementation lives in `libs/network-tcp/src/` if the helper crosses the public ABI; otherwise it can stay in `src/`.
- **"Wire a new logger callback into TCP sessions"** — `src/integration/logger_integration.cpp` plus `include/kcenon/network/integration/logger_integration.h`. Do **not** add it under `libs/network-tcp/`.

---

## Core Components

### include/network_system/ (Public API)

**Purpose**: Public header files that constitute the library's API

```
include/network_system/
├── 📁 concepts/                   # C++20 Concepts (backward compatibility)
│   ├── concepts.h                 # Unified umbrella header
│   └── network_concepts.h         # Backward compatibility redirect
│
├── 📁 core/                       # Core networking components
│   ├── messaging_server.h         # TCP server implementation
│   ├── messaging_client.h         # TCP client implementation
│   ├── messaging_udp_server.h     # UDP server implementation
│   ├── messaging_udp_client.h     # UDP client implementation
│   ├── secure_messaging_server.h  # TLS/SSL server
│   ├── secure_messaging_client.h  # TLS/SSL client
│   ├── messaging_ws_server.h      # WebSocket server
│   ├── messaging_ws_client.h      # WebSocket client
│   ├── http_server.h              # HTTP/1.1 server
│   └── http_client.h              # HTTP/1.1 client
│
├── 📁 session/                    # Session management
│   ├── messaging_session.h        # TCP session handling
│   ├── secure_session.h           # TLS session handling
│   └── ws_session_manager.h       # WebSocket session manager
│
├── 📁 internal/                   # Internal implementation details
│   ├── tcp_socket.h               # TCP socket wrapper
│   ├── udp_socket.h               # UDP socket wrapper
│   ├── secure_tcp_socket.h        # SSL stream wrapper
│   ├── websocket_socket.h         # WebSocket protocol
│   └── websocket_protocol.h       # WebSocket framing/handshake
│
├── 📁 integration/                # External system integration
│   ├── messaging_bridge.h         # Legacy messaging_system bridge
│   ├── thread_integration.h       # Thread system integration
│   ├── container_integration.h    # Container system integration
│   └── logger_integration.h       # Logger system integration
│
├── 📁 utils/                      # Utility components
│   ├── result_types.h             # Result<T> error handling
│   ├── network_config.h           # Configuration structures
│   └── network_utils.h            # Helper functions
│
└── compatibility.h                # Legacy API compatibility layer
```

### include/kcenon/network/ (Modern API)

**Purpose**: Modern namespace structure for public headers (recommended)

```
include/kcenon/network/
├── 📁 concepts/                   # C++20 Concepts definitions
│   ├── concepts.h                 # Unified umbrella header
│   └── network_concepts.h         # 16 network-specific concepts
│
├── 📁 core/                       # Core networking components
├── 📁 config/                     # Configuration structures
├── 📁 integration/                # External system integration
├── 📁 metrics/                    # Network metrics and monitoring
├── 📁 di/                         # Dependency injection support
└── compatibility.h                # API compatibility layer
```

#### Concepts Module

The `concepts/` directory provides C++20 concepts for compile-time type validation:

| Concept | Description |
|---------|-------------|
| `ByteBuffer` | Read-only buffer with `data()` and `size()` |
| `MutableByteBuffer` | Resizable buffer extending ByteBuffer |
| `DataReceiveHandler` | Handler for received data |
| `ErrorHandler` | Error notification handler |
| `ConnectionHandler` | Connection state change handler |
| `SessionHandler` | Session event handler |
| `SessionDataHandler` | Session data event handler |
| `SessionErrorHandler` | Session error handler |
| `DisconnectionHandler` | Disconnection event handler |
| `RetryCallback` | Reconnection attempt handler |
| `NetworkClient` | Client interface requirements |
| `NetworkServer` | Server interface requirements |
| `NetworkSession` | Session interface requirements |
| `DataTransformer` | Data transformation interface |
| `ReversibleDataTransformer` | Bidirectional transformation |
| `Duration` | std::chrono::duration constraint |

See [advanced/CONCEPTS.md](advanced/CONCEPTS.md) for detailed documentation.

#### Core Components Description

**messaging_server.h**:
- TCP server implementation
- Asynchronous connection handling
- Session lifecycle management
- Multi-client support

**messaging_client.h**:
- TCP client implementation
- Automatic reconnection
- Connection health monitoring
- Asynchronous send/receive

**secure_messaging_server.h / secure_messaging_client.h**:
- TLS/SSL encrypted communication
- Certificate management
- TLS 1.2/1.3 protocol support
- Secure handshake handling

**messaging_ws_server.h / messaging_ws_client.h**:
- RFC 6455 WebSocket protocol
- Text and binary message framing
- Ping/pong keepalive
- Connection upgrade from HTTP

**http_server.h / http_client.h**:
- HTTP/1.1 protocol support
- Pattern-based routing
- Cookie and header management
- Multipart/form-data file upload
- Chunked encoding
- Automatic compression (gzip/deflate)

---

## Source Organization

### src/ (Implementation)

```
src/
├── 📁 core/                       # Core component implementations
│   ├── messaging_server.cpp
│   ├── messaging_client.cpp
│   ├── messaging_udp_server.cpp
│   ├── messaging_udp_client.cpp
│   ├── secure_messaging_server.cpp
│   ├── secure_messaging_client.cpp
│   ├── messaging_ws_server.cpp
│   ├── messaging_ws_client.cpp
│   ├── http_server.cpp
│   └── http_client.cpp
│
├── 📁 session/                    # Session management implementations
│   ├── messaging_session.cpp
│   ├── secure_session.cpp
│   └── ws_session_manager.cpp
│
├── 📁 internal/                   # Internal implementations
│   ├── tcp_socket.cpp
│   ├── udp_socket.cpp
│   ├── secure_tcp_socket.cpp
│   ├── websocket_socket.cpp
│   └── websocket_protocol.cpp
│
├── 📁 integration/                # Integration implementations
│   ├── messaging_bridge.cpp
│   ├── thread_integration.cpp
│   ├── container_integration.cpp
│   └── logger_integration.cpp
│
└── 📁 utils/                      # Utility implementations
    ├── result_types.cpp
    ├── network_config.cpp
    └── network_utils.cpp
```

**Key Implementation Details**:

- **messaging_server.cpp**: ~800 lines
  - Server initialization and lifecycle
  - Accept loop with error handling
  - Session creation and management
  - Graceful shutdown logic

- **messaging_session.cpp**: ~600 lines
  - Session state machine
  - Async read/write loops
  - Buffer management
  - Error handling and cleanup

- **pipeline.cpp**: ~400 lines
  - Message transformation pipeline
  - Compression support
  - Encryption hooks
  - Move-aware buffer handling

- **websocket_protocol.cpp**: ~1,000 lines
  - Frame parsing and serialization
  - Masking/unmasking operations
  - Fragmentation handling
  - Protocol state machine

---

## Test Organization

### tests/ (Unit Tests)

```
tests/
├── 📁 unit/                       # Unit tests
│   ├── test_tcp_server.cpp        # TCP server tests
│   ├── test_tcp_client.cpp        # TCP client tests
│   ├── test_session.cpp           # Session tests
│   ├── test_pipeline.cpp          # Pipeline tests
│   ├── test_websocket.cpp         # WebSocket tests
│   ├── test_http.cpp              # HTTP tests
│   └── test_result_types.cpp     # Result<T> tests
│
├── 📁 mocks/                      # Mock objects
│   ├── mock_socket.h              # Mock socket
│   ├── mock_connection.h          # Mock connection
│   └── mock_session.h             # Mock session
│
└── CMakeLists.txt                 # Test build configuration
```

**Test Coverage**:
- Unit tests: 12 test files, 200+ test cases
- Test framework: Google Test
- Coverage: ~80% (measured with gcov/lcov)

### integration_tests/ (Integration Tests)

```
integration_tests/
├── 📁 framework/                  # Test framework
│   ├── system_fixture.h           # System-level fixture
│   ├── network_fixture.h          # Network test fixture
│   └── test_utils.h               # Test utilities
│
├── 📁 scenarios/                  # Test scenarios
│   ├── connection_lifecycle_test.cpp   # Connection lifecycle
│   ├── session_management_test.cpp     # Session management
│   ├── protocol_compliance_test.cpp    # Protocol compliance
│   └── error_handling_test.cpp         # Error handling
│
├── 📁 performance/                # Performance tests
│   ├── tcp_load_test.cpp          # TCP load test
│   ├── udp_load_test.cpp          # UDP load test
│   ├── websocket_load_test.cpp    # WebSocket load test
│   └── http_load_test.cpp         # HTTP load test
│
└── CMakeLists.txt                 # Integration test build config
```

**Integration Test Types**:
- Connection lifecycle tests
- Protocol compliance tests
- Error handling and recovery
- Load and stress tests
- Multi-client scenarios

### benchmarks/ (Performance Benchmarks)

```
benchmarks/
├── message_throughput_bench.cpp   # Message allocation benchmarks
├── connection_bench.cpp           # Connection benchmarks
├── session_bench.cpp              # Session benchmarks
├── protocol_bench.cpp             # Protocol overhead benchmarks
└── CMakeLists.txt                 # Benchmark build config
```

**Benchmark Suite**:
- Google Benchmark framework
- Synthetic CPU-only tests
- Regression detection
- Performance profiling

---

## Documentation Structure

### docs/ (Documentation)

```
docs/
├── 📄 ARCHITECTURE.md             # System architecture overview
├── 📄 API_REFERENCE.md            # Complete API documentation
├── 📄 FEATURES.md                 # Feature descriptions (NEW)
├── 📄 BENCHMARKS.md               # Performance benchmarks (NEW)
├── 📄 PROJECT_STRUCTURE.md        # This file (NEW)
├── 📄 PRODUCTION_QUALITY.md       # Production quality guide (NEW)
├── 📄 MIGRATION_GUIDE.md          # Migration from messaging_system
├── 📄 INTEGRATION.md              # Integration with other systems
├── 📄 BUILD.md                    # Build instructions
├── 📄 OPERATIONS.md               # Operations guide
├── 📄 TLS_SETUP_GUIDE.md          # TLS/SSL configuration
├── 📄 TROUBLESHOOTING.md          # Troubleshooting guide
├── 📄 LOAD_TEST_GUIDE.md          # Load testing guide
├── 📄 CHANGELOG.md                # Version history
│
├── 📁 diagrams/                   # Architecture diagrams
│   ├── architecture.svg
│   ├── protocol_stack.svg
│   └── data_flow.svg
│
└── 📁 adr/                        # Architecture Decision Records
    ├── 001-async-io-model.md
    ├── 002-result-type-error-handling.md
    └── 003-websocket-protocol.md
```

**Korean Translations**: Each major document has a `*.kr.md` Korean version.

---

## Build Artifacts

### build/ (Generated)

```
build/                             # CMake build directory (gitignored)
├── 📁 bin/                        # Executable binaries
│   ├── simple_tcp_server
│   ├── simple_tcp_client
│   ├── simple_http_server
│   └── integration_tests/
│       ├── tcp_load_test
│       ├── udp_load_test
│       └── websocket_load_test
│
├── 📁 lib/                        # Library outputs
│   ├── libnetwork_system.a        # Static library
│   └── libnetwork_system.so       # Shared library (Linux)
│
├── 📁 benchmarks/                 # Benchmark executables
│   └── network_benchmarks
│
├── 📁 tests/                      # Test executables
│   └── network_tests
│
└── 📁 CMakeFiles/                 # CMake internals
```

**Build Modes**:
- Debug: Full symbols, no optimization
- Release: Optimized (-O3), no symbols
- RelWithDebInfo: Optimized with symbols
- MinSizeRel: Size-optimized

---

## Module Dependencies

### Internal Module Dependencies

```
Core Components
  ├── messaging_server
  │   ├── depends on: tcp_socket, messaging_session, pipeline
  │   └── optionally uses: logger_integration, thread_integration
  │
  ├── messaging_client
  │   ├── depends on: tcp_socket, pipeline
  │   └── optionally uses: logger_integration, thread_integration
  │
  ├── secure_messaging_server
  │   ├── depends on: secure_tcp_socket, secure_session, pipeline
  │   └── inherits from: messaging_server pattern
  │
  └── messaging_ws_server
      ├── depends on: websocket_socket, websocket_protocol, ws_session_manager
      └── optionally uses: logger_integration

Session Management
  ├── messaging_session
  │   ├── depends on: tcp_socket, pipeline
  │   └── manages: connection lifecycle, buffer management
  │
  └── ws_session_manager
      ├── depends on: websocket_socket
      └── manages: WebSocket connection lifecycle

Internal Components
  ├── tcp_socket
  │   └── wraps: ASIO async operations
  │
  ├── pipeline
  │   └── depends on: container_integration (optional)
  │
  └── websocket_protocol
      └── depends on: tcp_socket
```

### External Dependencies

**Required Dependencies**:
```
network_system
  ├── C++20 Compiler (GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+)
  ├── CMake 3.20+
  ├── Standalone ASIO 1.30.2+ (Boost.ASIO not supported)
  └── OpenSSL 3.0+ (for TLS/SSL and WebSocket)
```

**Optional Dependencies**:
```
network_system (optional)
  ├── fmt 10.0+ (formatting, falls back to std::format)
  ├── container_system (advanced serialization)
  ├── thread_system (thread pool integration)
  └── logger_system (structured logging)
```

**Build Dependencies**:
```
Development
  ├── Google Test (testing framework)
  ├── Google Benchmark (performance benchmarking)
  ├── Doxygen (API documentation generation)
  └── clang-format (code formatting)
```

---

## CMake Structure

### Root CMakeLists.txt

**Key Sections**:
1. Project declaration and version
2. C++20 standard requirement
3. Dependency management (find_package)
4. Optional feature flags
5. Subdirectory inclusion
6. Target export and installation

**Build Options**:
```cmake
option(NETWORK_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(NETWORK_BUILD_TESTS "Build tests" OFF)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration" OFF)
option(BUILD_WITH_LOGGER_SYSTEM "Build with logger_system integration" OFF)
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
```

### cmake/ (CMake Modules)

```
cmake/
├── FindAsio.cmake                 # Find ASIO library
├── FindOpenSSL.cmake              # Find OpenSSL (extended)
├── CodeCoverage.cmake             # Code coverage support
└── CompilerWarnings.cmake         # Compiler warning configuration
```

---

## File Naming Conventions

### Header Files (.h)

**Pattern**: `snake_case.h`

**Examples**:
- `messaging_server.h` - Public API header
- `tcp_socket.h` - Internal implementation header
- `result_types.h` - Utility header

### Source Files (.cpp)

**Pattern**: `snake_case.cpp` (matches header name)

**Examples**:
- `messaging_server.cpp` - Implementation for messaging_server.h
- `tcp_socket.cpp` - Implementation for tcp_socket.h

### Test Files

**Pattern**: `test_<component>.cpp`

**Examples**:
- `test_tcp_server.cpp`
- `test_session.cpp`
- `test_result_types.cpp`

### Benchmark Files

**Pattern**: `<component>_bench.cpp`

**Examples**:
- `message_throughput_bench.cpp`
- `connection_bench.cpp`
- `session_bench.cpp`

---

## Code Organization Principles

### Header Organization

**Order**:
1. License header
2. Header guard (`#pragma once`)
3. System includes (`<...>`)
4. Third-party includes (`<asio/...>`, `<fmt/...>`)
5. Project includes (`"network_system/..."`)
6. Forward declarations
7. Namespace opening
8. Type declarations
9. Class definition
10. Namespace closing
11. Doxygen documentation

**Example**:
```cpp
// BSD 3-Clause License
// ...

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <asio.hpp>

#include "network_system/utils/result_types.h"

namespace network_system::core {

// Forward declarations
class messaging_session;

/**
 * @brief TCP messaging server
 * @details Provides asynchronous TCP server functionality
 */
class messaging_server {
    // ... class definition
};

}  // namespace network_system::core
```

### Source File Organization

**Order**:
1. Corresponding header include
2. System includes
3. Third-party includes
4. Project includes
5. Anonymous namespace (for internal helpers)
6. Namespace opening
7. Static/const definitions
8. Class method implementations
9. Namespace closing

---

## See Also

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture details
- [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
- [BUILD.md](guides/BUILD.md) - Build instructions and configuration
- [FEATURES.md](FEATURES.md) - Feature descriptions
- [BENCHMARKS.md](BENCHMARKS.md) - Performance benchmarks

---

**Last Updated**: 2025-12-10
**Maintained by**: kcenon@naver.com
