# Changelog

> **Note**: [`docs/CHANGELOG.md`](docs/CHANGELOG.md) is the canonical SSOT for the changelog.
> This root-level file is retained for tool compatibility (GitHub release notes, package managers).
> When updating, add entries to both files. <!-- TODO: consolidate into a single source -->

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Protocol support-status matrix committed as `docs/PROTOCOL_SUPPORT.md`, the deliverable of the protocol stub/placeholder audit. Classifies every protocol and protocol-adjacent surface as `production` / `experimental` / `test-only` / `remove` with per-row evidence and test references. Production protocols (TCP, UDP, TLS/SSL, WebSocket, HTTP/1.1, HTTP/2, HPACK, QUIC, gRPC) are confirmed covered by happy-path and non-happy-path tests; experimental surfaces (HPACK Huffman pass-through, experimental QUIC client `stats()`/`alpn_protocol()` accessors, DTLS server payload handling, QUIC `process_ack_frame` minimal handling, non-OTLP-HTTP tracing exporters) are documented and excluded from the v1.0 stability surface. Every `TODO`/`FIXME`/`HACK`/`stub`/`placeholder`/`not implemented` marker across `include/`, `src/`, and `libs/` is either eliminated or accounted for in the matrix, including the documented false positives (gRPC `stub_` member names, HTTP 501 reason phrases, the gRPC `unimplemented` status enum, the `null_connection_observer` doc comment, the `:param` route-syntax comment, and standalone-build fallback adapter types). README protocol feature matrix (English and Korean) updated with the audited status; experimental QUIC client header methods annotated as experimental ([#1144](https://github.com/kcenon/network_system/issues/1144), part of [#964](https://github.com/kcenon/network_system/issues/964))
- API freeze for v1.0 (candidate; v1.0.0 tag still pending) — public API surface audit committed as `docs/v1.0-api-surface.md`. The audit enumerates 31 public headers under `include/kcenon/network/` (excluding the `detail/` subtree), classifies every top-level symbol as `stable` / `rename-candidate` / `remove-candidate` / `experimental`, confirms zero `[[deprecated]]` symbols on the public surface, confirms zero experimental headers (already migrated to `src/internal/experimental/` per [#577](https://github.com/kcenon/network_system/issues/577)), and verifies the downstream Tier 5 consumer `pacs_system` builds against the frozen surface (uses `facade/tcp_facade.h`, `interfaces/i_session.h`, `session/session.h`). All 31 public headers are classified as `stable`; no removals or renames are required prior to freeze. After v1.0 tag, every header in the audit is governed by SemVer and may not be removed, renamed, or have its public symbols altered without a major version bump. The `detail/` subtree (34 headers) remains free to evolve in v1.x patches without SemVer impact. `CONTRIBUTING.md` updated with the v1.0 freeze policy ([#1125](https://github.com/kcenon/network_system/issues/1125), part of [#964](https://github.com/kcenon/network_system/issues/964))
- Known-stream dispatcher coverage tests for `src/protocols/http2/http2_client.cpp` targeting >=70% line / >=50% branch (Round 3 of [#953](https://github.com/kcenon/network_system/issues/953)). Adds `tests/unit/http2_client_dispatcher_branch_test.cpp` (38 TEST cases) and extends `tests/support/http2_client_test_access.h` with stream-seeding helpers so direct-dispatch tests can drive the known-stream branches Round 6 (#1115) and Round 2 (#1117) could not reach: `handle_rst_stream_frame` close-and-fulfill, `handle_goaway_frame` populated-streams loop, `handle_window_update_frame` known-stream non-zero arm, `handle_data_frame` non-streaming buffer / streaming callback / end_stream status-code paths plus per-stream and connection-level WINDOW_UPDATE branches, `handle_headers_frame` accumulate / end_stream / streaming-callback paths, `process_frame` dynamic_cast-fails fall-through for all seven dispatched types plus default arm for priority/push_promise/continuation, plus private-helper coverage of `build_headers` empty-path-to-"/" branch and `allocate_stream_id` RFC 7540 §5.1.1 odd-id contract ([#1119](https://github.com/kcenon/network_system/issues/1119))
- In-process loopback coverage tests for `src/http/websocket_server.cpp` targeting >=80% line / >=70% branch (up from 39.9% / 19.7% measured 2026-04-26) — adds `tests/unit/websocket_server_loopback_test.cpp` registered via `add_network_test`, driving `messaging_ws_server` end-to-end against a real RFC 6455 client built from `internal::websocket_socket` on a probed free port. Reaches branches uncovered by the hermetic #1053 branch tests: `do_start_impl` / `do_stop_impl` success paths, `do_accept` loop, `handle_new_connection` success / max_connections-zero / handshake-failure branches, `on_message` text+binary dispatch, `on_close`, `broadcast_text`/`broadcast_binary` populated-session branches, `get_connection` / `get_all_connections` success branches, `auto_pong` true and false branches, `~messaging_ws_server` while running, `bind_failed` catch arm, and `ws_connection` / `ws_connection_impl` methods reachable only after a real connection ([#1067](https://github.com/kcenon/network_system/issues/1067), part of [#953](https://github.com/kcenon/network_system/issues/953))
- Coverage expansion tests for `src/protocols/http2/http2_server.cpp` targeting >=80% line / >=70% branch coverage (up from 38.3% / 19.4%) — adds 23 tests in `tests/test_http2_server.cpp` organized into four groups using a real-loopback `Http2ServerLoopbackTest` fixture (no mocks): `HTTP2ServerPreface_*` (4 tests) for connection-preface validation, `HTTP2ServerErrorPath_*` (10 tests) for frame parse and protocol error paths, `HTTP2ServerStreamState_*` (5 tests) for stream-state-machine transitions, and `HTTP2ServerFlowControl_*` (4 tests) for `WINDOW_UPDATE` handling ([#1064](https://github.com/kcenon/network_system/issues/1064), part of [#953](https://github.com/kcenon/network_system/issues/953))
- Branch coverage tests for `src/http/websocket_server.cpp` exercising `ws_server_config` full-field round-trips with default-value verification, empty / 1024-character / binary-byte path values, port at 0 / 1 / 65535, `max_connections` at 0 / 1 / `std::numeric_limits<size_t>::max()` boundaries, `ping_interval` at 0 / 1 ms / 1 hour / max boundaries, `auto_pong` toggling, `max_message_size` at 0 / 1 / 1 MiB / max boundaries, full-config copy and assignment round-trips preserving every field; `messaging_ws_server` construction with empty / 512-character / binary-byte server IDs and 16-iteration repeated short-lived construction, `server_id()` reference stability across calls, `string_view` literal constructor; default-state queries on a never-started server: `is_running()==false`, `connection_count()==0`, empty `get_all_connections()`, `get_connection("")` / `get_connection("unknown")` / `get_connection(2048-char)` / binary-byte id returning `nullptr`, repeated default-state stability over 8 iterations; `stop_server()` and `i_websocket_server::stop()` returning `ok()` on the `prepare_stop()` early-return branch when never started, 5-iteration repeated rejection idempotency, `is_running()` invariance across stop calls; `broadcast_text()` and `broadcast_binary()` `session_mgr_ == nullptr` early-return for empty / small / 64 KiB payloads with binary-byte text strings and 4-iteration repeated invocation; interface (`i_websocket_server`) callback adapter coverage for all five callbacks (connection / disconnection / text / binary / error) with null callbacks (empty-function branch), populated lambdas (wrap-and-store branch), triple replacement across each adapter, combined registration test, `shared_ptr`-captured state lambdas, and null-then-populated-then-null toggle exercising the `if-callback / else-empty` branch in every adapter twice; concurrent polling under `shared_ptr` lifetime with 4 threads x 200 iterations of `is_running()`, 4 threads x 200 iterations of `connection_count()`, 4 threads x 200 iterations of `server_id()`, 4 threads x 100 iterations of `get_all_connections()`, plus a writer/reader pair on `set_error_callback()` / `is_running()`; multi-instance: 8 servers with independent default state, broadcast-on-one-does-not-affect-another, stop-one-affects-none; type alias coverage for `ws_server` and `secure_ws_server` and interop assignment between them; destructor cleanup tests on a never-started server, after `stop_server()`, with all five interface callbacks registered, and after broadcast ([#1053](https://github.com/kcenon/network_system/issues/1053))
- Branch coverage tests for `src/experimental/quic_server.cpp` exercising `quic_server_config` full-field round-trips with empty / 1024-character / binary-byte cert/key paths, optional `ca_cert_file` present and absent (including 2048-character path and `reset()`), `require_client_cert` toggling, ALPN protocol vector growth (empty, single, four-with-empty-string, 32-entry), all six numeric settings at zero and `std::numeric_limits` max boundaries, `enable_retry` toggling, `retry_key` vector growth (empty, 16 bytes, 256 bytes), full-config round-trip preserving every field; `messaging_quic_server` construction with empty / 512-character / binary-byte server IDs and 16-iteration repeated short-lived construction; `server_id()` reference stability across calls; `is_running()` / `session_count()` / `connection_count()` / `sessions()` / `get_session()` default invariants on a never-started server (including unknown / empty / 2048-character session ids); `stop_server()` rejection branch when never started, repeated rejection idempotency, and `i_quic_server::stop()` interface delegation; `disconnect_session()` `not_found` branch for unknown / empty / 2048-character / binary-byte ids with varied error codes (zero, 1, 42, max-uint64); `disconnect_all()` empty-map no-op for zero / 42 / max-uint64 error codes; `broadcast()` and `multicast()` empty-map `ok()`-return for empty / small / 64 KiB payloads, repeated invocation, empty `session_ids`, many unknown ids (64), and empty-string id; legacy callback registration with default-constructed (empty) `std::function` / triple-replaced lambdas / `shared_ptr`-captured state for all five callback types (connection / disconnection / receive / stream_receive / error); interface (`i_quic_server`) callback adapter coverage for all five callbacks (connection / disconnection / receive / stream / error) with null callbacks, populated lambdas, triple replacement, and combined registration; concurrent polling (4 threads x 100-200 iterations) of `is_running()` / `session_count()` / `connection_count()` / `server_id()` / `sessions()` and concurrent legacy callback replacement under shared_ptr lifetime; multi-instance independent default state across 8 servers and broadcast-on-one-does-not-affect-another; destructor on never-started / after failed-stop / with all five callbacks registered ([#1052](https://github.com/kcenon/network_system/issues/1052))
- Branch coverage tests for `src/internal/quic_socket.cpp` exercising many short-lived client/server construction loops, default invariants for `state` / `role` / `is_connected` / `is_handshake_complete` / `remote_endpoint` / `remote_connection_id`, `local_connection_id` RFC 9000 length bounds and pairwise uniqueness across 16 sockets, callback registration with default-constructed `std::function` / replaced lambdas / `shared_ptr`-captured state for all four callback types, mutable and const `socket()` accessor reachability, role-guard rejection in `connect()` / `accept()` (server cannot connect with or without SNI, client cannot accept with empty or missing PEM paths, repeated server-connect rejection idempotency), `close()` idempotency on idle/draining/closed including triple-close, application-error-code branch (`is_application_error=true`) with max-uint64 error code and 1024-character reason, `send_stream_data()` not-connected guard for empty / populated / 64 KiB payloads with and without FIN, `create_stream()` not-connected guard for client/server bidi and uni, `close_stream()` `not_found` early-return for nine distinct unknown stream ids, `stop_receive()` before start and repeated idempotency, `start_receive()`/`stop_receive()` flag transition, move construction preserving role and connection ID with populated callbacks, move assignment overwriting client target with server source, three-step move-assignment chain, self-move-assignment via reference (covers `this!=&other` guard), concurrent state-query polling and concurrent callback replacement under `shared_ptr` lifetime, multi-instance independent state across 4+4 client/server pairs, destructor cleanup after close / without close / after `start_receive()` ([#1051](https://github.com/kcenon/network_system/issues/1051))
- Branch coverage tests for `src/protocols/http2/http2_server.cpp` exercising `tls_config` field round-trips with empty / long / binary-byte paths and `verify_client` toggling, `http2_server` construction with empty / 512-char / binary-byte server IDs, `http2_settings` zero and `UINT32_MAX` round-trips with repeated `header_table_size` updates, `enable_push` toggling, pre-listen state queries (`is_running`, `active_connections`, `active_streams`, `server_id`, `stop` before any start), repeated start / stop cycles on ephemeral ports, already-running rejection for both `start()` and `start_tls()`, additional `start_tls()` rejection paths (empty cert/key, missing files, verify_client with missing ca), handler registration with default-constructed std::function / shared-state capture / multiple replacement, post-failed-TLS clear-start recovery, destructor-while-running, `wait()` completion after stop on a different thread, concurrent `is_running` / `active_connections` / `active_streams` / `server_id` polling under shared_ptr lifetime, concurrent `set_settings` writer with `get_settings` reader, multi-instance independent lifecycle ([#1050](https://github.com/kcenon/network_system/issues/1050))
- Branch coverage tests for `src/protocols/grpc/client.cpp` exercising `grpc_channel_config` full-field round-trip with zero/max boundary values, `call_options::set_timeout` across seconds/milliseconds/microseconds/nanoseconds duration types, metadata growth with empty keys / large values / many entries, varied target construction (long, IPv6 bracket, DNS, with-path), `wait_for_connected` timeout budget verification, repeated and concurrent `disconnect()` / `is_connected()` calls, move construction / move assignment / move chains with populated config, extended malformed-target connect error paths (multi-colon, leading/trailing colon, space in port, very long DNS label), `call_raw` guard-clause variants (empty method, no-leading-slash, large payload, future / pre-expired deadline, wait_for_ready, compression algorithm, post-disconnect), streaming guard clauses for server / client / bidi streams (empty method, missing slash, post-disconnect, expired deadline), concurrent async-callback stress, and `grpc_metadata` copy / move / clear / erase semantics ([#1049](https://github.com/kcenon/network_system/issues/1049))
- Branch coverage tests for `src/protocols/http2/http2_client.cpp` exercising `http2_response::get_header` case-insensitivity matrix, `get_body_string` byte-pattern preservation, `http2_settings` zero/max boundary round-trips, `http2_stream` move semantics with populated request/response buffers and streaming callbacks, `set_timeout` boundary values, `connect()` error paths to unreachable IPv4/IPv6 endpoints, disconnected-state early-return paths for every request helper, and concurrent `is_connected`/`set_timeout` queries ([#1048](https://github.com/kcenon/network_system/issues/1048))
- Unit tests for 4 unified adapter modules: `ws_connection`, `ws_listener`, `quic_connection`, `quic_listener` — part of the #953 coverage expansion effort ([#967](https://github.com/kcenon/network_system/issues/967))
- Unit tests for 11 untested modules across 5 categories — part of the #953 coverage expansion effort ([#968](https://github.com/kcenon/network_system/issues/968))
- Extra coverage tests for `src/tcp_socket.cpp` exercising `try_send` rejection, async_send-on-closed, `start_read` idempotence, `reset_metrics`, backpressure activation/release, multi-observer delivery, and default-state invariants ([#1032](https://github.com/kcenon/network_system/issues/1032))
- Extra coverage tests for `src/core/unified_session_manager.cpp` exercising default-config construction, backpressure short-circuit when disabled, zero-`max_sessions` divide-by-zero guard, pre-wrapped `session_handle` add path, `add_session_with_id` rejection / explicit / collision, `set_max_sessions` narrowing, `generate_id` concurrency, idle-cleanup accumulation, const `get_session`, and full `stats` population ([#1034](https://github.com/kcenon/network_system/issues/1034))
- Extra coverage tests for `src/protocols/quic/frame.cpp` exercising 4/8-byte varint type prefixes via `peek_type`, heterogeneous `parse_all` mixes, every concrete STREAM type byte (0x08-0x0f), ACK_ECN dispatch with truncation matrix, CRYPTO/NEW_TOKEN multi-byte length round-trips, `new_connection_id` minimal/large variants, CONNECTION_CLOSE transport-with-frame-type and multi-byte error_code, `build_padding(0)` edge, `frame_type_to_string` for previously-unasserted branches, and `is_stream_frame`/`get_stream_flags`/`make_stream_type` per-bit consistency ([#1033](https://github.com/kcenon/network_system/issues/1033))
  - Integration bridges (5): `container_integration`, `io_context_thread_manager`, `logger_integration`, `monitoring_integration`, `thread_integration`
  - Protocol factories (4): `tcp`, `udp`, `websocket`, `quic`
  - HTTP/2 (1): `http2_server_stream`
  - Internal (1): `websocket_socket`
- Extend `rate_limiter` to support composite session-based identification keys ([#872](https://github.com/kcenon/network_system/issues/872))
- Migration guide for transitioning from adapters to NetworkSystemBridge pattern
  - Comprehensive step-by-step migration instructions
  - API comparison tables for old vs new patterns
  - Common migration patterns and examples
  - Troubleshooting section
- Increase unit test coverage from 21% to 40% target ([#873](https://github.com/kcenon/network_system/issues/873))
  - Add `network_system_test` covering `network_manager` lifecycle, connection, and disconnection
  - Add `message_validator_extended_test` for extended message validation edge cases
  - Add `sliding_histogram_test` for `sliding_histogram` statistics and percentile calculations
  - Update Codecov configuration to track unit and integration coverage separately
- Expand HPACK (`src/protocols/http2/hpack.cpp`) unit coverage with `hpack_extra_coverage_test` — closes static-table boundary lookups, decoder happy paths for literal-without-indexing / never-indexed prefixes, multi-byte integer non-overflow path, huffman stub contract, and mixed static/dynamic round-trips ([#1031](https://github.com/kcenon/network_system/issues/1031))
- Pivot 7 dispatcher-only TEST_F in `tests/unit/http2_client_branch_test.cpp` to friend-injected `process_frame` (Round 6) — adds `tests/support/http2_client_test_access.h` as a single dedicated friend struct gated by the existing `NETWORK_ENABLE_TEST_INJECTION` macro (matching the `quic_server.h:55` / `websocket_server.h:33` pattern), bypassing the SETTINGS-handshake `wait_for` path that PR #1114 (Round 5) demonstrated to be structurally bounded by the ctest 300 s timeout under coverage instrumentation. Test #5868 measured 15.175 s and test #5869 hit the 300 s timeout in Coverage Analysis run [25562347620](https://github.com/kcenon/network_system/actions/runs/25562347620); after this refactor the same seven `Server*` TEST_F (`ServerPingFrameDrivesHandlePingAndKeepsConnectionAlive`, `ServerPingAckFrameIsAbsorbedSilently`, `ServerGoawayFrameFlipsConnectionStateToDisconnected`, `ServerWindowUpdateOnConnectionStreamExpandsWindow`, `ServerWindowUpdateOnUnknownStreamIsSilentlyIgnored`, `ServerRstStreamOnUnknownStreamIsSilentlyIgnored`, `ServerUnknownFrameTypeIsHandledWithoutCrashing`) complete in 0 ms wall-time under both Debug and Coverage builds. The 14 connect-state public-API TEST_F (`StartStream*`, `WriteStream*`, `CancelStream*`, `SecondConnect*`, `SendRequestTimesOut*`, `PostWithBody*`, `SetSettings*`) genuinely require the connected-state path and remain on the PR #1114 multiplier scaffold — out of scope for Round 6. Production header (`src/internal/protocols/http2/http2_client.h`) compiles byte-identical when `BUILD_TESTS=OFF` because the `NETWORK_ENABLE_TEST_INJECTION` macro is undefined in production builds. Post-merge `Coverage Analysis` workflow on develop will produce the definitive lcov measurement against the PR #1109 baseline (LH/LF=108/576 = 18.75% line, BRH/BRF=97/979 = 9.91% branch); acceptance gate is ≥+20pp line / ≥+10pp branch on `src/internal/protocols/http2/http2_client.cpp` ([#1115](https://github.com/kcenon/network_system/issues/1115), [#1116](https://github.com/kcenon/network_system/pull/1116), part of [#953](https://github.com/kcenon/network_system/issues/953))

### Changed

- Unify vcpkg manifest mode across all CI platforms (Linux, macOS, Windows) replacing per-platform manual ecosystem dependency builds ([#885](https://github.com/kcenon/network_system/issues/885))
- **Complete `Result<T>` migration for public API** — public headers now contain zero `throw` statements; every public function either returns `common::Result<T>` / `common::VoidResult` or is `noexcept`. Enforced by a new `public-api-check` CI job that rejects any PR reintroducing `throw` into `include/kcenon/network/`. ([#988](https://github.com/kcenon/network_system/issues/988))
- **Deprecated API audit for v1.0 freeze** — completed inventory of every `[[deprecated]]` attribute and `#pragma message("Deprecated:")` shim across `include/`, `src/`, and `cmake/`. Audit decision: freeze the deprecated surface as-is for v1.0; no symbols removed in this audit. Disposition recorded for 1 `[[deprecated]]` macro (retained, permanent), 14 `cmake/compat/` header shims (retained, removal target v1.1.0), and 6 CHANGELOG-announced deprecations missing source-level markers (retained through v1.x). See [`docs/migration/deprecated_api_audit_v1_0.md`](docs/migration/deprecated_api_audit_v1_0.md) for the full inventory and per-symbol removal targets ([#1127](https://github.com/kcenon/network_system/issues/1127), part of [#964](https://github.com/kcenon/network_system/issues/964))
- Parallelize connection pool initialization with `std::async` ([#870](https://github.com/kcenon/network_system/issues/870))

### Deprecated

- `thread_system_pool_adapter` class — replaced by `ThreadPoolBridge` from `network_system_bridge.h`; removal target v3.0.0. See [migration guide](docs/migration/adapter_to_bridge_migration.md)
- `common_thread_pool_adapter` class — replaced by `ThreadPoolBridge` from `network_system_bridge.h`; removal target v3.0.0. See [migration guide](docs/migration/adapter_to_bridge_migration.md)
- `common_logger_adapter` class — replaced by `ObservabilityBridge` from `network_system_bridge.h`; removal target v3.0.0. See [migration guide](docs/migration/adapter_to_bridge_migration.md)
- `common_monitoring_adapter` class — replaced by `ObservabilityBridge` from `network_system_bridge.h`; removal target v3.0.0. See [migration guide](docs/migration/adapter_to_bridge_migration.md)
- `bind_thread_system_pool_into_manager()` function — replaced by `NetworkSystemBridge::with_thread_system()`; removal target v3.0.0. See [migration guide](docs/migration/adapter_to_bridge_migration.md)

Deprecation timeline:
- **v2.1.0** (current): Deprecated adapters marked with `[[deprecated]]` attribute
- **v2.2.0** (Q2 2026): Migration strongly encouraged
- **v3.0.0** (Q3 2026): Deprecated adapters will be removed

Users are encouraged to migrate to the new `NetworkSystemBridge` facade as soon as possible. See the [migration guide](docs/migration/adapter_to_bridge_migration.md) for detailed instructions.

### Security

- Mark `no_tls` policy as deprecated with warning to use TLS-enabled policies in production ([#871](https://github.com/kcenon/network_system/issues/871))

---

## [v1.0.0] - Pending (candidate)

> **Status**: **v1.0 candidate — the v1.0.0 tag has NOT shipped.** The package
> version is still `0.1.1` (see `CMakeLists.txt` and `vcpkg.json`). The public
> API surface is *frozen as a candidate* (audited in
> [`docs/v1.0-api-surface.md`](docs/v1.0-api-surface.md)), but the SemVer
> guarantees below only take effect once the v1.0.0 tag is published. The tag is
> gated on the remaining v1.0 readiness work tracked in
> [#964](https://github.com/kcenon/network_system/issues/964) (test-coverage
> target in [#953](https://github.com/kcenon/network_system/issues/953) and the
> upstream Tier 0-3 v1.0 epics). Until then, treat `network_system` as pre-1.0.
>
> **At tag time**: entries currently in the `[Unreleased]` section will be moved
> here, and the package version will be bumped to `1.0.0`. After v1.0.0 ships,
> public headers under `include/kcenon/network/` are governed by Semantic
> Versioning and may not have symbols removed, renamed, or signature-changed
> without a major version bump. The `detail/` subtree remains free to evolve in
> v1.x patches without SemVer impact. See
> [`docs/v1.0-api-surface.md`](docs/v1.0-api-surface.md) and the v1.0 freeze
> policy in `CONTRIBUTING.md`.

---

> **Note:** Starting from v0.1.0 (2026-03-10), network_system adopted semantic versioning
> aligned with vcpkg packaging. Earlier versions (v0.2.0 – v2.0.0) reflect the pre-vcpkg
> release history and are retained for reference.

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
- **BREAKING**: `compatibility.h` and `network_module` namespace aliases — `network_module` namespace no longer available (#488)
- **BREAKING**: Deprecated WebSocket API methods (#487)
- **BREAKING**: Deprecated UDP API methods (#490)
- **BREAKING**: Deprecated `generate_session_id()` method (#489)
- **BREAKING**: Dropped OpenSSL 1.1.1 support; OpenSSL 3.x is now required (#491)

### Fixed
- `io_context` lifecycle management issues (#400, #410)
- Thread pool API compatibility for `thread_system` integration (#493, #495)
- PTO timeout loss detection handling for QUIC (#414, #418)

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

---

## Section Definitions

This changelog follows the [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/) standard sections:

- **Added** — new features
- **Changed** — changes in existing functionality
- **Deprecated** — soon-to-be removed features
- **Removed** — now-removed features
- **Fixed** — bug fixes
- **Security** — vulnerability fixes

For migration assistance, please refer to the migration guides in the `docs/migration/` directory.

[Unreleased]: https://github.com/kcenon/network_system/compare/v2.0.0...HEAD
[v1.0.0]: https://github.com/kcenon/network_system/releases/tag/v1.0.0
[v2.0.0]: https://github.com/kcenon/network_system/compare/v1.5.0...v2.0.0
[v1.5.0]: https://github.com/kcenon/network_system/compare/v1.4.0...v1.5.0
[v1.4.0]: https://github.com/kcenon/network_system/compare/v1.3.0...v1.4.0
[v1.3.0]: https://github.com/kcenon/network_system/compare/v1.2.0...v1.3.0
[v1.2.0]: https://github.com/kcenon/network_system/compare/v1.1.0...v1.2.0
[v1.1.0]: https://github.com/kcenon/network_system/compare/v1.0.0...v1.1.0
[v0.8.0]: https://github.com/kcenon/network_system/compare/v0.7.0...v0.8.0
[v0.7.0]: https://github.com/kcenon/network_system/compare/v0.6.0...v0.7.0
[v0.6.0]: https://github.com/kcenon/network_system/compare/v0.5.0...v0.6.0
[v0.5.0]: https://github.com/kcenon/network_system/compare/v0.4.0...v0.5.0
[v0.4.0]: https://github.com/kcenon/network_system/compare/v0.3.0...v0.4.0
[v0.3.0]: https://github.com/kcenon/network_system/compare/v0.2.0...v0.3.0
[v0.2.0]: https://github.com/kcenon/network_system/compare/v0.1.0...v0.2.0
[v0.1.0]: https://github.com/kcenon/network_system/compare/v0.0.1...v0.1.0
[v0.0.1]: https://github.com/kcenon/network_system/releases/tag/v0.0.1
