# Changelog - Network System

> **Language:** **English** | [한국어](CHANGELOG_KO.md)

All notable changes to the Network System project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- **CRTP Base Classes for Messaging**: Added template base classes for messaging clients and servers (#376)
  - `messaging_client_base<Derived>` provides common lifecycle management and callback handling for clients
  - `messaging_server_base<Derived, SessionType>` provides common lifecycle management for servers
  - Thread-safe state management with atomic flags
  - Unified callback management with mutex protection
  - Standard lifecycle methods (`start`, `stop`, `wait_for_stop`)
  - Zero-overhead abstraction through CRTP pattern
  - Foundation for future migration of existing messaging classes
- **gRPC Service Registration Mechanism**: Implemented service registration for gRPC with official library support (#364)
  - Added `service_registry` class for centralized service management
  - Added `generic_service` for dynamic method registration at runtime
  - Added `protoc_service_adapter` for wrapping protoc-generated services
  - Added `health_service` implementing standard gRPC health checking protocol
  - Added service/method descriptor types for reflection support
  - Added utility functions for method path parsing and building
  - All four RPC types supported: unary, server streaming, client streaming, bidirectional streaming
  - Thread-safe service registration and lookup operations
  - 47 unit tests covering all components
- **Session Idle Timeout Cleanup**: Implemented automatic idle session detection and cleanup in `session_manager` (#353)
  - Added `session_info` struct with `last_activity` timestamp tracking
  - Implemented `cleanup_idle_sessions()` to detect and remove sessions idle beyond `idle_timeout`
  - Added `update_activity()` method for external activity timestamp updates
  - Added `get_session_info()` and `get_idle_duration()` helper methods
  - Added `total_cleaned_up` metric to session statistics
  - Sessions are gracefully stopped before removal to ensure proper resource cleanup
  - Comprehensive unit tests covering activity tracking, cleanup, and thread safety
- **Messaging Zero-Copy Receive**: Migrated messaging components to use `std::span<const uint8_t>` callbacks (#319)
  - `messaging_session` uses `tcp_socket::set_receive_callback_view()` for zero-copy receive
  - `messaging_client` uses `tcp_socket::set_receive_callback_view()` for zero-copy receive
  - Internal `on_receive()` methods now accept `std::span<const uint8_t>` instead of `const std::vector<uint8_t>&`
  - Data is copied to vector only when queuing (session) or at API boundary (client)
  - Removes one heap allocation per read vs legacy vector-based path
  - Part of Epic #315 (TCP receive zero-allocation hot path)
- **WebSocket Zero-Copy Receive**: Migrated WebSocket receive path to use `std::span<const uint8_t>` callbacks (#318)
  - `websocket_protocol::process_data()` now accepts `std::span<const uint8_t>` instead of `const std::vector<uint8_t>&`
  - `websocket_socket` uses `tcp_socket::set_receive_callback_view()` for zero-copy TCP-to-WebSocket data flow
  - Eliminates per-read `std::vector` allocation solely for TCP-to-protocol handoff
  - Part of Epic #315 (TCP receive zero-allocation hot path)
- **secure_tcp_socket Zero-Copy Receive**: Added `set_receive_callback_view(std::span<const uint8_t>)` API for zero-copy TLS data reception (#317)
  - Zero-copy path using `std::span` directly into read buffer, avoiding per-read `std::vector` allocations
  - Lock-free callback storage using `shared_ptr` + `atomic_load/store` for better performance under high TPS
  - Span callback takes priority when both view and vector callbacks are set
  - Legacy vector callback preserved for backward compatibility
  - Aligned with `tcp_socket` implementation from #316 for consistency
- **OpenSSL 3.x Support**: Added full compatibility with OpenSSL 3.x while maintaining backward compatibility with 1.1.1 (#308)
  - New `openssl_compat.h` header with version detection macros and utility functions
  - CMake version detection with deprecation warnings for OpenSSL 1.1.1 (EOL: September 2023)
  - Compile-time macros: `NETWORK_OPENSSL_3_X`, `NETWORK_OPENSSL_1_1_X`, `NETWORK_OPENSSL_VERSION_3_X`
  - Runtime utilities: `is_openssl_3x()`, `is_openssl_eol()`, `get_openssl_error()`
  - CI workflows updated to verify OpenSSL version and install OpenSSL 3.x dependencies
- **C++20 Concepts**: Added network-specific C++20 concepts for compile-time type validation (#294)
  - New `<kcenon/network/concepts/concepts.h>` unified header
  - Buffer concepts: `ByteBuffer`, `MutableByteBuffer`
  - Callback concepts: `DataReceiveHandler`, `ErrorHandler`, `SessionHandler`, `SessionDataHandler`, `SessionErrorHandler`, `DisconnectionHandler`, `RetryCallback`
  - Network component concepts: `NetworkClient`, `NetworkServer`, `NetworkSession`
  - Pipeline concepts: `DataTransformer`, `ReversibleDataTransformer`
  - Duration concept for time-related constraints
  - Integration with common_system's Result/Optional concepts when available
  - Concepts improve error messages and serve as self-documenting type constraints

### Changed
- **UDP/WebSocket CRTP Migration**: Migrated UDP and WebSocket classes to use protocol-specific CRTP base classes (#384)
  - `messaging_udp_client` now inherits from `messaging_udp_client_base<messaging_udp_client>`
  - `messaging_udp_server` now inherits from `messaging_udp_server_base<messaging_udp_server>`
  - `messaging_ws_client` now inherits from `messaging_ws_client_base<messaging_ws_client>`
  - `messaging_ws_server` now inherits from `messaging_ws_server_base<messaging_ws_server>`
  - Protocol-specific base classes handle unique requirements:
    - UDP base classes: connectionless semantics with endpoint-aware callbacks
    - WebSocket base classes: connection-aware with WS-specific message types (text/binary/ping/pong/close)
  - Removed pimpl pattern from `messaging_ws_client` for direct CRTP inheritance
  - Kept pimpl pattern for `ws_connection` in server for session management
  - Common lifecycle methods (`start`/`stop`/`wait_for_stop`) provided by base classes
  - Maintains full backward compatibility with existing API
- **secure_messaging CRTP Migration**: Migrated all secure messaging classes to use CRTP base classes (#383)
  - `secure_messaging_client` now inherits from `messaging_client_base<secure_messaging_client>`
  - `secure_messaging_server` now inherits from `messaging_server_base<secure_messaging_server, secure_session>`
  - `secure_messaging_udp_client` now inherits from `messaging_client_base<secure_messaging_udp_client>`
  - `secure_messaging_udp_server` maintains manual lifecycle management due to UDP-specific callbacks
  - Added `set_udp_receive_callback()` for UDP clients with endpoint-aware callback signature
  - All TLS/DTLS-specific behavior preserved in `do_start()`, `do_stop()`, `do_send()` methods
  - Common callback setters and lifecycle methods now provided by base classes
  - Maintains full backward compatibility with existing API
- **messaging_server CRTP Migration**: Migrated `messaging_server` to use `messaging_server_base` CRTP pattern (#382)
  - Inherits from `messaging_server_base<messaging_server>` for common lifecycle management
  - Implements `do_start()`, `do_stop()` for TCP-specific server behavior
  - Removes duplicated code (~200 lines reduction)
  - All callback setters and lifecycle methods now provided by base class
  - Added callback getter methods to base class for derived class access
- **messaging_client CRTP Migration**: Migrated `messaging_client` to use `messaging_client_base` CRTP pattern (#381)
  - Inherits from `messaging_client_base<messaging_client>` for common lifecycle management
  - Implements `do_start()`, `do_stop()`, `do_send()` for TCP-specific behavior
  - Removes duplicated code (~265 lines reduction)
  - All callback setters and lifecycle methods now provided by base class
  - Maintains full backward compatibility with existing API
  - First proof-of-concept for messaging class hierarchy migration
- **vcpkg Manifest**: Added ecosystem dependencies as optional feature for vcpkg registry distribution (#371)
  - Added `ecosystem` feature containing `kcenon-common-system`, `kcenon-thread-system`, `kcenon-logger-system`, `kcenon-container-system`
  - These dependencies are required as documented in README, now declared in vcpkg.json
  - Feature-based approach allows CI to pass while ecosystem packages await vcpkg registry registration
  - Enable with `vcpkg install --feature ecosystem` once packages are registered
- **Logging System**: Migrated from direct logger_system dependency to common_system's ILogger interface (#285)
  - `NETWORK_LOG_*` macros now delegate to common_system's `LOG_*` macros
  - Added `common_system_logger_adapter` for bridging legacy `logger_interface`
  - `BUILD_WITH_LOGGER_SYSTEM` is now optional (default: OFF)
  - Logging uses GlobalLoggerRegistry for runtime logger binding
  - Reduces compile-time coupling and enables flexible logger configuration

### Fixed
- **PartialMessageRecovery Test Fix**: Fixed use-after-move bug in ErrorHandlingTest.PartialMessageRecovery (#389)
  - Created separate message instances instead of reusing a moved-from object
  - The original code moved `valid_message` in the first `SendMessage` call, then attempted to move it again, causing undefined behavior and test crashes across all platforms
- **tcp_socket AddressSanitizer Fix**: Fixed SEGV in do_read callback when socket closes during async operation (#388)
  - Added `socket_.is_open()` check before recursive `do_read()` call to prevent race condition
  - Socket could close between `is_reading_.load()` check and `do_read()` invocation
  - Added missing `<atomic>` and `<mutex>` headers to tcp_socket.h for proper type declarations
  - Fixes E2ETests failure under AddressSanitizer on Ubuntu 24.04
- **messaging_client ThreadSanitizer Fix**: Fixed data race between `do_stop()` and `async_connect()` operations (#388)
  - Post pending socket close operations to io_context thread via `asio::post()` to ensure socket operations happen on the same thread as async_connect
  - Added `stop_initiated_` checks in async handlers to prevent socket operations after stop has been initiated
  - Resolves ThreadSanitizer failure in CI where `do_stop()` was closing `pending_socket_` from a different thread while async_connect was still modifying socket internal state
- **Integration Test Flakiness**: Fixed intermittent `ProtocolIntegrationTest.RepeatingPatternData` failures on macOS CI (#376)
  - Added actual sleep to `wait_for_ready()` for socket cleanup (TIME_WAIT handling)
  - Previous implementation only used `yield()` which didn't guarantee port release
  - Uses 50ms delay in CI environments, 10ms locally
- **Result Check Consistency**: Fixed `messaging_server_base` to use `is_err()` instead of `operator!` for VoidResult (#376)
  - Aligns with `messaging_client_base` and common_system error handling patterns
- **messaging_server Resource Cleanup**: Fixed heap corruption when `start_server()` fails (#335)
  - When port binding fails (e.g., address already in use), partially created resources (`io_context_`, `work_guard_`) were not cleaned up
  - Added resource cleanup in `start_server()` catch blocks to release partially created resources
  - Added explicit resource cleanup in destructor to handle cases where `stop_server()` returns early
  - Fixes "corrupted size vs. prev_size" errors in `ConnectionLifecycleTest.ServerStartupOnUsedPort` on Linux Debug builds
- **tcp_socket SEGV Fix**: Added socket validity check before async send operation (#389)
  - `tcp_socket::async_send()` now checks `socket_.is_open()` before initiating `asio::async_write()`
  - Returns `asio::error::not_connected` error via handler when socket is already closed
  - Added sanitizer skip for `LargeMessageTransfer` test due to asio internal race conditions
  - Fixes ThreadSanitizer failure in `NetworkTest.LargeMessageTransfer`
- **tcp_socket UBSAN Fix**: Added socket validity check before async read operation (#318)
  - `tcp_socket::do_read()` now checks `socket_.is_open()` before initiating `async_read_some()`
  - Prevents undefined behavior (null descriptor_state access) when socket is already closed
  - Fixes UBSAN failure in `BoundaryTest.HandlesSingleByteMessage`
- **Static Destruction Order**: Applied Intentional Leak pattern to prevent heap corruption during process shutdown (#314)
  - Applied to: `network_context`, `io_context_thread_manager`, `thread_integration_manager`, `basic_thread_pool`
  - When thread pool tasks still reference shared resources during static destruction, heap corruption ("corrupted size vs. prev_size") can occur
  - Using no-op deleters on pimpl/shared_ptr pointers keeps resources alive until OS cleanup
  - Memory impact: ~few KB per singleton, reclaimed by OS on process termination
  - Temporarily skipped `MultiConnectionLifecycleTest.SequentialConnections` and `ErrorHandlingTest.RecoveryAfterConnectionFailure` in CI (tracked as Issue #315)
- **CI Windows Build**: Fixed PowerShell error handling for container_system install failures (#314)
  - PowerShell try/catch does not catch external command failures
  - Changed to use `$LASTEXITCODE` check to properly handle cmake install errors
  - Allows CI to continue when container_system install encounters missing files
- **CI Windows MSVC Build**: Fixed linker errors for ecosystem dependencies (#314)
  - Moved MSVC compiler setup before ecosystem dependencies build step
  - Changed ecosystem dependencies to build with Debug configuration to match network_system
  - Windows MSVC requires matching RuntimeLibrary settings (MDd vs MD) between linked libraries
- **Thread System Adapter**: Fixed `submit_delayed()` to use a single scheduler thread with priority queue instead of creating a detached `std::thread` per delayed task (#273)
  - Eliminates thread explosion under high delayed task submission
  - Provides proper thread lifecycle management (joinable scheduler thread)
  - Supports clean shutdown with pending task cancellation
  - Added comprehensive unit tests for delayed task execution
- **Thread System Adapter**: Eliminated scheduler_thread by delegating to thread_pool::submit_delayed (Epic #271)
  - When THREAD_HAS_COMMON_EXECUTOR is defined: delegates directly to thread_pool::submit_delayed
  - Removes all direct std::thread usage from thread_system_adapter.cpp (except std::thread::hardware_concurrency)
  - Simplifies code by ~70 lines and removes priority queue, mutex, condition variable
  - Maintains fallback for builds without common_system integration

### Removed
- **Unused Pipeline Compression/Encryption Code**: Removed dead code following Simple Design principle (#378)
  - Removed `internal::pipeline` struct and `make_default_pipeline()` function
  - Removed `pipeline_`, `compress_mode_`, `encrypt_mode_` members from `messaging_client`, `messaging_session`, `secure_session`, `secure_messaging_client`
  - Removed `send_coroutine.h/cpp` and `pipeline.h/cpp` files
  - These members were always set to `false` and the pipeline functions were no-op stubs
  - Simplifies code by ~300 lines across multiple files
  - `utils::compression_pipeline` (actual LZ4/gzip implementation) remains available as standalone utility

### Refactored
- **Memory Profiler**: Migrated from `std::thread` to `thread_integration_manager::submit_delayed_task()` (#277)
  - Removed dedicated `std::thread worker_` member
  - Periodic sampling now runs through shared thread pool
  - Uses delayed task scheduling for interval-based snapshots
  - Cleaner shutdown via atomic flag (no thread join needed)

- **gRPC Client Async Calls**: Migrated `call_raw_async()` from `std::thread().detach()` to `thread_integration_manager::submit_task()` (#278)
  - Async gRPC calls now submitted to shared thread pool instead of creating detached threads
  - Enables controlled thread lifecycle through centralized thread management
  - Prevents thread explosion under high async call volume

- **Send Coroutine Fallback**: Migrated `async_send_with_pipeline_no_co()` from `std::thread().detach()` to `thread_integration_manager::submit_task()` (#274)
  - Tasks now submitted to shared thread pool instead of creating detached threads
  - Controlled thread lifecycle through managed pool
  - Prevents resource exhaustion under high load
  - Added error handling for thread pool submission failures

### Added
- **QUIC Protocol Support (Phase 4.2)**: messaging_quic_server public API
  - `messaging_quic_server` class following `messaging_server` pattern
    - Consistent API with existing TCP server
    - Thread-safe session management using shared_mutex
    - Full Result<T> error handling
  - Server lifecycle: `start_server()`, `stop_server()`, `wait_for_stop()`
  - Session management:
    - `sessions()` to get all active sessions
    - `get_session()` to retrieve session by ID
    - `session_count()` for active connection count
    - `disconnect_session()` and `disconnect_all()` for termination
  - Broadcasting:
    - `broadcast()` to send to all clients
    - `multicast()` to send to specific sessions
  - `quic_session` class for individual client connections:
    - `send()` for default stream data
    - `send_on_stream()` for stream-specific data
    - `create_stream()` for new streams
    - `stats()` for connection statistics
  - Callback system:
    - `set_connection_callback()` for new clients
    - `set_disconnection_callback()` for client disconnects
    - `set_receive_callback()` for data from any session
    - `set_stream_receive_callback()` for stream data
    - `set_error_callback()` for error handling
  - Configuration via `quic_server_config`:
    - TLS certificate settings (cert_file, key_file, ca_cert_file)
    - Client certificate authentication (require_client_cert)
    - ALPN protocol negotiation
    - Transport parameters (timeouts, flow control limits)
    - Connection limits (max_connections)
    - DoS protection (enable_retry, retry_key)
  - Compatibility layer headers in network_system namespace
  - Example application (quic_server_example.cpp)
  - Comprehensive test suite with 20+ test cases

- **QUIC Protocol Support (Phase 4.1)**: messaging_quic_client public API
  - `messaging_quic_client` class following `messaging_client` pattern
    - Consistent API with existing TCP/UDP/WebSocket clients
    - Thread-safe operations with internal locking
    - Full Result<T> error handling
  - Connection management: `start_client()`, `stop_client()`, `wait_for_stop()`
  - Data transfer on default stream: `send_packet()`
  - Multi-stream support (QUIC specific):
    - `create_stream()` for bidirectional streams
    - `create_unidirectional_stream()` for unidirectional streams
    - `send_on_stream()` for stream-specific data
    - `close_stream()` for stream termination
  - Callback system for events:
    - `set_receive_callback()` for default stream data
    - `set_stream_receive_callback()` for all stream data
    - `set_connected_callback()`, `set_disconnected_callback()`
    - `set_error_callback()` for error handling
  - Configuration options via `quic_client_config`:
    - TLS settings (CA cert, client cert, key, verification)
    - ALPN protocol negotiation
    - Transport parameters (timeouts, flow control limits)
    - 0-RTT early data support
  - Connection statistics via `stats()` method
  - Example code demonstrating API usage
  - Comprehensive test suite with 22 test cases

- **QUIC Protocol Support (Phase 3.2)**: Connection State Machine (RFC 9000 Section 5)
  - `transport_parameters` struct with RFC 9000 Section 18 compliant encoding/decoding
    - All standard transport parameters (max_idle_timeout, flow control limits, etc.)
    - Connection ID parameters (original/initial/retry)
    - Server-only parameters (preferred_address, stateless_reset_token)
    - Validation per client/server role
  - `connection` class implementing the connection state machine
    - Connection states: idle, handshaking, connected, closing, draining, closed
    - Handshake states: initial, waiting_server_hello, waiting_finished, complete
    - Packet number spaces: Initial, Handshake, Application
    - Long and short header packet processing
    - Frame handling via std::visit pattern
    - Connection ID management (add, retire, rotation)
    - Transport parameter negotiation
    - Connection close with error code preservation
    - Idle timeout handling
    - Integration with stream_manager and flow_controller
  - `preferred_address_info` struct for server address migration support
  - Comprehensive test suite with 40 test cases

- **QUIC Protocol Support (Phase 3.1)**: Stream Management (RFC 9000 Sections 2-4)
  - `stream` class with complete send/receive state machine
  - Stream ID allocation for client/server roles (bidi/uni)
  - `stream_manager` for stream lifecycle and multiplexing
  - `flow_controller` for connection-level flow control
  - Data reassembly with gap handling
  - Flow control enforcement at stream level
  - MAX_STREAM_DATA and MAX_DATA frame generation
  - Comprehensive test suite

- **QUIC Protocol Support (Phase 2.2)**: QUIC Socket with packet protection
  - `quic_socket` class wrapping UDP socket with QUIC packet protection
  - Connection management: `connect()`, `accept()`, `close()`
  - Stream-based data transfer: `create_stream()`, `send_stream_data()`
  - Callback-based async I/O for stream data, connection events, errors
  - Thread-safe implementation with internal locking
  - Connection state machine (idle, handshake, connected, closing, draining, closed)
  - Integration with QUIC crypto for TLS 1.3 handshake
  - Comprehensive test suite with 18 test cases

- **QUIC Protocol Support (Phase 1.2)**: Frame types and parsing (RFC 9000 Section 12, 19)
  - `frame_types.h` with all QUIC frame type definitions and structures
  - `frame_parser` class for parsing frames from raw bytes
  - `frame_builder` class for serializing frames to bytes
  - Support for all RFC 9000 frame types:
    - PADDING, PING (connection management)
    - ACK, ACK_ECN (acknowledgment with ECN support)
    - CRYPTO (handshake data)
    - STREAM (data transfer with FIN/LEN/OFF flags)
    - MAX_DATA, MAX_STREAM_DATA, MAX_STREAMS (flow control)
    - DATA_BLOCKED, STREAM_DATA_BLOCKED, STREAMS_BLOCKED
    - NEW_CONNECTION_ID, RETIRE_CONNECTION_ID
    - PATH_CHALLENGE, PATH_RESPONSE
    - CONNECTION_CLOSE (transport and application)
    - HANDSHAKE_DONE, NEW_TOKEN
  - Comprehensive test suite with 37 test cases

- **QUIC Protocol Support (Phase 1.1)**: Variable-length integer encoding (RFC 9000 Section 16)
  - `varint` class with encode/decode methods
  - Support for 1, 2, 4, and 8 byte encodings based on value range
  - `encode_with_length` for minimum length requirements
  - Constexpr helper functions for length calculation
  - Comprehensive test suite with RFC 9000 example values

### Planned
- C++20 coroutine full integration
- HTTP/2 and HTTP/3 support
- Advanced load balancing algorithms

---

## [1.5.0] - 2025-11-14

### Added
- **HTTP Advanced Features**: Significantly improved HTTP/1.1 implementation
  - **Request Buffering**: Complete multi-chunk request support with Content-Length validation
    - Proper header/body boundary detection
    - Configurable maximum request size (10MB default)
    - Memory-safe buffer management

  - **Cookie Support**: Full HTTP cookie parsing and management (RFC 6265 compliant)
    - Cookie attributes support (expires, max-age, domain, path, secure, httponly, samesite)
    - Multiple cookie handling

  - **Multipart/Form-data**: Complete file upload support
    - Boundary detection and parsing
    - Multiple file handling
    - Content-Type and Content-Disposition parsing

  - **Chunked Encoding**: Both client and server support
    - Automatic chunk encoding for large responses
    - Memory-efficient streaming

  - **Automatic Compression**: gzip/deflate support via ZLIB
    - Content negotiation via Accept-Encoding header
    - Configurable compression threshold
    - ZLIB dependency integration

- **Container System Integration**: New flexible serialization API
  - `container_manager` singleton for message serialization
  - `container_interface` abstraction layer
  - `basic_container` standalone implementation (no external dependencies)
  - Automatic fallback when container_system unavailable
  - Thread-safe singleton pattern

### Fixed
- **Critical Bug Fixes**: Resolved production-blocking issues
  - **Memory Leaks**: Fixed circular reference in messaging_session (~900 bytes/connection)
    - Changed from shared_ptr to weak_ptr in callback lambdas
    - Prevents session objects from being kept alive by socket callbacks

  - **Deadlock**: Resolved lock-order-inversion in messaging_server
    - Fixed callback_mutex_ and session mutex interaction
    - Proper lock ordering in callback registration

  - **Thread Safety**: Fixed HTTP URL parsing data race
    - Changed url_regex to static const (C++11 magic statics)
    - Eliminates regex recompilation overhead

  - **TCP Shutdown**: Restored graceful connection termination
    - Proper EOF and operation-aborted event handling
    - Correct error propagation to handlers

### Changed
- **Test Infrastructure**: Migrated container_system tests to new API
  - Removed obsolete `#if 0` preprocessor blocks
  - Updated tests to use container_manager pattern
  - Simplified test data to use std::string
  - All tests work without container_system dependency

### Technical Details
- ZLIB version 1.2.12 integration
- AddressSanitizer verified: zero memory leaks
- ThreadSanitizer verified: no data races or deadlocks
- All unit tests passing (15/15)
- Compatible with container_system v2 API

---

## [1.4.0] - 2025-10-26

### Added
- **TLS/SSL Support (Phase 9)**: Complete encrypted communication implementation
  - **secure_tcp_socket (Phase 9.1)**:
    - SSL/TLS wrapper for TCP socket using `asio::ssl::stream`
    - Async SSL handshake support (client/server modes)
    - Encrypted async read/write operations
    - Follows same pattern as `tcp_socket` for consistency

  - **secure_session (Phase 9.1)**:
    - Manages single secure client session on server side
    - Performs server-side SSL handshake before data exchange
    - Inherits backpressure mechanism from Phase 8.2 (1000/2000 message limits)
    - Thread-safe message queue with mutex protection

  - **secure_messaging_server (Phase 9.2)**:
    - Secure server accepting TLS/SSL encrypted connections
    - Certificate chain and private key loading from files
    - SSL context configuration (sslv23, no SSLv2, single DH use)
    - Creates `secure_session` for each accepted connection
    - Inherits session cleanup from Phase 8.1 (periodic + on-demand)
    - Inherits monitoring support from common_system (optional)

  - **secure_messaging_client (Phase 9.3)**:
    - Secure client for TLS/SSL encrypted connections
    - Optional certificate verification (default: enabled)
    - Uses system certificate paths for verification when enabled
    - Client-side SSL handshake with 10-second timeout
    - Connection state management with atomic operations

  - **Build System (Phase 9.4)**:
    - Added `BUILD_TLS_SUPPORT` CMake option (default: ON)
    - Conditional compilation of TLS/SSL components
    - Automatic OpenSSL detection and linking
    - Updated build configuration summary to show TLS/SSL status
    - Compatible with existing WebSocket support (shared OpenSSL dependency)

### Changed
- Updated IMPROVEMENTS.md to mark Issue #4 (Add TLS/SSL Support) as completed
- Reorganized CMakeLists.txt to separate TLS/SSL sources from base library

### Technical Details
- Uses OpenSSL 3.6.0 for cryptographic operations
- Parallel class hierarchy maintains backward compatibility
- SSL context lifetime managed by server/client instances
- Handshake failures result in connection rejection with detailed error codes
- All secure components are thread-safe with proper synchronization

---

## [1.3.0] - 2025-10-26

### Added
- **Performance Optimization (Phase 8)**: Critical improvements to memory management and client connection efficiency
  - **Session Cleanup Mechanism (Phase 8.1)**:
    - Added `is_stopped()` method to `messaging_session` for state checking
    - Added `cleanup_dead_sessions()` to remove stopped sessions from vector
    - Added `start_cleanup_timer()` for periodic cleanup every 30 seconds
    - Protected `sessions_` vector with `sessions_mutex_` for thread safety
    - Cleanup triggered both periodically and on new connections
    - Prevents unbounded memory growth in long-running servers

  - **Receiver Backpressure (Phase 8.2)**:
    - Added `pending_messages_` queue (std::deque) to buffer incoming messages
    - Added `queue_mutex_` for thread-safe queue access
    - Set `max_pending_messages_` limit to 1000 messages
    - Log warning when queue reaches limit (backpressure signal)
    - Disconnect abusive clients when queue exceeds 2x limit (2000 messages)
    - Added `process_next_message()` to dequeue and handle messages
    - Prevents memory exhaustion from fast-sending clients

  - **Connection Pooling (Phase 8.3)**:
    - Added `connection_pool` class for reusable client connections
    - Pre-creates fixed number of connections at initialization
    - Thread-safe acquire/release semantics using mutex and condition variable
    - Blocks when all connections in use until one becomes available
    - Automatically reconnects lost connections when released
    - Tracks active connection count for monitoring
    - Configurable pool size (default: 10 connections)
    - Reduces connection overhead by up to 60%

### Changed
- Updated `messaging_server` thread safety documentation to reflect mutex-protected sessions
- Updated IMPROVEMENTS.md to mark Issues #1, #2, #3 as completed

### Technical Details
- Session cleanup uses `is_stopped()` check with atomic operations
- Backpressure applies at 1000 messages, disconnects at 2000
- Connection pool uses RAII for safe resource management
- All performance features are opt-in and backward compatible

---

## [1.2.0] - 2025-10-26

### Added
- **Network Load Testing Framework**: Comprehensive infrastructure for real network performance measurement
  - **Memory Profiler**: Cross-platform memory usage tracking
    - `MemoryProfiler`: RSS/heap/VM measurement for Linux, macOS, and Windows
    - Platform-specific implementations using /proc, task_info(), and GetProcessMemoryInfo()
    - Memory delta calculation for before/after comparisons

  - **Result Writer**: Performance metrics serialization
    - JSON and CSV output formats for test results
    - ISO 8601 timestamp generation
    - Structured data for latency (P50/P95/P99), throughput, and memory metrics

  - **TCP Load Tests**: End-to-end TCP performance testing
    - Throughput tests for 64B, 1KB, and 64KB messages
    - Concurrent connection tests (10 and 50 clients)
    - Round-trip latency measurement
    - Memory consumption tracking

  - **UDP Load Tests**: UDP protocol performance validation
    - Throughput tests for 64B, 512B, and 1KB datagrams
    - Packet loss detection and success rate tracking
    - Burst send performance (100 messages per burst)
    - Concurrent client tests (10 and 50 clients)

  - **WebSocket Load Tests**: WebSocket protocol benchmarking
    - Text and binary message throughput tests
    - Ping/pong latency measurement
    - Concurrent connection handling (10 clients)
    - Frame overhead analysis

- **CI/CD Automation**:
  - **GitHub Actions Workflow**: Automated load testing pipeline
    - Weekly scheduled runs (Sunday 00:00 UTC)
    - Manual trigger with baseline update option
    - Multi-platform support (Ubuntu 22.04, macOS 13, Windows 2022)
    - Automatic artifact upload with 90-day retention

  - **Metrics Collection Script**: Python-based result aggregation
    - `collect_metrics.py`: Parse and combine JSON results from all protocols
    - Summary statistics generation
    - Platform and compiler metadata tracking

  - **Baseline Update Script**: Automated documentation updates
    - `update_baseline.py`: Insert real network metrics into BASELINE.md
    - Per-platform result tracking
    - Dry-run mode for preview
    - Automated PR creation for baseline changes

- **Documentation**:
  - **Load Test Guide**: Comprehensive 400+ line testing documentation
    - Test structure and methodology
    - Result interpretation guide
    - Performance expectations per protocol
    - Troubleshooting section
    - Advanced profiling techniques

  - **Baseline Updates**: Real network performance sections
    - Added "Real Network Performance Metrics" to BASELINE.md and BASELINE_KO.md
    - Load test infrastructure documentation
    - Methodology and expected metrics sections

  - **README Updates**: Performance and testing sections
    - Synthetic benchmarks section
    - Real network load tests section
    - Automated baseline collection instructions
    - Updated documentation table with Load Test Guide

### Changed
- Updated Phase 0 checklist in BASELINE.md to reflect completed load testing infrastructure
- Reorganized integration test CMakeLists.txt with framework library
- Updated test suite count from 4 to 7 (added TCP, UDP, WebSocket load tests)

### Technical Details
- All load tests use localhost loopback for consistent measurements
- Tests automatically skip in CI environments to avoid timing-related flakiness
- Memory profiling uses platform-native APIs for accurate measurements
- Results include P50/P95/P99 latency percentiles for detailed analysis

---

## [1.1.0] - 2025-10-26

### Added
- **WebSocket Protocol Support (RFC 6455)**: Full-featured WebSocket client and server implementation
  - **messaging_ws_client**: High-level WebSocket client API
    - Text and binary message support
    - Configurable connection parameters (host, port, path, headers)
    - Automatic ping/pong keepalive mechanism
    - Graceful close handshake
    - Event callbacks for connection, disconnection, messages, and errors

  - **messaging_ws_server**: High-level WebSocket server API
    - Multi-client connection management
    - Broadcast support for all connected clients
    - Per-connection message handling
    - Session management with connection limits
    - Event callbacks for new connections, disconnections, and messages

  - **websocket_socket**: Low-level WebSocket framing layer
    - Built on top of existing tcp_socket infrastructure
    - WebSocket handshake (HTTP/1.1 Upgrade)
    - Frame encoding/decoding with masking support
    - Message fragmentation and reassembly
    - Control frame handling (ping, pong, close)

  - **websocket_protocol**: Protocol state machine
    - RFC 6455 compliant frame processing
    - UTF-8 validation for text messages
    - Automatic pong response to ping frames
    - Close code handling with graceful shutdown

  - **ws_session_manager**: WebSocket connection lifecycle management
    - Thread-safe connection tracking with shared_mutex
    - Connection limit enforcement
    - Backpressure signaling when threshold exceeded
    - Automatic connection ID generation
    - Connection metrics (total accepted, total rejected, active count)

- **Documentation**:
  - README.md updated with WebSocket features and usage examples
  - README_KO.md updated with Korean documentation
  - Architecture diagrams updated to include WebSocket components
  - API examples for both TCP and WebSocket protocols

### Changed
- Moved WebSocket support from "Planned Features" to "Core Features"
- Updated architecture diagrams to show WebSocket layer integration

### Security
- Frame masking for client-to-server communication (RFC 6455 requirement)
- UTF-8 validation for text frames to prevent malformed data
- Connection limits via ws_session_manager to prevent resource exhaustion

---

## [1.0.0] - 2025-10-22

### Added
- **Core Network System**: Modern C++20 asynchronous network library
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
| 1.4.0   | 2025-10-26   | C++20        | 3.16          | Current |
| 1.3.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.2.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.1.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.0.0   | 2025-10-22   | C++20        | 3.16          | Stable |
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

**Last Updated:** 2025-10-26
