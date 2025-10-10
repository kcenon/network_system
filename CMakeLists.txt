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
option(BUILD_SAMPLES "Build samples" OFF)
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration" ON)
option(BUILD_WITH_LOGGER_SYSTEM "Build with logger_system integration" ON)
option(BUILD_WITH_COMMON_SYSTEM "Build with common_system integration" ON)
option(BUILD_MESSAGING_BRIDGE "Build messaging_system bridge" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(NETWORK_BUILD_BENCHMARKS "Build network system benchmarks" OFF)
option(NETWORK_BUILD_INTEGRATION_TESTS "Build network system integration tests" ON)

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
    # Core implementation
    src/core/messaging_client.cpp
    src/core/messaging_server.cpp

    # Session management
    src/session/messaging_session.cpp

    # Internal implementation
    src/internal/tcp_socket.cpp
    src/internal/send_coroutine.cpp
    src/internal/pipeline.cpp

    # Basic integration layer (always built - no external dependencies)
    src/integration/logger_integration.cpp
    src/integration/thread_integration.cpp
    src/integration/container_integration.cpp
)

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

# Configure ASIO and FMT for verify_build
setup_asio_integration(verify_build)
setup_fmt_integration(verify_build)

set_target_properties(verify_build PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

##################################################
# Tests and Samples
##################################################

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Building network_system tests")
    add_test(NAME verify_build_test COMMAND verify_build)

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

setup_network_system_install()

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
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Integration tests: ${NETWORK_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Samples: ${BUILD_SAMPLES}")
message(STATUS "  container_system: ${BUILD_WITH_CONTAINER_SYSTEM}")
message(STATUS "  thread_system: ${BUILD_WITH_THREAD_SYSTEM}")
message(STATUS "  logger_system: ${BUILD_WITH_LOGGER_SYSTEM}")
message(STATUS "  common_system: ${BUILD_WITH_COMMON_SYSTEM}")
message(STATUS "  Messaging bridge: ${BUILD_MESSAGING_BRIDGE}")
message(STATUS "  Benchmarks: ${NETWORK_BUILD_BENCHMARKS}")
message(STATUS "========================================")
