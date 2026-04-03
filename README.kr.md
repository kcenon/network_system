[![CI](https://github.com/kcenon/network_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/ci.yml)
[![Code Quality](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml)
[![Coverage](https://github.com/kcenon/network_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/coverage.yml)
[![codecov](https://codecov.io/gh/kcenon/network_system/branch/main/graph/badge.svg)](https://codecov.io/gh/kcenon/network_system)
[![Doxygen](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml)
[![License](https://img.shields.io/github/license/kcenon/network_system)](https://github.com/kcenon/network_system/blob/main/LICENSE)

# Network System

> **Language:** [English](README.md) | **한국어**

분산 시스템과 메시징 애플리케이션을 위한 재사용 가능한 전송 프리미티브를 제공하는 현대적인 C++20 비동기 네트워크 라이브러리입니다.

## 목차

- [개요](#개요)
- [주요 기능](#주요-기능)
- [요구사항](#요구사항)
- [빠른 시작](#빠른-시작)
- [설치](#설치)
- [아키텍처](#아키텍처)
- [핵심 개념](#핵심-개념)
- [API 개요](#api-개요)
- [예제](#예제)
- [성능](#성능)
- [생태계 통합](#생태계-통합)
- [기여하기](#기여하기)
- [라이선스](#라이선스)

---

## 개요

Network System은 코루틴 친화적 비동기 I/O, 플러그인 가능한 프로토콜 스택, TLS 1.2/1.3 지원을 갖춘 ASIO 기반 비동기 네트워크 라이브러리입니다. messaging_system에서 추출되어 향상된 모듈성과 생태계 전체 재사용성을 제공합니다.

**핵심 가치**:
- **모듈식 아키텍처**: 코루틴 친화적 비동기 I/O, 플러그인 가능한 프로토콜 스택
- **고성능**: ASIO 기반 논블로킹, 소형 페이로드 ~769K msg/s
- **프로덕션 등급**: TSAN/ASAN/UBSAN 클린, RAII Grade A, 다중 플랫폼 CI/CD
- **보안**: TLS 1.2/1.3, 인증서 검증, 최신 암호화 스위트
- **크로스 플랫폼**: Ubuntu, Windows, macOS (GCC, Clang, MSVC)

---

## 주요 기능

| 기능 | 설명 | 상태 |
|------|------|------|
| **TCP 프로토콜** | 연결 지향 신뢰성 전송 | 안정 |
| **UDP 프로토콜** | 비연결 데이터그램 전송 | 안정 |
| **WebSocket** | RFC 6455 전이중 통신 | 안정 |
| **HTTP/2** | 멀티플렉싱 HTTP 전송 | 안정 |
| **QUIC** | UDP 기반 고속 전송 | 실험적 |
| **gRPC** | 공식 gRPC 래퍼 | 실험적 |
| **TLS 지원** | TLS 1.2/1.3, 정책 기반 구성 | 안정 |
| **Facade API** | 프로토콜별 간소화 API | 안정 |
| **파이프라인** | 압축/암호화 메시지 변환 | 안정 |
| **C++20 모듈** | 헤더 기반 인터페이스 대안 | 실험적 |

---

## 요구사항

### 컴파일러 매트릭스

| 컴파일러 | 최소 버전 | 비고 |
|----------|----------|------|
| GCC | 13+ | thread_system 전이 의존성 |
| Clang | 17+ | thread_system 전이 의존성 |
| Apple Clang | 14+ | macOS 지원 |
| MSVC | 2022+ | C++20 기능 필수 |

### 빌드 도구 및 의존성

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| CMake | 3.20+ | 예 | 빌드 시스템 |
| ASIO | latest | 예 | 비동기 I/O (standalone) |
| OpenSSL | 3.x (권장) / 1.1.1 (최소) | 예 | TLS/SSL 지원 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | 공통 인터페이스 및 Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | latest | 예 | 스레드 풀 및 비동기 연산 |
| [logger_system](https://github.com/kcenon/logger_system) | latest | 예 | 로깅 인프라 |
| [container_system](https://github.com/kcenon/container_system) | latest | 예 | 데이터 컨테이너 연산 |

> **OpenSSL 버전 참고**: OpenSSL 1.1.1은 2023년 9월 11일 지원 종료되었습니다. OpenSSL 3.x로 업그레이드를 강력히 권장합니다.

---

## 빠른 시작

```cpp
#include <kcenon/network/facade/tcp_facade.h>

int main() {
    // TCP 서버 시작
    auto server_config = kcenon::network::tcp_server_config{
        .port = 8080,
        .max_connections = 100
    };
    auto server = kcenon::network::tcp_facade::listen(server_config);

    // TCP 클라이언트 연결
    auto client_config = kcenon::network::tcp_client_config{
        .host = "localhost",
        .port = 8080
    };
    auto client = kcenon::network::tcp_facade::connect(client_config);

    return 0;
}
```

---

## 설치

### 의존성과 함께 빌드

```bash
# 의존성 클론
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/container_system.git
git clone https://github.com/kcenon/network_system.git

cd network_system
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### CMake 프리셋

```bash
cmake --preset release
cmake --build build
```

### 의존성 흐름

```
network_system
+-- common_system (필수)
+-- thread_system (필수)
|   +-- common_system
+-- logger_system (필수)
|   +-- common_system
+-- container_system (필수)
    +-- common_system
```

---

## 아키텍처

### 모듈식 프로토콜 라이브러리 (v2.0)

```
libs/
  network-core      - 핵심 추상화 및 유틸리티
  network-tcp       - TCP 프로토콜 구현
  network-udp       - UDP 프로토콜 구현
  network-websocket - WebSocket 프로토콜
  network-http2     - HTTP/2 프로토콜
  network-quic      - QUIC 프로토콜
  network-grpc      - gRPC 프로토콜
  network-all       - 모든 프로토콜 통합
```

### 주요 컴포넌트

```
include/kcenon/network/
  facade/        - 프로토콜별 간소화 Facade (tcp, udp, websocket, http, quic)
  protocol/      - 프로토콜 팩토리 함수 및 태그 타입
  interfaces/    - 핵심 추상화 (i_protocol_client, i_protocol_server, i_session)
  unified/       - unified_messaging_client/server<Protocol, TlsPolicy> 템플릿
  policy/        - TLS 정책 (정책 기반 TLS 구성)
  concepts/      - C++20 concepts (Protocol concept)
```

---

## 핵심 개념

### Facade API

각 프로토콜은 설정 구조체와 인터페이스 포인터를 반환하는 Facade를 가집니다. 사용자에게 템플릿 파라미터가 노출되지 않습니다:

```cpp
// TCP Facade - 간단한 API
auto server = tcp_facade::listen(config);
auto client = tcp_facade::connect(config);

// 프로토콜 팩토리 - 더 많은 제어
auto server = protocol::tcp::listen(config);
auto client = protocol::tcp::connect(config);
```

### 프로토콜 모듈

C++20 Concepts로 프로토콜 태그를 제약하며, 컴파일 타임 메타데이터 (`name`, `is_connection_oriented`, `is_reliable`)를 제공합니다.

### 파이프라인 아키텍처

비동기 전송 전에 플러그인 가능한 압축/암호화 훅이 적용됩니다.

### 분산 추적

monitoring_system에 대한 컴파일 타임 의존성 없이 EventBus 기반 메트릭 발행을 통해 관측성을 제공합니다.

---

## API 개요

| API | 헤더 | 설명 |
|-----|------|------|
| `tcp_facade` | `facade/tcp_facade.h` | TCP 간소화 API |
| `udp_facade` | `facade/udp_facade.h` | UDP 간소화 API |
| `websocket_facade` | `facade/websocket_facade.h` | WebSocket 간소화 API |
| `http_facade` | `facade/http_facade.h` | HTTP 간소화 API |
| `quic_facade` | `facade/quic_facade.h` | QUIC 간소화 API |
| `i_protocol_client` | `interfaces/i_protocol_client.h` | 클라이언트 인터페이스 |
| `i_protocol_server` | `interfaces/i_protocol_server.h` | 서버 인터페이스 |

---

## 예제

| 예제 | 난이도 | 설명 |
|------|--------|------|
| tcp_echo | 초급 | TCP 에코 서버/클라이언트 |
| websocket_chat | 중급 | WebSocket 채팅 |
| tls_server | 중급 | TLS 보안 서버 |
| multi_protocol | 고급 | 다중 프로토콜 서버 |

---

## 성능

| 메트릭 | 값 | 비고 |
|--------|------|------|
| **처리량** | ~769K msg/s | 소형 페이로드 |
| **RAII 등급** | Grade A | 리소스 관리 |
| **커버리지** | ~80% | 코드 커버리지 |

### 품질 메트릭

- TSAN/ASAN/UBSAN 클린
- 다중 플랫폼 CI/CD (Ubuntu, macOS, Windows)
- 정적 분석, 퍼징, 벤치마크, 부하 테스트
- CVE 스캔, SBOM

---

## 생태계 통합

### 의존성 계층

```
common_system      (Tier 0) [필수]
thread_system      (Tier 1) [필수]
container_system   (Tier 1) [선택] -- 직렬화
logger_system      (Tier 2) [선택] -- ILogger 런타임 바인딩
monitoring_system  (Tier 3) [선택] -- EventBus 구독자
network_system     (Tier 4) <-- 이 프로젝트
```

### 통합 프로젝트

| 프로젝트 | network_system 역할 |
|----------|-------------------|
| [common_system](https://github.com/kcenon/common_system) | 필수 의존성 |
| [thread_system](https://github.com/kcenon/thread_system) | 필수 의존성 |
| [pacs_system](https://github.com/kcenon/pacs_system) | 네트워크 전송 제공 |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | 내보내기 전송 |

### 플랫폼 지원

| 플랫폼 | 컴파일러 | 상태 |
|--------|----------|------|
| **Linux** | GCC 13+, Clang 17+ | 완전 지원 |
| **macOS** | Apple Clang 14+ | 완전 지원 |
| **Windows** | MSVC 2022+ | 완전 지원 |

---

## 기여하기

기여를 환영합니다! 자세한 내용은 [기여 가이드](docs/contributing/CONTRIBUTING.md)를 참조하세요.

1. 리포지토리 포크
2. 기능 브랜치 생성
3. 테스트와 함께 변경 사항 작성
4. 로컬에서 테스트 실행
5. Pull Request 열기

---

## 라이선스

이 프로젝트는 BSD 3-Clause 라이선스에 따라 배포됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
