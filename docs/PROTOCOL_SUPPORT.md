# Protocol Support Matrix

This document is the authoritative support-status reference for every protocol
and protocol-adjacent surface in `network_system`. It is the deliverable of the
2026-05 protocol stub/placeholder audit (kcenon/network_system#1144) and feeds
the cross-system summary in kcenon/common_system#684.

## Classification legend

| Status | Meaning |
|--------|---------|
| `production` | Fully implemented, RFC-conformant where applicable, covered by happy-path **and** non-happy-path tests. Safe for downstream consumers. |
| `experimental` | API is present and exercised, but one or more behaviors are intentionally incomplete or subject to change. Documented as experimental; not part of the v1.0 stability surface. |
| `test-only` | Code that exists to support the build/test structure or library layout. Not a consumer-facing capability and excluded from release notes. |
| `remove` | Dead, redundant, or superseded code that should be deleted. Tracked by a follow-up issue. |

## Protocol-level support

| Protocol / Module | Library | Status | Non-happy-path tests | Notes |
|-------------------|---------|--------|----------------------|-------|
| TCP | `network-tcp` | `production` | `tcp_socket_test`, `tcp_facade_test`, `protocol_tcp_factory_test`, `secure_tcp_socket_test`, `test_tcp_*_adapter` | Async server/client, lifecycle, reconnection. TLS via OpenSSL. |
| UDP | `network-udp` | `production` | `udp_socket_test`, `udp_facade_test`, `protocol_udp_factory_test`, `messaging_udp_*_test`, `test_udp_*_adapter` | Connectionless datagram transport, broadcast/multicast. |
| TLS/SSL | `network-tcp` | `production` | `test_tls_config`, `secure_session_test`, `secure_messaging_*_test`, `mock_tls_socket` | TLS 1.2/1.3, certificate validation, modern cipher suites. |
| WebSocket | `network-websocket` | `production` | `websocket_socket_test`, `websocket_frame_test`, `websocket_handshake_test`, `websocket_protocol_test`, `websocket_server_branch_test` | RFC 6455 framing, fragmentation, ping/pong. |
| HTTP/1.1 | (core `src/http`) | `production` | `http_parser_branch_test`, `http_server_test`, `http_client_test`, `http_error_coverage_test`, `http_types_test` | Routing, cookies, multipart, chunked encoding, gzip/deflate. |
| HTTP/2 | `network-http2` | `production` | `http2_client_branch_test`, `http2_server_branch_test`, `*_dispatcher_branch_test`, `http2_server_stream_test`, `http2_request_test` | Multiplexed streams, frame layer, server/client dispatch. HPACK Huffman is an experimental sub-surface (see below). |
| HPACK | `network-http2` | `production` | `hpack_branch_test`, `hpack_coverage_test`, `hpack_extra_coverage_test`, `test_http2_hpack_rfc7541` | RFC 7541 static/dynamic table, integer/string coding. Huffman coding is an experimental pass-through (see HPACK Huffman row). |
| QUIC | `network-quic` | `production` | 30+ unit tests: `quic_packet_branch_test`, `quic_connection_branch_test`, `quic_frame_branch_test`, `quic_loss_detector_test`, `quic_congestion_controller_test`, `quic_socket_branch_test`, `test_quic_e2e` | RFC 9000/9001/9002 core: packets, frames, streams, loss detection, congestion control, crypto, varint, transport params. Connection-stats/ALPN-result accessors on the experimental client surface are incomplete (see experimental rows). |
| gRPC | `network-grpc` | `production` | `grpc_client_branch_test`, `grpc_client_extended_coverage_test`, `test_grpc_*`, `mock_grpc_server_peer` | Custom HTTP/2 transport + optional official `grpc++` wrapper (`NETWORK_ENABLE_GRPC_OFFICIAL`, OFF by default). |
| DTLS (secure UDP) | `network-udp` | `experimental` | `test_dtls_socket` | DTLS socket exists and is tested, but server-side session payload handling in `secure_messaging_udp_server::process_session_data` is not yet wired (data buffer unused, `// TODO: Use when DTLS handling is implemented`). |

## Sub-surface and code-level findings

Each row below is a marker hit from
`rg -n -i "TODO|FIXME|HACK|stub|placeholder|not implemented" include src libs`,
classified with evidence and disposition.

| Location | Marker / behavior | Status | Disposition |
|----------|-------------------|--------|-------------|
| `src/protocols/http2/hpack.cpp` (huffman::encode/decode/encoded_size, ~L306/609/651) | Huffman coding is a documented pass-through stub (returns input as-is); non-Huffman HPACK paths are fully RFC 7541 compliant | `experimental` | Keep; covered by `test_http2_hpack.cpp` Huffman stub tests asserting the pass-through contract and by `hpack_branch_test` H-bit branch. Real Huffman tables tracked by follow-up. |
| `src/experimental/quic_client.cpp` `alpn_protocol()` (~L459) | Returns `std::nullopt`; ALPN negotiation result not retrieved from handshake | `experimental` | Header annotated as experimental. Internal header (`src/internal/experimental/`), not a public surface. Tracked by follow-up. |
| `src/experimental/quic_client.cpp` `stats()` (~L470) | Returns default `quic_connection_stats{}`; not wired to live connection | `experimental` | Header annotated as experimental. Tracked by follow-up. |
| `src/core/secure_messaging_udp_server.cpp` `process_session_data` (~L301) | `data` parameter `[[maybe_unused]]` pending DTLS payload handling | `experimental` | DTLS row above. Tracked by follow-up. |
| `src/internal/quic_socket.cpp` `process_ack_frame` (~L719) | Tracks largest-acked only; full retransmission-queue pruning deferred (`(void)f; // Placeholder`) | `experimental` | Functional minimal ACK handling; full loss-recovery integration in `src/protocols/quic/loss_detector.cpp`. Tracked by follow-up. |
| `src/tracing/exporters.cpp` (otlp_grpc/jaeger/zipkin, ~L547/560/573) | These exporters log "not implemented" and fall back to console/OTLP-HTTP | `experimental` | Tracing observability surface, not a network protocol. `otlp_http` exporter is production. README tracing note already says "use otlp_http". |
| `src/http/http_server.cpp` `// TODO: Add error logging when needed` (~L665) | Compression-failure path returns silently | `production` | Benign: behavior (send uncompressed / abort) is correct; only optional logging is deferred. Marker removed in this audit. |
| `src/http/http_server.cpp` `// Look for parameter placeholder (:param_name)` (~L541) | Route-param parsing comment; not a stub | `production` | False positive (the word "placeholder" describes `:param` route syntax). No action. |
| `src/internal/http_error.cpp` / `src/internal/http_types.cpp` ("Not Implemented") | HTTP 501 reason-phrase string literal | `production` | False positive (HTTP status text). No action. |
| `src/protocols/quic/packet.cpp` (~L460 "placeholder - actual encoding done separately") | Comment describing packet-number encoding split across functions | `production` | False positive (descriptive comment). No action. |
| `include/kcenon/network/interfaces/connection_observer.h` (~L118 `null_connection_observer`) | Doc comment: no-op observer "useful as a placeholder" | `production` | False positive (no-op observer is a real, intended type). No action. |
| `src/internal/integration/thread_system_adapter.h` (~L90), `thread_pool_adapters.h` (~L386) | Placeholder types compiled only when thread_system/common_system is absent | `test-only` | Intentional compile-time fallbacks for standalone builds. No action. |
| `src/internal/utils/common_defs.h` | Marker hit in shared defs | `test-only` | Build-structure helper. No action. |
| `src/protocols/grpc/client.cpp` (8 hits) | All hits are the `stub_` member name (`::grpc::GenericStub`), not stubs | `production` | False positive (gRPC SDK type name). No action. |
| `libs/network-{http2,grpc}/src/*.cpp` ("implementation stub for ... library") | Library wrapper `.cpp` files are intentional re-export placeholders; real code lives in `src/protocols/...` and is not compiled here in the umbrella build | `test-only` | Library-layout scaffolding for standalone builds, documented in each file header. No action. |
| `include/kcenon/network/detail/protocols/grpc/status.h`, `libs/network-grpc/include/network_grpc/grpc_status.h` | `unimplemented = 12  //!< Operation not implemented` | `production` | False positive (gRPC canonical status-code enum per spec). No action. |

## False positives summary

The audit `rg` query matched 24 files. Of those, the following are **not**
stubs/placeholders and require no change: gRPC `stub_` member names, HTTP 501
"Not Implemented" reason phrases, the gRPC `unimplemented` status enum, the
`null_connection_observer` doc comment, the `:param` route-syntax comment, the
QUIC packet-number "done separately" comment, and the standalone-build fallback
adapter types. They are listed above so the markers are fully accounted for.

## Follow-up issues

Rows classified `experimental` with a genuine implementation gap are tracked by
dedicated follow-up issues (see kcenon/network_system#1144 PR body for numbers):

- HPACK Huffman coding: replace the pass-through stub with RFC 7541 Huffman tables.
- Experimental QUIC client: wire `alpn_protocol()` and `stats()` to the live connection.
- DTLS server: implement `secure_messaging_udp_server::process_session_data` payload handling.

No rows were classified `remove`; the `libs/network-*` wrapper `.cpp` files are
retained as documented standalone-build scaffolding rather than deleted.

## Verification

Markers are eliminated or accounted for:

```
rg -n -i "TODO|FIXME|HACK|stub|placeholder|not implemented" include src libs
```

Every remaining hit corresponds to a row in the tables above (`experimental`,
`test-only`, or a documented false positive). Production protocols each have at
least one non-happy-path test as listed in the protocol-level table.
