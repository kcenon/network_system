# SOUP List &mdash; network_system

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-07 |
| network_system Version | 0.1.0 |

---

## Production SOUP

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Linking | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|---------|-----------------|
| SOUP-001 | [Standalone Asio](https://think-async.com/Asio/) | Christopher Kohlhoff | 1.30.2 | BSL-1.0 | Asynchronous I/O, networking, timers (core dependency) | B | Header-only | None |
| SOUP-002 | [zlib](https://zlib.net/) | Jean-loup Gailly, Mark Adler | 1.3.1 | Zlib | HTTP compression support (core dependency) | B | Dynamic | None (1.3+ fixes CVE-2023-45853) |

---

## Optional SOUP

| ID | Name | Manufacturer | Version | License | Usage | Safety Class | Linking | Known Anomalies |
|----|------|-------------|---------|---------|-------|-------------|---------|-----------------|
| SOUP-003 | [OpenSSL](https://www.openssl.org/) | OpenSSL Software Foundation | 3.3.0 | Apache-2.0 | TLS/SSL encryption for secure network communication (`ssl` feature) | C | Dynamic | None known at pinned version |
| SOUP-004 | [LZ4](https://github.com/lz4/lz4) | Yann Collet | 1.9.4 | BSD-2-Clause | Fast lossless compression algorithm | A | Dynamic | None |
| SOUP-005 | [gRPC](https://grpc.io/) | Google / CNCF | 1.51.1 | Apache-2.0 | High-performance RPC framework | B | Dynamic | None |
| SOUP-006 | [Protocol Buffers](https://protobuf.dev/) | Google | 3.21.12 | BSD-3-Clause | Serialization format for gRPC | B | Dynamic | None |

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
| **A** | No contribution to hazardous situation | Compression utilities, logging |
| **B** | Non-serious injury possible | Network I/O, data serialization |
| **C** | Death or serious injury possible | Encryption, access control |

---

## Version Pinning (IEC 62304 Compliance)

All SOUP versions are pinned in `vcpkg.json` via the `overrides` field:

```json
{
  "overrides": [
    { "name": "asio", "version": "1.30.2" },
    { "name": "zlib", "version": "1.3.1" },
    { "name": "openssl", "version": "3.3.0" },
    { "name": "lz4", "version": "1.9.4" },
    { "name": "grpc", "version": "1.51.1" },
    { "name": "protobuf", "version": "3.21.12" },
    { "name": "gtest", "version": "1.14.0" },
    { "name": "benchmark", "version": "1.8.3" }
  ]
}
```

The vcpkg baseline is locked in `vcpkg-configuration.json` to ensure reproducible builds.

---

## Version Update Process

When updating any SOUP dependency:

1. Update the version in `vcpkg.json` (overrides section)
2. Update the corresponding row in this document
3. Verify no new known anomalies (check CVE databases)
4. Run full CI/CD pipeline to confirm compatibility
5. Document the change in the PR description

---

## License Compliance Summary

| License | Count | Copyleft | Obligation |
|---------|-------|----------|------------|
| Apache-2.0 | 3 | No | Include license + NOTICE file |
| BSD-3-Clause | 2 | No | Include copyright + no-endorsement clause |
| BSL-1.0 | 1 | No | Include license text |
| BSD-2-Clause | 1 | No | Include copyright notice |
| Zlib | 1 | No | Include copyright notice |

> **No LGPL or copyleft dependencies** in network_system. All licenses are permissive and BSD-3-Clause compatible.
