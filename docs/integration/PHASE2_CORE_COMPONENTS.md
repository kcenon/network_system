# Phase 2: Core Component Refactoring

**Status**: 🔲 Not Started
**Dependencies**: Phase 1 (Foundation Infrastructure)
**Estimated Time**: 2-3 days
**Components**: 11 core network components

---

## 📋 Overview

Phase 2 focuses on refactoring all core network components to use `thread_pool` instead of direct `std::thread` usage. Each component currently creates dedicated threads to run ASIO `io_context::run()` loops. We will replace these with `io_context_executor` instances backed by thread pools from `thread_pool_manager`.

### Goals

1. **Eliminate Direct std::thread Usage**: Remove all `std::thread` creation in core components
2. **Use io_context_executor**: Leverage RAII wrapper for io_context execution
3. **Maintain Functionality**: Zero regression in component behavior
4. **Improve Monitoring**: Gain visibility into thread pool usage
5. **Simplify Lifecycle**: Automatic resource cleanup via RAII

### Success Criteria

- ✅ No `std::thread` in any core component
- ✅ All components use `io_context_executor`
- ✅ All existing tests pass without modification
- ✅ Clean shutdown with no leaked threads
- ✅ Performance within 5% of baseline

---

## 🗂️ Component List

### Server Components (5)

1. **messaging_server** - TCP server for messaging
2. **messaging_udp_server** - UDP server for messaging
3. **messaging_ws_server** - WebSocket server
4. **secure_messaging_server** - TLS/SSL TCP server
5. **http_server** - HTTP server implementation

### Client Components (4)

1. **messaging_client** - TCP client for messaging
2. **messaging_udp_client** - UDP client for messaging
3. **messaging_ws_client** - WebSocket client
4. **reliable_udp_client** - Reliable UDP client with retransmission

### Utility Components (2)

1. **health_monitor** - System health monitoring
2. **memory_profiler** - Memory usage profiling

---

## 🎯 Migration Pattern

Each component follows this refactoring pattern:

### Before (Current Implementation)

```cpp
class messaging_server {
private:
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<std::thread> server_thread_;
    std::atomic<bool> running_{false};

public:
    void start() {
        running_ = true;
        server_thread_ = std::make_unique<std::thread>([this]() {
            try {
                io_context_->run();
            } catch (const std::exception& e) {
                // Handle exception
            }
        });
    }

    void stop() {
        if (running_) {
            running_ = false;
            io_context_->stop();
            if (server_thread_ && server_thread_->joinable()) {
                server_thread_->join();
            }
        }
    }
};
```

### After (Thread Pool Integration)

```cpp
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"

class messaging_server {
private:
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<io_context_executor> io_executor_;
    std::atomic<bool> running_{false};

public:
    void start() {
        running_ = true;

        // Get pool from manager
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.create_io_pool("messaging_server");

        // Create executor
        io_executor_ = std::make_unique<io_context_executor>(
            pool,
            *io_context_,
            "messaging_server"
        );

        // Start execution
        io_executor_->start();
    }

    void stop() {
        if (running_) {
            running_ = false;
            io_context_->stop();

            // Executor automatically stops and joins in destructor
            io_executor_.reset();
        }
    }
};
```

### Key Changes

1. **Include Headers**: Add `thread_pool_manager.h` and `io_context_executor.h`
2. **Replace Member**: `std::unique_ptr<std::thread>` → `std::unique_ptr<io_context_executor>`
3. **Use Manager**: Get pool from `thread_pool_manager::instance()`
4. **Create Executor**: Pass pool, io_context, and component name
5. **Automatic Cleanup**: RAII handles thread joining and cleanup

---

## 📝 Component-by-Component Migration

### 1. messaging_server

**File**: `src/network/messaging_server.cpp`
**Current std::thread Usage**: Line 100
**Complexity**: ⭐⭐ (Medium)

#### Current Implementation Issues

