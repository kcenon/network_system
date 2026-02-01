# Migration Guide: From Adapters to NetworkSystemBridge

## Overview

This guide helps you migrate from the deprecated adapter classes to the new `NetworkSystemBridge` facade pattern. The new bridge pattern provides a unified, simplified interface for all external system integrations.

**Deprecation Timeline:**
- **v2.1.0** (current): Adapters marked as deprecated, deprecation warnings added
- **v2.2.0**: Migration guide published, migration encouraged
- **v3.0.0**: Deprecated adapters will be **removed**

## What's Changing

### Deprecated Classes

The following classes are deprecated and will be removed in v3.0.0:

| Deprecated Class | Replacement | Header |
|-----------------|------------|---------|
| `thread_system_pool_adapter` | `ThreadPoolBridge` | `network_system_bridge.h` |
| `common_thread_pool_adapter` | `ThreadPoolBridge` | `network_system_bridge.h` |
| `common_logger_adapter` | `ObservabilityBridge` | `network_system_bridge.h` |
| `common_monitoring_adapter` | `ObservabilityBridge` | `network_system_bridge.h` |
| `bind_thread_system_pool_into_manager()` | `NetworkSystemBridge::with_thread_system()` | `network_system_bridge.h` |

### New Architecture

The new `NetworkSystemBridge` facade consolidates all integration adapters into a single, unified interface:

```
NetworkSystemBridge (Facade)
├── ThreadPoolBridge (Thread pool integration)
├── ObservabilityBridge (Logger + Monitor integration)
└── MessagingBridge (Future: Message queue integration)
```

## Migration Steps

### Step 1: Update Includes

**Before:**
```cpp
// Old adapter headers
#include <kcenon/network/integration/thread_system_adapter.h>
#include <kcenon/network/integration/common_system_adapter.h>
```

**After:**
```cpp
// New unified bridge header
#include <kcenon/network/integration/network_system_bridge.h>
```

### Step 2: Migrate Thread Pool Integration

#### Scenario 1: Using `thread_system_pool_adapter`

**Before:**
```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

// Create adapter from thread_system pool
auto pool = std::make_shared<kcenon::thread::thread_pool>("network_pool", 4);
auto adapter = std::make_shared<thread_system_pool_adapter>(pool);

// Use adapter
adapter->submit([] {
    // Task implementation
});
```

**After:**
```cpp
#include <kcenon/network/integration/network_system_bridge.h>

// Create bridge with thread_system integration
auto bridge = NetworkSystemBridge::with_thread_system("network_pool");

// Configure and initialize
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
config.thread_pool_properties["pool_name"] = "network_pool";
bridge->initialize(config);

// Get thread pool interface
auto thread_pool = bridge->get_thread_pool();
if (thread_pool) {
    thread_pool->submit([] {
        // Task implementation
    });
}

// Cleanup
bridge->shutdown();
```

#### Scenario 2: Using `bind_thread_system_pool_into_manager()`

**Before:**
```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

// Bind to global integration manager
bool success = bind_thread_system_pool_into_manager("network_pool");
if (!success) {
    // Handle error
}
```

**After:**
```cpp
#include <kcenon/network/integration/network_system_bridge.h>

// Create and initialize bridge
auto bridge = NetworkSystemBridge::with_thread_system("network_pool");

NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
bridge->initialize(config);

// Store bridge globally if needed
// (e.g., in your application's service container)
```

### Step 3: Migrate Logger Integration

#### Using `common_logger_adapter`

**Before:**
```cpp
#include <kcenon/network/integration/common_system_adapter.h>
#include <kcenon/common/interfaces/logger_interface.h>

// Create logger adapter
auto common_logger = container.resolve<ILogger>();
auto logger_adapter = std::make_shared<common_logger_adapter>(common_logger);

// Use adapter
logger_adapter->log(log_level::info, "Application started");
```

**After:**
```cpp
#include <kcenon/network/integration/network_system_bridge.h>
#include <kcenon/common/interfaces/logger_interface.h>

// Create bridge with common_system integration
auto common_logger = container.resolve<ILogger>();
auto bridge = NetworkSystemBridge::with_common_system(
    executor,  // IExecutor from common_system
    common_logger
);

// Configure and initialize
NetworkSystemBridgeConfig config;
config.enable_logger = true;
bridge->initialize(config);

// Get logger interface
auto logger = bridge->get_logger();
if (logger) {
    logger->log(log_level::info, "Application started");
}
```

### Step 4: Migrate Monitoring Integration

#### Using `common_monitoring_adapter`

