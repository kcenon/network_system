# 빌드 지침

> **Language:** [English](BUILD.md) | **한국어**

## 목차
- [사전 요구 사항](#사전-요구-사항)
- [빠른 빌드](#빠른-빌드)
- [플랫폼별 지침](#플랫폼별-지침)
- [빌드 옵션](#빌드-옵션)
- [문제 해결](#문제-해결)

## 사전 요구 사항

### 최소 요구 사항
- C++20 호환 컴파일러
  - GCC 11 이상
  - Clang 14 이상
  - MSVC 2022 이상
- CMake 3.16 이상
- ASIO 라이브러리 또는 Boost.ASIO 1.28+

### 선택적 의존성
- fmt 라이브러리 10.0+ (사용 불가능한 경우 std::format으로 폴백)
- Google Test (단위 테스트용)
- Google Benchmark (성능 테스트용)
- Ninja 빌드 시스템 (권장)

## 빠른 빌드

```bash
# 저장소 클론
git clone https://github.com/kcenon/network_system.git
cd network_system

# 빌드 디렉터리 생성
mkdir build && cd build

# 구성
cmake .. -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build . --parallel

# 설치 검증
./verify_build
```

## 플랫폼별 지침

### Ubuntu/Debian

#### 의존성 설치
```bash
# 패키지 목록 업데이트
sudo apt update

# 빌드 필수 요소 설치
sudo apt install -y build-essential cmake ninja-build

# 필수 라이브러리 설치
sudo apt install -y libasio-dev libfmt-dev

# 선택 사항: Boost 설치 (Boost.ASIO 폴백용)
sudo apt install -y libboost-all-dev

# 선택 사항: 테스트 프레임워크 설치
sudo apt install -y libgtest-dev libbenchmark-dev
```

#### 빌드
```bash
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_SAMPLES=ON
ninja
```

### macOS

#### 의존성 설치
```bash
# 아직 설치되지 않은 경우 Homebrew 설치
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 의존성 설치
brew install cmake ninja asio fmt boost

# 선택 사항: 테스트 프레임워크 설치
brew install googletest google-benchmark
```

#### 빌드
```bash
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_SAMPLES=ON
ninja
```

### Windows (Visual Studio)

#### 의존성 설치

##### 옵션 1: vcpkg (권장)
```powershell
# vcpkg 클론
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 의존성 설치
.\vcpkg install asio fmt --triplet x64-windows
```

##### 옵션 2: 수동 설치
- Boost를 https://www.boost.org/ 에서 다운로드
- 압축 해제 후 시스템 PATH에 추가

#### 빌드
```powershell
# 빌드 디렉터리 생성
mkdir build
cd build

# vcpkg로 구성
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="path\to\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release

# 빌드
cmake --build . --config Release
```

### Windows (MSYS2/MinGW)

#### 의존성 설치
```bash
# MSYS2 업데이트
pacman -Syu

# 빌드 도구 설치
pacman -S mingw-w64-x86_64-toolchain
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-ninja

# 라이브러리 설치
pacman -S mingw-w64-x86_64-asio
pacman -S mingw-w64-x86_64-fmt
pacman -S mingw-w64-x86_64-boost
```

#### 빌드
```bash
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_SAMPLES=ON
ninja
```

## 빌드 옵션

### CMake 옵션

| 옵션 | 기본값 | 설명 |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | OFF | 공유 라이브러리로 빌드 |
| `BUILD_TESTS` | ON | 단위 테스트 빌드 |
| `BUILD_SAMPLES` | ON | 샘플 애플리케이션 빌드 |
| `BUILD_WITH_CONTAINER_SYSTEM` | ON | container_system 통합 활성화 |
| `BUILD_WITH_THREAD_SYSTEM` | ON | thread_system 통합 활성화 |
| `BUILD_MESSAGING_BRIDGE` | ON | messaging_system 호환성 브리지 빌드 |
| `CMAKE_BUILD_TYPE` | Debug | 빌드 구성 (Debug/Release/RelWithDebInfo/MinSizeRel) |

### 예제 구성

#### 최소 빌드
```bash
cmake .. -DBUILD_TESTS=OFF -DBUILD_SAMPLES=OFF
```

#### 전체 기능 빌드
```bash
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_SAMPLES=ON \
  -DBUILD_WITH_CONTAINER_SYSTEM=ON \
  -DBUILD_WITH_THREAD_SYSTEM=ON \
  -DBUILD_MESSAGING_BRIDGE=ON
```

#### 디버그 빌드
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=address"
```

## 빌드 구성 표준

### C++ 표준

network_system은 특정 기능을 갖춘 **C++20이 필요**합니다:
- Concepts 및 constraints
- Coroutines (비동기 작업용)
- 표준 라이브러리 기능 (`std::span`, `std::format`)
- 3방향 비교 연산자 (`<=>`)

**CMake 구성**:
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

### 컴파일 명령 내보내기

IDE 통합 및 정적 분석 도구용:
```bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

다음을 위해 `compile_commands.json`을 생성합니다:
- CLion, VS Code 및 기타 IDE
- Clang-Tidy 및 기타 정적 분석기
- 코드 완성 및 탐색 도구

### 커버리지 지원

코드 커버리지 계측으로 빌드하려면:
```bash
cmake .. -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest
# build/coverage/에 커버리지 보고서 생성
```

## 정적 분석

### Clang-Tidy

network_system에는 다음에 대한 검사가 포함된 `.clang-tidy` 구성이 있습니다:
- **modernize**: 현대적인 C++ 관용구 강제
- **concurrency**: 스레드 안전성 및 데이터 경합 감지
- **performance**: 성능 안티 패턴
- **bugprone**: 일반적인 프로그래밍 오류
- **cert**: CERT C++ 코딩 표준
- **cppcoreguidelines**: C++ Core Guidelines 준수

**Clang-Tidy 실행**:
```bash
# 특정 파일 분석
clang-tidy -p build $(find src include -name "*.cpp" -o -name "*.h")

# 자동 수정으로 분석
clang-tidy -p build --fix-errors src/network_system/core/*.cpp

# CMake 통합 (활성화된 경우)
cmake .. -DCMAKE_CXX_CLANG_TIDY="clang-tidy;-checks=*"
```

**경고 억제**:
구성된 억제 및 검사 사용자 정의는 `.clang-tidy` 파일을 참조하세요.

### Cppcheck

network_system에는 다음을 포함하는 `.cppcheck` 구성이 있습니다:
- 프로젝트 기반 파일 선택
- Thread safety 애드온
- CERT 보안 애드온
- 성능 분석

**Cppcheck 실행**:
```bash
# 전체 분석
cppcheck --project=.cppcheck --enable=all

# 특정 디렉토리
cppcheck --enable=all src/network_system/core/

# 특정 애드온 사용
cppcheck --addon=threadsafety --addon=cert src/
```

**구성 파일** (`.cppcheck`):
```xml
<?xml version="1.0"?>
<project>
    <paths>
        <dir name="src"/>
        <dir name="include"/>
    </paths>
    <exclude>
        <path name="build/"/>
        <path name="tests/"/>
    </exclude>
</project>
```

### CI/CD와의 통합

정적 분석은 다음에서 자동으로 실행됩니다:
- 모든 커밋 (GitHub Actions)
- 풀 리퀘스트 (중요한 문제가 발견되면 차단)
- Pre-commit 훅 (선택사항)

**Pre-commit 훅 설정**:
```bash
# Pre-commit 훅 설치
cp scripts/pre-commit .git/hooks/
chmod +x .git/hooks/pre-commit
```

## 통합

### 프로젝트에서 NetworkSystem 사용

#### CMake 통합
```cmake
# 패키지 찾기
find_package(NetworkSystem REQUIRED)

# 타겟에 링크
target_link_libraries(your_target
  PRIVATE
    NetworkSystem::NetworkSystem
)
```

#### 수동 통합
```cmake
# include 디렉터리 추가
target_include_directories(your_target
  PRIVATE
    /path/to/network_system/include
)

# 라이브러리 링크
target_link_libraries(your_target
  PRIVATE
    /path/to/network_system/build/libNetworkSystem.a
)
```

## 문제 해결

### 일반적인 문제

#### ASIO를 찾을 수 없음
```
CMake Error: ASIO not found in standard locations
```

**해결책**: ASIO 또는 Boost 설치:
```bash
# Ubuntu/Debian
sudo apt install libasio-dev libboost-all-dev

# macOS
brew install asio boost

# Windows (vcpkg)
vcpkg install asio boost
```

#### Windows에서 pthread를 찾을 수 없음
```
LINK : fatal error LNK1104: cannot open file 'pthread.lib'
```

**해결책**: CMakeLists.txt에서 이미 수정되었습니다. pthread는 Unix 시스템에서만 링크됩니다.

#### vcpkg 빌드 실패
```
Error: run-vcpkg action execution failed
```

**해결책**: 빌드 시스템이 자동으로 시스템 라이브러리로 폴백합니다. 시스템 패키지 관리자를 통해 ASIO가 설치되었는지 확인하세요.

#### C++20 기능을 사용할 수 없음
```
error: 'jthread' is not a member of 'std'
```

**해결책**: 컴파일러 업데이트:
```bash
# Ubuntu/Debian
sudo apt install g++-11

# 기본값으로 설정
sudo update-alternatives --config g++
```

### 디버그 빌드 문제

빌드 문제 진단을 위해 자세한 출력 활성화:
```bash
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
cmake --build . --verbose
```

### 클린 빌드

지속적인 문제가 발생하면 클린 빌드 시도:
```bash
rm -rf build
mkdir build && cd build
cmake .. -G Ninja
ninja
```

## 성능 튜닝

### 컴파일러 최적화

최대 성능을 위해:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
```

### 링크 타임 최적화 (LTO)
```bash
cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

## 테스트

### 테스트 실행
```bash
# 모든 테스트 실행
ctest --output-on-failure

# 특정 테스트 실행
./tests/unit_tests

# 자세한 출력으로 실행
ctest -V
```

### 벤치마크 실행
```bash
./tests/benchmark_tests
```

## 지원

빌드 문제가 있는 경우 다음을 확인하세요:
1. [GitHub Issues](https://github.com/kcenon/network_system/issues)
2. 알려진 문제에 대한 [CHANGELOG.md](../CHANGELOG.md)
3. 연락처: kcenon@naver.com

---

*Last Updated: 2025-10-20*
