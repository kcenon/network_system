# Network System Integration with Monitoring System

## Overview

This guide explains how to integrate network_system with monitoring_system for metrics collection and observability.

**Important**: monitoring_system integration is **OPTIONAL** (OFF by default) to prevent circular dependencies.

## Dependency Structure

```
Tier 4: network_system
  ├── common_system (Tier 0) [REQUIRED]
  ├── thread_system (Tier 1) [REQUIRED]
  ├── logger_system (Tier 2) [REQUIRED]
  └── monitoring_system (Tier 3) [OPTIONAL]
```

### Why Optional?

monitoring_system can optionally depend on network_system for HTTP metrics export (`MONITORING_WITH_NETWORK_SYSTEM`). Making monitoring_system a required dependency of network_system would create a circular dependency:

```
network_system → monitoring_system → network_system (circular!)
```

By keeping both integrations optional and using adapter patterns, we avoid compile-time cycles.

## Build Configuration

### Standalone Mode (Default)

```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_MONITORING_SYSTEM=OFF  # Default, can be omitted
```

In standalone mode, network_system uses `basic_monitoring` which logs metrics to console via logger_system.

### With monitoring_system Integration

```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_MONITORING_SYSTEM=ON
```

When enabled, network_system uses `monitoring_system_adapter` for full metrics collection capabilities.

## API Usage

### Monitoring Interface

network_system provides a unified `monitoring_interface` that works in both modes:

```cpp
#include <kcenon/network/integration/monitoring_integration.h>

using namespace kcenon::network::integration;

// Get the monitoring manager (singleton)
auto& monitor = monitoring_integration_manager::instance();

// Report metrics
monitor.report_counter("network.connections.total", 1, {{"type", "tcp"}});
monitor.report_gauge("network.connections.active", 42, {{"server", "main"}});
monitor.report_histogram("network.latency_ms", 12.5, {{"endpoint", "/api"}});

// Report health
monitor.report_health("connection_1",
    true,      // is_alive
    15.3,      // response_time_ms
    0,         // missed_heartbeats
    0.0);      // packet_loss_rate
```

### Custom Monitoring Implementation

You can inject a custom monitoring implementation:

```cpp
#include <kcenon/network/integration/monitoring_integration.h>

// Create custom implementation
class my_monitoring : public monitoring_interface {
public:
    void report_counter(const std::string& name, double value,
                       const std::map<std::string, std::string>& labels) override {
        // Custom implementation
    }
    // ... implement other methods
};

// Set custom monitoring
auto& manager = monitoring_integration_manager::instance();
manager.set_monitoring(std::make_shared<my_monitoring>());
```

### Using monitoring_system_adapter (When Available)

```cpp
#ifdef BUILD_WITH_MONITORING_SYSTEM
#include <kcenon/network/integration/monitoring_integration.h>

// Create adapter for monitoring_system
auto adapter = std::make_shared<monitoring_system_adapter>("my_network_service");
adapter->start();

// Set as the active monitoring implementation
monitoring_integration_manager::instance().set_monitoring(adapter);

// ... use network_system ...

adapter->stop();
#endif
```

## Metric Types

### Counters

Monotonically increasing values (e.g., total connections, bytes sent):

```cpp
monitor.report_counter("network.bytes_sent", 1024);
monitor.report_counter("network.errors.total", 1, {{"type", "timeout"}});
```

### Gauges

Values that can increase or decrease (e.g., active connections):

```cpp
monitor.report_gauge("network.connections.active", 42);
monitor.report_gauge("network.buffer.usage_percent", 75.5);
```

### Histograms

Distribution of values (e.g., latency):

```cpp
monitor.report_histogram("network.request.duration_ms", 125.3);
monitor.report_histogram("network.packet.size_bytes", 512);
```

### Health Reports

Connection health status:

```cpp
monitor.report_health("conn_123",
    true,   // is_alive
    10.5,   // response_time_ms
    0,      // missed_heartbeats
    0.01);  // packet_loss_rate (1%)
```

## Standard Network Metrics

network_system reports these metrics automatically when monitoring is enabled:

| Metric | Type | Description |
|--------|------|-------------|
| `network.connections.total` | Counter | Total connections established |
| `network.connections.active` | Gauge | Currently active connections |
| `network.connections.failed` | Counter | Failed connection attempts |
| `network.bytes.sent` | Counter | Total bytes sent |
| `network.bytes.received` | Counter | Total bytes received |
| `network.latency.ms` | Histogram | Request/response latency |
| `network.errors.total` | Counter | Total errors by type |

## Bidirectional Optional Integration

Both network_system and monitoring_system can optionally use each other:

### network_system → monitoring_system

```bash
# In network_system
cmake .. -DBUILD_WITH_MONITORING_SYSTEM=ON
```

Provides: Network metrics collection via monitoring_system

### monitoring_system → network_system

```bash
# In monitoring_system
cmake .. -DMONITORING_WITH_NETWORK_SYSTEM=ON
```

Provides: HTTP metrics export endpoint

### Both Enabled

When both systems have their optional integrations enabled:
- network_system collects metrics via monitoring_system
- monitoring_system exports metrics via network_system HTTP server
- No circular dependency due to adapter patterns

## Implementation Details

### basic_monitoring (Standalone Mode)

- Logs metrics to console via logger_system
- No external dependencies beyond logger_system
- Suitable for development and testing

### monitoring_system_adapter (Integration Mode)

- Full monitoring_system integration
- Uses `performance_monitor` for metric collection
- Uses `health_monitor` for health tracking
- Requires monitoring_system library

## Requirements

### Standalone Mode

- logger_system (for metric logging)

### Integration Mode

- monitoring_system built and available
- Headers at `../monitoring_system/include`
- Library at `../monitoring_system/lib` or build directory

## Troubleshooting

### BUILD_WITH_MONITORING_SYSTEM=ON but monitoring_system not found

```
CMake Warning: BUILD_WITH_MONITORING_SYSTEM is ON but monitoring_system not found
```

Solution:
1. Ensure monitoring_system is built first (Tier 3 before Tier 4)
2. Check include path: `../monitoring_system/include`
3. Verify library exists in expected location

### No metrics being reported

Check that monitoring is enabled and configured:

```cpp
auto& mgr = monitoring_integration_manager::instance();
auto mon = mgr.get_monitoring();

// Check if using basic_monitoring (logs to console)
if (auto* basic = dynamic_cast<basic_monitoring*>(mon.get())) {
    basic->set_logging_enabled(true);
}
```

### Performance concerns

basic_monitoring has minimal overhead as it only logs when logging is enabled. monitoring_system_adapter has slightly more overhead but provides full observability.

---

*Last Updated: 2025-11-30*
