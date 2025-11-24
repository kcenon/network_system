# Connection Pooling Guide

## Overview

Connection pooling allows efficient reuse of network connections, reducing the overhead of establishing new connections for each request. The `connection_pool` class manages a pool of `messaging_client` instances that can be borrowed and returned.

## Quick Start

```cpp
#include "network_system/core/connection_pool.h"

using namespace network_system::core;

// Create a pool with 10 connections
auto pool = std::make_shared<connection_pool>("localhost", 5555, 10);

// Initialize the pool (creates all connections)
auto result = pool->initialize();
if (result.is_err()) {
    std::cerr << "Failed to initialize pool: " << result.error().message << "\n";
    return -1;
}

// Acquire a connection
auto client = pool->acquire();

// Use the connection
std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
client->send_packet(std::move(data));

// Release back to pool when done
pool->release(std::move(client));
```

## Configuration

### Constructor Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `host` | `std::string` | - | Remote host to connect to |
| `port` | `unsigned short` | - | Remote port to connect to |
| `pool_size` | `size_t` | 10 | Number of connections in the pool |

### Example Configurations

#### High Throughput

```cpp
// For high-throughput scenarios with many concurrent requests
auto pool = std::make_shared<connection_pool>("localhost", 5555, 50);
```

#### Resource Constrained

```cpp
// For memory-constrained environments
auto pool = std::make_shared<connection_pool>("localhost", 5555, 5);
```

## API Reference

### connection_pool

```cpp
class connection_pool {
public:
    // Construction
    connection_pool(std::string host, unsigned short port, size_t pool_size = 10);

    // Destructor - stops all connections
    ~connection_pool() noexcept;

    // Initialize the pool (must be called before use)
    auto initialize() -> VoidResult;

    // Acquire a connection (blocks until available)
    auto acquire() -> std::unique_ptr<messaging_client>;

    // Release a connection back to the pool
    auto release(std::unique_ptr<messaging_client> client) -> void;

    // Get number of active (borrowed) connections
    [[nodiscard]] auto active_count() const noexcept -> size_t;

    // Get total pool size
    [[nodiscard]] auto pool_size() const noexcept -> size_t;
};
```

### messaging_client (Pooled Connection)

When you acquire a connection from the pool, you get a `messaging_client` with these key methods:

```cpp
// Send data to the server
auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

// Check if connected
auto is_connected() const -> bool;

// Set callback for received data
auto set_receive_callback(std::function<void(const std::vector<uint8_t>&)> callback) -> void;

// Set callback for connection events
auto set_connected_callback(std::function<void()> callback) -> void;
auto set_disconnected_callback(std::function<void()> callback) -> void;
auto set_error_callback(std::function<void(std::error_code)> callback) -> void;
```

## Best Practices

### 1. Size Your Pool Appropriately

```cpp
// Rule of thumb: pool_size >= expected_concurrent_requests
// Too small: requests will block waiting for connections
// Too large: wastes memory and file descriptors

size_t expected_concurrency = 20;
auto pool = std::make_shared<connection_pool>(host, port, expected_concurrency);
```

### 2. Always Release Connections

```cpp
void process_request() {
    auto client = pool->acquire();

    // IMPORTANT: Always release, even on error
    try {
        client->send_packet(std::move(data));
        // ... process response ...
    } catch (...) {
        pool->release(std::move(client));  // Release on error
        throw;
    }

    pool->release(std::move(client));  // Release on success
}
```

### 3. Use RAII Wrapper for Automatic Release

```cpp
// Helper class for automatic release
class scoped_connection {
public:
    scoped_connection(std::shared_ptr<connection_pool> pool)
        : pool_(pool), client_(pool->acquire()) {}

    ~scoped_connection() {
        if (client_) {
            pool_->release(std::move(client_));
        }
    }

    auto operator->() { return client_.get(); }
    auto get() { return client_.get(); }

    // Disable copy
    scoped_connection(const scoped_connection&) = delete;
    scoped_connection& operator=(const scoped_connection&) = delete;

    // Enable move
    scoped_connection(scoped_connection&&) = default;
    scoped_connection& operator=(scoped_connection&&) = default;

private:
    std::shared_ptr<connection_pool> pool_;
    std::unique_ptr<messaging_client> client_;
};

// Usage
void process_request() {
    scoped_connection conn(pool);  // Acquires connection
    conn->send_packet(std::move(data));
    // Connection automatically released when conn goes out of scope
}
```

### 4. Handle Blocking Acquire

The `acquire()` method blocks if no connections are available. Plan for this:

```cpp
// Monitor pool usage
void monitor_pool(std::shared_ptr<connection_pool> pool) {
    size_t active = pool->active_count();
    size_t total = pool->pool_size();

    double utilization = static_cast<double>(active) / total;

    if (utilization > 0.8) {
        log_warning("Connection pool utilization high: " +
                   std::to_string(active) + "/" + std::to_string(total));
    }
}
```

### 5. Check Connection Validity

The pool automatically reconnects dropped connections when released, but you can verify:

```cpp
auto client = pool->acquire();

if (!client->is_connected()) {
    // This shouldn't happen normally, but handle gracefully
    pool->release(std::move(client));
    // Retry or handle error
    return;
}

client->send_packet(std::move(data));
pool->release(std::move(client));
```

