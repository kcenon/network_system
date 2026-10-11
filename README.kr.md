# Network System

Status: active | 공개 릴리스: [v0.1.1](https://github.com/kcenon/network_system/releases/tag/v0.1.1)

## 개요

독립형 ASIO를 사용하는 C++20 비동기 네트워크 라이브러리입니다. 서버와 클라이언트를 위한
TCP, UDP, TLS, WebSocket, HTTP, QUIC, gRPC 인터페이스를 제공합니다.

> **언어:** [English](README.md) | **한국어**

[![CI](https://github.com/kcenon/network_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/ci.yml)
[![Code Quality](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml)
[![Coverage](https://github.com/kcenon/network_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/coverage.yml)
[![Documentation](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml)

현재 프로젝트는 pre-1.0 단계입니다. 현재 API는 v1.0 후보이며 v1.0.0은 아직 출시되지 않았습니다.
[API 범위](docs/v1.0-api-surface.md)와
[릴리스 정책 및 과거 매니페스트 정정](ci/RELEASE_POLICY.md#published-versions-and-historical-erratum)을 참고하세요.
v0.1.1 태그의 CMake 버전은 0.1.1이지만 vcpkg 매니페스트 버전은 0.1.0입니다.

## 목차

- [요구사항](#요구사항)
- [시작하기](#시작하기)
- [문서](#문서)
- [개발 및 지원](#개발-및-지원)

## 요구사항

| 의존성 | 요구사항 |
|--------|----------|
| C++20 컴파일러 | GCC 13+, Clang 17+, MSVC 2022+ 또는 Apple Clang 14+ |
| CMake | 3.20+; 프리셋 사용 시 3.25+ |
| 독립형 ASIO | 1.30.2+; Boost.ASIO로 대체할 수 없음 |
| OpenSSL / zlib | OpenSSL 3.0+ 및 zlib |
| [common_system](https://github.com/kcenon/common_system), [thread_system](https://github.com/kcenon/thread_system) | 설치된 CMake 패키지 |
| logger_system / container_system | 선택 사항; logger 기본 OFF, container 탐색 기본 ON이며 패키지가 없으면 통합 비활성화 |

## 시작하기

공개 릴리스는 [vcpkg 레지스트리](https://github.com/kcenon/vcpkg-registry)로 설치합니다.

```bash
vcpkg install kcenon-network-system \
  --overlay-ports=path/to/kcenon/vcpkg-registry/ports
# 선택적 logger 통합
vcpkg install "kcenon-network-system[logging]" \
  --overlay-ports=path/to/kcenon/vcpkg-registry/ports
```

소비자 포트의 선택 기능은 `logging`입니다. 루트 개발 매니페스트의 `ssl`, `ecosystem`,
`testing` 같은 기능은 레지스트리 소비자 포트 기능이 아닙니다.

아래 예제는 **현재 main을 소스에서 빌드하여 설치한 패키지**를 대상으로 합니다.
먼저 [빌드 가이드](docs/guides/BUILD.kr.md)에 따라 network_system과 의존성을 설치하세요.
의존성 저장소를 복제하는 것만으로 라이브러리가 빌드되지는 않습니다.
공개 태그에 현재 API가 모두 포함되어 있다고 가정하지 마세요.

다음 코드를 `main.cpp`로 저장합니다.

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

int main() {
    kcenon::network::facade::tcp_facade tcp;
    auto created = tcp.create_server({.port = 9090});
    if (created.is_err()) {
        std::cerr << created.error().message << '\n';
        return 1;
    }
    auto server = created.value();
    auto started = server->start(9090);
    if (started.is_err()) {
        std::cerr << started.error().message << '\n';
        return 1;
    }
    std::cout << "Listening on port 9090; press Enter to stop." << std::endl;
    std::cin.get();
    auto stopped = server->stop();
    return stopped.is_err() ? 1 : 0;
}
```

다음을 `CMakeLists.txt`로 저장합니다.

```cmake
cmake_minimum_required(VERSION 3.20)
project(readme_example LANGUAGES CXX)
find_package(network_system CONFIG REQUIRED)
add_executable(readme_example main.cpp)
target_compile_features(readme_example PRIVATE cxx_std_20)
target_link_libraries(readme_example PRIVATE network_system::network_system)
```

설치 경로를 지정해 구성하고 서버를 실행합니다.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/network/install;/path/to/dependency/prefix"
cmake --build build --parallel
./build/readme_example
```

Visual Studio 빌드는 `--config Debug`를 지정하고 `build/Debug/readme_example.exe`를 실행합니다.
서버는 포트 9090에서 대기하며 Enter를 누르면 종료됩니다.
연결 예제는 [클라이언트/서버 가이드](docs/guides/QUICK_START.kr.md)를 참고하세요.

## 문서

| 주제 | 가이드 |
|------|--------|
| 설치, 프리셋, 선택한 예제 빌드 | [빌드 가이드](docs/guides/BUILD.kr.md) |
| 클라이언트 연결 준비와 데이터 수신 | [빠른 시작](docs/guides/QUICK_START.kr.md) |
| 프로토콜 타깃, 추적, 오류, 스레드 풀 | [사용 예제](docs/guides/USAGE_EXAMPLES.kr.md) |
| 지원 동작 및 구현 제한 | [프로토콜 지원 표](docs/PROTOCOL_SUPPORT.md) |
| API 참조 | [생성된 API 문서](https://kcenon.github.io/network_system/) |
| 설계 및 마이그레이션 | [아키텍처](docs/ARCHITECTURE.md), [마이그레이션](docs/MIGRATION.md) |
| 과거 측정 기록 및 근거의 한계 | [벤치마크](docs/BENCHMARKS.kr.md) |
| TLS 및 운영 | [TLS 설정](docs/guides/TLS_SETUP_GUIDE.md), [운영](docs/advanced/OPERATIONS.md) |

QUIC은 RFC 9000/9001/9002의 일부를 구현합니다. 복구 및 통계 제한은 지원 표를 참고하세요.
DTLS와 일부 추적 내보내기는 실험 단계입니다.

## 개발 및 지원

[CI](.github/workflows/ci.yml)에서 Linux, macOS, Windows 빌드를 실행합니다.
Unix 작업은 CTest를 실행하며 Windows 작업은 CTest 없이 빌드합니다.
새니타이저 작업에는 테스트 제외 및 억제 설정이 적용됩니다.
[TSan](tsan_suppressions.txt), [LSan](sanitizers/lsan_suppressions.txt),
[UBSan](cmake/ubsan.supp) 설정을 확인하세요. 새니타이저 작업은 참고용이며,
통과했다는 사실만으로 결함이 없다고 판단할 수 없습니다. LSan 억제에는 프로젝트 코드도 포함됩니다.
[커버리지 보고서](https://codecov.io/gh/kcenon/network_system)는 각 실행 결과를 보여 줍니다.

저장소의 일부 예제는 이전 API를 사용합니다. 빌드 가이드에 검증한 개별 타깃을 안내하며,
모든 예제를 한꺼번에 빌드하면 실패할 수 있습니다.

[기여 가이드](docs/contributing/CONTRIBUTING.md), [.clang-format](.clang-format),
[README 편집 정책](docs/contributing/README_POLICY.md)을 참고하세요.
버그와 질문은 [GitHub Issues](https://github.com/kcenon/network_system/issues)에 등록합니다.
릴리스 준비 상태는 [#964](https://github.com/kcenon/network_system/issues/964)와
[#1165](https://github.com/kcenon/network_system/issues/1165)에서 추적합니다.

## 라이선스

[BSD 3-Clause 라이선스](LICENSE)를 따릅니다.
