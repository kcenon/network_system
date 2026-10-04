---
doc_id: "NET-ADR-003"
doc_title: "ADR-003: Protocol Modularization Strategy"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Accepted"
project: "network_system"
category: "ADR"
---

# ADR-003: Protocol Modularization Strategy

> **SSOT**: This document is the single source of truth for **ADR-003: Protocol Modularization Strategy**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2025-08-01 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

network_system supports 6 protocols (TCP, UDP, WebSocket, HTTP/2, QUIC, gRPC).
Each protocol has distinct dependencies:
- TCP/UDP: ASIO only (no external deps beyond the core)
- WebSocket: ASIO + frame encoding
- HTTP/2: nghttp2 library
- QUIC: quiche or msquic library
- gRPC: official grpc++ library (heavy dependency)

Linking all protocols into a single library forces every consumer to pull all
protocol dependencies, even if they only use TCP. pacs_system needs TCP and HTTP;
monitoring_system needs only TCP for metric export.

## Decision

**Split protocol implementations into modular CMake library targets:**

```
network-core       - Common interfaces, config, utilities
network-tcp        - TCP client/server (ASIO)
network-udp        - UDP client/server (ASIO)
network-websocket  - WebSocket (ASIO + frame codec)
network-http2      - HTTP/2 (nghttp2)
network-quic       - QUIC (quiche/msquic)
network-grpc       - gRPC wrapper (grpc++)
network-all        - Meta-target linking all protocols
```

Each target is a separate CMake library with explicit `target_link_libraries`
dependencies. Consumers link only the protocols they need:

```cmake
# pacs_system: only TCP and HTTP
target_link_libraries(pacs_system PRIVATE
    network-tcp network-http2)

# Full network support
target_link_libraries(app PRIVATE network-all)
```

Protocol tags (`tcp_protocol`, `udp_protocol`, etc.) are defined in `network-core`
and constrained by a C++20 `Protocol` concept with compile-time metadata:
`name`, `is_connection_oriented`, `is_reliable`.

## Alternatives Considered

### Monolithic Library

- **Pros**: Simple build configuration, single link target.
- **Cons**: Forces all protocol dependencies on every consumer. gRPC alone
  adds protobuf, Abseil, c-ares, re2 — unacceptable for lightweight services.

### Header-Only with `#ifdef` Guards

- **Pros**: Single library, conditional compilation.
- **Cons**: Proliferating `#ifdef`s complicate code, break IDE navigation,
  and make testing combinatorially expensive. Difficult to verify that all
  `#ifdef` combinations compile correctly.

### Separate Repositories per Protocol

- **Pros**: Maximum independence, separate versioning.
- **Cons**: Shared code (connection management, session handling, config)
  would need to be duplicated or extracted into yet another repository.
  8 repositories in the ecosystem is already the practical limit.

## Consequences

### Positive

- **Minimal dependency footprint**: Consumers only link protocol dependencies
  they actually use. A TCP-only application has zero protocol-specific
  external dependencies beyond ASIO.
- **Independent compilation**: Protocol modules compile independently,
  enabling parallel builds and selective CI testing.
- **Clean dependency graph**: CMake target dependencies make protocol
  requirements explicit and auditable.
- **Incremental adoption**: New protocols can be added as new modules without
  modifying existing ones.

### Negative

- **Build system complexity**: 8 CMake targets (including `network-all`) with
  inter-target dependencies require careful `CMakeLists.txt` management.
- **Version coupling**: All protocol modules share the same version number
  and release cycle. An incompatible change in `network-core` affects all
  protocol modules.
- **Discovery overhead**: Users must know which protocol module to link.
  Mitigated by the `network-all` convenience target and documentation.

---

## See Also

- [Design Decisions](../DESIGN_DECISIONS.md) — Parent index of design decisions
- [Monolithic-to-Modular Migration](../MIGRATION.md) — Practical migration guide
- [ADR-001: gRPC Official Library Wrapper](ADR-001-grpc-official-library-wrapper.md)
- [ADR-002: Facade API Introduction](ADR-002-facade-api-introduction.md)