```cpp
// Line 100: messaging_server.cpp
server_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("[messaging_server] Thread exception: " +
                            std::string(e.what()));
        }
    });
```

**Problems**:
- Manual thread lifecycle management
- Error handling in lambda
- No monitoring or statistics
- Complex shutdown coordination

#### Refactored Implementation

**Header Changes** (`include/network_system/network/messaging_server.h`):

```cpp
// Add includes
#include "network_system/integration/io_context_executor.h"

// Forward declarations
namespace kcenon::thread {
    class thread_pool;
}

class messaging_server {
private:
    // REMOVE: std::unique_ptr<std::thread> server_thread_;

    // ADD: IO context executor
    std::unique_ptr<io_context_executor> io_executor_;

    // Existing members...
    std::unique_ptr<asio::io_context> io_context_;
    std::atomic<bool> running_{false};
};
```

**Implementation Changes** (`src/network/messaging_server.cpp`):

```cpp
#include "network_system/integration/thread_pool_manager.h"

void messaging_server::start() {
    if (running_) {
        NETWORK_LOG_WARN("[messaging_server] Already running");
        return;
    }

    try {
        // Initialize io_context if needed
        if (!io_context_) {
            io_context_ = std::make_unique<asio::io_context>();
        }

        // Get thread pool from manager
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.create_io_pool("messaging_server");

        // Create io_context executor
        io_executor_ = std::make_unique<io_context_executor>(
            pool,
            *io_context_,
            "messaging_server"
        );

        // Start acceptor and other ASIO operations...
        start_accept();

        // Start io_context execution
        io_executor_->start();
        running_ = true;

        NETWORK_LOG_INFO("[messaging_server] Started successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[messaging_server] Start failed: " +
                        std::string(e.what()));
        throw;
    }
}

void messaging_server::stop() {
    if (!running_) {
        return;
    }

    try {
        NETWORK_LOG_INFO("[messaging_server] Stopping...");

        running_ = false;

        // Stop acceptor
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }

        // Stop io_context
        if (io_context_) {
            io_context_->stop();
        }

        // Stop executor (RAII will handle cleanup)
        io_executor_.reset();

        NETWORK_LOG_INFO("[messaging_server] Stopped successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[messaging_server] Stop failed: " +
                        std::string(e.what()));
    }
}
```

#### Testing Updates

**Test File**: `tests/messaging_server_test.cpp`

```cpp
TEST_F(MessagingServerTest, ThreadPoolIntegration) {
    // Initialize thread pool manager
    auto& mgr = thread_pool_manager::instance();
    ASSERT_TRUE(mgr.initialize());

    // Start server
    server_->start();
    EXPECT_TRUE(server_->is_running());

    // Verify statistics
    auto stats = mgr.get_statistics();
    EXPECT_GT(stats.io_pools_created, 0);
    EXPECT_GT(stats.total_active_pools, 0);

    // Stop server
    server_->stop();
    EXPECT_FALSE(server_->is_running());

    // Cleanup
    mgr.shutdown();
}

TEST_F(MessagingServerTest, CleanShutdown) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    server_->start();

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server_->stop();

    // Verify no leaked threads
    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.total_active_pools, 0);

    mgr.shutdown();
}
```

---

### 2. messaging_client

**File**: `src/network/messaging_client.cpp`
**Current std::thread Usage**: Line 143
**Complexity**: ⭐⭐ (Medium)

#### Current Implementation

```cpp
// Line 143: messaging_client.cpp
client_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("[messaging_client] Thread exception: " +
                            std::string(e.what()));
        }
    });
```

#### Refactored Implementation

**Header Changes** (`include/network_system/network/messaging_client.h`):

```cpp
#include "network_system/integration/io_context_executor.h"

class messaging_client {
private:
    // REMOVE: std::unique_ptr<std::thread> client_thread_;

    // ADD:
    std::unique_ptr<io_context_executor> io_executor_;

    // Existing...
    std::unique_ptr<asio::io_context> io_context_;
    std::atomic<bool> connected_{false};
};
```

