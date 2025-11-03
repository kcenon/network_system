# Thread System Integration Plan

**Status**: 🚧 In Progress
**Created**: 2025-11-03
**Target Completion**: TBD
**Author**: Integration Team

---

## 📋 Overview

This document outlines the comprehensive plan to replace all direct `std::thread` usage in network_system with thread_system's `thread_pool` and `typed_thread_pool` implementations.

**Goal**: Eliminate direct thread management and leverage thread_system's advanced features for better resource management, monitoring, and priority-based task execution.

---

## 🎯 Integration Strategy

### Thread Pool Usage Policy

| Use Case | Pool Type | Size | Components |
|----------|-----------|------|------------|
| **I/O Event Loop** | `thread_pool` | 1 per component | All server/client components |
| **Data Pipeline** | `typed_thread_pool<pipeline_priority>` | Hardware cores | Encryption, compression, serialization |
| **Utility Tasks** | `thread_pool` | Hardware cores / 2 | Blocking I/O, background tasks |

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Network System                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  I/O Event Loop Thread Pools (size=1 each)                │  │
│  │                                                             │  │
│  │  messaging_server        → thread_pool (1 worker)          │  │
│  │  messaging_client        → thread_pool (1 worker)          │  │
│  │  messaging_udp_server    → thread_pool (1 worker)          │  │
│  │  messaging_udp_client    → thread_pool (1 worker)          │  │
│  │  messaging_ws_server     → thread_pool (1 worker)          │  │
│  │  messaging_ws_client     → thread_pool (1 worker)          │  │
│  │  secure_messaging_*      → thread_pool (1 worker)          │  │
│  │  health_monitor          → thread_pool (1 worker)          │  │
│  │                                                             │  │
│  │  Purpose: Execute io_context::run() for ASIO event loop   │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Data Pipeline Thread Pool (typed, priority-based)        │  │
│  │                                                             │  │
│  │  REALTIME (0)   → Real-time encryption                    │  │
│  │  HIGH (1)       → Important data processing               │  │
│  │  NORMAL (2)     → General compression, serialization      │  │
│  │  LOW (3)        → Background validation                   │  │
│  │  BACKGROUND (4) → Logging, statistics collection          │  │
│  │                                                             │  │
│  │  Purpose: CPU-intensive data transformation with priority │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Utility Thread Pool (general-purpose)                    │  │
│  │                                                             │  │
│  │  - memory_profiler sampling                                │  │
│  │  - reliable_udp_client retransmission                      │  │
│  │  - send_coroutine async operations                         │  │
│  │  - Blocking file I/O operations                            │  │
│  │                                                             │  │
│  │  Purpose: General async tasks not tied to I/O or pipeline │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
                              ↓
              Uses: kcenon::thread library
```

---

## 📝 Implementation Plan

### Phase 1: Foundation Infrastructure

#### 1.1 Thread Pool Manager

**File**: `include/network_system/integration/thread_pool_manager.h`

**Purpose**: Centralized manager for all thread pools in network_system

**Key Features**:
- Singleton pattern for global access
- Separate pools for I/O, pipeline, and utility tasks
- Component-specific I/O pool creation
- Resource monitoring and statistics

**API Design**:
```cpp
namespace network_system::integration {

enum class pipeline_priority : int {
    REALTIME = 0,      // Real-time encryption, urgent transmission
    HIGH = 1,          // Important data processing
    NORMAL = 2,        // General compression, serialization
    LOW = 3,           // Background validation
    BACKGROUND = 4     // Logging, statistics
};

class thread_pool_manager {
public:
    static thread_pool_manager& instance();

    void initialize(
        size_t io_pool_count = 10,
        size_t pipeline_workers = std::thread::hardware_concurrency(),
        size_t utility_workers = std::thread::hardware_concurrency() / 2
    );

    void shutdown();

    // Create dedicated I/O pool for each component (size=1)
    std::shared_ptr<kcenon::thread::thread_pool>
    create_io_pool(const std::string& component_name);

    // Get shared typed pipeline pool
    std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>>
    get_pipeline_pool();

