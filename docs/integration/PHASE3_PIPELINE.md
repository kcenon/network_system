# Phase 3: Data Pipeline Integration

**Status**: 🔲 Not Started
**Dependencies**: Phase 1 (Foundation Infrastructure)
**Estimated Time**: 1-2 days
**Components**: Data pipeline, send coroutine, pipeline jobs

---

## 📋 Overview

Phase 3 focuses on integrating `typed_thread_pool` for the data processing pipeline. The pipeline performs CPU-intensive operations like encryption, compression, and serialization. Using a typed thread pool with priority support ensures critical operations are processed before less important ones.

### Goals

1. **Priority-Based Processing**: High-priority data processed before low-priority
2. **Eliminate std::async Usage**: Replace with typed_thread_pool
3. **Better Resource Management**: Centralized thread pool for all pipeline operations
4. **Improved Monitoring**: Track pipeline performance and bottlenecks
5. **Flexible Scheduling**: Dynamic priority adjustment based on system load

### Success Criteria

- ✅ All pipeline operations use typed_thread_pool
- ✅ Priority-based job scheduling working
- ✅ Pipeline throughput maintained or improved
- ✅ All existing tests pass
- ✅ Performance within 5% of baseline

---

## 🎯 Priority System Design

### Priority Levels

```cpp
enum class pipeline_priority : int {
    REALTIME = 0,      // Real-time encryption, urgent transmission
    HIGH = 1,          // Important data processing
    NORMAL = 2,        // General compression, serialization
    LOW = 3,           // Background validation
    BACKGROUND = 4     // Logging, statistics
};
```

### Priority Assignment Rules

| Operation | Priority | Rationale |
|-----------|----------|-----------|
| **Critical Encryption** | REALTIME | Security-sensitive, time-critical |
| **Urgent Send** | REALTIME | Network transmission deadlines |
| **Normal Encryption** | HIGH | Security operations take precedence |
| **Compression** | NORMAL | Standard data processing |
| **Serialization** | NORMAL | Standard data processing |
| **Validation** | LOW | Can be delayed |
| **Logging** | BACKGROUND | Non-critical, async |
| **Statistics** | BACKGROUND | Non-critical, async |

---

## 🏗️ Architecture

### Current Pipeline Flow

```
┌──────────────────────────────────────────────────────────┐
│                    pipeline.cpp                          │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  1. process_data()                                       │
│      ├─> std::async(serialize)                          │
│      ├─> std::async(compress)                           │
│      └─> std::async(encrypt)                            │
│                                                           │
│  2. send_data()                                          │
│      └─> std::thread (detached)                         │
│                                                           │
└──────────────────────────────────────────────────────────┘
```

**Problems**:
- No priority control
- Unbounded thread creation with std::async
- No centralized monitoring
- Complex error handling

### New Pipeline Flow

```
┌──────────────────────────────────────────────────────────┐
│                    pipeline.cpp                          │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  1. process_data()                                       │
│      ├─> pipeline_pool->submit(serialize, NORMAL)       │
│      ├─> pipeline_pool->submit(compress, NORMAL)        │
│      └─> pipeline_pool->submit(encrypt, HIGH)           │
│                                                           │
│  2. send_data()                                          │
│      └─> utility_pool->submit(send_task)                │
│                                                           │
└──────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────┐
│              thread_pool_manager                         │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Pipeline Pool (typed_thread_pool)               │   │
│  │  - REALTIME queue                                │   │
│  │  - HIGH queue                                    │   │
│  │  - NORMAL queue                                  │   │
│  │  - LOW queue                                     │   │
│  │  - BACKGROUND queue                              │   │
│  └──────────────────────────────────────────────────┘   │
│                                                           │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Utility Pool (thread_pool)                      │   │
│  │  - Network sends                                 │   │
│  │  - Blocking I/O                                  │   │
│  └──────────────────────────────────────────────────┘   │
│                                                           │
└──────────────────────────────────────────────────────────┘
```

---

