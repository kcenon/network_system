# NetworkSystemBridge Migration Guide

## Overview

This guide explains how to migrate from individual integration adapters to the unified `NetworkSystemBridge` facade.

## What Changed

### Issue #579: Consolidate Integration Adapters

The network_system previously had multiple integration adapter classes:
- `thread_system_pool_adapter` - Direct thread_system integration
- `common_thread_pool_adapter` - common_system thread pool adapter
- `common_logger_adapter` - common_system logger adapter
- `common_monitoring_adapter` - common_system monitoring adapter

These have been consolidated into a unified facade:
- `NetworkSystemBridge` - Single entry point for all integrations
- `ThreadPoolBridge` - Unified thread pool bridge implementing `INetworkBridge`

## Migration Path

### Before: Using Individual Adapters

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>
#include <kcenon/network/integration/common_system_adapter.h>

// Create individual adapters
auto thread_pool = thread_system_pool_adapter::create_default("network_pool");
auto logger = std::make_shared<common_logger_adapter>(common_logger);
auto monitoring = std::make_shared<common_monitoring_adapter>(common_monitor);

// Use adapters separately
thread_pool->submit([]{
    // task implementation
});
```

### After: Using NetworkSystemBridge

```cpp
#include <kcenon/network/integration/network_system_bridge.h>

// Option 1: Create with default configuration
auto bridge = NetworkSystemBridge::create_default();

// Option 2: Create with thread_system
auto bridge = NetworkSystemBridge::with_thread_system("network_pool");

// Option 3: Create with common_system
auto executor = container.resolve<IExecutor>();
auto logger = container.resolve<ILogger>();
auto bridge = NetworkSystemBridge::with_common_system(executor, logger);

// Initialize
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
bridge->initialize(config);

// Use unified interface
if (auto pool = bridge->get_thread_pool()) {
    pool->submit([]{
        // task implementation
    });
}
```

## Detailed Migration Examples

### 1. Thread Pool Integration

#### Old Code

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

// Create and bind thread pool adapter
auto adapter = thread_system_pool_adapter::create_default("network_pool");

// Submit tasks
adapter->submit([]{
    // task
});
```

#### New Code

```cpp
#include <kcenon/network/integration/network_system_bridge.h>

// Create bridge with thread pool
auto bridge = NetworkSystemBridge::with_thread_system("network_pool");

// Initialize
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
bridge->initialize(config);

// Submit tasks
if (auto pool = bridge->get_thread_pool()) {
    pool->submit([]{
        // task
    });
}
```

### 2. Common System Integration

#### Old Code

```cpp
#include <kcenon/network/integration/common_system_adapter.h>

// Create separate adapters
auto thread_adapter = std::make_shared<common_thread_pool_adapter>(executor);
auto logger_adapter = std::make_shared<common_logger_adapter>(logger);
auto monitoring_adapter = std::make_shared<common_monitoring_adapter>(monitor);

// Use adapters
thread_adapter->submit([]{});
logger_adapter->log(log_level::info, "message");
monitoring_adapter->report_gauge("metric", 42.0);
```

#### New Code

```cpp
#include <kcenon/network/integration/network_system_bridge.h>

// Create bridge with all common_system components
auto bridge = NetworkSystemBridge::with_common_system(
    executor,
    logger,
    monitor
);

// Initialize with configuration
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
config.enable_logger = true;
config.enable_monitoring = true;
bridge->initialize(config);

// Use unified interface
if (auto pool = bridge->get_thread_pool()) {
    pool->submit([]{});
}

if (auto log = bridge->get_logger()) {
    log->log(log_level::info, "message");
}

if (auto mon = bridge->get_monitoring()) {
    mon->report_gauge("metric", 42.0);
}
```

### 3. Custom Thread Pool

#### Old Code

```cpp
#include <kcenon/network/integration/thread_integration.h>

// Create custom thread pool
class my_thread_pool : public thread_pool_interface {
    // Implementation...
};

auto pool = std::make_shared<my_thread_pool>();

// Use directly
pool->submit([]{});
```

#### New Code

```cpp
#include <kcenon/network/integration/network_system_bridge.h>

// Create custom thread pool
class my_thread_pool : public thread_pool_interface {
    // Implementation...
};

auto pool = std::make_shared<my_thread_pool>();

// Create bridge with custom pool
auto bridge = NetworkSystemBridge::with_custom(pool);

// Initialize
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
bridge->initialize(config);

// Access via bridge
if (auto thread_pool = bridge->get_thread_pool()) {
    thread_pool->submit([]{});
}
```

## Configuration

### NetworkSystemBridgeConfig Properties

```cpp
NetworkSystemBridgeConfig config;

// Global settings
config.integration_name = "my_integration";

// Enable/disable integrations
config.enable_thread_pool = true;
config.enable_logger = false;
config.enable_monitoring = false;

// Thread pool configuration
config.thread_pool_properties["pool_name"] = "network_pool";
config.thread_pool_properties["worker_count"] = "8";

// Logger configuration
config.logger_properties["log_level"] = "info";
config.logger_properties["output_file"] = "/var/log/network.log";

// Monitoring configuration
config.monitoring_properties["enable_metrics"] = "true";
config.monitoring_properties["metrics_interval_ms"] = "1000";
```

## Benefits of NetworkSystemBridge

