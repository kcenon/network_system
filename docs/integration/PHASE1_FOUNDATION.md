# Phase 1: Foundation Infrastructure

**Status**: 🔲 Not Started
**Estimated Time**: 1-2 days
**Dependencies**: None
**Next Phase**: [Phase 2: Core Components](PHASE2_CORE_COMPONENTS.md)

---

## 📋 Overview

Phase 1 establishes the foundation infrastructure for thread_system integration. This phase creates the core abstractions and utilities that will be used by all other phases.

### Objectives

1. Create centralized thread pool management system
2. Implement RAII wrapper for I/O context execution
3. Define typed job classes for data pipeline
4. Establish patterns for thread pool usage

### Deliverables

- ✅ `thread_pool_manager.h` - Centralized pool manager
- ✅ `thread_pool_manager.cpp` - Implementation
- ✅ `io_context_executor.h` - I/O execution wrapper
- ✅ `io_context_executor.cpp` - Implementation
- ✅ `pipeline_jobs.h` - Typed job definitions
- ✅ `pipeline_jobs.cpp` - Implementation
- ✅ Unit tests for all components

---

## 🎯 Component 1: thread_pool_manager

### Purpose

Centralized manager for all thread pools in network_system. Provides:
- Singleton access to thread pools
- Separate pools for I/O, pipeline, and utility tasks
- Component-specific I/O pool creation
- Resource monitoring and statistics

### API Design

**Header**: `include/network_system/integration/thread_pool_manager.h`

```cpp
#pragma once

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/typed_thread_pool.h>
#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include <thread>

namespace network_system::integration {

/**
 * @brief Priority levels for network data pipeline
 */
enum class pipeline_priority : int {
    REALTIME = 0,      ///< Real-time encryption, urgent transmission
    HIGH = 1,          ///< Important data processing
    NORMAL = 2,        ///< General compression, serialization
    LOW = 3,           ///< Background validation
    BACKGROUND = 4     ///< Logging, statistics
};

/**
 * @brief Centralized thread pool manager for network_system
 *
 * Provides singleton access to thread pools used throughout the system:
 * - I/O thread pools (size=1) for io_context execution
 * - Data pipeline pool (typed) for priority-based processing
 * - Utility pool for general-purpose async tasks
 *
 * @note Thread-safe singleton implementation
 */
class thread_pool_manager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the singleton instance
     */
    static thread_pool_manager& instance();

    /**
     * @brief Initialize all thread pools
     *
     * Must be called before using any pools. Safe to call multiple times
     * (subsequent calls are no-op).
     *
     * @param io_pool_count Reserved capacity for I/O pools (default: 10)
     * @param pipeline_workers Number of pipeline workers (default: hardware cores)
     * @param utility_workers Number of utility workers (default: half cores)
     * @return true if initialized successfully, false if already initialized
     */
    bool initialize(
        size_t io_pool_count = 10,
        size_t pipeline_workers = std::thread::hardware_concurrency(),
        size_t utility_workers = std::thread::hardware_concurrency() / 2
    );

    /**
     * @brief Check if manager is initialized
     * @return true if initialized
     */
    bool is_initialized() const;

    /**
     * @brief Shutdown all thread pools
     *
     * Stops all pools and waits for completion. After shutdown, pools
     * cannot be used unless initialize() is called again.
     */
    void shutdown();

    /**
     * @brief Create a dedicated I/O thread pool for a component
     *
     * Creates a new thread pool with exactly 1 worker for running
     * io_context::run(). Each component should get its own pool.
     *
     * @param component_name Unique name for logging (e.g., "messaging_server:srv1")
     * @return Shared pointer to thread pool (size=1)
     * @throws std::runtime_error if not initialized
     */
    std::shared_ptr<kcenon::thread::thread_pool>
    create_io_pool(const std::string& component_name);

    /**
     * @brief Get the shared data pipeline thread pool
     *
     * Returns the typed thread pool for data processing with priority support.
     * This pool is shared across all components for CPU-intensive tasks.
     *
     * @return Shared pointer to typed thread pool
     * @throws std::runtime_error if not initialized
     */
    std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>>
    get_pipeline_pool();

    /**
     * @brief Get the general-purpose utility thread pool
     *
     * Returns the shared utility pool for blocking I/O and background tasks.
     *
     * @return Shared pointer to thread pool
     * @throws std::runtime_error if not initialized
     */
    std::shared_ptr<kcenon::thread::thread_pool>
    get_utility_pool();

    /**
     * @brief Statistics for monitoring
     */
    struct statistics {
        size_t total_io_pools = 0;        ///< Number of I/O pools created
        size_t active_io_tasks = 0;       ///< Active tasks in I/O pools
        size_t pipeline_queue_size = 0;   ///< Pending pipeline jobs
        size_t pipeline_workers = 0;      ///< Pipeline worker count
        size_t utility_queue_size = 0;    ///< Pending utility jobs
        size_t utility_workers = 0;       ///< Utility worker count
        bool is_initialized = false;      ///< Initialization status
    };

    /**
     * @brief Get current statistics
     * @return Current pool statistics
     */
    statistics get_statistics() const;

    // Delete copy and move
    thread_pool_manager(const thread_pool_manager&) = delete;
    thread_pool_manager& operator=(const thread_pool_manager&) = delete;
    thread_pool_manager(thread_pool_manager&&) = delete;
    thread_pool_manager& operator=(thread_pool_manager&&) = delete;

private:
    thread_pool_manager() = default;
    ~thread_pool_manager();

    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::integration
```