    // Get shared utility pool
    std::shared_ptr<kcenon::thread::thread_pool>
    get_utility_pool();

    struct statistics {
        size_t total_io_pools = 0;
        size_t active_io_tasks = 0;
        size_t pipeline_queue_size = 0;
        size_t utility_queue_size = 0;
    };

    statistics get_statistics() const;
};

} // namespace network_system::integration
```

**Implementation File**: `src/integration/thread_pool_manager.cpp`

---

#### 1.2 I/O Context Executor Wrapper

**File**: `include/network_system/integration/io_context_executor.h`

**Purpose**: RAII wrapper for executing `io_context::run()` in thread_pool

**Key Features**:
- Automatic submission of io_context::run() to thread pool
- Clean shutdown handling
- Exception safety
- Lifecycle management

**API Design**:
```cpp
namespace network_system::integration {

class io_context_executor {
public:
    io_context_executor(
        std::shared_ptr<kcenon::thread::thread_pool> pool,
        asio::io_context& io_context,
        const std::string& component_name
    );

    ~io_context_executor();

    // No copy, only move
    io_context_executor(const io_context_executor&) = delete;
    io_context_executor& operator=(const io_context_executor&) = delete;
    io_context_executor(io_context_executor&&) noexcept = default;
    io_context_executor& operator=(io_context_executor&&) noexcept = default;

    void start();
    void stop();
    bool is_running() const;

    // Get the underlying thread pool
    std::shared_ptr<kcenon::thread::thread_pool> get_pool() const;
};

} // namespace network_system::integration
```

**Implementation File**: `src/integration/io_context_executor.cpp`

---

#### 1.3 Pipeline Job Definitions

**File**: `include/network_system/internal/pipeline_jobs.h`

**Purpose**: Typed jobs for data pipeline with priority support

**Key Features**:
- Priority-based job classes
- Type-safe job submission
- Future-based async results
- Helper utilities for common operations

**API Design**:
```cpp
namespace network_system::internal {

// Base interface
class pipeline_job_base {
public:
    virtual ~pipeline_job_base() = default;
    virtual void execute() = 0;
    virtual integration::pipeline_priority get_priority() const = 0;
};

// Concrete job types
class encryption_job : public pipeline_job_base {
public:
    encryption_job(
        std::vector<uint8_t> data,
        std::function<void(std::vector<uint8_t>)> callback
    );

    void execute() override;
    integration::pipeline_priority get_priority() const override {
        return integration::pipeline_priority::HIGH;
    }
};

class compression_job : public pipeline_job_base {
public:
    compression_job(
        std::vector<uint8_t> data,
        std::function<void(std::vector<uint8_t>)> callback
    );

    void execute() override;
    integration::pipeline_priority get_priority() const override {
        return integration::pipeline_priority::NORMAL;
    }
};

class serialization_job : public pipeline_job_base {
public:
    serialization_job(
        std::any data,
        std::function<void(std::vector<uint8_t>)> callback
    );

    void execute() override;
    integration::pipeline_priority get_priority() const override {
        return integration::pipeline_priority::LOW;
    }
};

// Convenience submitter
class pipeline_submitter {
public:
    static std::future<std::vector<uint8_t>>
    submit_encryption(std::vector<uint8_t> data);

    static std::future<std::vector<uint8_t>>
    submit_compression(std::vector<uint8_t> data);

    static std::future<std::vector<uint8_t>>
    submit_serialization(std::any data);
};

} // namespace network_system::internal
```

**Implementation File**: `src/internal/pipeline_jobs.cpp`

---

### Phase 2: Core Component Refactoring

#### 2.1 messaging_server

**Current Implementation** (`src/core/messaging_server.cpp:100`):
```cpp
server_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (...) {
            // Handle exception
        }
    }
);
```

**New Implementation**:
```cpp
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"

// In start_server()
auto& pool_mgr = thread_pool_manager::instance();
auto io_pool = pool_mgr.create_io_pool("messaging_server:" + server_id_);

io_executor_ = std::make_unique<io_context_executor>(
    io_pool,
    *io_context_,
    "messaging_server:" + server_id_
);

