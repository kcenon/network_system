---
doc_id: "NET-PROJ-003"
doc_title: "Network System 프로젝트 구조"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "PROJ"
---

# Network System 프로젝트 구조

> **SSOT**: This document is the single source of truth for **Network System 프로젝트 구조**.

**언어:** [English](PROJECT_STRUCTURE.md) | **한국어**

**최종 업데이트**: 2025-11-28

이 문서는 프로젝트 디렉토리 구조, 파일 구성 및 모듈 설명에 대한 포괄적인 가이드를 제공합니다.

---

## 목차

- [디렉토리 개요](#디렉토리-개요)
- [libs/ vs src/ 경계](#libs-vs-src-경계)
- [의사 결정 트리: 새 코드는 어디에 두는가?](#의사-결정-트리-새-코드는-어디에-두는가)
- [코어 컴포넌트](#코어-컴포넌트)
- [소스 구성](#소스-구성)
- [테스트 구성](#테스트-구성)
- [문서 구조](#문서-구조)
- [빌드 아티팩트](#빌드-아티팩트)
- [모듈 의존성](#모듈-의존성)

---

## 디렉토리 개요

```
network_system/
├── 📁 include/network_system/     # 공개 헤더 파일 (API)
├── 📁 src/                        # 구현 파일
├── 📁 examples/                   # 사용 예제
├── 📁 tests/                      # 테스트 스위트
├── 📁 benchmarks/                 # 성능 벤치마크
├── 📁 integration_tests/          # 통합 테스트 프레임워크
├── 📁 docs/                       # 문서
├── 📁 cmake/                      # CMake 모듈
├── 📁 .github/workflows/          # CI/CD 워크플로우
├── 📄 CMakeLists.txt              # 루트 빌드 설정
├── 📄 .clang-format               # 코드 포매팅 규칙
├── 📄 .gitignore                  # Git ignore 규칙
├── 📄 LICENSE                     # BSD 3-Clause 라이선스
├── 📄 README.md                   # 메인 문서
├── 📄 README.kr.md                # 한국어 문서
└── 📄 BASELINE.md                 # 성능 기준선
```

---

## libs/ vs src/ 경계

`network_system`은 프로토콜 구현을 **두 개의 병렬 위치**에 배포합니다.
이 분할은 의도적이며, 아래 규칙이 단일 진실 공급원 (single source of truth) 입니다:

| 위치 | 타겟 이름 | 목적 | 안정성 | 사용처 |
|------|-----------|------|--------|--------|
| `libs/network-*/` | `kcenon::network::tcp`, `kcenon::network::udp`, `kcenon::network::websocket`, `kcenon::network::http2`, `kcenon::network::quic`, `kcenon::network::grpc`, `kcenon::network::core`, `kcenon::network::all` | 공개, 모듈식, ABI 안정 프로토콜 라이브러리. 각 모듈은 자체 `CMakeLists.txt` 와 `include/network_<protocol>/` 하위의 공개 헤더를 가지며, `find_package` 를 통해 독립적으로 사용 가능합니다. | 공개 ABI. 호환성을 깨는 변경은 semver 를 따릅니다. | 다운스트림 프로젝트는 필요한 프로토콜 (`kcenon::network::tcp` 등) 을 직접 링크합니다. |
| `src/` | `network_system::network_system` (umbrella) | umbrella 타겟의 내부 구현. 프로토콜 entry-point 팩토리 (`src/protocol/`), 무거운 프로토콜 구현 (`src/protocols/`), 파사드 레이어 (`src/facade/`), 어댑터, 세션, 통합, 트레이싱, 내부 유틸리티가 여기에 위치합니다. | 내부. 심볼은 필요에 따라 재구성되며, API 보장은 없습니다. | umbrella `network_system::network_system` 타겟 — 이 저장소 내부의 샘플, 테스트, 벤치마크에서 사용되며, 모든 프로토콜을 하나의 라이브러리로 사용하려는 소비자도 사용합니다. |

**핵심 규칙:**

- 단일 프로토콜에 대한 **독립적이고 의존성이 최소화된 라이브러리** 가 필요한가? → `libs/network-<protocol>/` 아래에 추가하고, `include/network_<protocol>/` 에 헤더를 노출시킵니다.
- umbrella 빌드 뒤에서 **여러 조각을 연결** (팩토리, 파사드, 어댑터, 세션, 트레이싱, 통합) 해야 하는가? → `src/` 아래에 추가합니다.

> 마이그레이션 이력: TCP 와 UDP 는 `libs/network-tcp/` 와 `libs/network-udp/` 로의 마이그레이션을 완료했습니다.
> WebSocket, QUIC, HTTP/2, gRPC 는 현재 umbrella `src/` 트리 (`src/protocol/` 와 `src/protocols/` 경로) 와
> 모듈식 `libs/network-*/` 패키지에 **모두** 존재합니다.
> umbrella 트리는 `libs/` 동등성이 완료될 때까지 하위 호환성을 위해 컴파일 가능 상태로 유지됩니다.

### `src/protocol/` vs `src/protocols/`

이 두 디렉토리는 첫눈에 동일해 보이지만 서로 다른 역할을 수행합니다.
둘 다 현재 활성 상태이며, 루트 `CMakeLists.txt` 에서 컴파일됩니다:

| 디렉토리 | 레이아웃 | 목적 | 어떤 것이 들어가나 |
|----------|----------|------|---------------------|
| `src/protocol/` (단수) | 평탄한 `.cpp` 파일들: `tcp.cpp`, `udp.cpp`, `quic.cpp`, `websocket.cpp` | **얇은 프로토콜 entry point** — 통합 팩토리 API (`kcenon::network::protocol::<proto>::connect()`, `listen()`, `create_connection()`, `create_listener()`) 를 충족합니다. 각 파일은 작고 (~50-150 라인), `src/unified/adapters/` 의 어댑터로 위임합니다. | 프로토콜별 한 개의 파일로, `include/kcenon/network/detail/protocol/<proto>.h` 에 정의된 통합 팩토리 함수를 노출합니다. |
| `src/protocols/` (복수) | 프로토콜별 하위 디렉토리: `grpc/`, `http2/`, `quic/` | **완전한 프로토콜 구현** — 프레임 파서, 상태 머신, 혼잡 제어기, 패킷 보호, HPACK 등. 파일들은 크고 (보통 5-50 KB), 프로토콜 로직 자체를 포함합니다. | 자체 지원 소스 파일 서브트리가 필요한 프로토콜의 구현 세부 사항. |

요약하면: `src/protocol/` 은 *"어떻게 이 프로토콜의 연결을 생성하는가?"* 에 답하고,
`src/protocols/` 는 *"이 프로토콜이 와이어 위에서 실제로 어떻게 동작하는가?"* 에 답합니다.

> 2026-05 기준 두 디렉토리 모두 현재 사용 중이며, 둘 중 어느 것도 레거시가 아닙니다.
> 디렉토리명 변경은 이 문서의 범위 밖입니다. 단/복수 쌍이 걸림돌이 된다면 임시 수정 대신 후속 이슈를 등록하십시오.

---

## 의사 결정 트리: 새 코드는 어디에 두는가?

새 코드를 추가할 때 이 트리를 사용하십시오. 일치하는 첫 번째 분기를 선택합니다.

1. **완전히 새로운 프로토콜 구현** (예: MQTT, WebTransport) 인가?
   - 자체 `CMakeLists.txt`, `include/network_<protocol>/`, `src/` 를 갖춘 `libs/network-<protocol>/` 를 생성합니다.
   - umbrella 빌드에서도 노출이 필요하면, 모듈식 라이브러리로 위임하는 얇은 entry-point 파일을 `src/protocol/<protocol>.cpp` 아래에 추가합니다.

2. **기존 프로토콜의 와이어 레벨 구현** (프레임 파서, 혼잡 제어기, 코덱, 상태 머신) 추가인가?
   - 프로토콜이 모듈식 라이브러리 (`libs/network-<protocol>/`) 로 이미 마이그레이션된 경우, 그곳이 표준 위치입니다.
   - 그렇지 않으면, `src/protocols/<protocol>/` 의 동료 옆에 추가합니다.
   - `libs/` 와 `src/protocols/` 양쪽에 동일 소스 파일을 중복 배치하는 것을 피하십시오.
     일시적으로 둘 다 존재해야 한다면 미러링하고, 루트 `CMakeLists.txt` 에 마이그레이션 상태를 표시하는 주석을 추가합니다.

3. **기존 프로토콜에 대한 새로운 팩토리 entry point** (`connect()`, `listen()`, `create_connection()`) 추가인가?
   - `src/protocol/<protocol>.cpp` 와 그에 상응하는 `include/kcenon/network/detail/protocol/<protocol>.h` 헤더를 수정합니다.

4. **파사드 추가** (프로토콜 위의 고수준 편의 래퍼) 인가?
   - `src/facade/<protocol>_facade.cpp` 와 `include/kcenon/network/facade/<protocol>_facade.h`.

5. **어댑터 추가** (레거시 클래스를 통합 `i_connection` / `i_listener` 인터페이스에 연결) 인가?
   - `src/adapters/<thing>_adapter.cpp`.

6. **세션, 내부 헬퍼, 또는 통합** (thread/container/logger 브릿지) 추가인가?
   - 각각 `src/session/`, `src/internal/`, 또는 `src/integration/`.

7. **다운스트림 소비자가 include 해야 할 공개 헤더** 추가인가?
   - 최신 API: `include/kcenon/network/<area>/<header>.h` (권장).
   - 레거시 호환 API: `include/network_system/<area>/<header>.h` (레거시 표면을 확장하는 경우에만).

8. **테스트, 벤치마크, 또는 예제** 추가인가?
   - 해당 디렉토리 README 의 규칙에 따라 `tests/unit/`, `tests/integration/`, `benchmarks/`, 또는 `examples/`.

### 워크드 예제

- **"새로운 HTTP/3 프레임 타입 추가"** — `src/protocols/http3/frame.cpp` 를 수정합니다 (HTTP/3 이 새로 도입되는 경우 디렉토리를 생성). 모듈식 라이브러리가 존재하면 `libs/network-http3/` 에도 미러링합니다.
- **"TCP 용 `connect_with_retry()` 헬퍼 추가"** — `src/protocol/tcp.cpp` (entry-point 위임) 를 수정합니다. 헬퍼가 공개 ABI 를 가로지르면 구현은 `libs/network-tcp/src/` 에 위치하고, 그렇지 않으면 `src/` 에 머무를 수 있습니다.
- **"TCP 세션에 새 logger 콜백 연결"** — `src/integration/logger_integration.cpp` 와 `include/kcenon/network/integration/logger_integration.h`. `libs/network-tcp/` 아래에 추가하지 **마십시오**.

---

## 코어 컴포넌트

### include/network_system/ (공개 API)

**목적**: 라이브러리의 API를 구성하는 공개 헤더 파일

```
include/network_system/
├── 📁 core/                       # 코어 네트워킹 컴포넌트
│   ├── messaging_server.h         # TCP 서버 구현
│   ├── messaging_client.h         # TCP 클라이언트 구현
│   ├── messaging_udp_server.h     # UDP 서버 구현
│   ├── messaging_udp_client.h     # UDP 클라이언트 구현
│   ├── secure_messaging_server.h  # TLS/SSL 서버
│   ├── secure_messaging_client.h  # TLS/SSL 클라이언트
│   ├── messaging_ws_server.h      # WebSocket 서버
│   ├── messaging_ws_client.h      # WebSocket 클라이언트
│   ├── http_server.h              # HTTP/1.1 서버
│   └── http_client.h              # HTTP/1.1 클라이언트
│
├── 📁 session/                    # 세션 관리
│   ├── messaging_session.h        # TCP 세션 처리
│   ├── secure_session.h           # TLS 세션 처리
│   └── ws_session_manager.h       # WebSocket 세션 관리자
│
├── 📁 internal/                   # 내부 구현 세부사항
│   ├── tcp_socket.h               # TCP 소켓 래퍼
│   ├── udp_socket.h               # UDP 소켓 래퍼
│   ├── secure_tcp_socket.h        # SSL 스트림 래퍼
│   ├── websocket_socket.h         # WebSocket 프로토콜
│   └── websocket_protocol.h       # WebSocket 프레이밍/핸드셰이크
│
├── 📁 integration/                # 외부 시스템 통합
│   ├── messaging_bridge.h         # 레거시 messaging_system 브릿지
│   ├── thread_integration.h       # Thread 시스템 통합
│   ├── container_integration.h    # Container 시스템 통합
│   └── logger_integration.h       # Logger 시스템 통합
│
├── 📁 utils/                      # 유틸리티 컴포넌트
│   ├── result_types.h             # Result<T> 오류 처리
│   ├── network_config.h           # 설정 구조체
│   └── network_utils.h            # 헬퍼 함수
│
└── compatibility.h                # 레거시 API 호환성 레이어
```

#### 코어 컴포넌트 설명

**messaging_server.h**:
- TCP 서버 구현
- 비동기 연결 처리
- 세션 라이프사이클 관리
- 멀티 클라이언트 지원

**messaging_client.h**:
- TCP 클라이언트 구현
- 자동 재연결
- 연결 상태 모니터링
- 비동기 송신/수신

**secure_messaging_server.h / secure_messaging_client.h**:
- TLS/SSL 암호화 통신
- 인증서 관리
- TLS 1.2/1.3 프로토콜 지원
- 보안 핸드셰이크 처리

**messaging_ws_server.h / messaging_ws_client.h**:
- RFC 6455 WebSocket 프로토콜
- 텍스트 및 바이너리 메시지 프레이밍
- Ping/pong 킵얼라이브
- HTTP에서 연결 업그레이드

**http_server.h / http_client.h**:
- HTTP/1.1 프로토콜 지원
- 패턴 기반 라우팅
- 쿠키 및 헤더 관리
- Multipart/form-data 파일 업로드
- Chunked 인코딩
- 자동 압축 (gzip/deflate)

---

## 소스 구성

### src/ (구현)

```
src/
├── 📁 core/                       # 코어 컴포넌트 구현
│   ├── messaging_server.cpp
│   ├── messaging_client.cpp
│   ├── messaging_udp_server.cpp
│   ├── messaging_udp_client.cpp
│   ├── secure_messaging_server.cpp
│   ├── secure_messaging_client.cpp
│   ├── messaging_ws_server.cpp
│   ├── messaging_ws_client.cpp
│   ├── http_server.cpp
│   └── http_client.cpp
│
├── 📁 session/                    # 세션 관리 구현
│   ├── messaging_session.cpp
│   ├── secure_session.cpp
│   └── ws_session_manager.cpp
│
├── 📁 internal/                   # 내부 구현
│   ├── tcp_socket.cpp
│   ├── udp_socket.cpp
│   ├── secure_tcp_socket.cpp
│   ├── websocket_socket.cpp
│   └── websocket_protocol.cpp
│
├── 📁 integration/                # 통합 구현
│   ├── messaging_bridge.cpp
│   ├── thread_integration.cpp
│   ├── container_integration.cpp
│   └── logger_integration.cpp
│
└── 📁 utils/                      # 유틸리티 구현
    ├── result_types.cpp
    ├── network_config.cpp
    └── network_utils.cpp
```

**주요 구현 세부사항**:

- **messaging_server.cpp**: ~800 라인
  - 서버 초기화 및 라이프사이클
  - 오류 처리가 포함된 Accept 루프
  - 세션 생성 및 관리
  - 우아한 종료 로직

- **messaging_session.cpp**: ~600 라인
  - 세션 상태 머신
  - 비동기 읽기/쓰기 루프
  - 버퍼 관리
  - 오류 처리 및 정리

- **pipeline.cpp**: ~400 라인
  - 메시지 변환 파이프라인
  - 압축 지원
  - 암호화 훅
  - Move-aware 버퍼 처리

- **websocket_protocol.cpp**: ~1,000 라인
  - 프레임 파싱 및 직렬화
  - 마스킹/언마스킹 연산
  - 단편화 처리
  - 프로토콜 상태 머신

---

## 테스트 구성

### tests/ (유닛 테스트)

```
tests/
├── 📁 unit/                       # 유닛 테스트
│   ├── test_tcp_server.cpp        # TCP 서버 테스트
│   ├── test_tcp_client.cpp        # TCP 클라이언트 테스트
│   ├── test_session.cpp           # 세션 테스트
│   ├── test_pipeline.cpp          # 파이프라인 테스트
│   ├── test_websocket.cpp         # WebSocket 테스트
│   ├── test_http.cpp              # HTTP 테스트
│   └── test_result_types.cpp      # Result<T> 테스트
│
├── 📁 mocks/                      # Mock 객체
│   ├── mock_socket.h              # Mock 소켓
│   ├── mock_connection.h          # Mock 연결
│   └── mock_session.h             # Mock 세션
│
└── CMakeLists.txt                 # 테스트 빌드 설정
```

**테스트 커버리지**:
- 유닛 테스트: 12개 테스트 파일, 200+ 테스트 케이스
- 테스트 프레임워크: Google Test
- 커버리지: ~80% (gcov/lcov로 측정)

### integration_tests/ (통합 테스트)

```
integration_tests/
├── 📁 framework/                  # 테스트 프레임워크
│   ├── system_fixture.h           # 시스템 레벨 픽스처
│   ├── network_fixture.h          # 네트워크 테스트 픽스처
│   └── test_utils.h               # 테스트 유틸리티
│
├── 📁 scenarios/                  # 테스트 시나리오
│   ├── connection_lifecycle_test.cpp   # 연결 라이프사이클
│   ├── session_management_test.cpp     # 세션 관리
│   ├── protocol_compliance_test.cpp    # 프로토콜 준수
│   └── error_handling_test.cpp         # 오류 처리
│
├── 📁 performance/                # 성능 테스트
│   ├── tcp_load_test.cpp          # TCP 부하 테스트
│   ├── udp_load_test.cpp          # UDP 부하 테스트
│   ├── websocket_load_test.cpp    # WebSocket 부하 테스트
│   └── http_load_test.cpp         # HTTP 부하 테스트
│
└── CMakeLists.txt                 # 통합 테스트 빌드 설정
```

**통합 테스트 유형**:
- 연결 라이프사이클 테스트
- 프로토콜 준수 테스트
- 오류 처리 및 복구
- 부하 및 스트레스 테스트
- 멀티 클라이언트 시나리오

### benchmarks/ (성능 벤치마크)

```
benchmarks/
├── message_throughput_bench.cpp   # 메시지 할당 벤치마크
├── connection_bench.cpp           # 연결 벤치마크
├── session_bench.cpp              # 세션 벤치마크
├── protocol_bench.cpp             # 프로토콜 오버헤드 벤치마크
└── CMakeLists.txt                 # 벤치마크 빌드 설정
```

**벤치마크 스위트**:
- Google Benchmark 프레임워크
- 합성 CPU 전용 테스트
- 회귀 감지
- 성능 프로파일링

---

## 문서 구조

### docs/ (문서)

```
docs/
├── 📄 ARCHITECTURE.md             # 시스템 아키텍처 개요
├── 📄 API_REFERENCE.md            # 완전한 API 문서
├── 📄 FEATURES.md                 # 기능 설명
├── 📄 BENCHMARKS.md               # 성능 벤치마크
├── 📄 PROJECT_STRUCTURE.md        # 이 파일
├── 📄 PRODUCTION_QUALITY.md       # 프로덕션 품질 가이드
├── 📄 MIGRATION_GUIDE.md          # messaging_system에서 마이그레이션
├── 📄 INTEGRATION.md              # 다른 시스템과 통합
├── 📄 BUILD.md                    # 빌드 지침
├── 📄 OPERATIONS.md               # 운영 가이드
├── 📄 TLS_SETUP_GUIDE.md          # TLS/SSL 설정
├── 📄 TROUBLESHOOTING.md          # 문제 해결 가이드
├── 📄 LOAD_TEST_GUIDE.md          # 부하 테스트 가이드
├── 📄 CHANGELOG.md                # 버전 히스토리
│
├── 📁 diagrams/                   # 아키텍처 다이어그램
│   ├── architecture.svg
│   ├── protocol_stack.svg
│   └── data_flow.svg
│
└── 📁 adr/                        # 아키텍처 결정 기록
    ├── 001-async-io-model.md
    ├── 002-result-type-error-handling.md
    └── 003-websocket-protocol.md
```

**한국어 번역**: 각 주요 문서에는 `*.kr.md` 한국어 버전이 있습니다.

---

## 빌드 아티팩트

### build/ (생성됨)

```
build/                             # CMake 빌드 디렉토리 (gitignored)
├── 📁 bin/                        # 실행 바이너리
│   ├── simple_tcp_server
│   ├── simple_tcp_client
│   ├── simple_http_server
│   └── integration_tests/
│       ├── tcp_load_test
│       ├── udp_load_test
│       └── websocket_load_test
│
├── 📁 lib/                        # 라이브러리 출력
│   ├── libnetwork_system.a        # 정적 라이브러리
│   └── libnetwork_system.so       # 공유 라이브러리 (Linux)
│
├── 📁 benchmarks/                 # 벤치마크 실행 파일
│   └── network_benchmarks
│
├── 📁 tests/                      # 테스트 실행 파일
│   └── network_tests
│
└── 📁 CMakeFiles/                 # CMake 내부 파일
```

**빌드 모드**:
- Debug: 전체 심볼, 최적화 없음
- Release: 최적화 (-O3), 심볼 없음
- RelWithDebInfo: 심볼 포함 최적화
- MinSizeRel: 크기 최적화

---

## 모듈 의존성

### 내부 모듈 의존성

```
Core Components
  ├── messaging_server
  │   ├── 의존: tcp_socket, messaging_session, pipeline
  │   └── 선택적 사용: logger_integration, thread_integration
  │
  ├── messaging_client
  │   ├── 의존: tcp_socket, pipeline
  │   └── 선택적 사용: logger_integration, thread_integration
  │
  ├── secure_messaging_server
  │   ├── 의존: secure_tcp_socket, secure_session, pipeline
  │   └── 상속: messaging_server 패턴
  │
  └── messaging_ws_server
      ├── 의존: websocket_socket, websocket_protocol, ws_session_manager
      └── 선택적 사용: logger_integration

Session Management
  ├── messaging_session
  │   ├── 의존: tcp_socket, pipeline
  │   └── 관리: 연결 라이프사이클, 버퍼 관리
  │
  └── ws_session_manager
      ├── 의존: websocket_socket
      └── 관리: WebSocket 연결 라이프사이클

Internal Components
  ├── tcp_socket
  │   └── 래핑: ASIO 비동기 연산
  │
  ├── pipeline
  │   └── 의존: container_integration (선택적)
  │
  └── websocket_protocol
      └── 의존: tcp_socket
```

### 외부 의존성

**필수 의존성**:
```
network_system
  ├── C++20 컴파일러 (GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+)
  ├── CMake 3.20+
  ├── Standalone ASIO 1.30.2+ (Boost.ASIO 미지원)
  └── OpenSSL 3.0+ (TLS/SSL 및 WebSocket용)
```

**선택적 의존성**:
```
network_system (선택적)
  ├── fmt 10.0+ (포매팅, std::format으로 폴백)
  ├── container_system (고급 직렬화)
  ├── thread_system (스레드 풀 통합)
  └── logger_system (구조화된 로깅)
```

**빌드 의존성**:
```
Development
  ├── Google Test (테스팅 프레임워크)
  ├── Google Benchmark (성능 벤치마킹)
  ├── Doxygen (API 문서 생성)
  └── clang-format (코드 포매팅)
```

---

## CMake 구조

### 루트 CMakeLists.txt

**주요 섹션**:
1. 프로젝트 선언 및 버전
2. C++20 표준 요구사항
3. 의존성 관리 (find_package)
4. 선택적 기능 플래그
5. 서브디렉토리 포함
6. 타겟 내보내기 및 설치

**빌드 옵션**:
```cmake
option(NETWORK_BUILD_BENCHMARKS "벤치마크 빌드" OFF)
option(NETWORK_BUILD_TESTS "테스트 빌드" OFF)
option(BUILD_WITH_THREAD_SYSTEM "thread_system 통합 빌드" OFF)
option(BUILD_WITH_LOGGER_SYSTEM "logger_system 통합 빌드" OFF)
option(BUILD_WITH_CONTAINER_SYSTEM "container_system 통합 빌드" OFF)
option(BUILD_SHARED_LIBS "공유 라이브러리 빌드" OFF)
```

### cmake/ (CMake 모듈)

```
cmake/
├── FindAsio.cmake                 # ASIO 라이브러리 찾기
├── FindOpenSSL.cmake              # OpenSSL 찾기 (확장)
├── CodeCoverage.cmake             # 코드 커버리지 지원
└── CompilerWarnings.cmake         # 컴파일러 경고 설정
```

---

## 파일 명명 규칙

### 헤더 파일 (.h)

**패턴**: `snake_case.h`

**예시**:
- `messaging_server.h` - 공개 API 헤더
- `tcp_socket.h` - 내부 구현 헤더
- `result_types.h` - 유틸리티 헤더

### 소스 파일 (.cpp)

**패턴**: `snake_case.cpp` (헤더 이름과 일치)

**예시**:
- `messaging_server.cpp` - messaging_server.h 구현
- `tcp_socket.cpp` - tcp_socket.h 구현

### 테스트 파일

**패턴**: `test_<component>.cpp`

**예시**:
- `test_tcp_server.cpp`
- `test_session.cpp`
- `test_result_types.cpp`

### 벤치마크 파일

**패턴**: `<component>_bench.cpp`

**예시**:
- `message_throughput_bench.cpp`
- `connection_bench.cpp`
- `session_bench.cpp`

---

## 코드 구성 원칙

### 헤더 구성

**순서**:
1. 라이선스 헤더
2. 헤더 가드 (`#pragma once`)
3. 시스템 포함 (`<...>`)
4. 서드파티 포함 (`<asio/...>`, `<fmt/...>`)
5. 프로젝트 포함 (`"network_system/..."`)
6. 전방 선언
7. 네임스페이스 열기
8. 타입 선언
9. 클래스 정의
10. 네임스페이스 닫기
11. Doxygen 문서

**예시**:
```cpp
// BSD 3-Clause License
// ...

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <asio.hpp>

#include "network_system/utils/result_types.h"

namespace network_system::core {

// 전방 선언
class messaging_session;

/**
 * @brief TCP 메시징 서버
 * @details 비동기 TCP 서버 기능 제공
 */
class messaging_server {
    // ... 클래스 정의
};

}  // namespace network_system::core
```

---

## 참고 문서

- [ARCHITECTURE.md](ARCHITECTURE.md) / [ARCHITECTURE.kr.md](ARCHITECTURE.kr.md) - 시스템 아키텍처 세부사항
- [API_REFERENCE.md](API_REFERENCE.md) - 완전한 API 문서
- [BUILD.md](guides/BUILD.md) - 빌드 지침 및 설정
- [FEATURES.md](FEATURES.md) / [FEATURES.kr.md](FEATURES.kr.md) - 기능 설명
- [BENCHMARKS.md](BENCHMARKS.md) / [BENCHMARKS.kr.md](BENCHMARKS.kr.md) - 성능 벤치마크

---

**최종 업데이트**: 2025-11-28
**관리자**: kcenon@naver.com

---

Made with ❤️ by 🍀☀🌕🌥 🌊
