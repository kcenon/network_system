# Integration Guide

> **Language:** [English](INTEGRATION.md) | **한국어**

이 가이드는 network_system을 외부 시스템 및 라이브러리와 통합하는 방법을 설명합니다.

## 목차
- [Thread System 통합](#thread-system-통합)
- [Container System 통합](#container-system-통합)
- [Logger System 통합](#logger-system-통합)
- [빌드 구성](#빌드-구성)

## Thread System 통합

network_system은 성능 향상을 위해 외부 스레드 풀과 선택적으로 통합할 수 있습니다.

### 구성
```cmake
cmake .. -DBUILD_WITH_THREAD_SYSTEM=ON
```

### 아키텍처

`BUILD_WITH_THREAD_SYSTEM`이 활성화되면, `basic_thread_pool` 클래스는 내부적으로 `thread_system::thread_pool`에 위임합니다. 이를 통해:

- **통합 스레드 관리**: 모든 스레드 연산이 thread_system을 통해 처리됨
- **고급 기능**: adaptive_job_queue, hazard pointers, 워커 상태 모니터링에 접근
- **일관된 메트릭**: 스레드 풀 메트릭이 thread_system의 메트릭 인프라를 통해 보고됨
- **자동 혜택**: 코드 변경 불필요 - 기존 `basic_thread_pool` 사용이 자동으로 thread_system 기능을 활용

### 구현 세부사항

```cpp
// basic_thread_pool은 이제 내부적으로 thread_system::thread_pool을 사용
class basic_thread_pool::impl {
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
    // ... 모든 연산을 pool_에 위임
};
```

thread_system을 사용할 수 없는 경우, `basic_thread_pool`은 독립형 std::thread 기반 구현으로 폴백합니다.

### 사용
thread_system이 사용 가능한 경우, 네트워크 연산은 다음을 위해 스레드 풀을 자동으로 활용합니다:
- 연결 처리
- 메시지 처리
- 비동기 연산

### thread_system_pool_adapter 사용

thread_system 기능에 직접 접근하려면 `thread_system_pool_adapter`를 사용할 수 있습니다:

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

// 서비스 컨테이너에서 어댑터 생성 또는 새 풀 생성
auto adapter = thread_system_pool_adapter::from_service_or_default("network_pool");

// 또는 thread_system을 직접 통합 관리자에 바인딩
bind_thread_system_pool_into_manager("my_pool");
```

### 요구 사항
- thread_system은 `../thread_system`에 설치되어야 함
- 헤더는 `../thread_system/include`에서 사용 가능해야 함

## Container System 통합

고급 직렬화 및 역직렬화 기능을 활성화합니다.

### 구성
```cmake
cmake .. -DBUILD_WITH_CONTAINER_SYSTEM=ON
```

### 기능
- 바이너리 직렬화
- JSON 직렬화
- Protocol buffer 지원
- 맞춤 컨테이너 타입

### API 예제
```cpp
#include <network_system/integration/container_integration.h>

// 맞춤 데이터 직렬화
auto serialized = container_adapter::serialize(my_data);
client->send_packet(serialized);
```

## Logger System 통합

구성 가능한 로그 레벨과 출력 형식을 가진 구조화된 로깅을 제공합니다.

### 구성
```cmake
cmake .. -DBUILD_WITH_LOGGER_SYSTEM=ON
```

### 기능
- **로그 레벨**: TRACE, DEBUG, INFO, WARN, ERROR, FATAL
- **소스 위치 추적**: 자동 파일, 라인 및 함수 기록
- **타임스탬프 형식**: 밀리초 정밀도 타임스탬프
- **조건부 컴파일**: logger_system을 사용할 수 없을 때 콘솔 출력으로 폴백

### 사용

#### 기본 로깅
```cpp
#include <network_system/integration/logger_integration.h>

// 편의 매크로 사용
NETWORK_LOG_INFO("Server started on port " + std::to_string(port));
NETWORK_LOG_ERROR("Connection failed: " + error_message);
NETWORK_LOG_DEBUG("Received " + std::to_string(bytes) + " bytes");
```

#### 고급 구성
```cpp
// logger 인스턴스 가져오기
auto& logger_mgr = kcenon::network::integration::logger_integration_manager::instance();
auto logger = logger_mgr.get_logger();

// 로그 레벨이 활성화되었는지 확인
if (logger->is_enabled(kcenon::network::integration::log_level::debug)) {
    // 비용이 많이 드는 디버그 로깅 수행
    std::string detailed_state = generate_detailed_state();
    NETWORK_LOG_DEBUG(detailed_state);
}

// 매크로 없이 직접 로깅
logger->log(kcenon::network::integration::log_level::warn,
           "Custom warning message",
           __FILE__, __LINE__, __FUNCTION__);
```

### 구현 세부사항

logger 통합은 두 가지 구현을 제공합니다:

1. **basic_logger**: 독립형 콘솔 로거
   - BUILD_WITH_LOGGER_SYSTEM이 OFF일 때 사용
   - std::cout (INFO 이하) 또는 std::cerr (WARN 이상)로 출력
   - 간단한 타임스탬프 형식

2. **logger_system_adapter**: 외부 로거 통합
   - BUILD_WITH_LOGGER_SYSTEM이 ON일 때 사용
   - 전체 기능 로깅을 위해 kcenon::logger를 래핑
   - 로그 파일 회전, 원격 로깅 등 지원

### 요구 사항
- BUILD_WITH_LOGGER_SYSTEM=ON일 때:
  - logger_system은 `../logger_system`에 설치되어야 함
  - 헤더는 `../logger_system/include`에서 사용 가능해야 함
- string_view 지원을 위한 C++17 이상

## 빌드 구성

### 모든 통합을 포함한 완전한 빌드
```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=ON \
    -DBUILD_WITH_CONTAINER_SYSTEM=ON \
    -DBUILD_WITH_LOGGER_SYSTEM=ON
```

### 최소 빌드 (외부 의존성 없음)
```bash
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=OFF \
    -DBUILD_WITH_CONTAINER_SYSTEM=OFF \
    -DBUILD_WITH_LOGGER_SYSTEM=OFF
```

### 사용 가능한 통합 확인

시스템은 사용 가능한 통합을 확인하기 위한 매크로를 제공합니다:

```cpp
#ifdef BUILD_WITH_THREAD_SYSTEM
    // Thread system 사용 가능
    use_thread_pool();
#endif

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    // Container system 사용 가능
    use_advanced_serialization();
#endif

#ifdef BUILD_WITH_LOGGER_SYSTEM
    // Logger system 사용 가능
    use_structured_logging();
#else
    // 기본 로깅으로 폴백
    use_console_logging();
#endif
```

## 공통 플래그 요약

- `BUILD_WITH_THREAD_SYSTEM` — `thread_integration_manager`를 통한 스레드 풀 통합 활성화.
- `BUILD_WITH_CONTAINER_SYSTEM` — `container_manager`를 통한 컨테이너 어댑터 활성화.
- `BUILD_WITH_LOGGER_SYSTEM` — `logger_system_adapter` 사용; 그렇지 않으면 `basic_logger`로 폴백.

## 버전 관리 및 의존성

- 모듈 간 권장 vcpkg 기준선 정렬 (예제):
  - fmt: 10.2.1
  - 공유 `builtin-baseline` 및 fmt 재정의를 설정하기 위한 최상위 `vcpkg-configuration.json` 제공.
- C++ 표준: C++20 (string_view 및 코루틴 친화적 API; 비활성화 시 비코루틴 경로로 폴백).
- ASIO standalone 필수 (`vcpkg.json` 참조).

## 성능 고려사항

### Thread System과 함께
- **이점**: 동시 연결 처리에서 최대 40% 향상
- **트레이드오프**: 약간 증가된 메모리 사용량

### Container System과 함께
- **이점**: 최소 오버헤드를 가진 타입 안전 직렬화
- **트레이드오프**: 추가 의존성 및 빌드 시간

### Logger System과 함께
- **이점**: 필터링을 가진 구조화된 로깅으로 I/O 오버헤드 감소
- **트레이드오프**: 최소 성능 영향 (~1-2%)

## 문제 해결

### 통합을 찾을 수 없음
CMake가 통합 시스템을 찾을 수 없는 경우:
1. 시스템이 예상 위치에 설치되었는지 확인
2. include 경로가 올바른지 확인
3. 시스템이 호환 가능한 컴파일러 설정으로 빌드되었는지 확인

### 링크 오류
- 모든 시스템이 동일한 C++ 표준으로 빌드되었는지 확인
- 시스템 간 ABI 호환성 확인
- 필요한 모든 라이브러리가 링크되었는지 확인

### 런타임 이슈
- 모든 통합 시스템이 올바르게 초기화되었는지 확인
- 여러 통합을 사용할 때 스레드 안전성 확인
- 초기화 오류에 대한 로그 출력 검토

---

*Last Updated: 2025-10-20*