1. **Unified Interface**: Single entry point for all integrations
2. **Simplified Configuration**: Configure all integrations at once
3. **Lifecycle Management**: Coordinated initialization and shutdown
4. **Metrics Aggregation**: Combined metrics from all bridges
5. **Factory Methods**: Easy creation for common scenarios
6. **Type Safety**: Consistent error handling via `Result<T>`

## Backward Compatibility

### Adapter Classes

The following adapter classes are **still available** but should be migrated:

- ✅ `thread_system_pool_adapter` - Still functional, use `ThreadPoolBridge` instead
- ✅ `common_thread_pool_adapter` - Still functional, use `NetworkSystemBridge::with_common_system()`
- ✅ `common_logger_adapter` - Still functional, use `NetworkSystemBridge::with_common_system()`
- ✅ `common_monitoring_adapter` - Still functional, use `NetworkSystemBridge::with_common_system()`

### Deprecation Timeline

- **v0.1.0** (Current): NetworkSystemBridge introduced, old adapters available
- **v0.2.0** (Future): Old adapters marked as deprecated
- **v1.0.0** (Future): Old adapters removed

## Complete Migration Example

### Before: Legacy Code

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>
#include <kcenon/network/integration/common_system_adapter.h>

class MyNetworkService {
public:
    void initialize() {
        // Create thread pool adapter
        thread_pool_ = thread_system_pool_adapter::create_default("network_pool");

        // Create logger adapter
        auto common_logger = container_.resolve<ILogger>();
        logger_ = std::make_shared<common_logger_adapter>(common_logger);

        // Create monitoring adapter
        auto common_monitor = container_.resolve<IMonitor>();
        monitoring_ = std::make_shared<common_monitoring_adapter>(common_monitor);
    }

    void process_request(const Request& request) {
        thread_pool_->submit([this, request]{
            logger_->log(log_level::info, "Processing request");
            // Process request
            monitoring_->report_counter("requests_processed", 1.0);
        });
    }

private:
    std::shared_ptr<thread_pool_interface> thread_pool_;
    std::shared_ptr<logger_interface> logger_;
    std::shared_ptr<monitoring_interface> monitoring_;
    ServiceContainer container_;
};
```

### After: Using NetworkSystemBridge

```cpp
#include <kcenon/network/integration/network_system_bridge.h>

class MyNetworkService {
public:
    void initialize() {
        // Create bridge with common_system integration
        auto executor = container_.resolve<IExecutor>();
        auto logger = container_.resolve<ILogger>();
        auto monitor = container_.resolve<IMonitor>();

        bridge_ = NetworkSystemBridge::with_common_system(
            executor,
            logger,
            monitor
        );

        // Initialize with configuration
        NetworkSystemBridgeConfig config;
        config.integration_name = "my_service";
        config.enable_thread_pool = true;
        config.enable_logger = true;
        config.enable_monitoring = true;

        auto result = bridge_->initialize(config);
        if (result.is_err()) {
            throw std::runtime_error("Failed to initialize bridge: " + result.error().message);
        }
    }

    void process_request(const Request& request) {
        if (auto pool = bridge_->get_thread_pool()) {
            pool->submit([this, request]{
                if (auto logger = bridge_->get_logger()) {
                    logger->log(log_level::info, "Processing request");
                }
                // Process request
                if (auto monitoring = bridge_->get_monitoring()) {
                    monitoring->report_counter("requests_processed", 1.0);
                }
            });
        }
    }

    void shutdown() {
        if (bridge_) {
            bridge_->shutdown();
        }
    }

private:
    std::shared_ptr<NetworkSystemBridge> bridge_;
    ServiceContainer container_;
};
```

## Troubleshooting

### Bridge Initialization Fails

**Problem**: `initialize()` returns error

**Solution**: Check that:
1. Thread pool bridge is set before initialization
2. Thread pool is running before bridge initialization
3. Configuration properties are valid

```cpp
auto bridge = NetworkSystemBridge::with_thread_system("network_pool");

NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;

auto result = bridge->initialize(config);
if (result.is_err()) {
    std::cerr << "Error: " << result.error().message << std::endl;
    std::cerr << "Source: " << result.error().source << std::endl;
}
```

### Null Pointer When Getting Components

**Problem**: `get_thread_pool()` returns `nullptr`

**Solution**: Ensure:
1. Bridge is initialized
2. Thread pool integration is enabled in configuration
3. Thread pool bridge was set before initialization

```cpp
if (!bridge->is_initialized()) {
    std::cerr << "Bridge not initialized" << std::endl;
}

if (auto pool = bridge->get_thread_pool()) {
    // Use pool
} else {
    std::cerr << "Thread pool not available" << std::endl;
}
```

### Cannot Set Bridge After Initialization

**Problem**: `set_thread_pool_bridge()` returns error after `initialize()`

**Solution**: Set all bridges before calling `initialize()`:

```cpp
auto bridge = std::make_shared<NetworkSystemBridge>();

// Set bridges BEFORE initialize
auto thread_pool_bridge = ThreadPoolBridge::from_thread_system("network_pool");
bridge->set_thread_pool_bridge(thread_pool_bridge);

// Now initialize
bridge->initialize(config);
```

## Related Documentation

- [INetworkBridge Interface](../api/bridge_interface.md) - Base interface for all bridges
- [ThreadPoolBridge](../api/thread_pool_bridge.md) - Thread pool bridge details
- [Integration Architecture](../architecture/integration.md) - Overall integration design

## Support

For questions or issues:
- File an issue on GitHub: https://github.com/kcenon/network_system/issues
- Reference issue #579 for NetworkSystemBridge consolidation
