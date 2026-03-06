# SOUP List &mdash; network_system

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-06 |
| network_system Version | 1.0.0 |

---

## Internal Ecosystem Dependencies (Optional `ecosystem` Feature)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| INT-001 | [common_system](https://github.com/kcenon/common_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Result&lt;T&gt; pattern, error handling primitives | B | None |
| INT-002 | [thread_system](https://github.com/kcenon/thread_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Thread pool, async task scheduling | B | None |
| INT-003 | [logger_system](https://github.com/kcenon/logger_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Structured logging infrastructure | A | None |
| INT-004 | [container_system](https://github.com/kcenon/container_system) | kcenon | Latest (vcpkg / source) | BSD-3-Clause | Serializable data containers for message payloads | B | None |
| INT-005 | [monitoring_system](https://github.com/kcenon/monitoring_system) | kcenon | Latest (source) | BSD-3-Clause | Performance metrics collection | A | None |

> **Note**: Ecosystem dependencies are enabled via the optional `ecosystem` vcpkg feature. network_system can operate independently with only ASIO, fmt, and zlib.

---

## Production SOUP (Required)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-001 | [ASIO](https://github.com/chriskohlhoff/asio) (standalone) | Christopher Kohlhoff | 1.30.2 | BSL-1.0 | Foundation for all async network I/O (TCP/UDP) | B | None |
| SOUP-002 | [fmt](https://github.com/fmtlib/fmt) | Victor Zverovich | 10.2.1 | MIT | String formatting for log messages and protocol handling | A | None |
| SOUP-003 | [zlib](https://www.zlib.net/) | Jean-loup Gailly, Mark Adler | 1.3.1 | zlib License | Gzip/deflate data compression for network payloads | A | None |

### System Dependencies

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-004 | POSIX Threads (pthreads) | POSIX / OS vendor | System-provided | N/A (OS) | Concurrent network operations via `find_package(Threads)` | B | None |

---

## Optional SOUP

### SSL/TLS Feature (`ssl`)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-005 | [OpenSSL](https://www.openssl.org/) | OpenSSL Project | 3.3.0 | Apache-2.0 | TLS/SSL encryption for secure network communication | C | CVE tracking via vendor advisories required |

### Compression (optional)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-006 | [LZ4](https://github.com/lz4/lz4) | Yann Collet | 1.9.4 | BSD-2-Clause | High-speed lossless data compression for network payloads | A | None |

### gRPC Feature (optional)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-007 | [gRPC](https://github.com/grpc/grpc) | Google | &ge;1.50.0 | Apache-2.0 | Official gRPC transport for inter-service communication | B | None |
| SOUP-008 | [Protocol Buffers](https://github.com/protocolbuffers/protobuf) | Google | &ge;3.21.0 | BSD-3-Clause | Serialization for gRPC transport | B | None |
| SOUP-009 | [Abseil](https://github.com/abseil/abseil-cpp) | Google | Latest (transitive) | Apache-2.0 | C++ utility library (transitive dependency via gRPC 1.50+) | A | None |

### Observability (network-core)

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|-----------------|
| SOUP-010 | [opentelemetry-cpp](https://github.com/open-telemetry/opentelemetry-cpp) | OpenTelemetry Authors (CNCF) | Latest | Apache-2.0 | Distributed tracing for network operations (network-core module) | A | None |

---

## Development/Test SOUP (Not Deployed)

| ID | Name | Manufacturer | Version | License | Usage | Qualification |
|----|------|-------------|---------|---------|-------|--------------|
| SOUP-T01 | [Google Test](https://github.com/google/googletest) | Google | 1.14.0 | BSD-3-Clause | Unit testing framework (includes GMock) | Required |
| SOUP-T02 | [Google Benchmark](https://github.com/google/benchmark) | Google | 1.8.3 | Apache-2.0 | Performance benchmarking framework | Not required |

---

## Safety Classification Key

| Class | Definition | Example |
|-------|-----------|---------|
| **A** | No contribution to hazardous situation | Logging, formatting, test frameworks |
| **B** | Non-serious injury possible | Data processing, network communication |
| **C** | Death or serious injury possible | Encryption, access control |

---

## Version Pinning (IEC 62304 Compliance)

All SOUP versions are pinned in `vcpkg.json` via the `overrides` field:

```json
{
  "overrides": [
    { "name": "asio", "version": "1.30.2" },
    { "name": "fmt", "version": "10.2.1" },
    { "name": "zlib", "version": "1.3.1" },
    { "name": "openssl", "version": "3.3.0" },
    { "name": "lz4", "version": "1.9.4" },
    { "name": "gtest", "version": "1.14.0" },
    { "name": "benchmark", "version": "1.8.3" }
  ]
}
```

The vcpkg baseline is locked in `vcpkg-configuration.json` to ensure reproducible builds.

### FetchContent Fallback (ASIO)

If standalone ASIO is not found via vcpkg or system paths, `cmake/NetworkSystemDependencies.cmake` fetches `asio-1-36-0` from upstream via CMake `FetchContent`. This fallback version should also be tracked as SOUP.

---

## Version Update Process

When updating any SOUP dependency:

1. Update the version in `vcpkg.json` (overrides section)
2. If ASIO FetchContent fallback version changed, update `cmake/NetworkSystemDependencies.cmake`
3. Update the corresponding row in this document
4. Verify no new known anomalies (check CVE databases, especially for OpenSSL)
5. Run full CI/CD pipeline to confirm compatibility
6. Document the change in the PR description

---

## License Compliance Summary

| License | Count | Copyleft | Obligation |
|---------|-------|----------|------------|
| BSL-1.0 | 1 | No | Include license |
| MIT | 1 | No | Include copyright notice |
| zlib License | 1 | No | Include copyright notice |
| Apache-2.0 | 4 | No | Include license + NOTICE file |
| BSD-2-Clause | 1 | No | Include copyright notice |
| BSD-3-Clause | 2 | No | Include copyright + no-endorsement clause |

> **GPL contamination**: None detected. All dependencies are permissively licensed.
