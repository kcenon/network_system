cmake_minimum_required(VERSION 3.16)

# Set policy for FindBoost module
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

##################################################
# Network System CMakeLists.txt
#
# Independent high-performance network system
##################################################

project(NetworkSystem
    VERSION 1.0.0
    DESCRIPTION "Independent High-Performance Network System"
    LANGUAGES CXX
)

##################################################
# C++ Standard
##################################################

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

##################################################
# Options
##################################################

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_WEBSOCKET_SUPPORT "Build WebSocket protocol support (RFC 6455)" ON)
option(BUILD_MESSAGING_BRIDGE "Build messaging_system bridge" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(NETWORK_BUILD_BENCHMARKS "Build network system benchmarks" OFF)
option(NETWORK_BUILD_INTEGRATION_TESTS "Build network system integration tests" ON)

##################################################
# Dependency Configuration (Option A Structure)
#
# Tier 4: network_system
#   Required:
#     - common_system (Tier 0)
#     - thread_system (Tier 1)
#     - logger_system (Tier 2)
#   Optional:
#     - container_system (Tier 1) - serialization
#     - monitoring_system (Tier 3) - metrics collection
#
# Note: monitoring_system is OPTIONAL to prevent circular dependency.
#       network_system provides basic_monitoring for standalone operation.
##################################################

# Required dependencies (Tier 0-2)
option(BUILD_WITH_COMMON_SYSTEM "Build with common_system integration (REQUIRED)" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration (REQUIRED)" ON)
option(BUILD_WITH_LOGGER_SYSTEM "Build with logger_system integration (REQUIRED)" ON)

# Optional dependencies
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)

# Optional integration (Tier 3 - prevents circular dependency)
option(BUILD_WITH_MONITORING_SYSTEM "Build with monitoring_system integration (OPTIONAL - prevents circular dep)" OFF)

##################################################
# Dependency Validation (Advisory)
#
# Note: These dependencies are RECOMMENDED for full functionality.
#       Disabling them may result in reduced features but allows
#       standalone builds for testing and CI purposes.
##################################################

if(NOT BUILD_WITH_COMMON_SYSTEM)
    message(WARNING "common_system (Tier 0) is disabled - some features may be unavailable")
endif()

if(NOT BUILD_WITH_THREAD_SYSTEM)
    message(WARNING "thread_system (Tier 1) is disabled - some features may be unavailable")
endif()

if(NOT BUILD_WITH_LOGGER_SYSTEM)
    message(WARNING "logger_system (Tier 2) is disabled - some features may be unavailable")
endif()

# Optional integration status message
if(BUILD_WITH_MONITORING_SYSTEM)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "BUILD_WITH_MONITORING_SYSTEM is ENABLED")
    message(STATUS "")
    message(STATUS "This creates an optional dependency on Tier 3.")
    message(STATUS "network_system will use monitoring_system_adapter")
    message(STATUS "for metrics collection if monitoring_system is available.")
    message(STATUS "")
    message(STATUS "Note: If monitoring_system also has MONITORING_WITH_NETWORK_SYSTEM=ON,")
    message(STATUS "      this creates a bidirectional optional dependency.")
    message(STATUS "      Both systems use adapter patterns to avoid compile-time cycles.")
    message(STATUS "========================================")
    message(STATUS "")
else()
    message(STATUS "BUILD_WITH_MONITORING_SYSTEM is OFF (default)")
    message(STATUS "network_system will use basic_monitoring (standalone mode)")
endif()

# Respect global BUILD_INTEGRATION_TESTS flag if set
if(DEFINED BUILD_INTEGRATION_TESTS)
    if(BUILD_INTEGRATION_TESTS)
        set(_NETWORK_BUILD_IT_VALUE ON)
    else()
        set(_NETWORK_BUILD_IT_VALUE OFF)
    endif()
    set(NETWORK_BUILD_INTEGRATION_TESTS ${_NETWORK_BUILD_IT_VALUE} CACHE BOOL "Build network system integration tests" FORCE)
endif()

##################################################
# Global Configuration
##################################################

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Coverage settings
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -latomic")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -latomic")
        endif()
    endif()
endif()

##################################################
# Include CMake Modules
##################################################

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(NetworkSystemFeatures)
include(NetworkSystemDependencies)
include(NetworkSystemIntegration)
include(NetworkSystemInstall)

##################################################
# Dependency Detection
##################################################

find_package(PkgConfig QUIET)
find_network_system_dependencies()

##################################################
# Feature Detection
##################################################

check_network_system_features()

##################################################
# Create Main Library
##################################################

add_library(NetworkSystem
    # Main API implementation
    src/network_system.cpp

    # Core implementation
    src/core/network_context.cpp
    src/core/messaging_client.cpp
    src/core/messaging_server.cpp
    src/core/messaging_udp_client.cpp
    src/core/messaging_udp_server.cpp
    src/core/reliable_udp_client.cpp
    src/core/connection_pool.cpp
    src/core/http_client.cpp
    src/core/http_server.cpp
    src/core/messaging_quic_client.cpp

    # Session management
    src/session/messaging_session.cpp

    # Internal implementation
    src/internal/tcp_socket.cpp
    src/internal/udp_socket.cpp
    src/internal/send_coroutine.cpp
    src/internal/pipeline.cpp
    src/internal/http_types.cpp
    src/internal/http_parser.cpp
    src/internal/http_error.cpp

    # Utility implementations
    src/utils/resilient_client.cpp
    src/utils/health_monitor.cpp
    src/utils/buffer_pool.cpp
    src/utils/compression_pipeline.cpp

    # Basic integration layer (always built - no external dependencies)
    src/integration/logger_integration.cpp
    src/integration/thread_integration.cpp
    src/integration/container_integration.cpp
    src/integration/monitoring_integration.cpp

    # Metrics implementation
    src/metrics/network_metrics.cpp

    # HTTP/2 protocol implementation (frame and hpack are TLS-independent)
    src/protocols/http2/frame.cpp
    src/protocols/http2/hpack.cpp

    # gRPC protocol implementation (prototype)
    src/protocols/grpc/frame.cpp
    src/protocols/grpc/client.cpp
    src/protocols/grpc/server.cpp

    # QUIC protocol implementation (RFC 9000)
    src/protocols/quic/varint.cpp
    src/protocols/quic/frame.cpp
    src/protocols/quic/connection_id.cpp
    src/protocols/quic/packet.cpp
    src/protocols/quic/keys.cpp
    src/protocols/quic/crypto.cpp
    src/protocols/quic/stream.cpp
    src/protocols/quic/stream_manager.cpp
    src/protocols/quic/flow_control.cpp
    src/protocols/quic/transport_params.cpp
    src/protocols/quic/connection.cpp
    src/protocols/quic/rtt_estimator.cpp
    src/protocols/quic/loss_detector.cpp
    src/protocols/quic/congestion_controller.cpp

    # QUIC socket (UDP wrapper with packet protection)
    src/internal/quic_socket.cpp
)

# Suppress warnings inherited from parent project (especially from ASIO)
# WARNING: Suppressions removed for strict code quality check. 
# Fix the code instead of suppressing warnings!
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # target_compile_options(NetworkSystem PRIVATE
    #     -Wno-sign-conversion
    #     -Wno-conversion
    #     -Wno-implicit-fallthrough
    #     -Wno-shadow
    #     -Wno-unused-parameter
    #     -Wno-unused-but-set-variable
    #     -Wno-unused-variable
    # )
elseif(MSVC)
    # target_compile_options(NetworkSystem PRIVATE
    #     /wd4244  # conversion from 'type1' to 'type2', possible loss of data
    #     /wd4267  # conversion from 'size_t' to 'type', possible loss of data
    #     /wd4100  # unreferenced formal parameter
    #     /wd4101  # unreferenced local variable
    #     /wd4456  # declaration hides previous local declaration
    #     /wd4457  # declaration hides function parameter
    # )
endif()

# TLS/SSL support (conditional)
option(BUILD_TLS_SUPPORT "Build with TLS/SSL support for secure messaging" ON)
option(BUILD_LZ4_COMPRESSION "Build with LZ4 compression support" ON)

if(BUILD_TLS_SUPPORT)
    target_sources(NetworkSystem PRIVATE
        src/internal/secure_tcp_socket.cpp
        src/internal/dtls_socket.cpp
        src/session/secure_session.cpp
        src/core/secure_messaging_server.cpp
        src/core/secure_messaging_client.cpp
        src/core/secure_messaging_udp_client.cpp
        src/core/secure_messaging_udp_server.cpp
        # HTTP/2 client requires TLS (ALPN negotiation)
        src/protocols/http2/http2_client.cpp
    )
    target_compile_definitions(NetworkSystem PUBLIC BUILD_TLS_SUPPORT)

    # Find OpenSSL for TLS/SSL (require 1.1.1+ for TLS 1.3 support)
    find_package(OpenSSL 1.1.1 REQUIRED)
    target_link_libraries(NetworkSystem PUBLIC OpenSSL::SSL OpenSSL::Crypto)

    message(STATUS "TLS/SSL support enabled (TLS 1.2/1.3)")
    message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")

    # Verify TLS 1.3 support
    if(OPENSSL_VERSION VERSION_LESS "1.1.1")
        message(WARNING "OpenSSL ${OPENSSL_VERSION} detected. TLS 1.3 requires OpenSSL 1.1.1 or newer.")
    else()
        message(STATUS "  TLS 1.3 support: Available")
    endif()
endif()

# WebSocket support (conditional)
if(BUILD_WEBSOCKET_SUPPORT)
    target_sources(NetworkSystem PRIVATE
        src/internal/websocket_frame.cpp
        src/internal/websocket_handshake.cpp
        src/internal/websocket_protocol.cpp
        src/internal/websocket_socket.cpp
        src/core/messaging_ws_client.cpp
        src/core/messaging_ws_server.cpp
    )
    target_compile_definitions(NetworkSystem PUBLIC BUILD_WEBSOCKET_SUPPORT)

    # Find OpenSSL for SHA-1 hashing in handshake (if not already found)
    if(NOT BUILD_TLS_SUPPORT)
        find_package(OpenSSL 1.1.1 REQUIRED)
        target_link_libraries(NetworkSystem PRIVATE OpenSSL::Crypto)
        message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
    endif()

    message(STATUS "WebSocket support enabled")
endif()

# LZ4 compression support (conditional)
if(BUILD_LZ4_COMPRESSION)
    # Try to find lz4 library directly (works better on both macOS and Linux)
    find_library(LZ4_LIBRARY NAMES lz4)
    find_path(LZ4_INCLUDE_DIR NAMES lz4.h)

    if(LZ4_LIBRARY AND LZ4_INCLUDE_DIR)
        target_compile_definitions(NetworkSystem PUBLIC BUILD_LZ4_COMPRESSION)
        target_include_directories(NetworkSystem PRIVATE ${LZ4_INCLUDE_DIR})
        target_link_libraries(NetworkSystem PRIVATE ${LZ4_LIBRARY})
        message(STATUS "LZ4 compression enabled")
        message(STATUS "  LZ4 library: ${LZ4_LIBRARY}")
        message(STATUS "  LZ4 include: ${LZ4_INCLUDE_DIR}")
    else()
        # Fallback to pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LZ4 QUIET liblz4)
            if(LZ4_FOUND)
                target_compile_definitions(NetworkSystem PUBLIC BUILD_LZ4_COMPRESSION)
                target_include_directories(NetworkSystem PRIVATE ${LZ4_INCLUDE_DIRS})
                target_link_libraries(NetworkSystem PRIVATE ${LZ4_LIBRARIES})
                message(STATUS "LZ4 compression enabled (via pkg-config)")
                message(STATUS "  LZ4 version: ${LZ4_VERSION}")
            else()
                message(WARNING "LZ4 library not found. Compression support will be disabled.")
                message(STATUS "Install LZ4: brew install lz4 (macOS) or apt-get install liblz4-dev (Linux)")
            endif()
        else()
            message(WARNING "LZ4 library not found. Compression support will be disabled.")
            message(STATUS "Install LZ4: brew install lz4 (macOS) or apt-get install liblz4-dev (Linux)")
        endif()
    endif()
endif()

# ZLIB compression support (for gzip/deflate)
option(BUILD_ZLIB_COMPRESSION "Build with ZLIB compression support (gzip/deflate)" ON)

if(BUILD_ZLIB_COMPRESSION)
    find_package(ZLIB QUIET)

    if(ZLIB_FOUND)
        target_compile_definitions(NetworkSystem PUBLIC BUILD_ZLIB_COMPRESSION)
        target_link_libraries(NetworkSystem PRIVATE ZLIB::ZLIB)
        message(STATUS "ZLIB compression enabled (gzip/deflate)")
        message(STATUS "  ZLIB version: ${ZLIB_VERSION_STRING}")
    else()
        message(WARNING "ZLIB library not found. gzip/deflate compression will be disabled.")
        message(STATUS "Install ZLIB: vcpkg install zlib or system package manager")
    endif()
endif()

# Set target properties
set_target_properties(NetworkSystem PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(NetworkSystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

##################################################
# Configure Integrations
##################################################

setup_network_system_integrations(NetworkSystem)

# Messaging bridge (optional - requires external systems)
if(BUILD_MESSAGING_BRIDGE)
    target_sources(NetworkSystem PRIVATE
        src/integration/messaging_bridge.cpp
    )
    target_compile_definitions(NetworkSystem PRIVATE BUILD_MESSAGING_BRIDGE)
endif()

##################################################
# Build Verification Test
##################################################

# Only build verify_build if all required libraries are available
# Disabled by default as it requires external system libraries
option(BUILD_VERIFY_BUILD "Build verify_build executable" OFF)

if(BUILD_VERIFY_BUILD)
    add_executable(verify_build verify_build.cpp)
    target_link_libraries(verify_build PRIVATE NetworkSystem)

    # Add system include paths to verify_build
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(BUILD_MESSAGING_BRIDGE)
        target_compile_definitions(verify_build PRIVATE BUILD_MESSAGING_BRIDGE)
    endif()

    # Configure ASIO for verify_build
    setup_asio_integration(verify_build)

    set_target_properties(verify_build PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endif()

##################################################
# Tests and Samples
##################################################

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Building network_system tests")
    if(BUILD_VERIFY_BUILD AND TARGET verify_build)
        add_test(NAME verify_build_test COMMAND verify_build)
    endif()

    # Add tests subdirectory if it exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
        add_subdirectory(tests)
        message(STATUS "Network system unit tests and thread safety tests enabled")
    endif()

    # Add integration tests subdirectory if enabled
    if(NETWORK_BUILD_INTEGRATION_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests/CMakeLists.txt)
        add_subdirectory(integration_tests)
        message(STATUS "Network system integration tests enabled")
    endif()
endif()

if(BUILD_SAMPLES)
    message(STATUS "Building network_system samples")

    # Add samples subdirectory if it exists
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/samples/CMakeLists.txt)
        add_subdirectory(samples)
        message(STATUS "Network system samples enabled")
    endif()
endif()

##################################################
# Benchmarks
##################################################

if(NETWORK_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
    message(STATUS "Network benchmarks will be built")
endif()

##################################################
# Installation
##################################################

# Skip installation in unified build to avoid export conflicts
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    setup_network_system_install()
else()
    message(STATUS "Installation skipped in unified build mode")
endif()

##################################################
# CPack Configuration
##################################################

# CPack configuration temporarily disabled due to recursive include issue
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPack.cmake)
#     include(cmake/CPack.cmake)
# endif()

##################################################
# Build Summary
##################################################

message(STATUS "========================================")
message(STATUS "NetworkSystem Build Configuration:")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Dependency Chain (Tier 4):")
message(STATUS "  network_system")
message(STATUS "    ├── common_system (Tier 0): ${BUILD_WITH_COMMON_SYSTEM} [REQUIRED]")
message(STATUS "    ├── thread_system (Tier 1): ${BUILD_WITH_THREAD_SYSTEM} [REQUIRED]")
message(STATUS "    ├── logger_system (Tier 2): ${BUILD_WITH_LOGGER_SYSTEM} [REQUIRED]")
message(STATUS "    ├── container_system (Tier 1): ${BUILD_WITH_CONTAINER_SYSTEM}")
message(STATUS "    └── monitoring_system (Tier 3): ${BUILD_WITH_MONITORING_SYSTEM} [OPTIONAL]")
message(STATUS "")
if(NOT BUILD_WITH_MONITORING_SYSTEM)
    message(STATUS "Standalone Mode: ENABLED (basic_monitoring)")
else()
    message(STATUS "Standalone Mode: DISABLED (using monitoring_system_adapter)")
endif()
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Integration tests: ${NETWORK_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Samples: ${BUILD_SAMPLES}")
message(STATUS "  Verify build: ${BUILD_VERIFY_BUILD}")
message(STATUS "  TLS/SSL support: ${BUILD_TLS_SUPPORT}")
message(STATUS "  WebSocket support: ${BUILD_WEBSOCKET_SUPPORT}")
message(STATUS "  Messaging bridge: ${BUILD_MESSAGING_BRIDGE}")
message(STATUS "  Benchmarks: ${NETWORK_BUILD_BENCHMARKS}")
message(STATUS "========================================")
