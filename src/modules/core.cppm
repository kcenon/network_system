// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file core.cppm
 * @brief Core partition for network_system module.
 *
 * This partition provides core network infrastructure including
 * network context, connection management, and base classes.
 *
 * Contents:
 * - network_context: Global context for shared resources
 * - connection_pool: Connection pooling for efficient reuse
 * - messaging_client_base: Base class for all messaging clients
 * - messaging_server_base: Base class for all messaging servers
 * - io_context_thread_manager: ASIO io_context management
 *
 * @see kcenon.network for the primary module interface
 */

module;

// =============================================================================
// Global Module Fragment - Standard Library Headers
// =============================================================================
#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

// Third-party headers (must be in global module fragment)
#include <asio.hpp>

export module kcenon.network:core;

export import kcenon.common;
export import kcenon.thread;

// =============================================================================
// Forward Declarations
// =============================================================================

export namespace kcenon::network::core {

class network_context;
class connection_pool;

} // namespace kcenon::network::core

export namespace kcenon::network::integration {

class thread_pool_interface;
class logger_interface;
class monitoring_interface;
class io_context_thread_manager;

} // namespace kcenon::network::integration

// =============================================================================
// Thread Pool Interface
// =============================================================================

export namespace kcenon::network::integration {

/**
 * @brief Interface for thread pool integration
 *
 * This interface allows network_system to use external thread pool
 * implementations (e.g., from thread_system) for task execution.
 */
class thread_pool_interface {
public:
    virtual ~thread_pool_interface() = default;

    /**
     * @brief Submit a task to the thread pool
     * @param task The task to execute
     */
    virtual void submit(std::function<void()> task) = 0;

    /**
     * @brief Get the number of worker threads
     * @return Number of worker threads
     */
    virtual size_t worker_count() const = 0;

    /**
     * @brief Check if the thread pool is running
     * @return true if running, false otherwise
     */
    virtual bool is_running() const = 0;
};

/**
 * @brief Interface for logger integration
 *
 * This interface allows network_system to use external logger
 * implementations for logging messages.
 */
class logger_interface {
public:
    virtual ~logger_interface() = default;

    /**
     * @brief Log an informational message
     * @param message The message to log
     */
    virtual void info(std::string_view message) = 0;

    /**
     * @brief Log a warning message
     * @param message The message to log
     */
    virtual void warning(std::string_view message) = 0;

    /**
     * @brief Log an error message
     * @param message The message to log
     */
    virtual void error(std::string_view message) = 0;

    /**
     * @brief Log a debug message
     * @param message The message to log
     */
    virtual void debug(std::string_view message) = 0;
};

/**
 * @brief Interface for monitoring integration
 *
 * This interface allows network_system to report metrics to external
 * monitoring systems.
 */
class monitoring_interface {
public:
    virtual ~monitoring_interface() = default;

    /**
     * @brief Record a counter metric
     * @param name Metric name
     * @param value Counter value
     */
    virtual void record_counter(std::string_view name, int64_t value) = 0;

    /**
     * @brief Record a gauge metric
     * @param name Metric name
     * @param value Gauge value
     */
    virtual void record_gauge(std::string_view name, double value) = 0;

    /**
     * @brief Record a histogram metric
     * @param name Metric name
     * @param value Histogram value
     */
    virtual void record_histogram(std::string_view name, double value) = 0;
};

/**
 * @class io_context_thread_manager
 * @brief Manages ASIO io_context lifecycle and worker threads
 *
 * This class provides a thread-safe wrapper around asio::io_context,
 * managing worker threads for asynchronous I/O operations.
 */
class io_context_thread_manager {
public:
    /**
     * @brief Construct a manager with specified worker count
     * @param worker_count Number of worker threads (0 = hardware concurrency)
     */
    explicit io_context_thread_manager(size_t worker_count = 0);

    /**
     * @brief Destructor - stops the manager if running
     */
    ~io_context_thread_manager();

    // Non-copyable, movable
    io_context_thread_manager(const io_context_thread_manager&) = delete;
    io_context_thread_manager& operator=(const io_context_thread_manager&) = delete;
    io_context_thread_manager(io_context_thread_manager&&) noexcept;
    io_context_thread_manager& operator=(io_context_thread_manager&&) noexcept;

    /**
     * @brief Start the io_context worker threads
     * @return true if started successfully, false if already running
     */
    bool start();

    /**
     * @brief Stop the io_context and join worker threads
     */
    void stop();

    /**
     * @brief Check if the manager is running
     * @return true if running, false otherwise
     */
    bool is_running() const noexcept;

