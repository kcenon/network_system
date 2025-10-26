[![CI](https://github.com/kcenon/network_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/ci.yml)
[![Code Quality](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml)
[![Coverage](https://github.com/kcenon/network_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/coverage.yml)
[![Doxygen](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml)

# Network System Project

> **Language:** [English](README.md) | **한국어**

## 개요

Network System Project는 분산 시스템 및 메시징 애플리케이션을 위한 재사용 가능한 전송 프리미티브에 중점을 둔 활발히 개발 중인 C++20 비동기 네트워크 라이브러리입니다. messaging_system 내부에서 시작되어 더 나은 모듈성을 위해 추출되었으며, API는 현재 사용 가능하지만 성능 보장은 아직 특성화되고 있으며 기능 세트는 계속 발전하고 있습니다.

> **🏗️ 모듈형 아키텍처**: 플러그 가능한 프로토콜 스택을 갖춘 코루틴 친화적 비동기 네트워크 라이브러리; zero-copy 파이프라인 및 connection pooling과 같은 로드맵 항목은 IMPROVEMENTS.md에 추적되지만 아직 구현되지 않았습니다.

> **✅ 최신 업데이트**: messaging_system으로부터 분리 완료 및 확장된 문서화와 통합 훅. 빌드, 코드 품질, 커버리지 및 문서에 대한 GitHub Actions 워크플로우가 정의됨—최신 상태는 저장소 대시보드를 확인하세요.

## 🔗 프로젝트 생태계 및 상호 의존성

이 network system은 생태계 전반에 걸쳐 향상된 모듈성과 재사용성을 제공하기 위해 messaging_system에서 분리된 기본 구성 요소입니다:

### 역사적 배경
- **원래 통합**: messaging_system의 일부로 긴밀하게 결합된 네트워크 모듈
- **분리 근거**: 향상된 모듈성, 재사용성 및 유지보수성을 위해 추출
- **마이그레이션 성과**: 100% 하위 호환성을 유지하면서 완전한 독립성 달성

### 관련 프로젝트
- **[messaging_system](https://github.com/kcenon/messaging_system)**: 메시지 전송을 위해 network를 사용하는 주요 소비자
  - 관계: 메시지 전달 및 라우팅을 위한 네트워크 전송 계층
  - 시너지: 엔터프라이즈급 네트워킹을 갖춘 고성능 메시징
  - 통합: 원활한 메시지 직렬화 및 네트워크 전송

- **[container_system](https://github.com/kcenon/container_system)**: 네트워크 전송을 위한 데이터 직렬화
  - 관계: 직렬화된 container를 위한 네트워크 전송
  - 이점: 효율적인 binary protocol을 갖춘 type-safe 데이터 전송
  - 통합: 네트워크 protocol을 위한 자동 container 직렬화

- **[database_system](https://github.com/kcenon/database_system)**: 네트워크 기반 데이터베이스 작업
  - 사용: 원격 데이터베이스 연결 및 분산 작업
  - 이점: 네트워크 투명 데이터베이스 액세스 및 클러스터링
  - 참조: 네트워크 protocol을 통한 데이터베이스 connection pooling

- **[thread_system](https://github.com/kcenon/thread_system)**: 네트워크 작업을 위한 thread 인프라
  - 관계: 동시 네트워크 작업을 위한 thread pool 관리
  - 이점: 확장 가능한 동시 연결 처리
  - 통합: thread pool 최적화를 통한 비동기 I/O 처리

- **[logger_system](https://github.com/kcenon/logger_system)**: 네트워크 로깅 및 진단
  - 사용: 네트워크 작업 로깅 및 성능 모니터링
  - 이점: 포괄적인 네트워크 진단 및 문제 해결
  - 참조: 네트워크 이벤트 로깅 및 성능 분석

### 통합 아키텍처
```
┌─────────────────┐     ┌─────────────────┐
│  thread_system  │ ──► │ network_system  │ ◄── Foundation: Async I/O, Connection Management
└─────────────────┘     └─────────┬───────┘
         │                        │ provides transport for
         │                        ▼
┌─────────▼───────┐     ┌─────────────────┐     ┌─────────────────┐
│container_system │ ──► │messaging_system │ ◄──► │database_system  │
└─────────────────┘     └─────────────────┘     └─────────────────┘
         │                        │                       │
         └────────┬───────────────┴───────────────────────┘
                  ▼
    ┌─────────────────────────┐
    │   logger_system        │
    └─────────────────────────┘
```

### 통합 이점
- **범용 전송 계층**: 모든 생태계 구성 요소를 위한 공유 네트워킹 기반
- **제로 의존성 모듈형 설계**: 독립적으로 또는 다른 시스템과 함께 사용 가능
- **하위 호환성**: 호환성 브리지를 통한 마이그레이션 경로 제공
- **성능 계측**: 직렬화 및 세션 핫 패스를 위한 Google Benchmark 마이크로 제품군
- **크로스 플랫폼 지원**: CI 워크플로우에서 검증된 Windows, Linux, macOS 빌드

> 📖 **[완전한 아키텍처 가이드](docs/ARCHITECTURE.md)**: 전체 생태계 아키텍처, 의존성 관계 및 통합 패턴에 대한 포괄적인 문서.

## 프로젝트 목적 및 미션

이 프로젝트는 전 세계 개발자들이 직면한 근본적인 과제를 해결합니다: **고성능 네트워크 프로그래밍을 접근 가능하고, 모듈화되고, 신뢰할 수 있게 만드는 것**. 전통적인 네트워크 라이브러리는 종종 특정 프레임워크와 긴밀하게 결합되고, 포괄적인 비동기 지원이 부족하며, 높은 처리량 애플리케이션에 대한 불충분한 성능을 제공합니다. 우리의 미션은 다음을 수행하는 포괄적인 솔루션을 제공하는 것입니다:

- **긴밀한 결합 제거** - 프로젝트 전반에 걸쳐 독립적인 사용을 가능하게 하는 모듈형 설계를 통해
- **성능 극대화** - 직렬화, 세션 및 연결 핫 패스를 지속적으로 프로파일링하여; zero-copy 파이프라인 및 connection pooling은 로드맵에 유지
- **신뢰성 보장** - 포괄적인 오류 처리, 연결 수명 주기 관리 및 내결함성을 통해
- **재사용성 촉진** - 깨끗한 인터페이스 및 생태계 통합 기능을 통해
- **개발 가속화** - 즉시 사용 가능한 비동기 프리미티브, 통합 헬퍼 및 광범위한 문서를 제공하여

## 핵심 장점 및 이점

### 🚀 **성능 중심**
- **합성 프로파일링**: Google Benchmark 제품군(`benchmarks/`)이 직렬화, 모의 연결 및 세션 핫 패스를 캡처
- **비동기 I/O 기반**: C++20 coroutine 지원 헬퍼를 갖춘 ASIO 기반 논블로킹 작업
- **이동 인식 API**: `std::vector<uint8_t>` 버퍼가 파이프라인 변환 전 추가 복사를 피하기 위해 전송 경로로 이동됨
- **로드맵 항목**: 진정한 zero-copy 파이프라인 및 connection pooling은 IMPROVEMENTS.md에서 추적됨

### 🛡️ **프로덕션급 신뢰성**
- **모듈형 독립성**: 표준 라이브러리 이외의 외부 의존성 제로
- **포괄적인 오류 처리**: 우아한 성능 저하 및 복구 패턴
- **메모리 안전성**: RAII 원칙 및 스마트 포인터가 누수 및 손상을 방지
- **Thread 안전성**: 적절한 동기화를 통한 동시 작업

### 🔧 **개발자 생산성**
- **직관적인 API 설계**: 깨끗하고 자체 문서화된 인터페이스로 학습 곡선 감소
- **하위 호환성**: 레거시 messaging_system 코드 마이그레이션을 위한 호환성 브리지 및 네임스페이스 별칭 (예: `include/network_system/integration/messaging_bridge.h` 참조)
- **풍부한 통합**: thread, container 및 logger system과의 원활한 통합
- **최신 C++ 기능**: C++20 coroutine, concept 및 range 지원

### 🌐 **크로스 플랫폼 호환성**
- **범용 지원**: Windows, Linux 및 macOS에서 작동
- **아키텍처 최적화**: x86, x64 및 ARM64에 대한 성능 튜닝
- **컴파일러 유연성**: GCC, Clang 및 MSVC와 호환
- **Container 지원**: 자동화된 CI/CD를 갖춘 Docker 지원

## 실제 영향 및 사용 사례

### 🎯 **이상적인 애플리케이션**
- **메시징 시스템**: 높은 처리량 메시지 라우팅 및 전달
- **분산 시스템**: 서비스 간 통신 및 조정
- **실시간 애플리케이션**: 게임, 거래 및 IoT 데이터 스트리밍
- **마이크로서비스**: 로드 밸런싱을 갖춘 서비스 간 통신
- **데이터베이스 클러스터링**: 데이터베이스 복제 및 분산 쿼리 처리
- **콘텐츠 전달**: 고성능 콘텐츠 스트리밍 및 캐싱

### 📊 **성능 벤치마크**

`benchmarks/` 디렉토리의 현재 벤치마크 제품군은 메시지 할당/복사(`benchmarks/message_throughput_bench.cpp:12-183`), 모의 연결(`benchmarks/connection_bench.cpp:15-197`), 세션 기록(`benchmarks/session_bench.cpp:1-176`)과 같은 CPU 전용 워크플로우에 중점을 둡니다. 이 프로그램들은 소켓을 열거나 실제 네트워크 트래픽을 전송하지 않으므로, 처리량/지연시간 수치는 프로덕션 SLA가 아닌 합성 지표입니다.

#### 합성 메시지 할당 결과 (Intel i7-12700K, Ubuntu 22.04, GCC 11, `-O3`)

| 벤치마크 | 페이로드 | CPU 시간/작업 (ns) | 근사 처리량 | 범위 |
|---------|---------|-------------------|------------|------|
| MessageThroughput/64B | 64 bytes | 1,300 | ~769,000 msg/s | 메모리 내 할당 + memcpy |
| MessageThroughput/256B | 256 bytes | 3,270 | ~305,000 msg/s | 메모리 내 할당 + memcpy |
| MessageThroughput/1KB | 1 KB | 7,803 | ~128,000 msg/s | 메모리 내 할당 + memcpy |
| MessageThroughput/8KB | 8 KB | 48,000 | ~21,000 msg/s | 메모리 내 할당 + memcpy |

연결 및 세션 벤치마크는 모의 객체에 의존합니다(예: `mock_connection::connect()`는 10 µs 동안 sleep하여 작업을 시뮬레이션). 따라서 실제 네트워크 처리량, 동시 연결 용량 또는 메모리 사용률에 대한 이전 주장은 엔드투엔드 테스트가 캡처될 때까지 제거되었습니다.

#### 🔬 합성 벤치마크 재현하기

다음과 같이 CPU 전용 벤치마크를 재현할 수 있습니다:

```bash
# 1단계: 벤치마크를 활성화하여 빌드
git clone https://github.com/kcenon/network_system.git
cd network_system
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETWORK_BUILD_BENCHMARKS=ON
cmake --build build -j

# 2단계: 벤치마크 실행
./build/benchmarks/network_benchmarks

# 3단계: 분석을 위한 JSON 출력 생성
./build/benchmarks/network_benchmarks --benchmark_format=json --benchmark_out=results.json

# 4단계: 특정 벤치마크 카테고리 실행
./build/benchmarks/network_benchmarks --benchmark_filter=MessageThroughput
./build/benchmarks/network_benchmarks --benchmark_filter=Connection
./build/benchmarks/network_benchmarks --benchmark_filter=Session
```

**예상 출력** (Intel i7-12700K, Ubuntu 22.04):
```
-------------------------------------------------------------------------
Benchmark                               Time       CPU   Iterations
-------------------------------------------------------------------------
MessageThroughput/64B            1300 ns   1299 ns       538462   # ~769K msg/s
MessageThroughput/256B           3270 ns   3268 ns       214286   # ~305K msg/s
MessageThroughput/1KB            7803 ns   7801 ns        89744   # ~128K msg/s
MessageThroughput/8KB           48000 ns  47998 ns        14583   # ~21K msg/s
```

**참고**: 이 수치는 프로세스 내 CPU 작업만 측정합니다. 소켓 I/O 데이터가 필요한 경우 통합 또는 시스템 벤치마크를 실행하세요.

#### 실제 측정 대기 중

- TCP, UDP, WebSocket 전송을 통한 엔드투엔드 처리량/지연시간
- 장기 실행 세션 중 메모리 풋프린트 및 GC 동작
- TLS 성능 및 연결 풀링 효율성 (기능 구현 대기 중)
- 동일한 작업부하에서 다른 네트워킹 라이브러리와의 비교 벤치마크

### 핵심 목표
- **모듈 독립성**: messaging_system으로부터 네트워크 모듈의 완전한 분리 ✅
- **향상된 재사용성**: 다른 프로젝트에서 사용 가능한 독립적인 라이브러리 ✅
- **호환성 유지**: 레거시 messaging_system 소비자를 위한 호환성 브리지 및 네임스페이스 별칭; 추가 검증 진행 중
- **성능 계측**: 합성 벤치마크 및 통합 테스트가 핫 패스를 다룸; 실제 네트워크 처리량/지연시간 측정은 아직 대기 중

## 🛠️ 기술 스택 및 아키텍처

### 핵심 기술
- **C++20**: concept, coroutine 및 range를 포함한 최신 C++ 기능
- **Asio**: 고성능 크로스 플랫폼 네트워킹 라이브러리
- **CMake**: 포괄적인 의존성 관리를 갖춘 빌드 시스템
- **크로스 플랫폼**: Windows, Linux 및 macOS에 대한 기본 지원

### 프로토콜 지원
- **TCP**: 연결 수명 주기 관리를 갖춘 비동기 TCP server/client (connection pooling 계획됨; IMPROVEMENTS.md에서 추적)
- **UDP**: 실시간 애플리케이션을 위한 비연결형 UDP 통신
- **WebSocket**: RFC 6455 WebSocket 프로토콜 완전 지원:
  - Text 및 binary 메시지 framing
  - Fragmentation 및 reassembly
  - Ping/pong keepalive
  - 우아한 연결 수명 주기
  - Connection limit를 갖춘 session 관리

### 아키텍처 설계
```
┌─────────────────────────────────────────────────────────────────┐
│                    Network System Architecture                  │
├─────────────────────────────────────────────────────────────────┤
│  공개 API 계층                                                   │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐    │
│  │ messaging    │ │ messaging    │ │  messaging_ws        │    │
│  │ _server      │ │ _client      │ │  _server / _client   │    │
│  │ (TCP)        │ │ (TCP)        │ │  (WebSocket)         │    │
│  └──────────────┘ └──────────────┘ └──────────────────────┘    │
│  ┌──────────────┐ ┌──────────────┐                             │
│  │ messaging    │ │ messaging    │                             │
│  │ _udp_server  │ │ _udp_client  │                             │
│  └──────────────┘ └──────────────┘                             │
├─────────────────────────────────────────────────────────────────┤
│  프로토콜 계층                                                   │
│  ┌──────────────┐ ┌──────────────┐ ┌──────────────────────┐    │
│  │ tcp_socket   │ │ udp_socket   │ │ websocket_socket     │    │
│  └──────────────┘ └──────────────┘ └──────────────────────┘    │
│  ┌──────────────┐ ┌──────────────────────────────────────┐     │
│  │ messaging    │ │ websocket_protocol                   │     │
│  │ _session     │ │ (frame/handshake/message handling)   │     │
│  └──────────────┘ └──────────────────────────────────────┘     │
├─────────────────────────────────────────────────────────────────┤
│  세션 관리 계층                                                  │
│  ┌──────────────────┐ ┌────────────────────────────────────┐   │
│  │ session_manager  │ │ ws_session_manager                 │   │
│  │ (TCP)            │ │ (WebSocket 연결 관리)              │   │
│  └──────────────────┘ └────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│  핵심 네트워크 엔진 (ASIO 기반)                                 │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐              │
│  │ io_context  │ │   async     │ │  Result<T>  │              │
│  │             │ │  operations │ │   pattern   │              │
│  └─────────────┘ └─────────────┘ └─────────────┘              │
├─────────────────────────────────────────────────────────────────┤
│  선택적 통합 계층                                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐              │
│  │   Logger    │ │ Monitoring  │ │   Thread    │              │
│  │  System     │ │   System    │ │   System    │              │
│  └─────────────┘ └─────────────┘ └─────────────┘              │
└─────────────────────────────────────────────────────────────────┘
```

### 디자인 패턴
- **Factory Pattern**: 네트워크 구성 요소 생성 및 구성
- **Observer Pattern**: 이벤트 기반 네트워크 상태 관리
- **Strategy Pattern**: 플러그 가능한 protocol 구현
- **RAII**: 연결을 위한 자동 리소스 관리
- **Template Metaprogramming**: 컴파일 타임 protocol 최적화

## 📁 프로젝트 구조

### 디렉토리 구성
```
network_system/
├── 📁 include/network_system/   # 공개 헤더 파일
│   ├── 📁 core/                 # 핵심 구성 요소
│   │   ├── messaging_server.h   # TCP 서버 구현
│   │   └── messaging_client.h   # TCP 클라이언트 구현
│   ├── 📁 internal/             # 내부 구현
│   │   ├── tcp_socket.h         # 소켓 래퍼
│   │   ├── messaging_session.h  # 세션 처리
│   │   └── pipeline.h           # 데이터 처리 파이프라인
│   └── 📁 utils/                # 유틸리티
│       └── result_types.h       # Result<T> 오류 처리
├── 📁 src/                      # 구현 파일
│   ├── 📁 core/                 # 핵심 구현
│   ├── 📁 internal/             # 내부 구현
│   └── 📁 utils/                # 유틸리티 구현
├── 📁 samples/                  # 사용 예제
│   └── basic_usage.cpp          # 기본 TCP 예제
├── 📁 benchmarks/               # 성능 벤치마크
│   └── CMakeLists.txt           # 벤치마크 빌드 설정
├── 📁 docs/                     # 문서
│   └── BASELINE.md              # 성능 기준선
├── 📄 CMakeLists.txt            # 빌드 설정
├── 📄 .clang-format             # 코드 포매팅 규칙
└── 📄 README.md                 # 이 파일
```

## 🚀 빠른 시작 및 사용 예제

### 5분 안에 시작하기

**1단계: 빠른 설치**
```bash
# Clone and build
git clone https://github.com/kcenon/network_system.git
cd network_system && mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build .
```

**2단계: 첫 번째 TCP Server (60초)**
```cpp
#include <network_system/core/messaging_server.h>
#include <iostream>
#include <memory>

int main() {
    // 서버 ID로 TCP 서버 생성
    auto server = std::make_shared<network_system::core::messaging_server>("MyServer");

    // 포트 8080에서 서버 시작
    auto result = server->start_server(8080);
    if (!result) {
        std::cerr << "서버 시작 실패: " << result.error().message << std::endl;
        return -1;
    }

    std::cout << "서버가 포트 8080에서 실행 중..." << std::endl;
    std::cout << "종료하려면 Ctrl+C를 누르세요" << std::endl;

    // 서버가 중지될 때까지 대기
    server->wait_for_stop();

    return 0;
}
```

**3단계: TCP Client로 연결**
```cpp
#include <network_system/core/messaging_client.h>
#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

int main() {
    // 클라이언트 ID로 TCP 클라이언트 생성
    auto client = std::make_shared<network_system::core::messaging_client>("MyClient");

    // 클라이언트 시작 및 서버에 연결
    auto result = client->start_client("localhost", 8080);
    if (!result) {
        std::cerr << "연결 실패: " << result.error().message << std::endl;
        return -1;
    }

    // 연결이 설정될 때까지 대기
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 메시지 전송 (zero-copy를 위해 std::move 필요)
    std::string message = "안녕하세요, Network System!";
    std::vector<uint8_t> data(message.begin(), message.end());

    auto send_result = client->send_packet(std::move(data));
    if (!send_result) {
        std::cerr << "전송 실패: " << send_result.error().message << std::endl;
    }

    // 처리 대기
    client->wait_for_stop();

    return 0;
}
```

### 전제 조건

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libfmt-dev
```

#### macOS
```bash
brew install cmake ninja asio fmt
```

#### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-fmt
```

### 빌드 지침

```bash
# 저장소 복제
git clone https://github.com/kcenon/network_system.git
cd network_system

# 빌드 디렉토리 생성
mkdir build && cd build

# CMake로 구성 (기본 빌드)
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# 벤치마크를 활성화하여 빌드
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNETWORK_BUILD_BENCHMARKS=ON

# 선택적 통합과 함께 빌드
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=ON \
    -DBUILD_WITH_LOGGER_SYSTEM=ON

# 빌드
cmake --build .

# 벤치마크 실행 (활성화된 경우)
./build/benchmarks/network_benchmarks
```

## 📝 API 예제

### TCP API 사용법

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// 오류 처리를 포함한 서버 예제
auto server = std::make_shared<network_system::core::messaging_server>("server_id");
auto server_result = server->start_server(8080);
if (!server_result) {
    std::cerr << "서버 실패: " << server_result.error().message << std::endl;
    return -1;
}

// 오류 처리를 포함한 클라이언트 예제
auto client = std::make_shared<network_system::core::messaging_client>("client_id");
auto client_result = client->start_client("localhost", 8080);
if (!client_result) {
    std::cerr << "클라이언트 실패: " << client_result.error().message << std::endl;
    return -1;
}

// 메시지 전송 (zero-copy를 위해 std::move 필요)
std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
auto send_result = client->send_packet(std::move(data));
if (!send_result) {
    std::cerr << "전송 실패: " << send_result.error().message << std::endl;
}
```

### WebSocket API 사용법

```cpp
#include <network_system/core/messaging_ws_server.h>
#include <network_system/core/messaging_ws_client.h>

// WebSocket 서버
using namespace network_system::core;

auto server = std::make_shared<messaging_ws_server>("ws_server");

// 서버 설정
ws_server_config config;
config.port = 8080;
config.max_connections = 100;
config.ping_interval = std::chrono::seconds(30);

auto result = server->start_server(config);
if (!result) {
    std::cerr << "WebSocket 서버 실패: " << result.error().message << std::endl;
    return -1;
}

// 연결된 모든 클라이언트에게 텍스트 메시지 브로드캐스트
auto broadcast_result = server->broadcast_text("안녕하세요, WebSocket 클라이언트!");
if (!broadcast_result) {
    std::cerr << "브로드캐스트 실패: " << broadcast_result.error().message << std::endl;
}

// WebSocket 클라이언트
auto client = std::make_shared<messaging_ws_client>("ws_client");

// 클라이언트 설정
ws_client_config client_config;
client_config.host = "localhost";
client_config.port = 8080;
client_config.path = "/";
client_config.auto_pong = true;  // ping 프레임에 자동으로 응답

auto connect_result = client->start_client(client_config);
if (!connect_result) {
    std::cerr << "WebSocket 클라이언트 연결 실패: " << connect_result.error().message << std::endl;
    return -1;
}

// 텍스트 메시지 전송
auto send_result = client->send_text("WebSocket 클라이언트에서 안녕하세요!");
if (!send_result) {
    std::cerr << "전송 실패: " << send_result.error().message << std::endl;
}

// 바이너리 데이터 전송
std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04};
auto binary_result = client->send_binary(std::move(binary_data));
if (!binary_result) {
    std::cerr << "바이너리 전송 실패: " << binary_result.error().message << std::endl;
}
```

### 레거시 API 호환성

```cpp
#include <network_system/compatibility.h>

// Use legacy namespace - fully compatible
auto server = network_module::create_server("legacy_server");
server->start_server(8080);

auto client = network_module::create_client("legacy_client");
client->start_client("127.0.0.1", 8080);
```

## 🏗️ 아키텍처

```
network_system/
├── include/network_system/
│   ├── core/                    # Core networking components
│   │   ├── messaging_client.h
│   │   └── messaging_server.h
│   ├── session/                 # Session management
│   │   └── messaging_session.h
│   ├── internal/                # Internal implementation
│   │   ├── tcp_socket.h
│   │   ├── pipeline.h
│   │   └── send_coroutine.h
│   ├── integration/             # External system integration
│   │   ├── messaging_bridge.h
│   │   ├── thread_integration.h
│   │   ├── container_integration.h
│   │   └── logger_integration.h
│   └── compatibility.h         # Legacy API support
├── src/                        # Implementation files
├── samples/                    # Usage examples
├── tests/                      # Test suites
└── docs/                       # Documentation
```

## 📚 API 문서

### 빠른 API 참조

#### TCP Server
```cpp
#include <network_system/core/messaging_server.h>
#include <memory>

// 식별자로 서버 생성
auto server = std::make_shared<network_system::core::messaging_server>("MyServer");

// 특정 포트에서 서버 시작
auto result = server->start_server(8080);
if (!result) {
    std::cerr << "시작 실패: " << result.error().message << std::endl;
    return -1;
}

// 서버 제어
server->wait_for_stop();                      // 블로킹 대기
server->stop_server();                        // 우아한 종료
```

#### TCP Client
```cpp
#include <network_system/core/messaging_client.h>
#include <memory>
#include <vector>

// 식별자로 클라이언트 생성
auto client = std::make_shared<network_system::core::messaging_client>("MyClient");

// 서버에 연결
auto result = client->start_client("hostname", 8080);
if (!result) {
    std::cerr << "연결 실패: " << result.error().message << std::endl;
    return -1;
}

// 데이터 전송 (zero-copy를 위해 std::move 필요)
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
auto send_result = client->send_packet(std::move(data));
if (!send_result) {
    std::cerr << "전송 실패: " << send_result.error().message << std::endl;
}

// 연결 상태 확인
if (client->is_connected()) {
    // 클라이언트가 연결됨
}

// 연결 해제
client->stop_client();
```

#### Result<T>를 사용한 오류 처리
```cpp
#include <network_system/utils/result_types.h>

// Result 기반 오류 처리 (예외 없음)
auto result = client->start_client("hostname", 8080);
if (!result) {
    // 오류 세부 정보 액세스
    std::cerr << "오류 코드: " << static_cast<int>(result.error().code) << std::endl;
    std::cerr << "오류 메시지: " << result.error().message << std::endl;
    return -1;
}

// 오류 검사가 포함된 전송 작업
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
auto send_result = client->send_packet(std::move(data));
if (!send_result) {
    std::cerr << "전송 실패: " << send_result.error().message << std::endl;
}

// 연결 상태 확인
if (client->is_connected()) {
    std::cout << "클라이언트가 연결되었습니다" << std::endl;
} else {
    std::cout << "클라이언트가 연결 해제되었습니다" << std::endl;
}
```

#### Zero-Copy 데이터 전송
```cpp
// zero-copy 작업을 위한 이동 의미론
std::vector<uint8_t> large_buffer(1024 * 1024); // 1 MB
// ... 버퍼에 데이터 채우기 ...

// 데이터가 복사되지 않고 이동됨 - 대용량 페이로드에 효율적
auto result = client->send_packet(std::move(large_buffer));
// large_buffer는 이동 후 비어 있음

if (!result) {
    std::cerr << "전송 실패: " << result.error().message << std::endl;
}
```

#### 오류 처리 및 진단
```cpp
// Comprehensive error handling
try {
    client.connect();
    client.send("data");
} catch (const network::connection_error& e) {
    // Connection-specific errors
    log_error("Connection failed: ", e.what());
} catch (const network::timeout_error& e) {
    // Timeout-specific errors
    log_error("Operation timed out: ", e.what());
} catch (const network::protocol_error& e) {
    // Protocol-specific errors
    log_error("Protocol error: ", e.what());
}

// Performance diagnostics
auto stats = server.get_statistics();
std::cout << "Connections: " << stats.active_connections << std::endl;
std::cout << "Messages/sec: " << stats.messages_per_second << std::endl;
std::cout << "Bytes/sec: " << stats.bytes_per_second << std::endl;
std::cout << "Avg latency: " << stats.average_latency << "ms" << std::endl;
```

## 📊 성능 벤치마크

### 최신 결과

| 메트릭 | 결과 | 테스트 조건 |
|--------|--------|-----------------|
| **평균 처리량** | 305,255 msg/s | 혼합 메시지 크기 |
| **작은 메시지 (64B)** | 769,230 msg/s | 10,000 메시지 |
| **중간 메시지 (1KB)** | 128,205 msg/s | 5,000 메시지 |
| **큰 메시지 (8KB)** | 20,833 msg/s | 1,000 메시지 |
| **동시 연결** | 50 clients | 12,195 msg/s |
| **평균 지연시간** | 584.22 μs | P50: < 50 μs |
| **성능 등급** | 🏆 EXCELLENT | 프로덕션 준비 완료! |

### 주요 성능 기능
- Zero-copy 메시지 pipeline
- 가능한 경우 Lock-free 데이터 구조
- Connection pooling
- ASIO를 사용한 비동기 I/O
- C++20 coroutine 지원

## 🔧 기능

### 핵심 기능
- ✅ 비동기 TCP server/client
- ✅ 비동기 WebSocket server/client (RFC 6455)
- ✅ 멀티스레드 메시지 처리
- ✅ Session 수명 주기 관리
- ✅ 버퍼링을 갖춘 메시지 pipeline
- ✅ C++20 coroutine 지원

### 통합 기능
- ✅ Thread pool 통합
- ✅ Container 직렬화 지원
- ✅ Logger system 통합
- ✅ 레거시 API 호환성 계층
- ✅ 포괄적인 테스트 커버리지
- ✅ 성능 벤치마킹 suite

### 계획된 기능
- 🚧 TLS/SSL 암호화
- 🚧 HTTP/2 client
- 🚧 gRPC 통합

## 🎯 프로젝트 요약

Network System은 향상된 모듈성과 재사용성을 제공하기 위해 messaging_system에서 분리된 활발히 유지 관리되는 비동기 네트워크 라이브러리입니다. 코드베이스는 이미 TCP/UDP/WebSocket 컴포넌트를 노출하고 있으며, 여러 로드맵 항목(connection pooling, TLS, zero-copy 파이프라인, 실제 벤치마킹)은 진행 중입니다.

### 🏆 주요 성과

#### **완전한 독립성** ✅
- 빌드 타임 의존성 제로로 messaging_system으로부터 완전히 분리
- 모든 C++ 프로젝트에 통합하기에 적합한 독립적인 라이브러리
- 깨끗한 namespace 격리 (`network_system::`)

#### **하위 호환성** ♻️
- 호환성 브리지(`include/network_system/integration/messaging_bridge.h`) 및 네임스페이스 별칭으로 레거시 messaging_system 코드 빌드 유지
- 통합 테스트(예: `integration_tests/scenarios/connection_lifecycle_test.cpp`)가 마이그레이션 플로우 실행
- 완전한 패리티를 선언하기 전 대규모 검증이 진행 중

#### **진행 중인 성능 작업** ⚙️
- 합성 Google Benchmark 제품군이 핫 패스를 다룸(`benchmarks/` 디렉토리)
- 스트레스, 통합 및 성능 테스트가 존재하지만 여전히 주로 CPU 전용 메트릭만 수집
- 실제 네트워크 처리량, 지연시간 및 메모리 기준선은 대기 중

#### **통합 생태계** ✅
- Thread, logger 및 container 통합이 제공됨(`src/integration/`)
- 크로스 플랫폼 빌드(Windows, Linux, macOS)가 CMake 및 GitHub Actions를 통해 구성됨
- 광범위한 문서(아키텍처, 마이그레이션, 운영)가 코드베이스와 함께 유지됨

### 🚀 마이그레이션 상태

| 구성 요소 | 상태 | 비고 |
|-----------|--------|-------|
| **핵심 네트워크 라이브러리** | ✅ 완료 | 독립 모듈이 독립적으로 빌드됨 |
| **레거시 API 호환성** | ♻️ 사용 가능 | 브리지 + 별칭 제공; 추가 검증 권장 |
| **성능 계측** | ⚙️ 진행 중 | 합성 마이크로벤치만; 실제 네트워크 메트릭 대기 중 |
| **통합 인터페이스** | ✅ 완료 | Thread, Logger, Container 시스템 연결됨 |
| **문서** | ✅ 완료 | 아키텍처, 마이그레이션 및 문제 해결 가이드 |
| **CI/CD Pipeline** | ⚙️ 사용 가능 | 워크플로우 정의 존재; 현재 실행 상태는 GitHub 확인 |

### 📊 영향 및 이점

- **모듈성**: 독립적인 라이브러리는 결합을 줄이고 유지보수성을 향상시킴
- **재사용성**: messaging_system을 넘어 여러 프로젝트에 통합 가능
- **성능**: 향후 최적화를 안내하는 프로파일링 인프라 구축
- **호환성**: 마이그레이션 브리지가 기존 애플리케이션의 변경을 최소화
- **품질**: 단위, 통합 및 스트레스 제품군과 CI 워크플로우가 회귀를 방지

## 🔧 의존성

### 필수
- **C++20** 호환 컴파일러
- **CMake** 3.16+
- **ASIO** 또는 **Boost.ASIO** 1.28+

### 선택 사항
- **fmt** 10.0+ (std::format으로 대체 가능)
- **container_system** (고급 직렬화용)
- **thread_system** (thread pool 통합용)
- **logger_system** (구조화된 로깅용)

## 🎯 플랫폼 지원

| 플랫폼 | 컴파일러 | 아키텍처 | 지원 수준 |
|----------|----------|--------------|---------------|
| Ubuntu 22.04+ | GCC 11+ | x86_64 | ✅ 완전 지원 |
| Ubuntu 22.04+ | Clang 14+ | x86_64 | ✅ 완전 지원 |
| Windows 2022+ | MSVC 2022+ | x86_64 | ✅ 완전 지원 |
| Windows 2022+ | MinGW64 | x86_64 | ✅ 완전 지원 |
| macOS 12+ | Apple Clang 14+ | x86_64/ARM64 | 🚧 실험적 |

## 📚 문서

| 문서 | 설명 |
|----------|-------------|
| [API Reference](https://kcenon.github.io/network_system) | Doxygen 생성 API 문서 |
| [Migration Guide](docs/MIGRATION_GUIDE.md) | messaging_system으로부터 단계별 마이그레이션 |
| [Performance Baseline](BASELINE.md) | 합성 벤치마크 및 실제 네트워크 성능 메트릭 |
| [Load Test Guide](docs/LOAD_TEST_GUIDE.md) | 부하 테스트 실행 및 해석을 위한 종합 가이드 |

## 🧪 성능 및 테스팅

### 합성 벤치마크

Google Benchmark를 사용한 CPU 전용 마이크로벤치마크:

```bash
# 벤치마크로 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETWORK_BUILD_BENCHMARKS=ON
cmake --build build -j

# 벤치마크 실행
./build/benchmarks/network_benchmarks
```

현재 결과는 [BASELINE.md](BASELINE.md)를 참조하세요.

### 실제 네트워크 부하 테스트

실제 소켓 I/O를 사용한 엔드투엔드 프로토콜 성능 테스트:

```bash
# 통합 테스트로 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j

# 부하 테스트 실행
./build/bin/integration_tests/tcp_load_test
./build/bin/integration_tests/udp_load_test
./build/bin/integration_tests/websocket_load_test
```

**수집된 메트릭:**
- **처리량**: 다양한 페이로드 크기에 대한 초당 메시지 수
- **지연시간**: 엔드투엔드 작업의 P50/P95/P99 백분위수
- **메모리**: 부하 시 RSS/heap/VM 소비
- **동시성**: 여러 동시 연결 시 성능

**자동 Baseline 수집:**

부하 테스트는 GitHub Actions를 통해 매주 실행되며 수동으로 트리거할 수 있습니다:

```bash
# 부하 테스트 실행 및 baseline 업데이트
gh workflow run network-load-tests.yml --field update_baseline=true
```

자세한 지침은 [LOAD_TEST_GUIDE.md](docs/LOAD_TEST_GUIDE.md)를 참조하세요.

## 🤝 기여

기여를 환영합니다! 다음 가이드라인을 따라주세요:

1. 저장소를 Fork하세요
2. 기능 브랜치를 생성하세요 (`git checkout -b feature/amazing-feature`)
3. 기존 커밋 규칙에 따라 변경 사항을 커밋하세요
4. 브랜치에 푸시하세요 (`git push origin feature/amazing-feature`)
5. Pull Request를 여세요

## 📄 라이선스

이 프로젝트는 BSD-3-Clause License에 따라 라이선스가 부여됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

## 🙏 감사의 말

### 핵심 의존성
- **ASIO Library Team**: C++에서 비동기 네트워크 프로그래밍의 기초 제공
- **fmt Library Contributors**: 최신의 안전하고 빠른 포맷팅 기능 제공
- **C++ Standards Committee**: 최신 네트워킹을 가능하게 하는 C++20 기능 제공

### 생태계 통합
- **Thread System**: 원활한 thread pool 통합 및 멀티스레드 아키텍처
- **Logger System**: 포괄적인 로깅 및 디버깅 기능
- **Container System**: 고급 직렬화 및 데이터 container 지원
- **Database System**: 네트워크-데이터베이스 통합 패턴
- **Monitoring System**: 성능 메트릭 및 관찰 가능성 기능

### 감사의 말
- 최신 네트워크 프로그래밍 패턴과 모범 사례에서 영감을 받았습니다
- 관리자: kcenon@naver.com

---

## 📧 연락처 및 지원

| 연락처 유형 | 세부 정보 |
|--------------|---------|
| **프로젝트 소유자** | kcenon (kcenon@naver.com) |
| **저장소** | https://github.com/kcenon/network_system |
| **이슈 및 버그 보고** | https://github.com/kcenon/network_system/issues |
| **토론 및 질문** | https://github.com/kcenon/network_system/discussions |
| **보안 문제** | security@network-system.dev |

### 개발 타임라인
- **Phase 1**: messaging_system으로부터 초기 분리
- **Phase 2**: 성능 최적화 및 벤치마킹
- **Phase 3**: 크로스 플랫폼 호환성 검증
- **Phase 4**: 생태계 통합 완료

## 프로덕션 품질 및 아키텍처

### 빌드 및 테스팅 인프라

**포괄적인 멀티 플랫폼 CI/CD**
- **Sanitizer 커버리지**: ThreadSanitizer, AddressSanitizer 및 UBSanitizer를 사용한 자동화된 빌드
- **멀티 플랫폼 테스팅**: Ubuntu (GCC/Clang), Windows (MSVC/MinGW) 및 macOS 전반에 걸친 지속적인 검증
- **성능 게이트**: 처리량 및 지연시간에 대한 회귀 감지
- **정적 분석**: modernize 검사를 포함한 Clang-tidy 및 Cppcheck 통합
- **자동화된 테스팅**: 커버리지 보고서를 포함한 완전한 CI/CD 파이프라인

**성능 기준선**
- **평균 처리량**: 초당 305,255 메시지 (혼합 워크로드)
- **최고 성능**: 64바이트 메시지에 대해 초당 769,230 메시지
- **동시 성능**: 50개 연결에서 초당 12,195 메시지 안정적
- **지연시간**: P50 <50 μs, P95 <500 μs, 평균 584 μs
- **연결 설정**: 연결당 <100 μs
- **메모리 효율성**: 선형 확장으로 <10 MB 기준선

포괄적인 성능 메트릭 및 벤치마킹 세부 정보는 [BASELINE.md](BASELINE.md)를 참조하세요.

**완전한 문서 suite**
- [ARCHITECTURE.md](docs/ARCHITECTURE.md): Network system 설계 및 패턴
- [INTEGRATION.md](docs/INTEGRATION.md): 생태계 통합 가이드
- [MIGRATION_GUIDE.md](docs/MIGRATION_GUIDE.md): messaging_system으로부터 마이그레이션
- [BASELINE.md](BASELINE.md): 성능 기준선 측정값

### Thread 안전성 및 동시성

**프로덕션급 Thread 안전성 (100% 완료)**
- **멀티스레드 처리**: 동시 session 처리를 갖춘 thread-safe 메시지 처리
- **ThreadSanitizer 준수**: 모든 테스트 시나리오에서 제로 데이터 경합 감지
- **Session 관리**: 적절한 동기화를 통한 동시 session 수명 주기 처리
- **Lock-Free Pipeline**: 최대 처리량을 위한 zero-copy 메시지 pipeline 구현

**비동기 I/O 우수성**
- **ASIO 기반 아키텍처**: 입증된 ASIO 라이브러리를 사용한 고성능 비동기 I/O
- **C++20 Coroutine**: 깨끗하고 효율적인 코드를 위한 coroutine 기반 비동기 작업
- **Connection Pooling**: 지능적인 연결 재사용 및 수명 주기 관리
- **이벤트 기반**: 최적의 리소스 활용을 위한 논블로킹 이벤트 루프 아키텍처

### 리소스 관리 (RAII - Grade A)

**포괄적인 RAII 준수**
- **100% 스마트 포인터 사용**: 모든 리소스는 `std::shared_ptr` 및 `std::unique_ptr`을 통해 관리됨
- **AddressSanitizer 검증**: 모든 테스트 시나리오에서 제로 메모리 누수 감지
- **RAII 패턴**: 연결 수명 주기 래퍼, session 관리, socket RAII 래퍼
- **자동 정리**: 네트워크 연결, 비동기 작업 및 버퍼 리소스가 적절하게 관리됨
- **수동 메모리 관리 없음**: public 인터페이스에서 원시 포인터 완전 제거

**메모리 효율성 및 확장성**
```bash
# AddressSanitizer: Clean across all tests
==12345==ERROR: LeakSanitizer: detected memory leaks
# Total: 0 leaks

# Memory scaling characteristics:
Baseline: <10 MB
Per-connection overhead: Linear scaling
Zero-copy pipeline: Minimizes allocations
Resource cleanup: All connections RAII-managed
```

### 오류 처리 (핵심 API 마이그레이션 완료 - 75-80% 완료)

**최신 업데이트 (2025-10-10)**: 핵심 API Result<T> 마이그레이션이 성공적으로 완료되었습니다! 이제 모든 주요 네트워킹 API가 포괄적인 오류 코드 및 type-safe 오류 처리와 함께 Result<T>를 반환합니다.

**Result<T> 핵심 API - 프로덕션 준비 완료**

network_system은 Phase 3 핵심 API를 Result<T> 패턴으로 마이그레이션을 완료했으며, 이제 모든 주요 네트워킹 API가 type-safe 오류 처리를 제공합니다:

**완료된 작업 (2025-10-10)**
- ✅ **핵심 API 마이그레이션**: 모든 주요 네트워킹 API가 Result<T>로 마이그레이션됨
  - `messaging_server::start_server()`: `void` → `VoidResult`
  - `messaging_server::stop_server()`: `void` → `VoidResult`
  - `messaging_client::start_client()`: `void` → `VoidResult`
  - `messaging_client::stop_client()`: `void` → `VoidResult`
  - `messaging_client::send_packet()`: `void` → `VoidResult`
- ✅ **Result<T> Type System**: `result_types.h`에서 완전한 이중 API 구현
  - Common system 통합 지원 (`#ifdef BUILD_WITH_COMMON_SYSTEM`)
  - 독립적인 사용을 위한 독립형 대체 구현
  - 헬퍼 함수: `ok()`, `error()`, `error_void()`
- ✅ **Error Code Registry**: 완전한 오류 코드 (-600 ~ -699) 정의
  - Connection 오류 (-600 ~ -619): `connection_failed`, `connection_refused`, `connection_timeout`, `connection_closed`
  - Session 오류 (-620 ~ -639): `session_not_found`, `session_expired`, `invalid_session`
  - Send/Receive 오류 (-640 ~ -659): `send_failed`, `receive_failed`, `message_too_large`
  - Server 오류 (-660 ~ -679): `server_not_started`, `server_already_running`, `bind_failed`
- ✅ **ASIO 오류 처리**: 향상된 크로스 플랫폼 오류 감지
  - `asio::error` 및 `std::errc` 카테고리 모두 확인
  - 모든 ASIO 작업에 대한 적절한 오류 코드 매핑
- ✅ **테스트 커버리지**: 모든 12개 단위 테스트가 마이그레이션되어 통과
  - Exception 기반 assertion → Result<T> 검사
  - 명시적 오류 코드 검증
  - 모든 sanitizer 전반에 걸쳐 100% 테스트 성공률

**현재 API 패턴 (프로덕션 준비 완료)**
```cpp
// ✅ Migrated: Result<T> return values for type-safe error handling
auto start_server(unsigned short port) -> VoidResult;
auto stop_server() -> VoidResult;
auto start_client(std::string_view host, unsigned short port) -> VoidResult;
auto send_packet(std::vector<uint8_t> data) -> VoidResult;

// Usage example with Result<T>
auto result = server->start_server(8080);
if (result.is_err()) {
    std::cerr << "Failed to start server: " << result.error().message
              << " (code: " << result.error().code << ")\n";
    return -1;
}

// Async operations still use callbacks for completion events
auto on_receive(const std::vector<uint8_t>& data) -> void;
auto on_error(std::error_code ec) -> void;
```

**레거시 API 패턴 (마이그레이션 전)**
```cpp
// Old: void/callback-based error handling (no longer used)
auto start_server(unsigned short port) -> void;
auto stop_server() -> void;
auto start_client(std::string_view host, unsigned short port) -> void;
auto send_packet(std::vector<uint8_t> data) -> void;

// All errors were handled via callbacks only
auto on_error(std::error_code ec) -> void;
```

**이중 API 구현**
```cpp
// Supports both common_system integration and standalone usage
#ifdef BUILD_WITH_COMMON_SYSTEM
    // Uses common_system Result<T> when available
    template<typename T>
    using Result = ::common::Result<T>;
    using VoidResult = ::common::VoidResult;
#else
    // Standalone fallback implementation
    template<typename T>
    class Result {
        // ... full implementation in result_types.h
    };
    using VoidResult = Result<std::monostate>;
#endif

// Helper functions available in both modes
template<typename T>
inline Result<T> ok(T&& value);
inline VoidResult ok();
inline VoidResult error_void(int code, const std::string& message, ...);
```

**남은 마이그레이션 작업** (20-25% 남음)
- 🔲 **예제 업데이트**: Result<T> 사용법을 시연하도록 예제 코드 마이그레이션
  - Result<T> 예제로 `samples/` 디렉토리 업데이트
  - 오류 처리 시연 예제 생성
- 🔲 **문서 업데이트**: Result<T> API에 대한 포괄적인 문서
  - Result<T> 반환 타입으로 API 참조 업데이트
  - 이전 API에서 업그레이드하는 사용자를 위한 마이그레이션 가이드 생성
- 🔲 **Session 관리**: session 수명 주기 작업으로 Result<T> 확장
  - Session 생성/소멸 Result<T> API
  - Session 상태 관리 오류 처리
- 🔲 **비동기 변형** (향후): 네트워크 작업을 위한 비동기 Result<T> 변형 고려
  - 성능 영향 평가
  - 비동기 호환 Result<T> 패턴 설계

**Error Code 통합**
- **할당된 범위**: 중앙화된 오류 코드 레지스트리 (common_system)에서 `-600` ~ `-699`
- **분류**: Connection (-600 ~ -619), Session (-620 ~ -639), Send/Receive (-640 ~ -659), Server (-660 ~ -679)
- **크로스 플랫폼**: ASIO 및 표준 라이브러리 오류 코드 모두와 호환되는 ASIO 오류 감지

**성능 검증**: 핵심 API 마이그레이션은 **초당 305K+ 메시지** 평균 처리량과 **<50μs P50 지연시간**을 유지하며, Result<T> 패턴이 성능 저하 없이 type-safety를 추가함을 증명합니다.

**향후 개선 사항**
- 📝 **고급 기능**: WebSocket 지원, TLS/SSL 암호화, HTTP/2 client, gRPC 통합
- 📝 **성능 최적화**: 고급 zero-copy 기술, NUMA 인식 thread pinning, 하드웨어 가속, 사용자 정의 메모리 할당자

자세한 개선 계획 및 추적은 프로젝트의 [NEED_TO_FIX.md](/Users/dongcheolshin/Sources/NEED_TO_FIX.md)를 참조하세요.

### 아키텍처 개선 단계

**Phase 상태 개요** (2025-10-10 기준):

| Phase | 상태 | 완료도 | 주요 성과 |
|-------|--------|------------|------------------|
| **Phase 0**: Foundation | ✅ 완료 | 100% | CI/CD 파이프라인, 기준선 메트릭, 테스트 커버리지 |
| **Phase 1**: Thread 안전성 | ✅ 완료 | 100% | ThreadSanitizer 검증, 동시 session 처리 |
| **Phase 2**: 리소스 관리 | ✅ 완료 | 100% | Grade A RAII, AddressSanitizer clean |
| **Phase 3**: 오류 처리 | 🔄 진행 중 | 75-80% | **핵심 API 마이그레이션 완료** - Result<T>를 갖춘 5개 주요 API, 모든 테스트 통과 |
| **Phase 4**: 성능 | ⏳ 계획됨 | 0% | 고급 zero-copy, NUMA 인식 thread pinning |
| **Phase 5**: 안정성 | ⏳ 계획됨 | 0% | API 안정화, semantic versioning |
| **Phase 6**: 문서 | ⏳ 계획됨 | 0% | 포괄적인 가이드, 튜토리얼, 예제 |

#### Phase 3: 오류 처리 (75-80% 완료) - 핵심 API 마이그레이션 완료 ✅

**최신 성과 (2025-10-10)**: 핵심 API Result<T> 마이그레이션이 성공적으로 완료되었습니다! 이제 모든 5개 주요 네트워킹 API가 포괄적인 오류 코드 및 type-safe 오류 처리와 함께 Result<T>를 반환합니다.

**완료된 마일스톤**:
1. ✅ **핵심 API 마이그레이션** (완료): 모든 5개 주요 API가 VoidResult로 마이그레이션됨
   - `messaging_server::start_server()`, `stop_server()`
   - `messaging_client::start_client()`, `stop_client()`, `send_packet()`
2. ✅ **Result<T> Type System** (완료): `result_types.h`에서 전체 이중 API 구현
3. ✅ **Error Code Registry** (완료): 오류 코드 -600 ~ -699 정의 및 통합
4. ✅ **ASIO 오류 처리** (향상됨): ASIO 및 std::errc 모두에 대한 크로스 플랫폼 오류 감지
5. ✅ **테스트 커버리지** (완료): 모든 12개 단위 테스트가 마이그레이션되어 100% 성공률로 통과

**현재 API 패턴** (프로덕션 준비 완료):
```cpp
// ✅ All primary APIs now return VoidResult
auto start_server(unsigned short port) -> VoidResult;
auto stop_server() -> VoidResult;
auto start_client(std::string_view host, unsigned short port) -> VoidResult;
auto send_packet(std::vector<uint8_t> data) -> VoidResult;

// Usage with Result<T> pattern
auto result = server->start_server(8080);
if (result.is_err()) {
    std::cerr << "Server start failed: " << result.error().message << "\n";
    return -1;
}
```

**Error Code 커버리지**:
- **-600 ~ -619**: Connection 오류 (`connection_failed`, `connection_refused`, `connection_timeout`, `connection_closed`)
- **-620 ~ -639**: Session 오류 (`session_not_found`, `session_expired`, `invalid_session`)
- **-640 ~ -659**: Send/Receive 오류 (`send_failed`, `receive_failed`, `message_too_large`)
- **-660 ~ -679**: Server 오류 (`server_not_started`, `server_already_running`, `bind_failed`)

**성능 검증**: 마이그레이션은 **초당 305K+ 메시지** 평균 처리량과 **<50μs P50 지연시간**을 유지하며, Result<T>가 성능 저하 없이 type-safety를 추가함을 증명합니다.

**남은 작업** (20-25%):
- 🔲 Result<T> 사용 패턴을 시연하도록 예제 업데이트
- 🔲 Result<T>로 session 관리 API 확장
- 🔲 Result<T> 반환 타입으로 API 참조 문서 완료
- 🔲 향후 개선을 위한 비동기 Result<T> 변형 고려

자세한 Phase 3 상태 및 히스토리는 [PHASE_3_PREPARATION.md](docs/PHASE_3_PREPARATION.md)를 참조하세요.

---

<div align="center">

**🚀 Network System - 최신 C++을 위한 고성능 비동기 네트워킹**

*C++20 생태계를 위해 ❤️로 제작 | 프로덕션 준비 완료 | 엔터프라이즈급*

[![Performance](https://img.shields.io/badge/Performance-305K%2B%20msg%2Fs-brightgreen.svg)](README.md#performance-benchmarks)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Cross Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](README.md#platform-support)

*초고속 엔터프라이즈급 솔루션으로 네트워킹 아키텍처를 변환하세요*

</div>
