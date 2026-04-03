# network_system

## Overview

Modern C++20 asynchronous network library providing reusable transport primitives for
distributed systems and messaging applications. ASIO-based non-blocking I/O with
coroutine-friendly async model, pluggable protocol stack, and TLS 1.2/1.3 support.

## Architecture

```
include/kcenon/network/
  facade/        - Simplified protocol facades (tcp, udp, websocket, http, quic)
  protocol/      - Protocol factory functions and tag types
  interfaces/    - Core abstractions (i_protocol_client, i_protocol_server, i_session)
  detail/        - Implementation details (protocol tags, session types, metrics, tracing)
  config/        - Configuration types
  policy/        - TLS policies (policy-based TLS configuration)
  concepts/      - C++20 concepts (Protocol concept)
  unified/       - unified_messaging_client/server<Protocol, TlsPolicy> templates

libs/            - Modular protocol libraries:
  network-core, network-tcp, network-udp, network-websocket,
  network-http2, network-quic, network-grpc, network-all

src/
  facade/        - Facade implementations
  core/          - Client/server (messaging_client, messaging_server, connection_pool)
  internal/      - Socket implementations (tcp, udp, websocket, quic, dtls, secure_tcp)
  protocol/      - Protocol factory implementations
  session/       - Session management
```

Key abstractions:
- **Protocol tags** — `tcp_protocol`, `udp_protocol`, `websocket_protocol`, `quic_protocol`
  with C++20 `Protocol` concept constraints
- **Unified templates** — `unified_messaging_client<Protocol, TlsPolicy>` /
  `unified_messaging_server<Protocol, TlsPolicy>`
- **Facade layer** — `tcp_facade`, `udp_facade`, `websocket_facade`, `http_facade` hiding
  template complexity with config structs
- **Protocol factories** — `protocol::tcp::connect()`, `protocol::tcp::listen()` returning interfaces
- **Pipeline** — Compression/encryption message transformation before send

## Build & Test

```bash
# CMake presets (recommended)
cmake --preset release && cmake --build build
cd build && ctest --output-on-failure

# Manual
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

Key CMake options:
- `BUILD_TESTS` (ON) — Google Test unit tests
- `BUILD_WEBSOCKET_SUPPORT` (ON) — WebSocket (RFC 6455)
- `BUILD_MESSAGING_BRIDGE` (ON) — messaging_system bridge
- `NETWORK_BUILD_BENCHMARKS` (OFF) — Google Benchmark suite
- `NETWORK_BUILD_INTEGRATION_TESTS` (ON) — Integration tests
- `NETWORK_BUILD_MODULES` (OFF) — C++20 modules (CMake 3.28+)
- `NETWORK_ENABLE_GRPC_OFFICIAL` (OFF) — Official grpc++ wrapper
- `BUILD_WITH_COMMON_SYSTEM` (ON), `BUILD_WITH_THREAD_SYSTEM` (ON)
- `BUILD_WITH_LOGGER_SYSTEM` (OFF), `BUILD_WITH_CONTAINER_SYSTEM` (ON)

Presets: `default`, `debug`, `release`, `asan`, `tsan`, `ubsan`, `ci`, `vcpkg`

CI: Multi-platform (Ubuntu GCC/Clang, macOS, Windows MSVC), sanitizers, coverage (~80%),
static analysis, fuzzing, benchmarks, load tests, CVE scan, SBOM.

## Key Patterns

- **Facade API** — Each protocol has a facade with config structs returning interface pointers;
  no template parameters exposed to users
- **Protocol modules** — C++20 concepts constrain protocol tags with compile-time metadata
  (`name`, `is_connection_oriented`, `is_reliable`)
- **Coroutine-based I/O** — `asio::async_initiate` with completion tokens, thread pool
  integration for pipeline processing
- **Pipeline architecture** — Pluggable compress/encrypt hooks applied before async send
- **Result\<T\>** — Error handling from common_system; ~75-80% migration complete
- **EventBus metrics** — No compile-time dependency on monitoring_system; uses EventBus-based
  metric publishing via common_system

## Ecosystem Position

**Tier 4** — Transport layer, highest infrastructure tier.

```
common_system      (Tier 0) [required]
thread_system      (Tier 1) [required]
container_system   (Tier 1) [optional] — serialization
logger_system      (Tier 2) [optional] — runtime binding via ILogger
monitoring_system  (Tier 3) [optional] — EventBus subscriber only
network_system     (Tier 4)  <-- this project
```

Consumed by: pacs_system (Tier 5), messaging_system, database_system.

## Dependencies

**Required**: kcenon-common-system, kcenon-thread-system, ASIO >= 1.30.2, OpenSSL 3.x, zlib >= 1.3, fmt 10.0.0+
**Optional ecosystem**: kcenon-logger-system, kcenon-container-system
**Optional external**: lz4 1.9.4 (compression), gRPC 1.60.0 + protobuf 4.25.1
**Dev/test**: Google Test 1.17.0, Google Benchmark 1.9.5

## Known Constraints

- C++20 required; GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+
- Sanitizers are mutually exclusive (only one of ASAN/TSAN/UBSAN per build)
- OpenSSL 1.1.1 EOL: build warns, strongly recommends 3.x
- Result\<T\> migration ~75-80% complete (some APIs still use exceptions)
- C++20 modules experimental (CMake 3.28+)
- gRPC official integration disabled by default
- UWP/Xbox unsupported
