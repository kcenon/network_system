---
doc_id: "NET-ADR-002"
doc_title: "ADR-002: Facade API Introduction"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Accepted"
project: "network_system"
category: "ADR"
---

# ADR-002: Facade API Introduction

> **SSOT**: This document is the single source of truth for **ADR-002: Facade API Introduction**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2025-06-01 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

network_system's core API uses `unified_messaging_client<Protocol, TlsPolicy>` and
`unified_messaging_server<Protocol, TlsPolicy>` templates that expose protocol
selection and TLS configuration as template parameters.

While powerful, this API has usability issues:
1. **Template complexity**: Users must understand `tcp_protocol`, `udp_protocol`,
   `tls_policy::require`, etc., before writing their first network call.
2. **Error messages**: Template instantiation errors span hundreds of lines,
   making debugging difficult for newcomers.
3. **Header cost**: Template-heavy headers increase downstream compile times.
4. **Type erasure**: Storing heterogeneous client/server instances requires
   `std::any` or type-erased wrappers.

Downstream projects (pacs_system, database_system) need a simpler entry point that
hides protocol mechanics while preserving the full-featured template API for power users.

## Decision

**Introduce a Facade layer** (`tcp_facade`, `udp_facade`, `websocket_facade`,
`http_facade`, `quic_facade`) that wraps the template API with config-struct-based
construction returning interface pointers.

```cpp
// Facade API — simple, no templates
auto server = tcp_facade::listen({
    .port = 8080,
    .tls = tls_config{.cert = "server.pem", .key = "server.key"},
    .on_message = [](auto session, auto msg) { /* ... */ }
});

// Template API — full control (still available)
auto server = unified_messaging_server<tcp_protocol, tls_policy::require>{config};
```

Design:
1. **Config structs** — Each facade takes a plain struct with sensible defaults.
   No template parameters, no trait types.
2. **Interface return** — Facades return `unique_ptr<i_protocol_server>` or
   `unique_ptr<i_protocol_client>`, hiding the concrete template type.
3. **Protocol factories** — `protocol::tcp::connect()`, `protocol::tcp::listen()`
   provide an alternative functional API.
4. **Coexistence** — The facade layer calls the template API internally. Both
   APIs are supported and documented.

## Alternatives Considered

### Template API Only (Status Quo)

- **Pros**: Maximum flexibility, no additional abstraction layer.
- **Cons**: Steep learning curve, poor error messages, prevents type erasure.
  Downstream projects wrote their own wrappers inconsistently.

### Type-Erased Client/Server (Single Class)

- **Pros**: One class for all protocols, simplest possible API.
- **Cons**: Loses protocol-specific features (UDP multicast, WebSocket frames,
  HTTP headers). A single interface cannot capture all protocol semantics.

### Builder Pattern (Fluent API)

- **Pros**: Discoverable API with IDE autocompletion.
- **Cons**: Builders that produce different concrete types based on method calls
  (`.tcp()` vs `.udp()`) either require runtime dispatch or become templates
  themselves, reintroducing the original complexity.

## Consequences

### Positive

- **Low barrier to entry**: New users write working network code in 5-10 lines
  using config structs. No template knowledge required.
- **Clean separation**: Facade users see `i_protocol_server` interface; power
  users access `unified_messaging_server<Protocol, TlsPolicy>` directly.
- **Reduced compile times**: Facade headers include only interface definitions,
  not full template implementations.
- **Heterogeneous storage**: `unique_ptr<i_protocol_server>` allows containers
  of mixed-protocol servers.

### Negative

- **Two API surfaces**: Both facade and template APIs must be documented,
  tested, and maintained. Feature additions must be reflected in both.
- **Runtime dispatch**: Facade-created objects use virtual dispatch instead of
  static dispatch. Overhead is ~2-5 ns per call — negligible for I/O operations.
- **Feature access**: Some protocol-specific features require downcasting from
  the interface to the concrete type, partially defeating the abstraction.