## 📝 Component Refactoring

### 1. pipeline.cpp - Core Pipeline

**File**: `src/network/pipeline.cpp`
**Complexity**: ⭐⭐⭐ (High)

#### Current Implementation

```cpp
// Current pipeline.cpp (simplified)
class pipeline {
public:
    std::future<std::vector<uint8_t>> process_data(
        const std::vector<uint8_t>& data) {

        // Step 1: Serialize
        auto serialized = std::async(std::launch::async, [data]() {
            return serialize_impl(data);
        });

        // Step 2: Compress
        auto compressed = std::async(std::launch::async, [&serialized]() {
            return compress_impl(serialized.get());
        });

        // Step 3: Encrypt
        return std::async(std::launch::async, [&compressed]() {
            return encrypt_impl(compressed.get());
        });
    }
};
```

**Problems**:
- std::async creates unbounded threads
- No priority control
- Sequential dependencies make parallel execution difficult
- Error propagation complex

#### Refactored Implementation

**Header Changes** (`include/network_system/network/pipeline.h`):

```cpp
#pragma once

#include "network_system/integration/thread_pool_manager.h"
#include <vector>
#include <future>
#include <cstdint>

namespace network_system {

/**
 * @brief Data processing pipeline with priority-based scheduling
 *
 * The pipeline performs data transformation operations (serialization,
 * compression, encryption) using a typed thread pool with priority support.
 */
class pipeline {
public:
    /**
     * @brief Constructs pipeline with specified priorities
     *
     * @param serialize_priority Priority for serialization (default: NORMAL)
     * @param compress_priority Priority for compression (default: NORMAL)
     * @param encrypt_priority Priority for encryption (default: HIGH)
     */
    explicit pipeline(
        pipeline_priority serialize_priority = pipeline_priority::NORMAL,
        pipeline_priority compress_priority = pipeline_priority::NORMAL,
        pipeline_priority encrypt_priority = pipeline_priority::HIGH);

    /**
     * @brief Process data through the pipeline
     *
     * @param data Input data to process
     * @return Future containing processed data
     *
     * The pipeline performs these operations in sequence:
     * 1. Serialize (with serialize_priority)
     * 2. Compress (with compress_priority)
     * 3. Encrypt (with encrypt_priority)
     */
    std::future<std::vector<uint8_t>> process_data(
        const std::vector<uint8_t>& data);

    /**
     * @brief Process data with custom priority
     *
     * @param data Input data to process
     * @param priority Override priority for all operations
     * @return Future containing processed data
     */
    std::future<std::vector<uint8_t>> process_data_with_priority(
        const std::vector<uint8_t>& data,
        pipeline_priority priority);

    /**
     * @brief Set default priorities for pipeline operations
     */
    void set_priorities(pipeline_priority serialize_priority,
                       pipeline_priority compress_priority,
                       pipeline_priority encrypt_priority);

private:
    // Pipeline operation implementations
    std::vector<uint8_t> serialize_impl(const std::vector<uint8_t>& data);
    std::vector<uint8_t> compress_impl(const std::vector<uint8_t>& data);
    std::vector<uint8_t> encrypt_impl(const std::vector<uint8_t>& data);

    // Default priorities
    pipeline_priority serialize_priority_;
    pipeline_priority compress_priority_;
    pipeline_priority encrypt_priority_;
};

} // namespace network_system
```

**Implementation** (`src/network/pipeline.cpp`):

