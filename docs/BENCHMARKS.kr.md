---
doc_id: "NET-PERF-032"
doc_title: "네트워크 벤치마크 근거"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "PERF"
---

# 네트워크 벤치마크 근거

[English](BENCHMARKS.md)

문서 검토 날짜: 2026-10-11. 측정 날짜가 아닙니다.
이번 문서 변경에서는 벤치마크를 다시 실행하지 않았습니다.

## 근거 상태

벤치마크 소스는 결과 파일이 아니며, 워크플로 배지만으로 측정 환경이나 성능 기준 달성을 확인할 수 없습니다.
이 문서는 저장소에 남아 있는 기록과 그 한계를 구분합니다.

| 기록 | 확인 가능한 근거 | 한계 |
|------|------------------|------|
| 아래 과거 메시지 표 | 과거 영문 문서와 메시지 벤치마크 소스 | 원래 실행 날짜, 정확한 명령, 커밋, 원본 출력 없음 |
| 모의 연결/세션 연산 | [연결 소스](../benchmarks/connection_bench.cpp), [세션 소스](../benchmarks/session_bench.cpp) | 모의 객체이므로 소켓 처리량을 나타내지 않음 |
| TCP 루프백 | [소스](../benchmarks/tcp_loopback_bench.cpp) | 실제 소켓 I/O; 이 문서에는 대응하는 원본 결과 파일 없음 |
| 파사드와 직접 API 비교 | [소스](../benchmarks/facade_vs_direct_bench.cpp) | 비교 코드의 존재만으로 동일한 성능을 입증할 수 없음 |
| 이전 한국어 TCP, 확장성, Boost.Asio/libuv 비교 표 | 과거 문서 | 출처가 확인되는 원본 결과가 없어 해당 수치 제거 |

## 합성 벤치마크

### 검증되지 않은 과거 요약

아래 수치는 [이전 영문 문서](https://github.com/kcenon/network_system/blob/0a815e75631a55f8a8be54567e612d89fecb78ff/docs/BENCHMARKS.md#synthetic-benchmarks)에서 옮긴 기록입니다.
현재 성능이나 네트워크 처리량을 검증한 결과가 아닙니다. 이번 검토에서 확인한 저장소 기록으로는
원래 측정의 출처를 복원할 수 없습니다.

| 메타데이터 | 과거 기록 |
|------------|-----------|
| 기록된 하드웨어 | Intel i7-12700K 3.2 GHz, 32 GB DDR4-3200, NVMe SSD |
| 기록된 소프트웨어 | Ubuntu 22.04 LTS, GCC 11.3, C++20, Release 및 -O3 |
| 기록된 범위 | CPU 할당/복사 연산; 소켓 I/O 없음 |
| 측정 날짜 | 알 수 없음; 이전 페이지의 2025-11-15와 2026-04-04는 문서 날짜 |
| 원래 명령 / 소스 리비전 | 기록되지 않음 |
| 원본 결과 파일 | 찾지 못함; 과거 Markdown 표는 요약이며 원본 출력이 아님 |
| 현재 소스 | [message_throughput_bench.cpp](../benchmarks/message_throughput_bench.cpp) |

| 과거 페이로드 표기 | 기록된 연산 시간 (ns) | 기록된 처리량 (msg/s) |
|--------------------|-----------------------|-----------------------|
| 64 bytes | 1,300 | ~769,000 |
| 256 bytes | 3,270 | ~305,000 |
| 1 KB | 7,803 | ~128,000 |
| 8 KB | 48,000 | ~21,000 |

현재 등록된 벤치마크 사례가 과거 표의 모든 페이로드와 대응하지는 않습니다. 특히 복사/직렬화 사례의
크기 집합이 다르므로 현재 코드가 이 표를 재현한다고 추정하지 마세요. GCC 11.3은 과거 기록의
컴파일러이며, 현재 요구사항은 [빌드 가이드](guides/BUILD.kr.md)를 확인하세요.
근거가 없는 Apple M1 플랫폼 문구는 제거했습니다.

### 연결 및 세션 벤치마크

연결 소스는 모의 연산과 지연을 사용하고, 세션 소스는 모의 객체의 관리 연산을 측정합니다.
원본 실행 기록이 없는 수치 요약은 제거했습니다. 이 코드로 TCP 핸드셰이크나 종단 간 네트워크 성능을 입증할 수 없습니다.

## 실제 네트워크 부하 테스트

TCP 루프백 소스는 소켓을 이용한 연결, 에코 왕복, 스트림 전송을 포함합니다.
CPU 전용 벤치마크와 측정 범위가 다릅니다. [통합 테스트](../integration_tests/)도 네트워크 동작을
확인하지만, 테스트를 실행했다는 사실만으로 출처가 확인되는 성능 결과가 되지는 않습니다.

## 성능 비교

파사드/직접 API의 성능이 동일하다거나 다른 라이브러리보다 우수하다는 결론을 제시하지 않습니다.
비교하려면 같은 환경, 리비전, 워크로드 조건과 양쪽의 원본 출력이 필요합니다.

## 측정 방법

새 측정값에는 소스 커밋, 측정 날짜, CPU/OS/컴파일러, 빌드 플래그, 의존성 버전, 페이로드와 연결 수,
정확한 명령, 반복 및 집계 방법, 보존 가능한 원본 결과 링크를 기록하세요.
CPU 전용, 모의 연산, 실제 I/O 중 어떤 범위인지 명시하고 문서 수정 날짜와 측정 날짜를 구분합니다.

## 새 결과 수집 절차

아래는 **향후 결과 수집을 위한 제안 명령**입니다. 과거 표를 생성한 명령이 아니며, 이번 문서 PR에서
실행한 절차도 아닙니다. 먼저 [소스 빌드 요구사항](guides/BUILD.kr.md)을 준비하세요.
Google Benchmark는 별도로 설치해야 합니다. [CMake 구성](../benchmarks/CMakeLists.txt)은 해당 패키지가
없으면 타깃 생성을 건너뛰며, 옵션을 켜는 것만으로 의존성이 설치되지 않습니다.

```bash
cmake -S . -B build/perf -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix;/path/to/google-benchmark/prefix" \
  -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF -DNETWORK_BUILD_BENCHMARKS=ON \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build build/perf --target network_benchmarks
./build/perf/benchmarks/network_benchmarks \
  --benchmark_filter='BM_Message_(Create|Copy|Serialize)' \
  --benchmark_repetitions=5 --benchmark_out_format=json \
  --benchmark_out=build/perf/message-results.json
```

위 결과 파일은 향후 실행이 성공해야 생성됩니다. 현재 측정 근거가 아닙니다.
실행 전에 소스와 툴체인 호환성을 확인하세요. 이번 변경은 벤치마크 구현을 수정하지 않습니다.
[벤치마크 워크플로](../.github/workflows/benchmarks.yml)는 실행별 아티팩트를 업로드하며 일부 네트워크 의존 사례를 제외합니다.
새 결과를 인용할 때는 워크플로의 일반 링크 대신 특정 실행과 실제 결과 아티팩트를 사용하세요.
