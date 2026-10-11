---
doc_id: "NET-GUID-034"
doc_title: "사용 예제"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# 사용 예제

[English / 한국어](USAGE_EXAMPLES.md)

[현재 main 빌드 및 설치](BUILD.kr.md) 후 사용합니다. 아래 C++ 프로그램은 각각 별도의 main.cpp입니다.
프로토콜 예제를 제외하면 [README](../../README.kr.md)의 CMakeLists.txt와 `network_system::network_system` 타깃을 사용하세요.

## 프로토콜별 링크

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

## 공개 통합 헤더

```cpp
#include <kcenon/network/network_system.h>
#include <kcenon/network/facade/tcp_facade.h>

int main() {
    kcenon::network::facade::tcp_facade tcp;
    auto server = tcp.create_server({.port = 8080});
    return server.is_err() ? 1 : 0;
}
```

## 콘솔 추적

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

## Result 오류 처리

이 코드 조각은 [코어 서버 예제](QUICK_START.kr.md#코어-서버)의 main()에서 server 생성 직후 기존 시작/오류 처리 부분을 대체합니다.

```cpp
auto result = server->start_server(8080);
if (result.is_err()) {
    const auto& err = result.error();
    std::cerr << "Error: " << err.message
              << " (code: " << err.code << ")\n";
    return -1;
}
```

## 스레드 통합

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

외부 실행기를 연결하려면 공개 `integration::thread_pool_interface`를 구현하고 `set_thread_pool()`로 등록하세요.
내부 어댑터 헤더는 소비자 API가 아닙니다. [추적 가이드](tracing.md)와 [프로토콜 제한](../PROTOCOL_SUPPORT.md)도 참고하세요.
