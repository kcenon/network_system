# gRPC Integration Guide

> **Language:** **English** | [한국어](GRPC_GUIDE_KO.md)

---

## Overview

The network_system library provides a comprehensive gRPC implementation that wraps the official gRPC library (grpc++) while integrating with the network_system API. This guide covers the key components and usage patterns.

## Features

- **Service Registry**: Central management of gRPC services
- **Generic Services**: Dynamic method registration without protobuf definitions
- **Protoc Service Adapter**: Support for protoc-generated services
- **Health Checking**: Standard gRPC health checking protocol
- **Reflection Support**: Service discovery for debugging tools
- **Status Conversion**: Seamless conversion between gRPC and network_system types

## Quick Start

### Basic Service Setup

```cpp
#include <kcenon/network/protocols/grpc/service_registry.h>
#include <kcenon/network/protocols/grpc/server.h>

namespace grpc = kcenon::network::protocols::grpc;

int main() {
    // Create service registry with reflection enabled
    grpc::registry_config config;
    config.enable_reflection = true;
    config.enable_health_check = true;

    grpc::service_registry registry(config);

    // Create a generic service
    grpc::generic_service echo_service("myapp.EchoService");

    // Register a unary method
    echo_service.register_unary_method("Echo",
        [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
            // Echo back the request
            return {grpc::grpc_status::ok_status(), request};
        },
        "EchoRequest",   // Input type (optional, for reflection)
        "EchoResponse"   // Output type (optional, for reflection)
    );

    // Register the service
    registry.register_service(&echo_service);

    // Configure server with registered services
    grpc::grpc_server server;
    registry.configure_server(server);

    // Start server
    auto result = server.start(50051);
    if (result.is_err()) {
        std::cerr << "Failed to start: " << result.error().message << std::endl;
        return -1;
    }

    server.wait();
    return 0;
}
```

## Components

### Service Registry

The `service_registry` manages all registered services and provides:

- Service registration and lookup
- Method routing
- Reflection support
- Health checking integration

```cpp
// Create registry
grpc::service_registry registry({
    .enable_reflection = true,
    .enable_health_check = true
});

// Register service
registry.register_service(&my_service);

// Find service by name
auto* service = registry.find_service("myapp.MyService");

// Find method by full path
auto method = registry.find_method("/myapp.MyService/DoSomething");
if (method.has_value()) {
    auto [service, descriptor] = *method;
    // Use service and method descriptor
}

// List all service names
auto names = registry.service_names();
```

### Generic Service

Use `generic_service` for dynamic method registration without protobuf:

```cpp
grpc::generic_service service("mypackage.MyService");

// Unary method
service.register_unary_method("UnaryMethod",
    [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), process(request)};
    });

// Server streaming method
service.register_server_streaming_method("StreamData",
    [](grpc::server_context& ctx,
       const std::vector<uint8_t>& request,
       grpc::server_writer& writer) -> grpc::grpc_status {
        for (int i = 0; i < 10; ++i) {
            std::vector<uint8_t> chunk = generate_chunk(i);
            auto result = writer.write(chunk);
            if (result.is_err()) {
                return grpc::grpc_status::error_status(
                    grpc::status_code::internal, "Write failed");
            }
        }
        return grpc::grpc_status::ok_status();
    });

// Client streaming method
service.register_client_streaming_method("ReceiveData",
    [](grpc::server_context& ctx, grpc::server_reader& reader)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        std::vector<uint8_t> accumulated;
        while (reader.has_more()) {
            auto chunk = reader.read();
            if (chunk.is_ok()) {
                accumulated.insert(accumulated.end(),
                    chunk.value().begin(), chunk.value().end());
            }
        }
        return {grpc::grpc_status::ok_status(), accumulated};
    });

// Bidirectional streaming method
service.register_bidi_streaming_method("Chat",
    [](grpc::server_context& ctx, grpc::server_reader_writer& stream)
        -> grpc::grpc_status {
        while (stream.has_more()) {
            auto msg = stream.read();
            if (msg.is_ok()) {
                auto response = process(msg.value());
                stream.write(response);
            }
        }
        return grpc::grpc_status::ok_status();
    });
```

### Health Checking

The health service implements the standard gRPC health checking protocol:

```cpp
// Use with registry
grpc::service_registry registry({.enable_health_check = true});
registry.register_service(&my_service);

// Set service health
registry.set_service_health("myapp.MyService", true);   // Serving
registry.set_service_health("myapp.MyService", false);  // Not serving

// Or use standalone health service
grpc::health_service health;
health.set_status("myapp.MyService", grpc::health_status::serving);
health.set_status("myapp.OtherService", grpc::health_status::not_serving);

// Query status
auto status = health.get_status("myapp.MyService");
if (status == grpc::health_status::serving) {
    // Service is healthy
}
```

### Status Codes

The library provides standard gRPC status codes:

```cpp
// Create OK status
auto ok = grpc::grpc_status::ok_status();

// Create error status
auto err = grpc::grpc_status::error_status(
    grpc::status_code::not_found,
    "Resource not found"
);

// Status with details
grpc::grpc_status detailed(
    grpc::status_code::invalid_argument,
    "Validation failed",
    "field 'email' is required"
);

// Check status
if (status.is_ok()) {
    // Success
} else {
    std::cerr << "Error: " << status.code_string()
              << " - " << status.message << std::endl;
}
```

