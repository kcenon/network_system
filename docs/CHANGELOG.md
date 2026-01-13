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

### Fixed
- **UndefinedBehaviorSanitizer Null Pointer Access in Socket Operations** (2026-01-01) (#385)
  - Added `socket_.is_open()` check in `tcp_socket::do_read()` before initiating async operations
  - Added same checks to `secure_tcp_socket::do_read()` for SSL streams
  - Added `is_closed_` check to `secure_tcp_socket::async_send()` to prevent write to closed socket
  - Callback handlers now check `is_closed_` flag to prevent accessing invalid socket state
  - Added missing `<atomic>` header to `secure_tcp_socket.h`
  - Fixes UBSAN "member access within null pointer" error in Multi-Client Concurrent Test

- **gRPC Service Example Build Error** (2026-01-01) (#385)
  - Added `mock_server_context` class implementing `grpc::server_context` interface
  - `grpc::server_context` is an abstract class and cannot be instantiated directly
  - Enables handler invocation demonstration in example code

- **ThreadSanitizer Data Race in Socket Close Operations** (2026-01-01) (#389)
  - Added atomic `close()` method to `tcp_socket` and `secure_tcp_socket` classes
  - Introduced `is_closed_` atomic flag to track socket close state thread-safely
  - Replaced `socket_.is_open()` checks with atomic `is_closed_` flag checks in `do_read()` and `async_send()`
  - Updated all callers to use `close()` instead of `socket().close()` for thread-safe socket closure
  - Prevents data races between socket close operations and concurrent async read/write operations
  - Affected components: `tcp_socket`, `secure_tcp_socket`, `messaging_session`, `secure_session`, `messaging_client`, `secure_messaging_client`

- **Flaky Multi-Client Connection Test on macOS** (2026-01-01) (#385)
  - Changed `ConnectAllClients()` from sequential waiting to round-robin polling
  - Previous implementation blocked on first slow client, causing all subsequent clients to timeout
  - New implementation polls all clients each iteration, counting any that have connected
  - Increased CI timeout from 5 to 10 seconds for many-client scenarios
  - Adds early exit when all clients are connected
  - Fixes intermittent `MultiConnectionLifecycleTest.ConnectionScaling` failures on macOS Release CI

### Added
- **HTTP/2 Server Implementation** (2026-01-13) (#406, #433-#437)
  - Added `http2_server` class for creating HTTP/2 servers with TLS and h2c support
  - Added `http2_server_stream` class for server-side stream handling and response sending
  - Added `http2_request` structure for parsing HTTP/2 requests with pseudo-headers
  - Added `tls_config` structure for TLS configuration (cert, key, CA, client verification)
  - Features:
    - HTTP/2 protocol support (RFC 7540) with HPACK header compression (RFC 7541)
    - TLS 1.3 with ALPN "h2" negotiation via `start_tls()`
    - Cleartext HTTP/2 (h2c) support via `start()`
    - Stream multiplexing with configurable max concurrent streams
    - Flow control with window update handling
    - Customizable HTTP/2 settings (header table size, window size, frame size)
    - Request handler callback for implementing custom routing
  - Added comprehensive unit tests (28 tests) covering:
    - Request header parsing and validation
    - Server lifecycle management
    - Settings configuration
    - Connection handling
  - Added sample code (`http2_server_example.cpp`) demonstrating basic usage

- **CRTP to Composition Refactoring Phase 1.1-1.2** (2026-01-12) (#411, #422, #423)
  - Added comprehensive CRTP analysis document (`docs/design/crtp-analysis.md`)
    - Documented all 9 CRTP base classes across TCP, UDP, WebSocket, and QUIC protocols
    - Identified ~60% code duplication in boilerplate patterns
    - Analyzed dependencies and migration considerations
  - Added composition-based design specification (`docs/design/composition-design.md`)
    - Designed new interface hierarchy (INetworkComponent, IClient, IServer, ISession)
    - Defined utility classes for shared functionality
    - Outlined migration strategy with backward compatibility plan
  - Added new interface classes (`include/kcenon/network/interfaces/`)
    - `i_network_component.h`: Base interface for all network components
    - `i_client.h`: Client-side interface with lifecycle and callbacks
    - `i_server.h`: Server-side interface with session management
    - `i_session.h`: Session interface for server-client connections
  - Added utility classes to reduce code duplication (`include/kcenon/network/utils/`)
    - `lifecycle_manager.h`: Thread-safe start/stop state management
    - `callback_manager.h`: Generic thread-safe callback storage and invocation
    - `connection_state.h`: Connection status state machine with atomic operations
  - These components form the foundation for migrating from CRTP to composition pattern

- **gRPC Phase 5: Testing and Documentation** (2025-12-28) (#365)
  - Added comprehensive unit tests for `grpc_official_wrapper` (status codes, messages, timeouts)
  - Added integration tests for full gRPC stack (service registry, health check, streaming, thread safety)
  - Added performance benchmarks for message serialization and service lookup
  - Added gRPC guide documentation (`docs/guides/GRPC_GUIDE.md`)
  - Added `grpc_service_example.cpp` sample demonstrating all gRPC features
  - Updated README with gRPC in protocols and roadmap sections
  - All acceptance criteria met: unit test coverage, integration tests, documentation complete
- **gRPC Service Registration Mechanism** (2025-12-28) (#364)
  - Added `service_registry` class for centralized service management
  - Added `generic_service` for dynamic method registration at runtime
  - Added `protoc_service_adapter` for wrapping protoc-generated services
  - Added `health_service` implementing standard gRPC health checking protocol
  - Added service/method descriptor types for reflection support
  - Added utility functions for method path parsing and building
  - All four RPC types supported: unary, server streaming, client streaming, bidirectional streaming
  - Thread-safe service registration and lookup operations
  - 47 unit tests covering all components
- **gRPC Wrapper Layer Implementation** (2025-12-28) (#363)
  - Created `grpc_official_wrapper.h` with public API definitions
  - Implemented comprehensive `Result<T>` to `grpc::Status` conversion utilities
  - Added channel management functions (`create_channel`, `wait_for_channel_ready`)
  - Implemented deadline/timeout propagation utilities
  - Added ByteBuffer conversion utilities for seamless data handling
  - Implemented streaming adapter classes for server-side operations
  - Added official gRPC client implementation using `GenericStub`
  - Maintains backward compatibility with existing prototype implementation
  - Part of official gRPC integration epic (#360, Phase 3)

- **gRPC Dependency Configuration** (2025-12-28) (#362)
  - Added `find_grpc_library()` function to `NetworkSystemDependencies.cmake`
  - Comprehensive gRPC/Protobuf/Abseil detection via CMake config and pkg-config
  - Version requirement enforcement (grpc++ >= 1.50.0, protobuf >= 3.21.0)
  - Automatic fallback to prototype implementation when gRPC not found
  - Clear installation instructions in warning messages for all platforms
  - Updated BUILD.md with gRPC installation and configuration guide
  - Part of official gRPC integration epic (#360, Phase 2)

- **Official gRPC Library Integration** (2025-12-28)
  - Added `NETWORK_ENABLE_GRPC_OFFICIAL` CMake option for production gRPC support
  - Created ADR-001 documenting the wrapper architecture decision
  - Implemented `grpc_server` wrapper using official grpc++ library
  - Added server context adapter for seamless API compatibility
  - Enabled gRPC reflection and health check services when using official library
  - Support for both insecure and TLS server configurations
  - Existing prototype implementation remains available as fallback
  - Part of official gRPC integration epic (#360)

### Performance
- **Messaging Zero-Copy Receive Path** (2025-12-19)
  - `messaging_session` uses `tcp_socket::set_receive_callback_view()` for zero-copy receive
  - `messaging_client` uses `tcp_socket::set_receive_callback_view()` for zero-copy receive
  - Internal `on_receive()` methods now accept `std::span<const uint8_t>` instead of `const std::vector<uint8_t>&`
  - Data is copied to vector only when queuing (session) or at API boundary (client)
  - Removes one heap allocation per read vs legacy vector-based path
  - Part of TCP receive std::span callback migration epic (#315, #319)

- **WebSocket Zero-Copy Receive Path** (2025-12-19)
  - `websocket_protocol::process_data()` now accepts `std::span<const uint8_t>` instead of `const std::vector<uint8_t>&`
  - `websocket_socket` uses `tcp_socket::set_receive_callback_view()` for zero-copy TCP-to-WebSocket data flow
  - Eliminates per-read `std::vector` allocation solely for TCP-to-protocol handoff
  - Data is copied once into protocol's internal buffer for frame processing
  - Part of TCP receive std::span callback migration epic (#315, #318)

- **TCP Socket Zero-Allocation Receive Path** (2025-12-18)
  - Added `set_receive_callback_view(std::span<const uint8_t>)` to `tcp_socket` for zero-copy data reception
  - Eliminated per-read `std::vector` allocations when using span callback
  - Implemented lock-free callback storage using `shared_ptr` + `atomic_load/store`
  - Removed mutex contention from receive hot path
  - Legacy vector callback preserved for backward compatibility
  - Span callback takes precedence when both are set
  - Added comprehensive unit tests for new functionality
  - Part of TCP receive std::span callback migration epic (#315)
  - Closes #316

- **Secure TCP Socket Zero-Allocation Receive Path** (2025-12-18)
  - Added `set_receive_callback_view(std::span<const uint8_t>)` to `secure_tcp_socket` for zero-copy TLS data reception
  - Aligned with `tcp_socket` implementation pattern for consistency
  - Eliminated per-read `std::vector` allocations when using span callback
  - Implemented lock-free callback storage using `shared_ptr` + `atomic_load/store`
  - Removed mutex contention from receive hot path
  - Legacy vector callback preserved for backward compatibility
  - Span callback takes precedence when both are set
  - Part of TCP receive std::span callback migration epic (#315)
  - Closes #317

### Changed
- **KCENON_WITH_* Feature Flag Unification** (2025-12-23)
  - Switched integration flags from `BUILD_WITH_*` to `KCENON_WITH_*` macros (#336)
  - Added `feature_flags.h` for unified feature detection across network_system
  - CMake options remain as `BUILD_WITH_*` (user-facing), compile definitions changed to `WITH_*_SYSTEM`
  - `feature_flags.h` detects `WITH_*_SYSTEM` and sets `KCENON_WITH_*` macros for source code
  - Fixed ODR violation in integration_tests by using `*_SYSTEM_INCLUDE_DIR` instead of `BUILD_WITH_*`
  - Added `KCENON_HAS_COMMON_EXECUTOR` definition for consistent thread_pool class layout
  - Fixed macOS CI test failures by using `127.0.0.1` instead of `localhost` (avoids IPv6 fallback delays)
  - Closes #335

### Fixed
- **tcp_socket UBSAN Fix** (2025-12-19)
  - Added `socket_.is_open()` check in `tcp_socket::do_read()` before initiating `async_read_some()`
  - Prevents undefined behavior (null descriptor_state access) when socket is already closed
  - Fixes UBSAN failure in `BoundaryTest.HandlesSingleByteMessage` (#318)

### CI/CD
- **Ecosystem Dependencies Standardization** (2025-12-16)
  - Standardized checkout of ecosystem dependencies (common_system, thread_system, logger_system, container_system) using actions/checkout@v4
  - Added graceful error handling for container_system install step to handle missing files in upstream CMake configuration
  - Updated ci.yml and integration-tests.yml with consistent dependency build steps
  - Added codecov.yml with realistic coverage targets (55% project, 60% patch)
  - Clarified CI workspace paths in CMake dependency resolution
  - Closes #298
- **Windows MSVC Build Fix** (2025-12-16)
  - Moved MSVC compiler setup before ecosystem dependencies build step
  - Changed ecosystem dependencies to build with Debug configuration to match network_system
  - Windows MSVC requires matching RuntimeLibrary settings (MDd vs MD) between linked libraries

### Fixed
- **Static Destruction Order Heap Corruption** (2025-12-16)
  - Applied Intentional Leak pattern to `io_context_thread_manager` to prevent heap corruption during static destruction
  - Avoided capturing `this` pointer in lambdas submitted to thread pools
  - Captured atomic counter pointers directly instead of `this` in `thread_integration.cpp`
  - Fixes "corrupted size vs. prev_size" errors in integration tests when ecosystem dependencies are enabled

### Changed
- **Thread System Migration Epic Complete** (2025-12-06)
  - All direct `std::thread` usage in core source files migrated to `thread_system` integration
  - **Core component migrations:**
    - `messaging_server.cpp`: Uses `io_context_thread_manager` instead of direct `std::thread`
    - `messaging_client.cpp`: Uses `io_context_thread_manager` instead of direct `std::thread`
    - `send_coroutine.cpp`: Uses `thread_integration_manager::submit_task()` instead of `std::thread().detach()`
    - `basic_thread_pool`: Now internally uses `thread_system::thread_pool` when BUILD_WITH_THREAD_SYSTEM is enabled
    - `health_monitor`: Migrated to use centralized thread pool
    - `memory_profiler`: Uses delayed task scheduling
    - `grpc/client.cpp`: Uses thread pool for async calls
  - Added `io_context_thread_manager` for unified ASIO io_context thread management
  - Added `thread_system_pool_adapter` for direct integration with thread_system
  - Delayed tasks now use proper scheduler instead of detached threads
  - Thread pool metrics unified across all subsystems
  - Removed unused `#include <thread>` from headers where std::thread is no longer used
  - **Benefits:**
    - Unified thread resource management
    - Consistent shutdown behavior across all components
    - No more detached threads (proper lifecycle management)
    - Better resource utilization under high load
  - Closes Epic #271 and related issues #272, #273, #274, #275, #276, #277, #278

- **Thread System Integration - health_monitor** (2025-12-05)
  - Replaced direct `std::thread` usage with `thread_integration_manager`
  - Uses centralized thread pool for io_context execution
  - Improved resource management with `std::future` instead of raw thread
  - Part of Thread System Migration Epic (#271)

### Documentation
- **Thread System Integration Documentation** (2025-12-06)
  - Updated ARCHITECTURE.md with detailed thread integration architecture diagram
  - Added Thread Integration API section to API_REFERENCE.md
  - Updated PERFORMANCE_TUNING.md with thread_system configuration guide
  - Closes #279

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
