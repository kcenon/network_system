# Changelog

All notable changes to the Network System project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

### To Be Implemented
- Network manager factory class
- TCP server and client high-level interfaces
- HTTP client with response handling
- WebSocket support
- TLS/SSL encryption
- Connection pooling
- Performance benchmarks
- thread_system integration (Phase 3)
- messaging_system full compatibility (Phase 4)

## 2025-09-19 - Phase 2 Complete

### Added
- **Core System Separation**
  - Verified complete separation from messaging_system
  - Confirmed proper directory structure (include/network_system)
  - All modules properly organized (core, session, internal, integration)

- **Integration Module**
  - Completed messaging_bridge implementation
  - Performance metrics collection functionality
  - Container_system integration interface

- **Build Verification**
  - Successful build with container_system integration
  - verify_build test program passes all checks
  - CMakeLists.txt supports optional integrations

### Updated
- MIGRATION_CHECKLIST.md with Phase 2 completion status
- README.md with current project phase information

## 2025-09-19 - Phase 1 Complete

### Added
- **Core Infrastructure**
  - Complete separation from messaging_system
  - New namespace structure: `network_system::{core,session,internal,integration}`
  - ASIO-based asynchronous networking with C++20 coroutines
  - Messaging bridge for backward compatibility

- **Build System**
  - CMake configuration with vcpkg support
  - Flexible dependency detection (ASIO/Boost.ASIO)
  - Cross-platform support (Linux, macOS, Windows)
  - Manual vcpkg setup as fallback

- **CI/CD Pipeline**
  - GitHub Actions workflows for Ubuntu (GCC/Clang)
  - GitHub Actions workflows for Windows (Visual Studio/MSYS2)
  - Dependency security scanning with Trivy
  - License compatibility checks

- **Container Integration**
  - Full integration with container_system
  - Value container support in messaging bridge
  - Conditional compilation based on availability

- **Documentation**
  - Doxygen configuration for API documentation
  - Comprehensive README with build instructions
  - Migration and implementation plans
  - Architecture documentation

### Fixed
- **CI/CD Issues**
  - Removed problematic lukka/run-vcpkg action
  - Fixed pthread.lib linking error on Windows
  - Added _WIN32_WINNT definition for ASIO on Windows
  - Corrected CMake options (BUILD_TESTS instead of USE_UNIT_TEST)
  - Made vcpkg optional with system package fallback

- **Build Issues**
  - Fixed namespace qualification errors
  - Corrected include paths for internal headers
  - Resolved ASIO detection on various platforms
  - Fixed vcpkg baseline configuration issues

### Changed
- **Dependency Management**
  - Prefer system packages over vcpkg when available
  - Simplified vcpkg.json configuration
  - Added multiple fallback paths for dependency detection
  - Made container_system and thread_system optional

### Security
- Implemented dependency vulnerability scanning
- Added license compatibility verification
- Configured security event reporting

## 2025-09-18 - Initial Separation

### Added
- Initial implementation separated from messaging_system
- Basic TCP client/server functionality
- Session management
- Message pipeline processing

---

## Development Timeline

| Date | Milestone | Description |
|------|-----------|-------------|
| 2025-09-19 | Phase 1 Complete | Infrastructure and separation complete |
| 2025-09-18 | Initial Separation | Started separation from messaging_system |

## Migration Guide

### From messaging_system to network_system

#### Namespace Changes
```cpp
// Old (messaging_system)
#include <messaging_system/network/tcp_server.h>
using namespace network_module;

// New (network_system)
#include <network_system/core/messaging_server.h>
using namespace network_system::core;
```

#### CMake Integration
```cmake
# Old (messaging_system)
# Part of messaging_system

# New (network_system)
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_target NetworkSystem::NetworkSystem)
```

#### Container Integration
```cpp
// New feature in network_system
#include <network_system/integration/messaging_bridge.h>

auto bridge = std::make_unique<network_system::integration::messaging_bridge>();
bridge->set_container(container);
```

## Support

For issues, questions, or contributions, please visit:
- GitHub: https://github.com/kcenon/network_system
- Email: kcenon@naver.com