**Before:**
```cpp
#include <kcenon/network/integration/common_system_adapter.h>
#include <kcenon/common/interfaces/monitoring_interface.h>

// Create monitoring adapter
auto common_monitor = container.resolve<IMonitor>();
auto monitoring_adapter = std::make_shared<common_monitoring_adapter>(common_monitor);

// Use adapter
monitoring_adapter->report_counter("requests_total", 1.0);
```

**After:**
```cpp
#include <kcenon/network/integration/network_system_bridge.h>
#include <kcenon/common/interfaces/monitoring_interface.h>

// Create bridge with common_system integration
auto common_monitor = container.resolve<IMonitor>();
auto bridge = NetworkSystemBridge::with_common_system(
    executor,  // IExecutor from common_system
    nullptr,   // No logger
    common_monitor
);

// Configure and initialize
NetworkSystemBridgeConfig config;
config.enable_monitoring = true;
bridge->initialize(config);

// Get monitoring interface
auto monitoring = bridge->get_monitoring();
if (monitoring) {
    monitoring->report_counter("requests_total", 1.0);
}
```

### Step 5: Combined Integration

If you were using multiple adapters together, consolidate them into a single bridge:

**Before:**
```cpp
// Multiple adapters
auto thread_adapter = thread_system_pool_adapter::create_default();
auto logger_adapter = std::make_shared<common_logger_adapter>(common_logger);
auto monitor_adapter = std::make_shared<common_monitoring_adapter>(common_monitor);

// Use individually
thread_adapter->submit(task);
logger_adapter->log(log_level::info, "Message");
monitor_adapter->report_counter("metric", 1.0);
```

**After:**
```cpp
// Single bridge for all integrations
auto bridge = NetworkSystemBridge::with_common_system(
    executor,
    common_logger,
    common_monitor
);

// Configure all integrations
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
config.enable_logger = true;
config.enable_monitoring = true;
bridge->initialize(config);

// Access via unified interface
if (auto pool = bridge->get_thread_pool()) {
    pool->submit(task);
}
if (auto logger = bridge->get_logger()) {
    logger->log(log_level::info, "Message");
}
if (auto monitor = bridge->get_monitoring()) {
    monitor->report_counter("metric", 1.0);
}

// Single shutdown call
bridge->shutdown();
```

## API Comparison

### Thread Pool

| Old API (Adapter) | New API (Bridge) |
|-------------------|------------------|
| `auto adapter = std::make_shared<thread_system_pool_adapter>(pool)` | `auto bridge = NetworkSystemBridge::with_thread_system()` |
| `adapter->submit(task)` | `bridge->get_thread_pool()->submit(task)` |
| `adapter->submit_delayed(task, delay)` | `bridge->get_thread_pool()->submit_delayed(task, delay)` |
| `adapter->worker_count()` | `bridge->get_thread_pool()->worker_count()` |
| `bind_thread_system_pool_into_manager()` | `NetworkSystemBridge::with_thread_system()->initialize()` |

### Logger

| Old API (Adapter) | New API (Bridge) |
|-------------------|------------------|
| `auto adapter = std::make_shared<common_logger_adapter>(logger)` | `auto bridge = NetworkSystemBridge::with_common_system(executor, logger)` |
| `adapter->log(level, message)` | `bridge->get_logger()->log(level, message)` |
| `adapter->flush()` | `bridge->get_logger()->flush()` |

### Monitoring

| Old API (Adapter) | New API (Bridge) |
|-------------------|------------------|
| `auto adapter = std::make_shared<common_monitoring_adapter>(monitor)` | `auto bridge = NetworkSystemBridge::with_common_system(executor, nullptr, monitor)` |
| `adapter->report_counter(name, value)` | `bridge->get_monitoring()->report_counter(name, value)` |
| `adapter->report_gauge(name, value)` | `bridge->get_monitoring()->report_gauge(name, value)` |

## Benefits of the New Bridge Pattern

1. **Unified Interface**: Single entry point for all integrations
2. **Simplified Configuration**: One configuration object for all bridges
3. **Lifecycle Management**: Centralized initialize/shutdown
4. **Thread Safety**: All methods are thread-safe by design
5. **Metrics Aggregation**: Combined metrics from all bridges
6. **Factory Methods**: Convenient factory methods for common scenarios
7. **Better Error Handling**: Consistent Result<T> pattern across all operations

## Common Migration Patterns

### Pattern 1: Application Startup

**Before:**
```cpp
void initialize_network_integration() {
    bind_thread_system_pool_into_manager("network_pool");
    // Logger and monitoring set up separately
}
```

