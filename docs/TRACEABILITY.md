---
doc_id: "NET-QUAL-002"
doc_title: "Feature-Test-Module Traceability Matrix"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "QUAL"
---

# Traceability Matrix

> **SSOT**: This document is the single source of truth for **Network System Feature-Test-Module Traceability**.

## Feature -> Test -> Module Mapping

### TCP Protocol

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-001 | TCP Facade | tests/test_tcp_facade.cpp | include/kcenon/network/facade/, src/facade/ | Covered |
| NET-FEAT-002 | TCP Socket | tests/unit/tcp_socket_test.cpp | src/internal/tcp/ | Covered |
| NET-FEAT-003 | TCP Client/Server (Original) | tests/unit_tests.cpp, tests/unit_tests_original.cpp | src/core/ | Covered |

### UDP Protocol

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-004 | UDP Facade | tests/test_udp_facade.cpp | include/kcenon/network/facade/, src/facade/ | Covered |
| NET-FEAT-005 | UDP Socket | tests/unit/udp_socket_test.cpp | src/internal/udp/ | Covered |
| NET-FEAT-006 | UDP Composition | tests/unit/test_udp_composition.cpp | src/internal/udp/ | Covered |
| NET-FEAT-007 | UDP Basic Test | tests/udp_basic_test.cpp | src/internal/udp/ | Covered |

### TLS/SSL

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-008 | Secure Messaging Client | tests/unit/secure_messaging_client_test.cpp | src/internal/tcp/ | Covered |
| NET-FEAT-009 | Secure Messaging Server | tests/unit/secure_messaging_server_test.cpp | src/internal/tcp/ | Covered |
| NET-FEAT-010 | TLS Configuration | tests/unit/test_tls_config.cpp | include/kcenon/network/policy/ | Covered |
| NET-FEAT-011 | OpenSSL Compatibility | tests/unit/openssl_compat_test.cpp | src/internal/tcp/ | Covered |
| NET-FEAT-012 | DTLS Socket | tests/unit/test_dtls_socket.cpp | src/internal/udp/ | Covered |

### WebSocket Protocol

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-013 | WebSocket Facade | tests/test_websocket_facade.cpp | include/kcenon/network/facade/, src/facade/ | Covered |
| NET-FEAT-014 | WebSocket Frame | tests/unit/websocket_frame_test.cpp | src/internal/websocket/ | Covered |
| NET-FEAT-015 | WebSocket Handshake | tests/unit/websocket_handshake_test.cpp | src/internal/websocket/ | Covered |
| NET-FEAT-016 | WebSocket Protocol | tests/unit/websocket_protocol_test.cpp | src/internal/websocket/ | Covered |
| NET-FEAT-017 | WebSocket Server | tests/test_messaging_ws_server.cpp | src/internal/websocket/ | Covered |
| NET-FEAT-018 | WebSocket Client | tests/test_messaging_ws_client.cpp | src/internal/websocket/ | Covered |
| NET-FEAT-019 | WS Session Manager | tests/unit/ws_session_manager_test.cpp | src/internal/websocket/ | Covered |
| NET-FEAT-020 | WebSocket E2E | tests/integration/test_websocket_e2e.cpp | (cross-cutting) | Covered |

### HTTP/1.1 & HTTP/2

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-021 | HTTP Facade | tests/test_http_facade.cpp | include/kcenon/network/facade/, src/facade/ | Covered |
| NET-FEAT-022 | HTTP Client | tests/unit/http_client_test.cpp | src/http/ | Covered |
| NET-FEAT-023 | HTTP Server | tests/unit/http_server_test.cpp | src/http/ | Covered |
| NET-FEAT-024 | HTTP Parser | tests/unit/http_parser_test.cpp | src/http/ | Covered |
| NET-FEAT-025 | HTTP Types | tests/unit/http_types_test.cpp | include/kcenon/network/http/ | Covered |
| NET-FEAT-026 | HTTP/2 Frame | tests/test_http2_frame.cpp | src/protocols/http2/ | Covered |
| NET-FEAT-027 | HTTP/2 HPACK | tests/test_http2_hpack.cpp | src/protocols/http2/ | Covered |
| NET-FEAT-028 | HTTP/2 Client | tests/test_http2_client.cpp | src/protocols/http2/ | Covered |
| NET-FEAT-029 | HTTP/2 Server | tests/test_http2_server.cpp | src/protocols/http2/ | Covered |
| NET-FEAT-030 | HTTP/2 Request | tests/unit/http2_request_test.cpp | src/protocols/http2/ | Covered |
| NET-FEAT-031 | HTTP Config Error Handling | tests/test_network_config_http_error.cpp | include/kcenon/network/config/ | Covered |