## Common Patterns

### Request-Response Pattern

```cpp
void send_and_receive(const std::vector<uint8_t>& request) {
    auto client = pool->acquire();

    std::promise<std::vector<uint8_t>> response_promise;
    auto response_future = response_promise.get_future();

    client->set_receive_callback([&response_promise](const auto& data) {
        response_promise.set_value(data);
    });

    client->send_packet(std::vector<uint8_t>(request));

    // Wait for response with timeout
    auto status = response_future.wait_for(std::chrono::seconds(5));

    pool->release(std::move(client));

    if (status == std::future_status::timeout) {
        throw std::runtime_error("Response timeout");
    }

    return response_future.get();
}
```

### Worker Thread Pattern

```cpp
void worker_thread(std::shared_ptr<connection_pool> pool,
                   std::queue<Task>& task_queue,
                   std::mutex& queue_mutex,
                   std::atomic<bool>& running) {
    while (running.load()) {
        Task task;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (task_queue.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            task = std::move(task_queue.front());
            task_queue.pop();
        }

        auto client = pool->acquire();
        process_task(client.get(), task);
        pool->release(std::move(client));
    }
}
```

### Concurrent Request Pattern

```cpp
void process_batch(const std::vector<Request>& requests) {
    std::vector<std::future<Response>> futures;

    for (const auto& req : requests) {
        futures.push_back(std::async(std::launch::async, [&, req]() {
            auto client = pool->acquire();
            auto response = send_request(client.get(), req);
            pool->release(std::move(client));
            return response;
        }));
    }

    // Collect results
    std::vector<Response> responses;
    for (auto& f : futures) {
        responses.push_back(f.get());
    }
}
```

## Troubleshooting

### Pool Exhaustion (Blocking on acquire)

**Symptoms:**
- `acquire()` blocks for extended periods
- High `active_count()` compared to `pool_size()`

**Causes:**
1. Pool too small for workload
2. Connection leaks (not releasing)
3. Slow processing holding connections too long

**Solutions:**
```cpp
// 1. Increase pool size
auto pool = std::make_shared<connection_pool>(host, port, 50);  // Larger pool

// 2. Ensure all code paths release connections
// Use RAII wrapper (scoped_connection) for automatic release

// 3. Monitor and optimize slow operations
auto start = std::chrono::steady_clock::now();
// ... process ...
auto duration = std::chrono::steady_clock::now() - start;
if (duration > std::chrono::seconds(1)) {
    log_warning("Slow connection usage: " + std::to_string(duration.count()) + "ns");
}
```

### Connection Failures During Initialize

**Symptoms:**
- `initialize()` returns error
- Some connections fail to establish

**Causes:**
1. Server not running
2. Network issues
3. Too many connections for server to handle

**Solutions:**
```cpp
// 1. Verify server is running and accessible
// 2. Check firewall and network configuration
// 3. Reduce pool size or increase server capacity

auto result = pool->initialize();
if (result.is_err()) {
    std::cerr << "Pool init failed: " << result.error().message << "\n";
    std::cerr << "Context: " << result.error().context << "\n";
    // Handle specific error codes
}
```

### Stale Connections

**Symptoms:**
- Intermittent failures after idle periods
- Errors on first request after pause

**Causes:**
1. Server closing idle connections
2. Network timeouts

**Solutions:**
```cpp
// The pool automatically reconnects when releasing
// If you detect a stale connection, just release it:
auto client = pool->acquire();

auto result = client->send_packet(std::move(data));
if (result.is_err()) {
    // Release will attempt reconnection
    pool->release(std::move(client));

    // Retry with new connection
    client = pool->acquire();
    result = client->send_packet(std::move(data));
}
```

## Performance Considerations

### Memory Usage

Each pooled connection uses:
- `messaging_client` instance (~200 bytes)
- ASIO socket and buffers (~8KB default)
- I/O context overhead (~100 bytes)

**Estimate:** ~10KB per connection

For a pool of 100 connections: ~1MB memory

### File Descriptors

Each connection uses one file descriptor. Ensure your system limits allow for your pool size:

```bash
# Check current limit
ulimit -n

# Increase if needed (temporary)
ulimit -n 10000

# Permanent change: edit /etc/security/limits.conf
# * soft nofile 10000
# * hard nofile 10000
```

### CPU Overhead

Connection pooling reduces CPU overhead compared to creating new connections:
- **Without pooling:** ~1-5ms per connection establishment (TCP handshake)
- **With pooling:** ~10-100us per acquire/release (mutex lock)

## Thread Safety

The `connection_pool` class is fully thread-safe:
- Multiple threads can call `acquire()` and `release()` concurrently
- Internal synchronization uses mutex and condition variable
- No external locking required

```cpp
// Safe to use from multiple threads
std::vector<std::thread> workers;
for (int i = 0; i < 10; ++i) {
    workers.emplace_back([&pool]() {
        for (int j = 0; j < 100; ++j) {
            auto client = pool->acquire();
            // ... use client ...
            pool->release(std::move(client));
        }
    });
}

for (auto& w : workers) {
    w.join();
}
```

## See Also

- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Performance Tuning](PERFORMANCE_TUNING.md) - Optimization guide
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions
