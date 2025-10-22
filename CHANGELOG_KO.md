# 변경 이력 - Network System

> **언어:** [English](CHANGELOG.md) | **한국어**

Network System 프로젝트의 모든 주요 변경 사항이 이 파일에 문서화됩니다.

형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.0.0/)를 따르며,
이 프로젝트는 [Semantic Versioning](https://semver.org/lang/ko/spec/v2.0.0.html)을 준수합니다.

---

## [미배포]

### 계획됨
- C++20 코루틴 완전 통합
- WebSocket 프로토콜 지원
- HTTP/2 및 HTTP/3 지원
- TLS 1.3 지원
- 고급 로드 밸런싱 알고리즘

---

## [1.0.0] - 2025-10-22

### 추가됨
- **핵심 Network System**: 프로덕션 준비 완료된 C++20 비동기 네트워크 라이브러리
  - ASIO 기반 논블로킹 I/O 작업
  - 최대 효율성을 위한 제로카피 파이프라인
  - 지능형 라이프사이클 관리를 통한 커넥션 풀링
  - 비동기 작업을 위한 C++20 코루틴 지원

- **고성능**: 초고속 처리량 네트워킹
  - 평균 305K+ 메시지/초 처리량
  - 작은 메시지(<1KB) 769K+ msg/s
  - 마이크로초 미만 지연시간
  - 제로카피 전송을 위한 직접 메모리 매핑

- **핵심 컴포넌트**:
  - **MessagingClient**: 커넥션 풀링을 갖춘 고성능 클라이언트
  - **MessagingServer**: 세션 관리를 갖춘 확장 가능한 서버
  - **MessagingSession**: 커넥션별 세션 처리
  - **Pipeline**: 제로카피 버퍼 관리 시스템

- **모듈형 아키텍처**:
  - messaging_system으로부터 독립 (100% 하위 호환성)
  - container_system과의 선택적 통합
  - thread_system과의 선택적 통합
  - logger_system과의 선택적 통합
  - common_system과의 선택적 통합

- **통합 지원**:
  - container_system: 바이너리 프로토콜을 통한 타입 안전 데이터 전송
  - thread_system: 동시 작업을 위한 스레드 풀 관리
  - logger_system: 네트워크 작업 로깅 및 진단
  - common_system: Result<T> 오류 처리 패턴
  - messaging_system: 메시징 전송 계층 (브리지 모드)

- **고급 기능**:
  - 지수 백오프를 통한 자동 재연결
  - 커넥션 타임아웃 관리
  - 우아한 종료 처리
  - 세션 라이프사이클 관리
  - 오류 복구 패턴

- **빌드 시스템**:
  - 모듈형 구성을 갖춘 CMake 3.16+
  - 유연한 빌드를 위한 13개 이상의 기능 플래그
  - 크로스 플랫폼 지원 (Windows, Linux, macOS)
  - 컴파일러 지원 (GCC 10+, Clang 10+, MSVC 19.29+)

### 변경됨
- 해당 없음 (최초 독립 릴리스)

### 사용 중단됨
- 해당 없음 (최초 독립 릴리스)

### 제거됨
- 해당 없음 (최초 독립 릴리스)

### 수정됨
- 해당 없음 (최초 독립 릴리스)

### 보안
- TLS 지원을 통한 안전한 커넥션 처리
- 모든 네트워크 작업에 대한 입력 유효성 검사
- 버퍼 오버플로우 보호
- DoS 방지를 위한 커넥션 제한

---

## [0.9.0] - 2025-09-15 (messaging_system으로부터 분리 이전)

### 추가됨
- messaging_system의 일부로서의 초기 네트워크 기능
- 기본 클라이언트/서버 통신
- 메시지 직렬화 및 역직렬화
- 커넥션 관리

### 변경됨
- messaging_system과의 긴밀한 결합
- 제한된 독립적 사용

---

## 프로젝트 정보

**저장소:** https://github.com/kcenon/network_system
**문서:** [docs/](docs/)
**라이선스:** LICENSE 파일 참조
**관리자:** kcenon@naver.com

---

## 버전 지원 매트릭스

| 버전  | 릴리스 날짜 | C++ 표준 | CMake 최소 버전 | 상태   |
|-------|-------------|----------|----------------|--------|
| 1.0.0 | 2025-10-22  | C++20    | 3.16           | 현재   |
| 0.9.0 | 2025-09-15  | C++20    | 3.16           | 레거시 (messaging_system의 일부) |

---

## 마이그레이션 가이드

messaging_system 통합 네트워크 모듈에서 마이그레이션하려면 [MIGRATION.md](MIGRATION.md)를 참조하십시오.

---

## 성능 벤치마크

**플랫폼**: 시스템에 따라 다름 (자세한 내용은 benchmarks/ 참조)

### 처리량 벤치마크
- 작은 메시지 (<1KB): 769K+ msg/s
- 중간 메시지 (1-10KB): 305K+ msg/s
- 큰 메시지 (>10KB): 150K+ msg/s

### 지연시간 벤치마크
- 로컬 커넥션: < 1 마이크로초
- 네트워크 커넥션: 네트워크 지연시간 + 처리 오버헤드에 따라 다름

### 확장성
- 동시 커넥션: 10,000개 이상의 동시 커넥션
- 8코어까지 선형 확장
- 커넥션 풀링으로 오버헤드 60% 감소

---

## 호환성을 깨는 변경사항

### 1.0.0
- **네임스페이스 변경**: `messaging::network` → `network_system`
- **헤더 경로**: `<messaging/network/*>` → `<network_system/*>`
- **CMake 타겟**: `messaging_system` → `network_system`
- **빌드 옵션**: 생태계 통합을 위한 새로운 `BUILD_WITH_*` 옵션

자세한 마이그레이션 지침은 [MIGRATION.md](MIGRATION.md)를 참조하십시오.

---

**최종 업데이트:** 2025-10-22
