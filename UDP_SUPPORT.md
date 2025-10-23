# UDP Support in Network System

This document describes the UDP protocol support added to the network_system library.

## Overview

The network_system now supports both TCP and UDP protocols:
- **TCP**: Connection-oriented, reliable, ordered delivery
- **UDP**: Connectionless, fast, message-oriented

## Components

### 1. UDP Socket (Low-level)

**File**: `include/network_system/internal/udp_socket.h`

Low-level wrapper around ASIO UDP socket providing:
- Asynchronous datagram send/receive
- Endpoint tracking
- Thread-safe operations

**Example**:
```cpp
asio::io_context io_context;
asio::ip::udp::socket raw_socket(io_context, udp::endpoint(udp::v4(), 0));
auto socket = std::make_shared<udp_socket>(std::move(raw_socket));

socket->set_receive_callback(
    [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
        // Handle received datagram
    });

socket->start_receive();

// Send datagram
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
asio::ip::udp::endpoint target(asio::ip::address::from_string("127.0.0.1"), 8080);
socket->async_send_to(std::move(data), target,
    [](std::error_code ec, std::size_t bytes) {
        // Handle send completion
    });
```

### 2. UDP Server

**File**: `include/network_system/core/messaging_udp_server.h`

High-level UDP server for receiving datagrams and sending responses:

**Example**:
```cpp
auto server = std::make_shared<messaging_udp_server>("MyUDPServer");

// Set callback to handle incoming datagrams
server->set_receive_callback(
    [server](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
        std::cout << "Received " << data.size() << " bytes from "
                  << sender.address().to_string() << ":" << sender.port() << "\n";

        // Send response back
        std::vector<uint8_t> response = {0x04, 0x05, 0x06};
        server->async_send_to(std::move(response), sender,
            [](std::error_code ec, std::size_t bytes) {
                if (!ec) {
                    std::cout << "Response sent: " << bytes << " bytes\n";
                }
            });
    });

// Start server on port 5555
auto result = server->start_server(5555);
if (!result) {
    std::cerr << "Failed to start server: " << result.error().message << "\n";
    return -1;
}

// Server runs in background thread
// ...

// Stop server
server->stop_server();
```

### 3. UDP Client

**File**: `include/network_system/core/messaging_udp_client.h`

High-level UDP client for sending datagrams to a target endpoint:

**Example**:
```cpp
auto client = std::make_shared<messaging_udp_client>("MyUDPClient");

// Set callback to handle responses
client->set_receive_callback(
    [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender) {
        std::cout << "Received response: " << data.size() << " bytes\n";
    });

// Start client targeting localhost:5555
auto result = client->start_client("localhost", 5555);
if (!result) {
    std::cerr << "Failed to start client: " << result.error().message << "\n";
    return -1;
}

// Send datagram
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
client->send_packet(std::move(data),
    [](std::error_code ec, std::size_t bytes) {
        if (!ec) {
            std::cout << "Sent " << bytes << " bytes\n";
        }
    });

// Change target endpoint
client->set_target("192.168.1.100", 9999);

// Stop client
client->stop_client();
```

## UDP vs TCP Comparison

| Feature | TCP | UDP |
|---------|-----|-----|
| Connection | Connection-oriented | Connectionless |
| Reliability | Guaranteed delivery | No guarantee |
| Ordering | Ordered | No ordering guarantee |
| Speed | Slower (overhead) | Faster (minimal overhead) |
| Message boundaries | Stream-based | Message-based |
| Use cases | File transfer, HTTP, Email | Gaming, Video streaming, DNS |

## UDP Characteristics

### Advantages
- **Low latency**: No connection setup overhead
- **Lightweight**: Minimal protocol overhead
- **Message boundaries**: Each send corresponds to one receive
- **Broadcast/Multicast**: Supports group communication (future enhancement)

### Considerations
- **No reliability**: Packets may be lost, duplicated, or reordered
- **No flow control**: Can overwhelm receivers
- **No congestion control**: Can congest networks
- **Application responsibility**: Must handle retransmission, ordering, etc.

## Build Configuration

UDP support can be enabled/disabled at build time:

```bash
# Enable UDP (default)
cmake -DNETWORK_ENABLE_UDP=ON ..

# Disable UDP
cmake -DNETWORK_ENABLE_UDP=OFF ..
```

When disabled, UDP components are not compiled, reducing binary size.

## Thread Safety

All UDP components are thread-safe:
- Callbacks are invoked on ASIO worker threads
- Socket access is protected by mutexes
- Atomic flags prevent race conditions
- send_packet() can be called from any thread

## Error Handling

UDP operations use the Result<T> pattern:

```cpp
auto result = client->start_client("localhost", 5555);
if (!result) {
    std::cerr << "Error: " << result.error().message << "\n";
    std::cerr << "Code: " << result.error().code << "\n";
    std::cerr << "Source: " << result.error().source << "\n";
    return -1;
}
```

## Implementation Details

### Architecture
- **udp_socket**: ASIO UDP socket wrapper (internal layer)
- **messaging_udp_server**: High-level server component (core layer)
- **messaging_udp_client**: High-level client component (core layer)

### Design Patterns
- **ASIO Async Pattern**: Non-blocking I/O with callbacks
- **RAII**: Automatic resource management
- **Shared Pointer**: Lifetime management for async operations
- **Result Type**: Consistent error handling

### Performance
- **Zero-copy**: Move semantics for data buffers
- **Async I/O**: No thread blocking on I/O operations
- **Dedicated threads**: Background io_context workers
- **Buffer size**: 65536 bytes (maximum UDP datagram size)

## Future Enhancements

Potential improvements for UDP support:
1. **Multicast support**: Group communication
2. **Broadcast support**: Network-wide messages
3. **Fragmentation handling**: Large message support
4. **Reliability layer**: Optional ACK/retransmission
5. **Rate limiting**: Prevent network congestion
6. **Statistics**: Packet loss, latency tracking

## Related Documentation

- Main README: `../README.md`
- TCP Support: Network system uses TCP by default
- Integration Guide: `../docs/integration.md` (if exists)
- API Reference: See header files for detailed API documentation

## Version History

- **2025-01-XX**: Initial UDP support implementation
  - udp_socket, messaging_udp_server, messaging_udp_client
  - Build configuration option (NETWORK_ENABLE_UDP)
  - Thread-safe operations
  - Result-based error handling