**Implementation Changes** (`src/network/messaging_client.cpp`):

```cpp
#include "network_system/integration/thread_pool_manager.h"

void messaging_client::connect(const std::string& host, uint16_t port) {
    if (connected_) {
        NETWORK_LOG_WARN("[messaging_client] Already connected");
        return;
    }

    try {
        // Initialize io_context
        if (!io_context_) {
            io_context_ = std::make_unique<asio::io_context>();
        }

        // Get thread pool
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.create_io_pool("messaging_client");

        // Create executor
        io_executor_ = std::make_unique<io_context_executor>(
            pool,
            *io_context_,
            "messaging_client"
        );

        // Start async connect
        auto endpoint = asio::ip::tcp::endpoint(
            asio::ip::make_address(host), port);

        socket_->async_connect(endpoint,
            [this](const asio::error_code& ec) {
                handle_connect(ec);
            });

        // Start io_context
        io_executor_->start();

        NETWORK_LOG_INFO("[messaging_client] Connecting to " +
                        host + ":" + std::to_string(port));
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[messaging_client] Connect failed: " +
                        std::string(e.what()));
        throw;
    }
}

void messaging_client::disconnect() {
    if (!connected_) {
        return;
    }

    try {
        NETWORK_LOG_INFO("[messaging_client] Disconnecting...");

        connected_ = false;

        // Close socket
        if (socket_ && socket_->is_open()) {
            socket_->close();
        }

        // Stop io_context
        if (io_context_) {
            io_context_->stop();
        }

        // Stop executor
        io_executor_.reset();

        NETWORK_LOG_INFO("[messaging_client] Disconnected successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[messaging_client] Disconnect failed: " +
                        std::string(e.what()));
    }
}
```

---

### 3. messaging_udp_server

**File**: `src/network/messaging_udp_server.cpp`
**Current std::thread Usage**: Line 87
**Complexity**: ⭐⭐ (Medium)

#### Current Implementation

```cpp
// Line 87: messaging_udp_server.cpp
worker_thread_ = std::thread(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("Worker thread exception: " +
                            std::string(e.what()));
        }
    });
```

#### Refactored Implementation

**Header Changes** (`include/network_system/network/messaging_udp_server.h`):

```cpp
#include "network_system/integration/io_context_executor.h"

class messaging_udp_server {
private:
    // REMOVE: std::thread worker_thread_;

    // ADD:
    std::unique_ptr<io_context_executor> io_executor_;

    // Existing...
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    std::atomic<bool> running_{false};
};
```

**Implementation Changes** (`src/network/messaging_udp_server.cpp`):

```cpp
#include "network_system/integration/thread_pool_manager.h"

void messaging_udp_server::start(uint16_t port) {
    if (running_) {
        NETWORK_LOG_WARN("[udp_server] Already running");
        return;
    }

    try {
        // Initialize io_context
        if (!io_context_) {
            io_context_ = std::make_unique<asio::io_context>();
        }

        // Create and bind socket
        socket_ = std::make_unique<asio::ip::udp::socket>(
            *io_context_,
            asio::ip::udp::endpoint(asio::ip::udp::v4(), port)
        );

        // Get thread pool
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.create_io_pool("messaging_udp_server");

        // Create executor
        io_executor_ = std::make_unique<io_context_executor>(
            pool,
            *io_context_,
            "messaging_udp_server"
        );

        // Start receiving
        start_receive();

        // Start io_context
        io_executor_->start();
        running_ = true;

        NETWORK_LOG_INFO("[udp_server] Started on port " +
                        std::to_string(port));
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[udp_server] Start failed: " +
                        std::string(e.what()));
        throw;
    }
}

void messaging_udp_server::stop() {
    if (!running_) {
        return;
    }

    try {
        NETWORK_LOG_INFO("[udp_server] Stopping...");

        running_ = false;

        // Close socket
        if (socket_ && socket_->is_open()) {
            socket_->close();
        }

        // Stop io_context
        if (io_context_) {
            io_context_->stop();
        }

        // Stop executor
        io_executor_.reset();

        NETWORK_LOG_INFO("[udp_server] Stopped successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[udp_server] Stop failed: " +
                        std::string(e.what()));
    }
}
```

