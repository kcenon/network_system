# Integrating Logger System with Network System

## Overview

The network system provides unified logging through common_system's ILogger interface and
GlobalLoggerRegistry. This allows for runtime binding of logger implementations without
direct compile-time dependencies on logger_system.

**Note (Issue #285)**: As of this version, network_system uses common_system's logging
infrastructure by default. logger_system is now an optional runtime dependency.

### Key Features:
- Connection lifecycle logging
- Error and exception logging
- Performance logging
- Protocol-level debug tracing
- Security event logging
- Runtime logger binding via GlobalLoggerRegistry

## Dependencies

```cmake
# Required: common_system provides ILogger interface
target_link_libraries(my_network_app
    PRIVATE
        network_system
        common_system    # Required - provides ILogger and GlobalLoggerRegistry
        # logger_system  # Optional - for enhanced logging features
)
```

## Quick Start

### Basic TCP Server Logging

```cpp
#include "network_system/TcpServer.h"
#include "logger_system/Logger.h"

int main() {
    // Create logger
    auto logger = logger_system::createLogger({
        .filename = "network.log",
        .level = LogLevel::INFO,
        .async = true
    });

    // Create server with logger
    auto server = network_system::TcpServer::create(8080);
    server->set_logger(logger);

    server->on_connection([logger](auto conn) {
        logger->info("New connection from: {}", conn->remote_address());
    });

    server->on_disconnection([logger](auto conn) {
        logger->info("Disconnected: {}", conn->remote_address());
    });

    server->on_error([logger](const auto& error) {
        logger->error("Server error: {}", error.message());
    });

    logger->info("Starting TCP server on port 8080");
    server->listen();
}
```

### HTTP Client with Request Logging

```cpp
#include "network_system/HttpClient.h"
#include "logger_system/Logger.h"

auto logger = logger_system::createLogger("http_client.log");
auto client = network_system::HttpClient::create(logger);

client->on_request([logger](const auto& req) {
    logger->debug("HTTP {} {}", req.method(), req.url());
});

client->on_response([logger](const auto& resp) {
    logger->info("HTTP {} - {}", resp.status_code(), resp.reason());
});

auto response = client->get("https://api.example.com/data");
```

## Logging Levels

| Event | Log Level | Example |
|-------|-----------|---------|
| Server start/stop | INFO | `Server started on port 8080` |
| New connection | INFO | `New connection from 192.168.1.100:54321` |
| Connection closed | INFO | `Connection closed: 192.168.1.100:54321` |
| Data transfer | DEBUG | `Sent 1024 bytes to 192.168.1.100` |
| Protocol negotiation | DEBUG | `TLS handshake completed with TLS 1.3` |
| Connection errors | ERROR | `Connection failed: Connection refused` |
| Protocol errors | ERROR | `HTTP parse error: Invalid header` |
| Internal errors | ERROR | `Socket error: ECONNRESET` |
| Detailed protocol | TRACE | `Received HTTP headers: ...` |
| Raw data dumps | TRACE | `Raw bytes: 0x48 0x54 0x54 0x50...` |

## Configuration

### Log Rotation for High-Traffic Servers

```cpp
auto logger = logger_system::createLogger({
    .filename = "network.log",
    .level = LogLevel::INFO,
    .rotation = logger_system::Rotation::Size,
    .max_size = 100 * 1024 * 1024,  // 100 MB
    .max_files = 10,
    .async = true,
    .buffer_size = 8192
});
```

### Separate Logs for Different Components

```cpp
// Connection logging
auto conn_logger = logger_system::createLogger("connections.log");

// Error logging
auto error_logger = logger_system::createLogger({
    .filename = "network_errors.log",
    .level = LogLevel::WARN
});

// Performance logging
auto perf_logger = logger_system::createLogger("network_perf.log");

auto server = network_system::TcpServer::create({
    .port = 8080,
    .connection_logger = conn_logger,
    .error_logger = error_logger,
    .performance_logger = perf_logger
});
```

### Filtering Sensitive Data

```cpp
auto logger = logger_system::createLogger({
    .filename = "network.log",
    .filters = {
        logger_system::Filter::redact("password"),
        logger_system::Filter::redact("api_key"),
        logger_system::Filter::redact("Authorization")
    }
});

// This will log: "Request: GET /api?api_key=***REDACTED***"
logger->info("Request: GET /api?api_key=secret123");
```

## Advanced Usage

### Contextual Logging with Request IDs

```cpp
server->on_connection([logger](auto conn) {
    auto request_id = generate_uuid();

    // Create child logger with context
    auto req_logger = logger->with_context({
        {"request_id", request_id},
        {"remote_addr", conn->remote_address()}
    });

    req_logger->info("Processing request");
    // ... handle request ...
    req_logger->info("Request completed");
});
```

### Performance Logging

```cpp
server->on_connection([logger](auto conn) {
    auto start = std::chrono::steady_clock::now();

    conn->on_close([logger, start]() {
        auto duration = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        logger->info("Connection duration: {}ms", ms.count());
    });
});
```

### Structured Logging

```cpp
#include "logger_system/StructuredLogger.h"

auto logger = logger_system::createStructuredLogger("network.json");

server->on_connection([logger](auto conn) {
    logger->log({
        {"event", "connection"},
        {"remote_addr", conn->remote_address()},
        {"local_port", conn->local_port()},
        {"timestamp", std::time(nullptr)}
    });
});
```

## Best Practices

### 1. Use Async Logging for High-Throughput

```cpp
auto logger = logger_system::createLogger({
    .filename = "network.log",
    .async = true,          // Enable async logging
    .buffer_size = 8192     // Larger buffer for batching
});
```

### 2. Separate Logs by Severity

```cpp
// INFO and above to main log
auto main_logger = logger_system::createLogger({
    .filename = "network.log",
    .level = LogLevel::INFO
});

// DEBUG and TRACE to separate debug log
auto debug_logger = logger_system::createLogger({
    .filename = "network_debug.log",
    .level = LogLevel::TRACE
});
```

### 3. Use Sampling for High-Volume Events

```cpp
size_t packet_count = 0;

conn->on_data([logger, &packet_count](const auto& data) {
    packet_count++;

    // Log every 1000th packet
    if (packet_count % 1000 == 0) {
        logger->debug("Received packet #{}", packet_count);
    }
});
```

### 4. Include Context in Error Logs

```cpp
server->on_error([logger](const auto& error, auto conn) {
    logger->error(
        "Connection error: {} (remote: {}, local_port: {})",
        error.message(),
        conn->remote_address(),
        conn->local_port()
    );
});
```

## Performance Impact

| Log Level | Overhead | Use Case |
|-----------|----------|----------|
| ERROR only | < 1% | Production (minimal logging) |
| WARN + ERROR | ~2-3% | Production (recommended) |
| INFO + above | ~5-10% | Production (standard) |
| DEBUG + above | ~15-25% | Development/Staging |
| TRACE | ~30-50% | Debugging only |

**Recommendation**: Use INFO level with async logging for production.

## Troubleshooting

### Log Files Not Created

**Problem**: Logger created but no log files appear

**Solution**: Check file permissions and path
```cpp
auto logger = logger_system::createLogger({
    .filename = "/var/log/network.log",  // Absolute path
    .create_dirs = true                  // Create parent directories
});
```

### Logs Missing Under Load

**Problem**: Some log entries missing during high traffic

**Solution**: Increase async buffer size
```cpp
auto logger = logger_system::createLogger({
    .async = true,
    .buffer_size = 16384,  // Increase from default 8192
    .overflow_policy = logger_system::OverflowPolicy::Block  // Wait instead of drop
});
```

### Performance Degradation

**Problem**: Logging causing performance issues

**Solution**: Use async logging with appropriate buffer size
```cpp
auto logger = logger_system::createLogger({
    .async = true,
    .buffer_size = 8192,
    .flush_interval = std::chrono::milliseconds(100)
});
```

## Examples

See complete examples in:
- [examples/tcp_server_with_logging/](../../../examples/tcp_server_with_logging/)
- [examples/http_client_with_logging/](../../../examples/http_client_with_logging/)
- [examples/websocket_logging/](../../../examples/websocket_logging/)

## See Also

- [Logger System Documentation](../../../logger_system/README.md)
- [Logger System API Reference](../../../logger_system/docs/API_REFERENCE.md)
- [Network System Logging Guide](../guides/LOGGING.md)
- [Ecosystem Integration Guide](../../../ECOSYSTEM_INTEGRATION.md)
