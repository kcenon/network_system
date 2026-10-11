# Network System

Status: active | Published release: [v0.1.1](https://github.com/kcenon/network_system/releases/tag/v0.1.1)

## Overview

C++20 asynchronous networking with standalone ASIO: TCP, UDP, TLS, WebSocket,
HTTP, QUIC, and gRPC interfaces for servers and clients.

> **Language:** **English** | [한국어](README.kr.md)

[![CI](https://github.com/kcenon/network_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/ci.yml)
[![Code Quality](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/code-quality.yml)
[![Coverage](https://github.com/kcenon/network_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/coverage.yml)
[![Documentation](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/network_system/actions/workflows/build-Doxygen.yaml)

The project is pre-1.0. The current API is a v1.0 candidate; v1.0.0 has not been
published. See the [API surface](docs/v1.0-api-surface.md) and
[release policy and historical manifest erratum](ci/RELEASE_POLICY.md#published-versions-and-historical-erratum).
The v0.1.1 tag has CMake version 0.1.1 but vcpkg manifest version 0.1.0.

## Contents

- [Requirements](#requirements)
- [Getting started](#getting-started)
- [Documentation](#documentation)
- [Development and support](#development-and-support)

## Requirements

| Dependency | Requirement |
|------------|-------------|
| C++20 compiler | GCC 13+, Clang 17+, MSVC 2022+, or Apple Clang 14+ |
| CMake | 3.20+; 3.25+ for presets |
| Standalone ASIO | 1.30.2+; Boost.ASIO is not a substitute |
| OpenSSL / zlib | OpenSSL 3.0+ and zlib |
| [common_system](https://github.com/kcenon/common_system), [thread_system](https://github.com/kcenon/thread_system) | Installed CMake packages |
| logger_system / container_system | Optional; logger defaults OFF, container discovery defaults ON and disables integration when unavailable |

## Getting Started

For the published release, use the [vcpkg registry](https://github.com/kcenon/vcpkg-registry):

```bash
vcpkg install kcenon-network-system \
  --overlay-ports=path/to/kcenon/vcpkg-registry/ports
# Optional logger integration
vcpkg install "kcenon-network-system[logging]" \
  --overlay-ports=path/to/kcenon/vcpkg-registry/ports
```

The consumer port offers `logging`; root development-manifest features such as
`ssl`, `ecosystem`, and `testing` are not registry consumer features.

The following example targets **current main installed from source**. Follow
the [build guide](docs/guides/BUILD.md) to install network_system and its
dependencies first; cloning dependencies alone does not build their libraries.
Do not assume the published tag contains every current API.

Save this as `main.cpp`:

```cpp
#include <kcenon/network/facade/tcp_facade.h>
#include <iostream>

int main() {
    kcenon::network::facade::tcp_facade tcp;
    auto created = tcp.create_server({.port = 9090});
    if (created.is_err()) {
        std::cerr << created.error().message << '\n';
        return 1;
    }
    auto server = created.value();
    auto started = server->start(9090);
    if (started.is_err()) {
        std::cerr << started.error().message << '\n';
        return 1;
    }
    std::cout << "Listening on port 9090; press Enter to stop." << std::endl;
    std::cin.get();
    auto stopped = server->stop();
    return stopped.is_err() ? 1 : 0;
}
```

Save this as `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(readme_example LANGUAGES CXX)
find_package(network_system CONFIG REQUIRED)
add_executable(readme_example main.cpp)
target_compile_features(readme_example PRIVATE cxx_std_20)
target_link_libraries(readme_example PRIVATE network_system::network_system)
```

Configure against the installation prefixes, then start the server:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/network/install;/path/to/dependency/prefix"
cmake --build build --parallel
./build/readme_example
```

For Visual Studio builds, use `--config Debug` and run `build/Debug/readme_example.exe`.
The server listens on port 9090 and stops when you press Enter.
See the [client/server guide](docs/guides/QUICK_START.md) for a paired example.

## Documentation

| Topic | Guide |
|-------|-------|
| Installation, presets, selected examples | [Build guide](docs/guides/BUILD.md) |
| Client readiness and receiving data | [Quick start](docs/guides/QUICK_START.md) |
| Protocol targets, tracing, errors, thread pools | [Usage examples](docs/guides/USAGE_EXAMPLES.md) |
| Supported behavior and implementation gaps | [Protocol support matrix](docs/PROTOCOL_SUPPORT.md) |
| API reference | [Generated API documentation](https://kcenon.github.io/network_system/) |
| Design and migration | [Architecture](docs/ARCHITECTURE.md), [migration](docs/MIGRATION.md) |
| Historical measurements and evidence limits | [Benchmarks](docs/BENCHMARKS.md) |
| TLS and operations | [TLS setup](docs/guides/TLS_SETUP_GUIDE.md), [operations](docs/advanced/OPERATIONS.md) |

QUIC implements parts of RFC 9000/9001/9002; consult the support matrix for
recovery/statistics limitations. DTLS and some tracing exporters remain experimental.

## Development and Support

Builds run on Linux, macOS, and Windows in [CI](.github/workflows/ci.yml).
Unix jobs run CTest; the Windows job builds without running CTest.
Sanitizer jobs run with configured exclusions and suppressions, including
[TSan](tsan_suppressions.txt), [LSan](sanitizers/lsan_suppressions.txt), and
[UBSan](cmake/ubsan.supp). The sanitizer jobs are advisory; passing them does
not establish the absence of defects. LSan suppressions include project code.
[Coverage reports](https://codecov.io/gh/kcenon/network_system) describe individual runs.

Some repository examples still use earlier APIs. The build guide lists selected
validated targets; do not assume a build of every example succeeds.

See [contributing](docs/contributing/CONTRIBUTING.md), [.clang-format](.clang-format),
and the [README editing policy](docs/contributing/README_POLICY.md).
Use [GitHub Issues](https://github.com/kcenon/network_system/issues) for bugs and questions.
Release readiness is tracked in [#964](https://github.com/kcenon/network_system/issues/964)
and [#1165](https://github.com/kcenon/network_system/issues/1165).

## License

Licensed under the [BSD 3-Clause License](LICENSE).
