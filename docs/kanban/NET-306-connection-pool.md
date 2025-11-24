# NET-306: Write Connection Pool Usage Guide

| Field | Value |
|-------|-------|
| **ID** | NET-306 |
| **Title** | Write Connection Pool Usage Guide |
| **Category** | DOC |
| **Priority** | LOW |
| **Status** | TODO |
| **Est. Duration** | 2 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (Problem Statement)

### Current Problem
Connection pooling feature (implemented in v1.3.0) lacks user documentation:
- No usage guide for connection pool API
- Configuration options not documented
- Best practices not provided
- Common pitfalls not explained

### Goal
Create comprehensive documentation for connection pooling:
- API usage guide with examples
- Configuration reference
- Best practices and patterns
- Troubleshooting guide

### Deliverables
1. `docs/CONNECTION_POOLING.md` - Main guide
2. `samples/connection_pool_example.cpp` - Working example
3. API reference updates

---

## How (Implementation Plan)

### Step 1: Create Main Documentation

```markdown
# Connection Pooling Guide

## Overview

Connection pooling allows efficient reuse of network connections, reducing
the overhead of establishing new connections for each request.

## Quick Start

```cpp
#include "network_system/core/connection_pool.h"

using namespace network_system::core;

// Create a pool with 10 connections
connection_pool pool("localhost", 5555, {
    .min_connections = 2,
    .max_connections = 10,
    .connection_timeout = std::chrono::seconds(30)
});

// Acquire a connection
auto conn = pool.acquire();

// Use the connection
conn->send_packet({'h', 'e', 'l', 'l', 'o'});
auto response = conn->receive();

// Connection automatically returned when `conn` goes out of scope
```

## Configuration

### Pool Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `min_connections` | size_t | 1 | Minimum idle connections |
| `max_connections` | size_t | 10 | Maximum total connections |
| `connection_timeout` | duration | 30s | Timeout for acquiring connection |
| `idle_timeout` | duration | 5min | Max idle time before close |
| `health_check_interval` | duration | 30s | Interval for health checks |
| `max_lifetime` | duration | 1h | Max connection lifetime |

### Example Configurations

#### High Throughput
```cpp
connection_pool pool("localhost", 5555, {
    .min_connections = 10,
    .max_connections = 100,
    .connection_timeout = std::chrono::seconds(5),
    .idle_timeout = std::chrono::minutes(10)
});
```

#### Resource Constrained
```cpp
connection_pool pool("localhost", 5555, {
    .min_connections = 1,
    .max_connections = 5,
    .idle_timeout = std::chrono::seconds(30)
});
```

## API Reference

### connection_pool

```cpp
class connection_pool {
public:
    // Construction
    connection_pool(const std::string& host, uint16_t port,
                   const pool_config& config = {});

    // Acquire connection (blocks until available or timeout)
    auto acquire() -> std::unique_ptr<pooled_connection>;

    // Try to acquire without blocking
    auto try_acquire() -> std::optional<std::unique_ptr<pooled_connection>>;

    // Async acquire
    auto acquire_async(std::function<void(std::unique_ptr<pooled_connection>)> callback)
        -> void;

    // Pool statistics
    auto stats() const -> pool_stats;

    // Pool management
    auto resize(size_t new_max) -> void;
    auto close() -> void;
};
```

### pooled_connection

```cpp
class pooled_connection {
public:
    // Messaging operations (same as messaging_client)
    auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;
    auto receive() -> Result<std::vector<uint8_t>>;
    auto is_connected() const -> bool;

    // Explicit release (optional - happens automatically on destruction)
    auto release() -> void;

    // Mark as invalid (won't be returned to pool)
    auto invalidate() -> void;
};
```

### pool_stats

```cpp
struct pool_stats {
    size_t total_connections;      // All connections
    size_t active_connections;     // Currently in use
    size_t idle_connections;       // Available in pool
    size_t pending_requests;       // Waiting for connection
    size_t total_acquisitions;     // Lifetime acquisitions
    size_t total_releases;         // Lifetime releases
    size_t failed_acquisitions;    // Acquisition failures
    size_t connections_created;    // Total created
    size_t connections_destroyed;  // Total destroyed
};
```

## Best Practices

### 1. Size Your Pool Appropriately

```cpp
// Rule of thumb: max_connections = expected_concurrent_requests * 1.5
size_t expected_concurrency = 50;
pool_config config{
    .max_connections = static_cast<size_t>(expected_concurrency * 1.5)
};
```

### 2. Use RAII for Connection Management

```cpp
void process_request() {
    auto conn = pool.acquire();
    // conn is automatically released when function returns
    // even if an exception is thrown

    conn->send_packet(request);
    auto response = conn->receive();
    process(response);
}
```

### 3. Handle Acquisition Timeout

```cpp
try {
    auto conn = pool.acquire();
    // Use connection
} catch (const connection_pool_timeout& e) {
    // Pool exhausted, handle gracefully
    log_error("Failed to acquire connection: ", e.what());
    return service_unavailable();
}

// Or use try_acquire for non-blocking
if (auto conn = pool.try_acquire()) {
    // Use connection
} else {
    // No connection available
}
```

### 4. Invalidate Bad Connections

```cpp
auto conn = pool.acquire();

