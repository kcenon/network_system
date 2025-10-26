# Network System - Pending Features

> **Language:** **English** | [한국어](TODO_KO.md)

## Overview

This document tracks features that are planned but not yet implemented in the network system. Features are organized by priority and estimated implementation effort.

**Last Updated:** 2025-01-26
**Version:** 1.4.0

---

## High Priority Features (P2)

### ~~1. Reconnection Logic for Client~~ ✅ COMPLETED

**Status:** Implemented in v1.5.0
**Priority:** P2
**Actual Effort:** 2 days
**Completed:** 2025-01-26

**Description:**
Add automatic reconnection logic with exponential backoff for `messaging_client` and `secure_messaging_client` to handle network interruptions gracefully.

**Proposed Implementation:**
```cpp
class resilient_client {
public:
    resilient_client(const std::string& host, unsigned short port,
                    size_t max_retries = 3,
                    std::chrono::milliseconds initial_backoff = std::chrono::seconds(1));

    auto send_with_retry(std::vector<uint8_t>&& data) -> VoidResult;
    auto reconnect() -> VoidResult;
    auto set_reconnect_callback(std::function<void(size_t attempt)> callback) -> void;

private:
    std::unique_ptr<messaging_client> client_;
    size_t max_retries_;
    std::chrono::milliseconds initial_backoff_;
    std::function<void(size_t)> reconnect_callback_;
};
```

**Benefits:**
- Automatic recovery from network failures
- Exponential backoff prevents connection storms
- Configurable retry behavior
- Callback support for reconnection events

**Related Files:**
- `include/network_system/core/messaging_client.h`
- `include/network_system/core/secure_messaging_client.h`

---

### 2. HTTP/2 Client Support

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

### 3. gRPC Integration

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

### ~~4. Zero-Copy Pipeline~~ ✅ COMPLETED

**Status:** Implemented in v1.6.0
**Priority:** P3
**Actual Effort:** 3 days
**Completed:** 2025-01-26

**Description:**
Optimize the data pipeline to avoid unnecessary memory copies during send/receive operations.

**Implemented Features:**
- Buffer pool for memory reuse (reduces allocations)
- Move semantics throughout pipeline (eliminates copies)
- In-place transformations where possible

**Proposed Improvements:**
```cpp
class zero_copy_pipeline {
public:
    // Use buffer references instead of copying
    auto transform(std::span<const uint8_t> input) -> std::vector<asio::const_buffer>;

    // Support for vectored I/O
    auto async_send_vectored(
        std::vector<asio::const_buffer> buffers,
        std::function<void(std::error_code, size_t)> handler) -> void;

    // Memory pool for buffer reuse
    auto get_buffer(size_t size) -> std::shared_ptr<std::vector<uint8_t>>;
    auto release_buffer(std::shared_ptr<std::vector<uint8_t>> buffer) -> void;
};
```

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

### ~~5. Connection Health Monitoring~~ ✅ COMPLETED

**Status:** Implemented in v1.5.0
**Priority:** P3
**Actual Effort:** 3 days
**Completed:** 2025-01-26

**Description:**
Add connection health monitoring with heartbeat/keepalive mechanism to detect dead connections early.

**Key Features:**
- Configurable heartbeat interval
- Automatic dead connection detection
- Connection quality metrics (latency, packet loss)
- Health status callbacks

**Proposed API:**
```cpp
struct connection_health {
    bool is_alive;
    std::chrono::milliseconds last_response_time;
    size_t missed_heartbeats;
    double packet_loss_rate;
};

class health_monitor {
public:
    health_monitor(std::chrono::seconds heartbeat_interval = std::chrono::seconds(30));

    auto start_monitoring(std::shared_ptr<messaging_client> client) -> void;
    auto stop_monitoring() -> void;

    auto get_health() const -> connection_health;
    auto set_health_callback(std::function<void(connection_health)> callback) -> void;
};
```

**Benefits:**
- Early detection of dead connections
- Reduced resource waste on dead connections
- Better visibility into connection quality

**Related Files:**
- New: `include/network_system/utils/health_monitor.h`
- New: `src/utils/health_monitor.cpp`

---

### ~~6. Message Compression Support~~ ✅ COMPLETED

