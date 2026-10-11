---
doc_id: "NET-GUID-033"
doc_title: "빠른 시작"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# 빠른 시작

[English / 한국어](QUICK_START.md)

현재 main을 소스에서 설치한 패키지용 예제입니다. [빌드 가이드](BUILD.kr.md)를 먼저 따르세요.
[README](../../README.kr.md)의 CMakeLists.txt를 사용하고 각 프로그램을 별도 소비자 디렉터리의 main.cpp로 저장합니다.
공개 v0.1.1에 모든 현재 API가 포함되어 있지는 않습니다.

## 코어 서버

포트 8080에서 서버를 시작합니다. Enter를 누르면 종료됩니다.

```cpp
#include <kcenon/network/network_system.h>
#include <iostream>
#include <memory>

int main() {
    auto server = std::make_shared<kcenon::network::core::messaging_server>("MyServer");

    auto result = server->start_server(8080);
    if (result.is_err()) {
        const auto& err = result.error();
        std::cerr << "Failed to start: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
        return -1;
    }

    std::cout << "Server running on port 8080; press Enter to stop." << std::endl;
    std::cin.get();
    auto stopped = server->stop_server();
    return stopped.is_err() ? 1 : 0;
}
```

## 클라이언트

먼저 아래 수신 서버를 실행하세요. 연결은 비동기이므로 이 클라이언트는 준비 상태를 최대 5초 동안 확인한 후 전송하며, Enter를 누르면 종료합니다.

```cpp
#include <kcenon/network/network_system.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

int main() {
    auto client = std::make_shared<kcenon::network::core::messaging_client>("MyClient");

    auto result = client->start_client("127.0.0.1", 8080);
    if (result.is_err()) {
        const auto& err = result.error();
        std::cerr << "Failed to connect: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
        return -1;
    }

    // start_client initiates an asynchronous connection. Wait before sending.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!client->is_connected() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!client->is_connected()) {
        std::cerr << "Connection timed out\n";
        (void)client->stop_client();
        return 1;
    }

    // Send message (std::move avoids extra copies)
    std::string message = "Hello, Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());

    auto send_result = client->send_packet(std::move(data));
    if (send_result.is_err()) {
        const auto& err = send_result.error();
        std::cerr << "Failed to send: " << err.message
                  << " (code: " << err.code << ")" << std::endl;
    }

    std::cout << "Press Enter to stop the client." << std::endl;
    std::cin.get();
    auto stopped = client->stop_client();
    return send_result.is_err() || stopped.is_err() ? 1 : 0;
}
```

## 데이터를 수신하는 파사드 서버

수신한 바이트 수를 출력하려면 코어 서버 대신 이 서버를 실행하세요. 두 서버 모두 포트 8080을 사용하므로 동시에 실행하지 마세요.

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

int main() {
    kcenon::network::facade::tcp_facade tcp;
    auto created = tcp.create_server({.port = 8080});
    if (created.is_err()) {
        std::cerr << created.error().message << '\n';
        return 1;
    }
    auto server = created.value();
    server->set_receive_callback([](auto session_id, const auto& data) {
        std::cout << "Received " << data.size() << " bytes from "
                  << session_id << std::endl;
    });
    auto started = server->start(8080);
    if (started.is_err()) {
        std::cerr << started.error().message << '\n';
        return 1;
    }
    std::cout << "Listening on port 8080; press Enter to stop." << std::endl;
    std::cin.get();
    auto stopped = server->stop();
    return stopped.is_err() ? 1 : 0;
}
```

[프로토콜, 추적, 스레드 풀 예제](USAGE_EXAMPLES.kr.md) | [문제 해결](TROUBLESHOOTING.md)
