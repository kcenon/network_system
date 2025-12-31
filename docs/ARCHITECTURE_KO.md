# 아키텍처 개요

> **Language:** [English](ARCHITECTURE.md) | **한국어**

## 목적

network_system은 재사용과 모듈성을 위해 messaging_system에서 추출된 비동기 네트워킹 라이브러리(ASIO 기반)입니다.
- 연결/세션 관리, 제로 카피 파이프라인 및 코루틴 친화적 API를 제공합니다.

## 계층

- **Core**: messaging_client/server, 연결/세션 관리
- **Internal**: tcp_socket, pipeline, send_coroutine, common_defs
- **Integration**: thread_integration (스레드 풀 추상화), logger_integration (로깅 추상화)

## CRTP 기본 클래스 계층

network_system은 Curiously Recurring Template Pattern (CRTP)을 사용하여 모든 메시징 클래스에 일관된 생명주기 관리를 제공하는 제로 오버헤드 기본 클래스를 제공합니다.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    CRTP 기본 클래스 계층                                 │
├─────────────────────────────────────────────────────────────────────────┤
│  TCP (연결 지향)                                                         │
│  messaging_client_base<Derived> → messaging_client, secure_messaging_*  │
│  messaging_server_base<Derived> → messaging_server, secure_messaging_*  │
│                                                                         │
│  UDP (비연결형)                                                          │
│  messaging_udp_client_base<Derived> → messaging_udp_client              │
│  messaging_udp_server_base<Derived> → messaging_udp_server              │
│                                                                         │
│  WebSocket (TCP 위 전이중 통신)                                          │
│  messaging_ws_client_base<Derived> → messaging_ws_client                │
│  messaging_ws_server_base<Derived> → messaging_ws_server                │
└─────────────────────────────────────────────────────────────────────────┘
```

| 기본 클래스 | 생명주기 | 프로토콜 특화 |
|------------|---------|--------------|
| `messaging_client_base` | start/stop/wait_for_stop | TCP 연결 상태 |
| `messaging_udp_client_base` | start/stop/wait_for_stop | 비연결형, 엔드포인트 인식 |
| `messaging_ws_client_base` | start/stop/wait_for_stop | WS 메시지 타입, close 코드 |

## 통합 플래그

| 플래그 | 설명 |
|-------|-----|
| `BUILD_WITH_THREAD_SYSTEM` | 어댑터를 통한 외부 스레드 풀 사용 |
| `BUILD_WITH_LOGGER_SYSTEM` | logger_system 어댑터 사용 (폴백: basic_logger) |
| `BUILD_WITH_CONTAINER_SYSTEM` | 직렬화를 위한 container_system 어댑터 활성화 |

## 통합 토폴로지

```
thread_system (선택) ──► network_system ◄── logger_system (선택)
             스레드 제공        로깅 제공

container_system ──► 직렬화 ◄── messaging_system/database_system
```

---

## Thread System 통합

Thread System Migration Epic (#271) 완료로, network_system의 모든 직접 `std::thread` 사용이 thread_system으로 마이그레이션되었습니다.

### 아키텍처 개요

```
┌─────────────────────────────────────────────────────────────┐
│                  Thread Integration Layer                    │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │           thread_integration_manager                 │   │
│  │  (싱글톤 - 중앙 스레드 풀 관리)                      │   │
│  └────────────────────┬────────────────────────────────┘   │
│                       │                                     │
│           ┌───────────┼───────────┐                        │
│           ▼           ▼           ▼                        │
│  ┌────────────┐ ┌──────────────┐ ┌────────────────────┐   │
│  │ basic_     │ │ thread_      │ │ (커스텀            │   │
│  │ thread_    │ │ system_pool_ │ │ 구현체)            │   │
│  │ pool       │ │ adapter      │ │                    │   │
│  └────────────┘ └──────────────┘ └────────────────────┘   │
│       │               │                                     │
│       ▼               ▼                                     │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         thread_system::thread_pool                   │   │
│  │  (BUILD_WITH_THREAD_SYSTEM 활성화 시)               │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 주요 컴포넌트

| 컴포넌트 | 설명 |
|---------|-----|
| `thread_pool_interface` | 스레드 풀 구현을 위한 추상 인터페이스 |
| `basic_thread_pool` | 기본 구현 (내부적으로 thread_system 사용) |
| `thread_system_pool_adapter` | thread_system::thread_pool 직접 어댑터 |
| `thread_integration_manager` | 글로벌 스레드 풀 관리 싱글톤 |

### 사용 예제

```cpp
#include <kcenon/network/integration/thread_integration.h>

// 통합 관리자 사용 (권장)
auto& manager = integration::thread_integration_manager::instance();

// 태스크 제출
manager.submit_task([]() {
    // 태스크 실행
});

// 지연 태스크 제출
manager.submit_delayed_task(
    []() { /* 지연 태스크 */ },
    std::chrono::milliseconds(1000)
);

// 메트릭 조회
auto metrics = manager.get_metrics();
std::cout << "워커 수: " << metrics.worker_threads << "\n";
std::cout << "대기 태스크: " << metrics.pending_tasks << "\n";
```

### 마이그레이션 이점

- **통합 스레드 관리**: 모든 네트워크 작업이 중앙화된 스레드 풀 사용
- **고급 큐 기능**: mutex와 lock-free 모드 간 자동 전환하는 adaptive_job_queue 접근
- **지연 태스크 지원**: 분리된 스레드 없이 스케줄러 기반 지연 태스크 실행
- **일관된 메트릭**: 통합 인프라를 통한 스레드 풀 메트릭 보고
- **자동 이점**: basic_thread_pool 사용 시 코드 변경 불필요 - 내부적으로 thread_system에 위임

---

## 로깅

logger_integration은 최소한의 logger_interface 및 어댑터를 정의합니다. logger_system이 없는 경우 기본 콘솔 로거가 제공됩니다.

---

## 성능

- 파이프라인에서 제로 카피 버퍼
- 연결 풀링
- 코루틴 send/recv 헬퍼

---

## 빌드

- C++20, CMake
- 선택적 기능 플래그: BUILD_WITH_LOGGER_SYSTEM, BUILD_WITH_THREAD_SYSTEM

---

*Last Updated: 2025-12-06*