### QUIC Protocol

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-032 | QUIC Connection | tests/test_quic_connection.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-033 | QUIC Crypto | tests/test_quic_crypto.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-034 | QUIC Frame | tests/test_quic_frame.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-035 | QUIC Packet | tests/test_quic_packet.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-036 | QUIC Stream | tests/test_quic_stream.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-037 | QUIC Socket | tests/test_quic_socket.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-038 | QUIC VarInt | tests/test_quic_varint.cpp, tests/unit/quic_varint_test.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-039 | QUIC Flow/Congestion/RTT | tests/test_quic_flow_congestion_rtt.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-040 | QUIC Loss Detection | tests/test_quic_loss_detection.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-041 | QUIC ECN | tests/test_quic_ecn.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-042 | QUIC PMTUD | tests/test_quic_pmtud.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-043 | QUIC Session Ticket | tests/test_quic_session_ticket.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-044 | QUIC Server | tests/test_messaging_quic_server.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-045 | QUIC Client | tests/test_messaging_quic_client.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-046 | QUIC E2E | tests/integration/test_quic_e2e.cpp | (cross-cutting) | Covered |
| NET-FEAT-047 | QUIC Connection ID | tests/unit/quic_connection_id_test.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-048 | QUIC Frame Types | tests/unit/quic_frame_types_test.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-049 | QUIC Keys | tests/unit/quic_keys_test.cpp | src/internal/protocols/quic/ | Covered |
| NET-FEAT-050 | QUIC Transport Params | tests/unit/quic_transport_params_test.cpp | src/internal/protocols/quic/ | Covered |

### gRPC

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-051 | gRPC Client/Server | tests/test_grpc_client_server.cpp | src/protocols/grpc/ | Covered |
| NET-FEAT-052 | gRPC Frame | tests/test_grpc_frame.cpp | src/protocols/grpc/ | Covered |
| NET-FEAT-053 | gRPC Integration | tests/test_grpc_integration.cpp | src/protocols/grpc/ | Covered |
| NET-FEAT-054 | gRPC Service Registry | tests/test_grpc_service_registry.cpp | src/protocols/grpc/ | Covered |
| NET-FEAT-055 | gRPC Official Wrapper | tests/test_grpc_official_wrapper.cpp | src/protocols/grpc/ | Covered |

### Facade & Unified API

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-056 | Facade Validation | tests/test_facade_validation.cpp | include/kcenon/network/facade/ | Covered |
| NET-FEAT-057 | Facade ID Generation | tests/unit/facade_id_generation_test.cpp | include/kcenon/network/facade/ | Covered |
| NET-FEAT-058 | Unified Messaging Client | tests/unit/test_unified_messaging_client.cpp | include/kcenon/network/unified/ | Covered |
| NET-FEAT-059 | Unified Messaging Server | tests/unit/test_unified_messaging_server.cpp | include/kcenon/network/unified/ | Covered |
| NET-FEAT-060 | Unified Interfaces | tests/unified/unified_interfaces_test.cpp | include/kcenon/network/unified/ | Covered |
| NET-FEAT-061 | Unified Session Manager | tests/unit/unified_session_manager_test.cpp | src/unified/ | Covered |
| NET-FEAT-062 | Protocol Policy | tests/unit/test_policy_protocol.cpp | include/kcenon/network/policy/ | Covered |

### Session Management

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-063 | Session Manager | tests/unit/session_manager_test.cpp, tests/unit/session_manager_base_test.cpp | src/session/ | Covered |
| NET-FEAT-064 | Session Info Traits | tests/unit/session_info_traits_test.cpp | include/kcenon/network/session/ | Covered |
| NET-FEAT-065 | Session Type Erasure | tests/unit/session_type_erasure_test.cpp | include/kcenon/network/session/ | Covered |
| NET-FEAT-066 | Session Manager Integration | tests/integration/session_manager_integration_test.cpp | src/session/ | Covered |

### Core Infrastructure

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-067 | Network Concepts | tests/unit/network_concepts_test.cpp, tests/unit/socket_concepts_test.cpp | include/kcenon/network/concepts/ | Covered |
| NET-FEAT-068 | Connection Pool | tests/unit/connection_pool_test.cpp | src/core/ | Covered |
| NET-FEAT-069 | Buffer Pool | tests/unit/buffer_pool_test.cpp | src/core/ | Covered |
| NET-FEAT-070 | Compression Pipeline | tests/unit/compression_pipeline_test.cpp | src/internal/ | Covered |
| NET-FEAT-071 | Network Context | tests/unit/network_context_test.cpp | include/kcenon/network/core/ | Covered |
| NET-FEAT-072 | Network System Core | tests/unit/network_system_test.cpp | src/core/ | Covered |
| NET-FEAT-073 | Startable Base | tests/unit/startable_base_test.cpp | include/kcenon/network/core/ | Covered |
| NET-FEAT-074 | Common Defs | tests/unit/common_defs_test.cpp | include/kcenon/network/ | Covered |
| NET-FEAT-075 | Callback Indices | tests/unit/callback_indices_test.cpp | include/kcenon/network/ | Covered |
| NET-FEAT-076 | Resilient Client | tests/unit/resilient_client_test.cpp | src/core/ | Covered |
| NET-FEAT-077 | Result Types | tests/unit/result_types_test.cpp | include/kcenon/network/ | Covered |
| NET-FEAT-078 | Input Validation | tests/unit/test_input_validation.cpp | (cross-cutting) | Covered |
| NET-FEAT-079 | Message Validator | tests/unit/message_validator_extended_test.cpp | src/core/ | Covered |
| NET-FEAT-080 | Composition Utilities | tests/unit/test_composition_utilities.cpp | (cross-cutting) | Covered |
| NET-FEAT-081 | Interfaces | tests/unit/test_interfaces.cpp | include/kcenon/network/interfaces/ | Covered |

