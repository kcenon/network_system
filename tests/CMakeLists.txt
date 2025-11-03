##################################################
# Network System Tests Configuration
##################################################

# Enable testing
enable_testing()

# Find test dependencies
find_package(GTest QUIET)
find_package(benchmark QUIET)

##################################################
# Helper macro to add FMT library to test targets
##################################################
macro(link_fmt_library target_name)
    if(FMT_LIBRARY)
        target_link_libraries(${target_name} PRIVATE ${FMT_LIBRARY})
    endif()
endmacro()

##################################################
# Unit Tests
##################################################

if(GTest_FOUND OR GTEST_FOUND)
    # Create unit test executable
    add_executable(network_unit_tests
        unit_tests.cpp
    )

    # Link dependencies
    target_link_libraries(network_unit_tests PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO and FMT integration (required for network_system headers)
    setup_asio_integration(network_unit_tests)
    setup_fmt_integration(network_unit_tests)
    link_fmt_library(network_unit_tests)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    # Set properties
    set_target_properties(network_unit_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Don't add suite-level test to prevent port conflicts during parallel execution
    # add_test(NAME NetworkUnitTests COMMAND network_unit_tests)

    # Enable test discovery
    include(GoogleTest)
    gtest_discover_tests(network_unit_tests)

    # Thread safety tests (Phase 1 - Task 1.6)
    add_executable(network_thread_safety_test
        thread_safety_tests.cpp
    )

    target_link_libraries(network_thread_safety_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO and FMT integration (required for network_system headers)
    setup_asio_integration(network_thread_safety_test)
    setup_fmt_integration(network_thread_safety_test)
    link_fmt_library(network_thread_safety_test)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_thread_safety_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Don't add suite-level test to prevent port conflicts during parallel execution
    # add_test(NAME NetworkThreadSafetyTests COMMAND network_thread_safety_test)

    gtest_discover_tests(network_thread_safety_test)

    # TLS configuration tests
    add_executable(network_tls_config_test
        unit/test_tls_config.cpp
    )

    target_link_libraries(network_tls_config_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO and FMT integration
    setup_asio_integration(network_tls_config_test)
    setup_fmt_integration(network_tls_config_test)
    link_fmt_library(network_tls_config_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_tls_config_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_tls_config_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_tls_config_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_tls_config_test)

    # WebSocket frame tests
    if(BUILD_WEBSOCKET_SUPPORT)
        add_executable(network_websocket_frame_test
            unit/websocket_frame_test.cpp
        )

        target_link_libraries(network_websocket_frame_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO and FMT integration
        setup_asio_integration(network_websocket_frame_test)
        setup_fmt_integration(network_websocket_frame_test)
    link_fmt_library(network_websocket_frame_test)

        set_target_properties(network_websocket_frame_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_websocket_frame_test)
        message(STATUS "WebSocket frame tests enabled")

        # WebSocket handshake tests
        add_executable(network_websocket_handshake_test
            unit/websocket_handshake_test.cpp
        )

        target_link_libraries(network_websocket_handshake_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO and FMT integration
        setup_asio_integration(network_websocket_handshake_test)
        setup_fmt_integration(network_websocket_handshake_test)
    link_fmt_library(network_websocket_handshake_test)

        set_target_properties(network_websocket_handshake_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_websocket_handshake_test)
        message(STATUS "WebSocket handshake tests enabled")

        # WebSocket protocol tests
        add_executable(network_websocket_protocol_test
            unit/websocket_protocol_test.cpp
        )

        target_link_libraries(network_websocket_protocol_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO and FMT integration
        setup_asio_integration(network_websocket_protocol_test)
        setup_fmt_integration(network_websocket_protocol_test)
    link_fmt_library(network_websocket_protocol_test)

        set_target_properties(network_websocket_protocol_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_websocket_protocol_test)
        message(STATUS "WebSocket protocol tests enabled")
    endif()

    ##################################################
    # Phase 5: Thread System Integration Tests
    ##################################################

    # Thread Pool Manager Unit Tests
    add_executable(network_thread_pool_manager_test
        thread_pool_manager_test.cpp
    )

    target_link_libraries(network_thread_pool_manager_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO and FMT integration
    setup_asio_integration(network_thread_pool_manager_test)
    setup_fmt_integration(network_thread_pool_manager_test)
    link_fmt_library(network_thread_pool_manager_test)

    # Add system integration paths
    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_pool_manager_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_pool_manager_test PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_pool_manager_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_pool_manager_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_thread_pool_manager_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_thread_pool_manager_test)
    message(STATUS "Thread pool manager tests enabled")

    # Full System Integration Tests
    add_executable(network_integration_test_full
        integration_test_full.cpp
    )

    target_link_libraries(network_integration_test_full PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO and FMT integration
    setup_asio_integration(network_integration_test_full)
    setup_fmt_integration(network_integration_test_full)
    link_fmt_library(network_integration_test_full)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_integration_test_full PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_integration_test_full PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_integration_test_full PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_integration_test_full PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_integration_test_full PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_integration_test_full PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_integration_test_full PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_integration_test_full PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_integration_test_full PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_integration_test_full)
    message(STATUS "Full system integration tests enabled")

else()
    message(WARNING "GTest not found - unit tests will not be built")
endif()

##################################################
# Benchmark Tests
##################################################

# Temporarily disabled due to API incompatibilities
# TODO: Update benchmark_tests.cpp to use new network_system API
if(FALSE AND benchmark_FOUND)
    # Create benchmark executable
    add_executable(network_benchmark_tests
        benchmark_tests.cpp
    )

    # Link dependencies
    target_link_libraries(network_benchmark_tests PRIVATE
        NetworkSystem
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
    )

    # Setup ASIO and FMT integration (required for network_system headers)
    setup_asio_integration(network_benchmark_tests)
    setup_fmt_integration(network_benchmark_tests)
    link_fmt_library(network_benchmark_tests)

    # Add system integration paths
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    # Set properties
    set_target_properties(network_benchmark_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Platform-specific optimizations
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(network_benchmark_tests PRIVATE
            -O3
            -march=native
        )
    elseif(MSVC)
        target_compile_options(network_benchmark_tests PRIVATE
            /O2
        )
    endif()
else()
    message(WARNING "Google Benchmark not found - benchmark tests will not be built")
endif()

##################################################
# Integration Tests
##################################################

# Add integration tests subdirectory if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt)
    add_subdirectory(integration)
    message(STATUS "Integration tests enabled")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()