---

### 4. messaging_ws_client

**File**: `src/network/messaging_ws_client.cpp`
**Current std::thread Usage**: Line 103
**Complexity**: ⭐⭐⭐ (High - WebSocket handshake complexity)

#### Current Implementation

```cpp
// Line 103: messaging_ws_client.cpp
io_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("[ws_client] IO thread exception: " +
                            std::string(e.what()));
        }
    });
```

#### Refactored Implementation

**Header Changes** (`include/network_system/network/messaging_ws_client.h`):

```cpp
#include "network_system/integration/io_context_executor.h"

class messaging_ws_client {
private:
    // REMOVE: std::unique_ptr<std::thread> io_thread_;

    // ADD:
    std::unique_ptr<io_context_executor> io_executor_;

    // Existing...
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<websocket_stream_type> ws_stream_;
    std::atomic<bool> connected_{false};
};
```

**Implementation Changes** (`src/network/messaging_ws_client.cpp`):

```cpp
#include "network_system/integration/thread_pool_manager.h"

void messaging_ws_client::connect(const std::string& host,
                                  const std::string& port,
                                  const std::string& path) {
    if (connected_) {
        NETWORK_LOG_WARN("[ws_client] Already connected");
        return;
    }

    try {
        // Initialize io_context
        if (!io_context_) {
            io_context_ = std::make_unique<asio::io_context>();
        }

        // Get thread pool
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.create_io_pool("messaging_ws_client");

        // Create executor
        io_executor_ = std::make_unique<io_context_executor>(
            pool,
            *io_context_,
            "messaging_ws_client"
        );

        // Resolve hostname
        resolver_ = std::make_unique<asio::ip::tcp::resolver>(*io_context_);

        resolver_->async_resolve(host, port,
            [this, host, path](const asio::error_code& ec,
                              asio::ip::tcp::resolver::results_type results) {
                if (!ec) {
                    handle_resolve(results, host, path);
                } else {
                    NETWORK_LOG_ERROR("[ws_client] Resolve failed: " +
                                    ec.message());
                }
            });

        // Start io_context
        io_executor_->start();

        NETWORK_LOG_INFO("[ws_client] Connecting to ws://" +
                        host + ":" + port + path);
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[ws_client] Connect failed: " +
                        std::string(e.what()));
        throw;
    }
}

void messaging_ws_client::disconnect() {
    if (!connected_) {
        return;
    }

    try {
        NETWORK_LOG_INFO("[ws_client] Disconnecting...");

        connected_ = false;

        // Close WebSocket
        if (ws_stream_) {
            ws_stream_->async_close(websocket::close_code::normal,
                [](const asio::error_code& ec) {
                    if (ec) {
                        NETWORK_LOG_WARN("[ws_client] Close warning: " +
                                       ec.message());
                    }
                });
        }

        // Stop io_context
        if (io_context_) {
            io_context_->stop();
        }

        // Stop executor
        io_executor_.reset();

        NETWORK_LOG_INFO("[ws_client] Disconnected successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[ws_client] Disconnect failed: " +
                        std::string(e.what()));
    }
}
```

---

### 5. health_monitor

**File**: `src/utility/health_monitor.cpp`
**Current std::thread Usage**: Line 106
**Complexity**: ⭐ (Low - Simple periodic timer)

#### Current Implementation

```cpp
// Line 106: health_monitor.cpp
monitor_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("[health_monitor] Exception: " +
                            std::string(e.what()));
        }
    });
```

#### Refactored Implementation

**Header Changes** (`include/network_system/utility/health_monitor.h`):