### Metrics & Observability

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-082 | Network Metrics | tests/unit/test_network_metrics.cpp, tests/unit/network_metric_event_test.cpp | include/kcenon/network/metrics/, src/metrics/ | Covered |
| NET-FEAT-083 | Network Metrics Histogram | tests/unit/network_metrics_histogram_test.cpp | include/kcenon/network/metrics/ | Covered |
| NET-FEAT-084 | Socket Metrics | tests/unit/socket_metrics_test.cpp | include/kcenon/network/detail/metrics/ | Covered |
| NET-FEAT-085 | Sliding Histogram | tests/unit/sliding_histogram_test.cpp, tests/unit/test_histogram.cpp | include/kcenon/network/metrics/ | Covered |
| NET-FEAT-086 | Tracing | tests/unit/test_tracing.cpp, tests/integration/test_tracing_integration.cpp | include/kcenon/network/tracing/, src/tracing/ | Covered |
| NET-FEAT-087 | Metrics Integration | tests/integration/test_metrics_integration.cpp | (cross-cutting) | Covered |
| NET-FEAT-088 | Observability Bridge | tests/unit/test_observability_bridge.cpp | src/internal/ | Covered |

### Ecosystem Bridges

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-089 | Thread Pool Adapters | tests/unit/test_thread_pool_adapters.cpp, tests/unit/test_thread_pool_bridge.cpp, tests/unit/test_thread_system_adapter.cpp | src/internal/adapters/ | Covered |
| NET-FEAT-090 | Network System Bridge | tests/unit/test_network_system_bridge.cpp | src/internal/ | Covered |
| NET-FEAT-091 | Messaging Bridge Interface | tests/unit/test_messaging_bridge_interface.cpp | include/kcenon/network/integration/ | Covered |
| NET-FEAT-092 | Memory Profiler Session Timeout | tests/test_memory_profiler_session_timeout.cpp | src/internal/ | Covered |

### Integration & Quality

| Feature ID | Feature | Test File(s) | Module/Directory | Status |
|-----------|---------|-------------|-----------------|--------|
| NET-FEAT-093 | E2E Integration | tests/integration/test_e2e.cpp, tests/integration/test_integration.cpp | (cross-cutting) | Covered |
| NET-FEAT-094 | Thread Safety | tests/thread_safety_tests.cpp | (cross-cutting) | Covered |
| NET-FEAT-095 | Failure/Boundary Testing | tests/failure/network_failure_test.cpp, tests/failure/boundary_test.cpp | (cross-cutting) | Covered |
| NET-FEAT-096 | Stress Testing | tests/stress/stress_test.cpp | (cross-cutting) | Covered |
| NET-FEAT-097 | Error Handling Integration | integration_tests/failures/error_handling_test.cpp | (cross-cutting) | Covered |

## Coverage Summary

| Category | Total Features | Covered | Partial | Uncovered |
|----------|---------------|---------|---------|-----------|
| TCP Protocol | 3 | 3 | 0 | 0 |
| UDP Protocol | 4 | 4 | 0 | 0 |
| TLS/SSL | 5 | 5 | 0 | 0 |
| WebSocket Protocol | 8 | 8 | 0 | 0 |
| HTTP/1.1 & HTTP/2 | 11 | 11 | 0 | 0 |
| QUIC Protocol | 19 | 19 | 0 | 0 |
| gRPC | 5 | 5 | 0 | 0 |
| Facade & Unified API | 7 | 7 | 0 | 0 |
| Session Management | 4 | 4 | 0 | 0 |
| Core Infrastructure | 15 | 15 | 0 | 0 |
| Metrics & Observability | 7 | 7 | 0 | 0 |
| Ecosystem Bridges | 4 | 4 | 0 | 0 |
| Integration & Quality | 5 | 5 | 0 | 0 |
| **Total** | **97** | **97** | **0** | **0** |

## See Also

- [FEATURES.md](FEATURES.md) -- Detailed feature documentation
- [README.md](README.md) -- SSOT Documentation Registry