```cpp
#include "network_system/network/pipeline.h"
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/logger_integration.h"
#include <kcenon/thread/typed_job.h>

namespace network_system {

pipeline::pipeline(pipeline_priority serialize_priority,
                  pipeline_priority compress_priority,
                  pipeline_priority encrypt_priority)
    : serialize_priority_(serialize_priority)
    , compress_priority_(compress_priority)
    , encrypt_priority_(encrypt_priority) {
}

std::future<std::vector<uint8_t>> pipeline::process_data(
    const std::vector<uint8_t>& data) {

    // Get pipeline pool from manager
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.get_pipeline_pool();

    if (!pool) {
        NETWORK_LOG_ERROR("[pipeline] Pipeline pool not initialized");
        throw std::runtime_error("Pipeline pool not available");
    }

    // Create promise for final result
    auto result_promise = std::make_shared<std::promise<std::vector<uint8_t>>>();
    auto result_future = result_promise->get_future();

    // Step 1: Submit serialization job
    auto serialize_job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
        serialize_priority_,
        [this, data, pool, result_promise]() -> std::vector<uint8_t> {
            try {
                NETWORK_LOG_DEBUG("[pipeline] Starting serialization");
                auto serialized = serialize_impl(data);

                // Step 2: Submit compression job (chained)
                auto compress_job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
                    compress_priority_,
                    [this, serialized = std::move(serialized), pool, result_promise]() -> std::vector<uint8_t> {
                        try {
                            NETWORK_LOG_DEBUG("[pipeline] Starting compression");
                            auto compressed = compress_impl(serialized);

                            // Step 3: Submit encryption job (chained)
                            auto encrypt_job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
                                encrypt_priority_,
                                [this, compressed = std::move(compressed), result_promise]() {
                                    try {
                                        NETWORK_LOG_DEBUG("[pipeline] Starting encryption");
                                        auto encrypted = encrypt_impl(compressed);
                                        result_promise->set_value(std::move(encrypted));
                                    }
                                    catch (const std::exception& e) {
                                        NETWORK_LOG_ERROR("[pipeline] Encryption failed: " +
                                                        std::string(e.what()));
                                        result_promise->set_exception(std::current_exception());
                                    }
                                }
                            );

                            auto result = pool->enqueue(std::move(encrypt_job));
                            if (result.has_error()) {
                                NETWORK_LOG_ERROR("[pipeline] Failed to enqueue encryption: " +
                                                result.get_error().message());
                                throw std::runtime_error("Failed to enqueue encryption");
                            }

                            return compressed;
                        }
                        catch (const std::exception& e) {
                            NETWORK_LOG_ERROR("[pipeline] Compression failed: " +
                                            std::string(e.what()));
                            result_promise->set_exception(std::current_exception());
                            throw;
                        }
                    }
                );

                auto result = pool->enqueue(std::move(compress_job));
                if (result.has_error()) {
                    NETWORK_LOG_ERROR("[pipeline] Failed to enqueue compression: " +
                                    result.get_error().message());
                    throw std::runtime_error("Failed to enqueue compression");
                }

                return serialized;
            }
            catch (const std::exception& e) {
                NETWORK_LOG_ERROR("[pipeline] Serialization failed: " +
                                std::string(e.what()));
                result_promise->set_exception(std::current_exception());
                throw;
            }
        }
    );

    // Submit initial job
    auto result = pool->enqueue(std::move(serialize_job));
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[pipeline] Failed to enqueue serialization: " +
                        result.get_error().message());
        throw std::runtime_error("Failed to enqueue serialization");
    }

    return result_future;
}

std::future<std::vector<uint8_t>> pipeline::process_data_with_priority(
    const std::vector<uint8_t>& data,
    pipeline_priority priority) {

    // Temporarily override priorities
    auto old_serialize = serialize_priority_;
    auto old_compress = compress_priority_;
    auto old_encrypt = encrypt_priority_;

    serialize_priority_ = priority;
    compress_priority_ = priority;
    encrypt_priority_ = priority;

    auto result = process_data(data);

    // Restore priorities
    serialize_priority_ = old_serialize;
    compress_priority_ = old_compress;
    encrypt_priority_ = old_encrypt;

    return result;
}

void pipeline::set_priorities(pipeline_priority serialize_priority,
                              pipeline_priority compress_priority,
                              pipeline_priority encrypt_priority) {
    serialize_priority_ = serialize_priority;
    compress_priority_ = compress_priority;
    encrypt_priority_ = encrypt_priority;
}

std::vector<uint8_t> pipeline::serialize_impl(const std::vector<uint8_t>& data) {
    // Actual serialization implementation
    // ...
    return data; // Placeholder
}

std::vector<uint8_t> pipeline::compress_impl(const std::vector<uint8_t>& data) {
    // Actual compression implementation
    // ...
    return data; // Placeholder
}

std::vector<uint8_t> pipeline::encrypt_impl(const std::vector<uint8_t>& data) {
    // Actual encryption implementation
    // ...
    return data; // Placeholder
}

} // namespace network_system
```