    /**
     * @brief Get the managed io_context
     * @return Reference to the io_context
     */
    asio::io_context& get_io_context() noexcept;

    /**
     * @brief Get the number of worker threads
     * @return Number of worker threads
     */
    size_t worker_count() const noexcept;

private:
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    size_t worker_count_;
    mutable std::mutex mutex_;
};

} // namespace kcenon::network::integration

// =============================================================================
// Network Context
// =============================================================================

export namespace kcenon::network::core {

/**
 * @class network_context
 * @brief Global context for shared network system resources
 *
 * This class manages shared resources like thread pools, loggers, and monitoring
 * across all network system components. It follows the singleton pattern.
 */
class network_context {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static network_context& instance();

    /**
     * @brief Set custom thread pool
     * @param pool Thread pool implementation
     */
    void set_thread_pool(std::shared_ptr<integration::thread_pool_interface> pool);

    /**
     * @brief Get current thread pool
     * @return Shared pointer to thread pool interface
     */
    std::shared_ptr<integration::thread_pool_interface> get_thread_pool();

    /**
     * @brief Set custom logger
     * @param logger Logger implementation
     */
    void set_logger(std::shared_ptr<integration::logger_interface> logger);

    /**
     * @brief Get current logger
     * @return Shared pointer to logger interface
     */
    std::shared_ptr<integration::logger_interface> get_logger();

    /**
     * @brief Set custom monitoring system
     * @param monitoring Monitoring implementation
     */
    void set_monitoring(std::shared_ptr<integration::monitoring_interface> monitoring);

    /**
     * @brief Get current monitoring system
     * @return Shared pointer to monitoring interface
     */
    std::shared_ptr<integration::monitoring_interface> get_monitoring();

    /**
     * @brief Get the io_context thread manager
     * @return Reference to the io_context thread manager
     */
    integration::io_context_thread_manager& get_io_context_manager();

    /**
     * @brief Reset all resources to defaults
     */
    void reset();

private:
    network_context();
    ~network_context() = default;

    // Non-copyable
    network_context(const network_context&) = delete;
    network_context& operator=(const network_context&) = delete;

    std::shared_ptr<integration::thread_pool_interface> thread_pool_;
    std::shared_ptr<integration::logger_interface> logger_;
    std::shared_ptr<integration::monitoring_interface> monitoring_;
    std::unique_ptr<integration::io_context_thread_manager> io_manager_;
    mutable std::shared_mutex mutex_;
};

/**
 * @class connection_pool
 * @brief Connection pooling for efficient connection reuse
 *
 * This class manages a pool of connections to reduce the overhead
 * of creating new connections for each request.
 *
 * @tparam Connection The connection type to pool
 */
class connection_pool {
public:
    /**
     * @brief Construct a connection pool
     * @param max_connections Maximum number of pooled connections
     * @param idle_timeout Timeout for idle connections
     */
    explicit connection_pool(
        size_t max_connections = 10,
        std::chrono::seconds idle_timeout = std::chrono::seconds(60));

    /**
     * @brief Destructor - closes all pooled connections
     */
    ~connection_pool();

    // Non-copyable
    connection_pool(const connection_pool&) = delete;
    connection_pool& operator=(const connection_pool&) = delete;

    /**
     * @brief Get current pool size
     * @return Number of connections in the pool
     */
    size_t size() const noexcept;

    /**
     * @brief Get maximum pool size
     * @return Maximum number of connections
     */
    size_t max_size() const noexcept;

    /**
     * @brief Clear all pooled connections
     */
    void clear();

private:
    size_t max_connections_;
    std::chrono::seconds idle_timeout_;
    mutable std::mutex mutex_;
};

} // namespace kcenon::network::core

// =============================================================================
// Base Classes for Messaging
// =============================================================================

export namespace kcenon::network::core {

/**
 * @brief Connection state enumeration
 */
enum class connection_state {
    disconnected,
    connecting,
    connected,
    disconnecting
};

/**
 * @brief Convert connection state to string
 * @param state The connection state
 * @return String representation
 */
constexpr std::string_view to_string(connection_state state) noexcept {
    switch (state) {
        case connection_state::disconnected: return "disconnected";
        case connection_state::connecting: return "connecting";
        case connection_state::connected: return "connected";
        case connection_state::disconnecting: return "disconnecting";
        default: return "unknown";
    }
}

/**
 * @brief Callback type aliases for messaging
 */
using connection_callback = std::function<void()>;
using disconnection_callback = std::function<void()>;
using error_callback = std::function<void(const std::string&)>;
using data_callback = std::function<void(std::span<const uint8_t>)>;

} // namespace kcenon::network::core