io_executor_->start();

NETWORK_LOG_INFO("[messaging_server] Started with thread_pool execution");
```

**Header Changes** (`include/network_system/core/messaging_server.h`):
```cpp
// Remove:
// std::unique_ptr<std::thread> server_thread_;

// Add:
#include "network_system/integration/io_context_executor.h"
std::unique_ptr<integration::io_context_executor> io_executor_;
```

**Stop Logic**:
```cpp
auto messaging_server::stop_server() -> VoidResult {
    if (!is_running_.load()) {
        return ok();
    }

    // Stop io_executor (will stop io_context and wait for thread pool)
    if (io_executor_) {
        io_executor_->stop();
        io_executor_.reset();
    }

    is_running_.store(false);
    return ok();
}
```

---

#### 2.2 messaging_client

**Current Implementation** (`src/core/messaging_client.cpp:143`):
```cpp
client_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (...) {
            // Handle exception
        }
    }
);
```

**New Implementation**:
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto io_pool = pool_mgr.create_io_pool("messaging_client:" + client_id_);

io_executor_ = std::make_unique<io_context_executor>(
    io_pool,
    *io_context_,
    "messaging_client:" + client_id_
);

io_executor_->start();
```

**Header Changes** (`include/network_system/core/messaging_client.h:273`):
```cpp
// Remove:
// std::unique_ptr<std::thread> client_thread_;

// Add:
std::unique_ptr<integration::io_context_executor> io_executor_;
```

---

#### 2.3 messaging_udp_server

**Current Implementation** (`src/core/messaging_udp_server.cpp:87`):
```cpp
worker_thread_ = std::thread(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR(std::string("Worker thread exception: ") + e.what());
        }
    }
);
```

**New Implementation**:
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto io_pool = pool_mgr.create_io_pool("messaging_udp_server:" + server_id_);

io_executor_ = std::make_unique<io_context_executor>(
    io_pool,
    *io_context_,
    "messaging_udp_server:" + server_id_
);

io_executor_->start();
```

**Header Changes** (`include/network_system/core/messaging_udp_server.h:191`):
```cpp
// Remove:
// std::thread worker_thread_;

// Add:
std::unique_ptr<integration::io_context_executor> io_executor_;
```

**Stop Logic**:
```cpp
auto messaging_udp_server::stop_server() -> VoidResult {
    if (!is_running_.load()) {
        return ok();
    }

    // Stop socket operations first
    if (socket_) {
        socket_->close();
    }

    // Stop io_executor
    if (io_executor_) {
        io_executor_->stop();
        io_executor_.reset();
    }

    is_running_.store(false);
    return ok();
}
```

---

#### 2.4 messaging_udp_client

**Current Implementation** (`src/core/messaging_udp_client.cpp:116`):
```cpp
worker_thread_ = std::thread(
    [this]() {
        try {
            io_context_->run();
        } catch (...) {
            // Handle exception
        }
    }
);
```

**New Implementation**: Same pattern as messaging_udp_server

**Header Changes** (`include/network_system/core/messaging_udp_client.h:197`):
```cpp
// Remove:
// std::thread worker_thread_;

// Add:
std::unique_ptr<integration::io_context_executor> io_executor_;
```

---

#### 2.5 messaging_ws_client

**Current Implementation** (`src/core/messaging_ws_client.cpp:103`):
```cpp
io_thread_ = std::make_unique<std::thread>([this]() {
    io_context_->run();
});
```

**New Implementation**:
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto io_pool = pool_mgr.create_io_pool("messaging_ws_client:" + client_id_);

io_executor_ = std::make_unique<io_context_executor>(
    io_pool,
    *io_context_,
    "messaging_ws_client:" + client_id_
);

io_executor_->start();
```

**Header Changes** (in pimpl class):
```cpp
// Remove:
// std::unique_ptr<std::thread> io_thread_;

// Add:
std::unique_ptr<integration::io_context_executor> io_executor_;
```

---

#### 2.6 messaging_ws_server

