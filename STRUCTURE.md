# Network System - Project Structure

**English | [한국어](STRUCTURE_KO.md)**

---

## 📁 Directory Layout

```
network_system/
├── 📁 include/network_system/      # Public headers & interfaces
│   ├── 📁 core/                    # Core networking APIs
│   │   ├── messaging_client.h      # Client interface
│   │   ├── messaging_server.h      # Server interface
│   │   └── session_manager.h       # Session management
│   ├── 📁 session/                 # Session handling
│   │   └── messaging_session.h     # Message session management
│   ├── 📁 integration/             # Integration interfaces
│   │   ├── logger_integration.h    # Logger system adapter
│   │   ├── thread_integration.h    # Thread system adapter
│   │   ├── io_context_thread_manager.h # io_context thread management
│   │   ├── thread_system_adapter.h # Advanced thread integration
│   │   ├── common_system_adapter.h # Common adapter utilities
│   │   ├── container_integration.h # DI container integration
│   │   └── messaging_bridge.h      # Messaging system bridge
│   ├── 📁 internal/                # Internal implementation details
│   │   ├── tcp_socket.h            # TCP socket wrapper
│   │   ├── pipeline.h              # Message pipeline
│   │   ├── send_coroutine.h        # Async send operations
│   │   └── common_defs.h           # Common definitions
│   ├── 📁 utils/                   # Public utilities
│   │   ├── result_types.h          # Result/error types
│   │   ├── session_timeout.h       # Session timeout handling
│   │   └── memory_profiler.h       # Memory profiling utilities
│   ├── network_system.h            # Main system header
│   └── compatibility.h             # Legacy compatibility layer
├── 📁 src/                         # Implementation files
│   ├── 📁 core/                    # Core implementation
│   │   ├── messaging_client.cpp    # Client implementation
│   │   └── messaging_server.cpp    # Server implementation
│   ├── 📁 session/                 # Session implementation
│   │   └── messaging_session.cpp   # Session management
│   ├── 📁 integration/             # Integration implementations
│   │   ├── logger_integration.cpp  # Logger adapter implementation
│   │   ├── thread_integration.cpp  # Thread adapter implementation
│   │   ├── io_context_thread_manager.cpp # io_context thread management
│   │   ├── thread_system_adapter.cpp # Advanced thread integration
│   │   ├── container_integration.cpp # DI container integration
│   │   └── messaging_bridge.cpp    # Messaging bridge implementation
│   ├── 📁 internal/                # Internal implementations
│   │   ├── tcp_socket.cpp          # TCP socket implementation
│   │   ├── pipeline.cpp            # Message pipeline implementation
│   │   └── send_coroutine.cpp      # Async operations
│   └── 📁 utils/                   # Utility implementations
│       └── memory_profiler.cpp     # Memory profiling implementation
├── 📁 tests/                       # Comprehensive test suite
│   ├── 📁 unit/                    # Unit tests
│   │   └── test_tls_config.cpp     # TLS configuration tests
│   ├── 📁 integration/             # Integration tests
│   │   ├── test_integration.cpp    # Integration test suite
│   │   ├── test_compatibility.cpp  # Compatibility tests
│   │   └── test_e2e.cpp            # End-to-end tests
│   ├── 📁 performance/             # Performance tests
│   │   └── benchmark.cpp           # Performance benchmarks
│   ├── 📁 stress/                  # Stress testing
│   │   └── stress_test.cpp         # Stress test suite
│   ├── unit_tests.cpp              # Main unit test runner
│   ├── benchmark_tests.cpp         # Benchmark test runner
│   └── thread_safety_tests.cpp     # Thread safety tests
├── 📁 integration_tests/           # System integration tests
│   ├── 📁 framework/               # Test framework
│   │   ├── test_helpers.h          # Test helper utilities
│   │   └── system_fixture.h        # Test fixtures
│   ├── 📁 scenarios/               # Integration scenarios
│   │   ├── protocol_integration_test.cpp    # Protocol tests
│   │   └── connection_lifecycle_test.cpp    # Connection tests
│   ├── 📁 performance/             # Performance integration
│   │   └── network_performance_test.cpp     # Network benchmarks
│   └── 📁 failures/                # Failure scenarios
│       └── error_handling_test.cpp # Error handling tests
├── 📁 benchmarks/                  # Performance benchmarks
│   ├── connection_bench.cpp        # Connection benchmarks
│   ├── session_bench.cpp           # Session benchmarks
│   ├── message_throughput_bench.cpp # Throughput benchmarks
│   └── main_bench.cpp              # Benchmark runner
├── 📁 samples/                     # Usage examples & demos
│   ├── 📁 messaging_system_integration/ # Integration examples
│   │   ├── modern_usage.cpp        # Modern API usage
│   │   └── legacy_compatibility.cpp # Legacy API usage
│   ├── basic_usage.cpp             # Basic usage example
│   ├── tcp_server_client.cpp       # TCP client/server demo
│   ├── http_client_demo.cpp        # HTTP client demo
│   ├── memory_profile_demo.cpp     # Memory profiling demo
│   └── run_all_samples.cpp         # Sample runner
├── 📁 docs/                        # Comprehensive documentation
│   ├── API_REFERENCE.md            # API documentation
│   ├── ARCHITECTURE.md             # Architecture overview
│   ├── BUILD.md                    # Build instructions
│   ├── MIGRATION_GUIDE.md          # Migration guide
│   ├── INTEGRATION.md              # Integration guide
│   ├── TLS_SETUP_GUIDE.md          # TLS configuration
│   ├── TROUBLESHOOTING.md          # Troubleshooting guide
│   ├── OPERATIONS.md               # Operations guide
│   └── CHANGELOG.md                # Change log
├── 📁 scripts/                     # Build & utility scripts
│   └── 📁 migration/               # Migration utilities
├── 📁 cmake/                       # CMake modules
├── 📄 CMakeLists.txt               # Build configuration
├── 📄 .clang-format                # Code formatting rules
├── 📄 .clang-tidy                  # Static analysis rules
├── 📄 .cppcheck                    # Additional static checks
├── 📄 vcpkg.json                   # Dependency configuration
├── 📄 build.sh                     # Build script
└── 📄 README.md                    # Project overview & documentation
```

