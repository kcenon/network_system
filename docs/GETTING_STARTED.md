# Getting Started with network_system

A step-by-step guide to using `network_system` -- from installation through TCP,
WebSocket, UDP, the facade API, TLS configuration, and connection pooling.

## Prerequisites

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| C++ standard | C++20 | C++20 |
| CMake | 3.20 | 3.28+ |
| GCC | 13 | 13+ |
| Clang | 17 | 17+ |
| Apple Clang | 14 | 15+ |
| MSVC | 2022 (17.0) | 2022 (17.8+) |
| OpenSSL | 3.0 | 3.x |
| ASIO | 1.30.2 | 1.30+ |

**Required dependencies**:
- [common_system](https://github.com/kcenon/common_system)
- [thread_system](https://github.com/kcenon/thread_system)
- OpenSSL 3.x, zlib >= 1.3, fmt >= 10.0

## Installation

### Option A -- CMake FetchContent (recommended)

```cmake
include(FetchContent)

FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(network_system)

target_link_libraries(your_target PRIVATE network_system)
```

### Option B -- Clone and build locally

```bash
git clone https://github.com/kcenon/network_system.git
cd network_system

# CMake presets (recommended)
cmake --preset release
cmake --build build

# Or manual
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Option C -- Add as a Git submodule

```bash
git submodule add https://github.com/kcenon/network_system.git external/network_system
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/network_system)
target_link_libraries(your_target PRIVATE network_system)
```

## TCP Echo Server / Client

### Echo Server

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

int main() {
    using namespace kcenon::network::facade;

    tcp_facade facade;
    auto server = facade.create_server({.port = 8080});

    // Set up message handler -- echo received data back
    server->set_on_data([&](auto session, const auto& data) {
        std::cout << "Received " << data.size() << " bytes\n";
        session->send(std::vector<uint8_t>(data.begin(), data.end()));
    });

    server->set_on_connected([](auto session) {
        std::cout << "Client connected: " << session->id() << "\n";
    });

    server->set_on_disconnected([](auto session) {
        std::cout << "Client disconnected: " << session->id() << "\n";
    });

    auto result = server->start();
    if (!result) {
        std::cerr << "Server failed to start\n";
        return 1;
    }

    std::cout << "Server listening on port 8080\n";
    // ... run until shutdown ...
    server->stop();
    return 0;
}
```

### Echo Client

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

int main() {
    using namespace kcenon::network::facade;

    tcp_facade facade;
    auto client = facade.create_client({
        .host = "127.0.0.1",
        .port = 8080,
        .client_id = "echo-client"
    });

    client->set_on_data([](const auto& data) {
        std::cout << "Echo: " << std::string(data.begin(), data.end()) << "\n";
    });

    auto result = client->start("127.0.0.1", 8080);
    if (!result) {
        std::cerr << "Failed to connect\n";
        return 1;
    }

    // Send a message
    std::string msg = "Hello, server!";
    client->send(std::vector<uint8_t>(msg.begin(), msg.end()));

    // ... wait for echo, then stop
    client->stop();
    return 0;
}
```

## WebSocket Chat

```cpp
#include <kcenon/network/facade/websocket_facade.h>
#include <iostream>

int main() {
    using namespace kcenon::network::facade;

    // Server
    websocket_facade ws_facade;
    auto server = ws_facade.create_server({
        .port = 9090,
        .path = "/chat",
        .server_id = "chat-server"
    });

    server->set_on_data([&](auto session, const auto& data) {
        // Broadcast message to all connected clients
        std::string msg(data.begin(), data.end());
        std::cout << session->id() << ": " << msg << "\n";
        server->broadcast(std::vector<uint8_t>(data.begin(), data.end()));
    });

    server->start();

    // Client
    auto client = ws_facade.create_client({
        .client_id = "chat-user-1",
        .ping_interval = std::chrono::seconds(30)
    });

    client->set_on_data([](const auto& data) {
        std::cout << "Message: " << std::string(data.begin(), data.end()) << "\n";
    });

    client->start("127.0.0.1", 9090);

    std::string msg = "Hello everyone!";
    client->send(std::vector<uint8_t>(msg.begin(), msg.end()));
    // ...
}
```

## UDP

```cpp
#include <kcenon/network/facade/udp_facade.h>

using namespace kcenon::network::facade;

udp_facade facade;

// Server
auto server = facade.create_server({
    .port = 5555,
    .server_id = "udp-server"
});

server->set_on_data([](auto session, const auto& data) {
    // Process datagram
});

server->start();

// Client
auto client = facade.create_client({
    .host = "127.0.0.1",
    .port = 5555,
    .client_id = "udp-client"
});

client->start("127.0.0.1", 5555);
client->send(std::vector<uint8_t>{0x01, 0x02, 0x03});
```

## Facade API

All protocols share the same facade pattern:

| Facade | Header | Protocols |
|--------|--------|-----------|
| `tcp_facade` | `facade/tcp_facade.h` | TCP, TCP+TLS |
| `udp_facade` | `facade/udp_facade.h` | UDP |
| `websocket_facade` | `facade/websocket_facade.h` | WebSocket |
| `http_facade` | `facade/http_facade.h` | HTTP/1.1, HTTPS |
| `quic_facade` | `facade/quic_facade.h` | QUIC (experimental) |

Each facade provides:

```cpp
// Create client
auto client = facade.create_client(client_config{...});

// Create server
auto server = facade.create_server(server_config{...});
```

Both return `i_protocol_client` / `i_protocol_server` interfaces with a
consistent API across protocols.

### Observer Pattern (recommended for complex apps)

```cpp
class my_observer : public kcenon::network::interfaces::connection_observer {
public:
    void on_connected(std::shared_ptr<i_session> session) override { /* ... */ }
    void on_disconnected(std::shared_ptr<i_session> session) override { /* ... */ }
    void on_data(std::shared_ptr<i_session> session,
                 const std::vector<uint8_t>& data) override { /* ... */ }
    void on_error(std::error_code ec) override { /* ... */ }
};

auto observer = std::make_shared<my_observer>();
client->set_observer(observer);
```

## TLS

Enable TLS by setting `use_ssl = true` in the facade config:

```cpp
// Secure TCP client
auto client = tcp_facade.create_client({
    .host = "example.com",
    .port = 8443,
    .use_ssl = true,
    .ca_cert_path = "/path/to/ca.pem"
});

// Secure TCP server
auto server = tcp_facade.create_server({
    .port = 8443,
    .use_ssl = true,
    .cert_path = "/path/to/server.pem",
    .key_path = "/path/to/server.key"
});

// HTTPS client
auto https_client = http_facade.create_client({
    .client_id = "secure-http",
    .use_ssl = true
});
```

For advanced TLS configuration (policy-based), see the
[UNIFIED_API_GUIDE.md](UNIFIED_API_GUIDE.md).

## Next Steps

| Topic | Resource |
|-------|----------|
| Full API surface | [API_REFERENCE.md](API_REFERENCE.md) |
| API cheat sheet | [API_QUICK_REFERENCE.md](API_QUICK_REFERENCE.md) |
| Facade guide | [FACADE_GUIDE.md](FACADE_GUIDE.md) |
| Unified template API | [UNIFIED_API_GUIDE.md](UNIFIED_API_GUIDE.md) |
| DTLS resilient guide | [DTLS_RESILIENT_GUIDE.md](DTLS_RESILIENT_GUIDE.md) |
| HTTP/2 | [HTTP2_GUIDE.md](HTTP2_GUIDE.md) |
| Architecture | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Benchmarks | [BENCHMARKS.md](BENCHMARKS.md) |
