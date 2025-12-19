# network_system 성능 벤치마크

> **Language:** [English](README.md) | **한국어**

Phase 0, Task 0.2: 기준 성능 벤치마킹

## 개요

이 디렉토리는 network_system의 포괄적인 성능 벤치마크를 포함하며, 다음을 측정합니다:

- **메시지 처리량**: 메시지 생성, 직렬화 및 처리 성능
- **연결 관리**: 연결 설정, 수명 주기 및 풀 관리
- **세션 관리**: 세션 생성, 조회 및 정리 성능
- **TCP 수신 디스패치**: std::span vs std::vector 수신 콜백 오버헤드 비교

## 빌드

### 필수 요구사항

```bash
# macOS (Homebrew)
brew install google-benchmark

# Ubuntu/Debian
sudo apt-get install libbenchmark-dev

# 소스에서 빌드
git clone https://github.com/google/benchmark.git
cd benchmark
cmake -E make_directory build
cmake -E chdir build cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
cmake --build build --config Release
sudo cmake --build build --config Release --target install
```

### 벤치마크 빌드

```bash
cd network_system
cmake -B build -S . -DNETWORK_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 또는 빌드 타겟 사용
cd build
make network_benchmarks
```

## 벤치마크 실행

### 모든 벤치마크 실행

```bash
./build/benchmarks/network_benchmarks
```

### 특정 벤치마크 카테고리 실행

```bash
# 메시지 처리량 벤치마크만
./build/benchmarks/network_benchmarks --benchmark_filter=Message

# 연결 벤치마크만
./build/benchmarks/network_benchmarks --benchmark_filter=Connection

# 세션 벤치마크만
./build/benchmarks/network_benchmarks --benchmark_filter=Session

# TCP 수신 디스패치 벤치마크만
./build/benchmarks/network_benchmarks --benchmark_filter=TcpReceive
```

### 출력 형식

```bash
# 콘솔 출력 (기본값)
./build/benchmarks/network_benchmarks

# JSON 출력
./build/benchmarks/network_benchmarks --benchmark_format=json

# CSV 출력
./build/benchmarks/network_benchmarks --benchmark_format=csv

# 파일로 저장
./build/benchmarks/network_benchmarks --benchmark_format=json --benchmark_out=results.json
```

### 고급 옵션

```bash
# 최소 시간 동안 실행 (안정적인 결과를 위해)
./build/benchmarks/network_benchmarks --benchmark_min_time=5.0

# 반복 횟수 지정
./build/benchmarks/network_benchmarks --benchmark_repetitions=10

# 모든 통계 표시
./build/benchmarks/network_benchmarks --benchmark_report_aggregates_only=false
```

## 벤치마크 카테고리

### 1. 메시지 처리량 벤치마크

**파일**: `message_throughput_bench.cpp`

메시지 처리 성능을 측정합니다:

- 메시지 생성 (작은 크기 64B, 중간 크기 1KB, 큰 크기 64KB)
- 메시지 복사 및 이동 (64B ~ 64KB)
- 메시지 직렬화/역직렬화
- 메시지 큐 작업
- 버스트 메시지 처리 (10, 100, 1000개 메시지)
- 지속적 처리량 측정
- 동시 메시지 처리 (1, 4, 8 스레드)

**목표 지표**:
- 메시지 생성 (1KB): < 1μs
- 메시지 복사 (1KB): < 500ns
- 메시지 이동: < 100ns
- 직렬화 처리량: > 1 GB/s
- 지속적 처리량: > 100k messages/sec

### 2. 연결 벤치마크

**파일**: `connection_bench.cpp`

연결 관리 성능을 측정합니다:

- 연결 생성 오버헤드
- 연결 설정 지연 시간
- 연결 수명 주기 (연결 + 연결 해제)
- 연결 풀 생성 (5, 10, 20, 50개 연결)
- 연결 풀 전체 연결 작업
- 전송 작업 (64B ~ 64KB 메시지)
- 버스트 전송 (10, 100, 1000개 메시지)
- 연결 상태 확인
- 동시 연결 작업 (4, 8, 16 스레드)
- 연결 재사용 효율성

**목표 지표**:
- 연결 생성: < 1μs
- 연결 설정: < 100μs
- 전송 지연 시간 (1KB): < 10μs
- 연결 풀 (20개 연결): < 1ms
- 동시 확장성: 8 스레드까지 선형

### 3. 세션 관리 벤치마크

**파일**: `session_bench.cpp`

세션 관리 성능을 측정합니다:

- 세션 생성
- 세션 생성/소멸 주기
- 세션 데이터 저장 및 검색
- 세션 조회
- 다수 세션 (10, 100, 1000개)
- 다수 세션에서 세션 조회
- 세션 정리/만료
- 세션 활동 업데이트
- 동시 세션 액세스 (4, 8, 16 스레드)

