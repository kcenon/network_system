# Integrating Common System with Network System

## Overview

The network system depends on common_system for:
- Core interfaces (`ILogger`, `IMonitor`, `IExecutor`)
- Error handling with `Result<T>`
- RAII resource management
- Type utilities and concepts
- Smart pointer patterns

## Dependencies

```cmake
target_link_libraries(network_system
    PUBLIC
        common_system  # Public dependency
)
```

## Core Interfaces

### ILogger Interface

All network components accept `ILogger` for logging:

```cpp
#include "common_system/interfaces/ILogger.h"
#include "network_system/TcpServer.h"

class MyNetworkApp {
public:
    MyNetworkApp(std::shared_ptr<ILogger> logger)
        : logger_(std::move(logger)) {}

    void start() {
        auto server = network_system::TcpServer::create(8080);
        server->set_logger(logger_);  // Inject ILogger
        server->listen();
    }

private:
    std::shared_ptr<ILogger> logger_;
};
```

### IMonitor Interface

Network components support `IMonitor` for metrics:

```cpp
#include "common_system/interfaces/IMonitor.h"
#include "network_system/TcpServer.h"

auto monitor = /* your IMonitor implementation */;
auto server = network_system::TcpServer::create({
    .port = 8080,
    .monitor = monitor
});

// Network system automatically tracks:
// - connection_count
// - bytes_sent
// - bytes_received
// - error_count
// - request_duration
```

### IExecutor Interface

For async task execution:

```cpp
#include "common_system/interfaces/IExecutor.h"
#include "network_system/TcpServer.h"

auto executor = /* your IExecutor implementation */;

server->on_connection([executor](auto conn) {
    executor->execute([conn]() {
        // Handle connection in thread pool
        handleConnection(conn);
    });
});
```

## Error Handling with Result<T>

Network system uses `Result<T>` for all fallible operations. As of the ecosystem-wide
Result<T> unification initiative, **`kcenon::common::Result<T>` is the primary API**.

### Migration Notice

The `network_system::Result<T>` alias is retained for backward compatibility but
new code should use `kcenon::common::Result<T>` directly for ecosystem consistency.

To enable deprecation warnings during migration preparation, define:
```cpp
#define NETWORK_SYSTEM_ENABLE_DEPRECATION_WARNINGS
```

