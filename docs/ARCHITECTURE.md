# Network System Architecture

> **Language:** **English** | [한국어](ARCHITECTURE_KO.md)

## Overview

This document provides a comprehensive architectural overview of network_system, a high-performance asynchronous networking library built on ASIO (Asynchronous I/O). Originally extracted from messaging_system for enhanced modularity and reusability, network_system delivers 305K+ messages/second with sub-microsecond latency through zero-copy pipelines and efficient connection management.

**Version**: 1.0.0
**Last Updated**: 2025-10-22
**Architecture Style**: Layered, Asynchronous, Event-Driven

---

## Table of Contents

- [Purpose & Design Goals](#purpose--design-goals)
- [Architecture Layers](#architecture-layers)
- [Core Components](#core-components)
- [Integration Architecture](#integration-architecture)
- [Threading Model](#threading-model)
- [Memory Management](#memory-management)
- [Performance Characteristics](#performance-characteristics)
- [Build Configuration](#build-configuration)

---

## Purpose & Design Goals

### Primary Purpose

Network_system provides a production-ready, high-performance networking foundation that:

- **Abstracts complexity**: Simplifies asynchronous network programming through intuitive APIs
- **Maximizes performance**: Achieves 305K+ msg/s through zero-copy and efficient I/O
- **Enables modularity**: Cleanly separates from messaging layer for reusability
- **Supports integration**: Pluggable interfaces for thread, logger, and container systems

### Design Goals

| Goal | Description | Status |
|------|-------------|--------|
| **High Performance** | Sub-microsecond latency, 300K+ msg/s throughput | ✅ Achieved |
| **Zero-Copy I/O** | Direct memory mapping for network operations | ✅ Achieved |
| **Modularity** | Independent from messaging_system | ✅ Achieved |
| **Async-First** | Coroutine-based async operations (C++20) | ✅ Achieved |
| **Integration-Friendly** | Pluggable thread, logger, container systems | ✅ Achieved |
| **Connection Pooling** | Efficient connection reuse | 🔄 In Progress |
| **TLS/SSL Support** | Secure communication | 📋 Planned |

---

## Architecture Layers

Network_system follows a layered architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                   Application Layer                         │
│         (messaging_system, database_system, etc.)           │
└─────────────────────────┬───────────────────────────────────┘
                          │ uses
┌─────────────────────────▼───────────────────────────────────┐
│                  Core Layer (Public API)                    │
│  ┌────────────────────────────────────────────────────┐    │
│  │  MessagingServer  │  MessagingClient               │    │
│  │  ────────────────────────────────────────────────  │    │
│  │  - Connection management   - Session lifecycle    │    │
│  │  - Message routing         - Event callbacks      │    │
│  └────────────────────────────────────────────────────┘    │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│              Session Management Layer                       │
│  ┌────────────────────────────────────────────────────┐    │
│  │  MessagingSession  │  SessionManager              │    │
│  │  ────────────────────────────────────────────────  │    │
│  │  - Active connections   - Lifecycle management    │    │
│  │  - Read/Write ops       - Session pooling         │    │
│  └────────────────────────────────────────────────────┘    │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│              Internal Implementation Layer                  │
│  ┌────────────────────────────────────────────────────┐    │
│  │  TCPSocket  │  Pipeline  │  Coroutines            │    │
│  │  ────────────────────────────────────────────────  │    │
│  │  - ASIO wrapper        - Zero-copy buffer         │    │
│  │  - Socket operations   - Send/Receive coroutines  │    │
│  └────────────────────────────────────────────────────┘    │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│              Integration Layer (Pluggable)                  │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Thread Integration  │  Logger Integration        │    │
│  │  Container Integration │  Common System           │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│              Platform Layer (ASIO)                          │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Boost.ASIO / Standalone ASIO                      │    │
│  │  - I/O Context    - Async Operations               │    │
│  │  - TCP Sockets    - Event Loop                     │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Layer Descriptions

#### 1. Core Layer (Public API)

**Purpose**: Provide high-level networking abstractions for applications

**Components**:
- `MessagingServer`: TCP server with multi-client support
- `MessagingClient`: TCP client with reconnection support
- Public APIs for send/receive operations

**Responsibilities**:
- Connection lifecycle management
- Message routing and dispatching
- Event callback registration (on_connect, on_disconnect, on_message)

#### 2. Session Management Layer

**Purpose**: Manage active network sessions and their lifecycle

**Components**:
- `MessagingSession`: Represents an active connection
- `SessionManager`: Manages session pool and lifecycle

**Responsibilities**:
- Session creation and destruction
- Read/Write buffer management
- Session state tracking (connected, disconnecting, closed)

#### 3. Internal Implementation Layer

**Purpose**: Low-level networking primitives and optimizations

**Components**:
- `TCPSocket`: ASIO socket wrapper
- `Pipeline`: Zero-copy message pipeline
- `SendCoroutine`: Async send operations
- `ReceiveCoroutine`: Async receive operations

**Responsibilities**:
- Direct socket I/O operations
- Zero-copy buffer management
- Coroutine-based async patterns

#### 4. Integration Layer

**Purpose**: Enable pluggable integration with ecosystem modules

**Components**:
- `thread_integration`: Thread pool abstraction
- `logger_integration`: Logging abstraction
- `container_integration`: Serialization support
- `common_system_adapter`: Result<T> pattern integration

**Responsibilities**:
- Adapter pattern implementations
- Interface definitions for external systems
- Fallback implementations (basic_logger, basic_thread_pool)

#### 5. Platform Layer (ASIO)

**Purpose**: Cross-platform asynchronous I/O

**Components**:
- Boost.ASIO or standalone ASIO
- I/O context and event loop
- TCP socket primitives

---

## Core Components

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

## Integration Architecture

### Thread System Integration

**Purpose**: Pluggable thread pool for async operations

```cpp
// Interface definition
class thread_pool_interface {
public:
    virtual void post(std::function<void()> task) = 0;
    virtual size_t size() const = 0;
};

// Adapter for thread_system
class thread_system_adapter : public thread_pool_interface {
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
public:
    void post(std::function<void()> task) override {
        pool_->post([task](const auto&) {
            task();
            return kcenon::thread::job_parameter{};
        });
    }
};

// Usage
auto thread_pool = std::make_shared<kcenon::thread::thread_pool>(8);
auto adapter = std::make_shared<thread_system_adapter>(thread_pool);
auto server = MessagingServer("0.0.0.0", 8080, adapter);
```

**Benefits**:
- Reuse existing thread pools
- Consistent threading across application
- Performance monitoring via thread_system

### Logger System Integration

**Purpose**: Comprehensive logging of network operations

```cpp
// Interface definition
class logger_interface {
public:
    virtual void log(log_level level, const std::string& message) = 0;
};

// Adapter for logger_system
class logger_system_adapter : public logger_interface {
    std::shared_ptr<kcenon::logger::logger> logger_;
public:
    void log(log_level level, const std::string& message) override {
        logger_->log(convert_level(level), message);
    }
};

// Usage
auto logger = kcenon::logger::logger_builder().build();
auto adapter = std::make_shared<logger_system_adapter>(logger);
set_global_logger(adapter);
```

**Logged Events**:
- Connection establishment/closure
- Message send/receive
- Errors and warnings
- Performance metrics

### Container System Integration

**Purpose**: Efficient message serialization

```cpp
// Send container over network
auto container = container_system::create_container();
container->set_value("type", "request");
container->set_value("data", payload);

auto binary = container->serialize_to_binary();
client.send(binary);

// Receive and deserialize
client.on_message_received([](const std::vector<uint8_t>& data) {
    auto container = container_system::deserialize_from_binary(data);
    auto type = container->get_string("type");
    // Process message...
});
```

**Benefits**:
- Type-safe message serialization
- Multiple formats (binary, JSON, XML)
- Zero-copy with pipeline integration

### Common System Integration

**Purpose**: Standardized error handling

```cpp
// All network operations return Result<T>
auto connect_result = client.connect();
if (!connect_result.is_ok()) {
    auto error = connect_result.error();
    std::cerr << "Connection failed: " << error.message << std::endl;
}

// Monadic error handling
auto result = client.connect()
    .and_then([&client](const auto&) {
        return client.send(data);
    })
    .or_else([](const auto& error) {
        log_error(error);
        return retry_connection();
    });
```

---

## Threading Model

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

## Build Configuration

### CMake Options

```cmake
# Core options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_SAMPLES "Build sample programs" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)

# Integration options
option(BUILD_WITH_THREAD_SYSTEM "Enable thread_system integration" OFF)
option(BUILD_WITH_LOGGER_SYSTEM "Enable logger_system integration" OFF)
option(BUILD_WITH_CONTAINER_SYSTEM "Enable container_system integration" OFF)
option(BUILD_WITH_COMMON_SYSTEM "Enable common_system integration" ON)

# Feature options
option(ENABLE_COROUTINES "Enable C++20 coroutines" ON)
option(ENABLE_ZERO_COPY "Enable zero-copy pipeline" ON)
option(ENABLE_MEMORY_PROFILER "Enable memory profiling" ON)
```

### Integration Topology

```cmake
# Full ecosystem integration
find_package(thread_system CONFIG REQUIRED)
find_package(logger_system CONFIG REQUIRED)
find_package(container_system CONFIG REQUIRED)
find_package(common_system CONFIG REQUIRED)
find_package(network_system CONFIG REQUIRED)

target_link_libraries(my_app PRIVATE
    kcenon::thread_system
    kcenon::logger_system
    kcenon::container_system
    kcenon::common_system
    kcenon::network_system
)

target_compile_definitions(my_app PRIVATE
    BUILD_WITH_THREAD_SYSTEM=1
    BUILD_WITH_LOGGER_SYSTEM=1
    BUILD_WITH_CONTAINER_SYSTEM=1
)
```

---

## Design Patterns

### 1. Proactor Pattern (ASIO)

Asynchronous operation completion notifications:
```
Initiator → Async Operation → Completion Handler
```

### 2. Adapter Pattern

Pluggable integration with external systems:
```
Network System Interface → Adapter → External System
```

### 3. Observer Pattern

Event callbacks for connection lifecycle:
```
Subject (Server/Client) → Observers (Callbacks)
```

### 4. Object Pool Pattern

Session pooling for connection reuse:
```
Session Pool → Acquire → Use → Release → Pool
```

---

## Future Enhancements

### Planned Features

| Feature | Status | Priority | ETA |
|---------|--------|----------|-----|
| TLS/SSL Support | 📋 Planned | P1 | Q1 2026 |
| Connection Pooling | 🔄 In Progress | P1 | Q4 2025 |
| HTTP/2 Support | 📋 Planned | P2 | Q2 2026 |
| WebSocket Support | 📋 Planned | P2 | Q2 2026 |
| QUIC Protocol | 📋 Planned | P3 | Q3 2026 |
| Load Balancing | 📋 Planned | P3 | Q4 2026 |

---

## References

- **ASIO Documentation**: https://think-async.com/Asio/
- **C++20 Coroutines**: https://en.cppreference.com/w/cpp/language/coroutines
- **Integration Guide**: [INTEGRATION.md](../INTEGRATION.md)
- **Performance Baseline**: [BASELINE.md](../BASELINE.md)
- **API Reference**: [API_REFERENCE.md](API_REFERENCE.md)

---

**Last Updated**: 2025-10-22
**Maintainer**: kcenon@naver.com
**Version**: 1.0.0
