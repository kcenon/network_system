# Network System Project Structure

**Last Updated**: 2025-11-15

This document provides a comprehensive guide to the project directory structure, file organization, and module descriptions.

---

## Table of Contents

- [Directory Overview](#directory-overview)
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
├── 📁 samples/                    # Usage examples
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
  ├── C++20 Compiler (GCC 11+, Clang 14+, MSVC 2022+)
  ├── CMake 3.16+
  ├── ASIO 1.28+ (or Boost.ASIO)
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
- [BUILD.md](BUILD.md) - Build instructions and configuration
- [FEATURES.md](FEATURES.md) - Feature descriptions
- [BENCHMARKS.md](BENCHMARKS.md) - Performance benchmarks

---

**Last Updated**: 2025-12-10
**Maintained by**: kcenon@naver.com
