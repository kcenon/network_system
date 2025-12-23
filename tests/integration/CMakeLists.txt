##################################################
# Network System Integration Tests CMakeLists.txt
##################################################

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest QUIET)

if(NOT GTest_FOUND AND NOT GTEST_FOUND)
    message(WARNING "GTest not found - integration tests will not be built")
    return()
endif()

##################################################
# Integration Test Executables
##################################################

# E2E Test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_e2e.cpp)
    add_executable(test_e2e test_e2e.cpp)

    target_link_libraries(test_e2e PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(test_e2e)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_e2e PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_e2e PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_e2e PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_e2e PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_e2e PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_e2e PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_e2e PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_e2e PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(test_e2e PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME E2ETests COMMAND test_e2e)

    message(STATUS "Integration test configured: test_e2e")
endif()

# Integration Test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_integration.cpp)
    add_executable(stress_test test_integration.cpp)

    target_link_libraries(stress_test PRIVATE
        NetworkSystem
        Threads::Threads
    )

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(stress_test)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(stress_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(stress_test PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(stress_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(stress_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(stress_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(stress_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(stress_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(stress_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(stress_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    message(STATUS "Integration test configured: stress_test")
endif()

# Compatibility Test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_compatibility.cpp)
    add_executable(benchmark test_compatibility.cpp)

    target_link_libraries(benchmark PRIVATE
        NetworkSystem
        Threads::Threads
    )

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(benchmark)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(benchmark PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(benchmark PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(benchmark PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(benchmark PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(benchmark PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(benchmark PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(benchmark PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(benchmark PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(benchmark PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    message(STATUS "Integration test configured: benchmark")
endif()

# WebSocket E2E Test
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_websocket_e2e.cpp)
    add_executable(test_websocket_e2e test_websocket_e2e.cpp)

    target_link_libraries(test_websocket_e2e PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(test_websocket_e2e)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_websocket_e2e PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_websocket_e2e PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_websocket_e2e PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_websocket_e2e PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_websocket_e2e PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_websocket_e2e PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_websocket_e2e PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_websocket_e2e PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(test_websocket_e2e PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME WebSocketE2ETests COMMAND test_websocket_e2e)

    # Exclude from sanitizer tests due to timing-sensitive operations
    # that can cause false positives or timeouts under sanitizers
    set_tests_properties(WebSocketE2ETests PROPERTIES
        LABELS "integration;websocket;no_sanitizer"
        TIMEOUT 120
    )

    message(STATUS "Integration test configured: test_websocket_e2e")
endif()

# QUIC E2E Test
if(BUILD_QUIC_SUPPORT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_quic_e2e.cpp)
    add_executable(test_quic_e2e test_quic_e2e.cpp)

    target_link_libraries(test_quic_e2e PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(test_quic_e2e)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_quic_e2e PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_quic_e2e PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_quic_e2e PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_quic_e2e PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_quic_e2e PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_quic_e2e PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(test_quic_e2e PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(test_quic_e2e PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(test_quic_e2e PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    add_test(NAME QuicE2ETests COMMAND test_quic_e2e)

    # Exclude from sanitizer tests due to timing-sensitive operations
    # that can cause false positives or timeouts under sanitizers
    set_tests_properties(QuicE2ETests PROPERTIES
        LABELS "integration;quic;no_sanitizer"
        TIMEOUT 180
    )

    message(STATUS "Integration test configured: test_quic_e2e")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()
