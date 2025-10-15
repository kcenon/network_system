##################################################
# Network System Integration Tests
##################################################

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find GTest
find_package(GTest REQUIRED)

# Find Threads (required for all tests)
find_package(Threads REQUIRED)

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

add_test(NAME ConnectionLifecycleTests
    COMMAND connection_lifecycle_test
)

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

add_test(NAME ProtocolIntegrationTests
    COMMAND protocol_integration_test
)

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

add_test(NAME NetworkPerformanceTests
    COMMAND network_performance_test
)

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

add_test(NAME ErrorHandlingTests
    COMMAND error_handling_test
)

gtest_discover_tests(error_handling_test)

##################################################
# Test Summary
##################################################

message(STATUS "========================================")
message(STATUS "Network Integration Tests Configuration:")
message(STATUS "  Connection Lifecycle Tests: Enabled")
message(STATUS "  Protocol Integration Tests: Enabled")
message(STATUS "  Network Performance Tests: Enabled")
message(STATUS "  Error Handling Tests: Enabled")
message(STATUS "  Total Test Suites: 4")
message(STATUS "========================================")