**★ Insight ─────────────────────────────────────**

1. **Job Chaining**: The pipeline uses a chaining pattern where each job submits the next job. This ensures sequential execution while maintaining priority-based scheduling.

2. **Exception Safety**: Each stage wraps its operation in try-catch and propagates exceptions through the promise/future mechanism, ensuring errors don't silently fail.

3. **Flexible Priorities**: Default priorities can be set at construction, but individual operations can override them for urgent processing.

─────────────────────────────────────────────────

---

### 2. send_coroutine.cpp - Network Transmission

**File**: `src/network/send_coroutine.cpp`
**Complexity**: ⭐⭐ (Medium)

#### Current Implementation

```cpp
// Current send_coroutine.cpp (simplified)
void send_coroutine::async_send(const std::vector<uint8_t>& data) {
    std::thread([this, data]() {
        try {
            // Blocking network send
            socket_.write_some(asio::buffer(data));
        }
        catch (const std::exception& e) {
            handle_error(e);
        }
    }).detach();
}
```

**Problems**:
- Detached threads cannot be tracked
- No control over send rate
- Thread creation overhead

#### Refactored Implementation

**Header Changes** (`include/network_system/network/send_coroutine.h`):

```cpp
#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

namespace asio {
    class io_context;
    namespace ip {
        namespace tcp {
            class socket;
        }
    }
}

namespace network_system {

/**
 * @brief Asynchronous send coroutine using utility thread pool
 *
 * Handles network transmission using the utility thread pool for
 * potentially blocking send operations.
 */
class send_coroutine {
public:
    using send_callback = std::function<void(bool success, size_t bytes_sent)>;

    /**
     * @brief Constructs send coroutine with socket reference
     */
    explicit send_coroutine(asio::ip::tcp::socket& socket);

    /**
     * @brief Asynchronously send data
     *
     * @param data Data to send
     * @param callback Completion callback
     *
     * Uses utility thread pool for potentially blocking send operations.
     */
    void async_send(const std::vector<uint8_t>& data,
                   send_callback callback = nullptr);

    /**
     * @brief Cancel pending sends
     */
    void cancel();

private:
    asio::ip::tcp::socket& socket_;
    std::atomic<bool> cancelled_{false};
};

} // namespace network_system
```

**Implementation** (`src/network/send_coroutine.cpp`):

```cpp
#include "network_system/network/send_coroutine.h"
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/logger_integration.h"
#include <asio.hpp>

namespace network_system {

send_coroutine::send_coroutine(asio::ip::tcp::socket& socket)
    : socket_(socket) {
}

void send_coroutine::async_send(const std::vector<uint8_t>& data,
                                send_callback callback) {
    if (cancelled_) {
        NETWORK_LOG_WARN("[send_coroutine] Send cancelled");
        if (callback) {
            callback(false, 0);
        }
        return;
    }

    // Get utility pool from manager
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.get_utility_pool();

    if (!pool) {
        NETWORK_LOG_ERROR("[send_coroutine] Utility pool not initialized");
        if (callback) {
            callback(false, 0);
        }
        return;
    }

    // Submit send task to utility pool
    auto job = std::make_unique<kcenon::thread::job>(
        [this, data, callback]() {
            if (cancelled_) {
                NETWORK_LOG_DEBUG("[send_coroutine] Send cancelled before execution");
                if (callback) {
                    callback(false, 0);
                }
                return;
            }

            try {
                NETWORK_LOG_DEBUG("[send_coroutine] Sending " +
                                std::to_string(data.size()) + " bytes");

                // Perform blocking write
                size_t bytes_sent = socket_.write_some(asio::buffer(data));

                NETWORK_LOG_DEBUG("[send_coroutine] Sent " +
                                std::to_string(bytes_sent) + " bytes");

                if (callback) {
                    callback(true, bytes_sent);
                }
            }
            catch (const std::exception& e) {
                NETWORK_LOG_ERROR("[send_coroutine] Send failed: " +
                                std::string(e.what()));
                if (callback) {
                    callback(false, 0);
                }
            }
        }
    );

    auto result = pool->execute(std::move(job));
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[send_coroutine] Failed to submit send: " +
                        result.get_error().message());
        if (callback) {
            callback(false, 0);
        }
    }
}

void send_coroutine::cancel() {
    cancelled_ = true;
    NETWORK_LOG_INFO("[send_coroutine] Cancelled");
}

} // namespace network_system
```

