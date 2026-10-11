---
doc_id: "NET-GUID-030"
doc_title: "Usage Examples"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# Usage Examples

[English / 한국어](USAGE_EXAMPLES.kr.md)

Use these programs after [building and installing current main](BUILD.md). Each C++ program is a separate main.cpp.
Except for the protocol example, use the [README](../../README.md) CMakeLists.txt and `network_system::network_system` target.

## Selective protocol linking

```cmake
cmake_minimum_required(VERSION 3.20)
project(protocol_example LANGUAGES CXX)
find_package(network_system CONFIG REQUIRED)
add_executable(protocol_example main.cpp)
target_compile_features(protocol_example PRIVATE cxx_std_20)
target_link_libraries(protocol_example PRIVATE
    network_system::network-tcp
    network_system::network-udp
)
```

```cpp
#include <network_tcp/protocol/tcp.h>
#include <network_udp/protocol/udp.h>

int main() {
    auto tcp = kcenon::network::protocol::tcp::create_connection("readme-tcp");
    auto udp = kcenon::network::protocol::udp::create_connection("readme-udp");
    return tcp && udp ? 0 : 1;
}
```

## Public aggregate header

```cpp
#include <kcenon/network/network_system.h>
#include <kcenon/network/facade/tcp_facade.h>

int main() {
    kcenon::network::facade::tcp_facade tcp;
    auto server = tcp.create_server({.port = 8080});
    return server.is_err() ? 1 : 0;
}
```

## Console tracing

```cpp
#include <kcenon/network/tracing/tracing.h>

using namespace kcenon::network::tracing;

void process_request() {
    NETWORK_TRACE_SPAN("http.request.process");
    _span.set_attribute("http.method", "POST");
    _span.set_attribute("http.url", "/api/orders");
    {
        auto db_span = trace_context::current().create_child_span("database.query");
        db_span.set_attribute("db.system", "postgresql");
        db_span.set_status(span_status::ok);
    }
    _span.set_status(span_status::ok);
}

int main() {
    auto config = tracing_config::console();
    config.service_name = "my-service";
    configure_tracing(config);
    process_request();
    shutdown_tracing();
}
```

## Result error handling

This fragment replaces the start/error handling inside main() of the [core server example](QUICK_START.md#core-server), immediately after creating server.

```cpp
auto result = server->start_server(8080);
if (result.is_err()) {
    const auto& err = result.error();
    std::cerr << "Error: " << err.message
              << " (code: " << err.code << ")\n";
    return -1;
}
```

## Thread integration

```cpp
#include <kcenon/network/integration/thread_integration.h>
#include <iostream>
#include <memory>

int main() {
    using namespace kcenon::network::integration;
    auto pool = std::make_shared<basic_thread_pool>(2);
    auto& manager = thread_integration_manager::instance();
    manager.set_thread_pool(pool);
    manager.submit_task([] { std::cout << "Task completed\n"; }).get();
    std::cout << "Workers: " << manager.get_metrics().worker_threads << '\n';
    pool->stop();
}
```

For an external executor, implement the public `integration::thread_pool_interface` and register it with `set_thread_pool()`.
Internal adapter headers are not consumer APIs. See the [tracing guide](tracing.md) and [protocol limitations](../PROTOCOL_SUPPORT.md).
