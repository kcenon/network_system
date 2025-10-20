# Changelog

> **Language:** [English](CHANGELOG.md) | **한국어**

Network System 프로젝트의 모든 주목할 만한 변경 사항은 이 파일에 문서화됩니다.

형식은 [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)를 기반으로 합니다.

## [Unreleased]

### 추가됨
- **Logger System 통합** (2025-09-20)
  - 구조화된 로깅을 위한 새로운 logger_integration 인터페이스
  - 사용 가능한 경우 외부 logger_system 지원
  - 독립 실행을 위한 기본 콘솔 로거로 폴백
  - 모든 std::cout/cerr를 NETWORK_LOG_* 매크로로 교체
  - BUILD_WITH_LOGGER_SYSTEM CMake 옵션 추가
  - 로그 레벨: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
  - 소스 위치 추적 (__FILE__, __LINE__, __FUNCTION__)
  - 밀리초 정밀도 타임스탬프 형식

### 구현 예정
- 네트워크 관리자 팩토리 클래스
- TCP 서버 및 클라이언트 고수준 인터페이스
- 응답 처리를 포함한 HTTP 클라이언트
- WebSocket 지원
- TLS/SSL 암호화
- 연결 풀링
- 마이그레이션 가이드 완성 (Phase 4)

## 2025-09-20 - Phase 4 진행 중

### 추가됨
- **성능 벤치마킹**
  - 포괄적인 성능 벤치마크 스위트 (benchmark.cpp)
  - 다양한 메시지 크기에 대한 처리량 테스트 (64B, 1KB, 8KB)
  - 동시 연결 벤치마크 (10개 및 50개 클라이언트)
  - 스레드 풀 성능 측정
  - 컨테이너 직렬화 벤치마크
  - 평균 305K+ msg/s 처리량 달성

- **호환성 테스트**
  - 향상된 호환성 테스트 스위트 (test_compatibility.cpp)
  - 레거시 네임스페이스 테스트 (network_module, messaging)
  - 타입 별칭 검증
  - 기능 감지 매크로
  - 레거시 및 최신 API 간 교차 호환성 테스트
  - 메시지 전달 검증
  - 레거시 API를 통한 컨테이너 및 스레드 풀 통합

- **사용 예제**
  - 레거시 호환성 예제 (legacy_compatibility.cpp)
  - 최신 API 사용 예제 (modern_usage.cpp)
  - 하위 호환성 데모
  - 고급 기능 쇼케이스

### 업데이트됨
- 성능 결과 및 Phase 4 상태가 포함된 README.md
- 문서 정리 및 간소화
- 벤치마크 타겟이 포함된 CMakeLists.txt

### 성능 결과
- 평균 처리량: 305,255 msg/s
- 작은 메시지 (64B): 769,230 msg/s
- 중간 메시지 (1KB): 128,205 msg/s
- 큰 메시지 (8KB): 20,833 msg/s
- 동시 연결: 50개 클라이언트 성공적으로 처리
- 성능 등급: EXCELLENT - 프로덕션 준비 완료!

## 2025-09-20 - Phase 3 완료

### 추가됨
- **Thread System 통합**
  - 스레드 풀 추상화를 가진 thread_integration.h/cpp
  - 비동기 작업 관리를 위한 스레드 풀 인터페이스
  - 기본 스레드 풀 구현
  - 스레드 통합 관리자 싱글톤
  - 지연된 작업 제출 지원
  - 작업 메트릭 및 모니터링

- **Container System 통합**
  - 직렬화를 위한 container_integration.h/cpp
  - 메시지 직렬화를 위한 컨테이너 인터페이스
  - 기본 컨테이너 구현
  - 등록 및 관리를 위한 컨테이너 관리자
  - 맞춤형 직렬화기/역직렬화기 지원

- **호환성 레이어**
  - 레거시 네임스페이스 별칭이 있는 compatibility.h
  - messaging_system과 완전한 하위 호환성
  - 레거시 API 지원 (network_module 네임스페이스)
  - 기능 감지 매크로
  - 시스템 초기화/종료 함수

- **통합 테스트**
  - 통합 검증을 위한 test_integration.cpp
  - 브리지 기능 테스트
  - 컨테이너 통합 테스트
  - 스레드 풀 통합 테스트
  - 성능 기준선 검증

### 수정됨
- thread_integration의 private 멤버 접근 문제
- 호환성 헤더 구조 문제
- 샘플 코드의 API 불일치
- macOS에서 CMake fmt 링크 문제

## 2025-09-19 - Phase 2 완료

