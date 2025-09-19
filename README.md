# Network System

[![Build Status](https://github.com/kcenon/network_system/actions/workflows/build-ubuntu-gcc.yaml/badge.svg)](https://github.com/kcenon/network_system/actions)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)
[![C++ Version](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)

**Status**: Phase 1 Complete ✅
**Owner**: kcenon (kcenon@naver.com)

High-performance asynchronous network library separated from messaging_system for modularity and reusability.

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🎯 Project Overview

This project involves a large-scale refactoring effort to separate the `Sources/messaging_system/network` module into an independent `network_system`. The goal is to improve modularity, reusability, and maintainability while maintaining compatibility with existing systems.

### Core Objectives
- **Module Independence**: Complete separation of network module from messaging_system
- **Enhanced Reusability**: Independent library usable in other projects
- **Compatibility Maintenance**: Minimize modifications to existing messaging_system code
- **Performance Optimization**: Prevent performance degradation from separation

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 📚 Documentation

| Document | Description |
|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

----|
| [API Documentation](https://kcenon.github.io/network_system) | Doxygen-generated API reference |
| [Separation Plan](NETWORK_SYSTEM_SEPARATION_PLAN.md) | Complete migration roadmap |
| [Technical Details](TECHNICAL_IMPLEMENTATION_DETAILS.md) | Implementation specifications |
| [Phase 2 Plan](PHASE2_IMPLEMENTATION_PLAN.md) | Next phase implementation tasks |
| [Migration Checklist](MIGRATION_CHECKLIST.md) | Step-by-step checklist |

## ✅ Phase 1 Achievements

- **Core Architecture**: Complete namespace separation (`network_system::{core,session,internal,integration}`)
- **Build System**: CMake with vcpkg/system package support
- **CI/CD Pipeline**: GitHub Actions for Ubuntu (GCC/Clang) and Windows (VS/MSYS2)
- **Container Integration**: Full integration with container_system
- **Documentation**: Doxygen configuration with architectural overview
- **Dependency Management**: Flexible ASIO/Boost.ASIO detection with fallbacks

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🚀 Quick Start

### Prerequisites

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libfmt-dev libboost-all-dev
```

#### macOS
```bash
brew install cmake ninja asio fmt boost
```

#### Windows (MSYS2)
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-fmt mingw-w64-x86_64-boost
```

### Build Instructions

```bash
# Clone repository
git clone https://github.com/kcenon/network_system.git
cd network_system

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -G Ninja \
  -DBUILD_TESTS=ON \
  -DBUILD_SAMPLES=ON \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build .

# Run verification
./verify_build
```

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🏗️ Architecture Overview

### Pre-Separation Structure
```
messaging_system/
├── container/          # → container_system (already separated)
├── database/           # → database_system (already separated)
├── network/            # → network_system (separation target)
├── logger_system/      # (future separation planned)
├── monitoring_system/  # (already separated)
└── thread_system/      # (already separated)
```

### Current Structure
```
network_system/
├── include/network_system/
│   ├── core/
│   │   ├── messaging_client.h    # Async client implementation
│   │   └── messaging_server.h    # Multi-threaded server
│   ├── session/
│   │   └── messaging_session.h   # Session lifecycle management
│   ├── internal/
│   │   ├── tcp_socket.h         # ASIO TCP wrapper
│   │   ├── pipeline.h           # Message pipeline
│   │   └── send_coroutine.h    # C++20 coroutine support
│   └── integration/
│       └── messaging_bridge.h   # Legacy system bridge
├── src/                         # Implementation files
├── samples/                     # Usage examples (Phase 2)
├── tests/                       # Test suites (Phase 2)
└── .github/workflows/           # CI/CD pipelines
```

### Core Components

#### 1. Core API
- **messaging_server**: High-performance asynchronous TCP server
- **messaging_client**: TCP client implementation
- **messaging_session**: Connection session management

#### 2. Integration Layer
- **messaging_bridge**: messaging_system compatibility bridge
- **container_integration**: container_system integration
- **thread_integration**: thread_system integration

#### 3. Performance Characteristics
- **Throughput**: 100K+ messages/sec (1KB messages)
- **Concurrent connections**: 10K+ connections support
- **Latency**: < 1ms (message processing)
- **Memory usage**: ~8KB per connection

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 📊 Development Roadmap

### Phase 1: Core Infrastructure ✅ (Completed)
- [x] Directory structure and namespace separation
- [x] CMake build system with vcpkg support
- [x] CI/CD pipeline setup (GitHub Actions)
- [x] Container system integration
- [x] Doxygen documentation configuration
- [x] Core library builds successfully on all platforms

### Phase 2: Feature Implementation 🔨 (In Progress)
- [ ] Implement `network_manager` class
- [ ] Build `tcp_server` and `tcp_client` classes
- [ ] Create `http_client` with response handling
- [ ] Update sample applications
- [ ] Complete test suite implementation

### Phase 3: Advanced Features 📝 (Planned)
- [ ] WebSocket support
- [ ] TLS/SSL encryption
- [ ] Connection pooling
- [ ] Message serialization (JSON, Protocol Buffers)

### Phase 4: Performance & Optimization 🚀 (Planned)
- [ ] Performance benchmarking
- [ ] Memory optimization
- [ ] Load balancing implementation
- [ ] Async/await patterns with C++20 coroutines

### Phase 5: Production Ready 🏆 (Planned)
- [ ] Full API documentation
- [ ] User guides and tutorials
- [ ] Integration examples
- [ ] Official release planning

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🎯 CI/CD Status

| Platform | Compiler | Status |
|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

-----|
| Ubuntu 22.04 | GCC 11 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-ubuntu-gcc.yaml/badge.svg) |
| Ubuntu 22.04 | Clang 14 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-ubuntu-clang.yaml/badge.svg) |
| Windows 2022 | MSVC 2022 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-windows-vs.yaml/badge.svg) |
| Windows 2022 | MinGW64 | ![Build](https://github.com/kcenon/network_system/actions/workflows/build-windows-msys2.yaml/badge.svg) |
| Security | Trivy | ![Scan](https://github.com/kcenon/network_system/actions/workflows/dependency-security-scan.yml/badge.svg) |

## 🔧 Dependencies

### Required
- **C++20** compatible compiler
- **CMake** 3.16+
- **ASIO** or **Boost.ASIO** 1.28+

### Optional
- **fmt** 10.0+ (falls back to std::format)
- **container_system** (for advanced container support)
- **thread_system** (for thread pool integration)
- **vcpkg** (for dependency management)

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🔧 Technology Stack

### Core Dependencies
- **ASIO**: Asynchronous I/O and networking
- **fmt**: String formatting
- **C++20**: Modern C++ features

### Integration Dependencies (Conditional)
- **container_system**: Message serialization/deserialization
- **thread_system**: Asynchronous task scheduling
- **GTest**: Unit testing framework
- **Benchmark**: Performance benchmarking

### Build Tools
- **CMake 3.16+**: Build system
- **vcpkg**: Package management
- **Git**: Version control

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 📈 Performance Requirements

### Baseline (Pre-Separation)
| Metric | Value | Conditions |
|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

-----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---|
| Throughput | 100K messages/sec | 1KB messages, localhost |
| Latency | < 1ms | Message processing latency |
| Concurrent connections | 10K+ | Within system resource limits |
| Memory usage | ~8KB/connection | Including buffers and session data |

### Target (Post-Separation)
| Metric | Target | Acceptable Range |
|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

-----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

-----|## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---|
| Throughput | >= 100K messages/sec | Performance degradation < 5% |
| Latency | < 1.2ms | Bridge overhead < 20% |
| Concurrent connections | 10K+ | No change |
| Memory usage | ~10KB/connection | Increase < 25% |

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🚨 Risk Factors and Mitigation

### Major Risks
1. **Conflict with existing network_system**
   - **Mitigation**: Namespace separation, rename existing system

2. **Performance degradation**
   - **Mitigation**: Zero-copy bridge, optimized integration layer

3. **Compatibility issues**
   - **Mitigation**: Thorough compatibility testing, gradual migration

4. **Increased complexity**
   - **Mitigation**: Automated build scripts, clear documentation

### Rollback Plan
- Phase-wise checkpoints
- Original code backup retention (30 days)
- Runtime system switching capability
- Real-time performance monitoring

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 📞 Contact and Support

### Project Owner
- **Name**: kcenon
- **Email**: kcenon@naver.com
- **Role**: Project Lead, Architect

### Inquiries
- **Technical questions**: GitHub Issues or email
- **Bug reports**: Submit with detailed reproduction steps
- **Feature requests**: Propose with use cases

### Document Locations
- **Project documentation**: `/Users/dongcheolshin/Sources/network_system/`
- **Source code**: Same location after migration completion
- **Backups**: `network_migration_backup_YYYYMMDD_HHMMSS/`

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 📝 Change History

### v2.0.0 (2025-09-19) - Planning Phase
- [x] Migration master plan creation
- [x] Technical implementation details organization
- [x] Automation script development
- [x] Checklist and verification plan establishment

### v2.0.1 (Planned) - Core Separation
- [ ] messaging_system/network code separation
- [ ] Namespace and structure reorganization
- [ ] Basic build system configuration

### v2.0.2 (Planned) - Integration Completion
- [ ] All system integration interface implementation
- [ ] messaging_system compatibility assurance
- [ ] Performance optimization completion

### v2.1.0 (Planned) - Stabilization
- [ ] All tests passing
- [ ] Documentation completion
- [ ] Production deployment readiness

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

## 🎉 Getting Started

If you're ready, start the migration with the following command:

```bash
cd /Users/dongcheolshin/Sources/network_system
./scripts/migration/quick_start.sh
```

You can proceed step-by-step through the interactive menu or choose full automated migration.

**Good luck!** 🚀

## 📝 API Example

```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>

// Server example
auto server = std::make_shared<network_system::core::messaging_server>();
server->set_message_handler([](const std::string& message) {
    std::cout << "Received: " << message << std::endl;
    return "Echo: " + message;
});
server->start("0.0.0.0", 8080);

// Client example
auto client = std::make_shared<network_system::core::messaging_client>();
client->connect("localhost", 8080);
client->send("Hello, Network System!");
```

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

## 📧 Contact

**Owner**: kcenon (kcenon@naver.com)
**Repository**: https://github.com/kcenon/network_system

---

*This document is a comprehensive guide for the network_system separation project. Please contact us anytime if you have questions or issues.*