**Current Implementation** (`src/core/messaging_ws_server.cpp:225`):
```cpp
io_thread_ = std::make_unique<std::thread>([this]() {
    try {
        io_context_->run();
    } catch (...) {
        // Handle exception
    }
});
```

**New Implementation**: Same pattern as messaging_ws_client

---

#### 2.7 secure_messaging_server

**Current Implementation** (`src/core/secure_messaging_server.cpp:141`):
```cpp
server_thread_ = std::make_unique<std::thread>(
    [this]() { io_context_->run(); }
);
```

**New Implementation**: Same pattern as messaging_server

---

#### 2.8 secure_messaging_client

**Current Implementation** (`src/core/secure_messaging_client.cpp:172`):
```cpp
client_thread_ = std::make_unique<std::thread>(
    [this]() { io_context_->run(); }
);
```

**New Implementation**: Same pattern as messaging_client

---

#### 2.9 health_monitor

**Current Implementation** (`src/utils/health_monitor.cpp:106`):
```cpp
monitor_thread_ = std::make_unique<std::thread>(
    [this]() {
        try {
            io_context_->run();
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("[health_monitor] Exception: " +
                std::string(e.what()));
        }
    }
);
```

**New Implementation**:
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto io_pool = pool_mgr.create_io_pool(
    "health_monitor:" + client_->get_client_id()
);

io_executor_ = std::make_unique<io_context_executor>(
    io_pool,
    *io_context_,
    "health_monitor"
);

io_executor_->start();
```

**Header Changes** (`include/network_system/utils/health_monitor.h:184`):
```cpp
// Remove:
// std::unique_ptr<std::thread> monitor_thread_;

// Add:
std::unique_ptr<integration::io_context_executor> io_executor_;
```

---

#### 2.10 memory_profiler

**Current Implementation** (`src/utils/memory_profiler.cpp:32`):
```cpp
worker_ = std::thread(&memory_profiler::sampler_loop, this, interval);
```

**New Implementation** (uses utility pool):
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto utility_pool = pool_mgr.get_utility_pool();

sampling_future_ = utility_pool->submit([this, interval]() {
    sampler_loop(interval);
});
```

**Header Changes** (`include/network_system/utils/memory_profiler.h:63`):
```cpp
// Remove:
// std::thread worker_{};

// Add:
std::future<void> sampling_future_;
std::atomic<bool> stop_requested_{false};
```

**Sampler Loop Update**:
```cpp
void memory_profiler::sampler_loop(std::chrono::milliseconds interval) {
    while (!stop_requested_.load()) {
        // Sample memory
        record_sample();

        // Sleep (check stop flag periodically)
        auto end_time = std::chrono::steady_clock::now() + interval;
        while (std::chrono::steady_clock::now() < end_time) {
            if (stop_requested_.load()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
```

---

#### 2.11 reliable_udp_client

**Current Implementation** (`src/core/reliable_udp_client.cpp:574`):
```cpp
retransmission_thread_ = std::thread([this]() {
    // Retransmission loop
});
```

**New Implementation** (uses utility pool):
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto utility_pool = pool_mgr.get_utility_pool();

retransmission_future_ = utility_pool->submit([this]() {
    retransmission_loop();
});
```

**Header Changes**:
```cpp
// Remove:
// std::thread retransmission_thread_;

// Add:
std::future<void> retransmission_future_;
std::atomic<bool> stop_retransmission_{false};
```

---

### Phase 3: Data Pipeline Integration

#### 3.1 Pipeline Component

**File**: `src/internal/pipeline.cpp`

**Current**: Synchronous or std::async processing

**New Implementation**:
```cpp
#include "network_system/internal/pipeline_jobs.h"

