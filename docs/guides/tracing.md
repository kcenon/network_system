# OpenTelemetry Tracing Guide

This guide covers the OpenTelemetry-compatible distributed tracing support in network_system.

## Overview

The tracing module provides distributed tracing capabilities following the OpenTelemetry specification. It enables visibility into request flows across network operations, helping identify performance bottlenecks and debug distributed systems.

### Features

- **W3C Trace Context** propagation for cross-service tracing
- **RAII-based spans** for automatic lifecycle management
- **Multiple exporters**: Console, OTLP HTTP, OTLP gRPC, Jaeger, Zipkin
- **Configurable sampling** strategies
- **Semantic conventions** following OpenTelemetry standards
- **Thread-safe** span creation and context propagation

## Quick Start

### Basic Usage

```cpp
#include <kcenon/network/tracing/trace_context.h>
#include <kcenon/network/tracing/span.h>
#include <kcenon/network/tracing/tracing_config.h>

using namespace kcenon::network::tracing;

int main() {
    // Configure tracing with console output
    configure_tracing(tracing_config::console());

    // Create a span for an operation
    {
        auto span = trace_context::create_span("http.request");
        span.set_attribute("http.method", "GET");
        span.set_attribute("http.url", "https://api.example.com/users");
        span.set_attribute("http.status_code", int64_t{200});

        // Span automatically ends when leaving scope
    }

    // Shutdown tracing before exit
    shutdown_tracing();
    return 0;
}
```

### Using the RAII Macro

```cpp
void process_request() {
    NETWORK_TRACE_SPAN("process_request");

    _span.set_attribute("request.id", "12345");
    _span.add_event("processing_started");

    // Do work...

    if (error_occurred) {
        _span.set_error("Failed to process request");
    } else {
        _span.set_status(span_status::ok);
    }
    // span automatically ends on function return
}
```

## Configuration

### Exporter Types

```cpp
// Console output (for debugging)
auto config = tracing_config::console();

// OTLP HTTP (OpenTelemetry Collector)
auto config = tracing_config::otlp_http("http://localhost:4318");

// OTLP gRPC
auto config = tracing_config::otlp_grpc("http://localhost:4317");

// Jaeger
auto config = tracing_config::jaeger("http://localhost:14268/api/traces");

// Disabled (no-op)
auto config = tracing_config::disabled();
```

### Service Configuration

```cpp
tracing_config config;
config.exporter = exporter_type::otlp_http;
config.otlp.endpoint = "http://otel-collector:4318";

// Service identification
config.service_name = "my-service";
config.service_version = "1.2.3";
config.service_namespace = "production";
config.service_instance_id = "instance-001";

// Custom resource attributes
config.resource_attributes["deployment.environment"] = "production";
config.resource_attributes["host.name"] = "server-01";

configure_tracing(config);
```

### Sampling Configuration

```cpp
tracing_config config;

// Always sample all traces
config.sampler = sampler_type::always_on;

// Never sample (effectively disables tracing)
config.sampler = sampler_type::always_off;

// Sample based on trace ID (probabilistic)
config.sampler = sampler_type::trace_id;
config.sample_rate = 0.1;  // 10% of traces

// Respect parent span's sampling decision
config.sampler = sampler_type::parent_based;
```

### Batch Export Configuration

```cpp
tracing_config config;
config.batch.max_queue_size = 1024;           // Max spans in queue
config.batch.max_export_batch_size = 256;     // Max spans per export
config.batch.schedule_delay = std::chrono::milliseconds{5000};  // Batch interval
config.batch.export_timeout = std::chrono::milliseconds{30000}; // Export timeout
```

## Span Operations

### Creating Spans

```cpp
// Root span (new trace)
auto span = trace_context::create_span("operation.name");

// Child span (inherits trace context)
auto child = span.context().create_child_span("child.operation");

// Span with specific kind
span span_obj("server.request", ctx, span_kind::server);
```

### Setting Attributes

```cpp
auto span = trace_context::create_span("http.request");

// String attributes
span.set_attribute("http.method", "POST");
span.set_attribute("http.url", "https://api.example.com/data");

// Integer attributes
span.set_attribute("http.status_code", int64_t{201});
span.set_attribute("http.request.content_length", int64_t{1024});

// Double attributes
span.set_attribute("http.response_time_ms", 45.7);

// Boolean attributes
span.set_attribute("http.retry", true);

// Chained attribute setting
span.set_attribute("net.peer.name", "api.example.com")
    .set_attribute("net.peer.port", int64_t{443})
    .set_attribute("net.transport", "tcp");
```

### Recording Events

```cpp
auto span = trace_context::create_span("process_order");

// Simple event
span.add_event("order_received");

// Event with attributes
std::map<std::string, attribute_value> attrs = {
    {"item_count", int64_t{3}},
    {"total_amount", 99.99}
};
span.add_event("payment_processed", attrs);

span.add_event("order_completed");
```

### Setting Status

```cpp
auto span = trace_context::create_span("database.query");

// Success status
span.set_status(span_status::ok);

// Error status with message
span.set_status(span_status::error, "Connection timeout");

// Shorthand for error
span.set_error("Query failed: table not found");
```

## Context Propagation

### HTTP Header Propagation