### Implementation

**File**: `src/integration/thread_pool_manager.cpp`

```cpp
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/logger_integration.h"
#include <kcenon/thread/interfaces/thread_context.h>

namespace network_system::integration {

// Implementation class
class thread_pool_manager::impl {
public:
    impl() = default;

    bool initialize(size_t io_pool_count, size_t pipeline_workers, size_t utility_workers) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_) {
            NETWORK_LOG_WARN("[thread_pool_manager] Already initialized");
            return false;
        }

        try {
            // Create pipeline pool (typed, priority-based)
            kcenon::thread::thread_context ctx;
            pipeline_pool_ = std::make_shared<
                kcenon::thread::typed_thread_pool_t<pipeline_priority>
            >("pipeline_pool", ctx);

            auto result = pipeline_pool_->start();
            if (result.has_error()) {
                throw std::runtime_error(
                    "Failed to start pipeline pool: " + result.get_error().message()
                );
            }

            // Add workers to pipeline pool
            for (size_t i = 0; i < pipeline_workers; ++i) {
                pipeline_pool_->add_worker();
            }

            // Create utility pool
            utility_pool_ = std::make_shared<kcenon::thread::thread_pool>(
                "utility_pool", ctx
            );

            result = utility_pool_->start();
            if (result.has_error()) {
                throw std::runtime_error(
                    "Failed to start utility pool: " + result.get_error().message()
                );
            }

            // Add workers to utility pool
            for (size_t i = 0; i < utility_workers; ++i) {
                utility_pool_->add_worker();
            }

            // Reserve space for I/O pools
            io_pools_.reserve(io_pool_count);

            initialized_ = true;

            NETWORK_LOG_INFO("[thread_pool_manager] Initialized: "
                "pipeline_workers=" + std::to_string(pipeline_workers) +
                ", utility_workers=" + std::to_string(utility_workers));

            return true;
        } catch (const std::exception& e) {
            NETWORK_LOG_ERROR("[thread_pool_manager] Initialization failed: " +
                std::string(e.what()));
            cleanup();
            return false;
        }
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            return;
        }

        NETWORK_LOG_INFO("[thread_pool_manager] Shutting down...");

        cleanup();
        initialized_ = false;

        NETWORK_LOG_INFO("[thread_pool_manager] Shutdown complete");
    }

    std::shared_ptr<kcenon::thread::thread_pool>
    create_io_pool(const std::string& component_name) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            throw std::runtime_error(
                "thread_pool_manager not initialized. Call initialize() first."
            );
        }

        // Create new pool with 1 worker
        kcenon::thread::thread_context ctx;
        auto pool = std::make_shared<kcenon::thread::thread_pool>(
            "io_pool:" + component_name, ctx
        );

        auto result = pool->start();
        if (result.has_error()) {
            throw std::runtime_error(
                "Failed to start I/O pool for " + component_name +
                ": " + result.get_error().message()
            );
        }

        // Add exactly 1 worker for I/O
        pool->add_worker();

        // Store pool
        io_pools_[component_name] = pool;

        NETWORK_LOG_DEBUG("[thread_pool_manager] Created I/O pool for: " +
            component_name);

        return pool;
    }

    std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>>
    get_pipeline_pool() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            throw std::runtime_error(
                "thread_pool_manager not initialized. Call initialize() first."
            );
        }

        return pipeline_pool_;
    }

    std::shared_ptr<kcenon::thread::thread_pool>
    get_utility_pool() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_) {
            throw std::runtime_error(
                "thread_pool_manager not initialized. Call initialize() first."
            );
        }

        return utility_pool_;
    }

    thread_pool_manager::statistics get_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);

        statistics stats;
        stats.is_initialized = initialized_;
        stats.total_io_pools = io_pools_.size();

        if (!initialized_) {
            return stats;
        }

        // Collect I/O pool stats
        for (const auto& [name, pool] : io_pools_) {
            stats.active_io_tasks += pool->get_active_thread_count();
        }

        // Pipeline stats
        if (pipeline_pool_) {
            stats.pipeline_queue_size = pipeline_pool_->get_pending_job_count();
            stats.pipeline_workers = pipeline_pool_->get_thread_count();
        }

        // Utility stats
        if (utility_pool_) {
            stats.utility_queue_size = utility_pool_->get_pending_job_count();
            stats.utility_workers = utility_pool_->get_thread_count();
        }

        return stats;
    }

    bool is_initialized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return initialized_;
    }

private:
    void cleanup() {
        // Stop all I/O pools
        for (auto& [name, pool] : io_pools_) {
            if (pool) {
                pool->stop();
            }
        }
        io_pools_.clear();

        // Stop pipeline pool
        if (pipeline_pool_) {
            pipeline_pool_->stop();
            pipeline_pool_.reset();
        }

        // Stop utility pool
        if (utility_pool_) {
            utility_pool_->stop();
            utility_pool_.reset();
        }
    }

    mutable std::mutex mutex_;
    bool initialized_ = false;

    std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>> pipeline_pool_;
    std::shared_ptr<kcenon::thread::thread_pool> utility_pool_;
    std::unordered_map<std::string, std::shared_ptr<kcenon::thread::thread_pool>> io_pools_;
};

// Singleton implementation
thread_pool_manager& thread_pool_manager::instance() {
    static thread_pool_manager instance;
    return instance;
}

thread_pool_manager::~thread_pool_manager() {
    if (pimpl_) {
        pimpl_->shutdown();
    }
}

bool thread_pool_manager::initialize(
    size_t io_pool_count,
    size_t pipeline_workers,
    size_t utility_workers
) {
    if (!pimpl_) {
        pimpl_ = std::make_unique<impl>();
    }
    return pimpl_->initialize(io_pool_count, pipeline_workers, utility_workers);
}

bool thread_pool_manager::is_initialized() const {
    return pimpl_ && pimpl_->is_initialized();
}

void thread_pool_manager::shutdown() {
    if (pimpl_) {
        pimpl_->shutdown();
    }
}

std::shared_ptr<kcenon::thread::thread_pool>
thread_pool_manager::create_io_pool(const std::string& component_name) {
    if (!pimpl_) {
        throw std::runtime_error("thread_pool_manager not initialized");
    }
    return pimpl_->create_io_pool(component_name);
}

std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>>
thread_pool_manager::get_pipeline_pool() {
    if (!pimpl_) {
        throw std::runtime_error("thread_pool_manager not initialized");
    }
    return pimpl_->get_pipeline_pool();
}

std::shared_ptr<kcenon::thread::thread_pool>
thread_pool_manager::get_utility_pool() {
    if (!pimpl_) {
        throw std::runtime_error("thread_pool_manager not initialized");
    }
    return pimpl_->get_utility_pool();
}

thread_pool_manager::statistics
thread_pool_manager::get_statistics() const {
    if (!pimpl_) {
        return statistics{};
    }
    return pimpl_->get_statistics();
}

} // namespace network_system::integration
```

