# network-websocket

WebSocket protocol implementation for the network_system library.

## Overview

`network-websocket` provides a complete WebSocket protocol implementation (RFC 6455) including:

- WebSocket frame encoding/decoding
- HTTP/1.1 upgrade handshake
- Message fragmentation and reassembly
- Ping/pong keepalive
- Close handshake with status codes
- WebSocket socket wrapper on top of TCP

## Features

- **RFC 6455 Compliant**: Full implementation of the WebSocket protocol specification
- **Zero-Copy Operations**: Uses move semantics and span views for efficient data handling
- **Async I/O**: Built on top of network-tcp for asynchronous operations
- **Both Client and Server**: Supports both WebSocket client and server handshake

## Dependencies

- **network-core**: For interfaces and result types
- **network-tcp**: For underlying TCP transport
- **OpenSSL**: For SHA-1 hashing in handshake

## Usage

### As Part of network_system

When building as part of the main network_system project, the library is automatically integrated:

```cmake
# In your CMakeLists.txt
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::network-websocket)
```

### Standalone Build

```cmake
# In your CMakeLists.txt
find_package(network-websocket REQUIRED)
target_link_libraries(your_target PRIVATE kcenon::network-websocket)
```

### Example Code

```cpp
#include <network_websocket/websocket.h>

// Create a WebSocket socket wrapping an existing TCP socket
auto ws = std::make_shared<websocket_socket>(tcp_socket, true /* is_client */);

// Set message callback
ws->set_message_callback([](const ws_message& msg) {
    if (msg.type == ws_message_type::text) {
        std::cout << "Received: " << msg.as_text() << std::endl;
    }
});

// Perform client handshake
ws->async_handshake("example.com", "/chat", 80, [ws](std::error_code ec) {
    if (!ec) {
        // Handshake successful, start reading
        ws->start_read();

        // Send a text message
        ws->async_send_text("Hello, WebSocket!",
            [](std::error_code ec, std::size_t bytes) {
                if (!ec) {
                    std::cout << "Sent " << bytes << " bytes" << std::endl;
                }
            });
    }
});
```

## Library Structure

```
network-websocket/
├── include/
│   └── network_websocket/
│       ├── websocket.h                # Main public header
│       └── internal/
│           ├── websocket_socket.h     # WebSocket socket implementation
│           ├── websocket_frame.h      # Frame encoding/decoding
│           ├── websocket_handshake.h  # Handshake logic
│           └── websocket_protocol.h   # Protocol definitions
├── src/
│   ├── websocket_socket.cpp
│   ├── websocket_frame.cpp
│   ├── websocket_handshake.cpp
│   └── websocket_protocol.cpp
├── tests/
│   └── (test files)
├── CMakeLists.txt
├── network-websocket-config.cmake.in
└── README.md
```

## API Overview

### websocket_socket

The main class for WebSocket communication:

- `async_handshake()` - Perform client-side handshake
- `async_accept()` - Accept server-side handshake
- `start_read()` - Begin reading incoming messages
- `async_send_text()` - Send a text message
- `async_send_binary()` - Send a binary message
- `async_send_ping()` - Send a ping frame
- `async_close()` - Initiate connection close

### websocket_frame

Low-level frame operations:

- `encode_frame()` - Encode data into a WebSocket frame
- `decode_header()` - Parse a frame header
- `decode_payload()` - Extract payload from a frame

### websocket_handshake

HTTP/1.1 upgrade handshake:

- `create_client_handshake()` - Create client request
- `validate_server_response()` - Validate server response
- `parse_client_request()` - Parse client request (server-side)
- `create_server_response()` - Create server response

## License

BSD 3-Clause License. See LICENSE file for details.
