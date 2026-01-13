# Network System 통합 테스트

> **Language:** [English](README.md) | **한국어**

이 디렉토리는 network_system 라이브러리에 대한 포괄적인 통합 테스트를 포함합니다. 이 테스트는 실제 시나리오에서 함께 작동하는 네트워크 구성 요소의 완전한 동작을 검증합니다.

## 테스트 구조

```
integration_tests/
├── framework/              # 테스트 인프라
│   ├── system_fixture.h   # 기본 테스트 픽스처
│   └── test_helpers.h     # 헬퍼 함수 및 유틸리티
├── scenarios/             # 시나리오 기반 테스트
│   ├── connection_lifecycle_test.cpp
│   └── protocol_integration_test.cpp
├── performance/           # 성능 테스트
│   └── network_performance_test.cpp
└── failures/              # 오류 처리 테스트
    └── error_handling_test.cpp
```

## 테스트 스위트

### 1. 연결 수명 주기 테스트 (17개 테스트)
네트워크 연결의 전체 수명 주기를 테스트합니다:
- 서버 초기화 및 시작
- 클라이언트 연결 설정
- 연결 수락 처리
- 메시지 전송 및 수신
- 연결 종료 및 정리
- 다중 동시 연결
- 서버 종료

**주요 테스트:**
- `ServerInitialization` - 서버 생성 확인
- `ServerStartupSuccess` - 서버 시작 정상 동작
- `ClientConnectionSuccess` - 클라이언트 서버 연결
- `MultipleConcurrentConnections` - 다중 클라이언트 처리
- `ServerShutdownWithActiveConnections` - 우아한 종료

### 2. 프로토콜 통합 테스트 (13개 테스트)
프로토콜 수준 기능을 테스트합니다:
- 메시지 직렬화/역직렬화
- 요청-응답 패턴
- 메시지 단편화 및 재조립
- 데이터 무결성 검증

**주요 테스트:**
- `SmallMessageTransmission` - 작은 메시지 전송
- `LargeMessageTransmission` - 큰 메시지 전송
- `MessageFragmentation` - 메시지 단편화 처리
- `BinaryDataTransmission` - 바이너리 데이터 무결성
- `SequentialMessages` - 순차 메시지 처리

### 3. 네트워크 성능 테스트 (10개 테스트)
성능 특성을 측정합니다:
- 연결 처리량
- 메시지 지연 시간 (P50, P95, P99)
- 대역폭 활용
- 동시 연결 확장성
- 메시지 처리량

**성능 기준:**
- 연결 설정: < 100ms (P50)
- 메시지 지연 시간 P50: < 10ms
- 메시지 지연 시간 P95: < 50ms
- 메시지 처리량: > 1,000 msg/s
- 동시 연결: > 20개 동시

**주요 테스트:**
- `ConnectionThroughput` - 연결 속도 측정
- `SmallMessageLatency` - 작은 메시지의 지연 시간 측정
- `MessageThroughput` - 메시지 처리 속도 측정
- `ConcurrentConnectionScalability` - 다양한 부하로 테스트
- `SustainedLoad` - 지속적인 부하 처리

### 4. 오류 처리 테스트 (18개 테스트)
오류 조건 및 복구를 테스트합니다:
- 연결 실패 및 타임아웃
- 잘못된 작업
- 네트워크 오류
- 리소스 고갈
- 복구 시나리오

**주요 테스트:**
- `ConnectToInvalidHost` - 연결 실패 처리
- `SendWithoutConnection` - 잘못된 작업 방지
- `ServerShutdownDuringTransmission` - 중단 처리
- `LargeMessageHandling` - 버퍼 제한 처리
- `RecoveryAfterConnectionFailure` - 복구 테스트

## 테스트 실행

### 모든 통합 테스트 빌드 및 실행

```bash
# 통합 테스트를 활성화하여 구성
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DNETWORK_BUILD_INTEGRATION_TESTS=ON

# 빌드
cmake --build build

# 모든 통합 테스트 실행
cd build
ctest -R "ConnectionLifecycle|ProtocolIntegration|NetworkPerformance|ErrorHandling"
```