### Unit Tests

**File**: `tests/thread_pool_manager_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "network_system/integration/thread_pool_manager.h"

using namespace network_system::integration;

class ThreadPoolManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state
        auto& mgr = thread_pool_manager::instance();
        if (mgr.is_initialized()) {
            mgr.shutdown();
        }
    }

    void TearDown() override {
        auto& mgr = thread_pool_manager::instance();
        if (mgr.is_initialized()) {
            mgr.shutdown();
        }
    }
};

TEST_F(ThreadPoolManagerTest, Singleton) {
    auto& mgr1 = thread_pool_manager::instance();
    auto& mgr2 = thread_pool_manager::instance();

    EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(ThreadPoolManagerTest, InitializeOnce) {
    auto& mgr = thread_pool_manager::instance();

    EXPECT_FALSE(mgr.is_initialized());

    EXPECT_TRUE(mgr.initialize());
    EXPECT_TRUE(mgr.is_initialized());

    // Second initialize should return false (already initialized)
    EXPECT_FALSE(mgr.initialize());
}

TEST_F(ThreadPoolManagerTest, CreateIoPool) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    auto pool = mgr.create_io_pool("test_component");

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->is_started());
    EXPECT_EQ(pool->get_thread_count(), 1);
}

TEST_F(ThreadPoolManagerTest, GetPipelinePool) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize(10, 4, 2);

    auto pool = mgr.get_pipeline_pool();

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->is_started());
    EXPECT_EQ(pool->get_thread_count(), 4);
}

TEST_F(ThreadPoolManagerTest, GetUtilityPool) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize(10, 4, 2);

    auto pool = mgr.get_utility_pool();

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(pool->is_started());
    EXPECT_EQ(pool->get_thread_count(), 2);
}

TEST_F(ThreadPoolManagerTest, GetStatistics) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize(10, 4, 2);

    mgr.create_io_pool("comp1");
    mgr.create_io_pool("comp2");

    auto stats = mgr.get_statistics();

    EXPECT_TRUE(stats.is_initialized);
    EXPECT_EQ(stats.total_io_pools, 2);
    EXPECT_EQ(stats.pipeline_workers, 4);
    EXPECT_EQ(stats.utility_workers, 2);
}

TEST_F(ThreadPoolManagerTest, Shutdown) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    mgr.create_io_pool("test");

    mgr.shutdown();

    EXPECT_FALSE(mgr.is_initialized());

    // Should throw after shutdown
    EXPECT_THROW(mgr.create_io_pool("test2"), std::runtime_error);
}

TEST_F(ThreadPoolManagerTest, ThrowsWhenNotInitialized) {
    auto& mgr = thread_pool_manager::instance();

    EXPECT_THROW(mgr.create_io_pool("test"), std::runtime_error);
    EXPECT_THROW(mgr.get_pipeline_pool(), std::runtime_error);
    EXPECT_THROW(mgr.get_utility_pool(), std::runtime_error);
}
```

