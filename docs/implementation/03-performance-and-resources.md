# Performance & Resources

> **Part 3 of 4** | [📑 Index](README.md) | [← Previous: Testing](02-dependency-and-testing.md) | [Next: Error Handling →](04-error-handling.md)

> **Language:** **English** | [한국어](03-performance-and-resources_KO.md)

This document covers performance monitoring, benchmarking, and resource management patterns (RAII, smart pointers, thread safety).

---

## 📊 Performance Monitoring

### 1. Performance Metric Collection

```cpp
// include/network_system/utils/performance_monitor.h
#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace network_system::utils {

class performance_monitor {
public:
    struct metrics {
        // Connection metrics
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> failed_connections{0};
        std::atomic<uint64_t> connections_per_second{0};

        // Message metrics
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> messages_per_second{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> bytes_per_second{0};

        // Latency metrics (microseconds)
        std::atomic<uint64_t> avg_latency_us{0};
        std::atomic<uint64_t> min_latency_us{UINT64_MAX};
        std::atomic<uint64_t> max_latency_us{0};
        std::atomic<uint64_t> p95_latency_us{0};
        std::atomic<uint64_t> p99_latency_us{0};

        // Error metrics
        std::atomic<uint64_t> send_errors{0};
        std::atomic<uint64_t> receive_errors{0};
        std::atomic<uint64_t> timeout_errors{0};
        std::atomic<uint64_t> connection_errors{0};

        // Memory metrics
        std::atomic<uint64_t> memory_usage_bytes{0};
        std::atomic<uint64_t> buffer_pool_size{0};
        std::atomic<uint64_t> peak_memory_usage{0};

        // Time information
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;

        // Calculate uptime
        std::chrono::milliseconds uptime() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
        }
    };

    static performance_monitor& instance();

    // Update metrics
    void record_connection(bool success);
    void record_disconnection();
    void record_message_sent(size_t bytes);
    void record_message_received(size_t bytes);
    void record_latency(std::chrono::microseconds latency);
    void record_error(const std::string& error_type);
    void record_memory_usage(size_t bytes);

    // Query metrics
    metrics get_metrics() const;
    void reset_metrics();

    // Real-time statistics calculation
    void update_realtime_stats();

    // Export metrics as JSON
    std::string to_json() const;

    // Generate performance report
    std::string generate_report() const;

private:
    performance_monitor();
    ~performance_monitor() = default;

    performance_monitor(const performance_monitor&) = delete;
    performance_monitor& operator=(const performance_monitor&) = delete;

    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::utils
```

### 2. Benchmark Tools

