---
doc_id: "NET-API-002b"
doc_title: "API Reference - Protocols"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "API"
---

# API Reference - Protocols

> **SSOT**: This document is part of the API Reference. See also [API Reference - Facades](API_REFERENCE_FACADES.md) and the [API Reference Index](API_REFERENCE.md).

> **Language:** **English** | [한국어](API_REFERENCE.kr.md)

Protocol-level API documentation for the Network System library, covering low-level protocol classes, unified interface, and gRPC.

## Table of Contents

- [Protocol Handlers](#protocol-handlers)
- [Thread Integration API](#thread-integration-api)
- [Version History](#version-history)
- [License](#license)

## Protocol Handlers

### HTTP Handler

HTTP protocol implementation.

```cpp
namespace network::http {

class HttpServer {
public:
    /**
     * HTTP request handler function type.
     */
    using RequestHandler = std::function<Response(const Request&)>;

    /**
     * Constructs an HTTP server.
     * @param config Server configuration
     */
    explicit HttpServer(const HttpConfig& config);

    /**
     * Registers a route handler.
     * @param method HTTP method (GET, POST, etc.)
     * @param path URL path pattern
     * @param handler Request handler function
     */
    void route(const std::string& method, const std::string& path,
              RequestHandler handler);

    /**
     * Adds middleware to the request pipeline.
     * @param middleware Middleware function
     */
    void use(MiddlewareFunc middleware);

    /**
     * Starts the HTTP server.
     * @return Result<void> indicating success or failure
     */
    Result<void> start();

    /**
     * Stops the HTTP server.
     * @return Result<void> indicating success or failure
     */
    Result<void> stop();
};

class Request {
public:
    /**
     * Gets the HTTP method.
     * @return HTTP method string
     */
    std::string get_method() const;

    /**
     * Gets the request URI.
     * @return URI string
     */
    std::string get_uri() const;

    /**
     * Gets a header value.
     * @param name Header name
     * @return Optional header value
     */
    std::optional<std::string> get_header(const std::string& name) const;

    /**
     * Gets all headers.
     * @return Map of header name-value pairs
     */
    std::unordered_map<std::string, std::string> get_headers() const;

    /**
     * Gets the request body.
     * @return Request body as string
     */
    std::string get_body() const;

    /**
     * Gets a query parameter.
     * @param name Parameter name
     * @return Optional parameter value
     */
    std::optional<std::string> get_query_param(const std::string& name) const;

    /**
     * Gets all query parameters.
     * @return Map of parameter name-value pairs
     */
    std::unordered_map<std::string, std::string> get_query_params() const;
};

class Response {
public:
    /**
     * Sets the status code.
     * @param code HTTP status code
     * @return Reference to this response for chaining
     */
    Response& status(int code);

    /**
     * Sets a header.
     * @param name Header name
     * @param value Header value
     * @return Reference to this response for chaining
     */
    Response& header(const std::string& name, const std::string& value);

    /**
     * Sets the response body.
     * @param body Response body
     * @return Reference to this response for chaining
     */
    Response& body(const std::string& body);

    /**
     * Sets JSON response body.
     * @param json JSON object
     * @return Reference to this response for chaining
     */
    Response& json(const nlohmann::json& json);

    /**
     * Sends a file as response.
     * @param file_path Path to file
     * @return Reference to this response for chaining
     */
    Response& send_file(const std::string& file_path);
};

} // namespace network::http
```

### WebSocket Handler

WebSocket protocol implementation.

```cpp
namespace network::websocket {

class WebSocketServer {
public:
    /**
     * WebSocket connection handler.
     */
    using ConnectionHandler = std::function<void(WebSocketConnection&)>;

    /**
     * Constructs a WebSocket server.
     * @param config Server configuration
     */
    explicit WebSocketServer(const WebSocketConfig& config);

    /**
     * Sets the connection handler.
     * @param handler Connection handler function
     */
    void on_connection(ConnectionHandler handler);

    /**
     * Starts the WebSocket server.
     * @return Result<void> indicating success or failure
     */
    Result<void> start();

    /**
     * Stops the WebSocket server.
     * @return Result<void> indicating success or failure
     */
    Result<void> stop();

    /**
     * Broadcasts a message to all connected clients.
     * @param message Message to broadcast
     */
    void broadcast(const std::string& message);

    /**
     * Broadcasts binary data to all connected clients.
     * @param data Binary data
     * @param size Data size
     */
    void broadcast_binary(const uint8_t* data, size_t size);
};

class WebSocketConnection {
public:
    /**
     * Sends a text message.
     * @param message Text message
     * @return Result<void> indicating success or failure
     */
    Result<void> send(const std::string& message);

    /**
     * Sends binary data.
     * @param data Binary data
     * @param size Data size
     * @return Result<void> indicating success or failure
     */
    Result<void> send_binary(const uint8_t* data, size_t size);

    /**
     * Sets the message handler.
     * @param handler Message handler function
     */
    void on_message(std::function<void(const std::string&)> handler);

    /**
     * Sets the binary message handler.
     * @param handler Binary message handler function
     */
    void on_binary(std::function<void(const uint8_t*, size_t)> handler);

    /**
     * Sets the close handler.
     * @param handler Close handler function
     */
    void on_close(std::function<void(int code, const std::string& reason)> handler);

    /**
     * Closes the connection.
     * @param code Close code
     * @param reason Close reason
     */
    void close(int code = 1000, const std::string& reason = "");

    /**
     * Gets connection information.
     * @return ConnectionInfo object
     */
    ConnectionInfo get_info() const;
};

} // namespace network::websocket

---


## Thread Integration API

The thread integration module provides unified thread pool management for network operations.

### Header Files

```cpp
#include <kcenon/network/integration/thread_integration.h>
#include <kcenon/network/integration/thread_system_adapter.h>
```

### thread_pool_interface

Abstract interface for thread pool implementations.

```cpp
namespace kcenon::network::integration {

class thread_pool_interface {
public:
    virtual ~thread_pool_interface() = default;

    /**
     * @brief Submit a task to the thread pool
     * @param task The task to execute
     * @return Future for the task result
     */
    virtual std::future<void> submit(std::function<void()> task) = 0;

    /**
     * @brief Submit a task with delay
     * @param task The task to execute
     * @param delay Delay before execution
     * @return Future for the task result
     */
    virtual std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) = 0;

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

    /**
     * @brief Get pending task count
     * @return Number of tasks waiting to be executed
     */
    virtual size_t pending_tasks() const = 0;
};

} // namespace kcenon::network::integration
```

### basic_thread_pool

Default thread pool implementation. When `BUILD_WITH_THREAD_SYSTEM` is enabled, internally delegates to `thread_system::thread_pool`.

```cpp
namespace kcenon::network::integration {

class basic_thread_pool : public thread_pool_interface {
public:
    /**
     * @brief Construct with specified number of threads
     * @param num_threads Number of worker threads (0 = hardware concurrency)
     */
    explicit basic_thread_pool(size_t num_threads = 0);

    ~basic_thread_pool();

    // thread_pool_interface implementation
    std::future<void> submit(std::function<void()> task) override;
    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) override;
    size_t worker_count() const override;
    bool is_running() const override;
    size_t pending_tasks() const override;

    /**
     * @brief Stop the thread pool
     * @param wait_for_tasks Whether to wait for pending tasks
     */
    void stop(bool wait_for_tasks = true);

    /**
     * @brief Get completed tasks count
     * @return Number of completed tasks
     */
    size_t completed_tasks() const;
};

} // namespace kcenon::network::integration
```

### thread_integration_manager

Singleton manager for thread system integration.

```cpp
namespace kcenon::network::integration {

class thread_integration_manager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static thread_integration_manager& instance();

    /**
     * @brief Set the thread pool implementation
     * @param pool Thread pool to use
     */
    void set_thread_pool(std::shared_ptr<thread_pool_interface> pool);

    /**
     * @brief Get the current thread pool
     * @return Current thread pool (creates basic pool if none set)
     */
    std::shared_ptr<thread_pool_interface> get_thread_pool();

    /**
     * @brief Submit a task to the thread pool
     * @param task The task to execute
     * @return Future for the task result
     */
    std::future<void> submit_task(std::function<void()> task);

    /**
     * @brief Submit a task with delay
     * @param task The task to execute
     * @param delay Delay before execution
     * @return Future for the task result
     */
    std::future<void> submit_delayed_task(
        std::function<void()> task,
        std::chrono::milliseconds delay
    );

    /**
     * @brief Thread pool metrics
     */
    struct metrics {
        size_t worker_threads = 0;  ///< Number of worker threads
        size_t pending_tasks = 0;   ///< Tasks waiting to execute
        size_t completed_tasks = 0; ///< Total completed tasks
        bool is_running = false;    ///< Thread pool running state
    };

    /**
     * @brief Get current metrics
     * @return Current thread pool metrics
     */
    metrics get_metrics() const;
};

} // namespace kcenon::network::integration
```

### thread_system_pool_adapter

Adapter for direct thread_system::thread_pool integration (requires `BUILD_WITH_THREAD_SYSTEM`).

```cpp
#if KCENON_WITH_THREAD_SYSTEM

namespace kcenon::network::integration {

class thread_system_pool_adapter : public thread_pool_interface {
public:
    /**
     * @brief Construct adapter with existing thread_pool
     * @param pool The thread_system thread pool to adapt
     */
    explicit thread_system_pool_adapter(
        std::shared_ptr<kcenon::thread::thread_pool> pool
    );

    ~thread_system_pool_adapter();

    // thread_pool_interface implementation
    std::future<void> submit(std::function<void()> task) override;
    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay
    ) override;
    size_t worker_count() const override;
    bool is_running() const override;
    size_t pending_tasks() const override;

    /**
     * @brief Create default adapter with new thread pool
     * @param pool_name Name for the thread pool
     * @return Shared pointer to new adapter
     */
    static std::shared_ptr<thread_system_pool_adapter> create_default(
        const std::string& pool_name = "network_pool"
    );

    /**
     * @brief Try resolving from service container, fallback to default
     * @param pool_name Name for the thread pool
     * @return Shared pointer to adapter
     */
    static std::shared_ptr<thread_system_pool_adapter> from_service_or_default(
        const std::string& pool_name = "network_pool"
    );
};

/**
 * @brief Bind thread_system pool to the integration manager
 * @param pool_name Name for the thread pool
 * @return true on success, false otherwise
 */
bool bind_thread_system_pool_into_manager(
    const std::string& pool_name = "network_pool"
);

} // namespace kcenon::network::integration