---

## 🎯 Component 2: io_context_executor

### Purpose

RAII wrapper for executing `io_context::run()` in a thread pool. Provides:
- Automatic submission of io_context to thread pool
- Clean shutdown handling
- Exception safety
- Lifecycle management

### API Design

**Header**: `include/network_system/integration/io_context_executor.h`

```cpp
#pragma once

#include <kcenon/thread/core/thread_pool.h>
#include <asio.hpp>
#include <memory>
#include <string>
#include <future>
#include <atomic>

namespace network_system::integration {

/**
 * @brief RAII wrapper for executing io_context::run() in thread pool
 *
 * Automatically manages the lifecycle of io_context execution:
 * - Submits io_context::run() to thread pool on start()
 * - Stops io_context and waits for completion on stop()
 * - Ensures clean shutdown in destructor
 *
 * @note Thread-safe operations
 */
class io_context_executor {
public:
    /**
     * @brief Constructor
     *
     * @param pool Thread pool (should have size=1 for I/O)
     * @param io_context ASIO io_context to execute
     * @param component_name Name for logging and identification
     */
    io_context_executor(
        std::shared_ptr<kcenon::thread::thread_pool> pool,
        asio::io_context& io_context,
        const std::string& component_name
    );

    /**
     * @brief Destructor
     *
     * Automatically calls stop() if still running
     */
    ~io_context_executor();

    // Delete copy, allow move
    io_context_executor(const io_context_executor&) = delete;
    io_context_executor& operator=(const io_context_executor&) = delete;
    io_context_executor(io_context_executor&&) noexcept = default;
    io_context_executor& operator=(io_context_executor&&) noexcept = default;

    /**
     * @brief Start executing io_context::run() in thread pool
     *
     * Submits the io_context execution task to the thread pool.
     * Safe to call multiple times (subsequent calls are no-op).
     *
     * @throws std::runtime_error if pool is not running
     */
    void start();

    /**
     * @brief Stop io_context execution and wait for completion
     *
     * Stops the io_context and waits for the execution thread to complete.
     * Safe to call multiple times (subsequent calls are no-op).
     */
    void stop();

    /**
     * @brief Check if currently running
     * @return true if io_context is being executed
     */
    bool is_running() const;

    /**
     * @brief Get the underlying thread pool
     * @return Shared pointer to thread pool
     */
    std::shared_ptr<kcenon::thread::thread_pool> get_pool() const;

    /**
     * @brief Get the component name
     * @return Component name string
     */
    std::string get_component_name() const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::integration
```

