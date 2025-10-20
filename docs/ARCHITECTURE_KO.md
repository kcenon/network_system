아키텍처 개요
=====================

> **Language:** [English](ARCHITECTURE.md) | **한국어**

목적
- network_system은 재사용과 모듈성을 위해 messaging_system에서 추출된 비동기 네트워킹 라이브러리(ASIO 기반)입니다.
- 연결/세션 관리, 제로 카피 파이프라인 및 코루틴 친화적 API를 제공합니다.

계층
- Core: messaging_client/server, 연결/세션 관리
- Internal: tcp_socket, pipeline, send_coroutine, common_defs
- Integration: thread_integration (스레드 풀 추상화), logger_integration (로깅 추상화)

통합 플래그
- `BUILD_WITH_THREAD_SYSTEM`: 어댑터를 통한 외부 스레드 풀 사용
- `BUILD_WITH_LOGGER_SYSTEM`: logger_system 어댑터 사용 (폴백: basic_logger)
- `BUILD_WITH_CONTAINER_SYSTEM`: 직렬화를 위한 container_system 어댑터 활성화

통합 토폴로지
```
thread_system (선택) ──► network_system ◄── logger_system (선택)
             스레드 제공        로깅 제공

container_system ──► 직렬화 ◄── messaging_system/database_system
```

스레딩
- 애플리케이션이 thread_system을 주입하거나 내장 basic_thread_pool을 사용할 수 있도록 플러그 가능한 thread_pool_interface.

로깅
- logger_integration은 최소한의 logger_interface 및 어댑터를 정의합니다. logger_system이 없는 경우 기본 콘솔 로거가 제공됩니다.

성능
- 파이프라인에서 제로 카피 버퍼; 연결 풀링; 코루틴 send/recv 헬퍼.

빌드
- C++20, CMake. 선택적 기능 플래그: BUILD_WITH_LOGGER_SYSTEM, USE_THREAD_SYSTEM.

---

*Last Updated: 2025-10-20*