try {
    conn->send_packet(data);
} catch (const connection_error& e) {
    // Connection is bad, don't return to pool
    conn->invalidate();
    throw;
}
```

### 5. Monitor Pool Health

```cpp
void monitor_pool(connection_pool& pool) {
    auto stats = pool.stats();

    // Alert if pool is exhausted
    if (stats.pending_requests > 0) {
        alert("Connection pool has waiting requests");
    }

    // Alert if too many failures
    double failure_rate = static_cast<double>(stats.failed_acquisitions) /
                         stats.total_acquisitions;
    if (failure_rate > 0.01) {  // > 1% failure
        alert("High connection pool failure rate");
    }
}
```

## Common Patterns

### Per-Request Connection

```cpp
http_handler handle_request(const http_request& req) {
    auto conn = pool.acquire();
    auto response = forward_to_backend(conn, req);
    return response;
}
```

### Connection Per Transaction

```cpp
void process_transaction(const Transaction& txn) {
    auto conn = pool.acquire();

    conn->send_packet(serialize(txn.begin()));

    for (const auto& op : txn.operations()) {
        conn->send_packet(serialize(op));
        auto result = conn->receive();
        if (!is_success(result)) {
            conn->send_packet(serialize(txn.rollback()));
            throw TransactionFailed();
        }
    }

    conn->send_packet(serialize(txn.commit()));
}
```

### Long-Running Worker

```cpp
void worker_thread(connection_pool& pool) {
    while (running) {
        auto task = task_queue.pop();

        auto conn = pool.acquire();
        process_task(conn, task);
        // Connection released here
    }
}
```

## Troubleshooting

### Pool Exhaustion

**Symptoms:**
- `acquire()` times out frequently
- `pending_requests` in stats is high

**Causes:**
1. Pool too small for workload
2. Connection leaks (not releasing)
3. Slow backend causing long hold times

**Solutions:**
```cpp
// Increase pool size
pool.resize(new_max_connections);

// Check for leaks - compare created vs destroyed
auto stats = pool.stats();
if (stats.connections_created - stats.connections_destroyed > pool_max * 2) {
    log_warning("Possible connection leak");
}
```

### Connection Churn

**Symptoms:**
- High `connections_created` and `connections_destroyed`
- Poor performance

**Causes:**
1. `idle_timeout` too short
2. `min_connections` too low

**Solutions:**
```cpp
pool_config config{
    .min_connections = 5,  // Keep more idle
    .idle_timeout = std::chrono::minutes(5)  // Longer idle time
};
```

### Stale Connections

**Symptoms:**
- Intermittent failures after idle periods
- Errors on first request after pause

**Causes:**
1. Server closing idle connections
2. Network timeout

**Solutions:**
```cpp
pool_config config{
    .health_check_interval = std::chrono::seconds(15),
    .max_lifetime = std::chrono::minutes(30)
};
```
```

### Step 2: Create Example Code

```cpp
// samples/connection_pool_example.cpp

#include "network_system/core/connection_pool.h"
#include <iostream>
#include <thread>
#include <vector>

using namespace network_system::core;

void basic_usage_example() {
    std::cout << "=== Basic Usage ===\n";

    connection_pool pool("localhost", 5555, {
        .min_connections = 2,
        .max_connections = 10
    });

    // Simple acquire and use
    {
        auto conn = pool.acquire();
        conn->send_packet({'p', 'i', 'n', 'g'});
        auto response = conn->receive();
        std::cout << "Received: " << response.value().size() << " bytes\n";
    }  // Connection automatically released

    auto stats = pool.stats();
    std::cout << "Idle connections: " << stats.idle_connections << "\n";
}

void concurrent_usage_example() {
    std::cout << "\n=== Concurrent Usage ===\n";

    connection_pool pool("localhost", 5555, {
        .max_connections = 20
    });

    std::vector<std::thread> workers;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 100; ++i) {
        workers.emplace_back([&pool, &success_count, i]() {
            try {
                auto conn = pool.acquire();
                conn->send_packet({'t', 'e', 's', 't', static_cast<uint8_t>(i)});
                auto response = conn->receive();
                if (response.has_value()) {
                    success_count++;
                }
            } catch (const std::exception& e) {
                std::cerr << "Worker " << i << " failed: " << e.what() << "\n";
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    std::cout << "Successful requests: " << success_count << "/100\n";
    std::cout << "Pool stats:\n";
    auto stats = pool.stats();
    std::cout << "  Total acquisitions: " << stats.total_acquisitions << "\n";
    std::cout << "  Connections created: " << stats.connections_created << "\n";
}

void error_handling_example() {
    std::cout << "\n=== Error Handling ===\n";

    connection_pool pool("localhost", 5555, {
        .max_connections = 2,
        .connection_timeout = std::chrono::seconds(1)
    });

    // Exhaust the pool
    auto conn1 = pool.acquire();
    auto conn2 = pool.acquire();

    // Try to acquire when exhausted
    if (auto conn3 = pool.try_acquire()) {
        std::cout << "Acquired third connection (unexpected)\n";
    } else {
        std::cout << "Pool exhausted, try_acquire returned empty\n";
    }

    // Release one and try again
    conn1.reset();

    if (auto conn3 = pool.try_acquire()) {
        std::cout << "Successfully acquired after release\n";
    }
}

int main() {
    basic_usage_example();
    concurrent_usage_example();
    error_handling_example();
    return 0;
}
```

---

## Test (Verification Plan)

### Documentation Review
1. Technical accuracy check
2. All code examples compile
3. API matches implementation

### Example Verification
```bash
# Build example
cmake --build build --target connection_pool_example

# Run against test server
./build/samples/test_server &
./build/samples/connection_pool_example
```

### User Testing
- Follow guide to implement connection pooling
- Identify any unclear sections
- Verify troubleshooting advice works

---

## Acceptance Criteria

- [ ] Main documentation complete
- [ ] All code examples compile and run
- [ ] Configuration reference accurate
- [ ] Best practices section helpful
- [ ] Troubleshooting covers common issues
- [ ] API reference matches implementation
- [ ] Reviewed by at least one user

---

## Notes

- Keep examples practical and runnable
- Include performance considerations
- Reference NET-303 for tuning guidance
- Consider adding video/diagram of pool lifecycle
