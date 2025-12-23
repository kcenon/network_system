# Integration Guide

> **Language:** **English** | [한국어](INTEGRATION_KO.md)

This guide explains how to integrate network_system with external systems and libraries.

## Table of Contents
- [Thread System Integration](#thread-system-integration)
- [Container System Integration](#container-system-integration)
- [Logger System Integration](#logger-system-integration)
- [Monitoring System Integration](#monitoring-system-integration)
- [Build Configuration](#build-configuration)

## Thread System Integration

The network_system can optionally integrate with an external thread pool for improved performance.

### Configuration
```cmake
cmake .. -DBUILD_WITH_THREAD_SYSTEM=ON
```

### Architecture

When `BUILD_WITH_THREAD_SYSTEM` is enabled, the `basic_thread_pool` class internally delegates to `thread_system::thread_pool`. This provides:

- **Unified Thread Management**: All thread operations go through thread_system
- **Advanced Features**: Access to adaptive_job_queue, hazard pointers, and worker health monitoring
- **Consistent Metrics**: Thread pool metrics are reported through thread_system's metrics infrastructure
- **Automatic Benefits**: No code changes required - existing `basic_thread_pool` usage automatically gets thread_system features

### Implementation Details

```cpp
// basic_thread_pool now internally uses thread_system::thread_pool
class basic_thread_pool::impl {
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
    // ... delegates all operations to pool_
};
```

When thread_system is not available, `basic_thread_pool` falls back to a standalone std::thread-based implementation.

### Usage
When thread_system is available, network operations will automatically utilize the thread pool for:
- Connection handling
- Message processing
- Async operations
- Send coroutine fallback (non-coroutine path uses `thread_integration_manager::submit_task()`)

### Using thread_system_pool_adapter

For direct access to thread_system features, you can use the `thread_system_pool_adapter`:

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

// Create adapter from service container or create new pool
auto adapter = thread_system_pool_adapter::from_service_or_default("network_pool");

// Or bind thread_system directly to the integration manager
bind_thread_system_pool_into_manager("my_pool");
```

### io_context Thread Manager

The `io_context_thread_manager` provides unified management of asio::io_context execution across all messaging components:

```cpp
#include <kcenon/network/integration/io_context_thread_manager.h>

// Run an io_context on the shared thread pool
auto io_ctx = std::make_shared<asio::io_context>();
auto future = io_context_thread_manager::instance()
    .run_io_context(io_ctx, "my_component");

// Stop when done
io_context_thread_manager::instance().stop_io_context(io_ctx);
future.wait();

// Get metrics
auto metrics = io_context_thread_manager::instance().get_metrics();
// metrics.active_contexts, metrics.total_started, metrics.total_completed
```

Benefits:
- **Centralized Management**: All io_context instances use the same thread pool
- **Consistent Shutdown**: Uniform shutdown behavior across components
- **Reduced Complexity**: Components don't manage their own threads
- **Better Observability**: Metrics for all io_context instances in one place

### Requirements
- thread_system must be installed in `../thread_system`
- Headers should be available at `../thread_system/include`

## Container System Integration

Enable advanced serialization and deserialization capabilities.

### Configuration
```cmake
cmake .. -DBUILD_WITH_CONTAINER_SYSTEM=ON
```

### Features
- Binary serialization
- JSON serialization
- Protocol buffer support
- Custom container types

### API Example
```cpp
#include <network_system/integration/container_integration.h>

// Serialize custom data
auto serialized = container_adapter::serialize(my_data);
client->send_packet(serialized);
```

## Logger System Integration

Provides structured logging with configurable log levels and output formats.

### Configuration
```cmake
cmake .. -DBUILD_WITH_LOGGER_SYSTEM=ON
```

### Features
- **Log Levels**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **Source Location Tracking**: Automatic file, line, and function recording
- **Timestamp Formatting**: Millisecond precision timestamps
- **Conditional Compilation**: Falls back to console output when logger_system is unavailable

### Usage

#### Basic Logging
```cpp
#include <network_system/integration/logger_integration.h>

// Use convenience macros
NETWORK_LOG_INFO("Server started on port " + std::to_string(port));
NETWORK_LOG_ERROR("Connection failed: " + error_message);
NETWORK_LOG_DEBUG("Received " + std::to_string(bytes) + " bytes");
```

#### Advanced Configuration
```cpp
// Get logger instance
auto& logger_mgr = kcenon::network::integration::logger_integration_manager::instance();
auto logger = logger_mgr.get_logger();

// Check if log level is enabled
if (logger->is_enabled(kcenon::network::integration::log_level::debug)) {
    // Perform expensive debug logging
    std::string detailed_state = generate_detailed_state();
    NETWORK_LOG_DEBUG(detailed_state);
}

// Direct logging without macros
logger->log(kcenon::network::integration::log_level::warn,
           "Custom warning message",
           __FILE__, __LINE__, __FUNCTION__);
```

### Implementation Details

The logger integration provides two implementations:

1. **basic_logger**: Standalone console logger
   - Used when BUILD_WITH_LOGGER_SYSTEM is OFF
   - Outputs to std::cout (INFO and below) or std::cerr (WARN and above)
   - Simple timestamp formatting

2. **logger_system_adapter**: External logger integration
   - Used when BUILD_WITH_LOGGER_SYSTEM is ON
   - Wraps kcenon::logger for full-featured logging
   - Supports log file rotation, remote logging, etc.

### Requirements
- When BUILD_WITH_LOGGER_SYSTEM=ON:
  - logger_system must be installed in `../logger_system`
  - Headers should be available at `../logger_system/include`
- C++17 or later for string_view support

## Monitoring System Integration

Provides metrics collection and observability capabilities.

**Important**: This integration is **OPTIONAL** (OFF by default) to prevent circular dependencies with monitoring_system.

### Configuration
```cmake
cmake .. -DBUILD_WITH_MONITORING_SYSTEM=ON
```

### Why Optional?

monitoring_system (Tier 3) can optionally depend on network_system (Tier 4) for HTTP metrics export. Making monitoring_system a required dependency would create a circular dependency. Both integrations are optional and use adapter patterns to avoid compile-time cycles.

### Features
- **Counter metrics**: Track connection counts, bytes transferred
- **Gauge metrics**: Monitor active connections, buffer usage
- **Histogram metrics**: Measure latency distributions
- **Health reporting**: Track connection health status

### Usage

#### Basic Metrics Reporting
```cpp
#include <kcenon/network/integration/monitoring_integration.h>

using namespace kcenon::network::integration;

auto& monitor = monitoring_integration_manager::instance();

// Report metrics
monitor.report_counter("network.connections.total", 1, {{"type", "tcp"}});
monitor.report_gauge("network.connections.active", 42);
monitor.report_histogram("network.latency_ms", 12.5);
```

#### Health Reporting
```cpp
monitor.report_health("connection_1",
    true,   // is_alive
    15.3,   // response_time_ms
    0,      // missed_heartbeats
    0.0);   // packet_loss_rate
```

### Implementation Details

Two implementations are provided:

1. **basic_monitoring**: Standalone mode (default)
   - Used when BUILD_WITH_MONITORING_SYSTEM is OFF
   - Logs metrics to console via logger_system
   - No external dependencies

2. **monitoring_system_adapter**: Integration mode
   - Used when BUILD_WITH_MONITORING_SYSTEM is ON
   - Full monitoring_system integration
   - Uses performance_monitor and health_monitor

### Requirements
- When BUILD_WITH_MONITORING_SYSTEM=ON:
  - monitoring_system must be built first (Tier 3)
  - Headers at `../monitoring_system/include`

For detailed documentation, see [integration/with-monitoring.md](integration/with-monitoring.md).

## Build Configuration

### Complete Build with All Integrations
```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=ON \
    -DBUILD_WITH_CONTAINER_SYSTEM=ON \
    -DBUILD_WITH_LOGGER_SYSTEM=ON \
    -DBUILD_WITH_MONITORING_SYSTEM=ON  # Optional
```

### Minimal Build (No External Dependencies)
```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=OFF \
    -DBUILD_WITH_CONTAINER_SYSTEM=OFF \
    -DBUILD_WITH_LOGGER_SYSTEM=OFF
```

### Checking Available Integrations

The system provides macros to check which integrations are available:

```cpp
#if KCENON_WITH_THREAD_SYSTEM
    // Thread system is available
    use_thread_pool();
#endif

#if KCENON_WITH_CONTAINER_SYSTEM
    // Container system is available
    use_advanced_serialization();
#endif

#if KCENON_WITH_LOGGER_SYSTEM
    // Logger system is available
    use_structured_logging();
#else
    // Fallback to basic logging
    use_console_logging();
#endif

#if KCENON_WITH_MONITORING_SYSTEM
    // Monitoring system is available
    auto adapter = std::make_shared<monitoring_system_adapter>("my_service");
    monitoring_integration_manager::instance().set_monitoring(adapter);
#else
    // Fallback to basic monitoring (logs to console)
    auto basic = std::make_shared<basic_monitoring>(true);
    monitoring_integration_manager::instance().set_monitoring(basic);
#endif
```

## Common Flags Summary

- `BUILD_WITH_THREAD_SYSTEM` — enable thread pool integration via `thread_integration_manager`.
- `BUILD_WITH_CONTAINER_SYSTEM` — enable container adapters via `container_manager`.
- `BUILD_WITH_LOGGER_SYSTEM` — use `logger_system_adapter`; otherwise falls back to `basic_logger`.
- `BUILD_WITH_MONITORING_SYSTEM` — use `monitoring_system_adapter`; otherwise falls back to `basic_monitoring` (OFF by default).

## Versioning & Dependencies

- Recommended vcpkg baseline alignment across modules (example):
  - fmt: 10.2.1
  - Provide a top-level `vcpkg-configuration.json` to set a shared `builtin-baseline` and fmt override.
- C++ standard: C++20 (string_view and coroutine-friendly APIs; falls back to non-coroutine path when disabled).
- ASIO standalone is required (see `vcpkg.json`).

## Performance Considerations

### With Thread System
- **Benefit**: Up to 40% improvement in concurrent connection handling
- **Trade-off**: Slightly increased memory usage

### With Container System
- **Benefit**: Type-safe serialization with minimal overhead
- **Trade-off**: Additional dependency and build time

### With Logger System
- **Benefit**: Structured logging with filtering reduces I/O overhead
- **Trade-off**: Minimal performance impact (~1-2%)

### With Monitoring System
- **Benefit**: Full observability with metrics collection and health monitoring
- **Trade-off**: Slightly higher overhead than basic_monitoring (~3-5%)
- **Note**: basic_monitoring (default) has minimal overhead as it only logs when enabled

## Troubleshooting

### Integration Not Found
If CMake cannot find an integration system:
1. Verify the system is installed in the expected location
2. Check that include paths are correct
3. Ensure the system is built with compatible compiler settings

### Link Errors
- Ensure all systems are built with the same C++ standard
- Check ABI compatibility between systems
- Verify all required libraries are linked

### Runtime Issues
- Check that all integrated systems are initialized properly
- Verify thread safety when using multiple integrations
- Review log output for initialization errors

---

*Last Updated: 2025-12-05*