---

### 3. Pipeline Job Helpers

Create helper functions for common pipeline operations.

**File**: `include/network_system/integration/pipeline_jobs.h`

```cpp
#pragma once

#include "network_system/integration/thread_pool_manager.h"
#include <vector>
#include <future>
#include <cstdint>
#include <functional>

namespace network_system {

/**
 * @brief Helper functions for submitting pipeline jobs
 *
 * These functions provide convenient wrappers for common pipeline operations.
 */
namespace pipeline_jobs {

    /**
     * @brief Submit encryption job with HIGH priority
     *
     * @param data Data to encrypt
     * @return Future containing encrypted data
     */
    std::future<std::vector<uint8_t>> submit_encryption(
        const std::vector<uint8_t>& data);

    /**
     * @brief Submit compression job with NORMAL priority
     *
     * @param data Data to compress
     * @return Future containing compressed data
     */
    std::future<std::vector<uint8_t>> submit_compression(
        const std::vector<uint8_t>& data);

    /**
     * @brief Submit serialization job with NORMAL priority
     *
     * @param data Data to serialize
     * @return Future containing serialized data
     */
    std::future<std::vector<uint8_t>> submit_serialization(
        const std::vector<uint8_t>& data);

    /**
     * @brief Submit validation job with LOW priority
     *
     * @param data Data to validate
     * @return Future containing validation result
     */
    std::future<bool> submit_validation(
        const std::vector<uint8_t>& data);

    /**
     * @brief Submit custom pipeline job
     *
     * @param task Task to execute
     * @param priority Job priority
     * @return Future containing result
     */
    template<typename R>
    std::future<R> submit_custom(
        std::function<R()> task,
        pipeline_priority priority);

} // namespace pipeline_jobs

} // namespace network_system
```

**Implementation**: `src/integration/pipeline_jobs.cpp`

