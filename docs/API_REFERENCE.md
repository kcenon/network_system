# Network System API Reference

## Table of Contents
1. [Core Components](#core-components)
2. [Integration Layer](#integration-layer)
3. [Session Management](#session-management)
4. [Error Handling](#error-handling)
5. [Advanced Features](#advanced-features)

## Core Components

### messaging_server

High-performance asynchronous TCP server implementation.

```cpp
namespace network_system::core {
    class messaging_server;
}
```

#### Constructor
```cpp
explicit messaging_server(const std::string& server_id = "server");
```
Creates a new server instance with the specified identifier.

#### Methods

##### start_server
```cpp
void start_server(uint16_t port);
```
Starts the server listening on the specified port.
- **Parameters**: `port` - Port number to listen on (1-65535)
- **Throws**: `std::runtime_error` if port is already in use

##### stop_server
```cpp
void stop_server();
```
Stops the server and closes all active connections.

##### get_session_count
```cpp
size_t get_session_count() const;
```
Returns the number of active client sessions.

#### Example
```cpp
auto server = std::make_shared<network_system::core::messaging_server>("my_server");
server->start_server(8080);
// Server is now listening...
server->stop_server();
```

### messaging_client

Asynchronous TCP client for connecting to servers.

```cpp
namespace network_system::core {
    class messaging_client;
}
```

#### Constructor
```cpp
explicit messaging_client(const std::string& client_id = "client");
```
Creates a new client instance with the specified identifier.

#### Methods

##### start_client
```cpp
void start_client(const std::string& host, uint16_t port);
```
Connects to a server at the specified host and port.
- **Parameters**:
  - `host` - Hostname or IP address
  - `port` - Port number
- **Throws**: `std::runtime_error` if connection fails

##### stop_client
```cpp
void stop_client();
```
Disconnects from the server.

##### send_packet
```cpp
void send_packet(const std::vector<uint8_t>& data);
```
Sends data to the connected server.
- **Parameters**: `data` - Binary data to send
- **Throws**: `std::runtime_error` if not connected

##### is_connected
```cpp
bool is_connected() const;
```
Returns true if client is connected to a server.

#### Example
```cpp
auto client = std::make_shared<network_system::core::messaging_client>("my_client");
client->start_client("localhost", 8080);

std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
client->send_packet(data);

client->stop_client();
```

## Integration Layer

### messaging_bridge

Bridge component for legacy system compatibility.

```cpp
namespace network_system::integration {
    class messaging_bridge;
}
```

#### Methods

##### set_container_interface
```cpp
void set_container_interface(std::shared_ptr<container_interface> container);
```
Sets the container interface for message serialization.

##### set_thread_pool_interface
```cpp
void set_thread_pool_interface(std::shared_ptr<thread_pool_interface> pool);
```
Sets the thread pool for async operations.

##### create_server
```cpp
std::shared_ptr<core::messaging_server> create_server(const std::string& id);
```
Factory method to create a server instance.

##### create_client
```cpp
std::shared_ptr<core::messaging_client> create_client(const std::string& id);
```
Factory method to create a client instance.

##### get_metrics
```cpp
BridgeMetrics get_metrics() const;
```
Returns performance metrics.

#### BridgeMetrics Structure
```cpp
struct BridgeMetrics {
    size_t messages_sent;
    size_t messages_received;
    size_t bytes_sent;
    size_t bytes_received;
    size_t errors_count;
    size_t connections_active;
    size_t connections_total;
};
```

### thread_integration_manager

Singleton manager for thread pool integration.

```cpp
namespace network_system::integration {
    class thread_integration_manager;
}
```

#### Methods

##### instance
```cpp
static thread_integration_manager& instance();
```
Returns the singleton instance.

##### submit_task
```cpp
template<typename F>
std::future<void> submit_task(F&& task);
```
Submits a task to the thread pool.

##### submit_delayed_task
```cpp
template<typename F>
std::future<void> submit_delayed_task(F&& task, std::chrono::milliseconds delay);
```
Submits a delayed task.

##### get_metrics
```cpp
ThreadMetrics get_metrics() const;
```
Returns thread pool metrics.

#### ThreadMetrics Structure
```cpp
struct ThreadMetrics {
    size_t worker_threads;
    size_t pending_tasks;
    size_t completed_tasks;
    double average_wait_time;
};
```

### container_manager

Singleton manager for container serialization.

```cpp
namespace network_system::integration {
    class container_manager;
}
```

#### Methods

##### instance
```cpp
static container_manager& instance();
```
Returns the singleton instance.

##### register_container
```cpp
void register_container(const std::string& name,
                        std::shared_ptr<container_interface> container);
```
Registers a container implementation.

##### serialize
```cpp
std::vector<uint8_t> serialize(const std::any& data);
```
Serializes data using the default container.

##### deserialize
```cpp
std::any deserialize(const std::vector<uint8_t>& bytes);
```
Deserializes data using the default container.

## Session Management

### messaging_session

Represents a client connection session on the server.

```cpp
namespace network_system::session {
    class messaging_session;
}
```

#### Methods

##### get_session_id
```cpp
const std::string& get_session_id() const;
```
Returns the unique session identifier.

##### send
```cpp
void send(const std::vector<uint8_t>& data);
```
Sends data to the connected client.

##### close
```cpp
void close();
```
Closes the session.

##### is_active
```cpp
bool is_active() const;
```
Returns true if session is active.

## Error Handling

### Exception Types

#### network_error
Base exception for all network-related errors.
```cpp
class network_error : public std::runtime_error {
    // Error code and detailed message
};
```

#### connection_error
Thrown when connection operations fail.
```cpp
class connection_error : public network_error {
    // Connection-specific error details
};
```

#### timeout_error
Thrown when operations timeout.
```cpp
class timeout_error : public network_error {
    // Timeout duration and operation details
};
```

### Error Handling Best Practices

```cpp
try {
    client->start_client("host", 8080);
} catch (const network_system::connection_error& e) {
    std::cerr << "Connection failed: " << e.what() << std::endl;
    // Handle connection failure
} catch (const network_system::timeout_error& e) {
    std::cerr << "Connection timeout: " << e.what() << std::endl;
    // Handle timeout
} catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << std::endl;
}
```

## Advanced Features

### Coroutine Support

Network System supports C++20 coroutines for asynchronous operations.

```cpp
namespace network_system::internal {
    class send_coroutine;
}
```

#### Example
```cpp
co_await client->async_send(data);
co_await server->async_accept();
```

### Pipeline Processing

Message pipeline for efficient batch processing.

```cpp
namespace network_system::internal {
    class pipeline;
}
```

#### Configuration
```cpp
pipeline.set_batch_size(100);
pipeline.set_flush_interval(std::chrono::milliseconds(10));
```

### Performance Tuning

#### Server Configuration
```cpp
server->set_max_connections(10000);
server->set_receive_buffer_size(65536);
server->set_send_buffer_size(65536);
server->set_keep_alive(true);
server->set_no_delay(true);  // Disable Nagle's algorithm
```

#### Client Configuration
```cpp
client->set_connect_timeout(std::chrono::seconds(5));
client->set_receive_timeout(std::chrono::seconds(30));
client->set_reconnect_interval(std::chrono::seconds(1));
client->set_max_reconnect_attempts(5);
```

## Compatibility Layer

### Legacy Namespace Support

For backward compatibility with messaging_system:

```cpp
// Legacy namespace aliases
namespace network_module {
    using messaging_server = network_system::core::messaging_server;
    using messaging_client = network_system::core::messaging_client;

    // Factory functions
    inline auto create_server(const std::string& id) {
        return std::make_shared<messaging_server>(id);
    }

    inline auto create_client(const std::string& id) {
        return std::make_shared<messaging_client>(id);
    }
}
```

### Migration Helper Functions

```cpp
namespace network_system::compat {
    // Initialize the network system
    void initialize();

    // Shutdown the network system
    void shutdown();

    // Check feature availability
    bool has_container_support();
    bool has_thread_support();

    // Get version information
    const char* version();
}
```

## Thread Safety

All public methods in the Network System are thread-safe unless explicitly documented otherwise.

### Thread-Safe Components
- `messaging_server` - All methods
- `messaging_client` - All methods
- `thread_integration_manager` - Singleton, all methods
- `container_manager` - Singleton, all methods

### Not Thread-Safe
- `messaging_session` - Should only be accessed from the owning server thread
- `pipeline` - Internal component, not for direct use

## Memory Management

Network System uses RAII and smart pointers for automatic memory management.

### Best Practices
1. Always use `std::shared_ptr` for server and client instances
2. Sessions are managed automatically by the server
3. Message buffers are pooled for efficiency
4. Avoid circular references with callbacks

### Example
```cpp
{
    auto server = std::make_shared<messaging_server>();
    server->start_server(8080);
    // Server automatically cleaned up when out of scope
}
```

## Performance Considerations

### Benchmarks
- Throughput: 305,000+ messages/second
- Latency: < 600μs average
- Concurrent connections: 500+ tested
- Memory usage: ~10KB per connection

### Optimization Tips
1. Use batch sending when possible
2. Enable TCP_NODELAY for low-latency requirements
3. Adjust buffer sizes based on message patterns
4. Use thread pool for CPU-intensive processing
5. Enable connection pooling for client applications

## Platform-Specific Notes

### Linux
- Requires libpthread
- Optimal with kernel 5.0+
- Enable TCP_FASTOPEN for better performance

### macOS
- Requires Xcode Command Line Tools
- Use Homebrew for dependency management
- Enable SO_REUSEPORT for load balancing

### Windows
- Requires Windows 10 or later
- Visual Studio 2019+ recommended
- Link with ws2_32 and mswsock

## Version History

### v2.0.0 (Current)
- Complete separation from messaging_system
- C++20 coroutine support
- Thread pool integration
- Container serialization support
- Performance improvements (3x throughput)

### v1.0.0
- Initial release as part of messaging_system

---

For more examples and tutorials, see the [samples](../samples/) directory.