### 특정 테스트 스위트 실행

```bash
# 연결 수명 주기 테스트
./build/bin/integration_tests/connection_lifecycle_test

# 프로토콜 통합 테스트
./build/bin/integration_tests/protocol_integration_test

# 성능 테스트
./build/bin/integration_tests/network_performance_test

# 오류 처리 테스트
./build/bin/integration_tests/error_handling_test
```

### 자세한 출력으로 실행

```bash
ctest -V -R "ConnectionLifecycle"
```

## 커버리지 분석

### 커버리지 보고서 생성

```bash
# 커버리지를 활성화하여 구성
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DNETWORK_BUILD_INTEGRATION_TESTS=ON

# 빌드 및 테스트 실행
cmake --build build
cd build
ctest

# 커버리지 보고서 생성
lcov --capture --directory . --output-file coverage.info \
  --ignore-errors gcov,source,unused
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/integration_tests/*' \
  --output-file coverage_filtered.info --ignore-errors unused
genhtml coverage_filtered.info --output-directory coverage_html

# 커버리지 보기
open coverage_html/index.html  # macOS
xdg-open coverage_html/index.html  # Linux
```

### 커버리지 목표

- **라인 커버리지:** > 80%
- **브랜치 커버리지:** > 70%
- **함수 커버리지:** > 90%

## 테스트 프레임워크

### NetworkSystemFixture

다음을 제공하는 기본 픽스처:
- `StartServer()` - 테스트 서버 시작
- `StopServer()` - 테스트 서버 중지
- `ConnectClient()` - 테스트 클라이언트 연결
- `SendMessage()` - 클라이언트에서 메시지 전송
- `CreateTestMessage()` - 테스트 데이터 생성
- `WaitFor()` - 지정된 기간 동안 대기

### MultiConnectionFixture

다중 연결을 위한 확장 픽스처:
- `CreateClients()` - 다중 클라이언트 생성
- `ConnectAllClients()` - 모든 클라이언트 연결

### PerformanceFixture

성능 테스트 픽스처:
- `MeasureDuration()` - 작업 기간 측정
- `CalculateStats()` - 통계 계산

### 테스트 헬퍼

`test_helpers.h`의 유틸리티 함수:
- `find_available_port()` - 사용 가능한 포트 찾기
- `wait_for_connection()` - 연결 대기
- `wait_for_ready()` - 비동기 작업 완료 대기
- `is_ci_environment()` - CI 환경 감지
- `is_macos()` - macOS 플랫폼 감지
- `is_sanitizer_run()` - 새니타이저 계측 감지
- `calculate_statistics()` - 성능 통계 계산
- `generate_random_data()` - 테스트 데이터 생성
- `verify_message_data()` - 데이터 무결성 검증

## 성능 벤치마킹

성능 테스트는 자세한 지표를 제공합니다:

```
Connection latency (ms):
  P50: 5.2
  P95: 12.8
  P99: 18.5

Small message latency (ms):
  P50: 0.8
  P95: 2.1
  P99: 4.3

Message throughput: 15,234 msg/s
Bandwidth: 125.5 MB/s
```

## CI/CD 통합

통합 테스트는 다음에서 자동으로 실행됩니다:
- main/develop/feature 브랜치로 푸시
- 풀 리퀘스트
- 수동 워크플로우 디스패치

### 테스트 매트릭스

- **플랫폼:** Ubuntu, macOS
- **빌드 타입:** Debug, Release
- **커버리지:** Debug 빌드에서 생성

### 아티팩트

- 커버리지 보고서 (Debug 빌드)
- 테스트 결과
- 성능 지표

## 테스트 디버깅

### 단일 테스트 실행

```bash
./build/bin/integration_tests/connection_lifecycle_test \
  --gtest_filter="ConnectionLifecycleTest.ServerStartupSuccess"
```

### 디버그 출력 활성화

```bash
./build/bin/integration_tests/network_performance_test --gtest_color=yes
```

### 디버거에서 실행

```bash
lldb ./build/bin/integration_tests/error_handling_test
(lldb) run --gtest_filter="ErrorHandlingTest.ConnectToInvalidHost"
```

