# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Migration guide for transitioning from adapters to NetworkSystemBridge pattern
  - Comprehensive step-by-step migration instructions
  - API comparison tables for old vs new patterns
  - Common migration patterns and examples
  - Troubleshooting section

### Deprecated
- `thread_system_pool_adapter` class
  - Replaced by `ThreadPoolBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `common_thread_pool_adapter` class
  - Replaced by `ThreadPoolBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `common_logger_adapter` class
  - Replaced by `ObservabilityBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `common_monitoring_adapter` class
  - Replaced by `ObservabilityBridge` from `network_system_bridge.h`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

- `bind_thread_system_pool_into_manager()` function
  - Replaced by `NetworkSystemBridge::with_thread_system()`
  - Will be removed in v3.0.0
  - See [migration guide](docs/migration/adapter_to_bridge_migration.md)

### Deprecation Timeline
- **v2.1.0** (current): Deprecated adapters marked with `[[deprecated]]` attribute
- **v2.2.0** (Q2 2026): Migration strongly encouraged
- **v3.0.0** (Q3 2026): Deprecated adapters will be removed

Users are encouraged to migrate to the new `NetworkSystemBridge` facade as soon as possible.
See the [migration guide](docs/migration/adapter_to_bridge_migration.md) for detailed instructions.

---

## How to Read This Changelog

- **Added**: New features
- **Changed**: Changes in existing functionality
- **Deprecated**: Features that will be removed in future versions
- **Removed**: Removed features
- **Fixed**: Bug fixes
- **Security**: Security fixes

For migration assistance, please refer to the migration guides in the `docs/migration/` directory.

---

## [v2.0.0] - 2026-01-18

### Added
- Unified architecture with Type Erasure infrastructure for `session_manager` (#525, #526)
- Unified core interfaces for all protocols (Phase 1) (#523)
- Unified messaging client/server templates for TCP protocol (#511, #512)
- Unified UDP messaging templates (#518)
- Unified type aliases for WebSocket and QUIC protocols (#519)
- TLS policy interfaces and protocol tags (#510)
- Unified socket abstraction using C++20 concepts (#501)
- `startable_base` lifecycle pattern extraction (#500)
- TCP, UDP, WebSocket, and QUIC protocol factory functions (Phase 2) (#528, #534, #535, #537)
- Backward compatibility aliases and migration guide (#536)
- OpenTelemetry-compatible distributed tracing (#460)
  - TCP, HTTP/2, QUIC, and gRPC protocol instrumentation (#468, #469, #471, #476)
  - OTLP HTTP exporter and sampling support (#470)
  - Comprehensive unit and integration tests (#472, #477)
- Histogram metrics support for latency distributions (#453)
- Unified `SessionManager<T>` template with comprehensive tests (#465, #466)
- Circuit breaker pattern for resilient network clients (#420)
- QUIC enhancements:
  - 0-RTT session ticket storage and restoration (#419)
  - ECN feedback integration into congestion control (#421)
  - Connection ID storage and rotation management (#415)
  - Path MTU Discovery (PMTUD) per RFC 8899 (#439)
- HTTP/2 server with TLS and h2c support (#438)
- Interface classes and utility infrastructure (Phase 1.2) (#427)

### Changed
- Migrated all protocol classes from CRTP to composition pattern:
  - TCP classes (#445)
  - UDP classes (#446)
  - WebSocket classes (#447, #431)
  - QUIC classes (#448, #432)
  - Secure classes (#452)
- Reorganized protocol implementations into layered modules (#520)
- Replaced magic callback indices with named enum types (#502)
- Consolidated Result type aliases and added internal namespace (#496)
- Renamed `_KO.md` documentation files to `.kr.md` for naming consistency

### Removed
- `compatibility.h` and `network_module` namespace aliases (#488)
- Deprecated WebSocket API methods (#487)
- Deprecated UDP API methods (#490)
- Deprecated `generate_session_id()` method (#489)

### Fixed
- `io_context` lifecycle management issues (#400, #410)
- Thread pool API compatibility for `thread_system` integration (#493, #495)
- PTO timeout loss detection handling for QUIC (#414, #418)

### Breaking Changes
- **OpenSSL 3.x only**: Dropped OpenSSL 1.1.1 support; OpenSSL 3.x is now required (#491)
- **Namespace aliases removed**: `network_module` namespace no longer available (#488)
- **Deprecated APIs removed**: WebSocket, UDP, and session manager deprecated methods removed (#487, #489, #490)

---

## [v1.5.0] - 2026-01-01

### Added
- CRTP base classes for messaging clients and servers (#386)
- C++20 module files for `kcenon.network` (#396)
- Official gRPC library integration:
  - Infrastructure and dependency configuration (#366, #367)
  - Wrapper layer for official gRPC library (#368)
  - Service registration mechanism (#369)
  - Testing and documentation (#370)
- Session idle timeout cleanup (#359)
- Send-side backpressure mechanism (#358)
- vcpkg ecosystem dependencies and standardized manifest (#373, #374)

### Changed
- Migrated messaging classes to CRTP common base pattern:
  - `messaging_client` to `messaging_client_base` (#387)
  - `messaging_server` to `messaging_server_base` (#390)
  - Secure messaging classes (#391)
  - UDP and WebSocket classes (#393)
  - QUIC classes (#394)
- Consolidated build directories into CMake options (#388)
- Removed unused pipeline compression/encryption code (#389)
- Removed `/network_system/` namespace compatibility layer (#379)
- Created adapter between local `thread_pool_interface` and common `IThreadPool` (#351)
- Decoupled monitoring integration via EventBus pattern (#348)

### Fixed
- CMake compile definition propagation for `KCENON_WITH_COMMON_SYSTEM` (#341)
- Thread-safe `localtime_r`/`localtime_s` usage in `get_timestamp` (#340)
- `log_entry` API compatibility for common_system v3.0.0 (#337)
- Session cleanup order to prevent heap corruption (#334)

---

## [v1.4.0] - 2025-12-21

### Added
- TCP receive dispatch benchmark comparing `std::span` vs `std::vector` (#325)
- OpenSSL 3.x migration support (#312)
- `common_system` `Result<T>` adoption for unified error handling (#311)

### Changed
- Unified namespace from `network_system` to `kcenon::network` (#331)
  - Added backward compatibility namespace aliases
  - Added backward compatibility aliases to `forward.h`
- Migrated TCP receive path to `std::span` for zero-copy performance:
  - `tcp_socket` std::span receive callback (#321)
  - `secure_tcp_socket` std::span receive callback (#322)
  - WebSocket TCP receive path migration (#323)
  - Internal messaging consumption of std::span callback (#324)
- Switched integration flags to `KCENON_WITH_*` macros (#336)

### Fixed
- Intentional Leak pattern applied to `basic_thread_pool` (#313)

---

## [v1.3.0] - 2025-12-09

### Added
- C++20 Concepts integration from updated common_system (#295)
- Comprehensive C++20 Concepts documentation (#297)
- `io_context_thread_manager` for unified thread management (#286)

### Changed
- Complete `std::thread` migration to `thread_system` (#290):
  - Memory profiler migrated to thread_pool (#288)
  - Logging migrated to common_system `ILogger` interface (#287)
  - gRPC client async calls migrated to thread_pool (#284)
  - Health monitor integrated with thread_system (#283)
  - `send_coroutine` fallback migrated to thread_pool (#282)
  - Replaced `basic_thread_pool` with `thread_system::thread_pool` (#280)
- Eliminated `scheduler_thread` from `thread_system_adapter` (#291)
- Replaced `sleep_for` with synchronization primitives in tests (#293)
- Updated version format to 0.x.x.x for pre-release documentation

### Fixed
- Replaced `std::thread().detach()` with scheduler in `submit_delayed` (#281)

---

## [v1.2.0] - 2025-12-04

### Added
- QUIC protocol implementation (RFC 9000, RFC 9001, RFC 9002):
  - Variable-length integer encoding (#259)
  - Frame types and parsing (#260)
  - Packet header encoding/decoding (#261)
  - QUIC-TLS integration (#262)
  - QUIC socket with packet protection (#263)
  - Stream management (#264)
  - Connection state machine (#265)
  - Loss detection and congestion control (#266)
  - `messaging_quic_client` (#267) and `messaging_quic_server` (#268)
  - Integration tests and documentation (#269, #270)

### Changed
- Removed `fmt` library dependency; now uses C++20 `std::format` exclusively (#258)

### Fixed
- Build: resolved `-Werror` warnings for clean compilation
- OpenSSL linkage made PUBLIC for dependent targets (#123)

---

## [v1.1.0] - 2025-11-30

### Added
- HTTP/2 client support with TLS 1.3 and ALPN (#109)
- gRPC support:
  - Unary RPC using HTTP/2 transport (#111)
  - Streaming RPC support (#113)
- DTLS support for secure UDP communication (#107)
- Network input validation (TICKET-006, P1 High) (#103)
- Network context service registration for dependency injection (#118)
- Automated SBOM generation workflow (#115)
- Version 1.0.0 designation for unified_system integration (#119)

### Changed
- Migrated HTTP parsing functions to `Result` type (#117)
- Migrated `initialize`/`shutdown` to `VoidResult` (#116)
- Standardized C++20 compiler requirements documentation (#120)

### Security
- Enforced TLS 1.3 as minimum version (#102)

### Fixed
- WebSocket E2E test failures (#101)

---

## [v1.0.0] - 2025-11-25

### Added
- HTTP/2 protocol foundation: frame layer and HPACK compression (#98)
- HTTP/2 protocol support design (NET-301) (#99)
- gRPC integration prototype (NET-304) (#100)
- HTTP benchmarks and WebSocket E2E tests (#95)
- Namespace refactoring, memory profiling, and static analysis (#96)
- Documentation and testing improvements (NET-306, NET-303, NET-305) (#97)
- Monitoring, error handling, and test improvements (#94)
- Kanban board documentation for work tracking

### Changed
- Enhanced CI workflows with comprehensive dependency management and sanitizer testing (#87, #88)
- Enhanced dependency detection and test target naming (#89)
- Added forward declarations and modernized network components (#90)
- Applied consistent code formatting to source files (#91)
- Updated namespace and disabled warning suppressions (#93)

### Fixed
- Namespace inconsistency in integration headers (#92)

---

## [v0.8.0] - 2025-11-14

### Added
- HTTP Cookie and Multipart/form-data support (#83)
- HTTP advanced features (Phase 4) (#85)
- HTTP request buffering and connection management (#82)
- ZLIB support for HTTP compression (#81)
- Monitoring system and configuration framework integration (#80)
- thread_system integration with messaging_server and messaging_client (#78, #79)

### Changed
- Migrated container_system tests to new API (Phase 7) (#86)
- Consolidated build scripts into `scripts/` directory (#77)
- Standardized BSD 3-Clause license (#76)

### Fixed
- Critical concurrency and thread safety issues (#84)
- Namespace compatibility with common_system (#75)

---

## [v0.7.0] - 2025-11-01

### Added
- HTTP/1.1 server and client implementation (#70)
- HTTP Phase 1 remediation for server/client (#71)
- HTTP Phase 2 core fixes and enhancements
- Chunked transfer encoding support
- Automatic response compression (gzip/deflate)
- Cookie and multipart/form-data parsing infrastructure
- Callback infrastructure for messaging

### Fixed
- TCP graceful shutdown on peer disconnects
- Use-after-move bug in PartialMessageRecovery test
- HTTP integration test timeout from incorrect executor type check
- Circular reference in messaging_server session callbacks
- Thread safety issue in `http_url::parse()`
- Memory leak from circular reference in `messaging_session`
- HTTP request buffering and synchronous response transmission
- zlib added to vcpkg dependencies for Windows builds

---

## [v0.6.0] - 2025-10-26

### Added
- WebSocket protocol implementation (RFC 6455):
  - Phase 1: Frame layer (#53)
  - Phase 2: Handshake layer (#54)
  - Phase 3: Protocol layer (#55)
  - Phase 4: Socket layer (#56)
  - Phase 5: High-level client/server API (#57)
  - Phase 6: Session management (#58)
- Network load testing framework (Phase 7) (#59)
- Performance optimization (Phase 8) (#60)
- TLS/SSL support for secure communication (#61)
- TLS 1.3 support (#62)
- Stability and reliability features (#63)
- Monitoring system integration (#64)
- Performance optimizations (#65)
- UDP reliability layer (#66)
- Callback mechanism for TCP and Secure TCP protocols (#67)

### Fixed
- Build warnings for parent project integration with strict settings (#68)

---

## [v0.5.0] - 2025-10-24

### Added
- UDP protocol support:
  - UDP socket foundation
  - UDP server and client implementation
  - UDP build configuration option
  - UDP documentation and examples
- UDP always available like TCP (#51)
- Samples and integration tests enabled (#69)

### Changed
- Moved UDP example to samples directory

### Fixed
- Deprecated warning suppression from external headers
- `verify_build` and integration tests made optional

---

## [v0.4.0] - 2025-10-17

### Added
- Session timeout mechanism for idle connection management (#44)
- TLS/SSL configuration infrastructure (#43)
- Thread-safe session manager with backpressure (#41)
- Integration testing suite (Phase 5) (#38)
- Comprehensive documentation:
  - Korean translations for all system documentation
  - Architecture documentation with comprehensive technical details
  - Phase documentation consolidated into permanent guides

### Changed
- Optimized move semantics and added comprehensive thread safety docs (#40)
- Removed embedded dependencies; uses centralized Sources directory (#46)

### Fixed
- Async send pipeline lifetime issues and coroutine safety (#39)
- Messaging_client destructor safety (#42)
- Global `BUILD_INTEGRATION_TESTS` flag respected (#45)

---

## [v0.3.0] - 2025-10-07

### Added
- Comprehensive performance benchmarking infrastructure (#23)
- Sanitizer CI/CD pipeline (TSAN/ASAN/UBSAN) (#24)
- Baseline performance metrics documentation (#25)
- Thread safety tests (#27)
- Phase 3 error handling preparation (#30)
- API documentation configuration (Doxygen) (#18)

### Changed
- Simplified CMake configuration from 837 to 215 lines (#16)
- Enabled common_system integration by default (#17)
- Renamed `USE_COMMON_SYSTEM` to `BUILD_WITH_COMMON_SYSTEM` (#14)
- Added monitoring integration support (#15)

### Fixed
- Thread safety in session and socket management (#26)
- Re-enabled tests and samples in CMakeLists.txt (#29)
- CI badge and removed redundant workflow files (#22)
- Doxygen workflow configuration (#34)

---

## [v0.2.0] - 2025-09-28

### Added
- common_system integration adapter (#10)
- common_system integration support (#11)
- Thread system integration and external project updates (#9)
- logger_system integration for structured logging (#6)

### Changed
- Aligned dependency versions with unified configuration (#12)
- Comprehensive documentation reorganization and standardization (#7)
- Removed all hardcoded version information from codebase (#8)

### Fixed
- Namespace consistency and removed duplicate headers (#13)
- Build errors from version removal

---

## [v0.1.0] - 2025-09-20

### Added
- **Core Infrastructure**
  - Complete separation from messaging_system (Phase 1-5) (#1, #2, #3, #4, #5)
  - New namespace structure: `network_system::{core,session,internal,integration}`
  - ASIO-based asynchronous networking with C++20 coroutines
  - Messaging bridge for backward compatibility
- **Build System**
  - CMake configuration with vcpkg support
  - Flexible dependency detection (ASIO/Boost.ASIO)
  - Cross-platform support (Linux, macOS, Windows)
- **CI/CD Pipeline**
  - GitHub Actions workflows for Ubuntu (GCC/Clang) and Windows (Visual Studio/MSYS2)
  - Dependency security scanning with Trivy
  - License compatibility checks
- **Container Integration**
  - Full integration with container_system
  - Value container support in messaging bridge
- **Documentation**
  - Doxygen configuration for API documentation
  - Comprehensive README with build instructions
  - Architecture documentation
- **Samples**
  - Comprehensive sample programs for network system

---

## [v0.0.1] - 2025-07-28

### Added
- Initial extraction of network_system as independent component from messaging_system
- Basic TCP client/server functionality
- Session management foundation
- Message pipeline processing
- High-performance asynchronous messaging infrastructure