```cpp
// Extract headers for outgoing request
auto span = trace_context::create_span("http.client.request");
auto headers = span.context().to_headers();
// headers = [{"traceparent", "00-trace_id-span_id-01"}]

// Include in HTTP request
for (const auto& [name, value] : headers) {
    request.add_header(name, value);
}

// Parse context from incoming request
auto incoming_headers = request.headers();
std::vector<std::pair<std::string, std::string>> header_pairs;
for (const auto& h : incoming_headers) {
    header_pairs.emplace_back(h.name, h.value);
}
auto ctx = trace_context::from_headers(header_pairs);

// Create child span from parsed context
if (ctx.is_valid()) {
    auto server_span = ctx.create_child_span("http.server.request");
}
```

### W3C Trace Context Format

The traceparent header follows W3C Trace Context specification:

```
traceparent: 00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01
             │   │                                │                 │
             │   │                                │                 └─ trace-flags (01 = sampled)
             │   │                                └─ parent-id (16 hex chars)
             │   └─ trace-id (32 hex chars)
             └─ version (00)
```

## Span Kinds

```cpp
// Internal operation (default)
span span_internal("internal.process", ctx, span_kind::internal);

// Server-side request handling
NETWORK_TRACE_SERVER_SPAN("http.server.request");

// Client-side request
NETWORK_TRACE_CLIENT_SPAN("http.client.request");

// Message producer (queue publisher)
span span_producer("queue.publish", ctx, span_kind::producer);

// Message consumer (queue subscriber)
span span_consumer("queue.consume", ctx, span_kind::consumer);
```

## Custom Span Processors

```cpp
// Register a custom processor for additional export logic
register_span_processor([](const span& s) {
    // Log span completion
    std::cout << "Span completed: " << s.name()
              << " (duration: " << s.duration().count() / 1e6 << "ms)\n";

    // Custom metrics collection
    if (s.status() == span_status::error) {
        metrics::increment("span.errors", {{"name", s.name()}});
    }
});
```

## Semantic Conventions

Follow OpenTelemetry semantic conventions for consistent attribute naming:

### Network Attributes
| Attribute | Description | Example |
|-----------|-------------|---------|
| `net.peer.name` | Remote hostname | "api.example.com" |
| `net.peer.port` | Remote port | 443 |
| `net.transport` | Transport protocol | "tcp", "udp", "quic" |

### HTTP Attributes
| Attribute | Description | Example |
|-----------|-------------|---------|
| `http.method` | HTTP method | "GET", "POST" |
| `http.url` | Full URL | "https://api.example.com/users" |
| `http.status_code` | HTTP status code | 200 |
| `http.request.content_length` | Request body size | 1024 |

### RPC Attributes
| Attribute | Description | Example |
|-----------|-------------|---------|
| `rpc.system` | RPC system | "grpc" |
| `rpc.service` | RPC service name | "UserService" |
| `rpc.method` | RPC method name | "GetUser" |

## Thread Safety

The tracing module is designed for concurrent use:

- **Thread-local context**: Each thread maintains its own current trace context
- **Atomic operations**: Configuration changes are thread-safe
- **Lock-free span creation**: Spans can be created concurrently without contention

```cpp
// Safe concurrent span creation
std::vector<std::thread> threads;
for (int i = 0; i < 10; ++i) {
    threads.emplace_back([i]() {
        auto span = trace_context::create_span("worker." + std::to_string(i));
        span.set_attribute("worker.id", int64_t{i});
        // ... do work ...
    });
}
for (auto& t : threads) {
    t.join();
}
```

## Integration with Backends

### Jaeger

```yaml
# docker-compose.yml
services:
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # UI
      - "14268:14268"  # HTTP collector
      - "4317:4317"    # OTLP gRPC
      - "4318:4318"    # OTLP HTTP
```

```cpp
// Use OTLP with Jaeger (recommended)
auto config = tracing_config::otlp_http("http://localhost:4318/v1/traces");

// Or use native Jaeger format
auto config = tracing_config::jaeger("http://localhost:14268/api/traces");
```

### OpenTelemetry Collector

```yaml
# otel-collector-config.yaml
receivers:
  otlp:
    protocols:
      grpc:
        endpoint: 0.0.0.0:4317
      http:
        endpoint: 0.0.0.0:4318

exporters:
  jaeger:
    endpoint: "jaeger:14250"

service:
  pipelines:
    traces:
      receivers: [otlp]
      exporters: [jaeger]
```

```cpp
auto config = tracing_config::otlp_http("http://collector:4318/v1/traces");
configure_tracing(config);
```

## Best Practices

1. **Use meaningful span names**: Follow the pattern `component.operation` (e.g., "http.client.request", "database.query")

2. **Set attributes early**: Add attributes as soon as values are known

3. **Record significant events**: Mark important points in processing

4. **Set status appropriately**: Always set error status for failed operations

5. **Propagate context**: Always include trace context in outgoing requests

6. **Configure sampling**: Use probabilistic sampling in production to reduce overhead

7. **Clean shutdown**: Call `shutdown_tracing()` before application exit to flush pending spans

## Troubleshooting

### No spans appearing in backend

1. Check exporter configuration
2. Verify network connectivity to collector
3. Enable debug mode: `config.debug = true`
4. Check sampling configuration

### High memory usage

1. Reduce batch queue size
2. Increase export frequency
3. Lower sampling rate
4. Check for span leaks (spans not ending)

### Performance impact

1. Use `sampler_type::trace_id` with low sample rate
2. Use `sampler_type::always_off` to completely disable
3. Minimize attribute count on hot paths