#endif // BUILD_WITH_THREAD_SYSTEM
```

### Usage Examples

#### Basic Usage

```cpp
#include <kcenon/network/integration/thread_integration.h>

void example_basic_usage() {
    auto& manager = kcenon::network::integration::thread_integration_manager::instance();

    // Submit a simple task
    auto future = manager.submit_task([]() {
        std::cout << "Task executed!" << std::endl;
    });

    // Wait for completion
    future.get();

    // Submit delayed task
    auto delayed_future = manager.submit_delayed_task(
        []() { std::cout << "Delayed task!" << std::endl; },
        std::chrono::milliseconds(500)
    );

    delayed_future.get();

    // Check metrics
    auto metrics = manager.get_metrics();
    std::cout << "Completed: " << metrics.completed_tasks << std::endl;
}
```

#### Custom Thread Pool

```cpp
#include <kcenon/network/integration/thread_integration.h>

void example_custom_pool() {
    // Create custom thread pool with 8 workers
    auto pool = std::make_shared<kcenon::network::integration::basic_thread_pool>(8);

    // Set as the global thread pool
    auto& manager = kcenon::network::integration::thread_integration_manager::instance();
    manager.set_thread_pool(pool);

    // Use the manager as usual
    manager.submit_task([]() { /* ... */ });
}
```

#### thread_system Integration

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

void example_thread_system_integration() {
#if KCENON_WITH_THREAD_SYSTEM
    using namespace kcenon::network::integration;

    // Option 1: Use convenience function
    bind_thread_system_pool_into_manager("my_network_pool");

    // Option 2: Manual setup
    auto adapter = thread_system_pool_adapter::from_service_or_default("network_pool");
    thread_integration_manager::instance().set_thread_pool(adapter);

    // Now all network operations use thread_system
#endif
}
```



## Version History

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

## License

This library is licensed under the MIT License. See LICENSE file for details.
---

*Last Updated: 2025-12-10*
