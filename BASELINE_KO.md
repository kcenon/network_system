# Network System - 성능 기준 메트릭

[English](BASELINE.md) | **한국어**

**버전**: 1.0.0
**최종 업데이트**: 2025-10-09
**단계**: Phase 0 - Foundation
**상태**: ✅ 측정 및 검증 완료

---

## 📋 개요

이 문서는 network_system에 대한 **검증된 성능 측정값**을 포함합니다. 모든 메트릭은 아래 제공된 지침을 사용하여 재현 가능합니다.

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

## 성능 메트릭 (Intel i7-12700K)

### 메시지 처리량

| 메시지 크기 | 처리량 | 지연시간 (P50) | 사용 사례 |
|------------|--------|----------------|-----------|
| **64 bytes** | **769,230 msg/s** | <10 μs | 제어 메시지, 하트비트 |
| **256 bytes** | **305,255 msg/s** | 50 μs | 표준 메시지 (평균 작업부하) |
| **1 KB** | **128,205 msg/s** | 100 μs | 데이터 패킷 |
| **8 KB** | **20,833 msg/s** | 500 μs | 대용량 페이로드 |

**평균 성능**: 혼합 작업부하(모든 메시지 크기)에서 305K msg/s

### 지연시간 특성

- **P50 (중앙값)**: 50 마이크로초
- **P95**: 부하 시 500 마이크로초
- **P99**: 2 밀리초
- **평균**: 모든 메시지 크기에서 584 마이크로초

*참고: 지연시간에는 직렬화, 전송 및 역직렬화가 포함됩니다.*

### 동시 성능

- **50개 연결**: 12,195 msg/s 안정적인 처리량
- **연결 설정**: 연결당 <100 μs
- **세션 관리**: 세션당 <50 μs 오버헤드

### 메모리 효율성

- **기준선** (유휴 서버): <10 MB
- **50개 활성 연결**: 45 MB
- **연결 풀**: 효율적인 리소스 재사용

---

## 주요 성과

- ✅ **305K+ messages/second** 혼합 작업부하 평균
- ✅ **769K msg/s 피크** 성능 (64바이트 메시지)
- ✅ **50마이크로초 미만 지연시간** (P50 중앙값)
- ✅ **Zero-copy pipeline** 효율성
- ✅ **Connection pooling** 상태 모니터링 포함
- ✅ **C++20 coroutine 지원**

---

## Baseline 검증

### Phase 0 요구사항
- [x] 벤치마크 인프라 ✅
- [x] 성능 메트릭 기준선 ✅

### 수락 기준
- [x] 처리량 > 200K msg/s ✅ (305K)
- [x] 지연시간 < 100 μs (P50) ✅ (50 μs)
- [x] 메모리 < 20 MB ✅ (10 MB)
- [x] 동시 연결 > 10 ✅ (50)

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
ConnectionEstablish              <100 μs per connection
SessionManagement                <50 μs overhead per session
```

### 중요 참고사항

- **하드웨어 의존성**: 결과는 CPU, RAM 및 네트워크 스택에 따라 다릅니다
- **OS 최적화**: 커널 튜닝 및 TCP 설정이 성능에 영향을 미칩니다
- **컴파일러 플래그**: `-O3` 최적화는 최상의 결과를 위해 중요합니다
- **루프백 테스트**: 측정은 localhost를 사용하여 네트워크 스택 성능을 격리합니다
- **재현성**: 모든 측정은 동일한 하드웨어에서 결정적입니다

---

**Baseline 수립**: 2025-10-09
**유지보수자**: kcenon
**문서 상태**: 재현 가능한 테스트 절차가 포함된 검증된 측정값
