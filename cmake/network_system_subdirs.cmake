##################################################
# network_system_subdirs.cmake
#
# Conditional add_subdirectory() orchestration for the
# protocol libraries and the auxiliary trees
# (tests, examples, benchmarks, fuzz).
#
# Each block follows the same pattern:
#   1. check the gating option (and the existence of the
#      directory's CMakeLists.txt where applicable)
#   2. add_subdirectory(...)
#   3. log a "<name> ... enabled" status line
#
# This module assumes:
#   - network_system_targets.cmake has already been
#     included so that network-integration-objects and
#     network_system exist before any libs/network-*
#     subdirectory tries to link against them.
#   - All option(...) declarations have already been
#     processed.
##################################################

##################################################
# Protocol libraries (depend on
# network-integration-objects defined in
# network_system_targets.cmake).
##################################################

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
# Tests
##################################################

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Building network_system tests")
    if(BUILD_VERIFY_BUILD AND TARGET verify_build)
        add_test(NAME verify_build_test COMMAND verify_build)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
        add_subdirectory(tests)
        message(STATUS "Network system unit tests and thread safety tests enabled")
    endif()

    if(NETWORK_BUILD_INTEGRATION_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests/CMakeLists.txt)
        add_subdirectory(integration_tests)
        message(STATUS "Network system integration tests enabled")
    endif()
endif()

##################################################
# Examples
##################################################

if(BUILD_EXAMPLES)
    message(STATUS "Building network_system examples")

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt)
        add_subdirectory(examples)
        message(STATUS "Network system examples enabled")
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
# Fuzzing
##################################################

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/CMakeLists.txt)
    add_subdirectory(fuzz)
endif()
