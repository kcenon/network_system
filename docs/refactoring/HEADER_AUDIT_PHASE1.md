# Header Audit - Phase 1: Preparation for Internal Migration

> **Issue**: #610
> **Epic**: #577
> **Date**: 2026-02-02
> **Goal**: Reduce public headers from 153 to < 40 by moving implementation details to `src/internal/`

## Executive Summary

- **Current**: 153 public headers
- **Target**: < 40 public headers
- **Reduction**: ~113 headers (74%) to be moved to internal
- **Approach**: Aggressive facade-first API with internal implementation hiding

## Public API (Keep in `include/`) - 35 headers

### Category 1: Facades (4 headers) ✅ KEEP
Primary user-facing API for protocol access.

| Header | Purpose | Status |
|--------|---------|--------|
| `facade/tcp_facade.h` | TCP client/server factory | ✅ Public API |
| `facade/udp_facade.h` | UDP client/server factory | ✅ Public API |
| `facade/http_facade.h` | HTTP client/server factory | ✅ Public API |
| `facade/websocket_facade.h` | WebSocket client/server factory | ✅ Public API |

### Category 2: Interfaces (14 headers) ✅ KEEP
Abstract interfaces that define contracts.

| Header | Purpose | Status |
|--------|---------|--------|
| `interfaces/i_protocol_client.h` | Common protocol client interface | ✅ Public API |
| `interfaces/i_protocol_server.h` | Common protocol server interface | ✅ Public API |
| `interfaces/i_client.h` | Generic client interface | ✅ Public API |
| `interfaces/i_server.h` | Generic server interface | ✅ Public API |
| `interfaces/i_session.h` | Session interface | ✅ Public API |
| `interfaces/i_udp_client.h` | UDP-specific client interface | ✅ Public API |
| `interfaces/i_udp_server.h` | UDP-specific server interface | ✅ Public API |
| `interfaces/i_websocket_client.h` | WebSocket-specific client interface | ✅ Public API |
| `interfaces/i_websocket_server.h` | WebSocket-specific server interface | ✅ Public API |
| `interfaces/i_quic_client.h` | QUIC-specific client interface | ✅ Public API |
| `interfaces/i_quic_server.h` | QUIC-specific server interface | ✅ Public API |
| `interfaces/i_network_component.h` | Base network component interface | ✅ Public API |
| `interfaces/connection_observer.h` | Connection event observer | ✅ Public API |
| `interfaces/interfaces.h` | Interface header aggregator | ✅ Public API |

### Category 3: Integration (13 headers) ✅ KEEP
High-level integration and bridge components.

| Header | Purpose | Status |
|--------|---------|--------|
| `integration/network_system_bridge.h` | Unified integration facade | ✅ Public API |
| `integration/i_network_bridge.h` | Network bridge interface | ✅ Public API |
| `integration/messaging_bridge.h` | Messaging integration bridge | ✅ Public API |
| `integration/observability_bridge.h` | Logger/monitor integration | ✅ Public API |
| `integration/adapters/message_adapter.h` | Message format adapter | ✅ Public API |
| `integration/adapters/bytes_adapter.h` | Bytes format adapter | ✅ Public API |
| `integration/adapters/json_adapter.h` | JSON format adapter | ✅ Public API |
| `integration/adapters/compressed_adapter.h` | Compression adapter | ✅ Public API |
| `integration/legacy_messaging_adapter.h` | Legacy compatibility adapter | ✅ Public API |
| `integration/logger_integration.h` | Logger integration | ✅ Public API |
| `integration/monitor_integration.h` | Monitor integration | ✅ Public API |
| `integration/circuit_breaker_integration.h` | Circuit breaker integration | ✅ Public API |
| `integration/observer_integration.h` | Observer integration | ✅ Public API |

### Category 4: High-Level Utilities (4 headers) ✅ KEEP
Essential utilities for using the library.

| Header | Purpose | Status |
|--------|---------|--------|
| `network_system.h` | Main library header | ✅ Public API |
| `core/network_context.h` | ASIO context wrapper | ✅ Public API |
| `core/connection_pool.h` | Connection pooling | ✅ Public API |
| `core/session_handle.h` | Type-erased session handle | ✅ Public API |

**Total Public API: 35 headers** ✅

---

## Internal Implementation (Move to `src/internal/`) - 118 headers

### Category 5: Core Protocol Implementations (27 headers) 🔄 MOVE TO INTERNAL
Concrete implementations accessed via facades.

