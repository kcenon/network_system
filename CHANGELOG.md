# Changelog - Network System

> **Language:** **English** | [한국어](CHANGELOG_KO.md)

All notable changes to the Network System project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- C++20 coroutine full integration
- WebSocket protocol support
- HTTP/2 and HTTP/3 support
- TLS 1.3 support
- Advanced load balancing algorithms

---

## [1.0.0] - 2025-10-22

### Added
- **Core Network System**: Production-ready C++20 asynchronous network library
  - ASIO-based non-blocking I/O operations
  - Zero-copy pipeline for maximum efficiency
  - Connection pooling with intelligent lifecycle management
  - C++20 coroutine support for async operations

- **High Performance**: Ultra-high throughput networking
  - 305K+ messages/second average throughput
  - 769K+ msg/s for small messages (<1KB)
  - Sub-microsecond latency
  - Direct memory mapping for zero-copy transfers

- **Core Components**:
  - **MessagingClient**: High-performance client with connection pooling
  - **MessagingServer**: Scalable server with session management
  - **MessagingSession**: Per-connection session handling
  - **Pipeline**: Zero-copy buffer management system

- **Modular Architecture**:
  - Independent from messaging_system (100% backward compatible)
  - Optional integration with container_system
  - Optional integration with thread_system
  - Optional integration with logger_system
  - Optional integration with common_system

- **Integration Support**:
  - container_system: Type-safe data transmission with binary protocols
  - thread_system: Thread pool management for concurrent operations
  - logger_system: Network operation logging and diagnostics
  - common_system: Result<T> error handling pattern
  - messaging_system: Messaging transport layer (bridge mode)

- **Advanced Features**:
  - Automatic reconnection with exponential backoff
  - Connection timeout management
  - Graceful shutdown handling
  - Session lifecycle management
  - Error recovery patterns

- **Build System**:
  - CMake 3.16+ with modular configuration
  - 13+ feature flags for flexible builds
  - Cross-platform support (Windows, Linux, macOS)
  - Compiler support (GCC 10+, Clang 10+, MSVC 19.29+)

### Changed
- N/A (Initial independent release)

### Deprecated
- N/A (Initial independent release)

### Removed
- N/A (Initial independent release)

### Fixed
- N/A (Initial independent release)

### Security
- Secure connection handling with TLS support
- Input validation for all network operations
- Buffer overflow protection
- Connection limits to prevent DoS

---

## [0.9.0] - 2025-09-15 (Pre-separation from messaging_system)

### Added
- Initial network functionality as part of messaging_system
- Basic client/server communication
- Message serialization and deserialization
- Connection management

### Changed
- Tight coupling with messaging_system
- Limited independent usage

---

## Project Information

**Repository:** https://github.com/kcenon/network_system
**Documentation:** [docs/](docs/)
**License:** See LICENSE file
**Maintainer:** kcenon@naver.com

---

## Version Support Matrix

| Version | Release Date | C++ Standard | CMake Minimum | Status |
|---------|--------------|--------------|---------------|---------|
| 1.0.0   | 2025-10-22   | C++20        | 3.16          | Current |
| 0.9.0   | 2025-09-15   | C++20        | 3.16          | Legacy (part of messaging_system) |

---

## Migration Guide

For migration from messaging_system integrated network module, see [MIGRATION.md](MIGRATION.md).

---

## Performance Benchmarks

**Platform**: Varies by system (see benchmarks/ for details)

### Throughput Benchmarks
- Small messages (<1KB): 769K+ msg/s
- Medium messages (1-10KB): 305K+ msg/s
- Large messages (>10KB): 150K+ msg/s

### Latency Benchmarks
- Local connections: < 1 microsecond
- Network connections: Depends on network latency + processing overhead

### Scalability
- Concurrent connections: 10,000+ simultaneous connections
- Linear scaling up to 8 cores
- Connection pooling reduces overhead by 60%

---

## Breaking Changes

### 1.0.0
- **Namespace change**: `messaging::network` → `network_system`
- **Header paths**: `<messaging/network/*>` → `<network_system/*>`
- **CMake target**: `messaging_system` → `network_system`
- **Build options**: New `BUILD_WITH_*` options for ecosystem integration

See [MIGRATION.md](MIGRATION.md) for detailed migration instructions.

---

**Last Updated:** 2025-10-22
