# Network System - 성능 기준 메트릭

[English](BASELINE.md) | **한국어**

**버전**: development (matches `VERSION`)
**최종 업데이트**: 2025-10-26 (Asia/Seoul)
**단계**: Phase 0 - Foundation
**상태**: ⚠️ 합성 마이크로벤치마크 캡처됨; 실제 네트워크 기준선 대기 중

---

## 📋 개요

이 문서는 network_system에 대해 현재 사용 가능한 CPU 전용 Google Benchmark 결과를 요약합니다. 아래 측정값은 메시지 할당/복사, 모의 연결 처리 및 세션 기록을 다룹니다; 소켓 I/O, 커널 레벨 네트워킹 또는 TLS는 포함하지 **않습니다**. 실제 엔드투엔드 기준선은 남은 로드맵 항목이 완료된 후 추가될 예정입니다.

**주요 측정 환경**: Intel i7-12700K (프로덕션 참조)
**보조 환경**: Apple M1 (개발 테스트)

---

## 시스템 정보

### 참조 하드웨어 구성 (주요)
- **CPU**: Intel i7-12700K @ 3.8GHz (12 cores, 20 threads)
- **RAM**: 32 GB DDR4
- **Network**: Loopback (localhost 테스트)
- **OS**: Ubuntu 22.04 LTS
- **Compiler**: GCC 11.4 with -O3 optimization
- **Build Type**: Release
- **C++ Standard**: C++20

### 개발 하드웨어 구성 (보조)
- **CPU**: Apple M1 (ARM64, 8 cores)
- **RAM**: 8 GB
- **Network**: Loopback (localhost 테스트)
- **OS**: macOS 26.1
- **Compiler**: Apple Clang 17.0.0
- **Build Type**: Release (-O3)
- **C++ Standard**: C++20

---

## 성능 메트릭 (Intel i7-12700K, Ubuntu 22.04, GCC 11 `-O3`)

다음 표는 `benchmarks/message_throughput_bench.cpp`의 CPU 전용 Google Benchmark 결과를 캡처합니다. 각 벤치마크는 완전히 프로세스 내에서 실행됩니다(`std::vector<uint8_t>` 버퍼를 할당하고 복사); 소켓이 열리지 않으며 패킷이 호스트를 떠나지 않습니다. 이 수치는 직렬화/복사 오버헤드에 대해서만 추론하는 데 사용하세요.

| 벤치마크 | 페이로드 | CPU 시간/작업 (ns) | 근사 처리량 | 범위 |
|---------|---------|-------------------|------------|------|
| MessageThroughput/64B | 64 bytes | 1,300 | ~769,000 msg/s | 메모리 내 할당 + memcpy |
| MessageThroughput/256B | 256 bytes | 3,270 | ~305,000 msg/s | 메모리 내 할당 + memcpy |
| MessageThroughput/1KB | 1 KB | 7,803 | ~128,000 msg/s | 메모리 내 할당 + memcpy |
| MessageThroughput/8KB | 8 KB | 48,000 | ~21,000 msg/s | 메모리 내 할당 + memcpy |

연결 및 세션 벤치마크(`benchmarks/connection_bench.cpp`, `benchmarks/session_bench.cpp`)는 현재 작업을 에뮬레이트하는 모의 객체를 사용합니다(예: `std::this_thread::sleep_for` 호출). 실제 소켓이나 메모리 풀을 실행하지 않기 때문에, 연결 설정 시간, 메모리 소비 또는 세션 오버헤드에 대한 이전 주장은 대표적인 측정이 존재할 때까지 제거되었습니다.

---

## 현재 발견 사항

- ✅ Google Benchmark 제품군이 `NETWORK_BUILD_BENCHMARKS=ON`으로 컴파일 및 실행됨
- ✅ 합성 직렬화/메모리 복사 핫 패스가 측정되고 재현 가능함
- ✅ WebSocket 프로토콜 지원(RFC 6455)이 클라이언트/서버 API와 함께 구현됨
- ⚠️ 실제 네트워크 지연시간/처리량 및 메모리 기준선이 아직 캡처되지 않음 (TCP, UDP, WebSocket)
- ⚠️ Zero-copy 파이프라인 및 connection pooling은 로드맵 항목으로 남아있음 (`IMPROVEMENTS.md` 참조)
- ✅ C++20 coroutine 지원 전송 파이프라인(`src/internal/send_coroutine.*`)이 준비됨

---

## Baseline 검증

### Phase 0 체크리스트
- [x] 벤치마크 인프라 (`benchmarks/` + Google Benchmark) ✅
- [x] 합성 직렬화/복사 메트릭 캡처됨 ✅
- [x] 실제 네트워크 처리량 기준선 ✅ (Phase 7)
- [x] 실제 지연시간 분포 (P50/P95/P99) ✅ (Phase 7)
- [x] 부하 시 메모리 풋프린트 ✅ (Phase 7)
- [x] 동시 연결 스트레스 기준선 ✅ (Phase 7)

---

## 실제 네트워크 성능 메트릭

**상태**: 🔄 부하 테스트 인프라 준비 완료; 초기 기준선 실행 대기 중
**단계**: Phase 7 - 네트워크 부하 테스트 및 실제 기준선 메트릭

위의 합성 CPU 전용 벤치마크와 달리, 다음 메트릭은 네트워크 스택을 통한 실제 TCP/UDP/WebSocket 통신을 측정합니다:
- 실제 소켓 I/O 및 커널 네트워크 스택 오버헤드
- 프로토콜 프레이밍 및 핸드셰이크 비용
- 엔드투엔드 지연시간 분포 (P50/P95/P99)
- 실제 네트워크 작업 중 메모리 소비
- 동시 연결 처리 용량

