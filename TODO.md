# Network System - Pending Features

> **Language:** **English** | [한국어](TODO_KO.md)

## Overview

This document tracks features that are planned but not yet implemented in the network system. Features are organized by priority and estimated implementation effort.

**Last Updated:** 2025-01-26
**Version:** 1.4.0

---

## High Priority Features (P2)

### 1. HTTP/2 Client Support

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

### 2. gRPC Integration

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

## Low Priority / Future Features (P4)

### 3. QUIC Protocol Support

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

### ~~4. DTLS Support for UDP~~ ✅ Completed

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

### 5. Metrics and Monitoring Dashboard

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
| P2       | 2     | 0         | 2         | 17-24 days   |
| P4       | 3     | 1         | 2         | 25-34 days   |
| **Total** | **5** | **1**    | **4**     | **42-58 days** |

### By Target Version

| Version | Features | Status | Effort |
|---------|----------|--------|--------|
| v1.8.0  | DTLS Support | ✅ Completed | 1 day |
| v2.0.0  | HTTP/2, gRPC, Metrics Dashboard | Pending | 27-38 days |
| v2.1.0+ | QUIC Protocol | Pending | 15-20 days |

### Implementation Roadmap

**Phase 1 (v1.8.0):** Security Enhancements ✅ Completed
- ✅ DTLS support for secure UDP

**Phase 2 (v2.0.0):** Modern Protocols
- HTTP/2 support
- gRPC integration
- Monitoring dashboard

**Phase 3 (v2.1.0+):** Advanced Features
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
