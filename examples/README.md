# Network System Examples

Focused, minimal examples demonstrating common network\_system usage patterns. Each example is self-contained and can be built and run independently.

For more comprehensive demonstrations (HTTP, gRPC, QUIC, memory profiling), see the [samples/](../samples/) directory.

## Building

Examples are built when `BUILD_EXAMPLES=ON` is set (off by default):

```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build
```

Binaries are placed in `build/bin/examples/`.

## Examples

### tcp\_echo\_server

A minimal TCP server that echoes received data back to the client. Shows server lifecycle, session tracking, and callback-based event handling.

```bash
./build/bin/examples/example_tcp_echo_server
```

### tcp\_client

A TCP client that connects to a server, sends text and binary messages, and prints responses. Pair with `tcp_echo_server`.

```bash
# Terminal 1
./build/bin/examples/example_tcp_echo_server

# Terminal 2
./build/bin/examples/example_tcp_client
```

### websocket\_chat

A self-contained WebSocket chat demo. Starts a server and two clients in separate threads, demonstrating broadcast messaging over WebSocket.

Requires `BUILD_WEBSOCKET_SUPPORT=ON` (enabled by default).

```bash
./build/bin/examples/example_websocket_chat
```

### connection\_pool

Demonstrates TCP connection pooling with the `tcp_facade`. Covers pool initialization, single-threaded acquire/release, and multi-threaded concurrent access with throughput measurement.

```bash
./build/bin/examples/example_connection_pool
```

### udp\_echo

A UDP echo server and client running in a single program. Shows datagram-based communication with message boundary preservation.

```bash
./build/bin/examples/example_udp_echo
```

### observer\_pattern

Demonstrates the `connection_observer` interface, `null_connection_observer`, and `callback_adapter` for handling client events. Compares the observer pattern with callback-based approaches.

```bash
./build/bin/examples/example_observer_pattern
```

## Key Concepts

| Concept | Examples |
|---------|----------|
| TCP server/client | `tcp_echo_server`, `tcp_client` |
| UDP communication | `udp_echo` |
| WebSocket | `websocket_chat` |
| Connection pooling | `connection_pool` |
| Observer pattern | `observer_pattern` |
| Result\<T\> error handling | All examples |
| Session management | `tcp_echo_server`, `websocket_chat` |
| Facade API | All examples |

## API Quick Reference

All examples use the facade API (`tcp_facade`, `udp_facade`, `websocket_facade`) which provides a simplified interface for creating clients and servers. The facade returns `i_protocol_client` and `i_protocol_server` interfaces that offer a consistent API across all protocols.

### Common Pattern

```cpp
#include <kcenon/network/facade/tcp_facade.h>

using namespace kcenon::network;

facade::tcp_facade tcp;

// Server
auto server = tcp.create_server({ .port = 9000 });
server->set_receive_callback([](std::string_view session_id, const std::vector<uint8_t>& data) {
    // handle data
});
server->start(9000);

// Client
auto client = tcp.create_client({ .host = "127.0.0.1", .port = 9000 });
client->set_receive_callback([](const std::vector<uint8_t>& data) {
    // handle data
});
client->start("127.0.0.1", 9000);
client->send(std::vector<uint8_t>{'h', 'i'});
```