auto pipeline::process(std::vector<uint8_t> data) -> std::future<std::vector<uint8_t>> {
    // Create processing chain using typed_thread_pool

    if (encrypt_mode_) {
        auto encrypted = pipeline_submitter::submit_encryption(std::move(data));
        data = encrypted.get();
    }

    if (compress_mode_) {
        auto compressed = pipeline_submitter::submit_compression(std::move(data));
        data = compressed.get();
    }

    // Return as future for async continuation
    std::promise<std::vector<uint8_t>> promise;
    promise.set_value(std::move(data));
    return promise.get_future();
}
```

**Async Version**:
```cpp
auto pipeline::process_async(std::vector<uint8_t> data)
    -> std::future<std::vector<uint8_t>> {

    return std::async(std::launch::async, [this, data = std::move(data)]() mutable {
        if (encrypt_mode_) {
            auto encrypted = pipeline_submitter::submit_encryption(std::move(data));
            data = encrypted.get();
        }

        if (compress_mode_) {
            auto compressed = pipeline_submitter::submit_compression(std::move(data));
            data = compressed.get();
        }

        return data;
    });
}
```

---

#### 3.2 send_coroutine

**Current Implementation** (`src/internal/send_coroutine.cpp:112`):
```cpp
std::thread([sock, promise, future = std::move(future_processed)]() mutable {
    try {
        // Wait for processing
        auto processed_data = future.get();

        // Send data
        // ...

        promise.set_value();
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
}).detach();
```

**New Implementation**:
```cpp
auto& pool_mgr = thread_pool_manager::instance();
auto utility_pool = pool_mgr.get_utility_pool();

utility_pool->submit([sock, promise, future = std::move(future_processed)]() mutable {
    try {
        auto processed_data = future.get();
        // Send data
        // ...
        promise.set_value();
    } catch (...) {
        promise.set_exception(std::current_exception());
    }
});
```

---

### Phase 4: Integration Layer Updates

#### 4.1 thread_integration.cpp Refactoring

**File**: `src/integration/thread_integration.cpp`

**Current**: basic_thread_pool (custom implementation)

**New**: Direct delegation to thread_system

```cpp
#include <kcenon/thread/core/thread_pool.h>

namespace network_system::integration {

// Remove basic_thread_pool::impl completely

// Replace with adapter
class thread_system_adapter : public thread_pool_interface {
public:
    explicit thread_system_adapter(
        std::shared_ptr<kcenon::thread::thread_pool> pool
    ) : pool_(std::move(pool)) {}

    std::future<void> submit(std::function<void()> task) override {
        return pool_->submit(std::move(task));
    }

    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) override {
        // Use utility pool for delayed execution
        return pool_->submit([task = std::move(task), delay]() {
            std::this_thread::sleep_for(delay);
            task();
        });
    }

    size_t worker_count() const override {
        return pool_->get_thread_count();
    }

    bool is_running() const override {
        return pool_->is_started();
    }

    size_t pending_tasks() const override {
        return pool_->get_pending_job_count();
    }

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
};

// Update thread_integration_manager to use thread_pool_manager
class thread_integration_manager::impl {
public:
    impl() {
        // Get utility pool from thread_pool_manager
        auto& mgr = thread_pool_manager::instance();
        auto pool = mgr.get_utility_pool();
        default_pool_ = std::make_shared<thread_system_adapter>(pool);
    }

    std::shared_ptr<thread_pool_interface> default_pool_;
    std::shared_ptr<thread_pool_interface> custom_pool_;
};

} // namespace network_system::integration
```

---

#### 4.2 thread_system_adapter.cpp

**File**: `src/integration/thread_system_adapter.cpp`

**Purpose**: Provide direct integration with thread_system for advanced use cases

```cpp
#include "network_system/integration/thread_system_adapter.h"
#include <kcenon/thread/core/thread_pool.h>

namespace network_system::integration {

#ifdef BUILD_WITH_THREAD_SYSTEM

class thread_system_adapter::impl {
public:
    impl(std::shared_ptr<kcenon::thread::thread_pool> pool)
        : pool_(std::move(pool)) {}

    std::shared_ptr<kcenon::thread::thread_pool> pool_;
};

thread_system_adapter::thread_system_adapter(
    std::shared_ptr<kcenon::thread::thread_pool> pool
) : pimpl_(std::make_unique<impl>(std::move(pool))) {}

thread_system_adapter::~thread_system_adapter() = default;

std::future<void> thread_system_adapter::submit(std::function<void()> task) {
    return pimpl_->pool_->submit(std::move(task));
}

// ... other implementations

#endif // BUILD_WITH_THREAD_SYSTEM

} // namespace network_system::integration
```

---

### Phase 5: Build System Updates

#### 5.1 CMakeLists.txt Changes

**Main CMakeLists.txt**:
```cmake
# Make thread_system a REQUIRED dependency
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration" ON)

