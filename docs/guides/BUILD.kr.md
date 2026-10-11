---
doc_id: "NET-GUID-032"
doc_title: "빌드 안내"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# 빌드 안내

[English](BUILD.md)

현재 main을 대상으로 하는 안내입니다. 공개 릴리스의 동작 및 과거 매니페스트 불일치는
[릴리스 정책](../../ci/RELEASE_POLICY.md#published-versions-and-historical-erratum)을 참고하세요.

## 요구사항

| 의존성 | 버전 및 역할 |
|--------|--------------|
| C++20 컴파일러 | GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+ |
| CMake | 3.20+; 프리셋 사용 시 3.25+ |
| 독립형 ASIO | 1.30.2+; Boost.ASIO를 지원하지 않음 |
| OpenSSL / zlib | OpenSSL 3.0+ 및 zlib |
| common_system | 필수; 공통 인터페이스와 Result<T> |
| thread_system | 필수; 스레드 풀 및 비동기 작업 |
| logger_system | 선택 사항, 기본 OFF; common_system ILogger로 런타임 바인딩 |
| container_system | 선택 사항, 탐색 기본 ON; 패키지가 없으면 통합 비활성화 |

## 시스템 패키지

아래 명령은 시스템 라이브러리를 설치합니다. 배포판의 패키지 버전이 요구사항을 충족하는지 확인하세요.
컴파일러와 common_system, thread_system, ASIO의 CMake 패키지를 별도로 준비해야 합니다.
LZ4는 선택 사항입니다.

Ubuntu/Debian (Ubuntu 22.04 이상은 기본적으로 OpenSSL 3.x를 제공합니다):

```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libssl-dev liblz4-dev zlib1g-dev
```

macOS:

```bash
brew install cmake ninja asio openssl@3 lz4 zlib
```

Windows (vcpkg):

```powershell
# vcpkg provides OpenSSL 3.x
vcpkg install openssl asio lz4 zlib --triplet x64-windows
```

Windows (MSYS2):

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-openssl
```

## 소스 설치

common_system, thread_system, ASIO의 CMake 패키지와 OpenSSL/zlib를 먼저 설치하세요.
CMAKE_PREFIX_PATH를 실제 설치 경로로 바꾸세요. 의존성을 복제하는 것만으로 정적 라이브러리가 빌드되지 않습니다.
아래 구성은 선택적 logger/container 통합을 비활성화합니다.

```bash
git clone --branch main https://github.com/kcenon/network_system.git
cd network_system
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix" \
  -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

cmake --install의 prefix는 network_system 설치 경로입니다. 소비자를 구성할 때 이 경로를 의존성 경로와 함께 추가하세요.
공개 타깃은 `network_system::network_system`입니다. 비공개 include 디렉터리를 추가하는 대신 이 타깃을 사용합니다.
[README 소비자 예제](../../README.kr.md#시작하기)와 [연결 예제](QUICK_START.kr.md)를 참고하세요.

## 프리셋 및 테스트

프리셋 스키마 6은 CMake 3.25 이상이 필요합니다. asan, tsan, ubsan에는 대응하는 테스트 프리셋이 있습니다.
release와 coverage에는 테스트 프리셋이 없으므로 해당 빌드 디렉터리에서 CTest를 실행하세요.
구성과 빌드에는 동일한 프리셋 이름을 사용합니다.

```bash
cmake --list-presets
# Choose one configure/build preset: debug, release, asan, tsan, ubsan, coverage.
# Substitute the same name in both commands.
cmake --preset debug -DBUILD_EXAMPLES=OFF \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix" \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build --preset debug
ctest --preset debug
```

컴파일러와 새니타이저 프로필마다 별도 디렉터리를 사용하세요. 수동 구성 예시:

```bash
# Configure a separate directory for each sanitizer/profile.
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix" \
  -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure --timeout 120
```

## 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `BUILD_TESTS` | ON | 단위 테스트 빌드 |
| `BUILD_EXAMPLES` | ON | 사용 예제 빌드 |
| `BUILD_TLS_SUPPORT` | ON | TLS/SSL 지원 |
| `BUILD_WEBSOCKET_SUPPORT` | ON | WebSocket 지원 |
| `NETWORK_BUILD_BENCHMARKS` | OFF | 성능 벤치마크 빌드 |
| `ENABLE_ASAN` | OFF | AddressSanitizer |
| `ENABLE_TSAN` | OFF | ThreadSanitizer |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer |
| `ENABLE_COVERAGE` | OFF | 코드 커버리지 |
| `NETWORK_ENABLE_GRPC_OFFICIAL` | OFF | 공식 gRPC 라이브러리 사용 |

## 선택한 예제

현재 소스에서 개별 빌드할 수 있는 타깃입니다.

| 소스 | CMake 타깃 |
|------|------------|
| [concepts_example.cpp](../../examples/concepts_example.cpp) | `example_concepts_example` |
| [thread_integration_example.cpp](../../examples/thread_integration_example.cpp) | `example_thread_integration_example` |
| [tls_policy_example.cpp](../../examples/tls_policy_example.cpp) | `example_tls_policy_example` |
| [http2_server_example.cpp](../../examples/http2_server_example.cpp) | `example_http2_server_example` |

다른 일부 예제는 이전 API를 사용하므로 모든 예제를 빌드하지 말고 필요한 타깃을 지정하세요.
아래 명령은 위에서 의존성을 구성한 소스 빌드 디렉터리를 재사용합니다.

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build --target example_thread_integration_example
./build/bin/examples/example_thread_integration_example
```

## 실험적 모듈

NETWORK_BUILD_MODULES는 별도의 실험적 모듈 타깃을 활성화합니다. CMake 3.28 이상과
모듈을 지원하는 툴체인이 필요합니다. 헤더 API와 서로 바꿔 사용할 수 있는 인터페이스가 아닙니다.
[현재 모듈 인터페이스](../../src/modules/)를 참고하세요. 빠른 시작 예제는 헤더 API를 사용합니다.

## 생성된 API 참조

저장소 루트에서 `doxygen Doxyfile`을 실행하면 `documents/html/index.html`이 생성됩니다.
[문서 워크플로](../../.github/workflows/build-Doxygen.yaml)가 [API 문서](https://kcenon.github.io/network_system/)를 게시합니다.
[경고 검사 워크플로](../../.github/workflows/doxygen-warnings-check.yml)는 관련 헤더/Doxyfile/워크플로를 수정하는 PR과
main, develop, phase-* 브랜치의 푸시에서 실행됩니다.

## 문제 해결

CMake가 common_system, thread_system, ASIO를 찾지 못하면 해당 CMake 패키지를 설치하고
CMAKE_PREFIX_PATH를 수정하세요. 인접한 디렉터리에 저장소가 있는 것만으로는 충분하지 않습니다.
[문제 해결](TROUBLESHOOTING.md)과 [GitHub Issues](https://github.com/kcenon/network_system/issues)를 참고하세요.