See [common_system Result<T> Unification](https://github.com/kcenon/common_system/issues/205)
for more details on the ecosystem-wide migration.

### Basic Usage

```cpp
#include <kcenon/common/patterns/result.h>
#include <kcenon/network/core/messaging_client.h>

// Use common::Result<T> directly (recommended)
using kcenon::common::Result;
using kcenon::common::VoidResult;

auto client = std::make_shared<network_system::core::messaging_client>("client");

// Connect returns VoidResult
auto conn_result = client->start_client("example.com", 80);

if (conn_result.is_err()) {
    // Handle error
    std::cerr << "Connection failed: " << conn_result.error().message << std::endl;
    return;
}

// ... use client ...
```

### Propagating Errors

```cpp
Result<Response> sendRequest(const std::string& host, const Request& req) {
    auto client = network_system::TcpClient::create();

    // Use TRY macro to propagate errors
    auto conn = TRY(client->connect(host, 80));
    auto response = TRY(conn->send(req));

    return Ok(response);
}

// Usage
auto result = sendRequest("example.com", request);
if (!result) {
    logger->error("Request failed: {}", result.error());
}
```

### Async Result

For coroutine-based async operations:

```cpp
#include "common_system/async_result.h"

async_result<Data> fetchData(const std::string& url) {
    auto client = network_system::HttpClient::create();

    auto response = co_await client->get(url);
    if (!response) {
        co_return Error(response.error());
    }

    co_return Ok(response->body());
}

// Usage with co_await
auto data = co_await fetchData("https://api.example.com/data");
if (!data) {
    logger->error("Fetch failed: {}", data.error());
}
```

## RAII Resource Management

Network system follows common_system's RAII patterns:

### Automatic Connection Cleanup

```cpp
{
    auto client = network_system::TcpClient::create();
    auto conn = client->connect("example.com", 80);

    // ... use connection ...

} // Connection automatically closed when conn goes out of scope
```

### Connection Pools

```cpp
#include "network_system/ConnectionPool.h"

auto pool = network_system::ConnectionPool::create({
    .host = "db.example.com",
    .port = 5432,
    .min_connections = 5,
    .max_connections = 20
});

{
    // Acquire connection (RAII)
    auto conn = pool->acquire();

    // ... use connection ...

} // Connection automatically returned to pool
```

### Scoped Guards

```cpp
#include "common_system/scope_guard.h"

auto server = network_system::TcpServer::create(8080);

// Ensure server stops on scope exit
auto guard = common_system::make_scope_guard([&server]() {
    server->stop();
});

server->listen();
```

## Smart Pointer Patterns

Network system follows common_system's smart pointer guidelines:

### Ownership

```cpp
// Factory returns unique_ptr for exclusive ownership
std::unique_ptr<TcpClient> client = network_system::TcpClient::create();

// shared_ptr for shared ownership
std::shared_ptr<ILogger> logger = /* ... */;
server->set_logger(logger);  // Shared with server
```

### Avoiding Circular References

```cpp
class Connection {
public:
    void set_server(std::weak_ptr<TcpServer> server) {
        server_ = server;  // weak_ptr breaks circular reference
    }

private:
    std::weak_ptr<TcpServer> server_;  // Not shared_ptr!
};
```

## Type Utilities

### Network System Concepts

network_system provides 16 C++20 concepts for compile-time type validation. These concepts improve code quality through better error messages and self-documenting interfaces.

```cpp
#include <kcenon/network/concepts/concepts.h>

using namespace network_system::concepts;

// Data buffer concepts
template<ByteBuffer Buffer>
void send_data(const Buffer& buffer) {
    // buffer.data() and buffer.size() are guaranteed at compile time
}

// Callback concepts
template<DataReceiveHandler Handler>
void set_receive_handler(Handler&& handler) {
    // Handler must be callable with const std::vector<uint8_t>&
}

// Network component concepts
template<NetworkClient Client>
void use_client(Client& client) {
    // Client must have is_connected(), send_packet(), stop_client()
}
```

#### Available Concepts

| Category | Concepts |
|----------|----------|
| **Data Buffers** | `ByteBuffer`, `MutableByteBuffer` |
| **Callbacks** | `DataReceiveHandler`, `ErrorHandler`, `ConnectionHandler`, `SessionHandler`, `SessionDataHandler`, `SessionErrorHandler`, `DisconnectionHandler`, `RetryCallback` |
| **Components** | `NetworkClient`, `NetworkServer`, `NetworkSession` |
| **Pipeline** | `DataTransformer`, `ReversibleDataTransformer` |
| **Utility** | `Duration` |

See [advanced/CONCEPTS.md](../advanced/CONCEPTS.md) for detailed documentation.

### common_system Concepts Integration

When `BUILD_WITH_COMMON_SYSTEM` is enabled, additional concepts from common_system are available:

```cpp
#include <kcenon/network/concepts/concepts.h>

#if KCENON_WITH_COMMON_SYSTEM
// Re-exported from common_system
using common_system::concepts::Resultable;
using common_system::concepts::Unwrappable;
using common_system::concepts::Mappable;

template<Resultable T>
void handle_result(T&& result) {
    if (result.is_ok()) {
        // Process success...
    }
}
#endif
```

### Concepts with common_system Serializable

```cpp
#include "common_system/concepts.h"

template<common_system::Serializable T>
Result<void> sendData(const T& data) {
    auto serialized = serialize(data);
    return connection->send(serialized);
}
```

### Type Traits

```cpp
#include "common_system/type_traits.h"

template<typename T>
requires common_system::is_trivially_serializable_v<T>
Result<void> sendPOD(const T& data) {
    return connection->send_raw(&data, sizeof(T));
}
```

### Combining Concepts

You can combine network_system and common_system concepts for more precise constraints:

```cpp
#include <kcenon/network/concepts/concepts.h>

template<typename T>
    requires network_system::concepts::DataTransformer<T> &&
             std::default_initializable<T>
auto create_transformer() -> T {
    return T{};
}

// With common_system integration
#if KCENON_WITH_COMMON_SYSTEM
template<typename Handler, typename ResultType>
    requires network_system::concepts::DataReceiveHandler<Handler> &&
             common_system::concepts::Resultable<ResultType>
void process_with_result(Handler&& h, ResultType&& result) {
    // Both concepts satisfied
}
#endif
```

## Configuration

Network system uses common_system's configuration pattern:

```cpp
#include "common_system/config.h"

struct NetworkConfig {
    uint16_t port = 8080;
    size_t max_connections = 100;
    std::chrono::seconds timeout{30};
    bool enable_tls = false;
};

auto config = common_system::Config<NetworkConfig>::load("network.toml");
auto server = network_system::TcpServer::create(config.port);
server->set_max_connections(config.max_connections);
```

## Best Practices

### 1. Always Use Result<T> for Fallible Operations

✅ **Good**:
```cpp
Result<Connection> connect(const std::string& host, uint16_t port);
```

❌ **Bad**:
```cpp
Connection connect(const std::string& host, uint16_t port);  // Can fail!
```

### 2. Accept Interfaces, Not Implementations

✅ **Good**:
```cpp
void setLogger(std::shared_ptr<ILogger> logger);
```

❌ **Bad**:
```cpp
void setLogger(std::shared_ptr<Logger> logger);  // Concrete type!
```

### 3. Use RAII for All Resources

✅ **Good**:
```cpp
auto conn = pool->acquire();  // RAII
// ... use conn ...
// Automatic cleanup
```

❌ **Bad**:
```cpp
Connection* conn = pool->acquire_raw();
// ... use conn ...
pool->release(conn);  // Manual cleanup - error-prone!
```

### 4. Prefer unique_ptr for Exclusive Ownership

✅ **Good**:
```cpp
std::unique_ptr<TcpClient> client = TcpClient::create();
```

❌ **Bad**:
```cpp
std::shared_ptr<TcpClient> client = TcpClient::create();  // Unnecessary sharing
```

## Common Patterns

### Builder Pattern with Result

```cpp
auto server = network_system::TcpServer::builder()
    .port(8080)
    .max_connections(100)
    .logger(logger)
    .monitor(monitor)
    .build();  // Returns Result<TcpServer>

if (!server) {
    logger->error("Failed to create server: {}", server.error());
    return;
}
```

### Chain of Results

```cpp
Result<Response> processRequest(const Request& req) {
    auto validated = TRY(validate(req));
    auto authorized = TRY(authorize(validated));
    auto processed = TRY(process(authorized));
    auto response = TRY(format(processed));

    return Ok(response);
}
```

### Optional with Result

```cpp
#include "common_system/optional.h"

Result<std::optional<User>> findUser(int id) {
    auto conn = TRY(pool->acquire());
    auto result = TRY(conn->query("SELECT * FROM users WHERE id = $1", id));

    if (result.empty()) {
        return Ok(std::nullopt);  // User not found (not an error)
    }

    return Ok(User::from_row(result[0]));
}
```

## Performance Considerations

- `Result<T>` has zero overhead compared to exceptions in success path
- RAII automatic cleanup is as fast as manual cleanup
- Smart pointers have minimal overhead (usually just atomic increment/decrement)
- Interfaces use virtual dispatch (small overhead, usually acceptable)

## Troubleshooting

### Linker Errors

**Problem**: Undefined reference to common_system symbols

**Solution**: Link common_system
```cmake
target_link_libraries(my_app
    PRIVATE
        network_system
        common_system  # Explicitly link
)
```

### Result<T> Errors

**Problem**: Cannot convert to bool

**Solution**: Use `.is_ok()` or `!` operator
```cpp
auto result = connect(host, port);
if (result.is_ok()) {  // Or: if (result)
    // ...
}
```

## See Also

- [common_system Documentation](../../../common_system/README.md)
- [common_system Result Guide](../../../common_system/docs/guides/RESULT.md)
- [common_system RAII Guide](../../../common_system/docs/guides/RAII.md)
- [Ecosystem Integration Guide](../../../ECOSYSTEM_INTEGRATION.md)
