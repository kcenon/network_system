---
doc_id: "NET-GUID-015"
doc_title: "Build Instructions"
doc_version: "1.1.0"
doc_date: "2026-10-11"
doc_status: "Released"
project: "network_system"
category: "GUID"
---

# Build Instructions

[한국어](BUILD.kr.md)

These instructions target current main. Published release behavior and the historical
manifest mismatch are described in the [release policy](../../ci/RELEASE_POLICY.md#published-versions-and-historical-erratum).

## Requirements

| Dependency | Version | Required | Description |
|------------|---------|----------|-------------|
| C++20 Compiler | GCC 13+ / Clang 17+ / MSVC 2022+ / Apple Clang 14+ | Yes | Higher requirements due to thread_system dependency |
| CMake | 3.20+ | Yes | Build system |
| ASIO | 1.30.2+ | Yes | Asynchronous I/O (standalone) |
| OpenSSL | 3.0+ | Yes | TLS/SSL support |
| [common_system](https://github.com/kcenon/common_system) | latest | Yes | Common interfaces and Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | latest | Yes | Thread pool and async operations |
| [logger_system](https://github.com/kcenon/logger_system) | latest | Optional | Default OFF; runtime binding through common_system ILogger |
| [container_system](https://github.com/kcenon/container_system) | latest | Optional | CMake default ON; disabled if the package is unavailable |

> **OpenSSL note**: The default TLS build requires OpenSSL 3.0 or later.

## System packages

These commands install system libraries. Check that distribution package versions meet the requirements; provide the C++ compiler and common_system/thread_system/ASIO CMake packages separately. LZ4 is optional.

**Ubuntu/Debian** (Ubuntu 22.04+ provides OpenSSL 3.x by default):
```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libssl-dev liblz4-dev zlib1g-dev
```

**macOS** (OpenSSL 3.x via Homebrew):
```bash
brew install cmake ninja asio openssl@3 lz4 zlib
```

**Windows (vcpkg)**:
```powershell
# vcpkg provides OpenSSL 3.x
vcpkg install openssl asio lz4 zlib --triplet x64-windows
```

**Windows (MSYS2)**:
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-openssl
```

## Source installation

Before building, install the CMake packages for `common_system`, `thread_system`, and ASIO, plus OpenSSL/zlib. Replace `CMAKE_PREFIX_PATH` with their actual installation prefixes. Cloning dependency repositories alone does not build their static libraries. This recipe disables optional logger/container integration.

```bash
git clone --branch main https://github.com/kcenon/network_system.git
cd network_system
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix" \
  -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build build --parallel
cmake --install build --prefix "$HOME/.local"
```

The prefix supplied to `cmake --install` is the network_system installation prefix.
Add it alongside dependency prefixes when configuring a consumer. The exported
target is `network_system::network_system`; use it instead of adding private include directories.
See the [README consumer](../../README.md#getting-started) and [paired examples](QUICK_START.md).

## Presets and tests

CMake presets provide standardized build configurations:

Reading preset schema 6 requires CMake 3.25 or later. `asan`, `tsan`, and `ubsan` have matching test presets. `release` and `coverage` do not; run CTest from their build directory.

```bash
cmake --list-presets
# Choose one configure/build preset: debug, release, asan, tsan, ubsan, coverage.
# Substitute the same name in both commands.
cmake --preset debug -DBUILD_EXAMPLES=OFF \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix" \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build --preset debug
ctest --preset debug
```

### Manual CMake Configuration

```bash
# Configure a separate directory for each sanitizer/profile.
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="/path/to/dependency/prefix" \
  -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF \
  -DKCENON_WITH_LOGGER_SYSTEM=OFF -DKCENON_WITH_CONTAINER_SYSTEM=OFF
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure --timeout 120
```

## Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_EXAMPLES` | ON | Build usage examples |
| `BUILD_TLS_SUPPORT` | ON | Enable TLS/SSL support |
| `BUILD_WEBSOCKET_SUPPORT` | ON | Enable WebSocket protocol |
| `NETWORK_BUILD_BENCHMARKS` | OFF | Build performance benchmarks |
| `ENABLE_ASAN` | OFF | Enable AddressSanitizer |
| `ENABLE_TSAN` | OFF | Enable ThreadSanitizer |
| `ENABLE_UBSAN` | OFF | Enable UndefinedBehaviorSanitizer |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |
| `NETWORK_ENABLE_GRPC_OFFICIAL` | OFF | Use official gRPC library |

## Selected examples

These targets can be built individually from the current source:

| Source | CMake target |
|--------|--------------|
| [concepts_example.cpp](../../examples/concepts_example.cpp) | `example_concepts_example` |
| [thread_integration_example.cpp](../../examples/thread_integration_example.cpp) | `example_thread_integration_example` |
| [tls_policy_example.cpp](../../examples/tls_policy_example.cpp) | `example_tls_policy_example` |
| [http2_server_example.cpp](../../examples/http2_server_example.cpp) | `example_http2_server_example` |

Some other programs under `examples/` still use earlier APIs; select the target you need instead of building every example. The commands below reuse the source build directory with dependencies configured above.

```bash
cmake -S . -B build -DBUILD_EXAMPLES=ON
cmake --build build --target example_thread_integration_example
./build/bin/examples/example_thread_integration_example
```

## Experimental modules

`NETWORK_BUILD_MODULES` enables a separate experimental module target. It requires CMake 3.28 or later and a module-capable toolchain; its interface is not interchangeable with the header API. See [src/modules](../../src/modules/) for the current interfaces. The quick-start examples use the header API.

## Generated API reference

Run `doxygen Doxyfile` from the repository root; output is `documents/html/index.html`.
The [documentation workflow](../../.github/workflows/build-Doxygen.yaml) publishes
[API documentation](https://kcenon.github.io/network_system/).
The [warnings workflow](../../.github/workflows/doxygen-warnings-check.yml) checks relevant
header/Doxyfile/workflow PR changes and pushes to main, develop, and phase-* branches.

## Troubleshooting

If CMake cannot locate common_system, thread_system, or ASIO, install their CMake packages
and correct `CMAKE_PREFIX_PATH`. An adjacent clone alone is insufficient.
Use separate build directories for different compilers and sanitizer profiles.
Consult [troubleshooting](TROUBLESHOOTING.md) or [GitHub Issues](https://github.com/kcenon/network_system/issues).
