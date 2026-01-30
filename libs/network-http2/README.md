# network-http2

HTTP/2 protocol implementation for the network_system library.

## Overview

`network-http2` provides a complete HTTP/2 protocol implementation (RFC 7540) including:

- HTTP/2 frame encoding/decoding
- HPACK header compression (RFC 7541)
- TLS 1.3 with ALPN "h2" negotiation
- Stream multiplexing
- Flow control
- Server push (disabled by default)
- Both client and server support

## Features

- **RFC 7540 Compliant**: Full implementation of the HTTP/2 protocol specification
- **HPACK Compression**: Efficient header compression per RFC 7541
- **TLS Support**: Secure connections with ALPN negotiation
- **Multiplexing**: Multiple concurrent streams over single connection
- **Flow Control**: Per-stream and connection-level flow control
- **Both Client and Server**: Full support for both HTTP/2 client and server

## Dependencies

- **network-core**: For interfaces and result types
- **network-tcp**: For underlying TCP transport
- **OpenSSL**: For TLS support

## Usage

### As Part of network_system

When building as part of the main network_system project, the library is automatically integrated:

```cmake
# In your CMakeLists.txt
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::network-http2)
```

### Standalone Build

```cmake
# In your CMakeLists.txt
find_package(network-http2 REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::network-http2)
```

### Example Code

#### Client Usage

```cpp
#include <network_http2/http2.h>

// Create HTTP/2 client
auto client = std::make_shared<http2_client>("my-client");

// Connect to server
auto connect_result = client->connect("example.com", 443);
if (!connect_result) {
    std::cerr << "Connect failed: " << connect_result.error().message << "\n";
    return;
}

// Simple GET request
auto response = client->get("/api/users");
if (response) {
    std::cout << "Status: " << response->status_code << "\n";
    std::cout << "Body: " << response->get_body_string() << "\n";
}

// POST with JSON body
std::vector<http_header> headers = {
    {"content-type", "application/json"}
};
std::string json_body = R"({"name": "John"})";
auto post_response = client->post("/api/users", json_body, headers);

// Disconnect
client->disconnect();
```

#### Server Usage

```cpp
#include <network_http2/http2.h>

// Create HTTP/2 server
auto server = std::make_shared<http2_server>("my-server");

// Set request handler
server->set_request_handler([](http2_server_stream& stream,
                               const http2_request& request) {
    if (request.method == "GET" && request.path == "/api/health") {
        stream.send_headers(200, {{"content-type", "application/json"}});
        stream.send_data(R"({"status": "ok"})", true);
    } else {
        stream.send_headers(404, {});
        stream.send_data("Not Found", true);
    }
});

// Start with TLS
tls_config config;
config.cert_file = "/path/to/cert.pem";
config.key_file = "/path/to/key.pem";
server->start_tls(8443, config);

// Wait for shutdown signal
server->wait();

// Stop server
server->stop();
```

## Library Structure

```
network-http2/
├── include/
│   └── network_http2/
│       ├── http2.h                    # Main public header
│       ├── http2_client.h             # Client implementation
│       ├── http2_server.h             # Server implementation
│       ├── http2_server_stream.h      # Server stream handling
│       ├── http2_request.h            # Request/Response types
│       └── internal/
│           ├── frame.h                # Frame encoding/decoding
│           └── hpack.h                # HPACK compression
├── src/
│   ├── http2_client.cpp
│   ├── http2_server.cpp
│   └── http2_server_stream.cpp
├── tests/
│   └── (test files)
├── CMakeLists.txt
├── network-http2-config.cmake.in
└── README.md
```

## API Overview

### http2_client

The HTTP/2 client class:

- `connect()` - Connect to HTTP/2 server
- `disconnect()` - Disconnect from server
- `get()` - Perform GET request
- `post()` - Perform POST request
- `put()` - Perform PUT request
- `del()` - Perform DELETE request
- `start_stream()` - Start streaming request
- `write_stream()` - Write data to stream
- `close_stream_writer()` - Close stream writer

### http2_server

The HTTP/2 server class:

- `start()` - Start server without TLS (h2c)
- `start_tls()` - Start server with TLS
- `stop()` - Stop server
- `wait()` - Wait for server to stop
- `set_request_handler()` - Set request callback
- `set_error_handler()` - Set error callback

### http2_server_stream

Server-side stream for sending responses:

- `send_headers()` - Send response headers
- `send_data()` - Send response data
- `start_response()` - Start streaming response
- `write()` - Write data chunk
- `end_response()` - End response
- `reset()` - Reset stream with error

### http2_request

HTTP/2 request data structure:

- `method` - HTTP method
- `path` - Request path
- `authority` - Authority pseudo-header
- `scheme` - URI scheme
- `headers` - Regular headers
- `body` - Request body
- `get_header()` - Get header by name
- `content_type()` - Get content-type header
- `content_length()` - Get content-length header

## License

BSD 3-Clause License. See LICENSE file for details.