## 🏗️ Namespace Structure

### Core Namespaces
- **Root**: `network_system` - Main network system namespace
- **Core functionality**: `network_system::core` - Client/Server APIs
- **Session management**: `network_system::session` - Session handling
- **Integration**: `network_system::integration` - System integration adapters
- **Internal details**: `network_system::internal` - Implementation internals
- **Utilities**: `network_system::utils` - Helper functions and utilities
- **Compatibility**: `network_system::compat` - Legacy compatibility layer

### Nested Namespaces
- `network_system::core` - Core messaging components (client, server)
- `network_system::session` - Session management and lifecycle
- `network_system::integration` - Integration with logger, thread, container systems
- `network_system::internal` - TCP socket, pipeline, coroutines
- `network_system::utils` - Result types, timeouts, profiling
- `network_system::compat` - Backward compatibility layer

## 🔧 Key Components Overview

### 🎯 Public API Layer (`include/network_system/`)
| Component | File | Purpose |
|-----------|------|---------|
| **Main System** | `network_system.h` | Primary system interface |
| **Messaging Client** | `core/messaging_client.h` | Client networking interface |
| **Messaging Server** | `core/messaging_server.h` | Server networking interface |
| **Session Manager** | `core/session_manager.h` | Session lifecycle management |
| **Messaging Session** | `session/messaging_session.h` | Individual session handling |
| **Logger Integration** | `integration/logger_integration.h` | Logger system adapter |
| **Thread Integration** | `integration/thread_integration.h` | Thread system adapter |
| **Container Integration** | `integration/container_integration.h` | DI container integration |
| **Messaging Bridge** | `integration/messaging_bridge.h` | Cross-system messaging |
| **Result Types** | `utils/result_types.h` | Error handling types |
| **Compatibility** | `compatibility.h` | Legacy API support |

### ⚙️ Implementation Layer (`src/`)
| Component | Directory | Purpose |
|-----------|-----------|---------|
| **Core Networking** | `core/` | Client/server implementations |
| **Session Handling** | `session/` | Session lifecycle and management |
| **System Integration** | `integration/` | Adapter implementations |
| **TCP Operations** | `internal/` | Socket, pipeline, coroutines |
| **Utilities** | `utils/` | Profiling and helper functions |

### 🔒 Internal Components (`include/network_system/internal/`)
| Component | File | Purpose |
|-----------|------|---------|
| **TCP Socket** | `tcp_socket.h` | Low-level socket wrapper |
| **Pipeline** | `pipeline.h` | Message processing pipeline |
| **Send Coroutine** | `send_coroutine.h` | Asynchronous send operations |
| **Common Definitions** | `common_defs.h` | Shared constants and types |

## 📊 Performance Characteristics

- **Throughput**: High-performance async message processing
- **Latency**: Low-latency TCP/TLS communication
- **Memory**: Efficient memory management with profiling support
- **Thread Safety**: Thread-safe session management and concurrent operations
- **Scalability**: Session pooling and connection lifecycle management

## 🔄 Migration Guide

### Step 1: Backup Current Setup
```bash
# Automatic backup of old structure
mkdir -p old_structure/
cp -r include/ old_structure/include_backup/
cp -r src/ old_structure/src_backup/
```

### Step 2: Update Include Paths
```cpp
// Old style
#include "messaging_client.h"

// New style
#include "network_system/core/messaging_client.h"
```

### Step 3: Update Namespace Usage
```cpp
// Old style
using namespace messaging;

// New style
using namespace network_system::core;
```

### Step 4: Run Migration Scripts
```bash
# Automated namespace migration
./update_namespaces.sh
./build.sh
```

## 🚀 Quick Start with New Structure

```cpp
#include "network_system/network_system.h"
#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"

int main() {
    using namespace network_system;

    // Initialize network system
    network_system::initialize();

    // Create a messaging client
    auto client = core::messaging_client::create();

    // Connect to server
    if (auto result = client->connect("127.0.0.1", 8080); result) {
        // Send message
        client->send_message("Hello, Server!");
    }

    // Cleanup
    network_system::shutdown();

    return 0;
}
```

## 🔗 Integration Examples

### Logger Integration
```cpp
#include "network_system/integration/logger_integration.h"

using namespace network_system::integration;

// Enable logging for network operations
logger_integration::configure(log_level::info);
```

### Thread System Integration
```cpp
#include "network_system/integration/thread_integration.h"

using namespace network_system::integration;

// Configure thread pool for network operations
thread_integration::set_thread_pool(custom_pool);
```

### Container Integration
```cpp
#include "network_system/integration/container_integration.h"

using namespace network_system::integration;

// Register network components with DI container
container_integration::register_services(container);
```

## 📝 Additional Resources

- [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- [Architecture Guide](docs/ARCHITECTURE.md) - System architecture overview
- [Build Guide](docs/BUILD.md) - Building and configuration
- [Migration Guide](docs/MIGRATION_GUIDE.md) - Detailed migration instructions
- [Integration Guide](docs/INTEGRATION.md) - System integration examples
- [TLS Setup Guide](docs/TLS_SETUP_GUIDE.md) - TLS/SSL configuration
- [Troubleshooting](docs/TROUBLESHOOTING.md) - Common issues and solutions
