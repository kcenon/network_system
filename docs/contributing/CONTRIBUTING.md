# Contributing to Network System

**Version:** 0.1.0.0
**Last Updated:** 2025-11-28

Thank you for your interest in contributing to Network System! This guide will help you get started.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Code Style](#code-style)
- [Testing](#testing)
- [Documentation](#documentation)
- [Submitting Changes](#submitting-changes)
- [Review Process](#review-process)

---

## Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to:

- Be respectful and inclusive
- Welcome newcomers and help them learn
- Focus on what is best for the community
- Show empathy towards other community members

---

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- C++20 compatible compiler
  - **macOS**: Xcode 12+ or Clang 12+
  - **Linux**: GCC 10+ or Clang 12+
  - **Windows**: Visual Studio 2019+ or MinGW-w64
- CMake 3.16 or higher
- Git
- **Required dependencies**:
  - ASIO or Boost.ASIO 1.28+
  - OpenSSL 1.1.1+ (for TLS/SSL and WebSocket)
- **Optional dependencies**:
  - fmt 10.0+ (formatting)
  - container_system (advanced serialization)
  - thread_system (thread pool integration)
  - logger_system (structured logging)

### First-Time Contributors

New to open source? Start here:

1. **Browse issues labeled `good first issue`**:
   - [Good First Issues](https://github.com/kcenon/network_system/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)

2. **Read the documentation**:
   - [Architecture Guide](../ARCHITECTURE.md)
   - [API Reference](../API_REFERENCE.md)
   - [Build Guide](../guides/BUILD.md)

3. **Join discussions**:
   - [GitHub Discussions](https://github.com/kcenon/network_system/discussions)

---

## Development Setup

### 1. Fork and Clone

```bash
# Fork the repository on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/network_system.git
cd network_system

# Add upstream remote
git remote add upstream https://github.com/kcenon/network_system.git
```

### 2. Install Dependencies

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libfmt-dev libssl-dev
```

#### macOS

```bash
brew install cmake ninja asio fmt openssl
```

#### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-fmt \
          mingw-w64-x86_64-openssl
```

### 3. Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 4. Run Tests

```bash
cd build && ctest --output-on-failure
```

---

## Making Changes

### 1. Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
```

### 2. Make Your Changes

- Follow the code style guidelines
- Write tests for new features
- Update documentation as needed

### 3. Commit Your Changes

```bash
git add .
git commit -m "feat: add your feature description"
```

Use conventional commit messages:
- `feat:` for new features
- `fix:` for bug fixes
- `docs:` for documentation
- `refactor:` for code refactoring
- `test:` for adding tests
- `chore:` for maintenance tasks

---

## Code Style

- Follow modern C++ best practices
- Use RAII and smart pointers
- Use `clang-format` for formatting (configuration provided)
- Use `clang-tidy` for static analysis

### Code Formatting

```bash
# Format all source files
find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

---

## Testing

### Running Tests

```bash
cmake --build build --target test
# OR
cd build && ctest --output-on-failure
```

### Running with Sanitizers

```bash
# ThreadSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build
./build/tests/network_tests

# AddressSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build build
./build/tests/network_tests
```

---

## Documentation

- Update documentation for any API changes
- Add docstrings for new public functions/classes
- Include examples for new features
- Provide both English and Korean versions for major documents

---

## Submitting Changes

### 1. Push Your Branch

```bash
git push origin feature/your-feature-name
```

### 2. Create a Pull Request

1. Go to [GitHub](https://github.com/kcenon/network_system)
2. Click "New Pull Request"
3. Select your feature branch
4. Fill in the PR template
5. Submit the PR

---

## Review Process

1. **Automated checks**: CI/CD pipelines run tests and linters
2. **Code review**: Maintainers review your code
3. **Feedback**: Address any requested changes
4. **Merge**: Once approved, your PR will be merged

---

## Questions?

- **Issues**: [GitHub Issues](https://github.com/kcenon/network_system/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kcenon/network_system/discussions)
- **Email**: kcenon@naver.com

---

Thank you for contributing to Network System!
