# Network System Integration with Monitoring System

## Overview

This guide explains how network_system publishes metrics that can be consumed by monitoring_system or other metric collectors.

**Since issue #342**: network_system uses **EventBus-based metric publishing**. No compile-time dependency on monitoring_system is required.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    network_system                            │
│  ┌───────────────┐     ┌───────────────────────────────┐   │
│  │ metric_reporter├────►│ common_system::EventBus       │   │
│  └───────────────┘     │   - network_metric_event      │   │
│                        │   - network_connection_event  │   │
│  ┌────────────────────►│   - network_latency_event     │   │
│  │ basic_monitoring   │   - network_health_event      │   │
│  └────────────────────┘└─────────────┬─────────────────┘   │
└───────────────────────────────────────┼─────────────────────┘
                                        │
                 ┌──────────────────────▼────────────────────┐
                 │            External Consumers              │
                 │  - monitoring_system (subscribes)          │
                 │  - prometheus_exporter                     │
                 │  - custom metric collectors                │
                 └────────────────────────────────────────────┘
```

## Dependency Structure

```
Tier 4: network_system
  ├── common_system (Tier 0) [REQUIRED - provides EventBus]
  ├── thread_system (Tier 1) [REQUIRED]
  ├── logger_system (Tier 2) [OPTIONAL - runtime binding]
  └── monitoring_system (Tier 3) [NO COMPILE-TIME DEPENDENCY]
```

### Why EventBus?

The previous approach (`KCENON_WITH_MONITORING_SYSTEM`) created optional bidirectional dependencies that complicated build order and isolated testing. The EventBus pattern:

- Eliminates compile-time coupling
- Enables runtime metric subscription
- Simplifies build configuration
- Allows any consumer to receive metrics

## Subscribing to Metrics (for External Consumers)

### Basic Subscription

```cpp
#include <kcenon/common/patterns/event_bus.h>
#include <kcenon/network/events/network_metric_event.h>

// Subscribe to network metrics
auto& bus = kcenon::common::get_event_bus();

auto subscription_id = bus.subscribe<kcenon::network::events::network_metric_event>(
    [](const auto& event) {
        std::cout << "Metric: " << event.name
                  << " = " << event.value << std::endl;
    }
);

// Later: unsubscribe
bus.unsubscribe(subscription_id);
```

### Filtered Subscription

```cpp
// Subscribe only to connection metrics
auto subscription_id = bus.subscribe_filtered<network_metric_event>(
    [](const auto& event) {
        // Process connection metric
    },
    [](const auto& event) {
        return event.name.find("connections") != std::string::npos;
    }
);
```

### Specialized Event Types

```cpp
#include <kcenon/network/events/network_metric_event.h>

using namespace kcenon::network::events;

// Subscribe to connection events
bus.subscribe<network_connection_event>(
    [](const auto& event) {
        std::cout << "Connection " << event.connection_id
                  << " " << event.event_type << std::endl;
    }
);

// Subscribe to latency events
bus.subscribe<network_latency_event>(
    [](const auto& event) {
        if (event.latency_ms > 100.0) {
            std::cerr << "High latency: " << event.latency_ms << "ms" << std::endl;
        }
    }
);

// Subscribe to health events
bus.subscribe<network_health_event>(
    [](const auto& event) {
        if (!event.is_alive) {
            std::cerr << "Connection unhealthy: " << event.connection_id << std::endl;
        }
    }
);
```

## Event Types

### network_metric_event

Generic metric event with name, value, type, and labels:

```cpp
struct network_metric_event {
    std::string name;                              // Metric name
    double value;                                  // Metric value
    std::string unit;                              // Unit of measurement
    network_metric_type type;                      // counter, gauge, histogram, summary
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> labels;
};
```

### network_connection_event

Connection lifecycle events:

```cpp
struct network_connection_event {
    std::string connection_id;
    std::string event_type;     // "accepted", "closed", "failed"
    std::string protocol;        // "tcp", "udp", "websocket", "quic"
    std::string remote_address;
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> labels;
};
```

### network_transfer_event

Data transfer metrics:

```cpp
struct network_transfer_event {
    std::string connection_id;
    std::string direction;       // "sent", "received"
    std::size_t bytes;
    std::size_t packets;
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> labels;
};
```

### network_latency_event

Latency measurements:

```cpp
struct network_latency_event {
    std::string connection_id;
    double latency_ms;
    std::string operation;       // "request", "response", "roundtrip"
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> labels;
};
```

### network_health_event

Connection health status:

```cpp
struct network_health_event {
    std::string connection_id;
    bool is_alive;
    double response_time_ms;
    std::size_t missed_heartbeats;
    double packet_loss_rate;     // 0.0-1.0
    std::chrono::steady_clock::time_point timestamp;
    std::map<std::string, std::string> labels;
};
```

## Standard Network Metrics

network_system publishes these metrics automatically:

| Metric | Type | Description |
|--------|------|-------------|
| `network.connections.total` | Counter | Total connections established |
| `network.connections.active` | Gauge | Currently active connections |
| `network.connections.failed` | Counter | Failed connection attempts |
| `network.bytes.sent` | Counter | Total bytes sent |
| `network.bytes.received` | Counter | Total bytes received |
| `network.packets.sent` | Counter | Total packets sent |
| `network.packets.received` | Counter | Total packets received |
| `network.latency.ms` | Histogram | Request/response latency |
| `network.errors.total` | Counter | Total errors by type |
| `network.timeouts.total` | Counter | Total timeouts |
| `network.session.duration.ms` | Histogram | Session durations |

## Local Monitoring Interface

For direct access (without EventBus), use the monitoring integration manager:

```cpp
#include <kcenon/network/integration/monitoring_integration.h>

using namespace kcenon::network::integration;

// Get the monitoring manager (singleton)
auto& monitor = monitoring_integration_manager::instance();

// Report metrics directly
monitor.report_counter("custom.metric", 1, {{"type", "custom"}});
monitor.report_gauge("custom.gauge", 42.0);
monitor.report_histogram("custom.histogram", 12.5);

// Report health
monitor.report_health("custom_connection",
    true,      // is_alive
    15.3,      // response_time_ms
    0,         // missed_heartbeats
    0.0);      // packet_loss_rate
```

### Custom Monitoring Implementation

```cpp
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

## Migration from KCENON_WITH_MONITORING_SYSTEM

### Before (Compile-time Dependency)

```cpp
#if KCENON_WITH_MONITORING_SYSTEM
auto collector = monitoring_system::get_collector();
http_client client(collector);
#else
http_client client;
#endif
```

### After (EventBus Pattern)

```cpp
// In monitoring_system (the consumer):
auto& bus = common::get_event_bus();
bus.subscribe<network::events::network_metric_event>(
    [this](const auto& event) {
        record_metric(event.name, event.value, event.labels);
    }
);

// In your application:
http_client client;  // No conditional compilation needed
```

## Build Configuration

No special build flags required for metric publishing. Metrics are always published via EventBus.

```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
# That's it! No BUILD_WITH_MONITORING_SYSTEM needed
```

**Note**: `BUILD_WITH_MONITORING_SYSTEM` is deprecated and has no effect.

## Requirements

- common_system (provides EventBus)
- thread_system
- No compile-time monitoring_system dependency

---

*Last Updated: 2025-01-26*