### 추가됨
- **핵심 시스템 분리**
  - messaging_system으로부터 완전한 분리 검증
  - 적절한 디렉터리 구조 확인 (include/network_system)
  - 모든 모듈 적절히 조직화 (core, session, internal, integration)

- **통합 모듈**
  - messaging_bridge 구현 완료
  - 성능 메트릭 수집 기능
  - container_system 통합 인터페이스

- **빌드 검증**
  - container_system 통합으로 성공적인 빌드
  - verify_build 테스트 프로그램이 모든 검사 통과
  - CMakeLists.txt가 선택적 통합 지원

### 업데이트됨
- Phase 2 완료 상태가 포함된 MIGRATION_CHECKLIST.md
- 현재 프로젝트 단계 정보가 포함된 README.md

## 2025-09-19 - Phase 1 완료

### 추가됨
- **핵심 인프라**
  - messaging_system으로부터 완전한 분리
  - 새로운 네임스페이스 구조: `network_system::{core,session,internal,integration}`
  - C++20 코루틴을 사용한 ASIO 기반 비동기 네트워킹
  - 하위 호환성을 위한 메시징 브리지

- **빌드 시스템**
  - vcpkg 지원이 포함된 CMake 구성
  - 유연한 의존성 감지 (ASIO/Boost.ASIO)
  - 크로스 플랫폼 지원 (Linux, macOS, Windows)
  - 폴백으로 수동 vcpkg 설정

- **CI/CD 파이프라인**
  - Ubuntu용 GitHub Actions 워크플로우 (GCC/Clang)
  - Windows용 GitHub Actions 워크플로우 (Visual Studio/MSYS2)
  - Trivy를 사용한 의존성 보안 스캐닝
  - 라이선스 호환성 검사

- **Container 통합**
  - container_system과 완전한 통합
  - 메시징 브리지에서 값 컨테이너 지원
  - 가용성에 기반한 조건부 컴파일

- **문서화**
  - API 문서를 위한 Doxygen 구성
  - 빌드 지침이 포함된 포괄적인 README
  - 마이그레이션 및 구현 계획
  - 아키텍처 문서

### 수정됨
- **CI/CD 문제**
  - 문제가 있는 lukka/run-vcpkg 액션 제거
  - Windows에서 pthread.lib 링크 오류 수정
  - Windows의 ASIO를 위한 _WIN32_WINNT 정의 추가
  - CMake 옵션 수정 (USE_UNIT_TEST 대신 BUILD_TESTS)
  - 시스템 패키지 폴백으로 vcpkg를 선택사항으로 만듦

- **빌드 문제**
  - 네임스페이스 한정 오류 수정
  - 내부 헤더에 대한 include 경로 수정
  - 다양한 플랫폼에서 ASIO 감지 해결
  - vcpkg 기준선 구성 문제 해결

### 변경됨
- **의존성 관리**
  - 가능한 경우 vcpkg보다 시스템 패키지 선호
  - 간소화된 vcpkg.json 구성
  - 의존성 감지를 위한 여러 폴백 경로 추가
  - container_system 및 thread_system을 선택사항으로 만듦

### 보안
- 의존성 취약점 스캐닝 구현
- 라이선스 호환성 검증 추가
- 보안 이벤트 보고 구성

## 2025-09-18 - 초기 분리

### 추가됨
- messaging_system에서 분리된 초기 구현
- 기본 TCP 클라이언트/서버 기능
- 세션 관리
- 메시지 파이프라인 처리

---

## 개발 타임라인

| 날짜 | 마일스톤 | 설명 |
|------|-----------|-------------|
| 2025-09-19 | Phase 1 완료 | 인프라 및 분리 완료 |
| 2025-09-18 | 초기 분리 | messaging_system으로부터 분리 시작 |

## 마이그레이션 가이드

### messaging_system에서 network_system으로

#### 네임스페이스 변경
```cpp
// 이전 (messaging_system)
#include <messaging_system/network/tcp_server.h>
using namespace network_module;

// 새로운 (network_system)
#include <network_system/core/messaging_server.h>
using namespace network_system::core;
```

#### CMake 통합
```cmake
# 이전 (messaging_system)
# messaging_system의 일부

# 새로운 (network_system)
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_target NetworkSystem::NetworkSystem)
```

#### Container 통합
```cpp
// network_system의 새로운 기능
#include <network_system/integration/messaging_bridge.h>

auto bridge = std::make_unique<network_system::integration::messaging_bridge>();
bridge->set_container(container);
```

## 지원

문제, 질문 또는 기여에 대해서는 다음을 참조하세요:
- GitHub: https://github.com/kcenon/network_system
- Email: kcenon@naver.com
