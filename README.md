# Network System

[![Build Status](https://github.com/kcenon/network_system/actions/workflows/build-ubuntu-gcc.yaml/badge.svg)](https://github.com/kcenon/network_system/actions)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)
[![C++ Version](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

**Status**: Phase 4 In Progress 🚧 (messaging_system Update)
**Version**: 2.0.0
**Owner**: kcenon (kcenon@naver.com)

High-performance asynchronous network library separated from messaging_system for modularity and reusability.

## 🎯 Project Overview

This project is a comprehensive refactoring effort to separate the network module from messaging_system into an independent, reusable library while maintaining full backward compatibility.

### Core Objectives
- **Module Independence**: Complete separation of network module from messaging_system ✅
- **Enhanced Reusability**: Independent library usable in other projects ✅
- **Compatibility Maintenance**: Full backward compatibility with legacy code ✅
- **Performance Optimization**: 305K+ msg/s throughput achieved ✅

## 🚀 Quick Start

### Prerequisites

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libfmt-dev
```

#### macOS
```bash
brew install cmake ninja asio fmt
```

#### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-fmt
```

### Build Instructions

```bash
# Clone repository
git clone https://github.com/kcenon/network_system.git
cd network_system

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build with optional integrations
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=ON \
    -DBUILD_WITH_LOGGER_SYSTEM=ON

# Build
cmake --build .

# Run tests
./verify_build
./benchmark
```

## 📝 API Examples

### Modern API Usage

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>("server_id");
server->start_server(8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>("client_id");
client->start_client("localhost", 8080);

// Send message
std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
client->send_packet(data);
```

### Legacy API Compatibility

```cpp
#include <network_system/compatibility.h>

// Use legacy namespace - fully compatible
auto server = network_module::create_server("legacy_server");
server->start_server(8080);

auto client = network_module::create_client("legacy_client");
client->start_client("127.0.0.1", 8080);
```

## 🏗️ Architecture

```
network_system/
├── include/network_system/
│   ├── core/                    # Core networking components
│   │   ├── messaging_client.h
│   │   └── messaging_server.h
│   ├── session/                 # Session management
│   │   └── messaging_session.h
│   ├── internal/                # Internal implementation
│   │   ├── tcp_socket.h
│   │   ├── pipeline.h
│   │   └── send_coroutine.h
│   ├── integration/             # External system integration
│   │   ├── messaging_bridge.h
│   │   ├── thread_integration.h
│   │   ├── container_integration.h
│   │   └── logger_integration.h
│   └── compatibility.h         # Legacy API support
├── src/                        # Implementation files
├── samples/                    # Usage examples
├── tests/                      # Test suites
└── docs/                       # Documentation
```

## 📊 Performance Benchmarks

### Latest Results (v2.0.0)

| Metric | Result | Test Conditions |
|--------|--------|-----------------|
| **Average Throughput** | 305,255 msg/s | Mixed message sizes |
| **Small Messages (64B)** | 769,230 msg/s | 10,000 messages |
| **Medium Messages (1KB)** | 128,205 msg/s | 5,000 messages |
| **Large Messages (8KB)** | 20,833 msg/s | 1,000 messages |
| **Concurrent Connections** | 50 clients | 12,195 msg/s |
| **Average Latency** | 584.22 μs | P50: < 50 μs |
| **Performance Rating** | 🏆 EXCELLENT | Production ready! |

### Key Performance Features
- Zero-copy message pipeline
- Lock-free data structures where possible
- Connection pooling
- Async I/O with ASIO
- C++20 coroutine support

## 🔧 Features

### Core Features
- ✅ Asynchronous TCP server/client
- ✅ Multi-threaded message processing
- ✅ Session lifecycle management
- ✅ Message pipeline with buffering
- ✅ C++20 coroutine support

### Integration Features
- ✅ Thread pool integration
- ✅ Container serialization support
- ✅ Logger system integration
- ✅ Legacy API compatibility layer
- ✅ Comprehensive test coverage
- ✅ Performance benchmarking suite

### Planned Features
- 🚧 WebSocket support
- 🚧 TLS/SSL encryption
- 🚧 HTTP/2 client
- 🚧 gRPC integration

## 📈 Project Phases

### Completed Phases
- **Phase 1**: Preparation and Analysis ✅ (2025-09-19)
  - Project structure setup
  - Build system configuration
  - CI/CD pipeline

- **Phase 2**: Core System Separation ✅ (2025-09-19)
  - Complete namespace separation
  - Directory restructuring
  - CMake integration

- **Phase 3**: Integration Interface ✅ (2025-09-20)
  - Thread system integration
  - Container system integration
  - Compatibility layer

### Current Phase
- **Phase 4**: messaging_system Update 🚧 (2025-09-20)
  - [x] Usage examples
  - [x] Compatibility tests
  - [x] Performance benchmarks
  - [x] Documentation update
  - [ ] Migration guide

### Upcoming Phase
- **Phase 5**: Verification and Deployment ⏳
  - Production testing
  - Performance optimization
  - Official release

## 🔧 Dependencies

### Required
- **C++20** compatible compiler
- **CMake** 3.16+
- **ASIO** or **Boost.ASIO** 1.28+

### Optional
- **fmt** 10.0+ (falls back to std::format)
- **container_system** (for advanced serialization)
- **thread_system** (for thread pool integration)
- **logger_system** (for structured logging)

## 🎯 CI/CD Status

| Platform | Compiler | Status |
|----------|----------|--------|
| Ubuntu 22.04 | GCC 11 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-ubuntu-gcc.yaml/badge.svg) |
| Ubuntu 22.04 | Clang 14 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-ubuntu-clang.yaml/badge.svg) |
| Windows 2022 | MSVC 2022 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-windows-vs.yaml/badge.svg) |
| Windows 2022 | MinGW64 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-windows-msys2.yaml/badge.svg) |

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [API Reference](https://kcenon.github.io/network_system) | Doxygen-generated API documentation |
| [Migration Guide](MIGRATION_GUIDE.md) | Step-by-step migration from messaging_system |
| [Integration Guide](docs/INTEGRATION.md) | How to integrate with existing systems |
| [Performance Tuning](docs/PERFORMANCE.md) | Optimization guidelines |

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes following conventional commits
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the BSD-3-Clause License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- ASIO library for asynchronous I/O
- fmt library for modern formatting
- Original messaging_system contributors
- All contributors to this separation effort

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system
**Issues**: https://github.com/kcenon/network_system/issues

---

*This project is actively maintained and welcomes contributions. For questions or support, please open an issue on GitHub.*