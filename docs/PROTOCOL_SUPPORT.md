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
| HTTP/2 | `network-http2` | `production` | `http2_client_branch_test`, `http2_server_branch_test`, `*_dispatcher_branch_test`, `http2_server_stream_test`, `http2_request_test` | Multiplexed streams, frame layer, server/client dispatch. HPACK Huffman coding is implemented per RFC 7541 Appendix B. |
| HPACK | `network-http2` | `production` | `hpack_branch_test`, `hpack_coverage_test`, `hpack_extra_coverage_test`, `test_http2_hpack_rfc7541` | RFC 7541 static/dynamic table, integer/string coding, and Appendix B Huffman coding. |
| QUIC | `network-quic` | `production` | 30+ unit tests: `quic_packet_branch_test`, `quic_connection_branch_test`, `quic_frame_branch_test`, `quic_loss_detector_test`, `quic_congestion_controller_test`, `quic_socket_branch_test`, `test_quic_e2e` | RFC 9000/9001/9002 core: packets, frames, streams, loss detection, congestion control, crypto, varint, transport params. The experimental client's ALPN and connection-statistics accessors are wired to live socket/crypto/recovery state (see accessor details below). |
| gRPC | `network-grpc` | `production` | `grpc_client_branch_test`, `grpc_client_extended_coverage_test`, `test_grpc_*`, `mock_grpc_server_peer` | Custom HTTP/2 transport + optional official `grpc++` wrapper (`NETWORK_ENABLE_GRPC_OFFICIAL`, OFF by default). |
| DTLS (secure UDP) | `network-udp` | `experimental` | `test_dtls_socket` | DTLS socket and server-side session payload handling are implemented and tested: `secure_messaging_udp_server::process_session_data` feeds inbound datagrams into the per-endpoint `dtls_session` via `dtls_socket::deliver_encrypted()`, advancing the handshake and dispatching decrypted application data to the receive callback (negative paths covered by `test_dtls_socket`). Remains `experimental` pending DTLS cookie exchange (DoS protection) and a consolidated single-socket demux model. |

## Sub-surface and code-level findings

Each row below is a marker hit from
`rg -n -i "TODO|FIXME|HACK|stub|placeholder|not implemented" include src libs`,
classified with evidence and disposition.

| Location | Marker / behavior | Status | Disposition |
|----------|-------------------|--------|-------------|
| `src/protocols/http2/hpack.cpp` (huffman::encode/decode/encoded_size) | RFC 7541 Appendix B Huffman coding implemented: bit-packed encode, trie-based decode with EOS/padding validation (RFC 7541 5.2), accurate `encoded_size`; the encoder selects Huffman only when it produces a smaller output | `production` | Resolved in kcenon/network_system#1157. Covered by `test_http2_hpack.cpp` Appendix C.4/C.6 conformance vectors and an all-byte round-trip test. |
| `src/experimental/quic_client.cpp` `alpn_protocol()` | Configured offers reach the active TLS object; returns the actual negotiated protocol through a synchronized socket snapshot (`std::nullopt` before negotiation or without ALPN) | `production` | Completed in kcenon/network_system#1158. Client-level local TLS negotiation tests select the second offer and exercise no-ALPN, stop, and reconnect. |
| `src/experimental/quic_client.cpp` `stats()` | Live transport counters plus `packets_lost`, `smoothed_rtt`, `min_rtt`, and `cwnd` from the socket-owned loss detector, RTT estimator, and congestion controller | `production` | Completed in kcenon/network_system#1158. Client-level tests cover wire traffic, ACKs, packet/time-threshold loss, duplicate ACKs, PTO, failed sends, and concurrent lifecycle reads. |
| `secure_messaging_udp_server.cpp` `process_session_data` (`libs/network-udp`, mirrored in `src/core`) | Feeds the inbound datagram into the per-endpoint `dtls_session` via `dtls_socket::deliver_encrypted()`; `[[maybe_unused]]`/TODO removed | `experimental` | Resolved in kcenon/network_system#1159. See DTLS row above; remaining experimental gap (cookie exchange) tracked there. |
| `src/internal/quic_socket.cpp` `process_ack_frame` | ACKs now reach the loss detector in their packet-number space and update RTT, loss, and congestion accounting; recovery timers retransmit lost frames and send PTO probes | `experimental` | Statistics integration completed in #1158. The socket still uses default peer ACK-delay parameters; full transport-parameter negotiation and general QUIC interoperability are outside this accessor completion. |
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

- Experimental QUIC socket: negotiate peer ACK-delay transport parameters instead of relying on defaults. The accessor work in #1158 does not establish general QUIC interoperability or promote the entire experimental client API.

No rows were classified `remove`; the `libs/network-*` wrapper `.cpp` files are
retained as documented standalone-build scaffolding rather than deleted.

## QUIC accessor semantics (#1158)

- Sent counters count successful UDP send completions, including retransmissions;
  failed/cancelled sends are excluded. Receive counters count nonempty UDP datagrams
  and their bytes, including packets subsequently rejected by QUIC validation.
- Loss counts packets declared lost by recovery, never PTO events alone. Duplicate
  ACKs and late ACKs of already-lost packets do not count a loss twice.
- RTT values use microseconds and remain zero until sampled. The congestion window
  uses bytes and starts at the controller's initial window once a socket exists.
- Accessors return defaults without a socket, including after stop. Reconnect creates
  a fresh socket. Recovery fields and negotiated ALPN are protected by a mutex;
  transport counters are atomic and are not a transactional snapshot with recovery.
- `tests/unit/experimental_quic_client_test.cpp` verifies the client accessors using
  real local UDP traffic and the existing TLS memory-BIO crypto path, plus explicit
  timestamps for ACK/loss tests. It does not substitute final statistics or negotiated
  ALPN through test hooks, and does not claim interoperability with external QUIC stacks.

## Verification

Markers are eliminated or accounted for:

```
rg -n -i "TODO|FIXME|HACK|stub|placeholder|not implemented" include src libs
```

Every remaining hit corresponds to a row in the tables above (`experimental`,
`test-only`, or a documented false positive). Production protocols each have at
least one non-happy-path test as listed in the protocol-level table.
