# 마이그레이션 가이드: messaging_system에서 network_system으로

> **Language:** [English](MIGRATION_GUIDE.md) | **한국어**

이 가이드는 레거시 `messaging_system` 네트워크 모듈에서 새로운 독립형 `network_system` 라이브러리로 마이그레이션하기 위한 단계별 지침을 제공합니다.

## 목차
1. [개요](#개요)
2. [빠른 마이그레이션](#빠른-마이그레이션)
3. [네임스페이스 변경](#네임스페이스-변경)
4. [API 변경](#api-변경)
5. [빌드 시스템 마이그레이션](#빌드-시스템-마이그레이션)
6. [마이그레이션 테스트](#마이그레이션-테스트)
7. [문제 해결](#문제-해결)
8. [성능 고려사항](#성능-고려사항)

## 개요

`network_system` 라이브러리는 `messaging_system`에서 네트워크 모듈을 완전히 추출하고 개선한 것입니다. 다음을 제공합니다:
- 네임스페이스 별칭을 통한 완전한 하위 호환성
- 향상된 성능 (305K+ msg/s 처리량)
- 더 나은 모듈성 및 재사용성
- 코루틴을 포함한 최신 C++20 기능
- 스레드 및 컨테이너 시스템과의 통합

### 마이그레이션 경로

마이그레이션을 위한 두 가지 옵션이 있습니다:

1. **호환성 모드** (초기 마이그레이션에 권장)
   - 최소한의 코드 변경으로 호환성 레이어 사용
   - 기존 네임스페이스 및 API 호출 유지
   - 점진적으로 최신 API로 마이그레이션

2. **전체 마이그레이션** (새 프로젝트에 권장)
   - 새로운 네임스페이스 구조 채택
   - 최신 API 직접 사용
   - 새로운 기능 활용

## 빠른 마이그레이션

### 단계 1: 의존성 업데이트

messaging_system 네트워크 의존성을 network_system으로 교체:

```cmake
# 이전 CMakeLists.txt
find_package(MessagingSystem REQUIRED)
target_link_libraries(your_app MessagingSystem::MessagingSystem)

# 새로운 CMakeLists.txt
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_app NetworkSystem::NetworkSystem)
```

### 단계 2: Include 업데이트 (호환성 모드)

```cpp
// 이전 코드
#include <messaging_system/network/tcp_server.h>
#include <messaging_system/network/tcp_client.h>

// 새로운 코드 (호환성 모드)
#include <network_system/compatibility.h>
```

### 단계 3: 시스템 초기화

```cpp
// main 함수에 초기화 추가
int main() {
    // 네트워크 시스템 초기화 (messaging_system init 대체)
    network_system::compat::initialize();

    // 애플리케이션 코드

    // 정리
    network_system::compat::shutdown();
    return 0;
}
```

## 네임스페이스 변경

### 레거시 네임스페이스 매핑

호환성 레이어는 다음 네임스페이스 별칭을 제공합니다:

```cpp
// 레거시 네임스페이스가 자동으로 사용 가능
namespace network_module {
    // network_system::core의 모든 타입
    using messaging_server = network_system::core::messaging_server;
    using messaging_client = network_system::core::messaging_client;
    // ... 등
}

namespace messaging {
    // 편의 별칭
    using server = network_system::core::messaging_server;
    using client = network_system::core::messaging_client;
}
```

### 최신 네임스페이스 구조

새 코드의 경우 최신 네임스페이스 구조 사용:

```cpp
// 최신 사용법
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

using namespace network_system::core;
```

## API 변경

### 서버 API

#### 레거시 API (여전히 지원됨)
```cpp
auto server = network_module::create_server("server_id");
server->set_message_handler([](const std::string& msg) {
    return "Response: " + msg;
});
server->start("0.0.0.0", 8080);
```

#### 최신 API
```cpp
auto server = std::make_shared<network_system::core::messaging_server>("server_id");
// 참고: set_message_handler는 현재 구현에서 사용 불가
// 메시지는 세션 레이어를 통해 처리됨
server->start_server(8080);
```

### 클라이언트 API

#### 레거시 API (여전히 지원됨)
```cpp
auto client = network_module::create_client("client_id");
client->connect("localhost", 8080);
client->send("Hello");
```

#### 최신 API
```cpp
auto client = std::make_shared<network_system::core::messaging_client>("client_id");
client->start_client("localhost", 8080);

// 바이트로 데이터 전송
std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
client->send_packet(data);
```

### 주요 API 차이점

| 기능 | 레거시 API | 최신 API |
|---------|------------|------------|
| 서버 시작 | `start(host, port)` | `start_server(port)` |
| 클라이언트 연결 | `connect(host, port)` | `start_client(host, port)` |
| 메시지 전송 | `send(string)` | `send_packet(vector<uint8_t>)` |
| 서버 중지 | `stop()` | `stop_server()` |
| 클라이언트 중지 | `disconnect()` | `stop_client()` |

## 빌드 시스템 마이그레이션

### CMake 구성

CMakeLists.txt를 생성하거나 업데이트:

```cmake
cmake_minimum_required(VERSION 3.16)
project(YourApplication)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NetworkSystem 찾기
find_package(NetworkSystem REQUIRED PATHS /path/to/network_system/build)

# 실행 파일 생성
add_executable(your_app main.cpp)

# NetworkSystem 링크
target_link_libraries(your_app PRIVATE NetworkSystem::NetworkSystem)

# 플랫폼별 설정
if(WIN32)
    target_link_libraries(your_app PRIVATE ws2_32 mswsock)
    target_compile_definitions(your_app PRIVATE _WIN32_WINNT=0x0601)
endif()
```

### 패키지 관리

#### vcpkg 사용 (선택 사항)
```json
{
  "name": "your-app",
  "version": "1.0.0",
  "dependencies": [
    "asio",
    "fmt"
  ]
}
```

#### 시스템 패키지 사용
```bash
# Ubuntu/Debian
sudo apt install libasio-dev libfmt-dev

# macOS
brew install asio fmt

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-asio mingw-w64-x86_64-fmt
```

## 마이그레이션 테스트

### 1. 컴파일 테스트

먼저 코드가 컴파일되는지 확인:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. 런타임 테스트

기능을 확인하기 위한 간단한 테스트 생성:

```cpp
#include <network_system/compatibility.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // 초기화
    network_system::compat::initialize();

    // 레거시 API를 사용하여 서버 생성
    auto server = network_module::create_server("test_server");
    server->start_server(9090);

    // 서버 시작 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 레거시 API를 사용하여 클라이언트 생성
    auto client = network_module::create_client("test_client");
    client->start_client("127.0.0.1", 9090);

    // 테스트 메시지 전송
    std::vector<uint8_t> data = {'T', 'e', 's', 't'};
    client->send_packet(data);

    // 정리
    client->stop_client();
    server->stop_server();

    network_system::compat::shutdown();

    std::cout << "Migration test successful!" << std::endl;
    return 0;
}
```

### 3. 성능 테스트

포함된 벤치마크를 실행하여 성능 확인:

```bash
./build/benchmark
```

예상 결과:
- 처리량: > 100K msg/s
- 지연시간: < 1ms 평균
- 동시 연결: 50+ 클라이언트

## 문제 해결

### 일반적인 문제 및 해결 방법

#### 1. 정의되지 않은 참조 오류

**문제**: network_system 함수에 대한 정의되지 않은 참조 관련 링커 오류.

**해결책**: NetworkSystem이 적절히 링크되었는지 확인:
```cmake
target_link_libraries(your_app PRIVATE NetworkSystem::NetworkSystem pthread)
```

#### 2. 네임스페이스를 찾을 수 없음

**문제**: `network_module` 네임스페이스를 찾을 수 없음.

**해결책**: 호환성 헤더 포함:
```cpp
#include <network_system/compatibility.h>
```

#### 3. API 메서드 누락

**문제**: `set_message_handler`와 같은 메서드를 사용할 수 없음.

**해결책**: 이러한 메서드는 현재 버전에 아직 구현되지 않았습니다. 세션 기반 메시지 처리 방식을 사용하거나 향후 업데이트를 기다리세요.

#### 4. 성능 저하

**문제**: 마이그레이션 후 성능 저하.

**해결책**:
- Release 모드로 빌드하고 있는지 확인: `cmake .. -DCMAKE_BUILD_TYPE=Release`
- ASIO가 적절히 구성되었는지 확인
- 스레드 풀 통합이 활성화되었는지 확인

#### 5. Container System을 찾을 수 없음

**문제**: 컨테이너 직렬화 기능이 작동하지 않음.

**해결책**: container_system이 사용 가능하고 적절히 구성되었는지 확인:
```cmake
add_compile_definitions(BUILD_WITH_CONTAINER_SYSTEM)
```

## 성능 고려사항

### 최적화 팁

1. **최신 API 사용**: 최신 API는 더 나은 성능 최적화를 제공합니다.

2. **스레드 풀 활성화**: 더 나은 비동기 성능을 위해 thread_system과 통합:
```cpp
auto& thread_mgr = network_system::integration::thread_integration_manager::instance();
// 스레드 풀은 비동기 작업에 자동으로 사용됨
```

3. **일괄 작업**: 가능한 경우 여러 메시지를 일괄 전송.

4. **연결 풀링**: 새 연결을 생성하는 대신 기존 연결 재사용.

### 성능 메트릭

마이그레이션 후 다음을 확인할 수 있어야 합니다:
- **처리량**: 305K+ messages/second (혼합 크기)
- **지연시간**: < 600μs 평균
- **동시 클라이언트**: 저하 없이 50+
- **메모리 사용량**: 연결당 ~10KB

### 애플리케이션 벤치마킹

포함된 벤치마크를 참조로 사용:

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>
#include <chrono>

// 처리량 측정
auto start = std::chrono::high_resolution_clock::now();
for (int i = 0; i < 10000; ++i) {
    client->send_packet(data);
}
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

double throughput = 10000.0 / (duration.count() / 1000.0);
std::cout << "Throughput: " << throughput << " msg/s" << std::endl;
```

## 마이그레이션 체크리스트

- [ ] NetworkSystem을 사용하도록 CMakeLists.txt 업데이트
- [ ] messaging_system include를 network_system으로 교체
- [ ] 초기화 및 종료 호출 추가
- [ ] 네임스페이스 참조 업데이트 (또는 호환성 레이어 사용)
- [ ] 변경된 메서드에 대한 API 호출 수정
- [ ] 컴파일 테스트
- [ ] 기능 테스트 실행
- [ ] 성능 메트릭 확인
- [ ] 문서 업데이트
- [ ] messaging_system 의존성 제거

## 지원

마이그레이션 지원:
- **GitHub Issues**: https://github.com/kcenon/network_system/issues
- **Email**: kcenon@naver.com
- **문서**: README.md 및 API 문서 참조

## 결론

messaging_system에서 network_system으로의 마이그레이션은 다음을 제공합니다:
- 더 나은 성능 (3배 처리량 개선)
- 더 깔끔한 아키텍처
- 최신 C++ 기능
- 완전한 하위 호환성
- 활발한 유지보수 및 업데이트

원활한 전환을 위해 호환성 모드로 시작한 다음, 최상의 성능과 기능을 위해 점진적으로 최신 API를 채택하세요.