**목표 지표**:
- 세션 생성: < 1μs
- 세션 조회: < 100ns
- 데이터 저장: < 500ns
- 정리 (100개 세션): < 100μs
- 동시 조회: 잠금 없음 또는 최소 경합

### 4. TCP 수신 디스패치 벤치마크

**파일**: `tcp_receive_bench.cpp`

span 기반과 vector 기반 수신 디스패치 간의 오버헤드 차이를 측정합니다:

- Span 디스패치 (할당 없음, 64B ~ 64KB)
- Vector 폴백 (반복당 할당, 64B ~ 64KB)
- 멀티 콜백 span 공유 (3개 핸들러)
- 멀티 콜백 vector 복사 (3개 핸들러)
- Subspan 작업 (헤더/페이로드 파싱)
- Vector 슬라이스 작업 (레거시 패턴)

**목표 지표**:
- Span 디스패치: vector 폴백보다 10-50배 빠름
- Span 64B: < 1ns
- Span 64KB: < 300ns
- Span 경로에서 읽기당 할당 없음
- 프로토콜 파싱을 위한 효율적인 subspan 작업

## 기준 결과

**측정 예정**: 벤치마크를 실행하고 결과를 `docs/BASELINE.md`에 기록

예상 기준 범위 (확인 필요):

| 지표 | 목표 | 허용 가능 |
|--------|--------|------------|
| 메시지 생성 (1KB) | < 1μs | < 10μs |
| 메시지 직렬화 (1KB) | < 500ns | < 5μs |
| 연결 생성 | < 1μs | < 10μs |
| 연결 전송 (1KB) | < 10μs | < 100μs |
| 세션 생성 | < 1μs | < 10μs |
| 세션 조회 | < 100ns | < 1μs |
| 처리량 (1KB 메시지) | > 100k/s | > 50k/s |

## 결과 해석

### 벤치마크 출력 이해

```
---------------------------------------------------------------
Benchmark                         Time           CPU Iterations
---------------------------------------------------------------
BM_Message_Create_Medium        456 ns        455 ns    1534891
```

- **Time**: 반복당 벽시계 시간
- **CPU**: 반복당 CPU 시간
- **Iterations**: 벤치마크가 실행된 횟수

### 처리량 지표

많은 벤치마크가 처리량을 보고합니다:

```
BM_Message_Throughput    1.23 GB/s    100k items/s
```

- **Bytes/s**: 데이터 처리 속도
- **Items/s**: 메시지/작업 처리 속도

### 비교

이전/이후 성능을 비교하려면:

```bash
# 기준
./build/benchmarks/network_benchmarks --benchmark_out=baseline.json --benchmark_out_format=json

# 변경 후
./build/benchmarks/network_benchmarks --benchmark_out=after.json --benchmark_out_format=json

# 비교 (벤치마크 도구 필요)
compare.py baseline.json after.json
```

## CI 통합

벤치마크는 CI에서 모든 PR에 대해 실행되어 성능 저하를 감지합니다.

구성은 `.github/workflows/benchmarks.yml`을 참조하세요.

## 문제 해결

### Google Benchmark를 찾을 수 없음

```bash
# 설치 확인
find /usr -name "*benchmark*" 2>/dev/null

# pkg-config 시도
pkg-config --modversion benchmark

# 재설치
brew reinstall google-benchmark  # macOS
```

### 빌드 오류

```bash
# 깨끗한 빌드
rm -rf build
cmake -B build -S . -DNETWORK_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

### 불안정한 결과

```bash
# 최소 시간 증가
./build/benchmarks/network_benchmarks --benchmark_min_time=10.0

# CPU 주파수 스케일링 비활성화 (Linux)
sudo cpupower frequency-set --governor performance
```

## 기여하기

새 벤치마크를 추가할 때:

1. 기존 명명 규칙을 따르세요 (`BM_Category_SpecificTest`)
2. 적절한 벤치마크 유형을 사용하세요 (Fixture, Threaded 등)
3. 의미 있는 레이블과 카운터를 설정하세요 (SetBytesProcessed, SetItemsProcessed)
4. 파일 헤더에 목표 지표를 문서화하세요
5. TearDown/벤치마크 후 리소스를 정리하세요
6. 새 벤치마크 설명으로 이 README를 업데이트하세요

## 참고 자료

- [Google Benchmark 문서](https://github.com/google/benchmark)
- [벤치마크 모범 사례](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [network_system 아키텍처](../README.md)

---

**최종 업데이트**: 2025-10-07
**Phase**: 0 - 기초 및 도구
**상태**: 기준 측정 진행 중