if(NOT BUILD_WITH_THREAD_SYSTEM)
    message(FATAL_ERROR
        "thread_system is now REQUIRED for network_system. "
        "Cannot build without thread_system.")
endif()

# Find thread_system package
find_package(ThreadSystem REQUIRED)

if(NOT ThreadSystem_FOUND)
    message(FATAL_ERROR
        "thread_system not found. Please ensure thread_system is built and installed.")
endif()

# Add new integration sources
target_sources(NetworkSystem PRIVATE
    src/integration/thread_pool_manager.cpp
    src/integration/io_context_executor.cpp
    src/internal/pipeline_jobs.cpp
)

# Link thread_system library
target_link_libraries(NetworkSystem
    PRIVATE
        ThreadSystem::ThreadBase
)

# Add thread_system include directories
target_include_directories(NetworkSystem
    PRIVATE
        ${THREAD_SYSTEM_INCLUDE_DIR}
)

message(STATUS "thread_system integration: REQUIRED")
message(STATUS "  thread_system location: ${THREAD_SYSTEM_INCLUDE_DIR}")
```

---

#### 5.2 NetworkSystemIntegration.cmake Updates

**File**: `cmake/NetworkSystemIntegration.cmake`

```cmake
##################################################
# Configure mandatory thread_system integration
##################################################
function(setup_thread_system_integration target)
    # thread_system is now mandatory
    if(NOT THREAD_SYSTEM_INCLUDE_DIR)
        message(FATAL_ERROR "thread_system include directory not found")
    endif()

    target_include_directories(${target} PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})

    if(THREAD_SYSTEM_LIBRARY)
        target_link_libraries(${target} PUBLIC ${THREAD_SYSTEM_LIBRARY})
    endif()

    target_compile_definitions(${target} PRIVATE BUILD_WITH_THREAD_SYSTEM)
    message(STATUS "Configured ${target} with thread_system (MANDATORY)")
endfunction()

# Update setup_network_system_integrations
function(setup_network_system_integrations target)
    message(STATUS "Setting up integrations for ${target}...")

    setup_asio_integration(${target})
    setup_fmt_integration(${target})
    setup_thread_system_integration(${target})  # Now called unconditionally
    setup_container_system_integration(${target})
    setup_logger_system_integration(${target})
    setup_monitoring_system_integration(${target})
    setup_common_system_integration(${target})

    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32 mswsock)
    endif()

    if(NOT WIN32)
        target_link_libraries(${target} PUBLIC pthread)
    endif()

    message(STATUS "Integration setup complete for ${target}")
endfunction()
```

---

#### 5.3 Integration Tests CMakeLists.txt

**File**: `integration_tests/CMakeLists.txt`

```cmake
# Ensure thread_system is available for integration tests
if(NOT BUILD_WITH_THREAD_SYSTEM)
    message(WARNING "Integration tests require thread_system. Skipping.")
    return()
endif()

# Add thread_system to all integration test targets
foreach(test_target ${INTEGRATION_TEST_TARGETS})
    target_include_directories(${test_target} PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
    target_link_libraries(${test_target} PRIVATE ${THREAD_SYSTEM_LIBRARY})
    target_compile_definitions(${test_target} PRIVATE BUILD_WITH_THREAD_SYSTEM)
endforeach()
```

---

## 🧪 Testing Strategy

### Unit Tests

**File**: `tests/thread_system_integration_tests.cpp`

```cpp
#include <gtest/gtest.h>
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"

using namespace network_system::integration;

TEST(ThreadPoolManager, Initialization) {
    auto& mgr = thread_pool_manager::instance();

    ASSERT_NO_THROW(mgr.initialize());

    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.total_io_pools, 0);

    mgr.shutdown();
}