**Status:** Implemented in v1.6.0
**Priority:** P3
**Actual Effort:** 2 days
**Completed:** 2025-01-26

**Description:**
Implement actual compression in the pipeline using LZ4 algorithm.

**Implemented Features:**
- LZ4 fast compression algorithm
- Configurable compression threshold (default: 256 bytes)
- Automatic fallback for small/incompressible data
- Safe decompression with size validation

**Proposed Implementation:**
```cpp
enum class compression_algorithm {
    none,
    zlib,
    lz4,
    zstd
};

class compression_pipeline {
public:
    compression_pipeline(compression_algorithm algo = compression_algorithm::lz4,
                        int compression_level = 3);

    auto compress(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>;
    auto decompress(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>;

    auto set_compression_threshold(size_t bytes) -> void;  // Don't compress small messages
};
```

**Dependencies:**
- zlib (widely available)
- lz4 (optional, for speed)
- zstd (optional, for better compression)

**Benefits:**
- Reduced bandwidth usage
- Lower network costs
- Faster transmission for compressible data

**Related Files:**
- `include/network_system/internal/pipeline.h`
- `src/internal/pipeline.cpp`

---

### 7. UDP Reliability Layer

**Status:** Not Implemented
**Priority:** P3
**Estimated Effort:** 8-10 days
**Target Version:** v1.7.0

**Description:**
Add optional reliability layer on top of UDP for applications needing both speed and reliability.

**Key Features:**
- Selective acknowledgment (SACK)
- Packet retransmission
- In-order delivery option
- Congestion control
- Configurable reliability level

**Proposed API:**
```cpp
enum class reliability_mode {
    unreliable,          // Pure UDP
    reliable_ordered,    // Like TCP but over UDP
    reliable_unordered,  // Reliability without ordering
    sequenced           // Drop old packets, no retransmit
};

class reliable_udp_client {
public:
    reliable_udp_client(const std::string& client_id,
                       reliability_mode mode = reliability_mode::reliable_ordered);

    auto start_client(const std::string& host, unsigned short port) -> VoidResult;
    auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

    auto set_congestion_window(size_t packets) -> void;
    auto set_max_retries(size_t retries) -> void;
};
```

**Benefits:**
- Best of both worlds (UDP speed + TCP reliability)
- Configurable trade-offs
- Better for real-time applications than TCP

**Related Files:**
- New: `include/network_system/core/reliable_udp_client.h`
- New: `src/core/reliable_udp_client.cpp`

---

## Low Priority / Future Features (P4)

### 8. QUIC Protocol Support

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

### 9. DTLS Support for UDP

**Status:** Not Planned
**Priority:** P4
**Estimated Effort:** 7-9 days
**Target Version:** v1.8.0

**Description:**
Add DTLS (Datagram TLS) support for secure UDP communication.

**Benefits:**
- Encrypted UDP communication
- Security for real-time applications
- Compatible with existing TLS infrastructure

---

### 10. Metrics and Monitoring Dashboard

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
| P2       | 3     | 1         | 2         | 19-27 days   |
| P3       | 5     | 4         | 1         | 28-36 days   |
| P4       | 3     | 0         | 3         | 32-43 days   |
| **Total** | **11** | **5**   | **6**     | **79-106 days** |

### By Target Version

| Version | Features | Status | Effort |
|---------|----------|--------|--------|
| v1.5.0  | Reconnection Logic, Health Monitoring | ✅ Completed | 5 days |
| v1.6.0  | Zero-Copy Pipeline, Compression | ✅ Completed | 5 days |
| v1.7.0  | UDP Reliability Layer | Pending | 8-10 days |
| v2.0.0  | HTTP/2, gRPC, Metrics Dashboard | Pending | 27-38 days |
| v2.1.0+ | QUIC Protocol | Pending | 15-20 days |

### Implementation Roadmap

**Phase 1 (v1.5.0):** Stability & Reliability ✅ COMPLETED
- ✅ Reconnection logic
- ✅ Health monitoring

**Phase 2 (v1.6.0):** Performance Optimization ✅ COMPLETED
- ✅ Zero-copy pipeline (buffer pool + move semantics)
- ✅ Message compression (LZ4)

**Phase 3 (v1.7.0):** Protocol Enhancements
- UDP reliability layer
- DTLS support

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
