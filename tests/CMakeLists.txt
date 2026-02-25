##################################################
# Network System Tests Configuration
##################################################

# Enable testing
enable_testing()

# Find test dependencies
find_package(Threads REQUIRED)
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
    network_gtest_discover_tests(network_unit_tests
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

    network_gtest_discover_tests(network_thread_safety_test
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

    network_gtest_discover_tests(network_tls_config_test
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

        network_gtest_discover_tests(network_thread_system_adapter_test
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

        network_gtest_discover_tests(network_thread_pool_adapters_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "Thread pool adapters tests enabled")
    endif()

    # Thread pool bridge tests
    add_executable(network_thread_pool_bridge_test
        unit/test_thread_pool_bridge.cpp
    )

    target_link_libraries(network_thread_pool_bridge_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_thread_pool_bridge_test)

    # Network system bridge tests
    add_executable(network_system_bridge_test
        unit/test_network_system_bridge.cpp
    )

    target_link_libraries(network_system_bridge_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_system_bridge_test)

    # Add system integration paths if available
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_pool_bridge_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_pool_bridge_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_pool_bridge_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_pool_bridge_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_thread_pool_bridge_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_thread_pool_bridge_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_thread_pool_bridge_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_thread_pool_bridge_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Thread pool bridge tests enabled")

    # Add system integration paths for network_system_bridge_test
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_system_bridge_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_system_bridge_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_system_bridge_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_system_bridge_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_system_bridge_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_system_bridge_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_system_bridge_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_system_bridge_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Network system bridge tests enabled")

    # Observability bridge tests
    add_executable(network_observability_bridge_test
        unit/test_observability_bridge.cpp
    )

    target_link_libraries(network_observability_bridge_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_observability_bridge_test)

    # Add system integration paths if available
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_observability_bridge_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_observability_bridge_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_observability_bridge_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_observability_bridge_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_observability_bridge_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_observability_bridge_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_observability_bridge_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_observability_bridge_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Observability bridge tests enabled")

    # Messaging bridge interface tests
    add_executable(network_messaging_bridge_interface_test
        unit/test_messaging_bridge_interface.cpp
    )

    target_link_libraries(network_messaging_bridge_interface_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_messaging_bridge_interface_test)

    # Add system integration paths if available
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_messaging_bridge_interface_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_messaging_bridge_interface_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_messaging_bridge_interface_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_messaging_bridge_interface_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_messaging_bridge_interface_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_messaging_bridge_interface_test PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    set_target_properties(network_messaging_bridge_interface_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_messaging_bridge_interface_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Messaging bridge interface tests enabled")

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

    network_gtest_discover_tests(network_tcp_socket_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "TCP socket tests enabled")

    # Socket concepts tests (C++20 concepts for socket abstraction)
    add_executable(network_socket_concepts_test
        unit/socket_concepts_test.cpp
    )

    target_link_libraries(network_socket_concepts_test PRIVATE
        NetworkSystem
        network-udp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_socket_concepts_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_socket_concepts_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_socket_concepts_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_socket_concepts_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_socket_concepts_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Socket concepts tests enabled")

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

    network_gtest_discover_tests(network_session_manager_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Session manager tests enabled")

    # Session manager base template tests (Phase 2.4)
    add_executable(network_session_manager_base_test
        unit/session_manager_base_test.cpp
    )

    target_link_libraries(network_session_manager_base_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_session_manager_base_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_session_manager_base_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_session_manager_base_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_session_manager_base_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_session_manager_base_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Session manager base template tests enabled")

    # WebSocket session manager tests (Phase 2.4)
    add_executable(network_ws_session_manager_test
        unit/ws_session_manager_test.cpp
    )

    target_link_libraries(network_ws_session_manager_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_ws_session_manager_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_ws_session_manager_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_ws_session_manager_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_ws_session_manager_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_ws_session_manager_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "WebSocket session manager tests enabled")

    # Session manager integration tests (Phase 2.4)
    add_executable(network_session_manager_integration_test
        integration/session_manager_integration_test.cpp
    )

    target_link_libraries(network_session_manager_integration_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_session_manager_integration_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_session_manager_integration_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_session_manager_integration_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_session_manager_integration_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_session_manager_integration_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Session manager integration tests enabled")

    # Note: Circuit breaker tests moved to common_system (issue #315)
    # network_system now uses common_system's circuit_breaker implementation

    # Composition utilities tests (lifecycle_manager, connection_state, callback_manager)
    add_executable(network_composition_utilities_test
        unit/test_composition_utilities.cpp
    )

    target_link_libraries(network_composition_utilities_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_composition_utilities_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_composition_utilities_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_composition_utilities_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_composition_utilities_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_composition_utilities_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Composition utilities tests enabled")

    # Interface tests
    add_executable(network_interfaces_test
        unit/test_interfaces.cpp
    )

    target_link_libraries(network_interfaces_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_interfaces_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_interfaces_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_interfaces_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_interfaces_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_interfaces_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Interface tests enabled")

    # Policy and Protocol tests
    add_executable(network_policy_protocol_test
        unit/test_policy_protocol.cpp
    )

    target_link_libraries(network_policy_protocol_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_policy_protocol_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_policy_protocol_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_policy_protocol_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_policy_protocol_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_policy_protocol_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Policy and protocol tests enabled")

    # Unified messaging client tests
    add_executable(network_unified_messaging_client_test
        unit/test_unified_messaging_client.cpp
    )

    target_link_libraries(network_unified_messaging_client_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_unified_messaging_client_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_messaging_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_messaging_client_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_messaging_client_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_messaging_client_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_messaging_client_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_messaging_client_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_unified_messaging_client_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_unified_messaging_client_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Unified messaging client tests enabled")

    # Unified messaging server tests
    add_executable(network_unified_messaging_server_test
        unit/test_unified_messaging_server.cpp
    )

    target_link_libraries(network_unified_messaging_server_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_unified_messaging_server_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_messaging_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_messaging_server_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_messaging_server_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_messaging_server_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_messaging_server_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_messaging_server_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_unified_messaging_server_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_unified_messaging_server_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Unified messaging server tests enabled")

    # UDP composition tests
    add_executable(network_udp_composition_test
        unit/test_udp_composition.cpp
    )

    target_link_libraries(network_udp_composition_test PRIVATE
        NetworkSystem
        network-udp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_udp_composition_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_composition_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_composition_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_composition_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_composition_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_composition_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_composition_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_udp_composition_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_udp_composition_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "UDP composition tests enabled")

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

        network_gtest_discover_tests(network_websocket_frame_test
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

        network_gtest_discover_tests(network_websocket_handshake_test
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

        network_gtest_discover_tests(network_websocket_protocol_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "WebSocket protocol tests enabled")

        # WebSocket messaging client tests (Phase 3 unified pattern)
        add_executable(network_messaging_ws_client_test
            test_messaging_ws_client.cpp
        )

        target_link_libraries(network_messaging_ws_client_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO integration
        setup_asio_integration(network_messaging_ws_client_test)

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_messaging_ws_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_messaging_ws_client_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_messaging_ws_client_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        network_gtest_discover_tests(network_messaging_ws_client_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "WebSocket messaging client tests enabled")

        # WebSocket messaging server tests (Phase 3 unified pattern)
        add_executable(network_messaging_ws_server_test
            test_messaging_ws_server.cpp
        )

        target_link_libraries(network_messaging_ws_server_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        # Setup ASIO integration
        setup_asio_integration(network_messaging_ws_server_test)

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_messaging_ws_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_messaging_ws_server_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_messaging_ws_server_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        network_gtest_discover_tests(network_messaging_ws_server_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "WebSocket messaging server tests enabled")
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

    network_gtest_discover_tests(network_http2_frame_test
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

    network_gtest_discover_tests(network_http2_hpack_test
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

        network_gtest_discover_tests(network_http2_client_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "HTTP/2 client tests enabled")

        # HTTP/2 server tests
        add_executable(network_http2_server_test
            test_http2_server.cpp
        )

        target_link_libraries(network_http2_server_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        # Setup ASIO integration
        setup_asio_integration(network_http2_server_test)

        # Add system integration paths
        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_http2_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_http2_server_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(network_http2_server_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        network_gtest_discover_tests(network_http2_server_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "HTTP/2 server tests enabled")

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

        network_gtest_discover_tests(network_dtls_socket_test
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

    network_gtest_discover_tests(network_metrics_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Network metrics tests enabled")

    ##################################################
    # Histogram Tests
    ##################################################

    # Histogram unit tests
    add_executable(network_histogram_test
        unit/test_histogram.cpp
    )

    target_link_libraries(network_histogram_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_histogram_test)

    # Add helpers include directory
    target_include_directories(network_histogram_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
    )

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_histogram_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_histogram_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_histogram_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_histogram_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_histogram_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_histogram_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_histogram_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_histogram_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Histogram tests enabled")

    ##################################################
    # Tracing Tests (OpenTelemetry-compatible)
    ##################################################

    # Tracing unit tests
    add_executable(network_tracing_test
        unit/test_tracing.cpp
    )

    target_link_libraries(network_tracing_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_tracing_test)

    # Add helpers include directory
    target_include_directories(network_tracing_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
    )

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_tracing_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_tracing_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_tracing_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_tracing_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_tracing_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_tracing_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_tracing_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_tracing_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Tracing tests enabled")

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

    network_gtest_discover_tests(network_grpc_frame_test
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

    network_gtest_discover_tests(network_grpc_client_server_test
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

    network_gtest_discover_tests(network_grpc_service_registry_test
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

    network_gtest_discover_tests(network_grpc_official_wrapper_test
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

    network_gtest_discover_tests(network_grpc_integration_test
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

    network_gtest_discover_tests(network_quic_varint_test
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

    network_gtest_discover_tests(network_quic_frame_test
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

    network_gtest_discover_tests(network_quic_packet_test
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

        network_gtest_discover_tests(network_quic_crypto_test
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

        network_gtest_discover_tests(network_quic_socket_test
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

    network_gtest_discover_tests(network_quic_stream_test
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

    network_gtest_discover_tests(network_quic_connection_test
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

    network_gtest_discover_tests(network_quic_loss_detection_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC loss detection tests enabled")

    # QUIC PMTUD tests
    add_executable(network_quic_pmtud_test
        test_quic_pmtud.cpp
    )

    target_link_libraries(network_quic_pmtud_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_pmtud_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_pmtud_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_pmtud_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_pmtud_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_quic_pmtud_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC PMTUD tests enabled")

    # QUIC ECN (Explicit Congestion Notification) tests
    add_executable(network_quic_ecn_test
        test_quic_ecn.cpp
    )

    target_link_libraries(network_quic_ecn_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_ecn_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_ecn_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_ecn_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_ecn_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_quic_ecn_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC ECN tests enabled")

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

    network_gtest_discover_tests(network_quic_session_ticket_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC session ticket tests enabled")

    # QUIC flow control, congestion control, and RTT estimator tests
    add_executable(network_quic_flow_congestion_rtt_test
        test_quic_flow_congestion_rtt.cpp
    )

    target_link_libraries(network_quic_flow_congestion_rtt_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_quic_flow_congestion_rtt_test)

    # Add system integration paths
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_quic_flow_congestion_rtt_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_quic_flow_congestion_rtt_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_quic_flow_congestion_rtt_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_quic_flow_congestion_rtt_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC flow control, congestion control, and RTT estimator tests enabled")

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

    network_gtest_discover_tests(network_messaging_quic_client_test
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

    network_gtest_discover_tests(network_messaging_quic_server_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "QUIC messaging server tests enabled")

    ##################################################
    # Unified Interface Tests (Issue #521 Phase 1)
    ##################################################

    # Unified interfaces tests
    add_executable(network_unified_interfaces_test
        unified/unified_interfaces_test.cpp
    )

    target_link_libraries(network_unified_interfaces_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_unified_interfaces_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_interfaces_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_interfaces_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_unified_interfaces_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_unified_interfaces_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Unified interfaces tests enabled")

    ##################################################
    # Type Erasure Infrastructure Tests (Issue #524)
    ##################################################

    # Session type erasure tests (Phase 1 of #522)
    add_executable(network_session_type_erasure_test
        unit/session_type_erasure_test.cpp
    )

    target_link_libraries(network_session_type_erasure_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_session_type_erasure_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_session_type_erasure_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_session_type_erasure_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_session_type_erasure_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_session_type_erasure_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Session type erasure tests enabled")

    # Unified session manager tests (Phase 2 of #522)
    add_executable(network_unified_session_manager_test
        unit/unified_session_manager_test.cpp
    )

    target_link_libraries(network_unified_session_manager_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_unified_session_manager_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_unified_session_manager_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_unified_session_manager_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_unified_session_manager_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_unified_session_manager_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Unified session manager tests enabled")

    ##################################################
    # Connection Pool Tests (Issue #694)
    ##################################################

    add_executable(network_connection_pool_test
        unit/connection_pool_test.cpp
    )

    target_link_libraries(network_connection_pool_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_connection_pool_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_connection_pool_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_connection_pool_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_connection_pool_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_connection_pool_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_connection_pool_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_connection_pool_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_connection_pool_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_connection_pool_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Connection pool tests enabled")

    ##################################################
    # Buffer Pool Tests (Issue #703)
    ##################################################

    add_executable(network_buffer_pool_test
        unit/buffer_pool_test.cpp
    )

    target_link_libraries(network_buffer_pool_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_buffer_pool_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_buffer_pool_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_buffer_pool_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_buffer_pool_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_buffer_pool_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_buffer_pool_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_buffer_pool_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_buffer_pool_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_buffer_pool_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Buffer pool tests enabled")

    ##################################################
    # Compression Pipeline Tests (Issue #703)
    ##################################################

    add_executable(network_compression_pipeline_test
        unit/compression_pipeline_test.cpp
    )

    target_link_libraries(network_compression_pipeline_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_compression_pipeline_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_compression_pipeline_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_compression_pipeline_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_compression_pipeline_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_compression_pipeline_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_compression_pipeline_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_compression_pipeline_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_compression_pipeline_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_compression_pipeline_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Compression pipeline tests enabled")

    ##################################################
    # Resilient Client Tests (Issue #703)
    ##################################################

    add_executable(network_resilient_client_test
        unit/resilient_client_test.cpp
    )

    target_link_libraries(network_resilient_client_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_resilient_client_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_resilient_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_resilient_client_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_resilient_client_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_resilient_client_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_resilient_client_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_resilient_client_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_resilient_client_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_resilient_client_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "Resilient client tests enabled")

    ##################################################
    # Secure Messaging Client Tests (Issue #694)
    ##################################################

    if(BUILD_TLS_SUPPORT)
        add_executable(network_secure_messaging_client_test
            unit/secure_messaging_client_test.cpp
        )

        target_link_libraries(network_secure_messaging_client_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        setup_asio_integration(network_secure_messaging_client_test)

        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_secure_messaging_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_secure_messaging_client_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        if(THREAD_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_secure_messaging_client_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_secure_messaging_client_test PRIVATE WITH_THREAD_SYSTEM)
        endif()

        if(LOGGER_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_secure_messaging_client_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_secure_messaging_client_test PRIVATE WITH_LOGGER_SYSTEM)
        endif()

        set_target_properties(network_secure_messaging_client_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        network_gtest_discover_tests(network_secure_messaging_client_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "Secure messaging client tests enabled")

        ##################################################
        # Secure Messaging Server Tests (Issue #694)
        ##################################################

        add_executable(network_secure_messaging_server_test
            unit/secure_messaging_server_test.cpp
        )

        target_link_libraries(network_secure_messaging_server_test PRIVATE
            NetworkSystem
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            OpenSSL::SSL
            OpenSSL::Crypto
        )

        setup_asio_integration(network_secure_messaging_server_test)

        target_include_directories(network_secure_messaging_server_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/helpers
        )

        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_secure_messaging_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_secure_messaging_server_test PRIVATE WITH_COMMON_SYSTEM)
        endif()

        if(THREAD_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_secure_messaging_server_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_secure_messaging_server_test PRIVATE WITH_THREAD_SYSTEM)
        endif()

        if(LOGGER_SYSTEM_INCLUDE_DIR)
            target_include_directories(network_secure_messaging_server_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(network_secure_messaging_server_test PRIVATE WITH_LOGGER_SYSTEM)
        endif()

        set_target_properties(network_secure_messaging_server_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        network_gtest_discover_tests(network_secure_messaging_server_test
            DISCOVERY_TIMEOUT 60
        )
        message(STATUS "Secure messaging server tests enabled")
    endif()

    ##################################################
    # UDP Functional Tests (Issue #695)
    ##################################################

    add_executable(network_udp_basic_test
        udp_basic_test.cpp
    )

    target_link_libraries(network_udp_basic_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    target_compile_definitions(network_udp_basic_test PRIVATE
        NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS
    )

    setup_asio_integration(network_udp_basic_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_basic_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_basic_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_basic_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_basic_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_basic_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_basic_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_udp_basic_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_udp_basic_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "UDP functional tests enabled")

    ##################################################
    # HTTP Client/Server Unit Tests (Issue #695)
    ##################################################

    add_executable(network_http_client_test
        unit/http_client_test.cpp
    )

    target_link_libraries(network_http_client_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_http_client_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_client_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_client_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_client_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_client_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_client_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_http_client_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_http_client_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "HTTP client tests enabled")

    add_executable(network_http_server_test
        unit/http_server_test.cpp
    )

    target_link_libraries(network_http_server_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_http_server_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_server_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_server_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_server_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_server_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_server_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_http_server_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_http_server_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "HTTP server tests enabled")

    ##################################################
    # HTTP Parser Unit Tests (Issue #708)
    ##################################################

    add_executable(network_http_parser_test
        unit/http_parser_test.cpp
    )

    target_link_libraries(network_http_parser_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_http_parser_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_parser_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_parser_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_parser_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_parser_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http_parser_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http_parser_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_http_parser_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_http_parser_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "HTTP parser tests enabled")

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
# Facade Tests (Issue #596)
##################################################

# TCP facade tests
add_executable(network_tcp_facade_test
    test_tcp_facade.cpp
)

target_link_libraries(network_tcp_facade_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_tcp_facade_test)

# Add system integration paths if available
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_tcp_facade_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_tcp_facade_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_tcp_facade_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_tcp_facade_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "TCP facade tests enabled")

# UDP facade tests
add_executable(network_udp_facade_test
    test_udp_facade.cpp
)

target_link_libraries(network_udp_facade_test PRIVATE
    NetworkSystem
    network-udp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_udp_facade_test)

# Add system integration paths if available
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_udp_facade_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_udp_facade_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_udp_facade_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_udp_facade_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "UDP facade tests enabled")

# HTTP facade tests
add_executable(network_http_facade_test
    test_http_facade.cpp
)

target_link_libraries(network_http_facade_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_http_facade_test)

# Add system integration paths if available
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_http_facade_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_http_facade_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_http_facade_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_http_facade_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "HTTP facade tests enabled")

# WebSocket facade tests
if(BUILD_WEBSOCKET_SUPPORT)
    add_executable(network_websocket_facade_test
        test_websocket_facade.cpp
    )

    target_link_libraries(network_websocket_facade_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    # Setup ASIO integration
    setup_asio_integration(network_websocket_facade_test)

    # Add system integration paths if available
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_websocket_facade_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_websocket_facade_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_websocket_facade_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_websocket_facade_test
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "WebSocket facade tests enabled")
endif()

# Memory profiler and session timeout tests
add_executable(network_memory_profiler_session_timeout_test
    test_memory_profiler_session_timeout.cpp
    ${CMAKE_SOURCE_DIR}/src/internal/utils/memory_profiler.cpp
)

target_link_libraries(network_memory_profiler_session_timeout_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_memory_profiler_session_timeout_test)

# Add system integration paths
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_memory_profiler_session_timeout_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_memory_profiler_session_timeout_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_memory_profiler_session_timeout_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_memory_profiler_session_timeout_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Memory profiler and session timeout tests enabled")

# Facade config validation tests
add_executable(network_facade_validation_test
    test_facade_validation.cpp
)

target_link_libraries(network_facade_validation_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_facade_validation_test)

# Add system integration paths
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_facade_validation_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_facade_validation_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_facade_validation_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_facade_validation_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Facade config validation tests enabled")

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