```cpp
#include "network_system/integration/pipeline_jobs.h"
#include "network_system/integration/logger_integration.h"
#include <kcenon/thread/typed_job.h>

namespace network_system {
namespace pipeline_jobs {

std::future<std::vector<uint8_t>> submit_encryption(
    const std::vector<uint8_t>& data) {

    auto promise = std::make_shared<std::promise<std::vector<uint8_t>>>();
    auto future = promise->get_future();

    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.get_pipeline_pool();

    if (!pool) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Pipeline pool not initialized");
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error("Pipeline pool not available")));
        return future;
    }

    auto job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
        pipeline_priority::HIGH,
        [data, promise]() {
            try {
                // TODO: Actual encryption implementation
                std::vector<uint8_t> encrypted = data; // Placeholder
                promise->set_value(std::move(encrypted));
            }
            catch (...) {
                promise->set_exception(std::current_exception());
            }
        }
    );

    auto result = pool->enqueue(std::move(job));
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Failed to submit encryption: " +
                        result.get_error().message());
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error(result.get_error().message())));
    }

    return future;
}

std::future<std::vector<uint8_t>> submit_compression(
    const std::vector<uint8_t>& data) {

    auto promise = std::make_shared<std::promise<std::vector<uint8_t>>>();
    auto future = promise->get_future();

    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.get_pipeline_pool();

    if (!pool) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Pipeline pool not initialized");
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error("Pipeline pool not available")));
        return future;
    }

    auto job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
        pipeline_priority::NORMAL,
        [data, promise]() {
            try {
                // TODO: Actual compression implementation
                std::vector<uint8_t> compressed = data; // Placeholder
                promise->set_value(std::move(compressed));
            }
            catch (...) {
                promise->set_exception(std::current_exception());
            }
        }
    );

    auto result = pool->enqueue(std::move(job));
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Failed to submit compression: " +
                        result.get_error().message());
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error(result.get_error().message())));
    }

    return future;
}

std::future<std::vector<uint8_t>> submit_serialization(
    const std::vector<uint8_t>& data) {

    auto promise = std::make_shared<std::promise<std::vector<uint8_t>>>();
    auto future = promise->get_future();

    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.get_pipeline_pool();

    if (!pool) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Pipeline pool not initialized");
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error("Pipeline pool not available")));
        return future;
    }

    auto job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
        pipeline_priority::NORMAL,
        [data, promise]() {
            try {
                // TODO: Actual serialization implementation
                std::vector<uint8_t> serialized = data; // Placeholder
                promise->set_value(std::move(serialized));
            }
            catch (...) {
                promise->set_exception(std::current_exception());
            }
        }
    );

    auto result = pool->enqueue(std::move(job));
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Failed to submit serialization: " +
                        result.get_error().message());
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error(result.get_error().message())));
    }

    return future;
}

std::future<bool> submit_validation(const std::vector<uint8_t>& data) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();

    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.get_pipeline_pool();

    if (!pool) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Pipeline pool not initialized");
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error("Pipeline pool not available")));
        return future;
    }

    auto job = std::make_unique<kcenon::thread::typed_job_t<pipeline_priority>>(
        pipeline_priority::LOW,
        [data, promise]() {
            try {
                // TODO: Actual validation implementation
                bool valid = true; // Placeholder
                promise->set_value(valid);
            }
            catch (...) {
                promise->set_exception(std::current_exception());
            }
        }
    );

    auto result = pool->enqueue(std::move(job));
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[pipeline_jobs] Failed to submit validation: " +
                        result.get_error().message());
        promise->set_exception(
            std::make_exception_ptr(std::runtime_error(result.get_error().message())));
    }

    return future;
}

} // namespace pipeline_jobs
} // namespace network_system
```

---

## 🧪 Testing Strategy

### Unit Tests for Pipeline

**File**: `tests/pipeline_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "network_system/network/pipeline.h"
#include "network_system/integration/thread_pool_manager.h"

class PipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& mgr = thread_pool_manager::instance();
        ASSERT_TRUE(mgr.initialize());

        pipeline_ = std::make_unique<network_system::pipeline>();
    }

    void TearDown() override {
        pipeline_.reset();

        auto& mgr = thread_pool_manager::instance();
        mgr.shutdown();
    }

    std::unique_ptr<network_system::pipeline> pipeline_;
};

TEST_F(PipelineTest, BasicProcessing) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};

    auto future = pipeline_->process_data(data);

    ASSERT_TRUE(future.valid());

    auto result = future.get();
    EXPECT_FALSE(result.empty());
}

TEST_F(PipelineTest, PriorityProcessing) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};

    auto future = pipeline_->process_data_with_priority(
        data, pipeline_priority::REALTIME);

    auto result = future.get();
    EXPECT_FALSE(result.empty());
}

TEST_F(PipelineTest, MultipleJobs) {
    std::vector<std::future<std::vector<uint8_t>>> futures;

    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> data(100, static_cast<uint8_t>(i));
        futures.push_back(pipeline_->process_data(data));
    }

    for (auto& future : futures) {
        auto result = future.get();
        EXPECT_FALSE(result.empty());
    }
}

TEST_F(PipelineTest, ExceptionHandling) {
    // Test with invalid data that triggers exception
    std::vector<uint8_t> invalid_data;

    auto future = pipeline_->process_data(invalid_data);

    EXPECT_THROW(future.get(), std::exception);
}
```