## 새 테스트 추가

### 1. 테스트 스위트 선택

테스트가 속한 카테고리를 결정합니다:
- **scenarios/** - 연결 및 프로토콜 동작
- **performance/** - 성능 측정
- **failures/** - 오류 조건

### 2. 테스트 케이스 추가

```cpp
TEST_F(ConnectionLifecycleTest, YourTestName) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // 테스트 로직
    auto message = CreateTestMessage(256);
    EXPECT_TRUE(SendMessage(message));
}
```

### 3. CMakeLists.txt 업데이트

`CMakeLists.txt`의 적절한 실행 파일에 새 테스트 파일을 추가합니다.

### 4. 확인

```bash
cmake --build build
ctest -R "YourTestName" -V
```

## 네트워크별 고려사항

### 포트 할당

테스트는 충돌을 방지하기 위해 동적 포트 할당을 사용합니다:
```cpp
test_port_ = test_helpers::find_available_port();
```

### Localhost 테스트

모든 테스트는 localhost 루프백을 사용합니다 (외부 네트워크 의존성 없음):
```cpp
client_->start_client("localhost", test_port_);
```

### 타임아웃

테스트에는 비동기 작업에 대한 적절한 타임아웃이 포함됩니다:
```cpp
test_helpers::wait_for_connection(client_, std::chrono::seconds(5));
```

**플랫폼별 타임아웃:**
- macOS CI 환경(특히 Release 빌드)에서는 더 긴 타임아웃(10초) 사용
- kqueue 기반 비동기 I/O 동작 차이를 고려
- macOS CI에서 서버 시작 후 추가 100ms 대기
- macOS에서 순차적 연결 시 적절한 리소스 정리를 위해 연결 사이에 짧은 대기 시간 포함

### 정리

적절한 정리는 포트 고갈을 방지합니다:
```cpp
void TearDown() override {
    if (client_) client_->stop_client();
    if (server_) server_->stop_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
```

## 문제 해결

### 포트 충돌로 테스트 실패

`find_available_port()`의 포트 범위를 늘리거나 임의 포트를 사용합니다.

### CI에서 타임아웃

CI 환경은 느릴 수 있으므로 타임아웃 값을 늘립니다:
```cpp
EXPECT_LT(stats.p50, 100.0);  // CI에 대해 더 보수적
```

### macOS CI 연결 실패

macOS CI 환경(특히 Release 빌드)에서 다음과 같은 이유로 연결 타임아웃이
발생할 수 있습니다:
- Linux의 epoll과 다른 kqueue 기반 비동기 I/O 동작 차이
- GitHub Actions 러너의 리소스 제약
- 최적화 환경에서 소켓 작업 속도 저하

테스트 프레임워크는 macOS CI 환경에서 자동으로 더 긴 타임아웃(10초)과
추가 서버 시작 대기 시간(100ms)을 적용합니다. 문제가 지속되면 네트워크
혼잡을 확인하거나 플랫폼별 처리를 위해 `test_helpers::is_macos()`를
사용하는 것을 고려하세요.

### 메모리 누수

새니타이저를 사용하여 실행:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address"
```

## 성능 최적화

테스트는 다음을 위해 설계되었습니다:
- 설정/해제 오버헤드 최소화
- 가능한 경우 연결 재사용
- 현실적인 메시지 크기 사용
- 실제 네트워크 동작 측정

## 기여하기

통합 테스트를 추가할 때:
1. 기존 패턴 및 픽스처를 따릅니다
2. 성능 기준을 포함합니다
3. 예상 동작을 문서화합니다
4. 오류 조건을 테스트합니다
5. 테스트가 결정론적인지 확인합니다
6. 리소스를 적절히 정리합니다

## 참고 자료

- [Google Test 문서](https://google.github.io/googletest/)
- [Network System API](../README.md)
- [성능 기준](../BASELINE.md)

## 총 테스트 수

- **연결 수명 주기:** 17개 테스트
- **프로토콜 통합:** 13개 테스트
- **네트워크 성능:** 10개 테스트
- **오류 처리:** 18개 테스트

**총계:** 58개 통합 테스트