### Implementation

**File**: `src/integration/io_context_executor.cpp`

```cpp
#include "network_system/integration/io_context_executor.h"
#include "network_system/integration/logger_integration.h"

namespace network_system::integration {

class io_context_executor::impl {
public:
    impl(std::shared_ptr<kcenon::thread::thread_pool> pool,
         asio::io_context& io_context,
         const std::string& component_name)
        : pool_(std::move(pool))
        , io_context_(io_context)
        , component_name_(component_name)
    {}

    void start() {
        if (running_.exchange(true)) {
            NETWORK_LOG_WARN("[io_context_executor] Already running: " + component_name_);
            return;
        }

        if (!pool_ || !pool_->is_started()) {
            running_.store(false);
            throw std::runtime_error(
                "Thread pool not running for " + component_name_
            );
        }

        // Submit io_context::run() to thread pool
        execution_future_ = pool_->submit([this]() {
            try {
                NETWORK_LOG_DEBUG("[io_context_executor] Starting io_context: " +
                    component_name_);

                io_context_.run();

                NETWORK_LOG_DEBUG("[io_context_executor] io_context stopped: " +
                    component_name_);
            } catch (const std::exception& e) {
                NETWORK_LOG_ERROR("[io_context_executor] Exception in " +
                    component_name_ + ": " + e.what());
            } catch (...) {
                NETWORK_LOG_ERROR("[io_context_executor] Unknown exception in " +
                    component_name_);
            }
        });

        NETWORK_LOG_INFO("[io_context_executor] Started: " + component_name_);
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;  // Already stopped
        }

        NETWORK_LOG_DEBUG("[io_context_executor] Stopping: " + component_name_);

        // Stop io_context
        if (!io_context_.stopped()) {
            io_context_.stop();
        }

        // Wait for execution thread to complete
        if (execution_future_.valid()) {
            try {
                execution_future_.get();
            } catch (const std::exception& e) {
                NETWORK_LOG_WARN("[io_context_executor] Exception during stop: " +
                    std::string(e.what()));
            }
        }

        NETWORK_LOG_INFO("[io_context_executor] Stopped: " + component_name_);
    }

    bool is_running() const {
        return running_.load();
    }

    std::shared_ptr<kcenon::thread::thread_pool> get_pool() const {
        return pool_;
    }

    std::string get_component_name() const {
        return component_name_;
    }

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
    asio::io_context& io_context_;
    std::string component_name_;
    std::atomic<bool> running_{false};
    std::future<void> execution_future_;
};

io_context_executor::io_context_executor(
    std::shared_ptr<kcenon::thread::thread_pool> pool,
    asio::io_context& io_context,
    const std::string& component_name
) : pimpl_(std::make_unique<impl>(std::move(pool), io_context, component_name)) {}

io_context_executor::~io_context_executor() {
    if (pimpl_ && pimpl_->is_running()) {
        pimpl_->stop();
    }
}

void io_context_executor::start() {
    pimpl_->start();
}

void io_context_executor::stop() {
    pimpl_->stop();
}

bool io_context_executor::is_running() const {
    return pimpl_->is_running();
}

std::shared_ptr<kcenon::thread::thread_pool>
io_context_executor::get_pool() const {
    return pimpl_->get_pool();
}

std::string io_context_executor::get_component_name() const {
    return pimpl_->get_component_name();
}

} // namespace network_system::integration
```

