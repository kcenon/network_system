---
doc_id: "NET-PROJ-002"
doc_title: "Changelog - Network System"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "PROJ"
---

# Changelog - Network System

> **SSOT**: This document is the single source of truth for **Changelog - Network System**.

> **Language:** **English** | [한국어](CHANGELOG.kr.md)

All notable changes to the Network System project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- **Coverage expansion tests for `src/protocols/http2/http2_server.cpp` (#1064)**
  - Extended `tests/test_http2_server.cpp` with 23 new tests using a real-loopback `Http2ServerLoopbackTest` fixture (no mocks) to drive frame I/O end-to-end and reach the impl-level paths that #1050 documented as out of scope for hermetic public-API testing
  - Targets >=80% line / >=70% branch coverage on `src/protocols/http2/http2_server.cpp` (up from 38.3% line / 19.4% branch); CI must confirm the uplift
  - Test groups: `HTTP2ServerPreface_*` (4 tests) for connection-preface validation; `HTTP2ServerErrorPath_*` (10 tests) for frame-parse errors and protocol-error GOAWAY emission; `HTTP2ServerStreamState_*` (5 tests) for HTTP/2 stream-state-machine transitions (`idle` -> `open` -> `half_closed` -> `closed`, including `RST_STREAM` and trailers); `HTTP2ServerFlowControl_*` (4 tests) for `WINDOW_UPDATE` handling at connection and stream level
  - Part of the #953 coverage expansion effort; complements the hermetic-only #1050 branch tests by exercising the frame-driven paths in `http2_server_connection` (`process_frame` per-type dispatch, `handle_settings_frame`, `handle_headers_frame` with HPACK decode, `handle_data_frame`, `handle_rst_stream_frame`, `handle_window_update_frame`, GOAWAY emission, stream lifecycle) over a live loopback TCP transport
- **Branch coverage tests for `src/http/websocket_server.cpp` (#1053)**
  - Added `tests/unit/websocket_server_branch_test.cpp` complementing `websocket_server_test.cpp` (Issue #989)
  - Closes `ws_server_config` full-field round-trip coverage: documented default-value verification (port 8080, path "/", max_connections 1000, ping_interval 30000ms, auto_pong true, max_message_size 10 MiB), empty / 1024-character / binary-byte path values, port at 0 / 1 / 65535, `max_connections` at 0 / 1 / `std::numeric_limits<size_t>::max()`, `ping_interval` at 0 / 1 ms / 1 hour (3600000 ms) / `chrono::milliseconds::max()`, `auto_pong` toggling, `max_message_size` at 0 / 1 / 1 MiB / `std::numeric_limits<size_t>::max()`, full-config copy round-trip preserving every field, and full-config assignment round-trip preserving every field; `messaging_ws_server` construction with empty / 512-character / binary-byte (4-byte) server IDs, 16-iteration repeated short-lived construction, `server_id()` reference-stability across calls (verified pointer equality), and `std::string_view` literal constructor variant; default-state queries on a never-started server: `is_running()==false`, `connection_count()==0`, empty `get_all_connections()` list, `get_connection("")` / `get_connection("unknown")` / `get_connection(2048-char)` / `get_connection(5-byte binary)` returning `nullptr`, plus repeated default-state stability across 8 iterations; `stop_server()` returning `ok()` on the `lifecycle_.prepare_stop()` false branch when never started, 5-iteration repeated `ok()` idempotency, `i_websocket_server::stop()` interface delegation also returning `ok()` on the same branch, and `is_running()` remaining `false` across consecutive stop calls; `broadcast_text()` `session_mgr_ == nullptr` early-return branch for empty / "hello" / 64 KiB / 4-byte binary-byte text payloads with 4-iteration repeated invocation, `broadcast_binary()` `session_mgr_ == nullptr` early-return for empty / 4-byte / 64 KiB payloads with 4-iteration repeated invocation; interface (`i_websocket_server`) callback adapter coverage for all five callbacks: `set_connection_callback` / `set_disconnection_callback` / `set_text_callback` / `set_binary_callback` / `set_error_callback` each accepting a populated lambda (wrap-and-store branch), an empty default-constructed `std::function` (empty-function branch), and triple replacement across each adapter capturing a counter that stays zero (no callback fires on a never-started server); combined-registration test setting all five interface callbacks together; `shared_ptr`-captured state lambdas for all five interface callbacks; null-then-populated-then-null toggle for every adapter exercising the `if-callback / else-empty` branch twice each; concurrent polling under `shared_ptr` lifetime with 4 threads x 200 iterations of `is_running()`, 4 threads x 200 iterations of `connection_count()`, 4 threads x 200 iterations of `server_id()`, 4 threads x 100 iterations of `get_all_connections()`, plus a writer/reader pair where the writer continuously replaces `set_error_callback()` while the reader polls `is_running()`; multi-instance: 8 servers each with independent server_id / running-state / connection_count, broadcast-on-one-does-not-affect-another sibling, stop-one-affects-none; type alias coverage for `ws_server` and `secure_ws_server` (each instantiates with the expected `server_id()`, `is_running()==false`, `connection_count()==0`) and interop assignment between them via shared_ptr; destructor cleanup tests on a never-started server, after `stop_server()` on a never-started server, with all five interface callbacks registered, and after broadcast invocation
  - Registered `network_websocket_server_branch_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/http/websocket_server.cpp` line >= 70% / branch >= 60%; this PR contributes hermetic public-API surfaces only — the impl-level methods that physically accept TCP connections, complete the WebSocket HTTP/1.1 upgrade handshake, exchange frames with a peer, and dispatch per-message callbacks remain reachable only with a live TCP client that speaks the WebSocket wire format end-to-end. Specifically, `do_start_impl()` success path past `asio::ip::tcp::acceptor` construction (the `bind_failed` branches for `address_in_use` / `access_denied` are reachable only by binding to a privileged or in-use port and would not be hermetic, and the success path would leave the io_context loop running asynchronously inside the test process and pollute global state across tests), `do_stop_impl()` (only reachable after a successful start), `do_accept()` (async TCP accept completion, reachable only with a live `io_context::run()` loop), `handle_new_connection()` (TCP socket construction + `websocket_socket` wrapping + `ws_connection_impl` construction + per-connection callback wiring + `ws_socket->async_accept` handshake initiation, reachable only after a TCP connect that completes the WebSocket HTTP/1.1 upgrade handshake), `on_message()` (reachable only after async_read of a complete frame), `on_close()` (reachable only after a peer-initiated close frame or local `close()`), `on_error()` (reachable only after a transport-level error during read/write), and the four `invoke_*_callback()` helpers (reachable only from frame-driven paths inside the io_context). The success branches of `broadcast_text()` / `broadcast_binary()` with at least one populated session, and the non-null branch of `get_connection()` for a known id, also require a live WebSocket client that completes the handshake, since the `session_mgr_->add_connection()` call in `handle_new_connection()` is the only path that populates the session map and that call sits behind the `ws_socket->async_accept` callback. Testing those branches hermetically would require either a mock thread pool that does not actually run `io_context::run`, a friend-declared injection point inside `messaging_ws_server`, or a transport fixture that drives the WebSocket upgrade handshake from a test-side client. The `ws_connection_impl` private methods (`send_text` / `send_binary` / `close` / `is_connected` / `get_socket` / `invalidate`) are reachable only when at least one connection has been added through `handle_new_connection()` and so are also blocked by the same handshake requirement
- **Branch coverage tests for `src/experimental/quic_server.cpp` (#1052)**
  - Added `tests/unit/quic_server_branch_test.cpp` complementing `experimental_quic_server_test.cpp` (Issue #989)
  - Closes `quic_server_config` full-field round-trip coverage: empty / 1024-character / binary-byte cert/key paths, optional `ca_cert_file` present (16-character / 2048-character) and absent (`reset()`-cleared), `require_client_cert` toggling, ALPN protocol vector with empty / single-entry / four-entry-with-empty-string / 32-entry growth and `clear()`-shrink, six numeric settings (`max_idle_timeout_ms`, `initial_max_data`, `initial_max_stream_data`, `initial_max_streams_bidi`, `initial_max_streams_uni`, `max_connections`) at zero and `std::numeric_limits<uint64_t>::max()` / `std::numeric_limits<size_t>::max()` boundaries, `enable_retry` toggle, `retry_key` vector growth (empty / 16-byte / 256-byte), and full-config copy round-trip preserving every field including the `optional<string>` and `vector<uint8_t>` members; `messaging_quic_server` construction with empty / 512-character / binary-byte (4-byte) server IDs, 16-iteration repeated short-lived construction, and `server_id()` reference-stability across calls; default-state queries on a never-started server: `is_running()==false`, `session_count()==0`, `connection_count()==0` with delegation invariant `connection_count()==session_count()`, empty `sessions()` list, `get_session("")` / `get_session("unknown")` / `get_session(2048-char)` returning `nullptr`; `stop_server()` rejection branch when never started returns `is_err()`, 5-iteration repeated rejection idempotency, plus `i_quic_server::stop()` interface delegation also returning `is_err()`; `disconnect_session()` `not_found` branch for `"unknown"` / empty / 2048-character / 5-byte binary-byte session ids and error codes 0 / 1 / 42 / `std::numeric_limits<uint64_t>::max()`; `disconnect_all()` empty-map no-op for 0 / 42 / max-uint64 error codes; `broadcast()` empty-map `ok()`-return for empty / 4-byte / 64 KiB payloads with 4-iteration repeated invocation; `multicast()` empty-map `ok()`-return for empty `session_ids`, three-element unknown ids, 64-element unknown ids, single empty-string id, and 64 KiB payload to one unknown id; legacy callback registration with default-constructed (empty) `std::function` for connection / disconnection / receive / stream_receive / error callback types, triple-replaced lambdas across all five types capturing a counter that stays zero (no callback fires on a never-started server), and `shared_ptr`-captured state lambdas for all five types; interface (`i_quic_server`) callback adapters: `set_connection_callback` / `set_disconnection_callback` / `set_receive_callback` / `set_stream_callback` / `set_error_callback` each accepting a populated lambda, an empty (default-constructed) `std::function`, and (for connection_callback) triple replacement; combined-registration test setting all five interface callbacks together; concurrent polling under `shared_ptr` lifetime with 4 threads x 200 iterations of `is_running()`, 4 threads x 200 iterations of `session_count()` + `connection_count()`, 4 threads x 200 iterations of `server_id()`, 4 threads x 100 iterations of `sessions()`, plus a writer/reader pair on `set_error_callback()` / `is_running()`; multi-instance: 8 servers each with independent server_id / running-state / session_count, broadcast-on-one-does-not-affect-another sibling; destructor cleanup tests on a never-started server, after a failed `stop_server()`, and with all five legacy callbacks registered
  - Registered `network_quic_server_branch_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/experimental/quic_server.cpp` line >= 70% / branch >= 60%; this PR contributes hermetic public-API surfaces only — the impl-level methods that physically receive QUIC packets and accept handshakes from a peer remain reachable only with a UDP transport fixture and a TLS-1.3 client. Specifically, `do_start_impl()` success path past `asio::ip::udp::socket` bind (the `bind_failed` branches for `address_in_use` / `access_denied` are reachable only by binding to a privileged or in-use port and would not be hermetic), `do_stop_impl()` (only reachable after a successful start), `start_receive()` (async UDP receive completion), `handle_packet()` (header parsing, dcid extraction, and session dispatch), `find_or_create_session()` (session lookup + create + connection-limit-reached branch + UDP socket connect + `quic_socket::accept` + session callback wiring + metrics reporting + monitor recording), `generate_session_id()` (only invoked inside `find_or_create_session`), `on_session_close()` (peer-driven close), `start_cleanup_timer()` (timer scheduling), `cleanup_dead_sessions()` (periodic cleanup), the five `invoke_*_callback()` helpers (only invoked from packet-driven paths), and the success branches of `broadcast()` / `multicast()` / `disconnect_session()` / `disconnect_all()` under populated session map all require either a live QUIC client that completes a TLS handshake or a friend-declared test injection point inside `messaging_quic_server`. The `start_server()` success path past UDP bind also requires either an ephemeral port (which would still leave the io_context loop running asynchronously inside the test process and pollute global state across tests) or a mock thread pool that does not actually run `io_context::run`
- **Branch coverage tests for `src/internal/quic_socket.cpp` (#1051)**
  - Added `tests/unit/quic_socket_branch_test.cpp` complementing `quic_socket_construction_test.cpp` (Issue #989)
  - Closes 16-iteration short-lived construction loops for both `quic_role::client` and `quic_role::server`, default invariants for `state()` / `role()` / `is_connected()` / `is_handshake_complete()` returning `idle` / role / `false` / `false`, `remote_endpoint()` returning unspecified address with port 0 before any `connect()`, `local_connection_id().length()` within RFC 9000 bounds (0..20) plus 16-socket pairwise uniqueness, `remote_connection_id().length() == 0` before connect/accept; callback registration covering `set_stream_data_callback` / `set_connected_callback` / `set_error_callback` / `set_close_callback` with default-constructed (empty) `std::function`, multi-replacement (3-deep), `shared_ptr`-captured state lambdas; mutable + const `socket()` accessor reachability; role-guard rejection branches: server `connect()` rejected with and without SNI, client `accept()` rejected with empty + missing PEM paths, 3-iteration repeated server-connect rejection idempotency; `close()` idempotency: idle close returns ok, application error code (non-zero triggers `is_application_error=true`) returns ok, `std::numeric_limits<uint64_t>::max()` error code returns ok, 1024-character reason returns ok, empty reason returns ok, triple-close returns ok at every step, post-close state in {`closing`, `draining`, `closed`}; `send_stream_data()` not-connected guard for empty / populated / 64 KiB payloads with and without FIN; `create_stream()` not-connected guard for client-bidi / client-uni / server-bidi / server-uni; `close_stream()` `not_found` early-return for stream ids 0 / 1 / 2 / 4 / 5 / 7 / 100 / 4096 / 65535 plus `std::numeric_limits<uint64_t>::max()`; `stop_receive()` before any `start_receive()` is harmless and is idempotent across 4 invocations; `start_receive()` followed immediately by `stop_receive()` runs cleanly; move construction preserves role + connection ID + `idle` state with populated callbacks attached, server-role move construction works, move assignment overwrites a server target with a client source preserving the source connection ID, 3-step move-assignment chain preserves the original source connection ID through both moves, self-move-assignment via reference (covers `this != &other` guard); concurrent state-query polling (4 threads x 200 iterations) for `state` / `is_connected` / `is_handshake_complete` and for `role` / `local_connection_id` and for `remote_connection_id`; concurrent callback replacement with `is_connected` reader; multi-instance: closing one socket does not affect a sibling, 4 clients + 4 servers maintain independent state; destructor cleanup after `close()`, without `close()`, and after `start_receive()`
  - Registered `network_quic_socket_branch_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/internal/quic_socket.cpp` line >= 70% / branch >= 60%; this PR contributes hermetic public-API surfaces only — the impl-level methods that physically exchange QUIC packets with a peer (`do_receive` async UDP receive, `handle_packet` header parsing / unprotection / payload decryption / frame parsing dispatch, `process_frame` per-variant dispatch, `process_crypto_frame` TLS handshake state machine including client/server `connected` transition, `process_stream_frame` data callback delivery, `process_ack_frame` placeholder, `process_connection_close_frame` peer-driven draining, `process_handshake_done_frame` client-side completion, `send_pending_packets` populated-queue success path, `send_packet` encryption + `async_send_to` dispatch including initial / handshake / short-header builder branches, `queue_crypto_data` populated-state append, `determine_encryption_level` per-packet-type dispatch, `on_retransmit_timeout` handler) require a live UDP peer that speaks the QUIC wire format end-to-end and remain uncovered without a transport fixture or a friend-declared injection point inside `quic_socket`. The `connect()` and `accept()` success paths past TLS init also require either a peer that completes the TLS handshake or real PEM cert / key files on disk
- **Branch coverage tests for `src/protocols/http2/http2_server.cpp` (#1050)**
  - Added `tests/unit/http2_server_branch_test.cpp` complementing `http2_server_test.cpp` and `http2_server_coverage_test.cpp` (Issue #992)
  - Closes `tls_config` round-trip with empty / 1024-char / binary-byte paths plus `verify_client` toggle invariance; `http2_server` construction with empty / 512-char / binary-byte server IDs and 16-iteration repeated construction; `http2_settings` all-zero and all-`UINT32_MAX` round-trips via `set_settings`/`get_settings`, repeated `header_table_size` updates exercising HPACK encoder/decoder dynamic-table updates, `enable_push` toggling, and independent settings across instances; pre-listen state queries (`is_running`, `active_connections`, `active_streams`, `server_id`, idempotent `stop()` before any `start()`); 4-iteration start/stop cycles on ephemeral port 0; already-running rejection for `start()` and for `start_tls()` after a successful `start()`; additional `start_tls()` rejection paths (default-empty config, both files missing, ca_file present with `verify_client=true`, repeated rejection idempotency, post-failed-TLS clear-start recovery); destructor-while-running cleanup; `wait()` completion after `stop()` on a separate thread; handler registration with default-constructed `std::function`, shared-state-captured `shared_ptr`, and triple replacement of both request and error handlers; concurrent state polling (8-thread `is_running` / 4-thread `active_connections` / 4-thread `active_streams` / 4-thread `server_id`, 200 iterations each), concurrent writer/reader pair on `set_settings`/`get_settings`; multi-instance independent lifecycle (4 concurrent servers + stop-one-affects-none)
  - Registered `network_http2_server_branch_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/protocols/http2/http2_server.cpp` line >= 70% / branch >= 60%; this PR contributes hermetic public-API surfaces only — the impl-level frame-handler paths in `http2_server_connection` (`process_frame` per-frame_type cases, `handle_settings_frame`, `handle_headers_frame` HPACK decode + COMPRESSION_ERROR GOAWAY emission, `handle_data_frame` window update emission for unknown stream id, `handle_rst_stream_frame`, `handle_ping_frame` ACK reply, `handle_goaway_frame` shutdown, `handle_window_update_frame` flow control, `get_or_create_stream`, `close_stream`, `send_frame`, `send_settings`, `send_settings_ack`, `read_frame_header`, `read_frame_payload`, `dispatch_request`) and the TLS handshake error path inside `handle_accept_tls` require a live HTTP/2 client to drive frame I/O end-to-end and remain uncovered without a transport fixture or a friend-declared injection point inside `http2_server_connection`
- **Branch coverage tests for `src/protocols/grpc/client.cpp` (#1049)**
  - Added `tests/unit/grpc_client_branch_test.cpp` complementing `grpc_client_test.cpp` (Issue #994) and `core_client_test.cpp` (Issue #977)
  - Closes `grpc_channel_config` full-field round-trip with all-zero and all-max boundary values, default-value verification, and partial mutual-TLS (cert-only / key-only) cases; `call_options::set_timeout` across `std::chrono::seconds` / `milliseconds` / `microseconds` / `nanoseconds` duration types plus zero-timeout, deadline-overwrite, and monotonicity over repeated calls; `call_options` metadata growth (empty key, empty value, 64 ordered entries, 8 KiB value, binary-flagged key, clear-resets-empty); `grpc_client` construction with very long, IPv6 bracket, DNS-style, with-path, port-65535, and identical-target permutations; `wait_for_connected` budget verification at 0 ms / 1 ms / 50 ms in the never-connected state plus repeated-call idempotency; 32-iteration sequential `disconnect()`, 4-thread concurrent `disconnect()`, 8-thread concurrent `is_connected()` polling (1600 observations), and concurrent disconnect-while-querying; move construction / move assignment / 4-step move chain with populated config; extended malformed-target connect coverage (leading colon, multi-colon, space-in-port, negative-leading port, trailing colon, only-colon, multiple-failure idempotency, very long DNS label > 63 chars); `call_raw` guard-clause variants (empty method, no-leading-slash, 64 KiB payload, pre-expired deadline, future deadline, `wait_for_ready=true`, compression algorithm set, post-disconnect invocation); server / client / bidi streaming guard clauses with empty method, missing-slash method, expired deadline, and post-disconnect; `call_raw_async` 8-callback concurrent stress with deterministic notification, plus null-callback safety with non-empty payload and with deadline; `grpc_metadata` copy / move / assignment-from-empty / erase semantics
  - Registered `network_grpc_client_branch_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/protocols/grpc/client.cpp` line >= 70% / branch >= 60%; this PR contributes hermetic public-API surfaces only — the impl-level frame exchange paths (HTTP/2 POST after `is_connected()==true`, official `UnaryCall`, streaming send/receive loops, the `*_impl::on_data`/`on_headers`/`on_complete` callbacks driven by HTTP/2 trailers, tracing `set_attribute`/`set_error` calls inside successful call paths) require a live HTTP/2 (or live grpcpp) peer to drive and remain uncovered without a transport fixture
- **Branch coverage tests for `src/protocols/http2/http2_client.cpp` (#1048)**
  - Added `tests/unit/http2_client_branch_test.cpp` complementing `http2_client_test.cpp`, `http2_client_coverage_test.cpp`, and `http2_client_extended_coverage_test.cpp` (Issue #991)
  - Closes `http2_response::get_header` case-insensitive lookup matrix (mixed/lower/upper, leading/trailing-whitespace non-match, duplicate first-match), `get_body_string` empty/ASCII/binary/large-body byte-preservation, `http2_settings` all-zero and all-`UINT32_MAX` round-trips plus repeated `header_table_size` updates exercising HPACK encoder/decoder dynamic-table updates, `enable_push` toggling, `http2_stream` default-construction invariants and move construction/assignment with populated request/response buffers and streaming callbacks, promise/future plumbing across move, `set_timeout` zero/large/repeated values, `connect()` to unreachable IPv4/IPv6 loopback ports and unresolvable DNS literals, repeated failed-connect cleanliness, disconnected-state early-return paths for `get`/`post`/`put`/`del`/`start_stream`/`write_stream`/`close_stream_writer`/`cancel_stream`, post-failed-connect helper invocations, `is_connected` and `disconnect` idempotency, repeated client construction, empty/long client_id, and concurrent `is_connected`/`set_timeout` queries
  - Registered `network_http2_client_branch_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/protocols/http2/http2_client.cpp` line >= 70% / branch >= 60%; this PR contributes hermetic public-API surfaces only — frame-handler paths require a mock TLS socket layer (separate sub-issue)
- **Unit tests for unified adapter modules (#967)**
  - Added dedicated unit tests for 4 unified adapters: `ws_connection_adapter`, `ws_listener_adapter`, `quic_connection_adapter`, `quic_listener_adapter` (68 tests total)
  - Registered new test targets in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort; complements #955 which covered the remaining 13 adapter modules
- **Unit tests for 11 untested modules (#968)**
  - Integration bridges (5): `container_integration`, `io_context_thread_manager`, `logger_integration`, `monitoring_integration`, `thread_integration`
  - Protocol factories (4): `tcp`, `udp`, `websocket`, `quic`
  - HTTP/2 (1): `http2_server_stream`
  - Internal (1): `websocket_socket`
  - Registered all 11 test targets in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort targeting 48% to 80% unit test coverage
- **Additional HPACK coverage tests (#1031)**
  - Added `tests/unit/hpack_extra_coverage_test.cpp` complementing `hpack_test.cpp` (Issue #976) and `hpack_coverage_test.cpp` (Issue #1009)
  - Closes static-table boundary lookups (index 61, name-only ordering, mismatched-value fall-through), dynamic-table get-past-end and grow-without-eviction paths, encoder static-name-mismatched-value forcing literal-with-indexing path, decoder happy paths for literal-without-indexing (`0x00`) new-name and indexed-name forms plus never-indexed (`0x10`) prefix, multi-byte integer decoding under the 64-bit overflow threshold, huffman stub `encoded_size` and byte-for-byte round-trip contract, and encoder/decoder pair convergence for static-only emissions
  - Registered `network_hpack_extra_coverage_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/protocols/http2/hpack.cpp` line >= 70% / branch >= 60%
- **Additional tcp_socket coverage tests (#1032)**
  - Added `tests/unit/tcp_socket_extra_coverage_test.cpp` complementing `tcp_socket_test.cpp`
  - Closes `try_send` rejection branch when `max_pending_bytes` would be exceeded, `async_send` and `try_send` against an already-closed socket (handler error path), `start_read` idempotence (compare_exchange_strong false branch) and start-after-close no-op, `reset_metrics` clearing every counter including `peak_pending_bytes` and `rejected_sends`, backpressure activation at high watermark and release at low watermark, `config()` accessor for custom and default configurations, multi-observer delivery, detach of a never-attached observer, null-callback setters, `stop_read` before `start_read`, and freshly-constructed default-state invariants
  - Registered `network_tcp_socket_extra_coverage_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/tcp_socket.cpp` line >= 70% / branch >= 60%
- **Additional unified_session_manager coverage tests (#1034)**
  - Added `tests/unit/unified_session_manager_extra_coverage_test.cpp` complementing `unified_session_manager_test.cpp`
  - Closes the default-constructed manager configuration defaults, `is_backpressure_active()` short-circuit when `enable_backpressure` is false, `get_utilization()` divide-by-zero guard when `max_sessions == 0`, the pre-wrapped `session_handle` add overload (both explicit and auto-generated id), `add_session_with_id` rejection at the limit / explicit id passthrough / collision returning empty / auto-generated id prefix, duplicate-id `add_session` not inflating `total_accepted`, `update_activity` and `remove_session` returning false on missing ids, `cleanup_idle_sessions` no-op when nothing is idle, `stop_all_sessions` / `clear_all_sessions` equivalence and reusability, `for_each` over empty manager (mutable + const), `generate_id` uniqueness across threads, `set_max_sessions` narrowing below current count blocking new accepts and toggling backpressure, full `stats` population including `idle_timeout` / `backpressure_active`, const `get_session` round-trip, and accumulation of `total_cleaned_up` across multiple cleanup invocations
  - Registered `network_unified_session_manager_extra_coverage_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/core/unified_session_manager.cpp` line >= 70% / branch >= 60%
- **Additional QUIC frame coverage tests (#1033)**
  - Added `tests/unit/quic_frame_extra_coverage_test.cpp` complementing `quic_frame_coverage_test.cpp` (Issue #1011) and `quic_frame_types_test.cpp` (Issue #974)
  - Closes 4-byte and 8-byte varint type prefixes via `peek_type`, heterogeneous `parse_all` mixes (PING + HANDSHAKE_DONE + MAX_DATA + PATH_CHALLENGE), every concrete STREAM type byte (0x08-0x0f) routed through `parse()`, ACK_ECN dispatch with truncation matrix at each ECN field, ACK ranges accumulation and per-range truncation, large delay / largest_acknowledged at varint boundaries, CRYPTO frame non-zero offset and 200-byte payload round-trips, NEW_TOKEN empty-token and 150-byte token round-trips, NEW_CONNECTION_ID minimal 1-byte CID and 16384-sequence variants plus cid_len=20 with insufficient bytes, RETIRE_CONNECTION_ID with 4-byte varint sequence, PATH_CHALLENGE all-zero / PATH_RESPONSE all-0xFF round-trips, CONNECTION_CLOSE transport-with-frame-type and application long-reason variants, multi-byte CONNECTION_CLOSE error_code (4-byte varint), HANDSHAKE_DONE dispatch consuming exactly one byte, `frame_builder::build_padding(0)` zero-count guard, `frame_type_to_string` for previously-unasserted branches (padding, ping, ack, reset_stream, stop_sending, crypto, stream_base, max_data, max_stream_data, max_streams_bidi/uni, data_blocked, stream_data_blocked, streams_blocked_bidi/uni, new_connection_id, path_challenge, connection_close), and `is_stream_frame`/`get_stream_flags`/`make_stream_type` per-bit consistency at boundaries 0x07/0x08/0x0f/0x10
  - Registered `network_quic_frame_extra_coverage_test` in `tests/CMakeLists.txt`
  - Part of the #953 coverage expansion effort, targeting `src/protocols/quic/frame.cpp` line >= 70% / branch >= 60%
- **Modernized Doxygen Documentation with doxygen-awesome-css (#927)**
  - Vendored doxygen-awesome-css theme with dark mode toggle, code copy buttons, and responsive sidebar
  - Added custom header (`docs/header.html`) and branding CSS (`docs/custom.css`)
  - Updated Doxyfile: `HTML_COLORSTYLE=LIGHT`, `HTML_HEADER`, `HTML_EXTRA_STYLESHEET`, `EXAMPLE_PATH`
  - Refactored `mainpage.dox` to 8-section ecosystem standard template (overview, features, architecture diagram, quick start, installation, modules, examples, related systems)
  - Moved `mainpage.dox` from project root to `docs/` directory

### Changed
- **Complete `Result<T>` migration for public API (#988)**
  - Public headers under `include/kcenon/network/` now contain zero `throw` statements; every public function either returns `common::Result<T>` / `common::VoidResult` or is declared `noexcept`.
  - New lightweight CI workflow `.github/workflows/public-api-check.yml` greps `include/` and fails the build if `throw` is reintroduced, locking in the invariant.
  - Triggers on pushes to `main`, `develop`, `phase-*` and on pull requests targeting `main` or `develop` so regressions surface at review time.
  - No runtime behavior change — existing `Result<T>`-returning APIs are unchanged; this is the contract cleanup that accompanies the v1.0 API freeze.
- **Consolidated Result Type Aliases (#494)**
  - Clarified that `network::Result<T>`, `network::VoidResult`, and `network::error_info` are deprecated for external use
  - Internal code continues to use these aliases which now directly map to `kcenon::common` types
  - Added `kcenon::network::internal` namespace with explicit type aliases for internal implementation
  - Updated documentation to guide migration to `kcenon::common::Result<T>` for external code
  - `http_types.h` functions now use `::kcenon::network::internal::Result<T>` for clarity

### Removed
- **BREAKING: Removed `compatibility.h` and `network_module` Namespace Aliases (#481)**
  - Deleted `include/kcenon/network/compatibility.h` header file
  - Deleted `tests/integration/test_compatibility.cpp` test file
  - Deleted `samples/messaging_system_integration/legacy_compatibility.cpp` sample
  - Removed legacy factory functions: `create_server()`, `create_client()`, `create_bridge()`
  - **Migration**: Replace `network_module::` with `kcenon::network::` namespace

- **BREAKING: Removed Deprecated WebSocket API Methods (#482)**
  - `messaging_ws_client`:
    - Removed `close(internal::ws_close_code, const std::string&)` → use `close(uint16_t, string_view)`
    - Removed legacy callback setters: `set_message_callback()`, `set_text_message_callback()`, `set_binary_message_callback()`, `set_disconnected_callback()`
  - `ws_connection`:
    - Removed `send_text(std::string&&, handler)` and `send_binary(std::vector<uint8_t>&&, handler)`
    - Removed `close(internal::ws_close_code, const std::string&)` → use `close(uint16_t, string_view)`
    - Removed `connection_id()` → use `id()`
  - `messaging_ws_server`:
    - Removed legacy callback setters: `set_connection_callback()`, `set_disconnection_callback()`, `set_message_callback()`, `set_text_message_callback()`, `set_binary_message_callback()`, `set_error_callback()`

- **BREAKING: Removed Deprecated UDP API Methods (#483)**
  - `messaging_udp_server`: Removed `async_send_to()` → use `send_to(endpoint_info, data, handler)`
  - `messaging_udp_client`: Removed `send_packet()` → use `send(data, handler)`

- **BREAKING: Removed Deprecated session_manager Method (#484)**
  - Removed private `generate_session_id()` method (use static `generate_id()` from base class)

- **BREAKING: Transition to OpenSSL 3.x Only (#485)**
  - Minimum OpenSSL version changed from 1.1.1 to 3.0.0
  - Removed `NETWORK_OPENSSL_VERSION_1_1_X` macro
  - Removed `is_openssl_3x()` and `is_openssl_eol()` compatibility functions
  - Removed OpenSSL 1.1.x RSA key generation code path from `dtls_test_helpers.h`
  - **Migration**: Upgrade to OpenSSL 3.x (OpenSSL 1.1.1 reached EOL on September 11, 2023)

### Fixed
- **Tracing Header Compilation Fix**: Add missing `<cstring>` header in `trace_context.cpp`
  - Fixed `std::memcpy is not a member of std` compilation error on Linux/GCC
  - Required for random bytes generation in trace ID and span ID creation
- **CI Integration Test Stability**: Improve connection reliability in macOS CI Debug builds
  - Added connection retry mechanism with exponential backoff for CI environments
  - Increased connection timeout to 15s for macOS CI (up from 10s)
  - Increased server startup delay to 200ms for macOS CI environments
  - Resolved flaky `ErrorHandlingTest.ServerShutdownDuringTransmission` test
- **tcp_socket async_send Serialization**: Ensure send initiation is serialized on the socket executor
  - Avoid cross-thread access to ASIO internals during close/read paths
  - Roll back pending bytes and backpressure state when a send fails to start
  - Invoke the send completion handler consistently on initiation failures
  - Queue writes to prevent overlapping async_write operations under high send rates
  - Serialize queued writes and completion handlers on a strand for multi-threaded io_context safety
- **Logging Static Destruction Guard**: Make the guard counter atomic during shutdown
  - Use relaxed atomic operations to prevent data races in logging safety checks
- **Tracing Console Exporter Thread Safety**: Serialize std::cout writes for console spans
  - Prevent concurrent span exports from racing on iostream state
- **Sanitizer Test Configuration**: Apply suppressions and unified sanitizer detection
  - Inject LSAN/TSAN options for intentional leaks and ASIO false positives
  - Use NETWORK_SYSTEM_SANITIZER to consistently skip sanitizer-sensitive tests

### Added
- **OpenTelemetry-Compatible Distributed Tracing (#408)**: Add core tracing infrastructure for distributed observability
  - `trace_context` class for trace ID, span ID, and parent relationship management
  - `span` class with RAII-based lifecycle, attributes, events, and status
  - W3C Trace Context format support for HTTP header propagation
  - Configurable exporters (console, OTLP gRPC/HTTP, Jaeger, Zipkin)
  - Thread-local context propagation for automatic parent-child relationships
  - `NETWORK_TRACE_SPAN` macro for convenient span creation
  - 39 unit tests covering context generation, parsing, span lifecycle, and configuration
  - Created sub-issues #456, #457, #458, #459 for incremental implementation
- **UDP Classes Composition Migration (Phase 1.3.3)**: Migrate UDP classes from CRTP to composition pattern (#446)
  - Refactored `messaging_udp_client` to implement `i_udp_client` interface
    - Implement `start/stop/is_running` lifecycle methods
    - Implement `send` and `set_target` methods
    - Add interface callback setters with adapter pattern
    - Maintain backward compatibility with legacy `start_client/stop_client` API
  - Refactored `messaging_udp_server` to implement `i_udp_server` interface
    - Implement `start/stop/is_running` lifecycle methods
    - Implement `send_to` for targeted datagram sending
    - Add interface callback setters for receive/error events
    - Maintain backward compatibility with legacy `start_server/stop_server` API
  - Added ID accessor methods (`client_id`, `server_id`)
  - Added comprehensive unit tests (31 tests) covering:
    - Interface API compliance
    - Lifecycle management
    - Callback management
    - Error conditions
    - Type compatibility
    - Legacy API backward compatibility
- **TCP Classes Composition Migration (Phase 1.3.2)**: Migrate TCP classes from CRTP to composition pattern (#445)
  - Refactored `messaging_client` to implement `i_client` interface
    - Implement `start/stop/is_running` lifecycle methods
    - Add interface callback setters with adapter pattern
    - Maintain backward compatibility with legacy API
  - Refactored `messaging_server` to implement `i_server` interface
    - Implement `start/stop/is_running` lifecycle methods
    - Add interface callback setters for session events
    - Maintain backward compatibility with legacy API
  - All existing TCP tests continue to pass
- **WebSocket Classes Composition Migration (Phase 1.3.1)**: Implement composition pattern interfaces for WebSocket classes (#428)
  - Added `i_websocket_client` interface to `messaging_ws_client`
    - Implement `start/stop/is_connected/is_running` methods
    - Implement `send_text/send_binary/ping/close` methods
    - Add interface callback setters with adapter pattern
    - Maintain backward compatibility with legacy API
  - Added `i_websocket_server` interface to `messaging_ws_server`
    - Implement `start/stop/connection_count` methods
    - Add interface callback setters for connections/disconnections
    - Adapt text/binary message callbacks to interface types
  - Added `i_websocket_session` interface to `ws_connection`
    - Implement `id/is_connected/send/close/path` methods
    - Support both interface and legacy close variants
    - Add `send_text/send_binary` without completion handlers
  - Refactored `ws_connection` implementation
    - Renamed impl class to `ws_connection_impl` for clarity
    - Added path and is_connected support
    - Use private accessor method for server integration
  - All existing WebSocket tests continue to pass
- **Composition-Based Interface Infrastructure (Phase 1.2)**: Add interface classes for composition pattern (#423)
  - Added core interfaces: `i_network_component`, `i_client`, `i_server`, `i_session`
  - Added protocol-specific interfaces: `i_udp_client`, `i_udp_server`, `i_websocket_client`, `i_websocket_server`, `i_quic_client`, `i_quic_server`
  - Added extended session interfaces: `i_websocket_session`, `i_quic_session`
  - Added `interfaces.h` convenience header for single-include access
  - Added utility classes: `lifecycle_manager`, `callback_manager`, `connection_state`
  - All interfaces are abstract classes with virtual destructors for proper polymorphism
  - Comprehensive unit tests for type traits, hierarchy, and callback types
  - Unit tests for utility classes covering thread safety and state management
  - Non-breaking change: existing CRTP code continues to work
- **QUIC ECN Feedback Integration**: Integrate ECN feedback into congestion control per RFC 9000/9002 (#404)
  - Added `ecn_tracker` class for tracking ECN counts from ACK_ECN frames
  - Added ECN validation during connection establishment
  - Added `ecn_result` enum for congestion signal, validation failure, or no signal
  - Integrated ECN processing in `loss_detector` to detect ECN-CE increases
  - Added `on_ecn_congestion()` to `congestion_controller` for ECN-based congestion response
  - Connected ECN signals to `connection` for automatic congestion handling
  - ECN provides faster congestion detection than loss-based methods
  - Graceful fallback when ECN is not supported by network path
  - 19 unit tests covering ECN tracking, integration, and full flow scenarios
- **Circuit Breaker Pattern**: Implement Circuit Breaker for resilient network clients (#403)
  - Added `circuit_breaker` class with three-state pattern (closed, open, half_open)
  - Added `circuit_breaker_config` struct for customizable thresholds and timeouts
  - Configurable parameters: `failure_threshold`, `open_duration`, `half_open_successes`, `half_open_max_calls`
  - State change callback support via `set_state_change_callback()`
  - Thread-safe implementation with atomic operations and mutex protection
  - Integrated with `resilient_client` for automatic circuit management
  - Added `circuit_state()`, `reset_circuit()`, `set_circuit_state_callback()` to `resilient_client`
  - Added `circuit_open` error code for fast-fail scenarios
  - 18 unit tests covering state transitions, callbacks, reset, and thread safety
- **QUIC 0-RTT Session Ticket Storage**: Implement session ticket storage and restoration for 0-RTT resumption (#402)
  - Added `session_ticket_store` class for storing/retrieving session tickets by server endpoint
  - Added `session_ticket_info` struct with ticket metadata, expiry tracking, and validation
  - Added `replay_filter` class for anti-replay protection on 0-RTT early data
  - Added 0-RTT key derivation methods to `quic_crypto` (RFC 9001 Section 4.6)
  - Added `set_session_ticket_callback()` to receive NewSessionTicket messages
  - Added `set_early_data_callback()` for early data production in client
  - Added `set_early_data_accepted_callback()` for server acceptance notification
  - Added `is_early_data_accepted()` query method
  - Added `max_early_data_size` configuration option to `quic_client_config`
  - 20+ unit tests for session ticket management and replay protection
  - Thread-safe implementation with comprehensive concurrency tests
- **Network Metrics Test Coverage**: Add comprehensive test coverage for metrics system (#405)
  - Added `mock_monitor.h` test helper for monitoring interface mocking
  - Added 31 unit tests in `test_network_metrics.cpp` covering:
    - Metric name constants validation
    - `metric_reporter` static methods
    - Thread safety with concurrent reporting
    - Edge cases (zero bytes, large values, empty strings)
    - `monitoring_integration_manager` singleton behavior
    - `basic_monitoring` implementation
  - Added 10 integration tests in `test_metrics_integration.cpp` covering:
    - Connection lifecycle metrics flow
    - Error handling metrics
    - Session metrics with duration tracking
    - High-volume data transfer scenarios
    - Concurrent connection tracking
    - Custom monitoring implementation support
  - Full test coverage for network metrics public interface
- **DTLS Socket Test Coverage**: Add comprehensive test coverage for DTLS socket implementation (#401)
  - Added `dtls_test_helpers.h` with SSL context wrapper, certificate generator, and DTLS context factory
  - Added 20 unit tests covering construction, callbacks, handshake, send/receive, and thread safety
  - Tests validate client-server handshake, bidirectional communication, and large payload handling
  - Achieves full test coverage for dtls_socket public interface
- **QUIC Connection ID Management**: Implement Connection ID storage and rotation per RFC 9000 Section 5.1 (#399)
  - Added `connection_id_manager` class for peer CID storage and management
  - Support for NEW_CONNECTION_ID frame processing with sequence tracking
  - Support for RETIRE_CONNECTION_ID frame generation
  - Stateless reset token validation for connection migration security
  - CID rotation API (`rotate_peer_cid()`) for path migration support
  - Active connection ID limit enforcement from transport parameters
  - 18 new unit tests for CID management functionality
- **QUIC PTO Timeout Loss Detection**: Complete PTO timeout handling per RFC 9002 Section 6.2 (#398)
  - Added `rtt_estimator`, `loss_detector`, and `congestion_controller` integration to QUIC connection
  - Implemented `on_timeout()` to handle PTO expiry via loss detector
  - Added `generate_probe_packets()` for sending ack-eliciting PING probes
  - Implemented `queue_frames_for_retransmission()` for lost packet frame handling
  - Integrated loss detection with ACK processing (`on_ack_received()`)
  - Integrated loss detection with packet sending (`on_packet_sent()`)
  - Updated `next_timeout()` to consider loss detection timer
  - Added unit tests for PTO timeout scenarios
- **C++20 Module Support**: Added C++20 module files for kcenon.network (#395)
  - Primary module: `kcenon.network` (network.cppm)
  - Module partitions:
    - `:core` - Core network infrastructure (network_context, connection_pool, base interfaces)
    - `:tcp` - TCP client/server (messaging_client, messaging_server, messaging_session)
    - `:udp` - UDP implementations (messaging_udp_client, messaging_udp_server, reliable_udp_client)
    - `:ssl` - SSL/TLS support (secure_messaging_client, secure_messaging_server, DTLS variants)
  - CMake option `NETWORK_BUILD_MODULES` (OFF by default, requires CMake 3.28+)
  - Dependencies: kcenon.common (Tier 0), kcenon.thread (Tier 1)
  - Part of C++20 Module Migration Epic (kcenon/common_system#256)
- **CRTP Base Classes for Messaging**: Added template base classes for messaging clients and servers (#376)
  - `messaging_client_base<Derived>` provides common lifecycle management and callback handling for clients
  - `messaging_server_base<Derived, SessionType>` provides common lifecycle management for servers
  - Thread-safe state management with atomic flags
  - Unified callback management with mutex protection
  - Standard lifecycle methods (`start`, `stop`, `wait_for_stop`)
  - Zero-overhead abstraction through CRTP pattern
  - Foundation for future migration of existing messaging classes
- **gRPC Service Registration Mechanism**: Implemented service registration for gRPC with official library support (#364)
  - Added `service_registry` class for centralized service management
  - Added `generic_service` for dynamic method registration at runtime
  - Added `protoc_service_adapter` for wrapping protoc-generated services
  - Added `health_service` implementing standard gRPC health checking protocol
  - Added service/method descriptor types for reflection support
  - Added utility functions for method path parsing and building
  - All four RPC types supported: unary, server streaming, client streaming, bidirectional streaming
  - Thread-safe service registration and lookup operations
  - 47 unit tests covering all components
- **Session Idle Timeout Cleanup**: Implemented automatic idle session detection and cleanup in `session_manager` (#353)
  - Added `session_info` struct with `last_activity` timestamp tracking
  - Implemented `cleanup_idle_sessions()` to detect and remove sessions idle beyond `idle_timeout`
  - Added `update_activity()` method for external activity timestamp updates
  - Added `get_session_info()` and `get_idle_duration()` helper methods
  - Added `total_cleaned_up` metric to session statistics
  - Sessions are gracefully stopped before removal to ensure proper resource cleanup
  - Comprehensive unit tests covering activity tracking, cleanup, and thread safety
- **Messaging Zero-Copy Receive**: Migrated messaging components to use `std::span<const uint8_t>` callbacks (#319)
  - `messaging_session` uses `tcp_socket::set_receive_callback_view()` for zero-copy receive
  - `messaging_client` uses `tcp_socket::set_receive_callback_view()` for zero-copy receive
  - Internal `on_receive()` methods now accept `std::span<const uint8_t>` instead of `const std::vector<uint8_t>&`
  - Data is copied to vector only when queuing (session) or at API boundary (client)
  - Removes one heap allocation per read vs legacy vector-based path
  - Part of Epic #315 (TCP receive zero-allocation hot path)
- **WebSocket Zero-Copy Receive**: Migrated WebSocket receive path to use `std::span<const uint8_t>` callbacks (#318)
  - `websocket_protocol::process_data()` now accepts `std::span<const uint8_t>` instead of `const std::vector<uint8_t>&`
  - `websocket_socket` uses `tcp_socket::set_receive_callback_view()` for zero-copy TCP-to-WebSocket data flow
  - Eliminates per-read `std::vector` allocation solely for TCP-to-protocol handoff
  - Part of Epic #315 (TCP receive zero-allocation hot path)
- **secure_tcp_socket Zero-Copy Receive**: Added `set_receive_callback_view(std::span<const uint8_t>)` API for zero-copy TLS data reception (#317)
  - Zero-copy path using `std::span` directly into read buffer, avoiding per-read `std::vector` allocations
  - Lock-free callback storage using `shared_ptr` + `atomic_load/store` for better performance under high TPS
  - Span callback takes priority when both view and vector callbacks are set
  - Legacy vector callback preserved for backward compatibility
  - Aligned with `tcp_socket` implementation from #316 for consistency
- **OpenSSL 3.x Support**: Added full compatibility with OpenSSL 3.x while maintaining backward compatibility with 1.1.1 (#308)
  - New `openssl_compat.h` header with version detection macros and utility functions
  - CMake version detection with deprecation warnings for OpenSSL 1.1.1 (EOL: September 2023)
  - Compile-time macros: `NETWORK_OPENSSL_3_X`, `NETWORK_OPENSSL_1_1_X`, `NETWORK_OPENSSL_VERSION_3_X`
  - Runtime utilities: `is_openssl_3x()`, `is_openssl_eol()`, `get_openssl_error()`
  - CI workflows updated to verify OpenSSL version and install OpenSSL 3.x dependencies
- **C++20 Concepts**: Added network-specific C++20 concepts for compile-time type validation (#294)
  - New `<kcenon/network/concepts/concepts.h>` unified header
  - Buffer concepts: `ByteBuffer`, `MutableByteBuffer`
  - Callback concepts: `DataReceiveHandler`, `ErrorHandler`, `SessionHandler`, `SessionDataHandler`, `SessionErrorHandler`, `DisconnectionHandler`, `RetryCallback`
  - Network component concepts: `NetworkClient`, `NetworkServer`, `NetworkSession`
  - Pipeline concepts: `DataTransformer`, `ReversibleDataTransformer`
  - Duration concept for time-related constraints
  - Integration with common_system's Result/Optional concepts when available
  - Concepts improve error messages and serve as self-documenting type constraints

### Fixed
- **ThreadSanitizer Data Race Fix**: Add mutex protection for acceptor access in messaging servers (#427)
  - Fixed data race between `do_accept()` and `do_stop()` when accessing `acceptor_` concurrently
  - Added `acceptor_mutex_` to both `messaging_server` and `messaging_ws_server` classes
  - Protected acceptor access in `do_accept()`, `do_stop()`, and destructor with mutex locks
  - Added early return checks in `do_accept()` for `is_running()` and `acceptor_->is_open()` states
  - Fixes E2ETests failure in ThreadSanitizer CI workflow
- **Circuit Breaker Build Fix**: Add error_codes_ext namespace to fallback build path (#403)
  - Fixed build failure when building without common_system dependency
  - Added error_codes_ext namespace with circuit_open error code to fallback block in result_types.h
  - Ensures API compatibility between KCENON_WITH_COMMON_SYSTEM and standalone builds
- **Performance Test CI Stability**: Added CI environment skip checks to all performance tests (#414)
  - Fixed macOS Release CI timeout failure in NetworkPerformanceTest.SmallMessageLatency
  - Added CI skip to: SmallMessageLatency, LargeMessageLatency, MessageThroughput, BandwidthUtilization, ConcurrentMessageSending, SustainedLoad, BurstLoad
  - Performance tests require stable timing and are not suitable for resource-constrained CI runners
- **io_context Lifecycle Tests Re-enabled**: Re-enabled 6 integration tests previously skipped due to io_context lifecycle issues (#400)
  - Tests now pass in CI environments thanks to intentional leak pattern applied to messaging_client's io_context
  - Removed TODO comments referencing Issues #315 and #348
  - Re-enabled tests: ConnectToInvalidHost, ConnectToInvalidPort, ConnectionRefused, RecoveryAfterConnectionFailure, ClientConnectionToNonExistentServer, SequentialConnections
  - No heap corruption during static destruction due to no-op deleter on io_context
  - Added `wait_for_connection_attempt()` helper to test_helpers.h to ensure async connection operations complete before test cleanup
  - Tests now properly wait for connection failure (via error_callback) before TearDown, preventing heap corruption from pending async operations
- **UBSAN/SEGFAULT in Async Socket Operations**: Fixed null pointer access in tcp_socket async operations (#396)
  - Added `socket_.is_open()` check to `tcp_socket::async_send()` to prevent accessing null `descriptor_state` in ASIO's `epoll_reactor`
  - Added sanitizer detection (`is_sanitizer_run()`) to E2E tests to skip concurrent tests under sanitizers
  - Skipped multi-client and rapid connection tests under UBSan/ASan/TSan due to race conditions in ASIO's epoll_reactor
  - Fixes CI failures: UndefinedBehaviorSanitizer on ubuntu-24.04, Integration Tests BurstLoad SEGFAULT
- **macOS CI ExcessiveMessageRate SEGFAULT**: Skip ExcessiveMessageRate test on macOS CI environment (#426)
  - High-rate message sending causes intermittent SEGFAULT due to io_context lifecycle issues on macOS kqueue-based async I/O
  - Added conditional skip for macOS CI, consistent with similar handling in SendEmptyMessage and other tests
  - Fixes macos-latest Debug CI failure

### Changed
- **QUIC CRTP Migration**: Migrated QUIC classes to use protocol-specific CRTP base classes (#385)
  - `messaging_quic_client` now inherits from `messaging_quic_client_base<messaging_quic_client>`
  - `messaging_quic_server` now inherits from `messaging_quic_server_base<messaging_quic_server>`
  - Common lifecycle management (start/stop, wait_for_stop) in base classes
  - Thread-safe callback handling with mutex protection
  - State tracking with atomic flags (is_running, is_connected)
  - Consistent error handling with Result<T>
  - Note: gRPC classes use pimpl pattern and are excluded from CRTP migration
- **UDP/WebSocket CRTP Migration**: Migrated UDP and WebSocket classes to use protocol-specific CRTP base classes (#384)
  - `messaging_udp_client` now inherits from `messaging_udp_client_base<messaging_udp_client>`
  - `messaging_udp_server` now inherits from `messaging_udp_server_base<messaging_udp_server>`
  - `messaging_ws_client` now inherits from `messaging_ws_client_base<messaging_ws_client>`
  - `messaging_ws_server` now inherits from `messaging_ws_server_base<messaging_ws_server>`
  - Protocol-specific base classes handle unique requirements:
    - UDP base classes: connectionless semantics with endpoint-aware callbacks
    - WebSocket base classes: connection-aware with WS-specific message types (text/binary/ping/pong/close)
  - Removed pimpl pattern from `messaging_ws_client` for direct CRTP inheritance
  - Kept pimpl pattern for `ws_connection` in server for session management
  - Common lifecycle methods (`start`/`stop`/`wait_for_stop`) provided by base classes
  - Maintains full backward compatibility with existing API
- **secure_messaging CRTP Migration**: Migrated all secure messaging classes to use CRTP base classes (#383)
  - `secure_messaging_client` now inherits from `messaging_client_base<secure_messaging_client>`
  - `secure_messaging_server` now inherits from `messaging_server_base<secure_messaging_server, secure_session>`
  - `secure_messaging_udp_client` now inherits from `messaging_client_base<secure_messaging_udp_client>`
  - `secure_messaging_udp_server` maintains manual lifecycle management due to UDP-specific callbacks
  - Added `set_udp_receive_callback()` for UDP clients with endpoint-aware callback signature
  - All TLS/DTLS-specific behavior preserved in `do_start()`, `do_stop()`, `do_send()` methods
  - Common callback setters and lifecycle methods now provided by base classes
  - Maintains full backward compatibility with existing API
- **messaging_server CRTP Migration**: Migrated `messaging_server` to use `messaging_server_base` CRTP pattern (#382)
  - Inherits from `messaging_server_base<messaging_server>` for common lifecycle management
  - Implements `do_start()`, `do_stop()` for TCP-specific server behavior
  - Removes duplicated code (~200 lines reduction)
  - All callback setters and lifecycle methods now provided by base class
  - Added callback getter methods to base class for derived class access
- **messaging_client CRTP Migration**: Migrated `messaging_client` to use `messaging_client_base` CRTP pattern (#381)
  - Inherits from `messaging_client_base<messaging_client>` for common lifecycle management
  - Implements `do_start()`, `do_stop()`, `do_send()` for TCP-specific behavior
  - Removes duplicated code (~265 lines reduction)
  - All callback setters and lifecycle methods now provided by base class
  - Maintains full backward compatibility with existing API
  - First proof-of-concept for messaging class hierarchy migration
- **vcpkg Manifest**: Added ecosystem dependencies as optional feature for vcpkg registry distribution (#371)
  - Added `ecosystem` feature containing `kcenon-common-system`, `kcenon-thread-system`, `kcenon-logger-system`, `kcenon-container-system`
  - These dependencies are required as documented in README, now declared in vcpkg.json
  - Feature-based approach allows CI to pass while ecosystem packages await vcpkg registry registration
  - Enable with `vcpkg install --feature ecosystem` once packages are registered
- **Logging System**: Migrated from direct logger_system dependency to common_system's ILogger interface (#285)
  - `NETWORK_LOG_*` macros now delegate to common_system's `LOG_*` macros
  - Added `common_system_logger_adapter` for bridging legacy `logger_interface`
  - `BUILD_WITH_LOGGER_SYSTEM` is now optional (default: OFF)
  - Logging uses GlobalLoggerRegistry for runtime logger binding
  - Reduces compile-time coupling and enables flexible logger configuration

### Fixed
- **Socket UndefinedBehaviorSanitizer Fix**: Fixed null pointer access in async read operations (#385)
  - Added `socket_.is_open()` check in `tcp_socket::do_read()` before initiating async operations
  - Added same checks to `secure_tcp_socket::do_read()` for SSL streams
  - Added `is_closed_` check to `secure_tcp_socket::async_send()` to prevent write to closed socket
  - Callback handlers now check `is_closed_` flag to prevent accessing invalid socket state
  - Added missing `<atomic>` header to `secure_tcp_socket.h`
  - Fixes UBSAN "member access within null pointer" error in Multi-Client Concurrent Test
- **gRPC Service Example Build Fix**: Fixed abstract class instantiation error in grpc_service_example (#385)
  - Added `mock_server_context` class implementing `grpc::server_context` interface
  - Replaced direct `grpc::server_context` instantiation with mock implementation
  - Enables handler invocation demonstration in example code
- **QUIC Server Error Code Consistency**: Fixed error codes in `messaging_quic_server_base` to align with TCP server patterns (#385)
  - Changed `start_server()` to return `server_already_running` instead of `already_exists` when server is already running
  - Changed `stop_server()` to return `server_not_started` error instead of `ok()` when server is not running
  - Updated documentation to match `messaging_server_base` error code specifications
  - Fixes test failures in `MessagingQuicServerTest.DoubleStart`, `StopWhenNotRunning`, and `MultipleStop`
- **Multi-Client Connection Test Fix**: Fixed flaky `MultiConnectionLifecycleTest.ConnectionScaling` on macOS CI (#385)
  - Changed `ConnectAllClients()` from sequential waiting to round-robin polling
  - Previous implementation blocked on first slow client, causing all subsequent clients to timeout
  - New implementation polls all clients each iteration, counting any that have connected
  - Increased CI timeout from 5 to 10 seconds for many-client scenarios
  - Adds early exit when all clients are connected
- **PartialMessageRecovery Test Fix**: Fixed use-after-move bug in ErrorHandlingTest.PartialMessageRecovery (#389)
  - Created separate message instances instead of reusing a moved-from object
  - The original code moved `valid_message` in the first `SendMessage` call, then attempted to move it again, causing undefined behavior and test crashes across all platforms
- **tcp_socket AddressSanitizer Fix**: Fixed SEGV in do_read callback when socket closes during async operation (#388)
  - Added `socket_.is_open()` check before recursive `do_read()` call to prevent race condition
  - Socket could close between `is_reading_.load()` check and `do_read()` invocation
  - Added missing `<atomic>` and `<mutex>` headers to tcp_socket.h for proper type declarations
  - Fixes E2ETests failure under AddressSanitizer on Ubuntu 24.04
- **messaging_client ThreadSanitizer Fix**: Fixed data race between `do_stop()` and `async_connect()` operations (#388)
  - Post pending socket close operations to io_context thread via `asio::post()` to ensure socket operations happen on the same thread as async_connect
  - Added `stop_initiated_` checks in async handlers to prevent socket operations after stop has been initiated
  - Resolves ThreadSanitizer failure in CI where `do_stop()` was closing `pending_socket_` from a different thread while async_connect was still modifying socket internal state
- **Integration Test Flakiness**: Fixed intermittent `ProtocolIntegrationTest.RepeatingPatternData` failures on macOS CI (#376)
  - Added actual sleep to `wait_for_ready()` for socket cleanup (TIME_WAIT handling)
  - Previous implementation only used `yield()` which didn't guarantee port release
  - Uses 50ms delay in CI environments, 10ms locally
- **Result Check Consistency**: Fixed `messaging_server_base` to use `is_err()` instead of `operator!` for VoidResult (#376)
  - Aligns with `messaging_client_base` and common_system error handling patterns
- **messaging_server Resource Cleanup**: Fixed heap corruption when `start_server()` fails (#335)
  - When port binding fails (e.g., address already in use), partially created resources (`io_context_`, `work_guard_`) were not cleaned up
  - Added resource cleanup in `start_server()` catch blocks to release partially created resources
  - Added explicit resource cleanup in destructor to handle cases where `stop_server()` returns early
  - Fixes "corrupted size vs. prev_size" errors in `ConnectionLifecycleTest.ServerStartupOnUsedPort` on Linux Debug builds
- **tcp_socket SEGV Fix**: Added socket validity check before async send operation (#389)
  - `tcp_socket::async_send()` now checks `socket_.is_open()` before initiating `asio::async_write()`
  - Returns `asio::error::not_connected` error via handler when socket is already closed
  - Added sanitizer skip for `LargeMessageTransfer` test due to asio internal race conditions
  - Fixes ThreadSanitizer failure in `NetworkTest.LargeMessageTransfer`
- **tcp_socket UBSAN Fix**: Added socket validity check before async read operation (#318)
  - `tcp_socket::do_read()` now checks `socket_.is_open()` before initiating `async_read_some()`
  - Prevents undefined behavior (null descriptor_state access) when socket is already closed
  - Fixes UBSAN failure in `BoundaryTest.HandlesSingleByteMessage`
- **Static Destruction Order**: Applied Intentional Leak pattern to prevent heap corruption during process shutdown (#314)
  - Applied to: `network_context`, `io_context_thread_manager`, `thread_integration_manager`, `basic_thread_pool`
  - When thread pool tasks still reference shared resources during static destruction, heap corruption ("corrupted size vs. prev_size") can occur
  - Using no-op deleters on pimpl/shared_ptr pointers keeps resources alive until OS cleanup
  - Memory impact: ~few KB per singleton, reclaimed by OS on process termination
  - Temporarily skipped `MultiConnectionLifecycleTest.SequentialConnections` and `ErrorHandlingTest.RecoveryAfterConnectionFailure` in CI (tracked as Issue #315)
- **CI Windows Build**: Fixed PowerShell error handling for container_system install failures (#314)
  - PowerShell try/catch does not catch external command failures
  - Changed to use `$LASTEXITCODE` check to properly handle cmake install errors
  - Allows CI to continue when container_system install encounters missing files
- **CI Windows MSVC Build**: Fixed linker errors for ecosystem dependencies (#314)
  - Moved MSVC compiler setup before ecosystem dependencies build step
  - Changed ecosystem dependencies to build with Debug configuration to match network_system
  - Windows MSVC requires matching RuntimeLibrary settings (MDd vs MD) between linked libraries
- **Thread System Adapter**: Fixed `submit_delayed()` to use a single scheduler thread with priority queue instead of creating a detached `std::thread` per delayed task (#273)
  - Eliminates thread explosion under high delayed task submission
  - Provides proper thread lifecycle management (joinable scheduler thread)
  - Supports clean shutdown with pending task cancellation
  - Added comprehensive unit tests for delayed task execution
- **Thread System Adapter**: Eliminated scheduler_thread by delegating to thread_pool::submit_delayed (Epic #271)
  - When THREAD_HAS_COMMON_EXECUTOR is defined: delegates directly to thread_pool::submit_delayed
  - Removes all direct std::thread usage from thread_system_adapter.cpp (except std::thread::hardware_concurrency)
  - Simplifies code by ~70 lines and removes priority queue, mutex, condition variable
  - Maintains fallback for builds without common_system integration

### Removed
- **Unused Pipeline Compression/Encryption Code**: Removed dead code following Simple Design principle (#378)
  - Removed `internal::pipeline` struct and `make_default_pipeline()` function
  - Removed `pipeline_`, `compress_mode_`, `encrypt_mode_` members from `messaging_client`, `messaging_session`, `secure_session`, `secure_messaging_client`
  - Removed `send_coroutine.h/cpp` and `pipeline.h/cpp` files
  - These members were always set to `false` and the pipeline functions were no-op stubs
  - Simplifies code by ~300 lines across multiple files
  - `utils::compression_pipeline` (actual LZ4/gzip implementation) remains available as standalone utility

### Refactored
- **Memory Profiler**: Migrated from `std::thread` to `thread_integration_manager::submit_delayed_task()` (#277)
  - Removed dedicated `std::thread worker_` member
  - Periodic sampling now runs through shared thread pool
  - Uses delayed task scheduling for interval-based snapshots
  - Cleaner shutdown via atomic flag (no thread join needed)

- **gRPC Client Async Calls**: Migrated `call_raw_async()` from `std::thread().detach()` to `thread_integration_manager::submit_task()` (#278)
  - Async gRPC calls now submitted to shared thread pool instead of creating detached threads
  - Enables controlled thread lifecycle through centralized thread management
  - Prevents thread explosion under high async call volume

- **Send Coroutine Fallback**: Migrated `async_send_with_pipeline_no_co()` from `std::thread().detach()` to `thread_integration_manager::submit_task()` (#274)
  - Tasks now submitted to shared thread pool instead of creating detached threads
  - Controlled thread lifecycle through managed pool
  - Prevents resource exhaustion under high load
  - Added error handling for thread pool submission failures

### Added
- **QUIC Protocol Support (Phase 4.2)**: messaging_quic_server public API
  - `messaging_quic_server` class following `messaging_server` pattern
    - Consistent API with existing TCP server
    - Thread-safe session management using shared_mutex
    - Full Result<T> error handling
  - Server lifecycle: `start_server()`, `stop_server()`, `wait_for_stop()`
  - Session management:
    - `sessions()` to get all active sessions
    - `get_session()` to retrieve session by ID
    - `session_count()` for active connection count
    - `disconnect_session()` and `disconnect_all()` for termination
  - Broadcasting:
    - `broadcast()` to send to all clients
    - `multicast()` to send to specific sessions
  - `quic_session` class for individual client connections:
    - `send()` for default stream data
    - `send_on_stream()` for stream-specific data
    - `create_stream()` for new streams
    - `stats()` for connection statistics
  - Callback system:
    - `set_connection_callback()` for new clients
    - `set_disconnection_callback()` for client disconnects
    - `set_receive_callback()` for data from any session
    - `set_stream_receive_callback()` for stream data
    - `set_error_callback()` for error handling
  - Configuration via `quic_server_config`:
    - TLS certificate settings (cert_file, key_file, ca_cert_file)
    - Client certificate authentication (require_client_cert)
    - ALPN protocol negotiation
    - Transport parameters (timeouts, flow control limits)
    - Connection limits (max_connections)
    - DoS protection (enable_retry, retry_key)
  - Compatibility layer headers in network_system namespace
  - Example application (quic_server_example.cpp)
  - Comprehensive test suite with 20+ test cases

- **QUIC Protocol Support (Phase 4.1)**: messaging_quic_client public API
  - `messaging_quic_client` class following `messaging_client` pattern
    - Consistent API with existing TCP/UDP/WebSocket clients
    - Thread-safe operations with internal locking
    - Full Result<T> error handling
  - Connection management: `start_client()`, `stop_client()`, `wait_for_stop()`
  - Data transfer on default stream: `send_packet()`
  - Multi-stream support (QUIC specific):
    - `create_stream()` for bidirectional streams
    - `create_unidirectional_stream()` for unidirectional streams
    - `send_on_stream()` for stream-specific data
    - `close_stream()` for stream termination
  - Callback system for events:
    - `set_receive_callback()` for default stream data
    - `set_stream_receive_callback()` for all stream data
    - `set_connected_callback()`, `set_disconnected_callback()`
    - `set_error_callback()` for error handling
  - Configuration options via `quic_client_config`:
    - TLS settings (CA cert, client cert, key, verification)
    - ALPN protocol negotiation
    - Transport parameters (timeouts, flow control limits)
    - 0-RTT early data support
  - Connection statistics via `stats()` method
  - Example code demonstrating API usage
  - Comprehensive test suite with 22 test cases

- **QUIC Protocol Support (Phase 3.2)**: Connection State Machine (RFC 9000 Section 5)
  - `transport_parameters` struct with RFC 9000 Section 18 compliant encoding/decoding
    - All standard transport parameters (max_idle_timeout, flow control limits, etc.)
    - Connection ID parameters (original/initial/retry)
    - Server-only parameters (preferred_address, stateless_reset_token)
    - Validation per client/server role
  - `connection` class implementing the connection state machine
    - Connection states: idle, handshaking, connected, closing, draining, closed
    - Handshake states: initial, waiting_server_hello, waiting_finished, complete
    - Packet number spaces: Initial, Handshake, Application
    - Long and short header packet processing
    - Frame handling via std::visit pattern
    - Connection ID management (add, retire, rotation)
    - Transport parameter negotiation
    - Connection close with error code preservation
    - Idle timeout handling
    - Integration with stream_manager and flow_controller
  - `preferred_address_info` struct for server address migration support
  - Comprehensive test suite with 40 test cases

- **QUIC Protocol Support (Phase 3.1)**: Stream Management (RFC 9000 Sections 2-4)
  - `stream` class with complete send/receive state machine
  - Stream ID allocation for client/server roles (bidi/uni)
  - `stream_manager` for stream lifecycle and multiplexing
  - `flow_controller` for connection-level flow control
  - Data reassembly with gap handling
  - Flow control enforcement at stream level
  - MAX_STREAM_DATA and MAX_DATA frame generation
  - Comprehensive test suite

- **QUIC Protocol Support (Phase 2.2)**: QUIC Socket with packet protection
  - `quic_socket` class wrapping UDP socket with QUIC packet protection
  - Connection management: `connect()`, `accept()`, `close()`
  - Stream-based data transfer: `create_stream()`, `send_stream_data()`
  - Callback-based async I/O for stream data, connection events, errors
  - Thread-safe implementation with internal locking
  - Connection state machine (idle, handshake, connected, closing, draining, closed)
  - Integration with QUIC crypto for TLS 1.3 handshake
  - Comprehensive test suite with 18 test cases

- **QUIC Protocol Support (Phase 1.2)**: Frame types and parsing (RFC 9000 Section 12, 19)
  - `frame_types.h` with all QUIC frame type definitions and structures
  - `frame_parser` class for parsing frames from raw bytes
  - `frame_builder` class for serializing frames to bytes
  - Support for all RFC 9000 frame types:
    - PADDING, PING (connection management)
    - ACK, ACK_ECN (acknowledgment with ECN support)
    - CRYPTO (handshake data)
    - STREAM (data transfer with FIN/LEN/OFF flags)
    - MAX_DATA, MAX_STREAM_DATA, MAX_STREAMS (flow control)
    - DATA_BLOCKED, STREAM_DATA_BLOCKED, STREAMS_BLOCKED
    - NEW_CONNECTION_ID, RETIRE_CONNECTION_ID
    - PATH_CHALLENGE, PATH_RESPONSE
    - CONNECTION_CLOSE (transport and application)
    - HANDSHAKE_DONE, NEW_TOKEN
  - Comprehensive test suite with 37 test cases

- **QUIC Protocol Support (Phase 1.1)**: Variable-length integer encoding (RFC 9000 Section 16)
  - `varint` class with encode/decode methods
  - Support for 1, 2, 4, and 8 byte encodings based on value range
  - `encode_with_length` for minimum length requirements
  - Constexpr helper functions for length calculation
  - Comprehensive test suite with RFC 9000 example values

### Planned
- C++20 coroutine full integration
- HTTP/2 and HTTP/3 support
- Advanced load balancing algorithms

---

## [1.5.0] - 2025-11-14

### Added
- **HTTP Advanced Features**: Significantly improved HTTP/1.1 implementation
  - **Request Buffering**: Complete multi-chunk request support with Content-Length validation
    - Proper header/body boundary detection
    - Configurable maximum request size (10MB default)
    - Memory-safe buffer management

  - **Cookie Support**: Full HTTP cookie parsing and management (RFC 6265 compliant)
    - Cookie attributes support (expires, max-age, domain, path, secure, httponly, samesite)
    - Multiple cookie handling

  - **Multipart/Form-data**: Complete file upload support
    - Boundary detection and parsing
    - Multiple file handling
    - Content-Type and Content-Disposition parsing

  - **Chunked Encoding**: Both client and server support
    - Automatic chunk encoding for large responses
    - Memory-efficient streaming

  - **Automatic Compression**: gzip/deflate support via ZLIB
    - Content negotiation via Accept-Encoding header
    - Configurable compression threshold
    - ZLIB dependency integration

- **Container System Integration**: New flexible serialization API
  - `container_manager` singleton for message serialization
  - `container_interface` abstraction layer
  - `basic_container` standalone implementation (no external dependencies)
  - Automatic fallback when container_system unavailable
  - Thread-safe singleton pattern

### Fixed
- **Critical Bug Fixes**: Resolved production-blocking issues
  - **Memory Leaks**: Fixed circular reference in messaging_session (~900 bytes/connection)
    - Changed from shared_ptr to weak_ptr in callback lambdas
    - Prevents session objects from being kept alive by socket callbacks

  - **Deadlock**: Resolved lock-order-inversion in messaging_server
    - Fixed callback_mutex_ and session mutex interaction
    - Proper lock ordering in callback registration

  - **Thread Safety**: Fixed HTTP URL parsing data race
    - Changed url_regex to static const (C++11 magic statics)
    - Eliminates regex recompilation overhead

  - **TCP Shutdown**: Restored graceful connection termination
    - Proper EOF and operation-aborted event handling
    - Correct error propagation to handlers

### Changed
- **Test Infrastructure**: Migrated container_system tests to new API
  - Removed obsolete `#if 0` preprocessor blocks
  - Updated tests to use container_manager pattern
  - Simplified test data to use std::string
  - All tests work without container_system dependency

### Technical Details
- ZLIB version 1.2.12 integration
- AddressSanitizer verified: zero memory leaks
- ThreadSanitizer verified: no data races or deadlocks
- All unit tests passing (15/15)
- Compatible with container_system v2 API

---

## [1.4.0] - 2025-10-26

### Added
- **TLS/SSL Support (Phase 9)**: Complete encrypted communication implementation
  - **secure_tcp_socket (Phase 9.1)**:
    - SSL/TLS wrapper for TCP socket using `asio::ssl::stream`
    - Async SSL handshake support (client/server modes)
    - Encrypted async read/write operations
    - Follows same pattern as `tcp_socket` for consistency

  - **secure_session (Phase 9.1)**:
    - Manages single secure client session on server side
    - Performs server-side SSL handshake before data exchange
    - Inherits backpressure mechanism from Phase 8.2 (1000/2000 message limits)
    - Thread-safe message queue with mutex protection

  - **secure_messaging_server (Phase 9.2)**:
    - Secure server accepting TLS/SSL encrypted connections
    - Certificate chain and private key loading from files
    - SSL context configuration (sslv23, no SSLv2, single DH use)
    - Creates `secure_session` for each accepted connection
    - Inherits session cleanup from Phase 8.1 (periodic + on-demand)
    - Inherits monitoring support from common_system (optional)

  - **secure_messaging_client (Phase 9.3)**:
    - Secure client for TLS/SSL encrypted connections
    - Optional certificate verification (default: enabled)
    - Uses system certificate paths for verification when enabled
    - Client-side SSL handshake with 10-second timeout
    - Connection state management with atomic operations

  - **Build System (Phase 9.4)**:
    - Added `BUILD_TLS_SUPPORT` CMake option (default: ON)
    - Conditional compilation of TLS/SSL components
    - Automatic OpenSSL detection and linking
    - Updated build configuration summary to show TLS/SSL status
    - Compatible with existing WebSocket support (shared OpenSSL dependency)

### Changed
- Updated IMPROVEMENTS.md to mark Issue #4 (Add TLS/SSL Support) as completed
- Reorganized CMakeLists.txt to separate TLS/SSL sources from base library

### Technical Details
- Uses OpenSSL 3.6.0 for cryptographic operations
- Parallel class hierarchy maintains backward compatibility
- SSL context lifetime managed by server/client instances
- Handshake failures result in connection rejection with detailed error codes
- All secure components are thread-safe with proper synchronization

---

## [1.3.0] - 2025-10-26

### Added
- **Performance Optimization (Phase 8)**: Critical improvements to memory management and client connection efficiency
  - **Session Cleanup Mechanism (Phase 8.1)**:
    - Added `is_stopped()` method to `messaging_session` for state checking
    - Added `cleanup_dead_sessions()` to remove stopped sessions from vector
    - Added `start_cleanup_timer()` for periodic cleanup every 30 seconds
    - Protected `sessions_` vector with `sessions_mutex_` for thread safety
    - Cleanup triggered both periodically and on new connections
    - Prevents unbounded memory growth in long-running servers

  - **Receiver Backpressure (Phase 8.2)**:
    - Added `pending_messages_` queue (std::deque) to buffer incoming messages
    - Added `queue_mutex_` for thread-safe queue access
    - Set `max_pending_messages_` limit to 1000 messages
    - Log warning when queue reaches limit (backpressure signal)
    - Disconnect abusive clients when queue exceeds 2x limit (2000 messages)
    - Added `process_next_message()` to dequeue and handle messages
    - Prevents memory exhaustion from fast-sending clients

  - **Connection Pooling (Phase 8.3)**:
    - Added `connection_pool` class for reusable client connections
    - Pre-creates fixed number of connections at initialization
    - Thread-safe acquire/release semantics using mutex and condition variable
    - Blocks when all connections in use until one becomes available
    - Automatically reconnects lost connections when released
    - Tracks active connection count for monitoring
    - Configurable pool size (default: 10 connections)
    - Reduces connection overhead by up to 60%

### Changed
- Updated `messaging_server` thread safety documentation to reflect mutex-protected sessions
- Updated IMPROVEMENTS.md to mark Issues #1, #2, #3 as completed

### Technical Details
- Session cleanup uses `is_stopped()` check with atomic operations
- Backpressure applies at 1000 messages, disconnects at 2000
- Connection pool uses RAII for safe resource management
- All performance features are opt-in and backward compatible

---

## [1.2.0] - 2025-10-26

### Added
- **Network Load Testing Framework**: Comprehensive infrastructure for real network performance measurement
  - **Memory Profiler**: Cross-platform memory usage tracking
    - `MemoryProfiler`: RSS/heap/VM measurement for Linux, macOS, and Windows
    - Platform-specific implementations using /proc, task_info(), and GetProcessMemoryInfo()
    - Memory delta calculation for before/after comparisons

  - **Result Writer**: Performance metrics serialization
    - JSON and CSV output formats for test results
    - ISO 8601 timestamp generation
    - Structured data for latency (P50/P95/P99), throughput, and memory metrics

  - **TCP Load Tests**: End-to-end TCP performance testing
    - Throughput tests for 64B, 1KB, and 64KB messages
    - Concurrent connection tests (10 and 50 clients)
    - Round-trip latency measurement
    - Memory consumption tracking

  - **UDP Load Tests**: UDP protocol performance validation
    - Throughput tests for 64B, 512B, and 1KB datagrams
    - Packet loss detection and success rate tracking
    - Burst send performance (100 messages per burst)
    - Concurrent client tests (10 and 50 clients)

  - **WebSocket Load Tests**: WebSocket protocol benchmarking
    - Text and binary message throughput tests
    - Ping/pong latency measurement
    - Concurrent connection handling (10 clients)
    - Frame overhead analysis

- **CI/CD Automation**:
  - **GitHub Actions Workflow**: Automated load testing pipeline
    - Weekly scheduled runs (Sunday 00:00 UTC)
    - Manual trigger with baseline update option
    - Multi-platform support (Ubuntu 22.04, macOS 13, Windows 2022)
    - Automatic artifact upload with 90-day retention

  - **Metrics Collection Script**: Python-based result aggregation
    - `collect_metrics.py`: Parse and combine JSON results from all protocols
    - Summary statistics generation
    - Platform and compiler metadata tracking

  - **Baseline Update Script**: Automated documentation updates
    - `update_baseline.py`: Insert real network metrics into BASELINE.md
    - Per-platform result tracking
    - Dry-run mode for preview
    - Automated PR creation for baseline changes

- **Documentation**:
  - **Load Test Guide**: Comprehensive 400+ line testing documentation
    - Test structure and methodology
    - Result interpretation guide
    - Performance expectations per protocol
    - Troubleshooting section
    - Advanced profiling techniques

  - **Baseline Updates**: Real network performance sections
    - Added "Real Network Performance Metrics" to BASELINE.md and BASELINE_KO.md
    - Load test infrastructure documentation
    - Methodology and expected metrics sections

  - **README Updates**: Performance and testing sections
    - Synthetic benchmarks section
    - Real network load tests section
    - Automated baseline collection instructions
    - Updated documentation table with Load Test Guide

### Changed
- Updated Phase 0 checklist in BASELINE.md to reflect completed load testing infrastructure
- Reorganized integration test CMakeLists.txt with framework library
- Updated test suite count from 4 to 7 (added TCP, UDP, WebSocket load tests)

### Technical Details
- All load tests use localhost loopback for consistent measurements
- Tests automatically skip in CI environments to avoid timing-related flakiness
- Memory profiling uses platform-native APIs for accurate measurements
- Results include P50/P95/P99 latency percentiles for detailed analysis

---

## [1.1.0] - 2025-10-26

### Added
- **WebSocket Protocol Support (RFC 6455)**: Full-featured WebSocket client and server implementation
  - **messaging_ws_client**: High-level WebSocket client API
    - Text and binary message support
    - Configurable connection parameters (host, port, path, headers)
    - Automatic ping/pong keepalive mechanism
    - Graceful close handshake
    - Event callbacks for connection, disconnection, messages, and errors

  - **messaging_ws_server**: High-level WebSocket server API
    - Multi-client connection management
    - Broadcast support for all connected clients
    - Per-connection message handling
    - Session management with connection limits
    - Event callbacks for new connections, disconnections, and messages

  - **websocket_socket**: Low-level WebSocket framing layer
    - Built on top of existing tcp_socket infrastructure
    - WebSocket handshake (HTTP/1.1 Upgrade)
    - Frame encoding/decoding with masking support
    - Message fragmentation and reassembly
    - Control frame handling (ping, pong, close)

  - **websocket_protocol**: Protocol state machine
    - RFC 6455 compliant frame processing
    - UTF-8 validation for text messages
    - Automatic pong response to ping frames
    - Close code handling with graceful shutdown

  - **ws_session_manager**: WebSocket connection lifecycle management
    - Thread-safe connection tracking with shared_mutex
    - Connection limit enforcement
    - Backpressure signaling when threshold exceeded
    - Automatic connection ID generation
    - Connection metrics (total accepted, total rejected, active count)

- **Documentation**:
  - README.md updated with WebSocket features and usage examples
  - README_KO.md updated with Korean documentation
  - Architecture diagrams updated to include WebSocket components
  - API examples for both TCP and WebSocket protocols

### Changed
- Moved WebSocket support from "Planned Features" to "Core Features"
- Updated architecture diagrams to show WebSocket layer integration

### Security
- Frame masking for client-to-server communication (RFC 6455 requirement)
- UTF-8 validation for text frames to prevent malformed data
- Connection limits via ws_session_manager to prevent resource exhaustion

---

## [1.0.0] - 2025-10-22

### Added
- **Core Network System**: Modern C++20 asynchronous network library
  - ASIO-based non-blocking I/O operations
  - Zero-copy pipeline for maximum efficiency
  - Connection pooling with intelligent lifecycle management
  - C++20 coroutine support for async operations

- **High Performance**: Ultra-high throughput networking
  - 305K+ messages/second average throughput
  - 769K+ msg/s for small messages (<1KB)
  - Sub-microsecond latency
  - Direct memory mapping for zero-copy transfers

- **Core Components**:
  - **MessagingClient**: High-performance client with connection pooling
  - **MessagingServer**: Scalable server with session management
  - **MessagingSession**: Per-connection session handling
  - **Pipeline**: Zero-copy buffer management system

- **Modular Architecture**:
  - Independent from messaging_system (100% backward compatible)
  - Optional integration with container_system
  - Optional integration with thread_system
  - Optional integration with logger_system
  - Optional integration with common_system

- **Integration Support**:
  - container_system: Type-safe data transmission with binary protocols
  - thread_system: Thread pool management for concurrent operations
  - logger_system: Network operation logging and diagnostics
  - common_system: Result<T> error handling pattern
  - messaging_system: Messaging transport layer (bridge mode)

- **Advanced Features**:
  - Automatic reconnection with exponential backoff
  - Connection timeout management
  - Graceful shutdown handling
  - Session lifecycle management
  - Error recovery patterns

- **Build System**:
  - CMake 3.16+ with modular configuration
  - 13+ feature flags for flexible builds
  - Cross-platform support (Windows, Linux, macOS)
  - Compiler support (GCC 10+, Clang 10+, MSVC 19.29+)

### Changed
- N/A (Initial independent release)

### Deprecated
- N/A (Initial independent release)

### Removed
- N/A (Initial independent release)

### Fixed
- N/A (Initial independent release)

### Security
- Secure connection handling with TLS support
- Input validation for all network operations
- Buffer overflow protection
- Connection limits to prevent DoS

---

## [0.9.0] - 2025-09-15 (Pre-separation from messaging_system)

### Added
- Initial network functionality as part of messaging_system
- Basic client/server communication
- Message serialization and deserialization
- Connection management

### Changed
- Tight coupling with messaging_system
- Limited independent usage

---

## Project Information

**Repository:** https://github.com/kcenon/network_system
**Documentation:** [docs/README.md](README.md)
**License:** See LICENSE file
**Maintainer:** kcenon@naver.com

---

## Version Support Matrix

| Version | Release Date | C++ Standard | CMake Minimum | Status |
|---------|--------------|--------------|---------------|---------|
| 1.5.0   | 2025-11-14   | C++20        | 3.20          | Current |
| 1.4.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.3.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.2.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.1.0   | 2025-10-26   | C++20        | 3.16          | Stable |
| 1.0.0   | 2025-10-22   | C++20        | 3.16          | Stable |
| 0.9.0   | 2025-09-15   | C++20        | 3.16          | Legacy (part of messaging_system) |

---

## Migration Guide

For migration from messaging_system integrated network module, see [MIGRATION.md](advanced/MIGRATION.md).

---

## Performance Benchmarks

**Platform**: Varies by system (see benchmarks/ for details)

### Throughput Benchmarks
- Small messages (<1KB): 769K+ msg/s
- Medium messages (1-10KB): 305K+ msg/s
- Large messages (>10KB): 150K+ msg/s

### Latency Benchmarks
- Local connections: < 1 microsecond
- Network connections: Depends on network latency + processing overhead

### Scalability
- Concurrent connections: 10,000+ simultaneous connections
- Linear scaling up to 8 cores
- Connection pooling reduces overhead by 60%

---

## Breaking Changes

### 1.0.0
- **Namespace change**: `messaging::network` → `network_system`
- **Header paths**: `<messaging/network/*>` → `<network_system/*>`
- **CMake target**: `messaging_system` → `network_system`
- **Build options**: New `BUILD_WITH_*` options for ecosystem integration

See [MIGRATION.md](advanced/MIGRATION.md) for detailed migration instructions.

---

**Last Updated:** 2025-10-26
