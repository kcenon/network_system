# Quick Start Guide

> **Language:** **English** | [한국어](QUICK_START_KO.md)

Get up and running with Network System in 5 minutes.

---

## Prerequisites

- CMake 3.20 or later
- C++20 capable compiler (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- Git
- OpenSSL (for TLS support)
- ASIO (standalone, non-Boost)

### Ecosystem Dependencies

| Dependency | Required | Description |
|------------|----------|-------------|
| [common_system](https://github.com/kcenon/common_system) | Yes | Common interfaces and Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | Yes | Thread pool and async operations |
| [logger_system](https://github.com/kcenon/logger_system) | Yes | Logging infrastructure |
| [container_system](https://github.com/kcenon/container_system) | Yes | Data container operations |

## Installation

### 1. Clone the Repositories

```bash
# Clone all ecosystem dependencies first
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/container_system.git

# Clone network_system alongside the dependencies
git clone https://github.com/kcenon/network_system.git
cd network_system
```

> **Note:** All repositories must be in the same parent directory for the build to work correctly.

### 2. Install System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libssl-dev
```

**macOS:**
```bash
brew install cmake ninja asio openssl
```

**Windows (vcpkg):**
```bash
vcpkg install asio openssl
```

### 3. Build

```bash
# Using build script
./scripts/build.sh

# Or using CMake directly
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 4. Verify Installation

```bash
# Run tests
./build/bin/network_system_tests

# Run sample server
./build/bin/echo_server_sample
```

---

## Your First Server

Create a simple TCP echo server:

```cpp
#include <network_system/core/messaging_server.h>
#include <iostream>

int main() {
    // 1. Create server instance
    auto server = std::make_shared<network_system::core::messaging_server>("EchoServer");

    // 2. Set up message handler
    server->set_message_handler([](auto session, auto data) {
        // Echo the message back
        session->send(data);
    });

    // 3. Start the server
    auto result = server->start_server(8080);
    if (result.is_err()) {
        std::cerr << "Failed to start: " << result.error().message << "\n";
        return 1;
    }

    std::cout << "Server running on port 8080...\n";
    server->wait_for_stop();
    return 0;
}
```

---

## Your First Client

Create a client that connects to the server:

```cpp
#include <network_system/core/messaging_client.h>
#include <iostream>

int main() {
    // 1. Create client instance
    auto client = std::make_shared<network_system::core::messaging_client>("EchoClient");

    // 2. Set up response handler
    client->set_message_handler([](auto data) {
        std::string response(data.begin(), data.end());
        std::cout << "Received: " << response << "\n";
    });

    // 3. Connect to server
    auto result = client->start_client("localhost", 8080);
    if (result.is_err()) {
        std::cerr << "Failed to connect: " << result.error().message << "\n";
        return 1;
    }

    // 4. Send message
    std::string message = "Hello, Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());
    client->send_packet(std::move(data));

    // 5. Wait for response
    std::this_thread::sleep_for(std::chrono::seconds(1));
    client->stop();
    return 0;
}
```

---

## Building Your Application

Add to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_network_app)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find network_system
find_package(NetworkSystem REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE NetworkSystem::network)
```

Or using FetchContent:

```cmake
include(FetchContent)

FetchContent_Declare(
    common_system
    GIT_REPOSITORY https://github.com/kcenon/common_system.git
    GIT_TAG main
)

FetchContent_Declare(
    thread_system
    GIT_REPOSITORY https://github.com/kcenon/thread_system.git
    GIT_TAG main
)

FetchContent_Declare(
    logger_system
    GIT_REPOSITORY https://github.com/kcenon/logger_system.git
    GIT_TAG main
)

FetchContent_Declare(
    container_system
    GIT_REPOSITORY https://github.com/kcenon/container_system.git
    GIT_TAG main
)

FetchContent_Declare(
    network_system
    GIT_REPOSITORY https://github.com/kcenon/network_system.git
    GIT_TAG main
)

FetchContent_MakeAvailable(common_system thread_system logger_system container_system network_system)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE kcenon::network)
```

---

## Key Concepts

### Messaging Server
Handles incoming connections and message processing.

```cpp
auto server = std::make_shared<messaging_server>("ServerName");
server->start_server(port);
server->wait_for_stop();
```

### Messaging Client
Connects to servers and sends/receives messages.

```cpp
auto client = std::make_shared<messaging_client>("ClientName");
client->start_client("host", port);
client->send_packet(data);
```

### TLS Support
Enable secure connections with TLS.

```cpp
auto server = std::make_shared<messaging_server>("SecureServer");
server->set_tls_config(cert_file, key_file);
server->start_server(443, true);  // Enable TLS
```

---

## Common Patterns

### Error Handling

```cpp
auto result = server->start_server(8080);
if (result.is_err()) {
    const auto& err = result.error();
    std::cerr << "Error: " << err.message
              << " (code: " << err.code << ")\n";
    return 1;
}
```

### Graceful Shutdown

```cpp
// Register signal handler
signal(SIGINT, [](int) {
    server->stop();
});

// Wait for shutdown
server->wait_for_stop();
```

### Connection Events

```cpp
server->set_connection_handler([](auto session, bool connected) {
    if (connected) {
        std::cout << "Client connected\n";
    } else {
        std::cout << "Client disconnected\n";
    }
});
```

---

## Next Steps

- **[Build Guide](BUILD.md)** - Detailed build instructions
- **[TLS Setup Guide](TLS_SETUP_GUIDE.md)** - Configure secure connections
- **[Troubleshooting](TROUBLESHOOTING.md)** - Common issues and solutions
- **[Load Test Guide](LOAD_TEST_GUIDE.md)** - Performance testing
- **[Examples](../../examples/)** - More sample applications

---

## Troubleshooting

### Common Issues

**Build fails with missing ASIO:**
```bash
# Ubuntu/Debian
sudo apt install libasio-dev

# macOS
brew install asio
```

**Build fails with missing OpenSSL:**
```bash
# Ubuntu/Debian
sudo apt install libssl-dev

# macOS
brew install openssl
```

**Connection refused:**
- Verify the server is running on the specified port
- Check firewall settings
- Ensure correct host/port in client

For more troubleshooting help, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

---

*Last Updated: 2025-12-14*
