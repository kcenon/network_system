# Network System 문서

> **Language:** [English](README.md) | **한국어**

**버전:** 2.0
**최종 업데이트:** 2025-11-28
**상태:** 포괄적

network_system 문서에 오신 것을 환영합니다! 이것은 분산 시스템과 메시징 애플리케이션을 위한 재사용 가능한 전송 프리미티브를 제공하는 고성능 C++20 비동기 네트워크 라이브러리입니다.

---

## 빠른 탐색

| 목적 | 문서 |
|------|------|
| 빠르게 시작하기 | [빌드 가이드](guides/BUILD.kr.md) |
| 아키텍처 이해하기 | [아키텍처](ARCHITECTURE.kr.md) |
| API 학습하기 | [API 레퍼런스](API_REFERENCE.kr.md) |
| TLS/SSL 설정하기 | [TLS 설정 가이드](guides/TLS_SETUP_GUIDE.kr.md) |
| 문제 해결하기 | [트러블슈팅](guides/TROUBLESHOOTING.kr.md) |
| 성능 튜닝하기 | [성능 튜닝](advanced/PERFORMANCE_TUNING.md) |
| 다른 시스템과 통합하기 | [통합 가이드](INTEGRATION.kr.md) |

---

## 목차

- [문서 구조](#문서-구조)
- [역할별 문서](#역할별-문서)
- [기능별 문서](#기능별-문서)
- [문서 기여하기](#문서-기여하기)

---

## 문서 구조

### 핵심 문서

시스템 이해를 위한 필수 문서:

| 문서 | 설명 | 영문 |
|------|------|------|
| [ARCHITECTURE.kr.md](ARCHITECTURE.kr.md) | 시스템 설계, ASIO 통합, 프로토콜 레이어 | [EN](ARCHITECTURE.md) |
| [API_REFERENCE.kr.md](API_REFERENCE.kr.md) | 예제가 포함된 완전한 API 문서 | [EN](API_REFERENCE.md) |
| [FEATURES.kr.md](FEATURES.kr.md) | 상세 기능 설명 및 역량 | [EN](FEATURES.md) |
| [BENCHMARKS.kr.md](BENCHMARKS.kr.md) | 성능 지표 및 방법론 | [EN](BENCHMARKS.md) |
| [PROJECT_STRUCTURE.kr.md](PROJECT_STRUCTURE.kr.md) | 코드베이스 구성 및 디렉토리 레이아웃 | [EN](PROJECT_STRUCTURE.md) |
| [PRODUCTION_QUALITY.kr.md](PRODUCTION_QUALITY.kr.md) | 품질 지표 및 CI/CD 상태 | [EN](PRODUCTION_QUALITY.md) |
| [CHANGELOG.kr.md](CHANGELOG.kr.md) | 버전 이력 및 변경사항 | [EN](CHANGELOG.md) |

### 사용자 가이드

단계별 안내:

| 문서 | 설명 | 영문 |
|------|------|------|
| [guides/BUILD.kr.md](guides/BUILD.kr.md) | 모든 플랫폼용 빌드 지침 | [EN](guides/BUILD.md) |
| [guides/TLS_SETUP_GUIDE.kr.md](guides/TLS_SETUP_GUIDE.kr.md) | TLS/SSL 설정 및 인증서 | [EN](guides/TLS_SETUP_GUIDE.md) |
| [guides/TROUBLESHOOTING.kr.md](guides/TROUBLESHOOTING.kr.md) | 일반적인 문제 및 해결 방법 | [EN](guides/TROUBLESHOOTING.md) |
| [INTEGRATION.kr.md](INTEGRATION.kr.md) | 에코시스템 시스템과의 통합 | [EN](INTEGRATION.md) |
| [advanced/OPERATIONS.kr.md](advanced/OPERATIONS.kr.md) | 운영 지침 및 배포 | [EN](advanced/OPERATIONS.md) |
| [guides/LOAD_TEST_GUIDE.md](guides/LOAD_TEST_GUIDE.md) | 부하 테스트 방법론 및 도구 | - |

### 고급 주제

숙련된 사용자 및 성능 최적화:

| 문서 | 설명 |
|------|------|
| [advanced/PERFORMANCE_TUNING.md](advanced/PERFORMANCE_TUNING.md) | 성능 최적화 기법 |
| [advanced/CONNECTION_POOLING.md](advanced/CONNECTION_POOLING.md) | 연결 풀 아키텍처 및 사용법 |
| [advanced/UDP_RELIABILITY.md](advanced/UDP_RELIABILITY.md) | UDP 신뢰성 패턴 및 구현 |
| [advanced/HTTP_ADVANCED.md](advanced/HTTP_ADVANCED.md) | 고급 HTTP 기능 및 설정 |
| [advanced/MEMORY_PROFILING.md](advanced/MEMORY_PROFILING.md) | 메모리 분석 및 최적화 |
| [advanced/STATIC_ANALYSIS.md](advanced/STATIC_ANALYSIS.md) | 정적 분석 설정 및 결과 |

### 기여하기

| 문서 | 설명 |
|------|------|
| [contributing/CONTRIBUTING.md](contributing/CONTRIBUTING.md) | 기여 가이드라인 |

### 구현 상세

기술 구현 문서:

| 문서 | 설명 |
|------|------|
| [implementation/README.md](implementation/README.md) | 구현 개요 |
| [01-architecture-and-components.md](implementation/01-architecture-and-components.md) | 컴포넌트 아키텍처 |
| [02-dependency-and-testing.md](implementation/02-dependency-and-testing.md) | 의존성 및 테스트 전략 |
| [03-performance-and-resources.md](implementation/03-performance-and-resources.md) | 성능 특성 |
| [04-error-handling.md](implementation/04-error-handling.md) | 에러 처리 패턴 |

### 통합 가이드

시스템 통합 문서:

| 문서 | 설명 |
|------|------|
| [integration/README.md](integration/README.md) | 통합 개요 |
| [integration/with-common-system.md](integration/with-common-system.md) | Common system 통합 |
| [integration/with-logger.md](integration/with-logger.md) | Logger system 통합 |

---

## 역할별 문서

### 신규 사용자용

**시작하기 경로**:
1. **빌드** - [빌드 가이드](guides/BUILD.kr.md)로 라이브러리 컴파일
2. **아키텍처** - [아키텍처](ARCHITECTURE.kr.md)로 시스템 개요 파악
3. **API** - [API 레퍼런스](API_REFERENCE.kr.md)로 기본 사용법 학습
4. **예제** - `samples/` 디렉토리에서 동작하는 예제 확인

**문제 발생 시**:
- [트러블슈팅](guides/TROUBLESHOOTING.kr.md) 먼저 확인
- SSL 문제는 [TLS 설정 가이드](guides/TLS_SETUP_GUIDE.kr.md) 참조
- [GitHub Issues](https://github.com/kcenon/network_system/issues) 검색

### 숙련된 개발자용

**고급 사용 경로**:
1. **아키텍처** - [아키텍처](ARCHITECTURE.kr.md) 심층 분석
2. **성능** - [성능 튜닝](advanced/PERFORMANCE_TUNING.md) 학습
3. **프로토콜** - 프로토콜별 문서(HTTP, WebSocket, UDP) 학습
4. **통합** - [통합 가이드](INTEGRATION.kr.md) 검토

**심층 주제**:
- [연결 풀링](advanced/CONNECTION_POOLING.md) - 풀 아키텍처
- [UDP 신뢰성](advanced/UDP_RELIABILITY.md) - 신뢰성 있는 UDP 패턴
- [메모리 프로파일링](advanced/MEMORY_PROFILING.md) - 메모리 최적화

### 시스템 통합자용

**통합 경로**:
1. **통합 가이드** - [시스템 통합](INTEGRATION.kr.md)
2. **Common System** - [Common system 통합](integration/with-common-system.md)
3. **Logger** - [Logger 통합](integration/with-logger.md)
4. **운영** - [배포 가이드](advanced/OPERATIONS.kr.md)

---

## 기능별 문서

### TCP/UDP 네트워킹

| 주제 | 문서 |
|------|------|
| 아키텍처 | [아키텍처](ARCHITECTURE.kr.md) |
| API | [API 레퍼런스](API_REFERENCE.kr.md) |
| UDP 신뢰성 | [UDP 신뢰성](advanced/UDP_RELIABILITY.md) |

### TLS/SSL 보안

| 주제 | 문서 |
|------|------|
| 설정 | [TLS 설정 가이드](guides/TLS_SETUP_GUIDE.kr.md) |
| 구성 | [아키텍처](ARCHITECTURE.kr.md#tls-설정) |
| 트러블슈팅 | [트러블슈팅](guides/TROUBLESHOOTING.kr.md#tls-문제) |

### HTTP 프로토콜

| 주제 | 문서 |
|------|------|
| 기본 사용 | [API 레퍼런스](API_REFERENCE.kr.md#http) |
| 고급 | [HTTP 고급](advanced/HTTP_ADVANCED.md) |
| 성능 | [벤치마크](BENCHMARKS.kr.md#http-성능) |

### WebSocket 프로토콜

| 주제 | 문서 |
|------|------|
| 사용법 | [API 레퍼런스](API_REFERENCE.kr.md#websocket) |
| 기능 | [기능](FEATURES.kr.md#websocket) |
| 성능 | [벤치마크](BENCHMARKS.kr.md#websocket-성능) |

### 성능

| 주제 | 문서 |
|------|------|
| 벤치마크 | [벤치마크](BENCHMARKS.kr.md) |
| 튜닝 | [성능 튜닝](advanced/PERFORMANCE_TUNING.md) |
| 프로파일링 | [메모리 프로파일링](advanced/MEMORY_PROFILING.md) |
| 부하 테스트 | [부하 테스트 가이드](guides/LOAD_TEST_GUIDE.md) |

---

## 프로젝트 정보

### 현재 상태
- **버전**: 0.2.0
- **유형**: 정적 라이브러리
- **C++ 표준**: C++20 (일부 기능은 C++17 호환)
- **라이선스**: BSD 3-Clause
- **테스트 상태**: 개발 중

### 지원 프로토콜
- 생명주기 관리 및 상태 모니터링이 포함된 TCP
- 신뢰성 옵션이 포함된 UDP
- 현대적 암호 스위트를 갖춘 TLS 1.2/1.3
- 프레이밍 및 킵얼라이브가 포함된 WebSocket (RFC 6455)
- 라우팅, 쿠키, 멀티파트를 지원하는 HTTP/1.1
- HTTP/2 프레임 및 HPACK 구현
- DTLS 지원

### 주요 기능
- ASIO 기반 논블로킹 이벤트 루프
- 소형 페이로드 ~769K msg/sec
- 이동 의미론 친화적 API (제로 카피)
- C++20 코루틴 지원
- 메시지 변환을 위한 파이프라인 아키텍처
- 포괄적인 에러 처리 (Result<> 타입)

### 플랫폼 지원
- **Linux**: Ubuntu 22.04+, GCC 9+, Clang 10+
- **macOS**: 12+, Apple Clang 14+
- **Windows**: 11, MSVC 2019+

---

## 문서 기여하기

### 문서 표준
- 모든 문서에 프론트 매터 (버전, 날짜, 상태)
- 코드 예제는 컴파일 가능해야 함
- 이중 언어 지원 (영어/한국어)
- 상대 링크를 사용한 상호 참조

### 제출 절차
1. 마크다운 파일 편집
2. 모든 코드 예제 테스트
3. 해당 시 한국어 번역 업데이트
4. Pull Request 제출

---

## 도움 받기

### 문서 이슈
- **누락된 정보**: GitHub에 문서 이슈 열기
- **잘못된 예제**: 세부 사항과 함께 신고
- **불명확한 지침**: 개선 제안

### 기술 지원
1. [트러블슈팅](guides/TROUBLESHOOTING.kr.md) 확인
2. [GitHub Issues](https://github.com/kcenon/network_system/issues) 검색
3. GitHub Discussions에서 질문

---

## 외부 리소스

- **GitHub 저장소**: [kcenon/network_system](https://github.com/kcenon/network_system)
- **이슈 트래커**: [GitHub Issues](https://github.com/kcenon/network_system/issues)
- **메인 README**: [../README.md](../README.md)
- **에코시스템 개요**: [../../docs/ECOSYSTEM_OVERVIEW.kr.md](../../docs/ECOSYSTEM_OVERVIEW.kr.md)

---

**Network System 문서** - 고성능 C++20 비동기 네트워킹

**최종 업데이트**: 2025-11-28
