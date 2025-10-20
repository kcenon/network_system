# Network System 분리 계획
# messaging_system에서 network_system 분리를 위한 상세 계획

> **Language:** [English](NETWORK_SYSTEM_SEPARATION_PLAN.md) | **한국어**

## 목차

- [📋 프로젝트 개요](#-프로젝트-개요)
  - [목표](#목표)
  - [범위](#범위)
- [🏗️ 현재 상태 분석](#-현재-상태-분석)
  - [messaging_system/network 구조](#messaging_systemnetwork-구조)
  - [기존 network_system 구조](#기존-network_system-구조)
  - [주요 의존성](#주요-의존성)
- [🎯 분리 전략](#-분리-전략)
  - [1. 기본 원칙](#1-기본-원칙)
  - [2. 네임스페이스 정책](#2-네임스페이스-정책)
  - [3. 호환성 레이어](#3-호환성-레이어)
- [📅 구현 로드맵](#-구현-로드맵)
  - [Phase 1: 준비 및 분석 (2-3일)](#phase-1-준비-및-분석-2-3일)
    - [Day 1](#day-1)
    - [Day 2](#day-2)
    - [Day 3](#day-3)
  - [Phase 2: 핵심 시스템 분리 (4-5일)](#phase-2-핵심-시스템-분리-4-5일)
    - [Day 4-5](#day-4-5)
    - [Day 6-7](#day-6-7)
    - [Day 8](#day-8)
  - [Phase 3: 통합 인터페이스 구현 (3-4일)](#phase-3-통합-인터페이스-구현-3-4일)
    - [Day 9-10](#day-9-10)
    - [Day 11-12](#day-11-12)
  - [Phase 4: messaging_system 업데이트 (2-3일)](#phase-4-messaging_system-업데이트-2-3일)
    - [Day 13-14](#day-13-14)
    - [Day 15](#day-15)
  - [Phase 5: 검증 및 배포 (2-3일)](#phase-5-검증-및-배포-2-3일)
    - [Day 16-17](#day-16-17)
    - [Day 18](#day-18)
- [🛠️ 기술 구현 세부사항](#-기술-구현-세부사항)
  - [1. 디렉터리 구조 재구성](#1-디렉터리-구조-재구성)
    - [목표 구조](#목표-구조)
  - [2. CMake 빌드 시스템](#2-cmake-빌드-시스템)
    - [Main CMakeLists.txt](#main-cmakeliststxt)
  - [3. 통합 인터페이스 설계](#3-통합-인터페이스-설계)
    - [messaging_bridge.h](#messaging_bridgeh)
  - [4. 의존성 관리](#4-의존성-관리)
    - [vcpkg.json](#vcpkgjson)
    - [dependency.sh](#dependencysh)
- [🧪 테스트 전략](#-테스트-전략)
  - [1. 단위 테스트](#1-단위-테스트)
  - [2. 통합 테스트](#2-통합-테스트)
  - [3. 성능 테스트](#3-성능-테스트)
  - [4. 스트레스 테스트](#4-스트레스-테스트)
- [📊 성능 요구사항](#-성능-요구사항)
  - [기준선 (분리 전)](#기준선-분리-전)
  - [목표 (분리 후)](#목표-분리-후)
- [🚨 리스크 요인 및 완화](#-리스크-요인-및-완화)
  - [1. 주요 리스크](#1-주요-리스크)
  - [2. 롤백 계획](#2-롤백-계획)
- [📝 문서화 계획](#-문서화-계획)
  - [1. API 문서](#1-api-문서)
  - [2. 아키텍처 문서](#2-아키텍처-문서)
  - [3. 운영 문서](#3-운영-문서)
- [✅ 완료 체크리스트](#-완료-체크리스트)
  - [Phase 1: 준비 및 분석](#phase-1-준비-및-분석)
  - [Phase 2: 핵심 시스템 분리](#phase-2-핵심-시스템-분리)
  - [Phase 3: 통합 인터페이스 구현](#phase-3-통합-인터페이스-구현)
  - [Phase 4: messaging_system 업데이트](#phase-4-messaging_system-업데이트)
  - [Phase 5: 검증 및 배포](#phase-5-검증-및-배포)
- [📞 연락처 및 지원](#-연락처-및-지원)

**날짜**: 2025-09-19
**버전**: 1.0.0
**담당자**: kcenon

---

## 📋 프로젝트 개요

### 목표
messaging_system에서 네트워크 모듈을 독립적인 network_system으로 분리하여 모듈성, 재사용성 및 유지보수성을 향상시킵니다.

### 범위
- `Sources/messaging_system/network/` 모듈의 완전한 분리
- 기존 `Sources/network_system/`와의 통합 또는 교체
- messaging_system과의 호환성 유지
- 다른 분리된 시스템과의 통합 (container_system, database_system, thread_system)

---

## 🏗️ 현재 상태 분석

### messaging_system/network 구조
```
messaging_system/network/
├── core/                               # 공개 API 레이어
│   ├── messaging_client.h/cpp             # TCP 클라이언트 구현
│   └── messaging_server.h/cpp             # TCP 서버 구현
├── session/                            # 세션 관리 레이어
│   └── messaging_session.h/cpp            # 연결 세션 처리
├── internal/                           # 내부 구현 레이어
│   ├── tcp_socket.h/cpp                   # TCP 소켓 래퍼
│   ├── send_coroutine.h/cpp               # 비동기 전송 코루틴
│   ├── pipeline.h/cpp                     # 메시지 파이프라인
│   └── common_defs.h                      # 공통 정의
├── network/                            # 통합 인터페이스
│   └── network.h                          # 메인 헤더
├── CMakeLists.txt                      # 빌드 구성
└── README.md                           # 모듈 문서
```

### 기존 network_system 구조
```
network_system/
├── core/                               # 기존 네트워크 코어
├── internal/                           # 기존 내부 구현
├── session/                            # 기존 세션 관리
├── samples/                            # 샘플 코드
├── tests/                             # 테스트 코드
├── CMakeLists.txt                     # 빌드 파일
└── README.md                          # 프로젝트 문서
```

### 주요 의존성
- **ASIO**: 비동기 I/O 및 네트워킹
- **fmt**: 문자열 포맷팅 라이브러리
- **container_system**: 메시지 직렬화/역직렬화
- **thread_system**: 비동기 작업 스케줄링
- **utilities**: 시스템 유틸리티 (messaging_system 내부)

---

## 🎯 분리 전략

### 1. 기본 원칙
- **점진적 마이그레이션**: 점진적 분리를 수행하면서 기존 코드 호환성 보장
- **의존성 역전**: network_system은 독립적으로 작동하지만 필요 시 통합 가능
- **기존 패턴 준수**: 성공적으로 분리된 시스템의 구조 및 패턴 준수
- **성능 최적화**: 분리로 인한 성능 오버헤드 최소화

### 2. 네임스페이스 정책
```cpp
// 기존 (messaging_system 내부)
namespace network_module { ... }

// 새로운 (독립적인 network_system)
namespace network_system {
    namespace core { ... }          // 핵심 API
    namespace session { ... }       // 세션 관리
    namespace integration { ... }   // 시스템 통합 인터페이스
}
```

### 3. 호환성 레이어
```cpp
namespace network_system::integration {
    // messaging_system 호환성을 위한 브리지
    class messaging_bridge;

    // container_system 통합
    class container_integration;

    // thread_system 통합
    class thread_integration;
}
```

---

## 📅 구현 로드맵

### Phase 1: 준비 및 분석 (2-3일)

#### Day 1
- [ ] messaging_system/network의 완전한 백업
- [ ] 기존 network_system과의 차이점 상세 분석
- [ ] 의존성 그래프 생성
- [ ] 마이그레이션 시나리오 검토

#### Day 2
- [ ] 새로운 디렉터리 구조 설계
- [ ] 네임스페이스 및 모듈 경계 정의
- [ ] 통합 인터페이스 설계
- [ ] 빌드 시스템 아키텍처 설계

#### Day 3
- [ ] 테스트 전략 수립
- [ ] 성능 벤치마크 기준선 설정
- [ ] 롤백 계획 수립

### Phase 2: 핵심 시스템 분리 (4-5일)

#### Day 4-5
- [ ] 새로운 network_system 디렉터리 구조 생성
- [ ] messaging_system/network 코드 복사 및 재구성
- [ ] 네임스페이스 변경 적용
- [ ] 기본 CMakeLists.txt 구성

#### Day 6-7
- [ ] 의존성 재구성 (ASIO, fmt, container_system, thread_system)
- [ ] 독립적인 빌드 시스템 구현
- [ ] vcpkg.json 및 dependency.sh 구성

#### Day 8
- [ ] 기본 단위 테스트 작성
- [ ] 독립 빌드 검증

### Phase 3: 통합 인터페이스 구현 (3-4일)

#### Day 9-10
- [ ] messaging_bridge 클래스 구현
- [ ] container_system 통합 인터페이스
- [ ] thread_system 통합 인터페이스

#### Day 11-12
- [ ] 호환성 API 구현
- [ ] 통합 테스트 작성

### Phase 4: messaging_system 업데이트 (2-3일)

#### Day 13-14
- [ ] messaging_system CMakeLists.txt 업데이트
- [ ] 외부 network_system 사용 옵션 추가
- [ ] 기존 코드 호환성 검증

#### Day 15
- [ ] 통합 테스트 실행
- [ ] 성능 벤치마크 비교

### Phase 5: 검증 및 배포 (2-3일)

#### Day 16-17
- [ ] 전체 시스템 통합 테스트
- [ ] 성능 최적화
- [ ] 문서 업데이트

#### Day 18
- [ ] 최종 검증
- [ ] 배포 준비

---

## 🛠️ 기술 구현 세부사항

### 1. 디렉터리 구조 재구성

#### 목표 구조
```
Sources/network_system/
├── include/network_system/             # 공개 헤더
│   ├── core/
│   │   ├── messaging_client.h
│   │   └── messaging_server.h
│   ├── session/
│   │   └── messaging_session.h
│   ├── integration/
│   │   ├── messaging_bridge.h
│   │   ├── container_integration.h
│   │   └── thread_integration.h
│   └── network_system.h               # 메인 헤더
├── src/                               # 구현 파일
│   ├── core/
│   ├── session/
│   ├── internal/
│   └── integration/
├── samples/                           # 샘플 코드
│   ├── basic_client_server/
│   ├── container_integration/
│   └── messaging_system_bridge/
├── tests/                             # 테스트
│   ├── unit/
│   ├── integration/
│   └── performance/
├── docs/                              # 문서
│   ├── api/
│   ├── tutorials/
│   └── migration_guide.md
├── cmake/                             # CMake 모듈
├── scripts/                           # 유틸리티 스크립트
│   ├── build.sh
│   ├── dependency.sh
│   └── migration/
├── CMakeLists.txt                     # 메인 빌드 파일
├── vcpkg.json                         # 패키지 의존성
├── README.md                          # 프로젝트 문서
└── CHANGELOG.md                       # 변경 이력
```

### 2. CMake 빌드 시스템

#### Main CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)

project(NetworkSystem
    VERSION 2.0.0
    DESCRIPTION "High-performance modular network system"
    LANGUAGES CXX
)

# 옵션
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration" ON)
option(BUILD_MESSAGING_BRIDGE "Build messaging_system bridge" ON)

# C++ 표준
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 패키지 찾기
find_package(asio CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# 조건부 의존성
if(BUILD_WITH_CONTAINER_SYSTEM)
    find_package(ContainerSystem CONFIG REQUIRED)
endif()

if(BUILD_WITH_THREAD_SYSTEM)
    find_package(ThreadSystem CONFIG REQUIRED)
endif()

# 라이브러리 생성
add_subdirectory(src)

# 테스트
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 샘플
if(BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# 설치 규칙
include(GNUInstallDirs)
install(TARGETS NetworkSystem
    EXPORT NetworkSystemTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Config 파일 생성
install(EXPORT NetworkSystemTargets
    FILE NetworkSystemTargets.cmake
    NAMESPACE NetworkSystem::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)
```

### 3. 통합 인터페이스 설계

#### messaging_bridge.h
```cpp
#pragma once

#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"

#ifdef BUILD_WITH_CONTAINER_SYSTEM
#include "container_system/container.h"
#endif

namespace network_system::integration {

class messaging_bridge {
public:
    messaging_bridge();
    ~messaging_bridge();

    // messaging_system 호환 API
    std::shared_ptr<core::messaging_server> create_server(
        const std::string& server_id
    );

    std::shared_ptr<core::messaging_client> create_client(
        const std::string& client_id
    );

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    // container_system 통합
    void set_container_factory(
        std::shared_ptr<container_system::factory> factory
    );

    void set_container_message_handler(
        std::function<void(const container_system::message&)> handler
    );
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
    // thread_system 통합
    void set_thread_pool(
        std::shared_ptr<thread_system::thread_pool> pool
    );
#endif

    // 성능 모니터링
    struct performance_metrics {
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t connections_active = 0;
        std::chrono::milliseconds avg_latency{0};
    };

    performance_metrics get_metrics() const;
    void reset_metrics();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::integration
```

### 4. 의존성 관리

#### vcpkg.json
```json
{
    "name": "network-system",
    "version-string": "2.0.0",
    "description": "High-performance modular network system",
    "dependencies": [
        "asio",
        "fmt",
        {
            "name": "gtest",
            "$condition": "BUILD_TESTS"
        },
        {
            "name": "benchmark",
            "$condition": "BUILD_TESTS"
        }
    ],
    "features": {
        "container-integration": {
            "description": "Enable container_system integration",
            "dependencies": [
                "container-system"
            ]
        },
        "thread-integration": {
            "description": "Enable thread_system integration",
            "dependencies": [
                "thread-system"
            ]
        }
    }
}
```

#### dependency.sh
```bash
#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

echo "========================================="
echo "Network System Dependency Setup"
echo "========================================="

# vcpkg 확인
if ! command -v vcpkg &> /dev/null; then
    echo "Error: vcpkg not found. Please install vcpkg first."
    exit 1
fi

# 기본 의존성 설치
echo "Installing basic dependencies..."
vcpkg install asio fmt

# 조건부 의존성
if [[ "${BUILD_TESTS:-ON}" == "ON" ]]; then
    echo "Installing test dependencies..."
    vcpkg install gtest benchmark
fi

# 외부 시스템 의존성 확인
echo "Checking external system dependencies..."

# container_system 확인
CONTAINER_SYSTEM_DIR="../container_system"
if [[ -d "$CONTAINER_SYSTEM_DIR" ]]; then
    echo "Found container_system at $CONTAINER_SYSTEM_DIR"
    export BUILD_WITH_CONTAINER_SYSTEM=ON
else
    echo "Warning: container_system not found. Integration will be disabled."
    export BUILD_WITH_CONTAINER_SYSTEM=OFF
fi

# thread_system 확인
THREAD_SYSTEM_DIR="../thread_system"
if [[ -d "$THREAD_SYSTEM_DIR" ]]; then
    echo "Found thread_system at $THREAD_SYSTEM_DIR"
    export BUILD_WITH_THREAD_SYSTEM=ON
else
    echo "Warning: thread_system not found. Integration will be disabled."
    export BUILD_WITH_THREAD_SYSTEM=OFF
fi

echo "Dependency setup completed successfully!"
```

---

## 🧪 테스트 전략

### 1. 단위 테스트
- **Core 모듈**: messaging_client, messaging_server의 개별 기능 테스트
- **Session 모듈**: 연결 관리 및 세션 상태 테스트
- **Integration 모듈**: 브리지 및 통합 인터페이스 테스트

### 2. 통합 테스트
- **독립 작동**: 독립 실행형 network_system 클라이언트-서버 통신
- **container_system 통합**: 메시지 직렬화/역직렬화 통합
- **thread_system 통합**: 비동기 작업 스케줄링 통합
- **messaging_system 호환성**: 기존 코드와의 호환성 검증

### 3. 성능 테스트
- **처리량**: Messages/second 처리 성능
- **지연시간**: 메시지 전송 지연시간
- **메모리 사용량**: 연결당 메모리 사용량
- **CPU 활용**: 부하 시 CPU 효율성

### 4. 스트레스 테스트
- **고부하 연결**: 10K+ 동시 연결 처리
- **장기 작동**: 24+ 시간 안정성 테스트
- **메모리 누수 감지**: 장기 메모리 누수 검사

---

## 📊 성능 요구사항

### 기준선 (분리 전)
- **처리량**: 100K messages/sec (1KB 메시지, localhost)
- **지연시간**: < 1ms (메시지 처리 지연시간)
- **동시 연결**: 10K+ 연결
- **메모리 사용량**: 연결당 ~8KB

### 목표 (분리 후)
- **처리량**: >= 100K messages/sec (성능 저하 < 5%)
- **지연시간**: < 1.2ms (브리지 오버헤드 < 20%)
- **동시 연결**: 10K+ 연결 (변경 없음)
- **메모리 사용량**: 연결당 ~10KB (증가 < 25%)

---

## 🚨 리스크 요인 및 완화

### 1. 주요 리스크
| 리스크 요인 | 영향 | 확률 | 완화 |
|-------------|---------|-------------|------------|
| 기존 network_system과 충돌 | 높음 | 중간 | 네임스페이스 분리, 기존 시스템 이름 변경 |
| 순환 의존성 | 높음 | 낮음 | 인터페이스 기반 의존성 역전 |
| 성능 저하 | 중간 | 중간 | 제로카피 브리지, 최적화된 통합 레이어 |
| 호환성 문제 | 높음 | 낮음 | 철저한 호환성 테스트, 점진적 마이그레이션 |
| 빌드 복잡도 증가 | 낮음 | 높음 | 자동화된 빌드 스크립트, 명확한 문서 |

### 2. 롤백 계획
- **Phase 체크포인트**: 각 단계별 롤백 지점 설정
- **백업 보관**: 원본 코드 백업 보관 (최소 30일)
- **기능 토글**: 구/신 시스템 간 런타임 전환
- **성능 모니터링**: 조기 문제 감지를 위한 실시간 성능 모니터링

---

## 📝 문서화 계획

### 1. API 문서
- **Doxygen**: 자동 생성 API 참조
- **사용 예제**: 실용적이고 사용 가능한 코드 예제
- **마이그레이션 가이드**: messaging_system에서 전환 방법

### 2. 아키텍처 문서
- **시스템 설계**: 전체 아키텍처 및 모듈 간 관계
- **의존성 그래프**: 시각적 의존성 관계 다이어그램
- **성능 특성**: 벤치마크 결과 및 최적화 가이드

### 3. 운영 문서
- **빌드 가이드**: 다양한 환경에서의 빌드 방법
- **배포 가이드**: 프로덕션 환경 배포 절차
- **문제 해결**: 일반적인 문제 및 해결책

---

## ✅ 완료 체크리스트

### Phase 1: 준비 및 분석
- [ ] messaging_system/network 백업 완료
- [ ] 기존 network_system과의 차이점 분석 완료
- [ ] 의존성 그래프 생성 완료
- [ ] 새로운 디렉터리 구조 설계 완료
- [ ] 네임스페이스 정의 완료
- [ ] 테스트 전략 수립

### Phase 2: 핵심 시스템 분리
- [ ] 새로운 디렉터리 구조 생성 완료
- [ ] 코드 복사 및 재구성 완료
- [ ] 네임스페이스 변경 적용
- [ ] 기본 CMakeLists.txt 구성 완료
- [ ] 의존성 재구성 완료
- [ ] 독립 빌드 시스템 구현 완료
- [ ] 기본 단위 테스트 작성

### Phase 3: 통합 인터페이스 구현
- [ ] messaging_bridge 클래스 구현 완료
- [ ] container_system 통합 인터페이스 완료
- [ ] thread_system 통합 인터페이스 완료
- [ ] 호환성 API 구현 완료
- [ ] 통합 테스트 작성

### Phase 4: messaging_system 업데이트
- [ ] messaging_system CMakeLists.txt 업데이트 완료
- [ ] 외부 network_system 사용 옵션 추가
- [ ] 기존 코드 호환성 검증 완료
- [ ] 통합 테스트 실행
- [ ] 성능 벤치마크 비교 완료

### Phase 5: 검증 및 배포
- [ ] 전체 시스템 통합 테스트 완료
- [ ] 성능 최적화 완료
- [ ] 문서 업데이트 완료
- [ ] 최종 검증 완료
- [ ] 배포 준비 완료

---

## 📞 연락처 및 지원

**프로젝트 담당자**: kcenon (kcenon@naver.com)
**프로젝트 기간**: 2025-09-19 ~ 2025-10-07 (예상 18일)
**상태**: 계획 완료, 구현 대기 중

---

*이 문서는 network_system 분리 작업을 위한 마스터 플랜입니다. 각 단계의 진행 상황에 따라 지속적으로 업데이트됩니다.*

---

*Last Updated: 2025-10-20*