```cpp
#include "network_system/integration/io_context_executor.h"

class health_monitor {
private:
    // REMOVE: std::unique_ptr<std::thread> monitor_thread_;

    // ADD:
    std::unique_ptr<io_context_executor> io_executor_;

    // Existing...
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::steady_timer> timer_;
    std::atomic<bool> monitoring_{false};
};
```

**Implementation Changes** (`src/utility/health_monitor.cpp`):

```cpp
#include "network_system/integration/thread_pool_manager.h"

void health_monitor::start(std::chrono::milliseconds interval) {
    if (monitoring_) {
        NETWORK_LOG_WARN("[health_monitor] Already monitoring");
        return;
    }

    try {
        // Initialize io_context
        if (!io_context_) {
            io_context_ = std::make_unique<asio::io_context>();
        }

        // Create timer
        timer_ = std::make_unique<asio::steady_timer>(*io_context_);

        // Get thread pool
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.create_io_pool("health_monitor");

        // Create executor
        io_executor_ = std::make_unique<io_context_executor>(
            pool,
            *io_context_,
            "health_monitor"
        );

        // Start monitoring
        schedule_check(interval);

        // Start io_context
        io_executor_->start();
        monitoring_ = true;

        NETWORK_LOG_INFO("[health_monitor] Started with interval " +
                        std::to_string(interval.count()) + "ms");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[health_monitor] Start failed: " +
                        std::string(e.what()));
        throw;
    }
}

void health_monitor::stop() {
    if (!monitoring_) {
        return;
    }

    try {
        NETWORK_LOG_INFO("[health_monitor] Stopping...");

        monitoring_ = false;

        // Cancel timer
        if (timer_) {
            timer_->cancel();
        }

        // Stop io_context
        if (io_context_) {
            io_context_->stop();
        }

        // Stop executor
        io_executor_.reset();

        NETWORK_LOG_INFO("[health_monitor] Stopped successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[health_monitor] Stop failed: " +
                        std::string(e.what()));
    }
}

void health_monitor::schedule_check(std::chrono::milliseconds interval) {
    if (!monitoring_) {
        return;
    }

    timer_->expires_after(interval);
    timer_->async_wait([this, interval](const asio::error_code& ec) {
        if (!ec && monitoring_) {
            // Perform health check
            perform_health_check();

            // Schedule next check
            schedule_check(interval);
        }
    });
}
```

---

## 🔧 Common Patterns

### Pattern 1: Include Headers

All components need these includes:

```cpp
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"
```

### Pattern 2: Member Variable Replacement

```cpp
// REMOVE:
std::unique_ptr<std::thread> thread_;  // or just std::thread

// ADD:
std::unique_ptr<io_context_executor> io_executor_;
```

### Pattern 3: Start Pattern

```cpp
void component::start() {
    // 1. Initialize io_context
    if (!io_context_) {
        io_context_ = std::make_unique<asio::io_context>();
    }

    // 2. Get thread pool from manager
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.create_io_pool("component_name");

    // 3. Create executor
    io_executor_ = std::make_unique<io_context_executor>(
        pool,
        *io_context_,
        "component_name"
    );

    // 4. Set up ASIO operations (acceptors, timers, etc.)
    setup_asio_operations();

    // 5. Start executor
    io_executor_->start();

    // 6. Update state
    running_ = true;
}
```

### Pattern 4: Stop Pattern

```cpp
void component::stop() {
    if (!running_) {
        return;
    }

    // 1. Update state
    running_ = false;

    // 2. Close/cancel ASIO operations
    close_asio_operations();

    // 3. Stop io_context
    if (io_context_) {
        io_context_->stop();
    }

    // 4. Stop executor (RAII cleanup)
    io_executor_.reset();
}
```

### Pattern 5: Exception Safety

```cpp
void component::start() {
    try {
        // ... initialization code ...

        io_executor_->start();
        running_ = true;

        NETWORK_LOG_INFO("[component] Started successfully");
    }
    catch (const std::exception& e) {
        NETWORK_LOG_ERROR("[component] Start failed: " +
                        std::string(e.what()));

        // Cleanup on failure
        cleanup();
        throw;
    }
}
```