### Unit Tests

**File**: `tests/io_context_executor_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "network_system/integration/io_context_executor.h"
#include "network_system/integration/thread_pool_manager.h"

using namespace network_system::integration;

class IoContextExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& mgr = thread_pool_manager::instance();
        if (!mgr.is_initialized()) {
            mgr.initialize();
        }
    }

    void TearDown() override {
        auto& mgr = thread_pool_manager::instance();
        mgr.shutdown();
    }
};

TEST_F(IoContextExecutorTest, BasicExecution) {
    asio::io_context io_ctx;
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.create_io_pool("test");

    io_context_executor executor(pool, io_ctx, "test");

    EXPECT_FALSE(executor.is_running());

    // Post work to io_context
    std::atomic<bool> executed{false};
    io_ctx.post([&executed]() {
        executed.store(true);
    });

    executor.start();
    EXPECT_TRUE(executor.is_running());

    // Wait a bit for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    executor.stop();
    EXPECT_FALSE(executor.is_running());

    EXPECT_TRUE(executed.load());
}

TEST_F(IoContextExecutorTest, AutoStopInDestructor) {
    asio::io_context io_ctx;
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.create_io_pool("test");

    std::atomic<bool> work_done{false};

    {
        io_context_executor executor(pool, io_ctx, "test");

        io_ctx.post([&work_done]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            work_done.store(true);
        });

        executor.start();
        EXPECT_TRUE(executor.is_running());

        // Destructor should stop automatically
    }

    // After destructor, should be stopped
    EXPECT_TRUE(work_done.load());
}

TEST_F(IoContextExecutorTest, MultipleStartStopCalls) {
    asio::io_context io_ctx;
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.create_io_pool("test");

    io_context_executor executor(pool, io_ctx, "test");

    // Multiple starts should be safe
    executor.start();
    EXPECT_TRUE(executor.is_running());
    executor.start();  // Second start is no-op
    EXPECT_TRUE(executor.is_running());

    // Multiple stops should be safe
    executor.stop();
    EXPECT_FALSE(executor.is_running());
    executor.stop();  // Second stop is no-op
    EXPECT_FALSE(executor.is_running());
}

TEST_F(IoContextExecutorTest, GetComponentName) {
    asio::io_context io_ctx;
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.create_io_pool("test");

    io_context_executor executor(pool, io_ctx, "my_component");

    EXPECT_EQ(executor.get_component_name(), "my_component");
}

TEST_F(IoContextExecutorTest, Move) {
    asio::io_context io_ctx;
    auto& mgr = thread_pool_manager::instance();
    auto pool = mgr.create_io_pool("test");

    io_context_executor exec1(pool, io_ctx, "test");
    exec1.start();

    // Move constructor
    io_context_executor exec2(std::move(exec1));
    EXPECT_TRUE(exec2.is_running());

    exec2.stop();
    EXPECT_FALSE(exec2.is_running());
}
```

