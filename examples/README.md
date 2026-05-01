# Network System Examples

Focused examples demonstrating common network\_system usage patterns. Each example is
self-contained and can be built and run independently. This directory is the single
canonical location for examples (the legacy `samples/` directory has been merged here).

Examples cover both the modern facade API (`tcp_facade`, `udp_facade`, etc.) and a
selection of lower-level APIs where protocol-specific features (multi-stream QUIC,
HTTP request handlers, etc.) are required.

## Building

Examples are built when `BUILD_EXAMPLES=ON` is set (default ON):

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

### basic\_usage

Demonstrates the facade API for creating TCP clients and servers. A good first example
to read after the focused `tcp_*` examples.

```bash
./build/bin/examples/example_basic_usage
```

### concepts\_example

Demonstrates the use of C++20 concepts to constrain protocol tags. Shows how the
`Protocol` concept enables compile-time validation and self-documenting template
interfaces.

```bash
./build/bin/examples/example_concepts_example
```

### memory\_profile\_demo

Profiles memory usage patterns under various network workloads (connection overhead,
message throughput, long-running stability).

```bash
./build/bin/examples/example_memory_profile_demo
```

### simple\_http\_client / simple\_http\_server

Lower-level HTTP client and server using the `http_client` / `http_server` classes
directly (instead of the `http_facade`). Useful when you need HTTP-specific features
such as request routing or response parsing that the facade does not expose.

```bash
./build/bin/examples/example_simple_http_server
./build/bin/examples/example_simple_http_client
```

### http\_facade\_example

The same role as `simple_http_*` but using the `http_facade` API. Use this when you
want a single, simplified interface and you don't need HTTP-specific extensions.

```bash
./build/bin/examples/example_http_facade_example
```

### http2\_server\_example

HTTP/2 server using the experimental HTTP/2 implementation. Demonstrates h2c (HTTP/2
cleartext) for testing; production deployments should configure TLS.

```bash
./build/bin/examples/example_http2_server_example
```

### quic\_client\_example / quic\_server\_example

Lower-level QUIC client/server using `messaging_quic_client` / `messaging_quic_server`
directly. Required when you need QUIC-specific features such as multi-stream support,
0-RTT, broadcasting, or session statistics that the `quic_facade` does not expose.

```bash
./build/bin/examples/example_quic_server_example
./build/bin/examples/example_quic_client_example
```

### quic\_facade\_example

The simplified `quic_facade` API for QUIC server/client lifecycle. Use this when you
do not need QUIC-specific extensions.

```bash
./build/bin/examples/example_quic_facade_example
```

### grpc\_service\_example

Demonstrates gRPC server and client using the facade API.

```bash
./build/bin/examples/example_grpc_service_example
```

### tls\_policy\_example

Demonstrates TLS policy configuration for secure connections.

```bash
./build/bin/examples/example_tls_policy_example
```

### thread\_integration\_example / network\_metrics\_example / network\_tracing\_example

Integration examples covering thread pool offload, EventBus-based metrics collection,
and distributed tracing for network operations.

```bash
./build/bin/examples/example_thread_integration_example
./build/bin/examples/example_network_metrics_example
./build/bin/examples/example_network_tracing_example
```

### unified\_messaging\_example

Demonstrates the unified transport interface that abstracts over multiple protocol
implementations behind a single template.

```bash
./build/bin/examples/example_unified_messaging_example
```

### messaging\_system\_integration / migration

Standalone subdirectories demonstrating integration with messaging\_system and
migration paths from the legacy API. Each contains its own CMakeLists.txt and is
built independently of the main `BUILD_EXAMPLES` flag.

## Key Concepts

| Concept | Examples |
|---------|----------|
| TCP server/client | `tcp_echo_server`, `tcp_client`, `basic_usage` |
| UDP communication | `udp_echo` |
| WebSocket | `websocket_chat` |
| HTTP | `http_facade_example`, `simple_http_client`, `simple_http_server` |
| HTTP/2 | `http2_server_example` |
| QUIC | `quic_facade_example`, `quic_client_example`, `quic_server_example` |
| gRPC | `grpc_service_example` |
| Connection pooling | `connection_pool` |
| Observer pattern | `observer_pattern` |
| TLS policy | `tls_policy_example` |
| Concepts (C++20) | `concepts_example` |
| Memory profiling | `memory_profile_demo` |
| Tracing & metrics | `network_tracing_example`, `network_metrics_example` |
| Thread integration | `thread_integration_example` |
| Unified template | `unified_messaging_example` |
| Result\<T\> error handling | All examples |
| Facade API | Most examples |

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
