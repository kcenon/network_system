# Network System - Pending Features

> **Language:** **English** | [한국어](TODO_KO.md)

## Overview

This document tracks features that are planned but not yet implemented in the network system. Features are organized by priority and estimated implementation effort.

**Last Updated:** 2025-11-26
**Version:** 1.8.0

---

## High Priority Features (P2)

### ~~1. Client Reconnection Logic~~ ✅ Completed

**Status:** Implemented in v1.5.0
**Priority:** P2
**Actual Effort:** 2 days
**Completed:** 2025-01-26

**Description:**
Add automatic reconnection logic with exponential backoff for `messaging_client` and `secure_messaging_client` to handle network interruptions gracefully.

**Implemented Features:**
- Exponential backoff for reconnection attempts
- Configurable retry behavior
- Callback support for reconnection events
- Automatic recovery from network failures

**Benefits:**
- Automatic recovery from network failures
- Exponential backoff prevents connection storms
- Configurable retry behavior
- Callback support for reconnection events

**Related Files:**
- `include/network_system/core/messaging_client.h`
- `include/network_system/core/secure_messaging_client.h`

---

### 3. HTTP/2 Client Support

**Status:** Not Implemented
**Priority:** P2
**Estimated Effort:** 10-14 days
**Target Version:** v2.0.0

**Description:**
Implement HTTP/2 protocol support for modern web service communication, including multiplexing, server push, and header compression.

**Key Features:**
- Full HTTP/2 protocol implementation (RFC 7540)
- Stream multiplexing over single connection
- Server push support
- HPACK header compression
- Connection upgrade from HTTP/1.1
- TLS/SSL integration (ALPN negotiation)

**Proposed API:**
```cpp
class http2_client {
public:
    http2_client(const std::string& host, unsigned short port, bool use_tls = true);

    auto connect() -> VoidResult;
    auto disconnect() -> VoidResult;

    auto get(const std::string& path,
            const http_headers& headers = {}) -> Result<http_response>;

    auto post(const std::string& path,
             const std::vector<uint8_t>& body,
             const http_headers& headers = {}) -> Result<http_response>;

    auto send_request(const http_request& request) -> Result<http_response>;

    // Streaming support
    auto create_stream() -> Result<std::shared_ptr<http2_stream>>;
};
```

**Dependencies:**
- nghttp2 library (optional, can implement from scratch)
- TLS/SSL support (already implemented)

**Related Files:**
- New: `include/network_system/protocols/http2_client.h`
- New: `src/protocols/http2_client.cpp`

---

### 4. gRPC Integration

**Status:** Not Implemented
**Priority:** P2
**Estimated Effort:** 7-10 days
**Target Version:** v2.0.0

**Description:**
Add gRPC protocol support for high-performance RPC communication using Protocol Buffers.

**Key Features:**
- gRPC over HTTP/2 transport
- Unary RPC calls
- Server streaming
- Client streaming
- Bidirectional streaming
- Deadline/timeout support
- Metadata handling

**Proposed API:**
```cpp
class grpc_client {
public:
    grpc_client(const std::string& host, unsigned short port,
               const grpc_channel_config& config = {});

    template<typename Request, typename Response>
    auto call(const std::string& method,
             const Request& request) -> Result<Response>;

    template<typename Request, typename Response>
    auto call_async(const std::string& method,
                   const Request& request,
                   std::function<void(Result<Response>)> callback) -> void;
};
```

**Dependencies:**
- HTTP/2 support (needs to be implemented first)
- Protocol Buffers library (protobuf)
- gRPC library (optional, can use HTTP/2 + protobuf directly)

**Related Files:**
- New: `include/network_system/protocols/grpc_client.h`
- New: `src/protocols/grpc_client.cpp`

---

## Medium Priority Features (P3)

### ~~5. Zero-Copy Pipeline~~ ✅ Completed

**Status:** Implemented in v1.6.0
**Priority:** P3
**Actual Effort:** 3 days
**Completed:** 2025-01-26

**Description:**
Optimize data pipeline to avoid unnecessary memory copies during send/receive operations.

**Implemented Features:**
- Buffer pool for memory reuse (reduced allocations)
- Move semantics throughout pipeline (eliminated copies)
- In-place transformations where possible

**Benefits:**
- Reduced memory allocations
- Lower CPU usage
- Higher throughput for large messages
- Better cache locality

**Related Files:**
- `include/network_system/internal/pipeline.h`
- `src/internal/pipeline.cpp`
- `include/network_system/internal/tcp_socket.h`
- `include/network_system/internal/secure_tcp_socket.h`

---

### ~~6. Connection Status Monitoring~~ ✅ Completed

**Status:** Implemented in v1.5.0
**Priority:** P3
**Actual Effort:** 3 days
**Completed:** 2025-01-26

**Description:**
Add connection health monitoring with heartbeat/keepalive mechanisms to detect dead connections early.

**Implemented Features:**
- Configurable heartbeat interval
- Automatic dead connection detection
- Connection quality metrics (latency, packet loss)
- Health callbacks

**Benefits:**
- Early detection of dead connections
- Reduced resource waste on dead connections
- Better visibility into connection quality

**Related Files:**
- New: `include/network_system/utils/health_monitor.h`
- New: `src/utils/health_monitor.cpp`

---

### ~~7. Message Compression Support~~ ✅ Completed

**Status:** Implemented in v1.6.0
**Priority:** P3
**Actual Effort:** 2 days
**Completed:** 2025-01-26

**Description:**
Implement real compression using LZ4 algorithm in the pipeline.