---

## 🎯 Component 3: pipeline_jobs

**Note**: This component will be fully detailed in [Phase 3: Pipeline Integration](PHASE3_PIPELINE.md).

For Phase 1, create placeholder header with enum definition:

**File**: `include/network_system/internal/pipeline_jobs.h`

```cpp
#pragma once

#include "network_system/integration/thread_pool_manager.h"
#include <vector>
#include <functional>
#include <future>
#include <any>

namespace network_system::internal {

// Forward declarations for Phase 3
class pipeline_job_base;
class encryption_job;
class compression_job;
class serialization_job;
class pipeline_submitter;

// Placeholder implementations will be added in Phase 3

} // namespace network_system::internal
```

---

## ✅ Acceptance Criteria

### Functional Requirements

- [ ] thread_pool_manager singleton works correctly
- [ ] Can initialize manager with custom pool sizes
- [ ] Can create multiple I/O pools (1 worker each)
- [ ] Pipeline pool and utility pool accessible
- [ ] Statistics accurately reflect pool states
- [ ] Clean shutdown of all pools
- [ ] io_context_executor starts and stops correctly
- [ ] Auto-stop in destructor works
- [ ] Exception safety verified

### Testing Requirements

- [ ] All unit tests pass
- [ ] Code coverage > 80%
- [ ] No memory leaks (valgrind/asan)
- [ ] Thread safety verified (tsan)

### Documentation Requirements

- [ ] All public APIs documented
- [ ] Usage examples provided
- [ ] Integration patterns explained

---

## 📈 Success Metrics

| Metric | Target | How to Measure |
|--------|--------|----------------|
| Test Coverage | > 80% | lcov/gcov |
| Memory Leaks | 0 | valgrind |
| Thread Safety | No races | ThreadSanitizer |
| Build Time | < +10% | CMake timing |

---

## 🚀 Implementation Steps

1. **Create Headers**
   - thread_pool_manager.h
   - io_context_executor.h
   - pipeline_jobs.h (placeholder)

2. **Implement thread_pool_manager**
   - Implementation class
   - Singleton pattern
   - Pool lifecycle management
   - Error handling

3. **Implement io_context_executor**
   - Implementation class
   - Start/stop logic
   - Exception handling
   - RAII semantics

4. **Write Unit Tests**
   - thread_pool_manager tests
   - io_context_executor tests
   - Edge cases and error conditions

5. **Verify Integration**
   - Build system integration
   - Run tests
   - Check code coverage
   - Memory leak detection

---

## 📝 Notes

- thread_pool_manager uses Pimpl idiom for ABI stability
- All operations are thread-safe
- Pools are started immediately after creation
- I/O pools always have exactly 1 worker
- Pipeline and utility pools can have configurable worker counts

---

**Next**: [Phase 2: Core Component Refactoring](PHASE2_CORE_COMPONENTS.md)
