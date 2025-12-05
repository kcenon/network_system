# Changelog

> **Language:** **English** | [한국어](CHANGELOG_KO.md)

## Table of Contents

- [[Unreleased]](#unreleased)
  - [Added](#added)
  - [To Be Implemented](#to-be-implemented)
- [2025-09-20 - Phase 4 In Progress](#2025-09-20-phase-4-in-progress)
  - [Added](#added)
  - [Updated](#updated)
  - [Performance Results](#performance-results)
- [2025-09-20 - Phase 3 Complete](#2025-09-20-phase-3-complete)
  - [Added](#added)
  - [Fixed](#fixed)
- [2025-09-19 - Phase 2 Complete](#2025-09-19-phase-2-complete)
  - [Added](#added)
  - [Updated](#updated)
- [2025-09-19 - Phase 1 Complete](#2025-09-19-phase-1-complete)
  - [Added](#added)
  - [Fixed](#fixed)
  - [Changed](#changed)
  - [Security](#security)
- [2025-09-18 - Initial Separation](#2025-09-18-initial-separation)
  - [Added](#added)
- [Development Timeline](#development-timeline)
- [Migration Guide](#migration-guide)
  - [From messaging_system to network_system](#from-messaging_system-to-network_system)
    - [Namespace Changes](#namespace-changes)
    - [CMake Integration](#cmake-integration)
    - [Container Integration](#container-integration)
- [Support](#support)

All notable changes to the Network System project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### Changed
- **Thread System Integration - health_monitor** (2025-12-05)
  - Replaced direct `std::thread` usage with `thread_integration_manager`
  - Uses centralized thread pool for io_context execution
  - Improved resource management with `std::future` instead of raw thread
  - Part of Thread System Migration Epic (#271)

### Added
- **QUIC Protocol Epic Complete** (2025-12-04)
  - All 4 phases of QUIC implementation completed
  - Phase 1: Foundation (varint, frame, packet)
  - Phase 2: Cryptography (TLS integration, quic_socket)
  - Phase 3: Connection & Streams (stream management, connection state machine, loss detection)
  - Phase 4: Public API (messaging_quic_client, messaging_quic_server, integration tests)
  - Closes Epic #245

- **QUIC Protocol - Phase 4.3 Complete** (2025-12-04)
  - Comprehensive E2E integration tests for QUIC protocol
    - Server lifecycle tests (start, stop, configuration)
    - Client lifecycle and connection tests
    - Data transfer tests (default stream, string data)
    - Multi-stream tests (bidirectional, unidirectional)
    - Session management tests
    - Broadcasting tests
    - Error handling scenarios
    - Thread safety tests (concurrent send, session access)
    - ALPN negotiation tests
  - Complete QUIC documentation suite
    - README.md: Overview and quick start guide
    - ARCHITECTURE.md: Layer structure, components, and data flow
    - API_REFERENCE.md: Complete API documentation for client/server
    - CONFIGURATION.md: Build and runtime configuration guide
    - EXAMPLES.md: Usage examples including echo, multi-stream, chat
  - Part of QUIC Protocol Support (#256)

- **QUIC Protocol - Packet Header Implementation** (2025-12-03)
  - `connection_id` class for managing QUIC connection IDs (0-20 bytes)
  - Packet header structures: `long_header` and `short_header` variants
  - `packet_parser` for parsing Initial, Handshake, 0-RTT, Retry, and 1-RTT packets
  - `packet_builder` for constructing packet headers
  - `packet_number` utilities for variable-length packet number encoding/decoding
  - Support for QUIC v1 (RFC 9000) and v2 (RFC 9369)
  - Version negotiation packet detection
  - Comprehensive unit tests (34 test cases)
  - Part of QUIC Phase 1.3 implementation (#248)

- **Logger System Integration** (2025-09-20)
  - New logger_integration interface for structured logging
  - Support for external logger_system when available
  - Fallback to basic console logger for standalone operation
  - Replaced all std::cout/cerr with NETWORK_LOG_* macros
  - Added BUILD_WITH_LOGGER_SYSTEM CMake option
  - Log levels: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
  - Source location tracking (__FILE__, __LINE__, __FUNCTION__)
  - Timestamp formatting with millisecond precision

### To Be Implemented
- Network manager factory class
- TCP server and client high-level interfaces
- HTTP client with response handling
- WebSocket support
- TLS/SSL encryption
- Connection pooling
- Migration guide completion (Phase 4)

## 2025-09-20 - Phase 4 In Progress

### Added
- **Performance Benchmarking**
  - Comprehensive performance benchmark suite (benchmark.cpp)
  - Throughput tests for various message sizes (64B, 1KB, 8KB)
  - Concurrent connection benchmarks (10 and 50 clients)
  - Thread pool performance measurements
  - Container serialization benchmarks
  - Achieved 305K+ msg/s average throughput

- **Compatibility Testing**
  - Enhanced compatibility test suite (test_compatibility.cpp)
  - Legacy namespace testing (network_module, messaging)
  - Type alias verification
  - Feature detection macros
  - Cross-compatibility tests between legacy and modern APIs
  - Message passing validation
  - Container and thread pool integration through legacy API

- **Usage Examples**
  - Legacy compatibility example (legacy_compatibility.cpp)
  - Modern API usage example (modern_usage.cpp)
  - Demonstration of backward compatibility
  - Advanced features showcase

### Updated
- README.md with performance results and Phase 4 status
- Documentation cleaned up and streamlined
- CMakeLists.txt with benchmark target

### Performance Results
- Average throughput: 305,255 msg/s
- Small messages (64B): 769,230 msg/s
- Medium messages (1KB): 128,205 msg/s
- Large messages (8KB): 20,833 msg/s
- Concurrent connections: Successfully handled 50 clients
- Performance rating: EXCELLENT - Production ready!

## 2025-09-20 - Phase 3 Complete

### Added
- **Thread System Integration**
  - thread_integration.h/cpp with thread pool abstraction
  - Thread pool interface for async task management
  - Basic thread pool implementation
  - Thread integration manager singleton
  - Support for delayed task submission
  - Task metrics and monitoring

- **Container System Integration**
  - container_integration.h/cpp for serialization
  - Container interface for message serialization
  - Basic container implementation
  - Container manager for registration and management
  - Support for custom serializers/deserializers

- **Compatibility Layer**
  - compatibility.h with legacy namespace aliases
  - Full backward compatibility with messaging_system
  - Legacy API support (network_module namespace)
  - Feature detection macros
  - System initialization/shutdown functions

- **Integration Tests**
  - test_integration.cpp for integration validation
  - Tests for bridge functionality
  - Tests for container integration
  - Tests for thread pool integration
  - Performance baseline verification

### Fixed
- Private member access issues in thread_integration
- Compatibility header structure issues
- API mismatches in sample code
- CMake fmt linking problems on macOS

## 2025-09-19 - Phase 2 Complete

### Added
- **Core System Separation**
  - Verified complete separation from messaging_system
  - Confirmed proper directory structure (include/network_system)
  - All modules properly organized (core, session, internal, integration)

- **Integration Module**
  - Completed messaging_bridge implementation
  - Performance metrics collection functionality
  - Container_system integration interface

- **Build Verification**
  - Successful build with container_system integration
  - verify_build test program passes all checks
  - CMakeLists.txt supports optional integrations

### Updated
- MIGRATION_CHECKLIST.md with Phase 2 completion status
- README.md with current project phase information

## 2025-09-19 - Phase 1 Complete

### Added
- **Core Infrastructure**
  - Complete separation from messaging_system
  - New namespace structure: `network_system::{core,session,internal,integration}`
  - ASIO-based asynchronous networking with C++20 coroutines
  - Messaging bridge for backward compatibility

- **Build System**
  - CMake configuration with vcpkg support
  - Flexible dependency detection (ASIO/Boost.ASIO)
  - Cross-platform support (Linux, macOS, Windows)
  - Manual vcpkg setup as fallback

- **CI/CD Pipeline**
  - GitHub Actions workflows for Ubuntu (GCC/Clang)
  - GitHub Actions workflows for Windows (Visual Studio/MSYS2)
  - Dependency security scanning with Trivy
  - License compatibility checks

- **Container Integration**
  - Full integration with container_system
  - Value container support in messaging bridge
  - Conditional compilation based on availability

- **Documentation**
  - Doxygen configuration for API documentation
  - Comprehensive README with build instructions
  - Migration and implementation plans
  - Architecture documentation

### Fixed
- **CI/CD Issues**
  - Removed problematic lukka/run-vcpkg action
  - Fixed pthread.lib linking error on Windows
  - Added _WIN32_WINNT definition for ASIO on Windows
  - Corrected CMake options (BUILD_TESTS instead of USE_UNIT_TEST)
  - Made vcpkg optional with system package fallback

- **Build Issues**
  - Fixed namespace qualification errors
  - Corrected include paths for internal headers
  - Resolved ASIO detection on various platforms
  - Fixed vcpkg baseline configuration issues

### Changed
- **Dependency Management**
  - Prefer system packages over vcpkg when available
  - Simplified vcpkg.json configuration
  - Added multiple fallback paths for dependency detection
  - Made container_system and thread_system optional

### Security
- Implemented dependency vulnerability scanning
- Added license compatibility verification
- Configured security event reporting

## 2025-09-18 - Initial Separation

### Added
- Initial implementation separated from messaging_system
- Basic TCP client/server functionality
- Session management
- Message pipeline processing

---

## Development Timeline

| Date | Milestone | Description |
|------|-----------|-------------|
| 2025-09-19 | Phase 1 Complete | Infrastructure and separation complete |
| 2025-09-18 | Initial Separation | Started separation from messaging_system |

## Migration Guide

### From messaging_system to network_system

#### Namespace Changes
```cpp
// Old (messaging_system)
#include <messaging_system/network/tcp_server.h>
using namespace network_module;

// New (network_system)
#include <network_system/core/messaging_server.h>
using namespace network_system::core;
```

#### CMake Integration
```cmake
# Old (messaging_system)
# Part of messaging_system

# New (network_system)
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_target NetworkSystem::NetworkSystem)
```

#### Container Integration
```cpp
// New feature in network_system
#include <network_system/integration/messaging_bridge.h>

auto bridge = std::make_unique<network_system::integration::messaging_bridge>();
bridge->set_container(container);
```

## Support

For issues, questions, or contributions, please visit:
- GitHub: https://github.com/kcenon/network_system
- Email: kcenon@naver.com
---

*Last Updated: 2025-10-20*
