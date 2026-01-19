# 빠른 시작 가이드

> **Language:** [English](QUICK_START.md) | **한국어**

Network System을 5분 만에 시작하세요.

---

## 사전 요구사항

- CMake 3.20 이상
- C++20 지원 컴파일러 (GCC 11+, Clang 14+, MSVC 2022+, Apple Clang 14+)
- Git
- OpenSSL (TLS 지원용)
- ASIO (standalone, non-Boost)

### 에코시스템 의존성

| 의존성 | 필수 | 설명 |
|--------|------|------|
| [common_system](https://github.com/kcenon/common_system) | 예 | 공통 인터페이스 및 Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | 예 | 스레드 풀 및 비동기 작업 |
| [logger_system](https://github.com/kcenon/logger_system) | 예 | 로깅 인프라 |
| [container_system](https://github.com/kcenon/container_system) | 예 | 데이터 컨테이너 작업 |

## 설치

### 1. 저장소 클론

```bash
# 모든 에코시스템 의존성을 먼저 클론
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/container_system.git

# 의존성과 같은 위치에 network_system 클론
git clone https://github.com/kcenon/network_system.git
cd network_system
```

> **참고:** 빌드가 올바르게 작동하려면 모든 저장소가 같은 상위 디렉토리에 있어야 합니다.

### 2. 시스템 의존성 설치

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

### 3. 빌드

```bash
# 빌드 스크립트 사용
./scripts/build.sh

# 또는 CMake 직접 사용
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 4. 설치 확인

```bash
# 테스트 실행
./build/bin/network_system_tests

# 샘플 서버 실행
./build/bin/echo_server_sample
```

---

## 첫 번째 서버 만들기

간단한 TCP 에코 서버를 만들어 봅시다:

```cpp
#include <network_system/core/messaging_server.h>
#include <iostream>

int main() {
    // 1. 서버 인스턴스 생성
    auto server = std::make_shared<network_system::core::messaging_server>("EchoServer");

    // 2. 메시지 핸들러 설정
    server->set_message_handler([](auto session, auto data) {
        // 메시지를 다시 에코
        session->send(data);
    });

    // 3. 서버 시작
    auto result = server->start_server(8080);
    if (result.is_err()) {
        std::cerr << "시작 실패: " << result.error().message << "\n";
        return 1;
    }

    std::cout << "서버가 포트 8080에서 실행 중...\n";
    server->wait_for_stop();
    return 0;
}
```

---

## 첫 번째 클라이언트 만들기

서버에 연결하는 클라이언트를 만들어 봅시다:

```cpp
#include <network_system/core/messaging_client.h>
#include <iostream>

int main() {
    // 1. 클라이언트 인스턴스 생성
    auto client = std::make_shared<network_system::core::messaging_client>("EchoClient");

    // 2. 응답 핸들러 설정
    client->set_message_handler([](auto data) {
        std::string response(data.begin(), data.end());
        std::cout << "수신: " << response << "\n";
    });

    // 3. 서버에 연결
    auto result = client->start_client("localhost", 8080);
    if (result.is_err()) {
        std::cerr << "연결 실패: " << result.error().message << "\n";
        return 1;
    }

    // 4. 메시지 전송
    std::string message = "Hello, Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());
    client->send_packet(std::move(data));

    // 5. 응답 대기
    std::this_thread::sleep_for(std::chrono::seconds(1));
    client->stop();
    return 0;
}
```

---

## 애플리케이션 빌드

`CMakeLists.txt`에 추가:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_network_app)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# network_system 찾기
find_package(NetworkSystem REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE NetworkSystem::network)
```

---

## 핵심 개념

### Messaging Server
들어오는 연결과 메시지 처리를 담당합니다.

```cpp
auto server = std::make_shared<messaging_server>("ServerName");
server->start_server(port);
server->wait_for_stop();
```

### Messaging Client
서버에 연결하고 메시지를 송수신합니다.

```cpp
auto client = std::make_shared<messaging_client>("ClientName");
client->start_client("host", port);
client->send_packet(data);
```

### TLS 지원
TLS로 보안 연결을 활성화합니다.

```cpp
auto server = std::make_shared<messaging_server>("SecureServer");
server->set_tls_config(cert_file, key_file);
server->start_server(443, true);  // TLS 활성화
```

---

## 일반적인 패턴

### 오류 처리

```cpp
auto result = server->start_server(8080);
if (result.is_err()) {
    const auto& err = result.error();
    std::cerr << "오류: " << err.message
              << " (코드: " << err.code << ")\n";
    return 1;
}
```

### 정상 종료

```cpp
// 시그널 핸들러 등록
signal(SIGINT, [](int) {
    server->stop();
});

// 종료 대기
server->wait_for_stop();
```

### 연결 이벤트

```cpp
server->set_connection_handler([](auto session, bool connected) {
    if (connected) {
        std::cout << "클라이언트 연결됨\n";
    } else {
        std::cout << "클라이언트 연결 해제됨\n";
    }
});
```

---

## 다음 단계

- **[빌드 가이드](BUILD.kr.md)** - 상세한 빌드 지침
- **[TLS 설정 가이드](TLS_SETUP_GUIDE.kr.md)** - 보안 연결 구성
- **[문제 해결](TROUBLESHOOTING.kr.md)** - 일반적인 문제와 해결책
- **[부하 테스트 가이드](LOAD_TEST_GUIDE.md)** - 성능 테스트
- **[예제](../../examples/)** - 더 많은 샘플 애플리케이션

---

## 문제 해결

### 일반적인 문제

**ASIO 누락으로 빌드 실패:**
```bash
# Ubuntu/Debian
sudo apt install libasio-dev

# macOS
brew install asio
```

**OpenSSL 누락으로 빌드 실패:**
```bash
# Ubuntu/Debian
sudo apt install libssl-dev

# macOS
brew install openssl
```

**연결 거부:**
- 서버가 지정된 포트에서 실행 중인지 확인
- 방화벽 설정 확인
- 클라이언트의 호스트/포트가 올바른지 확인

더 많은 문제 해결 도움말은 [TROUBLESHOOTING.kr.md](TROUBLESHOOTING.kr.md)를 참조하세요.

---

*최종 업데이트: 2025-12-14*
