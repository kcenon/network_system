##################################################
# Network System Tests Configuration
##################################################

# Enable testing
enable_testing()

# Find test dependencies
find_package(GTest QUIET)
find_package(benchmark QUIET)

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

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(network_unit_tests)

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
    gtest_discover_tests(network_unit_tests
        DISCOVERY_TIMEOUT 60
    )

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

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(network_thread_safety_test)

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

    gtest_discover_tests(network_thread_safety_test
        DISCOVERY_TIMEOUT 60
    )

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

    # Setup ASIO integration
    setup_asio_integration(network_tls_config_test)

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

    gtest_discover_tests(network_tls_config_test
        DISCOVERY_TIMEOUT 60
    )

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

        # Setup ASIO integration
        setup_asio_integration(network_websocket_frame_test)

        set_target_properties(network_websocket_frame_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_websocket_frame_test
            DISCOVERY_TIMEOUT 60
        )
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

        # Setup ASIO integration
        setup_asio_integration(network_websocket_handshake_test)

        set_target_properties(network_websocket_handshake_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_websocket_handshake_test
            DISCOVERY_TIMEOUT 60
        )
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

        # Setup ASIO integration
        setup_asio_integration(network_websocket_protocol_test)

        set_target_properties(network_websocket_protocol_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_websocket_protocol_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "WebSocket protocol tests enabled")
    endif()

    ##################################################
    # HTTP/2 Protocol Tests
    ##################################################

    # HTTP/2 frame tests
    add_executable(network_http2_frame_test
        test_http2_frame.cpp
    )

    target_link_libraries(network_http2_frame_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_http2_frame_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http2_frame_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http2_frame_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_http2_frame_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_http2_frame_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "HTTP/2 frame tests enabled")

    # HTTP/2 HPACK tests
    add_executable(network_http2_hpack_test
        test_http2_hpack.cpp
    )

    target_link_libraries(network_http2_hpack_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_http2_hpack_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http2_hpack_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http2_hpack_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_http2_hpack_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_http2_hpack_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "HTTP/2 HPACK tests enabled")

    # HTTP/2 client tests (requires TLS support)
    if(BUILD_TLS_SUPPORT)
        add_executable(network_http2_client_test
            test_http2_client.cpp
        )

        target_link_libraries(network_http2_client_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        # Setup ASIO integration
        setup_asio_integration(network_http2_client_test)

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_http2_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_http2_client_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_http2_client_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_http2_client_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "HTTP/2 client tests enabled")
    endif()

    ##################################################
    # gRPC Protocol Tests
    ##################################################

    # gRPC frame and status tests
    add_executable(network_grpc_frame_test
        test_grpc_frame.cpp
    )

    target_link_libraries(network_grpc_frame_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_grpc_frame_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_grpc_frame_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_grpc_frame_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_grpc_frame_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_grpc_frame_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "gRPC frame tests enabled")

    # gRPC client/server tests
    add_executable(network_grpc_client_server_test
        test_grpc_client_server.cpp
    )

    target_link_libraries(network_grpc_client_server_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_grpc_client_server_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_grpc_client_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_grpc_client_server_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_grpc_client_server_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_grpc_client_server_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "gRPC client/server tests enabled")

    ##################################################
    # QUIC Protocol Tests
    ##################################################

    # QUIC varint tests
    add_executable(network_quic_varint_test
        test_quic_varint.cpp
    )

    target_link_libraries(network_quic_varint_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_varint_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_varint_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_varint_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_varint_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_varint_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC varint tests enabled")

    # QUIC frame tests
    add_executable(network_quic_frame_test
        test_quic_frame.cpp
    )

    target_link_libraries(network_quic_frame_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_frame_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_frame_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_frame_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_frame_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_frame_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC frame tests enabled")

    # QUIC packet tests
    add_executable(network_quic_packet_test
        test_quic_packet.cpp
    )

    target_link_libraries(network_quic_packet_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_packet_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_packet_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_packet_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_packet_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_packet_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC packet tests enabled")

    # QUIC crypto tests (requires OpenSSL)
    if(BUILD_TLS_SUPPORT)
        add_executable(network_quic_crypto_test
            test_quic_crypto.cpp
        )

        target_link_libraries(network_quic_crypto_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        # Setup ASIO integration
        setup_asio_integration(network_quic_crypto_test)

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_quic_crypto_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_quic_crypto_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_quic_crypto_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_quic_crypto_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "QUIC crypto tests enabled")
    endif()

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

    # Setup ASIO integration (required for network_system headers)
    setup_asio_integration(network_benchmark_tests)

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

# Add failure scenario tests subdirectory if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/failure/CMakeLists.txt)
    add_subdirectory(failure)
    message(STATUS "Failure scenario tests enabled")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()
