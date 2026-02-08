# Network System Integration Guide

## Overview

This directory contains integration guides for using network_system with other KCENON systems and components.

## Integration Guides

- [With Common System](with-common-system.md) - Foundation interfaces and error handling
- [With Logger System](with-logger.md) - Logging network events and errors
- [With Monitoring System](with-monitoring.md) - Network metrics and observability
- [With Thread System](with-thread-system.md) - Async operations and thread pooling
- [With Database System](with-database-system.md) - Network-based database protocols

## Quick Start

### Basic TCP Server with Logging

```cpp
#include "network_system/TcpServer.h"
#include "logger_system/Logger.h"

int main() {
    auto logger = logger_system::createLogger("network.log");

    auto server = network_system::TcpServer::create(8080);
    server->set_logger(logger);

    server->on_connection([logger](auto conn) {
        logger->info("New connection from: {}", conn->remote_address());
    });

    server->listen();
}
```

### TCP Server with Full Observability

```cpp
#include "network_system/TcpServer.h"
#include "logger_system/Logger.h"
#include "monitoring_system/Monitor.h"
#include "thread_system/ThreadPool.h"

int main() {
    auto logger = logger_system::createLogger("network.log");
    auto monitor = monitoring_system::createMonitor();
    auto thread_pool = thread_system::ThreadPool(4);

    auto server = network_system::TcpServer::create({
        .port = 8080,
        .logger = logger,
        .monitor = monitor
    });

    server->on_connection([&](auto conn) {
        monitor->incrementCounter("connections.total");
        logger->info("Connection from: {}", conn->remote_address());

        thread_pool.enqueue([conn, logger, monitor]() {
            auto timer = monitor->startTimer("connection.duration");
            handleConnection(conn);
        });
    });

    server->listen();
}
```

## Integration Patterns

### Dependency Injection

Network system components accept dependencies via constructor or setters:

```cpp
// Constructor injection
auto server = network_system::TcpServer::create({
    .port = 8080,
    .logger = logger,
    .monitor = monitor,
    .executor = thread_pool
});

// Setter injection
auto server = network_system::TcpServer::create(8080);
server->set_logger(logger);
server->set_monitor(monitor);
```

### Error Handling

Network system uses `Result<T>` from common_system:

```cpp
auto result = client->connect(host, port);
if (!result) {
    logger->error("Connection failed: {}", result.error());
    return;
}

auto conn = result.value();
// ... use connection ...
```

### Async Operations

Network system supports C++20 coroutines:

```cpp
async_result<Data> fetchData(const std::string& url) {
    auto client = network_system::HttpClient::create();

    auto response = co_await client->get(url);
    if (!response) {
        logger->error("HTTP GET failed: {}", response.error());
        co_return Error(response.error());
    }

    co_return Ok(response->body());
}
```

## Common Use Cases

### 1. HTTP API Server

Dependencies: `logger_system`, `monitoring_system`, `container_system`

See: [with-logger.md](with-logger.md), [with-monitoring.md](with-monitoring.md)

### 2. Database Connection Pooling

Dependencies: `database_system`, `thread_system`

See: [with-database-system.md](with-database-system.md)

### 3. WebSocket Chat Server

Dependencies: `logger_system`, `thread_system`, `container_system`

See: [with-thread-system.md](with-thread-system.md)

## Performance Considerations

- Use async I/O for high-concurrency scenarios
- Configure thread pool size based on workload type (CPU-bound vs I/O-bound)
- Enable connection pooling for database connections
- Use monitoring to track network metrics and identify bottlenecks

See: [../performance/OPTIMIZATION.md](../performance/OPTIMIZATION.md)

## Security

- Always use TLS/SSL for production deployments
- Implement rate limiting and connection limits
- Filter sensitive data from logs
- Monitor for security events

See: [../security/SECURITY.md](../security/SECURITY.md)

## Additional Resources

- [Network System API Reference](../API_REFERENCE.md)
- [Network System Architecture](../ARCHITECTURE.md)
- [Ecosystem Integration Guide](../../../ECOSYSTEM.md)
- [Example Applications](../../../examples/network/)

## Support

For questions or issues:
1. Check system-specific documentation
2. Review [TROUBLESHOOTING guide](../guides/TROUBLESHOOTING.md)
3. File an issue in the network_system repository