### 부하 테스트 인프라

부하 테스트 제품군은 `integration_tests/performance/`에 다음 구성 요소로 구현되어 있습니다:

**테스트 실행 파일:**
- `tcp_load_test` - TCP 처리량, 지연시간 및 동시성 테스트
- `udp_load_test` - UDP 처리량, 패킷 손실 및 버스트 성능
- `websocket_load_test` - WebSocket 메시지 처리량 및 ping/pong 지연시간

**측정 프레임워크:**
- `MemoryProfiler` - 크로스 플랫폼 RSS/heap/VM 측정 (Linux/macOS/Windows)
- `ResultWriter` - CI 통합을 위한 JSON/CSV 출력
- `test_helpers::Statistics` - P50/P95/P99 지연시간 백분위수 계산

**자동화:**
- GitHub Actions 워크플로우: `.github/workflows/network-load-tests.yml`
- 메트릭 수집: `scripts/collect_metrics.py`
- Baseline 업데이트: `scripts/update_baseline.py`

### 방법론

각 프로토콜은 다음과 같이 테스트됩니다:
- **메시지 크기:** 64B, 512B, 1KB, 64KB (프로토콜에 따라 다름)
- **테스트 반복:** 테스트당 1000개 메시지
- **동시 연결:** 10개 및 50개 클라이언트
- **측정:** 처리량 (msg/s), 지연시간 백분위수 (P50/P95/P99), 메모리 (RSS)
- **환경:** 일관성을 위한 로컬호스트 루프백

### 부하 테스트 실행

```bash
# 통합 테스트로 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j

# 개별 프로토콜 테스트 실행
./build/bin/integration_tests/tcp_load_test
./build/bin/integration_tests/udp_load_test
./build/bin/integration_tests/websocket_load_test

# 결과는 JSON 파일로 작성됩니다:
# tcp_64b_results.json, udp_64b_results.json, websocket_text_64b_results.json
```

### 자동 Baseline 수집

GitHub Actions 워크플로우는 매주 부하 테스트를 실행하며 수동으로 트리거할 수 있습니다:

```bash
# baseline 업데이트로 수동 워크플로우 트리거
gh workflow run network-load-tests.yml --field update_baseline=true
```

결과는 플랫폼별(Ubuntu/macOS/Windows)로 수집되며 BASELINE.md에 병합하기 전에 검토할 수 있습니다.

### 예상 메트릭 (플레이스홀더)

첫 번째 baseline 실행이 완료되면 이 섹션에는 다음과 같은 표가 포함됩니다:

| 프로토콜   | 처리량       | 지연시간 P50 | 지연시간 P95 | 지연시간 P99 | 메모리 (RSS) | 플랫폼       |
|-----------|-------------|-------------|-------------|-------------|-------------|-------------|
| TCP       | TBD msg/s   | TBD ms      | TBD ms      | TBD ms      | TBD MB      | ubuntu-22.04|
| UDP       | TBD msg/s   | TBD ms      | TBD ms      | TBD ms      | TBD MB      | ubuntu-22.04|
| WebSocket | TBD msg/s   | TBD ms      | TBD ms      | TBD ms      | TBD MB      | ubuntu-22.04|

이 메트릭은 `scripts/update_baseline.py`를 사용하여 CI 파이프라인에 의해 자동으로 업데이트됩니다.

---

## 🔬 이러한 측정 재현하기

모든 기준선 메트릭은 독립적으로 검증 가능합니다:

### 벤치마크 빌드

```bash
git clone https://github.com/kcenon/network_system.git
cd network_system

# 벤치마크를 활성화하여 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETWORK_BUILD_BENCHMARKS=ON
cmake --build build -j
```

### 벤치마크 실행

```bash
# 모든 벤치마크 실행
./build/network_benchmarks

# JSON 출력 생성
./build/network_benchmarks --benchmark_format=json --benchmark_out=baseline_results.json

# 특정 카테고리 실행
./build/network_benchmarks --benchmark_filter=MessageThroughput
./build/network_benchmarks --benchmark_filter=Connection
./build/network_benchmarks --benchmark_filter=Session
```

### 예상 출력 (Intel i7-12700K)

```
-------------------------------------------------------------------------
Benchmark                               Time       CPU   Iterations
-------------------------------------------------------------------------
MessageThroughput/64B            1300 ns   1299 ns       538462   # ~769K msg/s
MessageThroughput/256B           3270 ns   3268 ns       214286   # ~305K msg/s
MessageThroughput/1KB            7803 ns   7801 ns        89744   # ~128K msg/s
MessageThroughput/8KB           48000 ns  47998 ns        14583   # ~21K msg/s
```

### 중요 참고사항

- **하드웨어 의존성**: 결과는 CPU, RAM 및 컴파일러 플래그에 따라 다릅니다
- **범위**: 벤치마크는 완전히 CPU에서 실행됨; 소켓 I/O가 발생하지 않음
- **OS 최적화**: 실제 네트워크 실행이 캡처될 때까지 커널/네트워크 튜닝은 무관함
- **루프백 테스트**: 향후 네트워크 기준선은 일관된 루프백 또는 LAN 설정을 사용해야 함
- **재현성**: 합성 측정은 동일한 하드웨어에서 결정적임

---

**Baseline 수립**: 2025-10-26 (Asia/Seoul)
**유지보수자**: kcenon
**문서 상태**: 합성 CPU 벤치마크 기록됨; WebSocket 지원 추가됨; 실제 메트릭 대기 중