**After:**
```cpp
std::shared_ptr<NetworkSystemBridge> initialize_network_integration() {
    auto bridge = NetworkSystemBridge::with_thread_system("network_pool");

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;

    auto result = bridge->initialize(config);
    if (result.is_err()) {
        throw std::runtime_error("Failed to initialize bridge: " +
                                result.error().message);
    }

    return bridge;
}
```

### Pattern 2: Dependency Injection Container

**Before:**
```cpp
void register_adapters(Container& container) {
    container.register_singleton<thread_pool_interface>(
        []() { return thread_system_pool_adapter::create_default(); }
    );

    container.register_singleton<logger_interface>(
        [](ILogger* logger) {
            return std::make_shared<common_logger_adapter>(
                std::shared_ptr<ILogger>(logger)
            );
        }
    );
}
```

**After:**
```cpp
void register_bridge(Container& container) {
    // Register bridge as singleton
    container.register_singleton<NetworkSystemBridge>(
        []() {
            auto bridge = NetworkSystemBridge::create_default();

            NetworkSystemBridgeConfig config;
            config.enable_thread_pool = true;
            config.enable_logger = true;
            bridge->initialize(config);

            return bridge;
        }
    );

    // Optionally register individual interfaces
    container.register_singleton<thread_pool_interface>(
        [](NetworkSystemBridge* bridge) {
            return bridge->get_thread_pool();
        }
    );

    container.register_singleton<logger_interface>(
        [](NetworkSystemBridge* bridge) {
            return bridge->get_logger();
        }
    );
}
```

### Pattern 3: Testing

**Before:**
```cpp
TEST(NetworkTest, ThreadPoolAdapter) {
    auto pool = std::make_shared<kcenon::thread::thread_pool>("test_pool", 2);
    auto adapter = std::make_shared<thread_system_pool_adapter>(pool);

    // Test with adapter
    adapter->submit([] { /* test */ });
}
```

**After:**
```cpp
TEST(NetworkTest, ThreadPoolBridge) {
    auto bridge = NetworkSystemBridge::with_thread_system("test_pool");

    NetworkSystemBridgeConfig config;
    config.enable_thread_pool = true;
    config.thread_pool_properties["worker_count"] = "2";
    bridge->initialize(config);

    auto thread_pool = bridge->get_thread_pool();
    ASSERT_NE(thread_pool, nullptr);

    // Test with bridge
    thread_pool->submit([] { /* test */ });

    bridge->shutdown();
}
```

## Troubleshooting

### Issue: Deprecation warnings during compilation

**Symptom:**
```
warning: 'thread_system_pool_adapter' is deprecated: Use ThreadPoolBridge from network_system_bridge.h instead
```

**Solution:**
Follow the migration steps above to update to the new bridge pattern. Deprecation warnings are intentional to guide migration.

### Issue: Missing metrics after migration

**Symptom:**
Metrics that were previously collected are no longer appearing.

**Solution:**
Ensure you've enabled monitoring in the bridge configuration:
```cpp
NetworkSystemBridgeConfig config;
config.enable_monitoring = true;
bridge->initialize(config);
```

### Issue: Thread pool not initialized

**Symptom:**
`get_thread_pool()` returns `nullptr`.

**Solution:**
Make sure you've called `initialize()` and enabled thread pool:
```cpp
NetworkSystemBridgeConfig config;
config.enable_thread_pool = true;
bridge->initialize(config);
```

## Additional Resources

- [NetworkSystemBridge API Documentation](../api/network_system_bridge.md)
- [Bridge Pattern Overview](../architecture/bridge_pattern.md)
- [Issue #579: Consolidate integration adapters](https://github.com/kcenon/network_system/issues/579)
- [Issue #577: EPIC for Facade pattern refactoring](https://github.com/kcenon/network_system/issues/577)

## Support

If you encounter issues during migration:

1. Check the troubleshooting section above
2. Review the API comparison table
3. Consult the example code in `examples/bridge/`
4. Open an issue on GitHub with the `migration` label

## Timeline Summary

| Version | Date | Status | Action Required |
|---------|------|--------|-----------------|
| v2.1.0 | 2026-02-01 | Current | Deprecation warnings added |
| v2.2.0 | Q2 2026 | Planned | Migration strongly encouraged |
| v3.0.0 | Q3 2026 | Planned | **Deprecated adapters removed** |

**Recommendation:** Migrate as soon as possible to avoid breaking changes in v3.0.0.
