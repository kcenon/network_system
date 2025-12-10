[![CI](https://github.com/kcenon/network_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/ci.yml)
[![Code Quality](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml)
[![Coverage](https://github.com/kcenon/network_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/coverage.yml)
[![Doxygen](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml)

# Network System

> **Language:** **English** | [한국어](README_KO.md)

---

## Overview

A production-ready C++20 asynchronous network library providing reusable transport primitives for distributed systems and messaging applications. Originally extracted from messaging_system for enhanced modularity and ecosystem-wide reusability.

**Key Characteristics**:
- 🏗️ **Modular Architecture**: Coroutine-friendly async I/O with pluggable protocol stack
- ⚡ **High Performance**: ASIO-based non-blocking operations with synthetic benchmarks showing ~769K msg/s for small payloads
- 🛡️ **Production Grade**: Comprehensive sanitizer coverage (TSAN/ASAN/UBSAN clean), RAII Grade A, multi-platform CI/CD
- 🔒 **Secure**: TLS 1.2/1.3 support, certificate validation, modern cipher suites
- 🌐 **Cross-Platform**: Ubuntu, Windows, macOS with GCC, Clang, MSVC support

---

## Quick Start

### Prerequisites

**Ubuntu/Debian**:
```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libssl-dev
```

**macOS**:
```bash
brew install cmake ninja asio openssl
```

**Windows (MSYS2)**:
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-openssl
```

### Build

```bash
git clone https://github.com/kcenon/network_system.git
cd network_system
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Your First Server (60 seconds)

```cpp
#include <network_system/core/messaging_server.h>
#include <iostream>

int main() {
    auto server = std::make_shared<network_system::core::messaging_server>("MyServer");

    auto result = server->start_server(8080);
    if (result.is_err()) {
        const auto& err = result.error();
        std::cerr << "Failed to start: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
        return -1;
    }

    std::cout << "Server running on port 8080..." << std::endl;
    server->wait_for_stop();
    return 0;
}
```

### Your First Client

```cpp
#include <network_system/core/messaging_client.h>
#include <iostream>

int main() {
    auto client = std::make_shared<network_system::core::messaging_client>("MyClient");

    auto result = client->start_client("localhost", 8080);
    if (result.is_err()) {
        const auto& err = result.error();
        std::cerr << "Failed to connect: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
        return -1;
    }

    // Send message (std::move avoids extra copies)
    std::string message = "Hello, Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());

    auto send_result = client->send_packet(std::move(data));
    if (send_result.is_err()) {
        const auto& err = send_result.error();
        std::cerr << "Failed to send: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
    }

    client->wait_for_stop();
    return 0;
}
```

---

## Core Features

### Protocols

- **TCP**: Asynchronous TCP server/client with lifecycle management
  - Non-blocking I/O, automatic reconnection, health monitoring
  - Multi-threaded message processing, session management

- **UDP**: Connectionless UDP for real-time applications
  - Low-latency datagram transmission, broadcast/multicast support

- **TLS/SSL**: Secure communication (TLS 1.2/1.3)
  - Modern cipher suites (AES-GCM, ChaCha20-Poly1305)
  - Certificate validation, forward secrecy (ECDHE)

- **WebSocket**: RFC 6455 compliant
  - Text and binary message framing, ping/pong keepalive
  - Fragmentation/reassembly, graceful connection lifecycle

- **HTTP/1.1**: Server and client with advanced features
  - Routing, cookies, multipart/form-data, chunked encoding
  - Automatic compression (gzip/deflate)

- **QUIC**: RFC 9000/9001/9002 compliant (NEW)
  - UDP-based multiplexed transport with TLS 1.3 encryption
  - Stream multiplexing, 0-RTT connection resumption
  - Loss detection and congestion control
  - Connection migration support

📖 **[Detailed Protocol Documentation →](docs/FEATURES.md)**

### Asynchronous Model

- **ASIO-Based**: Non-blocking event loop with async operations
- **C++20 Coroutines**: Optional coroutine-based async helpers
- **C++20 Concepts**: Compile-time type validation with clear error messages
- **Pipeline Architecture**: Message transformation with compression/encryption hooks
- **Move Semantics**: Zero-copy friendly APIs (move-aware buffer handling)

### Failure Handling
- All server/client start helpers return `Result<void>`; check `result.is_err()` before continuing and log `result.error().message`.
- For historical fixes (session cleanup, backpressure, TLS rollout), review `IMPROVEMENTS.md` to understand the safeguards already in place and how to react if regression symptoms reappear.
- When building higher-level services, propagate `common::error_info` up the stack so monitoring and alerting pipelines can correlate failures across tiers.

### Error Handling

**Result<T> Pattern** (75-80% migrated):
```cpp
auto result = server->start_server(8080);
if (result.is_err()) {
    const auto& err = result.error();
    std::cerr << "Error: " << err.message
              << " (code: " << err.code << ")\n";
    return -1;
}
```

**Error Codes** (-600 to -699):
- Connection (-600 to -619): `connection_failed`, `connection_refused`, `connection_timeout`
- Session (-620 to -639): `session_not_found`, `session_expired`
- Send/Receive (-640 to -659): `send_failed`, `receive_failed`, `message_too_large`
- Server (-660 to -679): `server_not_started`, `server_already_running`, `bind_failed`

---

## Performance Highlights

### Synthetic Benchmarks (Intel i7-12700K, Ubuntu 22.04, GCC 11, `-O3`)

| Payload | Throughput | Latency (ns/op) | Scope |
|---------|-----------|-----------------|-------|
| 64 bytes | ~769K msg/s | 1,300 | CPU-only (allocation + memcpy) |
| 256 bytes | ~305K msg/s | 3,270 | CPU-only (allocation + memcpy) |
| 1 KB | ~128K msg/s | 7,803 | CPU-only (allocation + memcpy) |
| 8 KB | ~21K msg/s | 48,000 | CPU-only (allocation + memcpy) |

**Note**: Synthetic benchmarks measure CPU-only operations without real network I/O. Real network throughput/latency measurements are pending.

### Reproducing Benchmarks

```bash
# Build with benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETWORK_BUILD_BENCHMARKS=ON
cmake --build build -j

# Run benchmarks
./build/benchmarks/network_benchmarks

# Run specific category
./build/benchmarks/network_benchmarks --benchmark_filter=MessageThroughput
```

⚡ **[Full Benchmarks & Load Testing →](docs/BENCHMARKS.md)**

**Platform**: Apple M1 @ 3.2GHz (when tested on Apple Silicon)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Network System Architecture                  │
├─────────────────────────────────────────────────────────────────┤
│  Public API Layer                                               │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐    │
│  │ messaging    │ │ messaging    │ │  messaging_ws        │    │
│  │ _server      │ │ _client      │ │  _server / _client   │    │
│  │ (TCP)        │ │ (TCP)        │ │  (WebSocket)         │    │
│  └──────────────┘ └──────────────┘ └──────────────────────┘    │
│  ┌──────────────────────┐ ┌─────────────────────────┐          │
│  │ secure_messaging     │ │ secure_messaging        │          │
│  │ _server (TLS/SSL)    │ │ _client (TLS/SSL)       │          │
│  └──────────────────────┘ └─────────────────────────┘          │
│  ┌──────────────────────┐ ┌─────────────────────────┐          │
│  │ messaging_quic       │ │ messaging_quic          │          │
│  │ _server (QUIC)       │ │ _client (QUIC)          │          │
│  └──────────────────────┘ └─────────────────────────┘          │
├─────────────────────────────────────────────────────────────────┤
│  Protocol Layer                                                 │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐    │
│  │ tcp_socket   │ │ udp_socket   │ │ websocket_socket     │    │
│  └──────────────┘ └──────────────┘ └──────────────────────┘    │
│  ┌──────────────────────┐ ┌──────────────────────────────┐     │
│  │ secure_tcp_socket    │ │ websocket_protocol           │     │
│  │ (SSL stream wrapper) │ │ (frame/handshake/msg handle) │     │
│  └──────────────────────┘ └──────────────────────────────┘     │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ protocols/quic/ (RFC 9000/9001/9002)                     │  │
│  │ connection, stream, packet, frame, crypto, varint        │  │
│  └──────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│  Session Management Layer                                       │
│  ┌──────────────┐ ┌──────────────┐ ┌────────────────────────┐  │
│  │ messaging    │ │ secure       │ │ ws_session_manager     │  │
│  │ _session     │ │ _session     │ │ (WebSocket mgmt)       │  │
│  │ (TCP)        │ │ (TLS/SSL)    │ │                        │  │
│  └──────────────┘ └──────────────┘ └────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ quic_session (QUIC session management)                     ││
│  └────────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────────┤
│  Core Network Engine (ASIO-based)                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐              │
│  │ io_context  │ │   async     │ │  Result<T>  │              │
│  │             │ │  operations │ │   pattern   │              │
│  └─────────────┘ └─────────────┘ └─────────────┘              │
└─────────────────────────────────────────────────────────────────┘
```

**Design Patterns**: Factory, Observer, Strategy, RAII, Template Metaprogramming

🏛️ **[Detailed Architecture Guide →](docs/ARCHITECTURE.md)**

---

## Ecosystem Integration

### Related Projects

This system integrates seamlessly with:

- **[messaging_system](https://github.com/kcenon/messaging_system)**: High-performance message routing and delivery
- **[container_system](https://github.com/kcenon/container_system)**: Type-safe data serialization (MessagePack, FlatBuffers)
- **[thread_system](https://github.com/kcenon/thread_system)**: Thread pool management for concurrent operations
- **[logger_system](https://github.com/kcenon/logger_system)**: Comprehensive network diagnostics and logging
- **[database_system](https://github.com/kcenon/database_system)**: Network-based database operations and clustering

### Integration Example

```cpp
#include <kcenon/network/core/messaging_server.h>
#include <kcenon/network/integration/thread_system_adapter.h>

using namespace kcenon::network;

int main() {
    // Bind thread_system for unified thread management
#if defined(BUILD_WITH_THREAD_SYSTEM)
    integration::bind_thread_system_pool_into_manager("network_pool");
#endif

    // Create and start server
    auto server = std::make_shared<core::messaging_server>("IntegratedServer");
    auto result = server->start_server(8080);

    if (result.is_err()) {
        std::cerr << "Failed: " << result.error().message << std::endl;
        return -1;
    }

    // Get thread pool metrics
    auto& manager = integration::thread_integration_manager::instance();
    auto metrics = manager.get_metrics();
    std::cout << "Workers: " << metrics.worker_threads << std::endl;

    server->wait_for_stop();
    return 0;
}
```

🌐 **[Full Integration Guide →](docs/INTEGRATION.md)**

---

## Documentation

### Getting Started
- 📖 [Features Guide](docs/FEATURES.md) - Comprehensive feature descriptions
- 🏗️ [Architecture](docs/ARCHITECTURE.md) - System design and patterns
- 📘 [API Reference](docs/API_REFERENCE.md) - Complete API documentation
- 🔧 [Build Guide](docs/BUILD.md) - Detailed build instructions
- 🚀 [Migration Guide](docs/MIGRATION_GUIDE.md) - Migrating from messaging_system

### Advanced Topics
- ⚡ [Performance & Benchmarks](docs/BENCHMARKS.md) - Performance metrics and testing
- 🏭 [Production Quality](docs/PRODUCTION_QUALITY.md) - CI/CD, security, quality assurance
- 📁 [Project Structure](docs/PROJECT_STRUCTURE.md) - Directory organization and modules
- 🧩 [C++20 Concepts](docs/advanced/CONCEPTS.md) - Compile-time type validation
- 🔒 [TLS Setup Guide](docs/TLS_SETUP_GUIDE.md) - TLS/SSL configuration
- 🔍 [Troubleshooting](docs/TROUBLESHOOTING.md) - Common issues and solutions
- 🧪 [Load Test Guide](docs/LOAD_TEST_GUIDE.md) - Load testing procedures

### Development
- 🔄 [Integration Guide](docs/INTEGRATION.md) - Ecosystem integration patterns
- 📊 [Operations Guide](docs/OPERATIONS.md) - Deployment and operations
- 📋 [Changelog](docs/CHANGELOG.md) - Version history and updates

---

## Platform Support

| Platform | Compiler | Architecture | Support Level |
|----------|----------|--------------|---------------|
| Ubuntu 22.04+ | GCC 11+ | x86_64, ARM64 | ✅ Full Support |
| Ubuntu 22.04+ | Clang 14+ | x86_64, ARM64 | ✅ Full Support |
| Windows 2022+ | MSVC 2022+ | x86_64 | ✅ Full Support |
| Windows 2022+ | MinGW64 | x86_64 | ✅ Full Support |
| macOS 12+ | Apple Clang 14+ | x86_64, ARM64 | ✅ Full Support |

---

## Production Quality

### CI/CD Infrastructure

**Comprehensive Multi-Platform Testing**:
- ✅ **Sanitizer Coverage**: ThreadSanitizer, AddressSanitizer, UBSanitizer
- ✅ **Multi-Platform**: Ubuntu (GCC/Clang), Windows (MSVC/MinGW), macOS
- ✅ **Performance Gates**: Automated regression detection
- ✅ **Static Analysis**: clang-tidy, cppcheck with modernize checks
- ✅ **Code Coverage**: ~80% with Codecov integration

### Security

**TLS/SSL Implementation**:
- TLS 1.2/1.3 protocol support
- Modern cipher suites (AES-GCM, ChaCha20-Poly1305)
- Forward secrecy (ECDHE), certificate validation
- Hostname verification, optional certificate pinning

**Additional Security**:
- Input validation and buffer overflow protection
- WebSocket origin validation and frame masking
- HTTP request size limits and path traversal protection
- Cookie security (HttpOnly, Secure, SameSite)

### Thread Safety & Memory Safety

**Thread Safety**:
- ✅ Comprehensive synchronization (mutex, atomic, shared_mutex)
- ✅ ThreadSanitizer clean (zero data races)
- ✅ Concurrent session handling tested

**Memory Safety** (RAII Grade A):
- ✅ 100% smart pointer usage (`std::shared_ptr`, `std::unique_ptr`)
- ✅ Zero manual memory management
- ✅ AddressSanitizer clean (zero leaks, zero buffer overflows)
- ✅ Automatic resource cleanup via RAII

🛡️ **[Complete Production Quality Guide →](docs/PRODUCTION_QUALITY.md)**

---

## Dependencies

### Required
- **C++20** compatible compiler (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- **CMake** 3.20+
- **ASIO** or **Boost.ASIO** 1.28+
- **OpenSSL** 1.1.1+ (for TLS/SSL and WebSocket)

### Optional
- **container_system** (advanced serialization)
- **thread_system** (thread pool integration)
- **logger_system** (structured logging)

---

## Build Options

```bash
# Basic build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# With benchmarks
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DNETWORK_BUILD_BENCHMARKS=ON

# With tests
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON

# With optional integrations
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=ON \
    -DBUILD_WITH_LOGGER_SYSTEM=ON \
    -DBUILD_WITH_CONTAINER_SYSTEM=ON

# Build and run tests
cmake --build build -j
cd build && ctest --output-on-failure
```

---

## Examples

Complete examples are available in the `samples/` directory:

- **basic_usage.cpp** - Basic TCP client/server
- **simple_tcp_server.cpp** - TCP server with session management
- **simple_tcp_client.cpp** - TCP client with reconnection
- **simple_http_server.cpp** - HTTP server with routing
- **simple_http_client.cpp** - HTTP client with various request types
- **websocket_example.cpp** - WebSocket server and client
- **quic_server_example.cpp** - QUIC server with multi-stream support
- **quic_client_example.cpp** - QUIC client with stream multiplexing

Build and run examples:
```bash
cmake --build build --target samples
./build/bin/simple_tcp_server
./build/bin/simple_tcp_client
```

---

## Roadmap

### Recently Completed
- ✅ **QUIC Protocol Support**: RFC 9000/9001/9002 compliant implementation

### Current Focus
- 🚧 Real network load test validation and baseline establishment
- 🚧 Complete Result<T> migration (currently 75-80%)
- 🚧 Documentation examples update

### Planned Features
- 🚧 **Connection Pooling**: Enterprise-grade connection management
- 🚧 **Zero-Copy Pipelines**: Eliminate unnecessary buffer copies
- 🚧 **HTTP/2 Client**: Modern HTTP/2 protocol support
- 🚧 **gRPC Integration**: High-performance RPC framework

See [IMPROVEMENTS.md](IMPROVEMENTS.md) for detailed roadmap and tracking.

---

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow the [Coding Style Rules](CODING_STYLE_RULES.md)
4. Commit changes with conventional commits
5. Push to the branch (`git push origin feature/amazing-feature`)
6. Open a Pull Request

**Before submitting**:
- Ensure all tests pass (`ctest`)
- Run sanitizers (TSAN, ASAN, UBSAN)
- Check code formatting (`clang-format`)
- Update documentation as needed

---

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

---

## Contact & Support

| Contact Type | Details |
|--------------|---------|
| **Project Owner** | kcenon (kcenon@naver.com) |
| **Repository** | https://github.com/kcenon/network_system |
| **Issues & Bug Reports** | https://github.com/kcenon/network_system/issues |
| **Discussions & Questions** | https://github.com/kcenon/network_system/discussions |

---

## Acknowledgments

### Core Dependencies
- **ASIO Library Team**: Foundation of asynchronous network programming
- **C++ Standards Committee**: C++20 features enabling modern networking

### Ecosystem Integration
- **Thread System**: Seamless thread pool integration
- **Logger System**: Comprehensive logging and debugging
- **Container System**: Advanced serialization support
- **Database System**: Network-database integration patterns
- **Monitoring System**: Performance metrics and observability

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