### Performance Benchmarks

```cpp
#include <benchmark/benchmark.h>
#include "network_system/network/pipeline.h"
#include "network_system/integration/thread_pool_manager.h"

static void BM_PipelineProcessing(benchmark::State& state) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    network_system::pipeline pipeline;
    std::vector<uint8_t> data(state.range(0), 0x42);

    for (auto _ : state) {
        auto future = pipeline.process_data(data);
        auto result = future.get();
        benchmark::DoNotOptimize(result);
    }

    mgr.shutdown();
}

BENCHMARK(BM_PipelineProcessing)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->Arg(65536);

BENCHMARK_MAIN();
```

---

## 📈 Performance Optimization

### 1. Batch Processing

For high-throughput scenarios, batch multiple operations:

```cpp
std::vector<std::future<std::vector<uint8_t>>> process_batch(
    const std::vector<std::vector<uint8_t>>& batch) {

    std::vector<std::future<std::vector<uint8_t>>> futures;
    futures.reserve(batch.size());

    for (const auto& data : batch) {
        futures.push_back(process_data(data));
    }

    return futures;
}
```

### 2. Priority Adjustment

Dynamically adjust priorities based on system load:

```cpp
pipeline_priority adjust_priority(size_t queue_depth) {
    if (queue_depth > 1000) {
        return pipeline_priority::REALTIME;  // System under load
    } else if (queue_depth > 100) {
        return pipeline_priority::HIGH;
    } else {
        return pipeline_priority::NORMAL;
    }
}
```

### 3. Monitoring

Track pipeline performance:

```cpp
struct pipeline_metrics {
    std::atomic<uint64_t> jobs_submitted{0};
    std::atomic<uint64_t> jobs_completed{0};
    std::atomic<uint64_t> jobs_failed{0};
    std::atomic<uint64_t> total_latency_us{0};

    double average_latency_us() const {
        auto completed = jobs_completed.load();
        return completed > 0 ?
            static_cast<double>(total_latency_us.load()) / completed : 0.0;
    }
};
```

---

## 📝 Acceptance Criteria

Phase 3 is complete when:

### Functional Requirements

- ✅ Pipeline uses typed_thread_pool
- ✅ All pipeline operations have appropriate priorities
- ✅ send_coroutine uses utility pool
- ✅ Pipeline jobs helpers implemented
- ✅ Exception handling working
- ✅ Existing APIs unchanged

### Testing Requirements

- ✅ All pipeline unit tests pass
- ✅ Performance benchmarks show <5% regression
- ✅ Priority ordering verified
- ✅ No memory leaks
- ✅ Load testing completed

### Documentation Requirements

- ✅ Priority assignment rules documented
- ✅ Pipeline job helpers documented
- ✅ Performance optimization guide complete
- ✅ Examples provided

---

## 🔗 Next Steps

After completing Phase 3:

1. **Verify Pipeline Performance**: Run benchmarks
2. **Test Priority Ordering**: Verify high-priority jobs execute first
3. **Code Review**: Review all changes
4. **Commit Changes**: Commit Phase 3 work
5. **Proceed to Phase 4**: [Integration Layer Updates](PHASE4_INTEGRATION_LAYER.md)

---

## 📚 References

- [Phase 1: Foundation Infrastructure](PHASE1_FOUNDATION.md)
- [Phase 2: Core Components](PHASE2_CORE_COMPONENTS.md)
- [Phase 4: Integration Layer](PHASE4_INTEGRATION_LAYER.md)
- [typed_thread_pool API](/Users/dongcheolshin/Sources/thread_system/src/impl/typed_pool/typed_thread_pool.h)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-03
**Status**: 🔲 Ready for Implementation
