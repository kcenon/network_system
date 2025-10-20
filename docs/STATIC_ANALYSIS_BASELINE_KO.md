# Static Analysis Baseline - network_system

> **Language:** [English](STATIC_ANALYSIS_BASELINE.md) | **한국어**

**날짜**: 2025-10-03
**버전**: 1.0.0
**도구 버전**:
- clang-tidy: 18.x
- cppcheck: 2.x

## 개요

이 문서는 network_system의 정적 분석 경고에 대한 기준선을 설정합니다.
목표는 시간 경과에 따른 개선을 추적하고 퇴보를 방지하는 것입니다.

## Clang-Tidy 기준선

### 구성
- 활성화된 검사: modernize-*, concurrency-*, performance-*, bugprone-*, cert-*, cppcoreguidelines-*
- 표준: C++20
- 구성 파일: .clang-tidy

### 초기 기준선 (Phase 0)

**총 경고**: TBD
실행: `clang-tidy -p build/compile_commands.json <source_files>`

**경고 분포**:
- modernize-*: TBD
- performance-*: TBD
- concurrency-*: TBD
- readability-*: TBD
- bugprone-*: TBD

### 주목할 억제

전체 억제된 검사 목록은 .clang-tidy를 참조하세요.

## Cppcheck 기준선

### 구성
- 프로젝트 파일: .cppcheck
- 활성화: 모든 검사
- 표준: C++20

### 초기 기준선 (Phase 0)

**총 이슈**: TBD
실행: `cppcheck --project=.cppcheck --enable=all`

**이슈 분포**:
- Error: TBD
- Warning: TBD
- Style: TBD
- Performance: TBD

### 주목할 억제

전체 억제 목록은 .cppcheck를 참조하세요.

## 목표 달성

**Phase 1 목표** (2025-11-01까지):
- clang-tidy: 0 errors, < 20 warnings
- cppcheck: 0 errors, < 10 warnings

**Phase 2 목표** (2025-12-01까지):
- clang-tidy: < 10 warnings
- cppcheck: < 5 warnings

**Phase 3 목표** (2026-01-01까지):
- 모든 경고 해결 또는 명시적으로 문서화

## 분석 실행 방법

### Clang-Tidy
```bash
# 컴파일 명령 생성
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# clang-tidy 실행
clang-tidy -p build <source_files>

# 또는 모든 파일 검사
find src include -name "*.cpp" -o -name "*.h" | xargs clang-tidy -p build
```

### Cppcheck
```bash
# 프로젝트 구성 사용
cppcheck --project=.cppcheck --enable=all --xml 2> cppcheck.xml

# HTML 보고서 생성
cppcheck-htmlreport --file=cppcheck.xml --report-dir=build/cppcheck-report
```

## 변경 사항 추적

경고 증가는 정당성과 함께 여기에 문서화되어야 합니다:

| 날짜 | 도구 | 변경 | 이유 | 해결됨 |
|------|------|--------|--------|----------|
| 2025-10-03 | clang-tidy | 초기 기준선 | Phase 0 설정 | N/A |
| 2025-10-03 | cppcheck | 초기 기준선 | Phase 0 설정 | N/A |

## 참고 사항

- 기준선은 Phase 1에서 초기 경고를 수정한 후 업데이트될 예정
- 목표는 지속적인 개선 및 제로 퇴보
- 모든 새 코드는 정적 분석 검사를 통과해야 함

---

*Last Updated: 2025-10-20*
