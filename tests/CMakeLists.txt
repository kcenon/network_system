##################################################
# Network System Tests Configuration
##################################################

# Enable testing
enable_testing()

##################################################
# Sanitizer Suppression Configuration
##################################################
#
# Configure LSAN (LeakSanitizer) to use suppression file for intentional leaks.
# This prevents false positives from the intentional leak pattern in thread
# integration. See docs/DESIGN_DECISIONS.md for details.

set(NETWORK_LSAN_SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../sanitizers/lsan_suppressions.txt")

if(EXISTS "${NETWORK_LSAN_SUPPRESSIONS}")
    # Set LSAN options for all tests via CTest environment
    set(NETWORK_LSAN_OPTIONS "suppressions=${NETWORK_LSAN_SUPPRESSIONS}")

    # Define a CMake variable for setting test environment
    set(NETWORK_SANITIZER_ENV "LSAN_OPTIONS=${NETWORK_LSAN_OPTIONS}")

    message(STATUS "LSAN suppressions configured: ${NETWORK_LSAN_SUPPRESSIONS}")
endif()

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
        target_compile_definitions(network_unit_tests PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unit_tests PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unit_tests PRIVATE WITH_COMMON_SYSTEM)
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
        target_compile_definitions(network_thread_safety_test PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_safety_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_safety_test PRIVATE WITH_COMMON_SYSTEM)
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
        target_compile_definitions(network_tls_config_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_tls_config_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_tls_config_test
        DISCOVERY_TIMEOUT 60
    )

    # Thread system adapter tests
    if(THREAD_SYSTEM_INCLUDE_DIR)
        add_executable(network_thread_system_adapter_test
            unit/test_thread_system_adapter.cpp
        )

        target_link_libraries(network_thread_system_adapter_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO integration
        setup_asio_integration(network_thread_system_adapter_test)

        # Add system integration paths
        target_include_directories(network_thread_system_adapter_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_system_adapter_test PRIVATE WITH_THREAD_SYSTEM)

        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_thread_system_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_thread_system_adapter_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        if(LOGGER_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_thread_system_adapter_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_thread_system_adapter_test PRIVATE WITH_LOGGER_SYSTEM)
        endif()

        set_target_properties(network_thread_system_adapter_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_thread_system_adapter_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "Thread system adapter tests enabled")
    endif()

    # Thread pool adapters tests (common_system integration)
    if(COMMON_SYSTEM_INCLUDE_DIR)
        add_executable(network_thread_pool_adapters_test
            unit/test_thread_pool_adapters.cpp
        )

        target_link_libraries(network_thread_pool_adapters_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO integration
        setup_asio_integration(network_thread_pool_adapters_test)

        # Add system integration paths
        target_include_directories(network_thread_pool_adapters_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_pool_adapters_test PRIVATE WITH_COMMON_SYSTEM)

        if(THREAD_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_thread_pool_adapters_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_thread_pool_adapters_test PRIVATE WITH_THREAD_SYSTEM)
        endif()

        if(LOGGER_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_thread_pool_adapters_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_thread_pool_adapters_test PRIVATE WITH_LOGGER_SYSTEM)
        endif()

        set_target_properties(network_thread_pool_adapters_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_thread_pool_adapters_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "Thread pool adapters tests enabled")
    endif()

    # TCP socket callback tests
    add_executable(network_tcp_socket_test
        unit/tcp_socket_test.cpp
    )

    target_link_libraries(network_tcp_socket_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_tcp_socket_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_tcp_socket_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_tcp_socket_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_tcp_socket_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_tcp_socket_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "TCP socket tests enabled")

    # Session manager idle timeout tests
    add_executable(network_session_manager_test
        unit/session_manager_test.cpp
    )

    target_link_libraries(network_session_manager_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_session_manager_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_session_manager_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_session_manager_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_session_manager_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_session_manager_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Session manager tests enabled")

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
        target_compile_definitions(network_http2_frame_test PRIVATE WITH_COMMON_SYSTEM)
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
        target_compile_definitions(network_http2_hpack_test PRIVATE WITH_COMMON_SYSTEM)
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
            target_compile_definitions(network_http2_client_test PRIVATE WITH_COMMON_SYSTEM)
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

        ##################################################
        # DTLS Socket Tests
        ##################################################

        # DTLS socket unit tests
        add_executable(network_dtls_socket_test
            unit/test_dtls_socket.cpp
        )

        target_link_libraries(network_dtls_socket_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        # Setup ASIO integration
        setup_asio_integration(network_dtls_socket_test)

        # Add helpers include directory
        target_include_directories(network_dtls_socket_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/helpers
        )

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_dtls_socket_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_dtls_socket_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_dtls_socket_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_dtls_socket_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "DTLS socket tests enabled")
    endif()

    ##################################################
    # Network Metrics Tests
    ##################################################

    # Network metrics unit tests
    add_executable(network_metrics_test
        unit/test_network_metrics.cpp
    )

    target_link_libraries(network_metrics_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_metrics_test)

    # Add helpers include directory
    target_include_directories(network_metrics_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
    )

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_metrics_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_metrics_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_metrics_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_metrics_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_metrics_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_metrics_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_metrics_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_metrics_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Network metrics tests enabled")

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
        target_compile_definitions(network_grpc_frame_test PRIVATE WITH_COMMON_SYSTEM)
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
        target_compile_definitions(network_grpc_client_server_test PRIVATE WITH_COMMON_SYSTEM)
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

    # gRPC service registry tests
    add_executable(network_grpc_service_registry_test
        test_grpc_service_registry.cpp
    )

    target_link_libraries(network_grpc_service_registry_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_grpc_service_registry_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_grpc_service_registry_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_grpc_service_registry_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_grpc_service_registry_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_grpc_service_registry_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "gRPC service registry tests enabled")

    # gRPC official wrapper tests
    add_executable(network_grpc_official_wrapper_test
        test_grpc_official_wrapper.cpp
    )

    target_link_libraries(network_grpc_official_wrapper_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_grpc_official_wrapper_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_grpc_official_wrapper_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_grpc_official_wrapper_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_grpc_official_wrapper_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_grpc_official_wrapper_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "gRPC official wrapper tests enabled")

    # gRPC integration tests
    add_executable(network_grpc_integration_test
        test_grpc_integration.cpp
    )

    target_link_libraries(network_grpc_integration_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_grpc_integration_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_grpc_integration_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_grpc_integration_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_grpc_integration_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_grpc_integration_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "gRPC integration tests enabled")

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
        target_compile_definitions(network_quic_varint_test PRIVATE WITH_COMMON_SYSTEM)
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
        target_compile_definitions(network_quic_frame_test PRIVATE WITH_COMMON_SYSTEM)
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
        target_compile_definitions(network_quic_packet_test PRIVATE WITH_COMMON_SYSTEM)
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
            target_compile_definitions(network_quic_crypto_test PRIVATE WITH_COMMON_SYSTEM)
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

        # QUIC socket tests
        add_executable(network_quic_socket_test
            test_quic_socket.cpp
        )

        target_link_libraries(network_quic_socket_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        # Setup ASIO integration
        setup_asio_integration(network_quic_socket_test)

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_quic_socket_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_quic_socket_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_quic_socket_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        gtest_discover_tests(network_quic_socket_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "QUIC socket tests enabled")
    endif()

    # QUIC stream tests (Phase 3.1)
    add_executable(network_quic_stream_test
        test_quic_stream.cpp
    )

    target_link_libraries(network_quic_stream_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_stream_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_stream_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_stream_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_stream_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_stream_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC stream tests enabled")

    # QUIC connection tests (Phase 3.2)
    add_executable(network_quic_connection_test
        test_quic_connection.cpp
    )

    target_link_libraries(network_quic_connection_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_connection_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_connection_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_connection_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_connection_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_connection_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC connection tests enabled")

    # QUIC loss detection tests (Phase 3.3)
    add_executable(network_quic_loss_detection_test
        test_quic_loss_detection.cpp
    )

    target_link_libraries(network_quic_loss_detection_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_loss_detection_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_loss_detection_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_loss_detection_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_loss_detection_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_loss_detection_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC loss detection tests enabled")

    # QUIC session ticket tests (0-RTT support)
    add_executable(network_quic_session_ticket_test
        test_quic_session_ticket.cpp
    )

    target_link_libraries(network_quic_session_ticket_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_session_ticket_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_session_ticket_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_session_ticket_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_session_ticket_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_quic_session_ticket_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC session ticket tests enabled")

    # QUIC messaging client tests (Phase 4.1)
    add_executable(network_messaging_quic_client_test
        test_messaging_quic_client.cpp
    )

    target_link_libraries(network_messaging_quic_client_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_messaging_quic_client_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_messaging_quic_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_messaging_quic_client_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_messaging_quic_client_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_messaging_quic_client_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC messaging client tests enabled")

    # QUIC messaging server tests (Phase 4.2)
    add_executable(network_messaging_quic_server_test
        test_messaging_quic_server.cpp
    )

    target_link_libraries(network_messaging_quic_server_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_messaging_quic_server_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_messaging_quic_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_messaging_quic_server_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_messaging_quic_server_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_messaging_quic_server_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC messaging server tests enabled")

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
        target_compile_definitions(network_benchmark_tests PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmark_tests PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmark_tests PRIVATE WITH_COMMON_SYSTEM)
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
# gRPC Benchmark Tests
##################################################

if(benchmark_FOUND)
    # gRPC benchmarks
    add_executable(network_grpc_benchmarks
        performance/grpc_benchmarks.cpp
    )

    target_link_libraries(network_grpc_benchmarks PRIVATE
        NetworkSystem
        benchmark::benchmark
        benchmark::benchmark_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_grpc_benchmarks)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_grpc_benchmarks PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_grpc_benchmarks PRIVATE WITH_COMMON_SYSTEM)
    endif()

    # Set properties
    set_target_properties(network_grpc_benchmarks PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Platform-specific optimizations (Release builds only)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(network_grpc_benchmarks PRIVATE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-march=native>
            $<$<CONFIG:RelWithDebInfo>:-O2>
        )
    elseif(MSVC)
        # Only add /O2 in Release builds to avoid conflict with /RTC1 in Debug
        target_compile_options(network_grpc_benchmarks PRIVATE
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:RelWithDebInfo>:/O2>
        )
    endif()

    message(STATUS "gRPC benchmarks enabled")
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
