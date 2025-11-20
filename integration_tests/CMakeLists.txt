##################################################
# Network System Integration Tests
##################################################

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find Threads (required for all tests)
find_package(Threads REQUIRED)

# Find or fetch GTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found locally, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Create GTest::gtest and GTest::gtest_main aliases for compatibility
    if(NOT TARGET GTest::gtest)
        add_library(GTest::gtest ALIAS gtest)
    endif()
    if(NOT TARGET GTest::gtest_main)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()

    message(STATUS "GTest fetched successfully")
endif()

##################################################
# Common Setup for All Tests
##################################################

# Include directories for all tests
set(INTEGRATION_TEST_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
    ${CMAKE_SOURCE_DIR}/include
)

# Common compile definitions
set(INTEGRATION_TEST_DEFINITIONS)
if(BUILD_WITH_COMMON_SYSTEM)
    list(APPEND INTEGRATION_TEST_DEFINITIONS BUILD_WITH_COMMON_SYSTEM)
endif()
if(BUILD_WITH_LOGGER_SYSTEM)
    list(APPEND INTEGRATION_TEST_DEFINITIONS BUILD_WITH_LOGGER_SYSTEM)
endif()
if(BUILD_WITH_THREAD_SYSTEM)
    list(APPEND INTEGRATION_TEST_DEFINITIONS BUILD_WITH_THREAD_SYSTEM)
endif()

##################################################
# Connection Lifecycle Tests
##################################################

add_executable(connection_lifecycle_test
    scenarios/connection_lifecycle_test.cpp
)

target_include_directories(connection_lifecycle_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(connection_lifecycle_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO and FMT integration (required for network_system headers)
setup_asio_integration(connection_lifecycle_test)
setup_fmt_integration(connection_lifecycle_test)

# Add system integration paths
if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(connection_lifecycle_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(connection_lifecycle_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(connection_lifecycle_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(connection_lifecycle_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(connection_lifecycle_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(connection_lifecycle_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

# Don't add suite-level test to prevent port conflicts during parallel execution
# add_test(NAME ConnectionLifecycleTests
#     COMMAND connection_lifecycle_test
# )

include(GoogleTest)
gtest_discover_tests(connection_lifecycle_test)

##################################################
# Protocol Integration Tests
##################################################

add_executable(protocol_integration_test
    scenarios/protocol_integration_test.cpp
)

target_include_directories(protocol_integration_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(protocol_integration_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO and FMT integration (required for network_system headers)
setup_asio_integration(protocol_integration_test)
setup_fmt_integration(protocol_integration_test)

# Add system integration paths
if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(protocol_integration_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(protocol_integration_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(protocol_integration_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(protocol_integration_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(protocol_integration_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(protocol_integration_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

# Don't add suite-level test to prevent port conflicts during parallel execution
# add_test(NAME ProtocolIntegrationTests
#     COMMAND protocol_integration_test
# )

gtest_discover_tests(protocol_integration_test)

##################################################
# Network Performance Tests
##################################################

add_executable(network_performance_test
    performance/network_performance_test.cpp
)

target_include_directories(network_performance_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(network_performance_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO and FMT integration (required for network_system headers)
setup_asio_integration(network_performance_test)
setup_fmt_integration(network_performance_test)

# Add system integration paths
if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_performance_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_performance_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_performance_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_performance_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(network_performance_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(network_performance_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

# Don't add suite-level test to prevent port conflicts during parallel execution
# add_test(NAME NetworkPerformanceTests
#     COMMAND network_performance_test
# )

gtest_discover_tests(network_performance_test)

##################################################
# Error Handling Tests
##################################################

add_executable(error_handling_test
    failures/error_handling_test.cpp
)

target_include_directories(error_handling_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(error_handling_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO and FMT integration (required for network_system headers)
setup_asio_integration(error_handling_test)
setup_fmt_integration(error_handling_test)

# Add system integration paths
if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(error_handling_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(error_handling_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(error_handling_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(error_handling_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(error_handling_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(error_handling_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

# Don't add suite-level test to prevent port conflicts during parallel execution
# add_test(NAME ErrorHandlingTests
#     COMMAND error_handling_test
# )

gtest_discover_tests(error_handling_test)

##################################################
# Test Framework Library
##################################################

add_library(network_integration_test_framework STATIC
    framework/memory_profiler.cpp
    framework/result_writer.cpp
)

target_include_directories(network_integration_test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(network_integration_test_framework PUBLIC
    Threads::Threads
)

# Setup ASIO and FMT integration for framework library
setup_asio_integration(network_integration_test_framework)
setup_fmt_integration(network_integration_test_framework)

set_target_properties(network_integration_test_framework PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

##################################################
# TCP Load Tests
##################################################

add_executable(tcp_load_test
    performance/tcp_load_test.cpp
)

target_include_directories(tcp_load_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(tcp_load_test PRIVATE
    NetworkSystem
    network_integration_test_framework
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(tcp_load_test)
setup_fmt_integration(tcp_load_test)

if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(tcp_load_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(tcp_load_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(tcp_load_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(tcp_load_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(tcp_load_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(tcp_load_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

gtest_discover_tests(tcp_load_test)

##################################################
# UDP Load Tests
##################################################

add_executable(udp_load_test
    performance/udp_load_test.cpp
)

target_include_directories(udp_load_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(udp_load_test PRIVATE
    NetworkSystem
    network_integration_test_framework
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(udp_load_test)
setup_fmt_integration(udp_load_test)

if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(udp_load_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(udp_load_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(udp_load_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(udp_load_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(udp_load_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(udp_load_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

gtest_discover_tests(udp_load_test)

##################################################
# WebSocket Load Tests
##################################################

add_executable(websocket_load_test
    performance/websocket_load_test.cpp
)

target_include_directories(websocket_load_test PRIVATE
    ${INTEGRATION_TEST_INCLUDE_DIRS}
)

target_link_libraries(websocket_load_test PRIVATE
    NetworkSystem
    network_integration_test_framework
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(websocket_load_test)
setup_fmt_integration(websocket_load_test)

if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(websocket_load_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(websocket_load_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(websocket_load_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
endif()

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(websocket_load_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
endif()

if(INTEGRATION_TEST_DEFINITIONS)
    target_compile_definitions(websocket_load_test PRIVATE
        ${INTEGRATION_TEST_DEFINITIONS}
    )
endif()

set_target_properties(websocket_load_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/integration_tests
)

gtest_discover_tests(websocket_load_test)

##################################################
# Test Summary
##################################################

message(STATUS "========================================")
message(STATUS "Network Integration Tests Configuration:")
message(STATUS "  Connection Lifecycle Tests: Enabled")
message(STATUS "  Protocol Integration Tests: Enabled")
message(STATUS "  Network Performance Tests: Enabled")
message(STATUS "  Error Handling Tests: Enabled")
message(STATUS "  TCP Load Tests: Enabled")
message(STATUS "  UDP Load Tests: Enabled")
message(STATUS "  WebSocket Load Tests: Enabled")
message(STATUS "  Total Test Suites: 7")
message(STATUS "========================================")