#### TCP Implementations (6 headers)
| Header | Destination | Facade Replacement |
|--------|-------------|-------------------|
| `core/messaging_client.h` | `src/internal/tcp/messaging_client.h` | `tcp_facade::create_client()` |
| `core/messaging_server.h` | `src/internal/tcp/messaging_server.h` | `tcp_facade::create_server()` |
| `core/secure_messaging_client.h` | `src/internal/tcp/secure_messaging_client.h` | `tcp_facade::create_client({.use_ssl=true})` |
| `core/secure_messaging_server.h` | `src/internal/tcp/secure_messaging_server.h` | `tcp_facade::create_server({.use_ssl=true})` |
| `core/unified_messaging_client.h` | `src/internal/tcp/unified_messaging_client.h` | Use facade |
| `core/unified_messaging_client.inl` | `src/internal/tcp/unified_messaging_client.inl` | Internal template |

#### UDP Implementations (7 headers)
| Header | Destination | Facade Replacement |
|--------|-------------|-------------------|
| `core/messaging_udp_client.h` | `src/internal/udp/messaging_udp_client.h` | `udp_facade::create_client()` |
| `core/messaging_udp_server.h` | `src/internal/udp/messaging_udp_server.h` | `udp_facade::create_server()` |
| `core/secure_messaging_udp_client.h` | `src/internal/udp/secure_messaging_udp_client.h` | `udp_facade::create_client({.use_ssl=true})` |
| `core/secure_messaging_udp_server.h` | `src/internal/udp/secure_messaging_udp_server.h` | `udp_facade::create_server({.use_ssl=true})` |
| `core/reliable_udp_client.h` | `src/internal/udp/reliable_udp_client.h` | Use facade with reliability option |
| `core/unified_udp_messaging_client.h` | `src/internal/udp/unified_udp_messaging_client.h` | Use facade |
| `core/unified_udp_messaging_client.inl` | `src/internal/udp/unified_udp_messaging_client.inl` | Internal template |

#### HTTP/WebSocket Implementations (8 headers)
| Header | Destination | Facade Replacement |
|--------|-------------|-------------------|
| `core/http_client.h` | `src/internal/http/http_client.h` | `http_facade::create_client()` |
| `core/http_server.h` | `src/internal/http/http_server.h` | `http_facade::create_server()` |
| `http/http_client.h` | `src/internal/http/http_client_impl.h` | Duplicate, consolidate |
| `http/http_server.h` | `src/internal/http/http_server_impl.h` | Duplicate, consolidate |
| `core/messaging_ws_client.h` | `src/internal/websocket/messaging_ws_client.h` | `websocket_facade::create_client()` |
| `core/messaging_ws_server.h` | `src/internal/websocket/messaging_ws_server.h` | `websocket_facade::create_server()` |
| `http/websocket_client.h` | `src/internal/websocket/websocket_client_impl.h` | Duplicate, consolidate |
| `http/websocket_server.h` | `src/internal/websocket/websocket_server_impl.h` | Duplicate, consolidate |

#### QUIC Implementations (3 headers)
| Header | Destination | Facade Replacement |
|--------|-------------|-------------------|
| `core/messaging_quic_client.h` | `src/internal/quic/messaging_quic_client.h` | Future QUIC facade |
| `core/messaging_quic_server.h` | `src/internal/quic/messaging_quic_server.h` | Future QUIC facade |
| `experimental/quic_client.h` | `src/internal/quic/experimental_quic_client.h` | Experimental API |

#### Session Management Implementations (3 headers)
| Header | Destination | Facade Replacement |
|--------|-------------|-------------------|
| `core/session_manager.h` | `src/internal/session/session_manager.h` | Internal, accessed via server |
| `session/messaging_session.h` | `src/internal/session/messaging_session.h` | Internal session impl |
| `session/secure_session.h` | `src/internal/session/secure_session.h` | Internal session impl |

### Category 6: Protocol Implementation Details (31 headers) 🔄 MOVE TO INTERNAL

#### QUIC Protocol (19 headers)
All QUIC protocol headers are internal implementation details:

| Header | Destination |
|--------|-------------|
| `protocols/quic/*.h` (19 files) | `src/internal/quic/protocol/` |

Examples:
- `protocols/quic/quic_connection.h` → `src/internal/quic/protocol/quic_connection.h`
- `protocols/quic/quic_stream.h` → `src/internal/quic/protocol/quic_stream.h`
- `protocols/quic/crypto.h` → `src/internal/quic/protocol/crypto.h`

#### HTTP2 Protocol (6 headers)
| Header | Destination |
|--------|-------------|
| `protocols/http2/*.h` (6 files) | `src/internal/http/http2/` |

