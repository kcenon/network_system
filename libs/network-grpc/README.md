# network-grpc

gRPC protocol implementation for network_system.

## Overview

The `network-grpc` library provides a complete gRPC implementation including:
- gRPC client for making unary and streaming RPC calls
- gRPC server for handling RPC requests
- Service registry for managing gRPC services
- Support for both custom implementation and official gRPC library wrapper
- Health checking and reflection support

## Features

- **Unary RPC**: Single request, single response
- **Server Streaming**: Single request, multiple responses
- **Client Streaming**: Multiple requests, single response
- **Bidirectional Streaming**: Multiple requests and responses
- **TLS Support**: Secure communication with mutual TLS
- **Service Registry**: Dynamic service registration and discovery
- **Health Checking**: Standard gRPC health checking protocol

## Dependencies

- `network-core`: Core interfaces and result types
- `network-http2`: HTTP/2 transport layer
- OpenSSL (optional): TLS support
- Official gRPC library (optional): For wrapper mode

## Usage

### Basic Client Example

```cpp
#include <network_grpc/grpc.h>

using namespace kcenon::network::protocols::grpc;

// Create a gRPC client
grpc_client client("localhost:50051");
if (auto result = client.connect(); !result.is_ok()) {
    // Handle connection error
    return;
}

// Make a unary RPC call
std::vector<uint8_t> request_data = /* serialized request */;
auto result = client.call_raw("/mypackage.MyService/MyMethod", request_data);

if (result.is_ok()) {
    const auto& response = result.value();
    // Process response.data
}
```

### Basic Server Example

```cpp
#include <network_grpc/grpc.h>

using namespace kcenon::network::protocols::grpc;

// Create a gRPC server
grpc_server server;

// Register a unary method handler
server.register_unary_method("/mypackage.MyService/MyMethod",
    [](server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc_status, std::vector<uint8_t>> {
        // Process request and return response
        std::vector<uint8_t> response = /* serialized response */;
        return {grpc_status::ok_status(), response};
    });

// Start the server
server.start(50051);
server.wait();
```

### Using Service Registry

```cpp
#include <network_grpc/grpc.h>

using namespace kcenon::network::protocols::grpc;

// Create a service registry with reflection enabled
service_registry registry({.enable_reflection = true});

// Create and register a generic service
generic_service echo_service("echo.EchoService");
echo_service.register_unary_method("Echo",
    [](server_context& ctx, const std::vector<uint8_t>& request) {
        return std::make_pair(grpc_status::ok_status(), request);
    });
registry.register_service(&echo_service);

// Configure server with registry
grpc_server server;
registry.configure_server(server);
server.start(50051);
```

## Building

### As Part of network_system

The library is automatically built when building the main network_system project:

```bash
cmake -B build
cmake --build build
```

### Standalone Build

```bash
cd libs/network-grpc
cmake -B build
cmake --build build
```

### With Official gRPC Library

To use the official gRPC library wrapper:

```bash
cmake -B build -DNETWORK_GRPC_USE_OFFICIAL=ON
cmake --build build
```

## API Reference

### Classes

| Class | Description |
|-------|-------------|
| `grpc_client` | Client for making gRPC calls |
| `grpc_server` | Server for handling gRPC requests |
| `service_registry` | Central registry for managing services |
| `generic_service` | Dynamic service registration |
| `health_service` | Health checking implementation |

### Status Codes

| Code | Description |
|------|-------------|
| `ok` | Success |
| `cancelled` | Operation cancelled |
| `unknown` | Unknown error |
| `invalid_argument` | Invalid argument |
| `deadline_exceeded` | Deadline exceeded |
| `not_found` | Resource not found |
| `permission_denied` | Permission denied |
| `unavailable` | Service unavailable |

## License

BSD 3-Clause License. See LICENSE file for details.