TEST(ThreadPoolManager, CreateIoPool) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    auto pool = mgr.create_io_pool("test_server");
    ASSERT_NE(pool, nullptr);

    // Pool should have exactly 1 worker for I/O
    EXPECT_EQ(pool->get_thread_count(), 1);

    mgr.shutdown();
}

TEST(IoContextExecutor, BasicExecution) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    asio::io_context io_ctx;
    auto pool = mgr.create_io_pool("test");

    io_context_executor executor(pool, io_ctx, "test");

    std::atomic<bool> executed{false};
    io_ctx.post([&executed]() {
        executed.store(true);
    });

    executor.start();

    // Wait a bit for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    executor.stop();

    EXPECT_TRUE(executed.load());

    mgr.shutdown();
}

TEST(PipelineJobs, PriorityOrdering) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    auto pipeline_pool = mgr.get_pipeline_pool();
    ASSERT_NE(pipeline_pool, nullptr);

    std::vector<int> execution_order;
    std::mutex order_mutex;

    // Submit jobs with different priorities
    // HIGH priority should execute before NORMAL

    // ... test implementation

    mgr.shutdown();
}
```

---

### Integration Tests

**Verify Existing Tests Still Pass**:
```bash
cd build
ctest -R integration_tests --output-on-failure
```

**Expected Results**:
- ✅ connection_lifecycle_test: All tests pass
- ✅ protocol_integration_test: All tests pass
- ✅ network_performance_test: Performance similar or better
- ✅ error_handling_test: All tests pass
- ✅ tcp_load_test: Throughput maintained or improved
- ✅ udp_load_test: Latency maintained or improved
- ✅ websocket_load_test: All tests pass

---

### Performance Benchmarks

**File**: `benchmarks/thread_integration_bench.cpp`

```cpp
#include <benchmark/benchmark.h>
#include "network_system/integration/thread_pool_manager.h"

// Benchmark thread pool overhead vs std::thread
static void BM_ThreadPoolSubmit(benchmark::State& state) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();
    auto pool = mgr.get_utility_pool();

    for (auto _ : state) {
        auto future = pool->submit([]() { /* no-op */ });
        future.get();
    }

    mgr.shutdown();
}
BENCHMARK(BM_ThreadPoolSubmit);

static void BM_DirectThreadCreate(benchmark::State& state) {
    for (auto _ : state) {
        std::thread t([]() { /* no-op */ });
        t.join();
    }
}
BENCHMARK(BM_DirectThreadCreate);

// Benchmark I/O context execution
static void BM_IoContextWithThreadPool(benchmark::State& state) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    for (auto _ : state) {
        asio::io_context io_ctx;
        auto pool = mgr.create_io_pool("bench");
        io_context_executor executor(pool, io_ctx, "bench");

        io_ctx.post([]() { /* no-op */ });

        executor.start();
        executor.stop();
    }

    mgr.shutdown();
}
BENCHMARK(BM_IoContextWithThreadPool);