---

## 📊 Migration Checklist

For each component, complete these steps:

### Pre-Migration

- [ ] Read current implementation
- [ ] Identify all std::thread usage
- [ ] Review component lifecycle (start/stop)
- [ ] Note any special threading requirements
- [ ] Check existing tests

### During Migration

- [ ] Update header file
  - [ ] Add new includes
  - [ ] Replace thread member with io_executor
  - [ ] Add forward declarations if needed
- [ ] Update implementation file
  - [ ] Include thread_pool_manager.h
  - [ ] Refactor start() method
  - [ ] Refactor stop() method
  - [ ] Add exception handling
  - [ ] Add logging
- [ ] Update tests
  - [ ] Add thread_pool_manager initialization
  - [ ] Add statistics verification
  - [ ] Add clean shutdown tests
  - [ ] Verify existing tests still pass

### Post-Migration

- [ ] Compile successfully
- [ ] All tests pass
- [ ] No memory leaks (valgrind/sanitizers)
- [ ] Performance benchmarks
- [ ] Code review
- [ ] Update documentation

---

## 🧪 Testing Strategy

### Unit Tests

Each component should have these tests:

```cpp
TEST_F(ComponentTest, ThreadPoolIntegration) {
    auto& mgr = thread_pool_manager::instance();
    ASSERT_TRUE(mgr.initialize());

    component_->start();
    EXPECT_TRUE(component_->is_running());

    auto stats = mgr.get_statistics();
    EXPECT_GT(stats.io_pools_created, 0);

    component_->stop();
    EXPECT_FALSE(component_->is_running());

    mgr.shutdown();
}

TEST_F(ComponentTest, MultipleStartStop) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    // First cycle
    component_->start();
    EXPECT_TRUE(component_->is_running());
    component_->stop();
    EXPECT_FALSE(component_->is_running());

    // Second cycle
    component_->start();
    EXPECT_TRUE(component_->is_running());
    component_->stop();
    EXPECT_FALSE(component_->is_running());

    mgr.shutdown();
}

TEST_F(ComponentTest, ExceptionSafety) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    // Test start failure handling
    // ... force error condition ...
    EXPECT_THROW(component_->start(), std::exception);

    // Verify clean state
    EXPECT_FALSE(component_->is_running());

    mgr.shutdown();
}

TEST_F(ComponentTest, CleanShutdown) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    component_->start();

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    component_->stop();

    // Verify no thread leaks
    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.total_active_pools, 0);

    mgr.shutdown();
}
```

### Integration Tests

Test component interaction with thread pools:

```cpp
TEST(IntegrationTest, MultipleComponents) {
    auto& mgr = thread_pool_manager::instance();
    ASSERT_TRUE(mgr.initialize());

    // Create multiple components
    auto server = std::make_unique<messaging_server>();
    auto client = std::make_unique<messaging_client>();
    auto monitor = std::make_unique<health_monitor>();

    // Start all
    server->start();
    client->connect("localhost", 8080);
    monitor->start(std::chrono::seconds(1));

    // Verify statistics
    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.io_pools_created, 3);
    EXPECT_EQ(stats.total_active_pools, 3);

    // Stop all
    monitor->stop();
    client->disconnect();
    server->stop();

    // Verify cleanup
    stats = mgr.get_statistics();
    EXPECT_EQ(stats.total_active_pools, 0);

    mgr.shutdown();
}
```

---

## 📈 Performance Benchmarks

### Baseline Metrics

Before migration, capture these metrics:

```cpp
// Benchmark: Component startup time
auto start_time = std::chrono::high_resolution_clock::now();
component->start();
auto end_time = std::chrono::high_resolution_clock::now();
auto startup_time = std::chrono::duration_cast<std::chrono::microseconds>(
    end_time - start_time).count();

// Benchmark: Component shutdown time
start_time = std::chrono::high_resolution_clock::now();
component->stop();
end_time = std::chrono::high_resolution_clock::now();
auto shutdown_time = std::chrono::duration_cast<std::chrono::microseconds>(
    end_time - start_time).count();

// Benchmark: Message throughput (for server/client components)
// Send N messages and measure time
```