```cpp
// tests/performance/benchmark_network_performance.cpp
#include <benchmark/benchmark.h>
#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"

namespace network_system::benchmark {

class NetworkBenchmark {
public:
    NetworkBenchmark() {
        server_ = std::make_shared<core::messaging_server>("bench_server");
        server_->set_message_handler([](auto session, const std::string& msg) {
            // Echo server
            session->send_message(msg);
        });

        server_->start_server(0); // Random port
        port_ = server_->port();
    }

    ~NetworkBenchmark() {
        server_->stop_server();
    }

    unsigned short get_port() const { return port_; }

private:
    std::shared_ptr<core::messaging_server> server_;
    unsigned short port_;
};

static NetworkBenchmark* g_benchmark = nullptr;

static void BM_SingleClientEcho(::benchmark::State& state) {
    if (!g_benchmark) {
        g_benchmark = new NetworkBenchmark();
    }

    auto client = std::make_shared<core::messaging_client>("bench_client");

    std::atomic<int> received_count{0};
    client->set_message_handler([&received_count](const std::string&) {
        received_count++;
    });

    client->start_client("127.0.0.1", g_benchmark->get_port());

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const std::string message = "benchmark_message";

    for (auto _ : state) {
        client->send_message(message);

        // Wait for response
        auto start = std::chrono::steady_clock::now();
        while (received_count.load() == 0) {
            auto now = std::chrono::steady_clock::now();
            if (now - start > std::chrono::seconds(1)) {
                state.SkipWithError("Timeout waiting for response");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        received_count--;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * message.size() * 2); // Send+receive
}

static void BM_MultipleClientsEcho(::benchmark::State& state) {
    if (!g_benchmark) {
        g_benchmark = new NetworkBenchmark();
    }

    const int num_clients = state.range(0);
    std::vector<std::shared_ptr<core::messaging_client>> clients;
    std::atomic<int> total_received{0};

    // Create and connect clients
    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_shared<core::messaging_client>(
            "bench_client_" + std::to_string(i));

        client->set_message_handler([&total_received](const std::string&) {
            total_received++;
        });

        client->start_client("127.0.0.1", g_benchmark->get_port());
        clients.push_back(client);
    }

    // Wait for all clients to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::string message = "benchmark_message";

    for (auto _ : state) {
        // Send messages from all clients simultaneously
        for (auto& client : clients) {
            client->send_message(message);
        }

        // Wait for all responses
        auto start = std::chrono::steady_clock::now();
        while (total_received.load() < num_clients) {
            auto now = std::chrono::steady_clock::now();
            if (now - start > std::chrono::seconds(5)) {
                state.SkipWithError("Timeout waiting for responses");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        total_received -= num_clients;
    }

    state.SetItemsProcessed(state.iterations() * num_clients);
    state.SetBytesProcessed(state.iterations() * num_clients * message.size() * 2);
}

static void BM_LargeMessageTransfer(::benchmark::State& state) {
    if (!g_benchmark) {
        g_benchmark = new NetworkBenchmark();
    }

    auto client = std::make_shared<core::messaging_client>("bench_client");

    std::atomic<bool> received{false};
    client->set_message_handler([&received](const std::string&) {
        received = true;
    });

    client->start_client("127.0.0.1", g_benchmark->get_port());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const size_t message_size = state.range(0);
    const std::string message(message_size, 'X');

    for (auto _ : state) {
        received = false;
        client->send_message(message);

        // Wait for response
        auto start = std::chrono::steady_clock::now();
        while (!received.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now - start > std::chrono::seconds(5)) {
                state.SkipWithError("Timeout waiting for response");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * message_size * 2);
}

// Register benchmarks
BENCHMARK(BM_SingleClientEcho)
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(10000);

BENCHMARK(BM_MultipleClientsEcho)
    ->Unit(::benchmark::kMicrosecond)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(100)
    ->Iterations(1000);

BENCHMARK(BM_LargeMessageTransfer)
    ->Unit(::benchmark::kMicrosecond)
    ->Arg(1024)->Arg(8192)->Arg(65536)->Arg(1048576)
    ->Iterations(100);

} // namespace network_system::benchmark

BENCHMARK_MAIN();
```

---

## 🛡️ Resource Management Patterns

### Overview

The network_system achieves **95% RAII compliance** with excellent async-safe resource management patterns. All resources (sessions, sockets, ASIO objects, threads) are managed through RAII and smart pointers.

### 1. Smart Pointer Strategy

#### Session Lifetime Management

**Pattern**: `std::shared_ptr<messaging_session>` with `std::enable_shared_from_this`

```cpp
class messaging_session
    : public std::enable_shared_from_this<messaging_session>
{
public:
    messaging_session(asio::ip::tcp::socket socket,
                      std::string_view server_id);
    ~messaging_session();  // Automatic cleanup

    auto start_session() -> void;
    auto stop_session() -> void;

private:
    std::shared_ptr<internal::tcp_socket> socket_;
    internal::pipeline pipeline_;
    std::atomic<bool> is_stopped_;
};
```

**Why `shared_ptr`?**
1. Server tracks active sessions in vector
2. Async ASIO callbacks need to keep session alive
3. Multiple references to same session possible
4. Automatic cleanup when last reference dropped

**Usage in Server**:
```cpp
class messaging_server {
    std::vector<std::shared_ptr<messaging_session>> active_sessions_;

    void do_accept() {
        acceptor_->async_accept([self = shared_from_this()](auto ec, auto socket) {
            if (!ec) {
                auto session = std::make_shared<messaging_session>(
                    std::move(socket), self->server_id_);

                {
                    std::lock_guard<std::mutex> lock(self->sessions_mutex_);
                    self->active_sessions_.push_back(session);
                }

                session->start_session();
            }
        });
    }
};
```