#### gRPC Protocol (6 headers)
| Header | Destination |
|--------|-------------|
| `protocols/grpc/*.h` (6 files) | `src/internal/grpc/` |

### Category 7: Internal Utilities (7 headers) 🔄 MOVE TO INTERNAL

| Header | Destination | Reason |
|--------|-------------|--------|
| `utils/buffer_pool.h` | `src/internal/utils/buffer_pool.h` | Implementation detail |
| `utils/connection_tracker.h` | `src/internal/utils/connection_tracker.h` | Implementation detail |
| `utils/retry_policy.h` | `src/internal/utils/retry_policy.h` | Implementation detail |
| `utils/timeout_manager.h` | `src/internal/utils/timeout_manager.h` | Implementation detail |
| `core/callback_indices.h` | `src/internal/utils/callback_indices.h` | Internal helper |
| `core/session_concept.h` | `src/internal/session/session_concept.h` | C++20 concept, internal |
| `core/session_model.h` | `src/internal/session/session_model.h` | Type erasure impl |

### Category 8: Experimental Features (4 headers) 🔄 MOVE TO INTERNAL

| Header | Destination | Reason |
|--------|-------------|--------|
| `experimental/quic_server.h` | `src/internal/quic/experimental_quic_server.h` | Unstable API |
| `experimental/reliable_udp_client.h` | `src/internal/udp/experimental_reliable_udp_client.h` | Experimental |
| `experimental/experimental_api.h` | `src/internal/experimental/experimental_api.h` | Unstable |
| Remaining experimental headers | `src/internal/experimental/` | Unstable APIs |

### Category 9: Legacy/Compatibility (2 headers) 🔄 MOVE TO INTERNAL

| Header | Destination | Reason |
|--------|-------------|--------|
| `compat/legacy_aliases.h` | `src/internal/compat/legacy_aliases.h` | Compatibility only |
| `compat/unified_compat.h` | `src/internal/compat/unified_compat.h` | Compatibility only |

### Category 10: Remaining Headers (47 headers) 🔍 REVIEW NEEDED

Additional headers in:
- `unified/` directory (7 headers)
- `utils/` directory (4 remaining headers)
- `protocol/` directory (6 headers)
- `config/` directory (3 headers)
- `concepts/` directory (3 headers)
- `session/` directory (1 remaining header)
- `tracing/` directory (3 headers)
- `metrics/` directory (3 headers)
- `events/` directory (1 header)
- `di/` directory (1 header)
- `policy/` directory (1 header)
- `http/` directory (2 remaining headers)
- Others (12 headers)

**Action**: Detailed review needed for each header to determine public vs. internal classification.

---

## Migration Impact Analysis

### Breaking Changes

**High Impact** (Users must migrate):
- Direct instantiation of `messaging_client`, `messaging_server` → Use `tcp_facade`
- Direct instantiation of `messaging_udp_client`, `messaging_udp_server` → Use `udp_facade`
- Direct use of QUIC protocol headers → Use future QUIC facade or marked experimental
- Direct use of HTTP2/gRPC protocol headers → Internal details

**Medium Impact** (Advanced users):
- Session management direct access → Use server/client interfaces
- Internal utility usage → Use public alternatives or migrate to internal access

**Low Impact**:
- Experimental feature users → Already expect instability
- Legacy compatibility users → Migration guide provided

### Migration Path

1. **Immediate**: Update code to use facades (recommended, forward-compatible)
2. **Transition**: Use `NETWORK_SYSTEM_EXPOSE_INTERNALS=ON` CMake option (temporary)
3. **Advanced**: Access internal headers explicitly (not recommended, maintenance burden)

### Test Impact

- Unit tests will need `target_include_directories(PRIVATE src/internal)` for direct testing
- Integration tests should use public facade API only
- Examples must be updated to use facade API

---

## Next Steps

### Phase 1 Deliverables (This PR)
1. ✅ This header audit document
2. ⏳ Migration guide with facade usage examples
3. ⏳ Add deprecation warnings to headers scheduled for moving
4. ⏳ Update examples to use facade API

### Phase 2-4 (Subsequent PRs)
- Phase 2: Move protocol implementations (4 PRs: TCP, UDP, HTTP/WS, QUIC)
- Phase 3: Move protocol details (3 PRs: HTTP2, gRPC, utilities)
- Phase 4: Cleanup and final documentation

---

**Prepared by**: Claude Code
**Review Required**: @kcenon
**Next**: Create migration guide