BENCHMARK_MAIN();
```

**Expected Performance**:
| Metric | Before (std::thread) | After (thread_pool) | Change |
|--------|---------------------|---------------------|--------|
| Thread creation overhead | ~50-100μs | ~1-5μs | ✅ 10-50x faster |
| Memory per thread | ~2-8MB | ~0 (reuse) | ✅ Reduced |
| Context switches | High | Lower | ✅ Improved |
| I/O latency | Baseline | ≈ Same | ≈ Neutral |
| Pipeline throughput | Baseline | +10-20% | ✅ Improved |

---

## 📊 Migration Checklist

### Phase 1: Infrastructure ✅
- [ ] Implement `thread_pool_manager.h`
- [ ] Implement `thread_pool_manager.cpp`
- [ ] Implement `io_context_executor.h`
- [ ] Implement `io_context_executor.cpp`
- [ ] Implement `pipeline_jobs.h`
- [ ] Implement `pipeline_jobs.cpp`
- [ ] Write unit tests for infrastructure

### Phase 2: Core Components
- [ ] Refactor `messaging_server`
- [ ] Refactor `messaging_client`
- [ ] Refactor `messaging_udp_server`
- [ ] Refactor `messaging_udp_client`
- [ ] Refactor `messaging_ws_server`
- [ ] Refactor `messaging_ws_client`
- [ ] Refactor `secure_messaging_server`
- [ ] Refactor `secure_messaging_client`
- [ ] Update `health_monitor`
- [ ] Update `memory_profiler`
- [ ] Update `reliable_udp_client`

### Phase 3: Pipeline
- [ ] Integrate `pipeline.cpp` with typed_thread_pool
- [ ] Update `send_coroutine.cpp`
- [ ] Implement encryption jobs
- [ ] Implement compression jobs
- [ ] Implement serialization jobs

### Phase 4: Integration Layer
- [ ] Remove `basic_thread_pool` implementation
- [ ] Update `thread_integration.cpp`
- [ ] Update `thread_system_adapter.cpp`

### Phase 5: Build System
- [ ] Update main CMakeLists.txt
- [ ] Update NetworkSystemIntegration.cmake
- [ ] Update integration tests CMakeLists.txt
- [ ] Update samples CMakeLists.txt

### Phase 6: Testing
- [ ] Run all unit tests
- [ ] Run all integration tests
- [ ] Run performance benchmarks
- [ ] Run stress tests
- [ ] Memory leak detection
- [ ] Thread safety validation

### Phase 7: Documentation
- [ ] Update README.md
- [ ] Update API_REFERENCE.md
- [ ] Update MIGRATION.md
- [ ] Update BUILD.md
- [ ] Create migration examples
- [ ] Document performance improvements

---

## 📈 Success Criteria

### Functional Requirements
- ✅ All existing tests pass without modification
- ✅ No direct `std::thread` usage remains in src/ directory
- ✅ All components use thread_pool_manager
- ✅ Data pipeline uses typed_thread_pool with priorities

### Non-Functional Requirements
- ✅ Performance: Within 5% of baseline (preferably better)
- ✅ Memory: No memory leaks detected
- ✅ Thread Safety: No race conditions in thread pool usage
- ✅ Code Coverage: Maintain or improve coverage (>80%)

### Documentation Requirements
- ✅ All new APIs documented
- ✅ Migration guide complete
- ✅ Examples provided
- ✅ Integration document removed (this file)

---

## 🚫 Removal Checklist

**This document will be removed after successful integration. Before removal, ensure:**

- [ ] All code changes merged to main branch
- [ ] All tests passing on CI
- [ ] Documentation updated and reviewed
- [ ] Performance benchmarks meet criteria
- [ ] Migration guide published
- [ ] Team training completed (if applicable)

**Final Cleanup**:
```bash
# After successful integration
git rm THREAD_SYSTEM_INTEGRATION.md
git commit -m "chore: remove integration document after successful thread_system integration"
```

---

## 📞 Support

### Issues During Integration

If you encounter issues during integration:

1. **Check thread_system documentation**: `/Users/dongcheolshin/Sources/thread_system/README.md`
2. **Review thread_system examples**: `/Users/dongcheolshin/Sources/thread_system/samples/`
3. **Check thread_system tests**: `/Users/dongcheolshin/Sources/thread_system/tests/`
4. **Consult logger_system integration** for reference patterns

### Common Issues

**Issue**: Thread pool not starting
- **Solution**: Ensure `initialize()` called before use
- **Check**: Thread pool has workers configured

**Issue**: I/O context not running
- **Solution**: Verify io_executor started properly
- **Check**: io_context has work to do

**Issue**: Pipeline jobs not executing
- **Solution**: Check typed_thread_pool is started
- **Check**: Jobs submitted with valid priority

**Issue**: Performance degradation
- **Solution**: Review thread pool sizes
- **Check**: No excessive context switching

---

## 🎯 Next Steps

1. **Start with Phase 1**: Implement infrastructure components
2. **Test incrementally**: Each component as it's refactored
3. **Monitor performance**: Benchmark at each phase
4. **Document learnings**: Update this doc with findings
5. **Cleanup**: Remove this doc after completion

---

**Document Version**: 1.0
**Last Updated**: 2025-11-03
**Status**: 🚧 Ready to Begin Implementation