**Implemented Features:**
- LZ4 high-speed compression algorithm
- Configurable compression threshold (default: 256 bytes)
- Automatic fallback for small/incompressible data
- Safe decompression with size validation

**Benefits:**
- Reduced bandwidth usage
- Lower network costs
- Faster transfers for compressible data

**Related Files:**
- `include/network_system/internal/pipeline.h`
- `src/internal/pipeline.cpp`

---

### ~~8. UDP Reliability Layer~~ ✅ Completed

**Status:** Implemented in v1.7.0
**Priority:** P3
**Actual Effort:** 3 days
**Completed:** 2025-01-27

**Description:**
Add an optional reliability layer on top of UDP for applications that need both speed and reliability.

**Implemented Features:**
- Selective Acknowledgments (SACK)
- Packet retransmission
- Ordered delivery option
- Congestion control
- Configurable reliability levels

**Benefits:**
- Best of both worlds (UDP speed + TCP reliability)
- Configurable trade-offs
- Better suited for real-time applications than TCP

**Related Files:**
- New: `include/network_system/core/reliable_udp_client.h`
- New: `src/core/reliable_udp_client.cpp`

---

## Low Priority / Future Features (P4)

### 9. QUIC Protocol Support

**Status:** Not Planned
**Priority:** P4
**Estimated Effort:** 15-20 days
**Target Version:** v2.1.0+

**Description:**
Add QUIC (Quick UDP Internet Connections) protocol support for modern low-latency networking.

**Key Features:**
- Built on UDP
- 0-RTT connection establishment
- Multiplexed streams
- Built-in encryption (TLS 1.3)
- Connection migration

**Dependencies:**
- Requires significant engineering effort
- Complex protocol implementation
- May require third-party library (e.g., quiche, ngtcp2)

---

### ~~10. DTLS Support for UDP~~ ✅ Completed

**Status:** Implemented in v1.8.0
**Priority:** P4
**Actual Effort:** 1 day
**Completed:** 2025-11-26

**Description:**
Add DTLS (Datagram TLS) support for secure UDP communication.

**Implemented Features:**
- `dtls_socket` - Low-level DTLS socket wrapper using OpenSSL
- `secure_messaging_udp_client` - DTLS client for secure UDP communication
- `secure_messaging_udp_server` - DTLS server with session management
- Full async I/O support with ASIO integration
- Thread-safe implementation

**Benefits:**
- Encrypted UDP communication
- Security for real-time applications
- Compatible with existing TLS infrastructure

**Related Files:**
- `include/kcenon/network/internal/dtls_socket.h`
- `src/internal/dtls_socket.cpp`
- `include/kcenon/network/core/secure_messaging_udp_client.h`
- `src/core/secure_messaging_udp_client.cpp`
- `include/kcenon/network/core/secure_messaging_udp_server.h`
- `src/core/secure_messaging_udp_server.cpp`

---

### 11. Metrics and Monitoring Dashboard

**Status:** Not Planned
**Priority:** P4
**Estimated Effort:** 10-14 days
**Target Version:** v2.0.0

**Description:**
Built-in web dashboard for monitoring network system metrics in real-time.

**Key Features:**
- Real-time connection statistics
- Throughput graphs
- Error rate monitoring
- WebSocket for live updates
- REST API for metrics export

---

## Summary

### By Priority

| Priority | Count | Completed | Remaining | Total Effort |
|----------|-------|-----------|-----------|--------------|
| P2       | 3     | 1         | 2         | 17-24 days   |
| P3       | 4     | 4         | 0         | 0 days       |
| P4       | 3     | 1         | 2         | 25-34 days   |
| **Total** | **10** | **6**    | **4**     | **42-58 days** |

### By Target Version

| Version | Features | Status | Effort |
|---------|----------|--------|--------|
| v1.5.0  | Reconnection Logic, Connection Monitoring | ✅ Completed | 5 days |
| v1.6.0  | Zero-Copy Pipeline, Compression | ✅ Completed | 5 days |
| v1.7.0  | UDP Reliability Layer | ✅ Completed | 3 days |
| v1.8.0  | DTLS Support | ✅ Completed | 1 day |
| v2.0.0  | HTTP/2, gRPC, Metrics Dashboard | Pending | 27-38 days |
| v2.1.0+ | QUIC Protocol | Pending | 15-20 days |

### Implementation Roadmap

**Phase 1 (v1.5.0):** Stability & Reliability ✅ Completed
- ✅ Client reconnection logic
- ✅ Connection status monitoring

**Phase 2 (v1.6.0):** Performance Optimization ✅ Completed
- ✅ Zero-copy pipeline (buffer pool + move semantics)
- ✅ Message compression (LZ4)

**Phase 3 (v1.7.0):** Protocol Enhancements ✅ Completed
- ✅ UDP reliability layer

**Phase 3.5 (v1.8.0):** Security Enhancements ✅ Completed
- ✅ DTLS support for secure UDP

**Phase 4 (v2.0.0):** Modern Protocols
- HTTP/2 support
- gRPC integration
- Monitoring dashboard

**Phase 5 (v2.1.0+):** Advanced Features
- QUIC protocol
- Additional protocols as needed

---

## Contributing

If you'd like to implement any of these features:

1. Check the [IMPROVEMENTS.md](IMPROVEMENTS.md) for completed features
2. Create an issue to discuss the implementation approach
3. Follow the existing code style and patterns
4. Add comprehensive tests
5. Update documentation

## Notes

- All effort estimates assume a single experienced developer
- Estimates include implementation, testing, and documentation
- Dependencies may affect the order of implementation
- Feature priorities may change based on user feedback