### Target Metrics

After migration:
- **Startup time**: Within 10% of baseline
- **Shutdown time**: Within 10% of baseline (likely faster due to RAII)
- **Message throughput**: Within 5% of baseline
- **Memory usage**: Equal or less than baseline
- **Thread count**: Equal or less than baseline

---

## 🚨 Common Pitfalls

### 1. Forgetting to Initialize Manager

```cpp
// WRONG: Start component without initializing manager
component->start();  // Will fail!

// CORRECT: Initialize manager first
auto& mgr = thread_pool_manager::instance();
mgr.initialize();
component->start();
```

### 2. Holding io_executor During Destruction

```cpp
// WRONG: io_context destroyed before executor
~component() {
    io_context_.reset();  // Destroyed first
    io_executor_.reset(); // Tries to use destroyed io_context!
}

// CORRECT: Destroy executor first
~component() {
    io_executor_.reset();  // Destroyed first
    io_context_.reset();   // Safe
}

// BEST: Use default member destruction order (reverse declaration order)
class component {
private:
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<io_context_executor> io_executor_;  // Destroyed first
};
```

### 3. Not Stopping io_context Before Executor

```cpp
// SUBOPTIMAL: Executor stops io_context automatically, but explicit is better
void stop() {
    io_executor_.reset();  // Implicit io_context->stop()
}

// BETTER: Explicit control flow
void stop() {
    if (io_context_) {
        io_context_->stop();  // Explicit
    }
    io_executor_.reset();
}
```

### 4. Reusing Component Name

```cpp
// WRONG: Multiple components with same name
server1->start();  // Creates pool "messaging_server"
server2->start();  // Creates another pool "messaging_server" - confusing!

// CORRECT: Unique names
server1->start_with_name("messaging_server_1");
server2->start_with_name("messaging_server_2");
```

---

## 📝 Acceptance Criteria

Phase 2 is complete when:

### Functional Requirements

- ✅ All 11 components refactored
- ✅ No direct std::thread usage in src/
- ✅ All components use io_context_executor
- ✅ All components use thread_pool_manager
- ✅ Clean startup and shutdown for all components
- ✅ Existing public APIs unchanged

### Testing Requirements

- ✅ All existing unit tests pass
- ✅ New thread pool integration tests added
- ✅ All integration tests pass
- ✅ No memory leaks detected
- ✅ No race conditions detected
- ✅ Clean shutdown tests pass

### Performance Requirements

- ✅ Startup time within 10% of baseline
- ✅ Shutdown time within 10% of baseline
- ✅ Message throughput within 5% of baseline
- ✅ Memory usage equal or less
- ✅ Thread count equal or less

### Documentation Requirements

- ✅ All components have updated headers
- ✅ All components have updated implementation comments
- ✅ Migration patterns documented
- ✅ Common pitfalls documented
- ✅ Examples provided

---

## 🔗 Next Steps

After completing Phase 2:

1. **Verify All Tests**: Run full test suite
2. **Performance Benchmark**: Compare with baseline
3. **Code Review**: Review all changes
4. **Commit Changes**: Commit Phase 2 work
5. **Proceed to Phase 3**: [Data Pipeline Integration](PHASE3_PIPELINE.md)

---

## 📚 References

- [Phase 1: Foundation Infrastructure](PHASE1_FOUNDATION.md)
- [Phase 3: Pipeline Integration](PHASE3_PIPELINE.md)
- [thread_pool API](../../include/network_system/integration/thread_pool_manager.h)
- [io_context_executor API](../../include/network_system/integration/io_context_executor.h)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-03
**Status**: 🔲 Ready for Implementation
