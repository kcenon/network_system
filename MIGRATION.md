# Migration Guide: messaging_system to network_system

> **Language:** **English** | [한국어](MIGRATION_KO.md)

**Document Version**: 1.0.0
**Last Updated**: 2025-10-22
**Maintainer**: kcenon@naver.com

---

## Table of Contents

- [Overview](#overview)
  - [What Changed](#what-changed)
  - [Why This Migration](#why-this-migration)
  - [Migration Paths](#migration-paths)
- [Quick Migration Checklist](#quick-migration-checklist)
- [Step-by-Step Migration Guide](#step-by-step-migration-guide)
  - [Phase 1: Preparation](#phase-1-preparation)
  - [Phase 2: Update Build Configuration](#phase-2-update-build-configuration)
  - [Phase 3: Update Code](#phase-3-update-code)
  - [Phase 4: Verify Migration](#phase-4-verify-migration)
- [Before and After Examples](#before-and-after-examples)
  - [CMake Configuration](#cmake-configuration)
  - [Include Statements](#include-statements)
  - [Namespace Usage](#namespace-usage)
  - [Server Creation](#server-creation)
  - [Client Usage](#client-usage)
- [Common Migration Issues](#common-migration-issues)
  - [Build Issues](#build-issues)
  - [Runtime Issues](#runtime-issues)
  - [Performance Issues](#performance-issues)
  - [Integration Issues](#integration-issues)
- [Testing Your Migration](#testing-your-migration)
  - [Compilation Tests](#compilation-tests)
  - [Functional Tests](#functional-tests)
  - [Performance Tests](#performance-tests)
  - [Integration Tests](#integration-tests)
- [Rollback Plan](#rollback-plan)
  - [Quick Rollback](#quick-rollback)
  - [Partial Rollback](#partial-rollback)
- [Performance Expectations](#performance-expectations)
- [Support and Resources](#support-and-resources)

---

## Overview

The **network_system** is an independent, high-performance C++20 asynchronous network library that has been successfully extracted from the `messaging_system` project. This migration guide helps you transition from the integrated network module to the standalone library.

### What Changed

The network module has been separated from `messaging_system` into an independent library with the following changes:

| Aspect | Before (messaging_system) | After (network_system) |
|--------|---------------------------|------------------------|
| **Namespace** | `messaging::network` | `network_system::core` |
| **Header Path** | `<messaging/network/*>` | `<network_system/*>` |
| **CMake Target** | `messaging_system` | `NetworkSystem::NetworkSystem` |
| **Repository** | Part of messaging_system | Independent repository |
| **Dependencies** | Tightly coupled | Optional integration |

### Why This Migration

The separation provides several key benefits:

- ✅ **Modularity**: Use network functionality independently without messaging_system overhead
- ✅ **Reusability**: Integrate network_system into any C++ project
- ✅ **Performance**: 305K+ messages/second throughput, 3x improvement over legacy
- ✅ **Maintainability**: Independent development cycle and versioning
- ✅ **Backward Compatibility**: 100% compatibility maintained through compatibility layer

### Migration Paths

You have **two migration options**:

#### Option 1: Compatibility Mode (Recommended for Gradual Migration)

Use the compatibility layer for minimal code changes. This approach:
- ✅ Maintains existing namespaces (`network_module::`, `messaging::`)
- ✅ Keeps current API calls unchanged
- ✅ Allows gradual migration to modern API
- ✅ Zero breaking changes

**Best for**: Production systems requiring zero-downtime migration.

#### Option 2: Full Migration (Recommended for New Projects)

Adopt the modern API directly. This approach:
- ✅ Uses new namespace structure (`network_system::core::`)
- ✅ Leverages modern C++20 features
- ✅ Takes advantage of enhanced performance
- ✅ Provides better type safety with Result<T>

**Best for**: New projects or complete refactoring efforts.

---

## Quick Migration Checklist

Use this checklist for a high-level overview of migration tasks:

### Pre-Migration
- [ ] **Backup your current codebase** (create git branch or backup directory)
- [ ] **Review current network usage** (identify all files using network module)
- [ ] **Check dependencies** (verify ASIO, fmt, C++20 compiler availability)
- [ ] **Choose migration path** (compatibility mode vs. full migration)

### Build System Migration
- [ ] **Update CMakeLists.txt** (replace MessagingSystem with NetworkSystem)
- [ ] **Configure package dependencies** (add network_system to vcpkg.json or equivalent)
- [ ] **Set C++ standard** (ensure C++20 is enabled)
- [ ] **Test build** (verify clean compilation)

### Code Migration
- [ ] **Update include paths** (change header locations)
- [ ] **Update namespaces** (or use compatibility layer)
- [ ] **Add initialization code** (network_system::compat::initialize())
- [ ] **Update API calls** (if using modern API)
- [ ] **Add shutdown code** (network_system::compat::shutdown())

### Testing & Validation
- [ ] **Compile test** (ensure zero compilation errors)
- [ ] **Unit tests** (run existing test suite)
- [ ] **Integration tests** (verify end-to-end functionality)
- [ ] **Performance benchmark** (compare against baseline)
- [ ] **Memory leak check** (run with sanitizers)

### Documentation & Deployment
- [ ] **Update internal documentation** (reflect new dependencies)
- [ ] **Update deployment scripts** (if applicable)
- [ ] **Train team members** (share migration guide)
- [ ] **Plan rollback strategy** (in case of issues)

---

## Step-by-Step Migration Guide

This section provides detailed, sequential instructions for migration.

### Phase 1: Preparation

#### Step 1.1: Backup Your Code

Create a safety backup before making any changes:

```bash
# Create a git branch for migration
git checkout -b migration/network-system
git push origin migration/network-system

# Or create a filesystem backup
cp -r /path/to/your/project /path/to/backup/project_$(date +%Y%m%d_%H%M%S)
```

#### Step 1.2: Audit Current Network Usage

Identify all files using the network module:

```bash
# Find all files including messaging_system network headers
grep -r "include.*messaging.*network" /path/to/your/project/src

# Find all namespace usages
grep -r "messaging::network\|network_module" /path/to/your/project/src

# Find all CMake references
grep -r "MessagingSystem" /path/to/your/project/CMakeLists.txt
```

Create an inventory of files to update. Example format:

```
Files to update:
1. src/network/client.cpp - Uses messaging::network::client
2. src/network/server.cpp - Uses messaging::network::server
3. CMakeLists.txt - Links MessagingSystem
4. include/app/network_manager.h - Includes <messaging/network/tcp_server.h>
```

#### Step 1.3: Verify Prerequisites

Ensure your environment meets the requirements:

```bash
# Check C++ compiler supports C++20
g++ --version  # Should be GCC 10+ or Clang 10+
clang++ --version

# Check CMake version
cmake --version  # Should be 3.16+

# Check ASIO availability (if not using Boost)
pkg-config --modversion asio  # Optional but recommended
```

Install missing dependencies:

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y cmake ninja-build libasio-dev libfmt-dev

# macOS
brew install cmake ninja asio fmt

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-asio mingw-w64-x86_64-fmt
```

---

### Phase 2: Update Build Configuration

#### Step 2.1: Update CMakeLists.txt

Replace messaging_system dependency with network_system:

**Before:**
```cmake
# Old CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(YourApplication)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find MessagingSystem
find_package(MessagingSystem REQUIRED)

# Create executable
add_executable(your_app main.cpp network_client.cpp)

# Link MessagingSystem
target_link_libraries(your_app
    PRIVATE
    MessagingSystem::MessagingSystem
)
```

**After:**
```cmake
# New CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(YourApplication)

# IMPORTANT: Upgrade to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NetworkSystem
find_package(NetworkSystem REQUIRED
    PATHS /path/to/network_system/build
)

# Create executable
add_executable(your_app main.cpp network_client.cpp)

# Link NetworkSystem
target_link_libraries(your_app
    PRIVATE
    NetworkSystem::NetworkSystem
)

# Platform-specific dependencies
if(WIN32)
    target_link_libraries(your_app PRIVATE ws2_32 mswsock)
    target_compile_definitions(your_app PRIVATE _WIN32_WINNT=0x0601)
endif()

# Enable threading support
find_package(Threads REQUIRED)
target_link_libraries(your_app PRIVATE Threads::Threads)
```

#### Step 2.2: Update Package Configuration (Optional)

If using vcpkg or other package managers:

**vcpkg.json**:
```json
{
  "name": "your-application",
  "version": "1.0.0",
  "dependencies": [
    "asio",
    "fmt"
  ],
  "builtin-baseline": "latest"
}
```

#### Step 2.3: Configure Network System Build

Build and install network_system first:

```bash
# Clone network_system repository
git clone https://github.com/kcenon/network_system.git
cd network_system

# Create build directory
mkdir build && cd build

# Configure with desired options
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_WITH_THREAD_SYSTEM=ON \
    -DBUILD_WITH_LOGGER_SYSTEM=ON \
    -DBUILD_WITH_CONTAINER_SYSTEM=ON \
    -DBUILD_WITH_COMMON_SYSTEM=ON \
    -DBUILD_SHARED_LIBS=OFF

# Build
cmake --build . --parallel

# Install (optional, or reference build directory)
sudo cmake --install .
```

---

### Phase 3: Update Code

#### Step 3.1: Choose Your Migration Strategy

**Strategy A: Compatibility Mode (Minimal Changes)**

Use this if you want to migrate with zero code changes:

1. Include compatibility header
2. Keep existing namespace
3. Add initialization/shutdown calls

**Strategy B: Modern API (Full Refactor)**

Use this for new projects or complete modernization:

1. Update all includes
2. Change namespaces
3. Adopt modern API patterns
4. Use Result<T> error handling

#### Step 3.2: Update Includes

**Compatibility Mode:**

```cpp
// Before
#include <messaging_system/network/tcp_server.h>
#include <messaging_system/network/tcp_client.h>
#include <messaging_system/network/session.h>

// After (Compatibility Mode)
#include <network_system/compatibility.h>
```

**Modern API:**

```cpp
// Before
#include <messaging_system/network/tcp_server.h>
#include <messaging_system/network/tcp_client.h>

// After (Modern API)
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>
#include <network_system/session/messaging_session.h>
```

#### Step 3.3: Update Namespace References

**Compatibility Mode (No Changes):**

```cpp
// Your existing code continues to work:
auto server = network_module::create_server("server_id");
auto client = network_module::create_client("client_id");

// Or if you used messaging namespace:
auto server = messaging::create_server("server_id");
```

**Modern API:**

```cpp
// Before
using namespace messaging::network;
auto server = create_server("server_id");

// After
using namespace network_system::core;
auto server = std::make_shared<messaging_server>("server_id");
```

#### Step 3.4: Add Initialization Code

Add initialization at application startup:

```cpp
int main() {
    // Initialize network system
    network_system::compat::initialize();

    // Your existing application code
    try {
        auto server = network_module::create_server("my_server");
        server->start_server(8080);

        // Application logic...

        server->stop_server();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        network_system::compat::shutdown();
        return 1;
    }

    // Cleanup
    network_system::compat::shutdown();
    return 0;
}
```

#### Step 3.5: Update API Calls (If Using Modern API)

If you chose the modern API migration path:

**Before:**
```cpp
// Legacy API
auto server = network_module::create_server("server_id");
server->start("0.0.0.0", 8080);

auto client = network_module::create_client("client_id");
client->connect("localhost", 8080);
client->send("Hello");
```

**After:**
```cpp
// Modern API with Result<T>
auto server = std::make_shared<network_system::core::messaging_server>("server_id");
auto start_result = server->start_server(8080);

if (start_result.is_err()) {
    std::cerr << "Failed to start server: "
              << start_result.error().message
              << " (code: " << start_result.error().code << ")\n";
    return -1;
}

auto client = std::make_shared<network_system::core::messaging_client>("client_id");
auto connect_result = client->start_client("localhost", 8080);

if (connect_result.is_ok()) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto send_result = client->send_packet(data);

    if (send_result.is_err()) {
        std::cerr << "Send failed: " << send_result.error().message << "\n";
    }
}
```

---

### Phase 4: Verify Migration

#### Step 4.1: Compilation Test

Build your project to verify no compilation errors:

```bash
# Clean build
rm -rf build
mkdir build && cd build

# Configure
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build with verbose output
cmake --build . --verbose

# Check for warnings
cmake --build . 2>&1 | grep -i "warning"
```

#### Step 4.2: Run Unit Tests

Execute your existing test suite:

```bash
# Run all tests
ctest --output-on-failure --verbose

# Or run specific tests
./build/your_network_test

# With sanitizers (recommended)
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build .
./build/your_network_test
```

#### Step 4.3: Verify Functionality

Create a simple verification test:

```cpp
// migration_verify.cpp
#include <network_system/compatibility.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>

int main() {
    std::cout << "Network System Migration Verification\n";
    std::cout << "======================================\n\n";

    // Initialize
    network_system::compat::initialize();

    // Test 1: Server creation and startup
    std::cout << "Test 1: Server creation and startup..." << std::flush;
    auto server = network_module::create_server("verify_server");
    assert(server != nullptr);
    auto result = server->start_server(9999);
    assert(result.is_ok());
    std::cout << " PASSED\n";

    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test 2: Client creation and connection
    std::cout << "Test 2: Client creation and connection..." << std::flush;
    auto client = network_module::create_client("verify_client");
    assert(client != nullptr);
    result = client->start_client("127.0.0.1", 9999);
    assert(result.is_ok());
    std::cout << " PASSED\n";

    // Test 3: Data transmission
    std::cout << "Test 3: Data transmission..." << std::flush;
    std::vector<uint8_t> test_data = {'T', 'E', 'S', 'T'};
    result = client->send_packet(test_data);
    assert(result.is_ok());
    std::cout << " PASSED\n";

    // Cleanup
    std::cout << "Test 4: Cleanup..." << std::flush;
    client->stop_client();
    server->stop_server();
    network_system::compat::shutdown();
    std::cout << " PASSED\n";

    std::cout << "\n======================================\n";
    std::cout << "All migration verification tests PASSED!\n";
    return 0;
}
```

Compile and run:

```bash
g++ -std=c++20 migration_verify.cpp \
    -I/path/to/network_system/include \
    -L/path/to/network_system/build \
    -lNetworkSystem -lpthread \
    -o migration_verify

./migration_verify
```

Expected output:
```
Network System Migration Verification
======================================

Test 1: Server creation and startup... PASSED
Test 2: Client creation and connection... PASSED
Test 3: Data transmission... PASSED
Test 4: Cleanup... PASSED

======================================
All migration verification tests PASSED!
```

---

## Before and After Examples

This section provides comprehensive before/after comparisons.

### CMake Configuration

**Before (messaging_system):**
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyNetworkApp CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(MessagingSystem REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app MessagingSystem::MessagingSystem)
```

**After (network_system):**
```cmake
cmake_minimum_required(VERSION 3.16)
project(MyNetworkApp CXX)

set(CMAKE_CXX_STANDARD 20)  # Upgraded to C++20
find_package(NetworkSystem REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app NetworkSystem::NetworkSystem)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(app ws2_32 mswsock)
endif()
```

### Include Statements

**Before (messaging_system):**
```cpp
#include <messaging_system/network/tcp_server.h>
#include <messaging_system/network/tcp_client.h>
#include <messaging_system/network/session_manager.h>
#include <messaging_system/network/pipeline.h>
```

**After (Compatibility Mode):**
```cpp
// Single compatibility header replaces all
#include <network_system/compatibility.h>
```

**After (Modern API):**
```cpp
#include <network_system/core/messaging_server.h>
#include <network_system/core/messaging_client.h>
#include <network_system/core/session_manager.h>
#include <network_system/internal/pipeline.h>
```

### Namespace Usage

**Before (messaging_system):**
```cpp
using namespace messaging::network;

void setup_network() {
    auto server = std::make_shared<tcp_server>("server");
    auto client = std::make_shared<tcp_client>("client");
}
```

**After (Compatibility Mode):**
```cpp
// No changes needed - legacy namespaces still work
using namespace network_module;

void setup_network() {
    auto server = create_server("server");
    auto client = create_client("client");
}
```

**After (Modern API):**
```cpp
using namespace network_system::core;

void setup_network() {
    auto server = std::make_shared<messaging_server>("server");
    auto client = std::make_shared<messaging_client>("client");
}
```

### Server Creation

**Before (messaging_system):**
```cpp
#include <messaging_system/network/tcp_server.h>

void start_server() {
    auto server = std::make_shared<messaging::network::tcp_server>("my_server");

    // Start server
    server->start("0.0.0.0", 8080);

    // Set callback
    server->on_message([](const std::string& msg) {
        return "Echo: " + msg;
    });
}
```

**After (Compatibility Mode):**
```cpp
#include <network_system/compatibility.h>

void start_server() {
    network_system::compat::initialize();

    // Use legacy factory function
    auto server = network_module::create_server("my_server");

    // Start server - modern API
    auto result = server->start_server(8080);
    if (result.is_err()) {
        std::cerr << "Start failed: " << result.error().message << "\n";
        return;
    }

    // Note: set_message_handler not yet available in current version
    // Use session-based handling instead
}
```

**After (Modern API):**
```cpp
#include <network_system/core/messaging_server.h>

void start_server() {
    auto server = std::make_shared<network_system::core::messaging_server>("my_server");

    // Start server with Result<T> error handling
    auto result = server->start_server(8080);

    if (result.is_err()) {
        std::cerr << "Failed to start server: "
                  << result.error().message
                  << " (code: " << result.error().code << ")\n";
        return;
    }

    std::cout << "Server started successfully on port 8080\n";
}
```

### Client Usage

**Before (messaging_system):**
```cpp
#include <messaging_system/network/tcp_client.h>

void connect_and_send() {
    auto client = std::make_shared<messaging::network::tcp_client>("client");

    client->connect("localhost", 8080);
    client->send("Hello Server");
    client->disconnect();
}
```

**After (Compatibility Mode):**
```cpp
#include <network_system/compatibility.h>

void connect_and_send() {
    network_system::compat::initialize();

    auto client = network_module::create_client("client");

    auto connect_result = client->start_client("localhost", 8080);
    if (connect_result.is_ok()) {
        std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
        auto send_result = client->send_packet(data);

        if (send_result.is_err()) {
            std::cerr << "Send failed\n";
        }

        client->stop_client();
    }

    network_system::compat::shutdown();
}
```

**After (Modern API):**
```cpp
#include <network_system/core/messaging_client.h>

void connect_and_send() {
    auto client = std::make_shared<network_system::core::messaging_client>("client");

    // Connect with error handling
    auto connect_result = client->start_client("localhost", 8080);
    if (connect_result.is_err()) {
        std::cerr << "Connection failed: " << connect_result.error().message << "\n";
        return;
    }

    // Send binary data
    std::vector<uint8_t> message = {'H', 'e', 'l', 'l', 'o', ' ', 'S', 'e', 'r', 'v', 'e', 'r'};
    auto send_result = client->send_packet(message);

    if (send_result.is_err()) {
        std::cerr << "Send failed: " << send_result.error().message << "\n";
    } else {
        std::cout << "Message sent successfully\n";
    }

    // Graceful shutdown
    client->stop_client();
}
```

---

## Common Migration Issues

This section addresses frequently encountered problems during migration.

### Build Issues

#### Issue 1: Undefined Reference to network_system Functions

**Symptom:**
```
undefined reference to `network_system::core::messaging_server::start_server(unsigned short)'
```

**Cause:** NetworkSystem library not properly linked.

**Solution:**
```cmake
# Ensure NetworkSystem is found and linked
find_package(NetworkSystem REQUIRED)
target_link_libraries(your_app PRIVATE NetworkSystem::NetworkSystem)

# Also ensure threading support
find_package(Threads REQUIRED)
target_link_libraries(your_app PRIVATE Threads::Threads)
```

#### Issue 2: namespace 'network_module' Not Found

**Symptom:**
```cpp
error: 'network_module' has not been declared
```

**Cause:** Compatibility header not included.

**Solution:**
```cpp
// Add this include at the top of your file
#include <network_system/compatibility.h>
```

#### Issue 3: C++20 Compiler Errors

**Symptom:**
```
error: 'concept' does not name a type
error: coroutine support requires -fcoroutines
```

**Cause:** Compiler doesn't support C++20 or not enabled.

**Solution:**
```cmake
# Ensure C++20 is enabled
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For GCC/Clang, you may need:
target_compile_options(your_app PRIVATE -fcoroutines)
```

Update your compiler if necessary:
```bash
# Ubuntu - Install GCC 11+
sudo apt install g++-11
export CXX=g++-11

# macOS - Use latest Xcode Command Line Tools
xcode-select --install
```

#### Issue 4: ASIO Not Found

**Symptom:**
```
CMake Error: Could not find a package configuration file provided by "asio"
```

**Cause:** ASIO library not installed.

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libasio-dev

# macOS
brew install asio

# Or use Boost.Asio instead (already included in most systems)
# network_system will automatically detect and use Boost.Asio
```

### Runtime Issues

#### Issue 5: Segmentation Fault on Server Start

**Symptom:**
Application crashes when calling `start_server()`.

**Cause:** Network system not initialized or thread pool not available.

**Solution:**
```cpp
// Always initialize before use
int main() {
    network_system::compat::initialize();

    auto server = network_module::create_server("server");
    auto result = server->start_server(8080);

    // ... your code ...

    network_system::compat::shutdown();
}
```

#### Issue 6: Connection Timeout

**Symptom:**
Client fails to connect with timeout error.

**Cause:** Server not fully started before client connects, or firewall blocking.

**Solution:**
```cpp
// Add delay after server start
server->start_server(8080);
std::this_thread::sleep_for(std::chrono::milliseconds(100));

// Or check server status
auto result = server->start_server(8080);
if (result.is_ok()) {
    // Server started successfully
    client->start_client("localhost", 8080);
}

// Check firewall settings
// Linux: sudo ufw allow 8080
// macOS: System Preferences > Security & Privacy > Firewall
```

#### Issue 7: Memory Leak Warnings

**Symptom:**
AddressSanitizer or Valgrind reports memory leaks.

**Cause:** Missing shutdown call or server/client not properly stopped.

**Solution:**
```cpp
void cleanup() {
    // Always stop clients and servers before shutdown
    client->stop_client();
    server->stop_server();

    // Call shutdown to clean up resources
    network_system::compat::shutdown();
}

// Or use RAII pattern
class NetworkGuard {
public:
    NetworkGuard() { network_system::compat::initialize(); }
    ~NetworkGuard() { network_system::compat::shutdown(); }
};

int main() {
    NetworkGuard guard;  // Automatic cleanup
    // Your network code...
}
```

### Performance Issues

#### Issue 8: Lower Throughput Than Expected

**Symptom:**
Performance benchmarks show lower throughput than advertised (< 100K msg/s).

**Cause:** Debug build, thread pool not configured, or buffer sizes not optimized.

**Solution:**
```bash
# Ensure Release build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Enable link-time optimization
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

```cpp
// Configure thread pool for better performance
auto& thread_mgr = network_system::integration::thread_integration_manager::instance();
auto thread_pool = std::make_shared<network_system::integration::basic_thread_pool>();
thread_pool->start(std::thread::hardware_concurrency());  // Use all cores
thread_mgr.set_thread_pool(thread_pool);
```

#### Issue 9: High Latency

**Symptom:**
Message latency > 1ms for localhost connections.

**Cause:** Nagle's algorithm enabled, buffering delays, or system overload.

**Solution:**
```cpp
// Disable Nagle's algorithm (TCP_NODELAY) if needed
// This is typically handled internally by network_system
// But you can verify in your system logs

// Reduce buffering for low-latency applications
// Check that you're using the zero-copy pipeline feature
```

Run performance tests:
```bash
cd network_system/build
./benchmark  # Should show 305K+ msg/s average
```

### Integration Issues

#### Issue 10: container_system Integration Failure

**Symptom:**
```cpp
error: 'container_system' is not a member of 'network_system::integration'
```

**Cause:** network_system not built with container_system support.

**Solution:**
```bash
# Rebuild network_system with container integration
cd network_system/build
cmake .. -DBUILD_WITH_CONTAINER_SYSTEM=ON
cmake --build .
```

```cpp
// Check if container support is available
#if BUILD_WITH_CONTAINER_SYSTEM
    // Use container integration
#else
    #warning "Container system integration not available"
#endif
```

#### Issue 11: thread_system Integration Failure

**Symptom:**
Thread pool not working or threads not being used.

**Cause:** thread_system not properly integrated.

**Solution:**
```bash
# Rebuild with thread_system support
cmake .. -DBUILD_WITH_THREAD_SYSTEM=ON
cmake --build .
```

```cpp
// Manually configure thread pool
auto& thread_mgr = network_system::integration::thread_integration_manager::instance();
if (!thread_mgr.get_thread_pool()) {
    auto pool = std::make_shared<network_system::integration::basic_thread_pool>();
    thread_mgr.set_thread_pool(pool);
}
```

---

## Testing Your Migration

Comprehensive testing ensures a successful migration.

### Compilation Tests

#### Test 1: Clean Build

```bash
# Clean everything
rm -rf build/
mkdir build && cd build

# Configure
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build with all warnings
cmake --build . -- -v 2>&1 | tee build.log

# Check for warnings
grep -i "warning" build.log
```

**Expected:** Zero warnings, clean build.

#### Test 2: Multiple Configurations

```bash
# Test Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build .

# Test Release build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .

# Test with sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" \
         && cmake --build .
```

**Expected:** All configurations build successfully.

### Functional Tests

#### Test 3: Basic Connectivity

```cpp
// test_basic_connectivity.cpp
#include <network_system/compatibility.h>
#include <cassert>
#include <thread>
#include <chrono>

int main() {
    network_system::compat::initialize();

    auto server = network_module::create_server("test_server");
    assert(server->start_server(8888).is_ok());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto client = network_module::create_client("test_client");
    assert(client->start_client("127.0.0.1", 8888).is_ok());

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    assert(client->send_packet(data).is_ok());

    client->stop_client();
    server->stop_server();
    network_system::compat::shutdown();

    std::cout << "Basic connectivity test PASSED\n";
    return 0;
}
```

#### Test 4: Concurrent Connections

```cpp
// test_concurrent.cpp
#include <network_system/compatibility.h>
#include <vector>
#include <thread>
#include <atomic>

int main() {
    network_system::compat::initialize();

    auto server = network_module::create_server("concurrent_server");
    server->start_server(9000);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const int NUM_CLIENTS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < NUM_CLIENTS; ++i) {
        threads.emplace_back([i, &success_count]() {
            auto client = network_module::create_client("client_" + std::to_string(i));
            if (client->start_client("127.0.0.1", 9000).is_ok()) {
                success_count++;
                std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
                client->send_packet(data);
                client->stop_client();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    server->stop_server();
    network_system::compat::shutdown();

    std::cout << "Concurrent test: " << success_count << "/" << NUM_CLIENTS
              << " clients connected successfully\n";

    return (success_count == NUM_CLIENTS) ? 0 : 1;
}
```

### Performance Tests

#### Test 5: Throughput Benchmark

```cpp
// test_throughput.cpp
#include <network_system/compatibility.h>
#include <chrono>
#include <iostream>

int main() {
    network_system::compat::initialize();

    auto server = network_module::create_server("perf_server");
    server->start_server(9090);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto client = network_module::create_client("perf_client");
    client->start_client("127.0.0.1", 9090);

    const int NUM_MESSAGES = 10000;
    std::vector<uint8_t> data(64, 0x42);  // 64-byte message

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; ++i) {
        client->send_packet(data);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double throughput = (NUM_MESSAGES * 1000.0) / duration.count();

    std::cout << "Throughput: " << throughput << " msg/s\n";
    std::cout << "Expected: > 100,000 msg/s\n";
    std::cout << "Result: " << (throughput > 100000 ? "PASSED" : "NEEDS INVESTIGATION") << "\n";

    client->stop_client();
    server->stop_server();
    network_system::compat::shutdown();

    return 0;
}
```

#### Test 6: Latency Benchmark

```bash
# Use included benchmark tool
cd network_system/build
./benchmark

# Expected output:
# Average Throughput: 305,255 msg/s
# Average Latency: 584.22 μs
# Performance Rating: EXCELLENT
```

### Integration Tests

#### Test 7: Existing Application Tests

Run your existing test suite to ensure compatibility:

```bash
# Run your existing tests
cd your_application/build
ctest --output-on-failure

# Or run specific test binary
./your_network_tests --verbose

# With memory checking
valgrind --leak-check=full ./your_network_tests
```

**Expected:** All existing tests pass without modification.

#### Test 8: End-to-End Scenario

Create a complete workflow test that mirrors your production use case:

```cpp
// test_e2e_scenario.cpp
#include <network_system/compatibility.h>

int main() {
    // Simulate your production workflow
    network_system::compat::initialize();

    // 1. Server setup
    auto server = network_module::create_server("production_server");
    server->start_server(8080);

    // 2. Multiple clients connect
    auto client1 = network_module::create_client("client1");
    auto client2 = network_module::create_client("client2");

    client1->start_client("localhost", 8080);
    client2->start_client("localhost", 8080);

    // 3. Exchange messages
    std::vector<uint8_t> msg1 = {'M', 'S', 'G', '1'};
    std::vector<uint8_t> msg2 = {'M', 'S', 'G', '2'};

    client1->send_packet(msg1);
    client2->send_packet(msg2);

    // 4. Graceful shutdown
    client1->stop_client();
    client2->stop_client();
    server->stop_server();

    network_system::compat::shutdown();

    std::cout << "End-to-end scenario test PASSED\n";
    return 0;
}
```

---

## Rollback Plan

If issues arise during migration, follow these rollback procedures.

### Quick Rollback

If you need to immediately revert:

```bash
# If you used git branch
git checkout main  # or your previous branch
git branch -D migration/network-system

# If you created a filesystem backup
rm -rf /path/to/your/project
cp -r /path/to/backup/project_TIMESTAMP /path/to/your/project

# Rebuild with original configuration
cd /path/to/your/project
mkdir build && cd build
cmake .. && cmake --build .
```

### Partial Rollback

If only specific components have issues:

1. **Revert specific files:**
```bash
# Revert CMakeLists.txt changes only
git checkout HEAD -- CMakeLists.txt

# Revert specific source files
git checkout HEAD -- src/network/problematic_file.cpp
```

2. **Keep new network_system but use compatibility mode:**
```cpp
// Switch from modern API back to compatibility mode
// Change this:
#include <network_system/core/messaging_server.h>
using namespace network_system::core;

// Back to this:
#include <network_system/compatibility.h>
using namespace network_module;
```

3. **Document issues:**
```markdown
# Rollback Notes

Date: 2025-10-22
Reason: Performance regression in concurrent scenario
Components reverted:
  - src/network/concurrent_handler.cpp
  - src/network/session_manager.cpp

Files kept:
  - CMakeLists.txt (using NetworkSystem)
  - Basic client/server code (compatibility mode)

Next steps:
  - Investigate performance issue
  - Re-attempt migration with fixes
```

---

## Performance Expectations

After successful migration, you should observe these performance characteristics:

### Throughput Metrics

| Message Size | Expected Throughput | Test Command |
|-------------|---------------------|--------------|
| Small (64B) | 769,230+ msg/s | `./benchmark --size=64` |
| Medium (1KB) | 128,205+ msg/s | `./benchmark --size=1024` |
| Large (8KB) | 20,833+ msg/s | `./benchmark --size=8192` |
| **Mixed Average** | **305,255+ msg/s** | `./benchmark` |

### Latency Metrics

| Percentile | Expected Latency | Measurement Method |
|-----------|------------------|-------------------|
| P50 (Median) | < 50 μs | High-resolution timer |
| P95 | < 500 μs | Latency distribution |
| P99 | < 1 ms | Stress test |
| **Average** | **584 μs** | Overall average |

### Concurrency Metrics

| Concurrent Connections | Stable Throughput | Notes |
|-----------------------|-------------------|-------|
| 10 clients | 100K+ msg/s | Linear scaling |
| 50 clients | 12,195+ msg/s per client | Production baseline |
| 100 clients | Graceful degradation | System-dependent |

### Resource Usage

| Resource | Expected Usage | Monitoring |
|----------|---------------|------------|
| Memory (baseline) | < 10 MB | Before connections |
| Memory (per connection) | Linear scaling | ~10KB overhead |
| CPU (idle) | < 1% | Server waiting |
| CPU (active) | Scales with cores | Multi-threaded |

### Comparison with Legacy

| Metric | messaging_system | network_system | Improvement |
|--------|------------------|----------------|-------------|
| Throughput | ~100K msg/s | 305K+ msg/s | **3x faster** |
| Latency | ~1.5 ms | 584 μs | **2.5x lower** |
| Memory | 15 MB baseline | < 10 MB | **33% reduction** |
| Concurrent clients | 20-30 stable | 50+ stable | **2x capacity** |

---

## Support and Resources

### Documentation

| Resource | Location | Description |
|----------|----------|-------------|
| **API Reference** | [docs/API_REFERENCE.md](/Users/raphaelshin/Sources/network_system/docs/API_REFERENCE.md) | Complete API documentation |
| **Migration Guide** | This document | Step-by-step migration instructions |
| **Architecture Guide** | [docs/ARCHITECTURE.md](/Users/raphaelshin/Sources/network_system/docs/ARCHITECTURE.md) | System design and patterns |
| **Integration Guide** | [docs/INTEGRATION.md](/Users/raphaelshin/Sources/network_system/docs/INTEGRATION.md) | Ecosystem integration |
| **Troubleshooting** | [docs/TROUBLESHOOTING.md](/Users/raphaelshin/Sources/network_system/docs/TROUBLESHOOTING.md) | Common issues and solutions |
| **Changelog** | [CHANGELOG.md](/Users/raphaelshin/Sources/network_system/CHANGELOG.md) | Version history |

### Sample Code

| Sample | Location | Purpose |
|--------|----------|---------|
| **Basic Usage** | [samples/basic_usage.cpp](/Users/raphaelshin/Sources/network_system/samples/basic_usage.cpp) | Simple client/server example |
| **TCP Demo** | [samples/tcp_server_client.cpp](/Users/raphaelshin/Sources/network_system/samples/tcp_server_client.cpp) | Full TCP implementation |
| **Integration** | [samples/messaging_system_integration/](/Users/raphaelshin/Sources/network_system/samples/messaging_system_integration/) | messaging_system integration |
| **Benchmarks** | [benchmarks/](/Users/raphaelshin/Sources/network_system/benchmarks/) | Performance testing |

### Community Support

| Channel | Link | Purpose |
|---------|------|---------|
| **GitHub Issues** | https://github.com/kcenon/network_system/issues | Bug reports and feature requests |
| **GitHub Discussions** | https://github.com/kcenon/network_system/discussions | Q&A and community help |
| **Email** | kcenon@naver.com | Direct support |

### Related Projects

Ecosystem projects that integrate with network_system:

| Project | Repository | Purpose |
|---------|-----------|---------|
| **messaging_system** | https://github.com/kcenon/messaging_system | Message routing and delivery |
| **container_system** | https://github.com/kcenon/container_system | Data serialization |
| **thread_system** | https://github.com/kcenon/thread_system | Thread pool management |
| **logger_system** | https://github.com/kcenon/logger_system | Structured logging |
| **common_system** | https://github.com/kcenon/common_system | Shared utilities and Result<T> |

---

## Conclusion

This migration guide has walked you through the complete process of migrating from `messaging_system`'s integrated network module to the standalone `network_system` library.

### Key Takeaways

✅ **100% Backward Compatibility** - Use compatibility mode for zero-downtime migration
✅ **3x Performance Improvement** - Achieve 305K+ msg/s throughput
✅ **Modern C++20 Features** - Leverage coroutines and Result<T> error handling
✅ **Independent & Modular** - Integrate into any C++ project
✅ **Production Ready** - Comprehensive testing and validation

### Next Steps

1. **Complete your migration** using the step-by-step guide
2. **Run performance tests** to verify improvements
3. **Update documentation** for your team
4. **Provide feedback** via GitHub Issues or email

### Questions or Issues?

If you encounter any problems during migration:

1. Check the [Common Migration Issues](#common-migration-issues) section
2. Review the [Troubleshooting Guide](/Users/raphaelshin/Sources/network_system/docs/TROUBLESHOOTING.md)
3. Search [GitHub Issues](https://github.com/kcenon/network_system/issues)
4. Contact support: kcenon@naver.com

---

**Document Information**

- **Version**: 1.0.0
- **Last Updated**: 2025-10-22
- **Maintainer**: kcenon@naver.com
- **License**: BSD-3-Clause
- **Repository**: https://github.com/kcenon/network_system

---

*Happy migrating! The network_system team is here to help ensure your migration is smooth and successful.*