Available status codes:

| Code | Name | Description |
|------|------|-------------|
| 0 | OK | Not an error |
| 1 | CANCELLED | Operation cancelled |
| 2 | UNKNOWN | Unknown error |
| 3 | INVALID_ARGUMENT | Client specified invalid argument |
| 4 | DEADLINE_EXCEEDED | Deadline expired |
| 5 | NOT_FOUND | Resource not found |
| 6 | ALREADY_EXISTS | Resource already exists |
| 7 | PERMISSION_DENIED | Permission denied |
| 8 | RESOURCE_EXHAUSTED | Resource exhausted |
| 9 | FAILED_PRECONDITION | Operation rejected |
| 10 | ABORTED | Operation aborted |
| 11 | OUT_OF_RANGE | Out of range |
| 12 | UNIMPLEMENTED | Not implemented |
| 13 | INTERNAL | Internal error |
| 14 | UNAVAILABLE | Service unavailable |
| 15 | DATA_LOSS | Data loss |
| 16 | UNAUTHENTICATED | Unauthenticated |

### Message Framing

gRPC messages use a specific wire format:

```cpp
// Create a message
std::vector<uint8_t> payload = {0x08, 0x96, 0x01};  // Example protobuf
grpc::grpc_message msg(payload, false);  // Not compressed

// Serialize for wire transmission
auto frame = msg.serialize();
// Format: [1 byte compression flag][4 bytes length][payload]

// Parse received frame
auto result = grpc::grpc_message::parse(received_data);
if (result.is_ok()) {
    auto& message = result.value();
    // Process message.data
}
```

### Timeout Handling

Parse and format gRPC timeouts:

```cpp
// Parse timeout string (gRPC format)
uint64_t ms = grpc::parse_timeout("30S");   // 30 seconds = 30000ms
ms = grpc::parse_timeout("1M");             // 1 minute = 60000ms
ms = grpc::parse_timeout("500m");           // 500 milliseconds

// Format timeout for gRPC header
std::string timeout = grpc::format_timeout(30000);  // "30S"
```

## CMake Integration

Enable official gRPC support in your CMakeLists.txt:

```cmake
option(NETWORK_ENABLE_GRPC_OFFICIAL "Use official gRPC library" OFF)

if(NETWORK_ENABLE_GRPC_OFFICIAL)
    find_package(gRPC CONFIG REQUIRED)
    target_link_libraries(your_target PRIVATE
        gRPC::grpc++
        gRPC::grpc++_reflection
    )
    target_compile_definitions(your_target PRIVATE NETWORK_GRPC_OFFICIAL=1)
endif()
```

## Best Practices

### 1. Service Organization

Group related methods into services:

```cpp
// User service
grpc::generic_service user_service("myapp.UserService");
user_service.register_unary_method("GetUser", get_user_handler);
user_service.register_unary_method("CreateUser", create_user_handler);
user_service.register_unary_method("UpdateUser", update_user_handler);

// Order service
grpc::generic_service order_service("myapp.OrderService");
order_service.register_unary_method("GetOrder", get_order_handler);
order_service.register_unary_method("CreateOrder", create_order_handler);
```

### 2. Error Handling

Return appropriate status codes:

```cpp
auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
    -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {

    if (req.empty()) {
        return {grpc::grpc_status::error_status(
            grpc::status_code::invalid_argument,
            "Request cannot be empty"), {}};
    }

    auto result = process_request(req);
    if (result.is_err()) {
        return {grpc::grpc_status::error_status(
            grpc::status_code::internal,
            result.error().message), {}};
    }

    return {grpc::grpc_status::ok_status(), result.value()};
};
```

### 3. Health Checking

Implement proper health management:

```cpp
// Set up health checking
registry.set_service_health("myapp.MyService", true);

// Monitor dependencies and update health
void check_dependencies() {
    bool db_healthy = check_database();
    bool cache_healthy = check_cache();

    registry.set_service_health("myapp.MyService",
        db_healthy && cache_healthy);
}
```

### 4. Thread Safety

The service registry is thread-safe for concurrent access:

```cpp
// Safe to call from multiple threads
registry.find_service("myapp.MyService");
registry.find_method("/myapp.MyService/Method");
registry.get_service_health("myapp.MyService");
```

## Performance Considerations

- **Message serialization**: O(n) where n is payload size
- **Service lookup**: O(1) hash-based lookup
- **Method lookup**: O(m) where m is methods per service
- **Health status**: O(1) for get/set operations

Run benchmarks to measure performance:

```bash
./build/bin/network_grpc_benchmarks
```

## Troubleshooting

### Common Issues

1. **Service not found**
   - Ensure service name includes package prefix (e.g., "myapp.Service")
   - Verify service is registered before server starts

2. **Method not found**
   - Check method path format: "/package.Service/Method"
   - Verify method is registered with correct name

3. **Connection refused**
   - Verify server is running on expected port
   - Check firewall settings

### Debug with Reflection

Enable reflection for debugging:

```cpp
grpc::registry_config config;
config.enable_reflection = true;
grpc::service_registry registry(config);
```

Then use grpc_cli to explore:

```bash
grpc_cli ls localhost:50051
grpc_cli ls localhost:50051 myapp.MyService
```

## See Also

- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [Architecture](../ARCHITECTURE.md) - System design overview
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions
