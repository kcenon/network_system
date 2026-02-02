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
    VERSION 0.1.0.0
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

# Sanitizer options (mutually exclusive - only one can be enabled at a time)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

##################################################
# C++20 Module Support (Experimental)
#
# Enable C++20 modules with CMake 3.28+ when building with module-capable compilers.
# This creates a separate module library target (network_system_modules) that can be
# used instead of the header-based library.
#
# Usage:
#   cmake -DNETWORK_BUILD_MODULES=ON ..
#
# Requirements:
#   - CMake 3.28+
#   - Clang 16+ or GCC 14+ or MSVC 17.4+
##################################################
option(NETWORK_BUILD_MODULES "Build C++20 module version of network_system" OFF)

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

# Required dependencies (Tier 0-1)
option(BUILD_WITH_COMMON_SYSTEM "Build with common_system integration (REQUIRED)" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration (REQUIRED)" ON)

# Optional dependencies - logger_system is now OPTIONAL (runtime binding via GlobalLoggerRegistry)
# Issue #285: Migrated to common_system's ILogger interface
option(BUILD_WITH_LOGGER_SYSTEM "Build with logger_system integration (OPTIONAL - runtime binding)" OFF)

# Optional dependencies
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)

# DEPRECATED: Since issue #342, monitoring integration uses EventBus pattern
# instead of compile-time dependency. This option is kept for backward
# compatibility but has no effect. It will be removed in a future version.
option(BUILD_WITH_MONITORING_SYSTEM "DEPRECATED - monitoring uses EventBus pattern (no compile-time dep)" OFF)

# Official gRPC library integration (issue #360)
# When enabled, wraps the official grpc++ library for production use.
# When disabled, uses the existing prototype implementation.
option(NETWORK_ENABLE_GRPC_OFFICIAL "Use official gRPC library (grpc++) for production" OFF)

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

if(BUILD_WITH_LOGGER_SYSTEM)
    message(STATUS "logger_system integration is ENABLED (optional - will use logger_system_adapter)")
else()
    message(STATUS "logger_system integration is DISABLED (default - uses common_system's ILogger)")
endif()

# Monitoring integration status message
# Note: Since issue #342, monitoring uses EventBus pattern - no compile-time dependency
if(BUILD_WITH_MONITORING_SYSTEM)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "BUILD_WITH_MONITORING_SYSTEM is DEPRECATED")
    message(STATUS "")
    message(STATUS "Since issue #342, network_system uses EventBus-based metric")
    message(STATUS "publishing. No compile-time monitoring_system dependency needed.")
    message(STATUS "External consumers subscribe to network_metric_event via EventBus.")
    message(STATUS "========================================")
    message(STATUS "")
endif()
message(STATUS "Monitoring: EventBus-based metric publishing (no compile-time dependency)")

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

# Sanitizer settings (mutually exclusive)
# Count enabled sanitizers for validation
set(_SANITIZER_COUNT 0)
if(ENABLE_ASAN)
    math(EXPR _SANITIZER_COUNT "${_SANITIZER_COUNT} + 1")
endif()
if(ENABLE_TSAN)
    math(EXPR _SANITIZER_COUNT "${_SANITIZER_COUNT} + 1")
endif()
if(ENABLE_UBSAN)
    math(EXPR _SANITIZER_COUNT "${_SANITIZER_COUNT} + 1")
endif()

if(_SANITIZER_COUNT GREATER 1)
    message(FATAL_ERROR "Only one sanitizer can be enabled at a time. "
        "ENABLE_ASAN=${ENABLE_ASAN}, ENABLE_TSAN=${ENABLE_TSAN}, ENABLE_UBSAN=${ENABLE_UBSAN}")
endif()

# AddressSanitizer configuration
if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
        message(STATUS "AddressSanitizer enabled")
    elseif(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
        message(STATUS "AddressSanitizer enabled (MSVC)")
    endif()
endif()

# ThreadSanitizer configuration
if(ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
        message(STATUS "ThreadSanitizer enabled")
    else()
        message(WARNING "ThreadSanitizer is only supported with GCC and Clang")
    endif()
endif()

# UndefinedBehaviorSanitizer configuration
if(ENABLE_UBSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    else()
        message(WARNING "UndefinedBehaviorSanitizer is only supported with GCC and Clang")
    endif()
endif()

# Mark builds that enable sanitizers for tests and helpers.
if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN)
    add_compile_definitions(NETWORK_SYSTEM_SANITIZER)
endif()

# Disable ASIO's small block recycling allocator when any sanitizer is enabled.
# The recycling allocator uses thread-local storage which can interact poorly
# with sanitizers, causing false positives or undefined behavior reports.
# With this disabled, ASIO uses standard aligned_new() for all allocations.
#
# NOTE: ASIO 1.31.0+ has native fixes for the recycling allocator, so this
# workaround is only needed for older ASIO versions. ASIO 1.32.0 has a bug
# where ASIO_DISABLE_SMALL_BLOCK_RECYCLING causes a compilation error.
# Use SKIP_ASIO_RECYCLING_WORKAROUND=ON when building with ASIO 1.31.0+.
option(SKIP_ASIO_RECYCLING_WORKAROUND "Skip ASIO recycling allocator workaround (for ASIO 1.31.0+)" OFF)
if((ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN) AND NOT SKIP_ASIO_RECYCLING_WORKAROUND)
    add_compile_definitions(ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
    message(STATUS "ASIO small block recycling disabled for sanitizer build")
endif()

# Configure sanitizer test environment (used by gtest discovery and add_test)
set(NETWORK_TEST_ENVIRONMENT "")

if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN)
    list(APPEND NETWORK_TEST_ENVIRONMENT "NETWORK_SYSTEM_SANITIZER=1")
endif()

set(NETWORK_LSAN_SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/sanitizers/lsan_suppressions.txt")
if(ENABLE_ASAN AND EXISTS "${NETWORK_LSAN_SUPPRESSIONS}")
    list(APPEND NETWORK_TEST_ENVIRONMENT "LSAN_OPTIONS=suppressions=${NETWORK_LSAN_SUPPRESSIONS}")
    message(STATUS "LSAN suppressions configured: ${NETWORK_LSAN_SUPPRESSIONS}")
endif()

set(NETWORK_TSAN_SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/tsan_suppressions.txt")
if(ENABLE_TSAN AND EXISTS "${NETWORK_TSAN_SUPPRESSIONS}")
    list(APPEND NETWORK_TEST_ENVIRONMENT
        "TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1:suppressions=${NETWORK_TSAN_SUPPRESSIONS}")
    message(STATUS "TSAN suppressions configured: ${NETWORK_TSAN_SUPPRESSIONS}")
endif()

function(network_gtest_discover_tests target)
    if(NETWORK_TEST_ENVIRONMENT)
        gtest_discover_tests(${target}
            PROPERTIES ENVIRONMENT "${NETWORK_TEST_ENVIRONMENT}"
            ${ARGN}
        )
    else()
        gtest_discover_tests(${target} ${ARGN})
    endif()
endfunction()

function(network_apply_test_environment test_name)
    if(NETWORK_TEST_ENVIRONMENT)
        set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${NETWORK_TEST_ENVIRONMENT}")
    endif()
endfunction()

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
# Modular Libraries (Issue #538)
#
# network-core: Header-only library with core interfaces
# This enables protocol libraries to depend only on interfaces,
# reducing coupling and build times.
##################################################

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-core/CMakeLists.txt)
    add_subdirectory(libs/network-core)
    message(STATUS "network-core library enabled")
endif()

##################################################
# Shared Integration Objects (ODR Fix - Issue #555)
#
# These sources are shared between NetworkSystem and network-tcp.
# Using an OBJECT library ensures symbols are defined exactly once,
# preventing ODR (One Definition Rule) violations that cause SEGV
# in sanitizer tests.
#
# IMPORTANT: This must be defined BEFORE add_subdirectory(libs/network-tcp)
# so that network-tcp can link against it.
##################################################

add_library(network-integration-objects OBJECT
    src/integration/logger_integration.cpp
    src/integration/thread_integration.cpp
    src/integration/io_context_thread_manager.cpp
    src/integration/monitoring_integration.cpp
    src/integration/thread_system_adapter.cpp
    src/integration/thread_pool_bridge.cpp
    src/integration/observability_bridge.cpp
    src/integration/network_system_bridge.cpp
    src/core/network_context.cpp
    src/session/messaging_session.cpp
    src/metrics/network_metrics.cpp
    src/metrics/sliding_histogram.cpp
    src/metrics/histogram.cpp
)

# Configure integration objects with necessary includes and dependencies
target_include_directories(network-integration-objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/network-core/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(network-integration-objects PUBLIC cxx_std_20)

# ASIO setup for integration objects
find_package(asio QUIET CONFIG)
if(TARGET asio::asio)
    target_link_libraries(network-integration-objects PUBLIC asio::asio)
else()
    find_path(ASIO_INCLUDE_DIR
        NAMES asio.hpp
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
        DOC "Path to standalone ASIO headers"
    )
    if(ASIO_INCLUDE_DIR)
        target_include_directories(network-integration-objects SYSTEM PUBLIC
            $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        )
        target_compile_definitions(network-integration-objects PUBLIC ASIO_STANDALONE)
    endif()
endif()

# common_system integration for network-integration-objects
setup_common_system_integration(network-integration-objects)

# Export the OBJECT library for network-tcp to use
set(NETWORK_INTEGRATION_OBJECTS_TARGET network-integration-objects CACHE INTERNAL "Integration objects target")

# network-tcp: TCP protocol implementation library (Issue #554)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-tcp/CMakeLists.txt)
    add_subdirectory(libs/network-tcp)
    message(STATUS "network-tcp library enabled")
endif()

# network-udp: UDP protocol implementation library (Issue #561)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-udp/CMakeLists.txt)
    add_subdirectory(libs/network-udp)
    message(STATUS "network-udp library enabled")
endif()

# network-websocket: WebSocket protocol implementation library (Issue #565)
if(BUILD_WEBSOCKET_SUPPORT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-websocket/CMakeLists.txt)
    add_subdirectory(libs/network-websocket)
    message(STATUS "network-websocket library enabled")
endif()

# network-http2: HTTP/2 protocol implementation library (Issue #567)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-http2/CMakeLists.txt)
    add_subdirectory(libs/network-http2)
    message(STATUS "network-http2 library enabled")
endif()

# network-quic: QUIC protocol implementation library (Issue #569)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-quic/CMakeLists.txt)
    add_subdirectory(libs/network-quic)
    message(STATUS "network-quic library enabled")
endif()

# network-grpc: gRPC protocol implementation library (Issue #571)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-grpc/CMakeLists.txt)
    add_subdirectory(libs/network-grpc)
    message(STATUS "network-grpc library enabled")
endif()

# network-all: Umbrella package aggregating all protocol libraries (Issue #573)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-all/CMakeLists.txt)
    add_subdirectory(libs/network-all)
    message(STATUS "network-all umbrella package enabled")
endif()

##################################################
# Create Main Library
##################################################

add_library(NetworkSystem
    # Main API implementation
    src/network_system.cpp

    # Core implementation (stable TCP/UDP)
    # Note: network_context.cpp is in network-integration-objects to avoid ODR violations
    # UDP messaging moved to libs/network-udp (Issue #561)
    src/core/messaging_client.cpp
    src/core/messaging_server.cpp
    src/core/messaging_udp_client.cpp
    src/core/messaging_udp_server.cpp
    src/core/connection_pool.cpp

    # HTTP layer (stable)
    src/http/http_client.cpp
    src/http/http_server.cpp

    # Experimental protocols
    # reliable_udp_client moved to libs/network-udp (Issue #561)
    src/experimental/quic_client.cpp
    src/experimental/quic_server.cpp

    # Session management
    # Note: messaging_session.cpp is in network-integration-objects to avoid ODR violations
    src/session/quic_session.cpp

    # Core type-erased session management (Issue #522)
    src/core/unified_session_manager.cpp

    # Unified interface adapters (Issue #521 Phase 2)
    # TCP adapters moved to libs/network-tcp (Issue #554)
    # UDP adapters moved to libs/network-udp (Issue #561)
    src/unified/adapters/ws_connection_adapter.cpp
    src/unified/adapters/ws_listener_adapter.cpp
    src/unified/adapters/quic_connection_adapter.cpp
    src/unified/adapters/quic_listener_adapter.cpp

    # Protocol factory functions (Issue #521 Phase 2)
    # TCP protocol moved to libs/network-tcp (Issue #554)
    # UDP protocol moved to libs/network-udp (Issue #561)
    src/protocol/websocket.cpp
    src/protocol/quic.cpp

    # Internal implementation
    # TCP socket moved to libs/network-tcp (Issue #554)
    # UDP socket moved to libs/network-udp (Issue #561)
    src/internal/http_types.cpp
    src/internal/http_parser.cpp
    src/internal/http_error.cpp

    # Utility implementations
    src/utils/resilient_client.cpp
    src/utils/health_monitor.cpp
    src/utils/buffer_pool.cpp
    src/utils/compression_pipeline.cpp

    # Facade layer - simplified protocol creation APIs (Issue #596)
    src/facade/tcp_facade.cpp
    src/facade/udp_facade.cpp
    src/facade/http_facade.cpp
    src/facade/websocket_facade.cpp
    src/facade/quic_facade.cpp

    # Basic integration layer - container_integration and messaging_bridge
    # (Other integration sources are in network-integration-objects to avoid ODR violations)
    # messaging_bridge.cpp is here because it depends on messaging_client/server
    src/integration/container_integration.cpp
    src/integration/messaging_bridge.cpp

    # Tracing implementation (OpenTelemetry-compatible)
    src/tracing/trace_context.cpp
    src/tracing/span.cpp
    src/tracing/exporters.cpp

    # HTTP/2 protocol implementation (frame and hpack are TLS-independent)
    src/protocols/http2/frame.cpp
    src/protocols/http2/hpack.cpp

    # gRPC protocol implementation (prototype)
    src/protocols/grpc/frame.cpp
    src/protocols/grpc/client.cpp
    src/protocols/grpc/server.cpp
    src/protocols/grpc/service_registry.cpp

    # QUIC protocol implementation (RFC 9000)
    src/protocols/quic/varint.cpp
    src/protocols/quic/frame.cpp
    src/protocols/quic/connection_id.cpp
    src/protocols/quic/packet.cpp
    src/protocols/quic/keys.cpp
    src/protocols/quic/crypto.cpp
    src/protocols/quic/session_ticket_store.cpp
    src/protocols/quic/stream.cpp
    src/protocols/quic/stream_manager.cpp
    src/protocols/quic/flow_control.cpp
    src/protocols/quic/transport_params.cpp
    src/protocols/quic/connection.cpp
    src/protocols/quic/connection_id_manager.cpp
    src/protocols/quic/rtt_estimator.cpp
    src/protocols/quic/loss_detector.cpp
    src/protocols/quic/congestion_controller.cpp
    src/protocols/quic/ecn_tracker.cpp
    src/protocols/quic/pmtud_controller.cpp

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
        # secure_tcp_socket moved to libs/network-tcp (Issue #554)
        # secure_messaging_udp moved to libs/network-udp (Issue #561)
        src/internal/dtls_socket.cpp
        src/session/secure_session.cpp
        src/core/secure_messaging_server.cpp
        src/core/secure_messaging_client.cpp
        # HTTP/2 client requires TLS (ALPN negotiation)
        src/protocols/http2/http2_client.cpp
        # HTTP/2 server requires TLS (ALPN negotiation)
        src/protocols/http2/http2_server.cpp
        src/protocols/http2/http2_server_stream.cpp
    )
    target_compile_definitions(NetworkSystem PUBLIC BUILD_TLS_SUPPORT)

    # Find OpenSSL for TLS/SSL
    # Minimum requirement: OpenSSL 3.0.0
    # OpenSSL 1.1.x reached EOL on September 11, 2023 and is no longer supported
    find_package(OpenSSL 3.0.0 REQUIRED)
    target_link_libraries(NetworkSystem PUBLIC OpenSSL::SSL OpenSSL::Crypto)

    message(STATUS "TLS/SSL support enabled (TLS 1.2/1.3)")
    message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
    message(STATUS "  OpenSSL 3.x: Using modern API")
    message(STATUS "  TLS 1.3 support: Available")
    target_compile_definitions(NetworkSystem PRIVATE NETWORK_OPENSSL_3_X)
endif()

# WebSocket support (conditional) - part of HTTP layer
if(BUILD_WEBSOCKET_SUPPORT)
    target_sources(NetworkSystem PRIVATE
        src/internal/websocket_frame.cpp
        src/internal/websocket_handshake.cpp
        src/internal/websocket_protocol.cpp
        src/internal/websocket_socket.cpp
        src/http/websocket_client.cpp
        src/http/websocket_server.cpp
    )
    target_compile_definitions(NetworkSystem PUBLIC BUILD_WEBSOCKET_SUPPORT)

    # Find OpenSSL for SHA-1 hashing in handshake (if not already found)
    if(NOT BUILD_TLS_SUPPORT)
        find_package(OpenSSL 3.0.0 REQUIRED)
        target_link_libraries(NetworkSystem PRIVATE OpenSSL::Crypto)
        message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
        target_compile_definitions(NetworkSystem PRIVATE NETWORK_OPENSSL_3_X)
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

# Official gRPC library support (conditional)
# See ADR-001 for architecture decision details
# gRPC detection is done in NetworkSystemDependencies.cmake via find_grpc_library()
if(NETWORK_ENABLE_GRPC_OFFICIAL AND GRPC_FOUND)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Official gRPC Integration (Issue #360)")
    message(STATUS "========================================")

    # Add gRPC wrapper sources
    target_sources(NetworkSystem PRIVATE
        src/protocols/grpc/grpc_official_wrapper.cpp
    )

    if(GRPC_CMAKE_CONFIG)
        # Use CMake config targets (preferred)
        target_link_libraries(NetworkSystem PRIVATE
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
        )

        # Link abseil if found separately (gRPC 1.50+ requirement)
        if(ABSEIL_FOUND)
            # abseil is typically linked transitively through gRPC::grpc++
            # but explicit linking may be needed for some configurations
            message(STATUS "  Abseil: Linked via gRPC")
        endif()

        message(STATUS "  gRPC++: Enabled (CMake config)")
        message(STATUS "  Reflection: Enabled")
        if(DEFINED Protobuf_VERSION)
            message(STATUS "  Protobuf: ${Protobuf_VERSION}")
        endif()
    elseif(GRPC_PKGCONFIG)
        # Use pkg-config libraries
        target_include_directories(NetworkSystem PRIVATE
            ${GRPCPP_INCLUDE_DIRS}
            ${PROTOBUF_INCLUDE_DIRS}
        )

        target_link_libraries(NetworkSystem PRIVATE
            ${GRPCPP_LIBRARIES}
            ${PROTOBUF_LIBRARIES}
        )

        message(STATUS "  gRPC++: Enabled (pkg-config)")
        message(STATUS "  Reflection: Manual linking required")
    endif()

    # Define compile-time flag
    target_compile_definitions(NetworkSystem PUBLIC NETWORK_GRPC_OFFICIAL=1)

    message(STATUS "")
    message(STATUS "Note: Existing gRPC API (grpc_server, grpc_client) remains unchanged.")
    message(STATUS "      Internal implementation now uses official gRPC library.")
    message(STATUS "========================================")
elseif(NETWORK_ENABLE_GRPC_OFFICIAL AND NOT GRPC_FOUND)
    # gRPC was requested but not found - warning already printed by find_grpc_library()
    message(STATUS "Official gRPC: Disabled (gRPC not found, using prototype)")
else()
    message(STATUS "Official gRPC: Disabled (using prototype implementation)")
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

# Link to network-core for shared interfaces (Issue #538, #539)
if(TARGET network-core)
    target_link_libraries(NetworkSystem PUBLIC network-core)
endif()

# Link to network-tcp for TCP protocol support (Issue #554)
if(TARGET network-tcp)
    target_link_libraries(NetworkSystem PUBLIC network-tcp)
endif()

# Link to network-udp for UDP protocol support (Issue #618)
if(TARGET network-udp)
    target_link_libraries(NetworkSystem PUBLIC network-udp)
endif()

# Note: Integration objects are provided by network-tcp (ODR fix - Issue #555)
# NetworkSystem gets these symbols through linking with network-tcp

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
        target_compile_definitions(verify_build PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE WITH_LOGGER_SYSTEM)
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
# C++20 Module Library Target
##################################################

if(NETWORK_BUILD_MODULES)
    if(CMAKE_VERSION VERSION_LESS "3.28")
        message(WARNING "C++20 modules require CMake 3.28+. Disabling module build.")
        set(NETWORK_BUILD_MODULES OFF)
    else()
        message(STATUS "C++20 module build enabled")

        # Create module library target
        add_library(network_system_modules)
        add_library(kcenon::network_modules ALIAS network_system_modules)

        # Set C++20 standard for module target
        target_compile_features(network_system_modules PUBLIC cxx_std_20)

        # Enable module scanning
        set_target_properties(network_system_modules PROPERTIES
            CXX_SCAN_FOR_MODULES ON
        )

        # Add module source files
        target_sources(network_system_modules
            PUBLIC FILE_SET CXX_MODULES
            FILES
                # Primary module
                src/modules/network.cppm
                # Partitions
                src/modules/core.cppm
                src/modules/tcp.cppm
                src/modules/udp.cppm
                src/modules/ssl.cppm
        )

        # Include directories for module target
        target_include_directories(network_system_modules PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        )

        # Link dependencies
        # Note: Module targets for common_system and thread_system must be available
        if(TARGET kcenon::common_modules)
            target_link_libraries(network_system_modules PUBLIC kcenon::common_modules)
        endif()
        if(TARGET kcenon::thread_modules)
            target_link_libraries(network_system_modules PUBLIC kcenon::thread_modules)
        endif()

        # Link ASIO
        if(TARGET asio::asio)
            target_link_libraries(network_system_modules PRIVATE asio::asio)
        elseif(TARGET asio)
            target_link_libraries(network_system_modules PRIVATE asio)
        elseif(ASIO_INCLUDE_DIR)
            target_include_directories(network_system_modules PRIVATE ${ASIO_INCLUDE_DIR})
        endif()

        # Link OpenSSL for SSL partition
        if(BUILD_TLS_SUPPORT AND OPENSSL_FOUND)
            target_link_libraries(network_system_modules PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        endif()

        # Define compile-time flags
        target_compile_definitions(network_system_modules PUBLIC
            KCENON_NETWORK_MODULES=1
            KCENON_USE_MODULES=1
        )

        message(STATUS "C++20 module target: network_system_modules")
    endif()
endif()

##################################################
# Build Summary
##################################################

message(STATUS "========================================")
message(STATUS "NetworkSystem Build Configuration:")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Dependency Chain (Tier 4):")
message(STATUS "  network_system")
message(STATUS "    ├── common_system (Tier 0): ${BUILD_WITH_COMMON_SYSTEM} [REQUIRED - provides ILogger, EventBus]")
message(STATUS "    ├── thread_system (Tier 1): ${BUILD_WITH_THREAD_SYSTEM} [REQUIRED]")
message(STATUS "    ├── logger_system (Tier 2): ${BUILD_WITH_LOGGER_SYSTEM} [OPTIONAL - runtime binding]")
message(STATUS "    ├── container_system (Tier 1): ${BUILD_WITH_CONTAINER_SYSTEM}")
message(STATUS "    └── monitoring_system (Tier 3): EventBus [NO COMPILE-TIME DEP]")
message(STATUS "")
message(STATUS "Monitoring Mode: EventBus-based metric publishing")
message(STATUS "  - Metrics published via common_system's EventBus")
message(STATUS "  - External consumers subscribe to network_metric_event")
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
message(STATUS "  Official gRPC: ${NETWORK_ENABLE_GRPC_OFFICIAL}")
message(STATUS "  C++20 Modules: ${NETWORK_BUILD_MODULES}")
message(STATUS "")
message(STATUS "Sanitizers:")
message(STATUS "  AddressSanitizer: ${ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer: ${ENABLE_TSAN}")
message(STATUS "  UndefinedBehaviorSanitizer: ${ENABLE_UBSAN}")
message(STATUS "  Coverage: ${ENABLE_COVERAGE}")
message(STATUS "========================================")
