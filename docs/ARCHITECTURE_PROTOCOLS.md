---
doc_id: "NET-ARCH-002b"
doc_title: "Network System Architecture - Protocols"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "ARCH"
---

# Network System Architecture - Protocols

> **SSOT**: This document is part of the Network System Architecture. See also [Architecture - Overview](ARCHITECTURE_OVERVIEW.md) and the [Architecture Index](ARCHITECTURE.md).

> **Language:** **English** | [한국어](ARCHITECTURE.kr.md)

Protocol-specific architecture details and implementation notes for the Network System.

## Table of Contents

- [Protocol Components](#protocol-components)
- [Threading Model](#threading-model)
- [Memory Management](#memory-management)
- [Performance Characteristics](#performance-characteristics)
- [Future Enhancements](#future-enhancements)

---

## Protocol Components

### MessagingServer

**Purpose**: High-performance TCP server supporting multiple clients

```cpp
class MessagingServer {
public:
    // Constructor
    MessagingServer(const std::string& host, uint16_t port,
                   std::shared_ptr<thread_pool_interface> pool = nullptr);

    // Lifecycle
    void start();
    void stop();

    // Callbacks
    void on_client_connected(std::function<void(session_id)> callback);
    void on_client_disconnected(std::function<void(session_id)> callback);
    void on_message_received(std::function<void(session_id, const std::vector<uint8_t>&)> callback);

    // Send operations
    void send(session_id id, const std::vector<uint8_t>& data);
    void broadcast(const std::vector<uint8_t>& data);

private:
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<thread_pool_interface> thread_pool_;
};
```

**Features**:
- Async accept loop for new connections
- Session management with unique IDs
- Broadcast support for multi-client scenarios
- Graceful shutdown with connection draining

**Performance**:
- Handles 10,000+ concurrent connections
- 305K+ messages/second throughput
- Sub-microsecond latency per operation

### MessagingClient

**Purpose**: Robust TCP client with automatic reconnection

```cpp
class MessagingClient {
public:
    // Constructor
    MessagingClient(const std::string& host, uint16_t port);

    // Connection management
    Result<void> connect();
    void disconnect();
    bool is_connected() const;

    // Callbacks
    void on_connected(std::function<void()> callback);
    void on_disconnected(std::function<void()> callback);
    void on_message_received(std::function<void(const std::vector<uint8_t>&)> callback);

    // Send operations
    Result<void> send(const std::vector<uint8_t>& data);

    // Reconnection
    void enable_auto_reconnect(bool enable, std::chrono::milliseconds retry_interval);

private:
    asio::io_context io_context_;
    std::shared_ptr<MessagingSession> session_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> auto_reconnect_{false};
};
```

**Features**:
- Automatic reconnection with configurable intervals
- Connection health monitoring
- Event-driven callback system
- Thread-safe send operations

### MessagingSession

**Purpose**: Represents an active network connection

```cpp
class MessagingSession : public std::enable_shared_from_this<MessagingSession> {
public:
    // Async operations
    void start_read();
    void async_send(const std::vector<uint8_t>& data);

    // State management
    bool is_connected() const;
    session_id get_id() const;

    // Callbacks
    void set_on_message(std::function<void(const std::vector<uint8_t>&)> callback);
    void set_on_error(std::function<void(const std::error_code&)> callback);

    // Socket access
    asio::ip::tcp::socket& socket();

private:
    asio::ip::tcp::socket socket_;
    std::unique_ptr<Pipeline> pipeline_;
    session_id id_;
    std::atomic<bool> connected_{true};
    std::vector<uint8_t> read_buffer_;
};
```

**Features**:
- Zero-copy pipeline for efficient I/O
- Async read loop with backpressure handling
- Graceful connection close
- Error propagation via callbacks

### Pipeline (Zero-Copy Buffer)

**Purpose**: Efficient message buffering with zero-copy operations

```cpp
class Pipeline {
public:
    // Write operations (zero-copy)
    void write(const void* data, size_t size);
    void write(std::vector<uint8_t>&& data);  // Move semantics

    // Read operations (zero-copy)
    std::span<const uint8_t> peek(size_t size);
    void consume(size_t size);

    // State
    size_t size() const;
    bool empty() const;
    void clear();

private:
    std::deque<std::vector<uint8_t>> buffers_;  // Segmented buffer
    size_t total_size_{0};
};
```

**Features**:
- Segmented buffer design for efficient memory use
- Move semantics to avoid copies
- Peek/consume pattern for efficient parsing
- Automatic memory management


---

## Threading Model

> **Cross-reference**:
> [Benchmarks §Concurrency](./BENCHMARKS.md) — Thread scaling and concurrency benchmarks
> [Production Quality](./PRODUCTION_QUALITY.md) — Thread sanitizer (TSAN) verification results

> **Ecosystem reference**:
> [thread_system Architecture](https://github.com/kcenon/thread_system/blob/main/docs/ARCHITECTURE.md) — Underlying thread pool implementation and adaptive queue

### ASIO Event Loop

Network_system uses ASIO's proactor pattern:

```
┌─────────────────────────────────────────┐
│         ASIO I/O Context                │
│                                         │
│  ┌───────────────────────────────┐     │
│  │   Async Operations Queue      │     │
│  │   - async_read()              │     │
│  │   - async_write()             │     │
│  │   - async_accept()            │     │
│  └───────────┬───────────────────┘     │
│              │                          │
│  ┌───────────▼───────────────────┐     │
│  │   Event Loop (io_context.run)│     │
│  │   - Polls for I/O completion  │     │
│  │   - Invokes completion        │     │
│  │     handlers                  │     │
│  └───────────┬───────────────────┘     │
└──────────────┼──────────────────────────┘
               │
    ┌──────────┼──────────┐
    │          │          │
┌───▼───┐ ┌───▼───┐ ┌───▼───┐
│Worker │ │Worker │ │Worker │
│Thread │ │Thread │ │Thread │
│   1   │ │   2   │ │   N   │
└───────┘ └───────┘ └───────┘
```

### Thread Pool Integration

```cpp
// Option 1: Use built-in thread pool
auto server = MessagingServer("0.0.0.0", 8080);
// Uses default thread pool (hardware_concurrency threads)

// Option 2: Use custom thread pool
auto custom_pool = std::make_shared<kcenon::thread::thread_pool>(16);
auto adapter = std::make_shared<thread_system_adapter>(custom_pool);
auto server = MessagingServer("0.0.0.0", 8080, adapter);

// Option 3: Single-threaded mode
auto server = MessagingServer("0.0.0.0", 8080, nullptr);
// All operations on single ASIO thread
```

---

## Memory Management

### Zero-Copy Pipeline

```
Traditional Approach (Multiple Copies):
Application → Copy 1 → Send Buffer → Copy 2 → Network Stack

Zero-Copy Approach:
Application → Direct Write → Network Stack
                ↑
          Shared Buffer (no copy)
```

### Buffer Management Strategy

1. **Receive Path**:
   ```cpp
   async_read() → Read Buffer → Pipeline (move) → Application
   ```
   - Single allocation per message
   - Move semantics avoid copies
   - Automatic buffer reuse

2. **Send Path**:
   ```cpp
   Application → Pipeline (move) → ASIO Buffer → Network
   ```
   - Zero-copy through std::span
   - Direct memory access
   - Efficient scatter-gather I/O

### Memory Profiling

Built-in memory profiler tracks:
- Total bytes allocated
- Peak memory usage
- Active buffers count
- Memory per session

```cpp
auto profiler = network_system::get_memory_profiler();
auto stats = profiler->get_statistics();

std::cout << "Peak memory: " << stats.peak_bytes / 1024 / 1024 << " MB\n";
std::cout << "Active buffers: " << stats.active_buffers << "\n";
```

---

## Performance Characteristics

### Throughput Benchmarks

| Configuration | Throughput | Latency (P50) | Connections |
|--------------|------------|---------------|-------------|
| Small messages (64B) | 769K msg/s | <1 μs | 1 |
| Medium messages (1KB) | 305K msg/s | <1 μs | 1 |
| Large messages (64KB) | 45K msg/s | 2.1 μs | 1 |
| Multi-client (100) | 285K msg/s | 1.8 μs | 100 |
| High concurrency (1000) | 250K msg/s | 3.2 μs | 1000 |

*Measured on Apple M1, macOS 26.1, 8 cores*

### Optimization Techniques

1. **Zero-Copy I/O**: Eliminates buffer copies
2. **Connection Pooling**: Reduces connection overhead
3. **Async Operations**: Non-blocking I/O
4. **Lock-Free Structures**: Minimizes contention
5. **SIMD Operations**: Fast buffer operations (planned)

### Scalability

```
Linear scalability up to ~5000 connections:
1 connection    →  769K msg/s  (100% efficiency)
100 connections →  285K msg/s  (37% per-conn)
1000 connections → 250K msg/s  (32% per-conn)
5000 connections → 200K msg/s  (26% per-conn)

Bottleneck beyond 5000: OS socket limits, scheduler overhead
```

---

## Future Enhancements

### Planned Features

| Feature | Status | Priority | ETA |
|---------|--------|----------|-----|
| TLS/SSL Support | ✅ Achieved | P1 | - |
| Connection Pooling | 🔄 In Progress | P1 | Q4 2025 |
| HTTP/2 Support | 📋 Planned | P2 | Q2 2026 |
| WebSocket Support | 📋 Planned | P2 | Q2 2026 |
| QUIC Protocol | 📋 Planned | P3 | Q3 2026 |
| Load Balancing | 📋 Planned | P3 | Q4 2026 |

---

---

**Last Updated**: 2025-10-22
**Maintainer**: kcenon@naver.com
**Version**: 0.1.0.0