#### ASIO Resource Ownership

**Pattern**: `std::unique_ptr` for exclusive ownership

```cpp
class messaging_server {
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::thread io_thread_;

    void start_server(unsigned short port) {
        io_context_ = std::make_unique<asio::io_context>();
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            *io_context_,
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)
        );

        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
    }

    ~messaging_server() {
        // Automatic cleanup via RAII
        stop_server();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        // unique_ptr automatically destroys io_context_ and acceptor_
    }
};
```

**Why `unique_ptr`?**
1. Server exclusively owns io_context and acceptor
2. Clear lifetime: created in start_server(), destroyed with server
3. Exception-safe resource management
4. No manual `delete` required

### 2. Async-Safe Pattern: enable_shared_from_this

**Problem**: Async callbacks may outlive the object

```cpp
// BAD: Dangling this pointer if session destroyed
socket_->async_read([this](auto data) {
    this->on_receive(data);  // UNSAFE: 'this' may be invalid!
});
```

**Solution**: Capture `shared_ptr` to keep object alive

```cpp
// GOOD: Captures shared_ptr, keeps session alive
void start_session() {
    auto self = shared_from_this();

    socket_->start_read(
        [self](const std::vector<uint8_t>& data) {
            self->on_receive(data);  // SAFE: self keeps session alive
        },
        [self](std::error_code ec) {
            self->on_error(ec);  // SAFE: self keeps session alive
        }
    );
}
```

**Benefits**:
- Session remains alive during async operation
- When operation completes, `self` goes out of scope, decrementing ref count
- If server removed session from `active_sessions_`, session destructor called when last callback completes
- No manual lifetime management needed

### 3. Thread Safety

#### Atomic State Flags

```cpp
std::atomic<bool> is_stopped_{false};
std::atomic<bool> is_running_{false};
std::atomic<bool> is_connected_{false};
```

#### Mutex Protection for Shared Data

```cpp
mutable std::mutex mode_mutex_;       // Protects pipeline mode flags
mutable std::mutex sessions_mutex_;   // Protects active_sessions_ vector

void remove_session(std::shared_ptr<messaging_session> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);  // RAII lock

    auto it = std::find(active_sessions_.begin(), active_sessions_.end(), session);
    if (it != active_sessions_.end()) {
        active_sessions_.erase(it);
    }
    // Automatic unlock on scope exit
}
```

### 4. Exception Safety

All destructors are `noexcept` and handle cleanup safely:

```cpp
~messaging_session() {
    stop_session();  // Idempotent, safe to call multiple times
    // socket_ and pipeline_ automatically cleaned up by RAII
}

~messaging_server() {
    stop_server();  // Ensures clean shutdown

    if (io_thread_.joinable()) {
        io_thread_.join();  // Wait for thread exit
    }

    // io_context_, acceptor_, active_sessions_ automatically cleaned up
    // unique_ptr and vector destructors handle cleanup
}
```

### 5. Resource Categories

| Resource Type | Management | Ownership | Cleanup |
|---------------|------------|-----------|---------|
| Sessions | `std::shared_ptr` | Shared (server + callbacks) | Ref counting |
| ASIO resources | `std::unique_ptr` | Exclusive (server) | Destructor |
| Threads | Automatic storage | Exclusive (server) | join() in destructor |
| Synchronization | Automatic storage | Class member | Automatic |

### 6. Best Practices Applied

✅ **Smart pointers for all heap allocations**
- `shared_ptr` for sessions (shared ownership)
- `unique_ptr` for ASIO resources (exclusive ownership)
- No naked `new`/`delete` in production code

✅ **RAII for all resources**
- Resources acquired in constructor
- Resources released in destructor
- Exception-safe initialization

✅ **enable_shared_from_this for async safety**
- Sessions can create `shared_ptr` to themselves
- Essential for capturing in async lambdas
- Prevents dangling `this` pointers

✅ **Thread-safe resource access**
- Atomic flags for state
- Mutexes for shared containers
- RAII lock guards for critical sections

✅ **Exception-safe destructors**
- No exceptions thrown from destructors
- Thread join handled safely
- Standard containers handle cleanup

