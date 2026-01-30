# network-all

Umbrella package that aggregates all network protocol libraries from `network_system`.

## Overview

`network-all` provides a single dependency for applications requiring multiple network protocols. Instead of linking each protocol library individually, link `network-all` to get:

| Library | Protocol | Description |
|---------|----------|-------------|
| network-core | N/A | Core interfaces and types |
| network-tcp | TCP | Reliable stream connections |
| network-udp | UDP | Unreliable datagrams |
| network-websocket | WebSocket | Full-duplex over HTTP |
| network-http2 | HTTP/2 | Multiplexed HTTP |
| network-quic | QUIC | UDP-based TLS 1.3 |
| network-grpc | gRPC | High-performance RPC |

## Installation

### CMake Integration (Recommended)

```cmake
# Option 1: find_package (after installation)
find_package(network-all REQUIRED)
target_link_libraries(my_app PRIVATE kcenon::network-all)

# Option 2: add_subdirectory (source inclusion)
add_subdirectory(libs/network-all)
target_link_libraries(my_app PRIVATE kcenon::network-all)
```

### Header Inclusion

```cpp
// Include all protocols
#include <network_all/network.h>

// Or include specific protocols
#include <network_tcp/protocol/tcp.h>
#include <network_udp/protocol/udp.h>
#include <network_websocket/websocket.h>
#include <network_http2/http2.h>
#include <network_quic/quic.h>
#include <network_grpc/grpc.h>
```

## Quick Start

### TCP Client

```cpp
#include <network_all/network.h>
#include <iostream>

int main() {
    using namespace kcenon::network;

    auto conn = protocol::tcp::connect({"localhost", 8080});
    conn->set_callbacks({
        .on_connected = []() {
            std::cout << "Connected!\n";
        },
        .on_data = [](std::span<const std::byte> data) {
            std::cout << "Received " << data.size() << " bytes\n";
        },
        .on_disconnected = [](std::string_view reason) {
            std::cout << "Disconnected: " << reason << "\n";
        }
    });

    // Connection in progress...
    return 0;
}
```

### TCP Server

```cpp
#include <network_all/network.h>

int main() {
    using namespace kcenon::network;

    auto listener = protocol::tcp::listen(8080);
    listener->set_callbacks({
        .on_accept = [](std::string_view conn_id) {
            std::cout << "New connection: " << conn_id << "\n";
        }
    });

    // Server is listening...
    return 0;
}
```

### gRPC Server

```cpp
#include <network_all/network.h>

int main() {
    using namespace kcenon::network::protocols::grpc;

    grpc_server server;
    server.register_unary_method("/mypackage.Service/Method",
        [](server_context& ctx, const std::vector<uint8_t>& request) {
            std::vector<uint8_t> response = process(request);
            return std::make_pair(grpc_status::ok_status(), response);
        });
    server.start(50051);

    return 0;
}
```

## Compile-Time Protocol Detection

Check if a protocol is available at compile time:

```cpp
#include <network_all/network.h>

using namespace kcenon::network;

static_assert(all::protocols::tcp, "TCP protocol required");

#if NETWORK_ALL_HAS_WEBSOCKET
    // WebSocket-specific code
#endif
```

## Selective Linking

For reduced binary size, link only the protocols you need:

```cmake
# Link only TCP and UDP
find_package(network-tcp REQUIRED)
find_package(network-udp REQUIRED)
target_link_libraries(my_app PRIVATE
    kcenon::network-tcp
    kcenon::network-udp
)
```

## Dependency Graph

```
                  network-core
                  /    |    \
                 /     |     \
          network-tcp  |  network-udp
                |      |      |
       network-websocket    network-quic
                |             |
            network-http2  network-grpc
```

## License

BSD 3-Clause License. See [LICENSE](../../LICENSE) for details.
