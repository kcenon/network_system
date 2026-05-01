##################################################
# Network System Tests Configuration
##################################################

# Enable testing
enable_testing()

# Find test dependencies
find_package(Threads REQUIRED)
find_package(GTest QUIET)
find_package(benchmark QUIET)

# Fetch GTest from source if not found locally
if(NOT GTest_FOUND AND NOT GTEST_FOUND)
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
    set(GTest_FOUND TRUE)
endif()

##################################################
# Hermetic Transport Fixture Support Library (Issue #1060)
##################################################

# Static helper library exposing tests/support/* — provides loopback TCP/UDP
# pair builders, in-memory TLS cert/key generation, and WebSocket frame
# builders for branch-coverage tests that need to drive private async/socket
# methods without a real network listener.
if(GTest_FOUND OR GTEST_FOUND)
    add_subdirectory(support)
endif()

##################################################
# Unit Tests
##################################################

if(GTest_FOUND OR GTEST_FOUND)
    include(GoogleTest)

    # ------------------------------------------------------------------
    # Helper to register a per-concern messaging test executable that
    # links network::test_support and inherits the optional WITH_* defines
    # the legacy network_unit_tests target carried (Issue #1088).
    # ------------------------------------------------------------------
    function(network_add_messaging_test target source)
        add_executable(${target} ${source})

        target_link_libraries(${target} PRIVATE
            network_system
            network::test_support
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
        )

        target_include_directories(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
        )

        setup_asio_integration(${target})

        if(CONTAINER_SYSTEM_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(${target} PRIVATE WITH_CONTAINER_SYSTEM)
        endif()

        if(THREAD_SYSTEM_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(${target} PRIVATE WITH_THREAD_SYSTEM)
        endif()

        if(LOGGER_SYSTEM_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(${target} PRIVATE WITH_LOGGER_SYSTEM)
        endif()

        if(COMMON_SYSTEM_INCLUDE_DIR)
            target_include_directories(${target} PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
            target_compile_definitions(${target} PRIVATE WITH_COMMON_SYSTEM)
        endif()

        set_target_properties(${target} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        # Each split file requests an OS-assigned ephemeral port via
        # messaging_loopback_fixture, so per-suite registration is parallel-safe.
        network_gtest_discover_tests(${target}
            DISCOVERY_TIMEOUT 60
        )
    endfunction()

    # Per-concern messaging test executables (Issue #1088 split of
    # tests/unit_tests.cpp). Each one owns a single test class so ctest -j N
    # never serializes the whole suite on a single binary.
    network_add_messaging_test(network_messaging_server_lifecycle_test
        unit/messaging_server_lifecycle_test.cpp)
    network_add_messaging_test(network_messaging_client_lifecycle_test
        unit/messaging_client_lifecycle_test.cpp)
    network_add_messaging_test(network_messaging_transfer_test
        unit/messaging_transfer_test.cpp)
    network_add_messaging_test(network_messaging_server_session_test
        unit/messaging_server_session_test.cpp)
    network_add_messaging_test(network_messaging_id_test
        unit/messaging_id_test.cpp)
    network_add_messaging_test(network_messaging_stress_test
        stress/messaging_stress_test.cpp)

    # Thread safety tests (Phase 1 - Task 1.6)
    add_executable(network_thread_safety_test
        thread_safety_tests.cpp
    )

    target_link_libraries(network_thread_safety_test PRIVATE
        network_system
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
        network_system
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
    )

    target_include_directories(network_tls_config_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/helpers
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
            network_system
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

        # Exclude from sanitizer builds: thread_system has a known heap-buffer-overflow
        # in thread_worker constructor (writes past allocated object boundary).
        # The upstream library must be fixed before these can run under sanitizers.
        # The dedicated sanitizers.yml already excludes thread_system entirely
        # via BUILD_WITH_THREAD_SYSTEM=OFF.
        if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN)
            message(STATUS "Thread system adapter tests excluded from sanitizer builds (upstream heap-buffer-overflow)")
        else()
            network_gtest_discover_tests(network_thread_system_adapter_test
                DISCOVERY_TIMEOUT 60
            )
            message(STATUS "Thread system adapter tests enabled")
        endif()
    endif()

    # Thread pool adapters tests (common_system integration)
    if(COMMON_SYSTEM_INCLUDE_DIR)
        add_executable(network_thread_pool_adapters_test
            unit/test_thread_pool_adapters.cpp
        )

        target_link_libraries(network_thread_pool_adapters_test PRIVATE
            network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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

    # UDP socket unit tests (raw udp_socket level)
    add_executable(network_udp_socket_test
        unit/udp_socket_test.cpp
    )

    target_link_libraries(network_udp_socket_test PRIVATE
        network_system
        network-udp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_udp_socket_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_socket_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_socket_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_socket_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_socket_test PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_udp_socket_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_udp_socket_test PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(network_udp_socket_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_udp_socket_test
        DISCOVERY_TIMEOUT 60
    )
    message(STATUS "UDP socket unit tests enabled")

    # WebSocket frame tests
    if(BUILD_WEBSOCKET_SUPPORT)
        add_executable(network_websocket_frame_test
            unit/websocket_frame_test.cpp
        )

        target_link_libraries(network_websocket_frame_test PRIVATE
            network_system
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
            network_system
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
            network_system
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
            network_system
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
            network_system
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
        network_system
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

    # HTTP/2 frame extended coverage tests (Issue #1030)
    add_executable(network_http2_frame_extended_test
        test_http2_frame_extended.cpp
    )

    target_link_libraries(network_http2_frame_extended_test PRIVATE
        network_system
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_http2_frame_extended_test)

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http2_frame_extended_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http2_frame_extended_test PRIVATE WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_http2_frame_extended_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(network_http2_frame_extended_test
        DISCOVERY_TIMEOUT 60
    )

    # HTTP/2 HPACK tests
    add_executable(network_http2_hpack_test
        test_http2_hpack.cpp
    )

    target_link_libraries(network_http2_hpack_test PRIVATE
        network_system
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

    # HTTP/2 HPACK RFC 7541 test vectors (Issue #956)
    add_executable(network_http2_hpack_rfc7541_test
        test_http2_hpack_rfc7541.cpp
    )
    target_link_libraries(network_http2_hpack_rfc7541_test PRIVATE
        network_system GTest::gtest GTest::gtest_main Threads::Threads)
    setup_asio_integration(network_http2_hpack_rfc7541_test)
    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_http2_hpack_rfc7541_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_http2_hpack_rfc7541_test PRIVATE WITH_COMMON_SYSTEM)
    endif()
    set_target_properties(network_http2_hpack_rfc7541_test PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    network_gtest_discover_tests(network_http2_hpack_rfc7541_test DISCOVERY_TIMEOUT 60)
    message(STATUS "HTTP/2 HPACK RFC 7541 tests enabled")

    # HTTP/2 client tests (requires TLS support)
    if(BUILD_TLS_SUPPORT)
        add_executable(network_http2_client_test
            test_http2_client.cpp
        )

        target_link_libraries(network_http2_client_test PRIVATE
            network_system
            network::test_support
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
            network_system
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
            network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
        network::test_support
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
            network_system
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
            network_system
            network::test_support
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
        network::test_support
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
            network_system
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
            network_system
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
        network_system
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
        network_system
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
    network_system
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
    network_system
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
    network_system
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
        network_system
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

# QUIC facade tests
add_executable(network_quic_facade_test
    test_quic_facade.cpp
)

target_link_libraries(network_quic_facade_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_quic_facade_test)

# Add system integration paths if available
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_facade_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_facade_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_facade_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_facade_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "QUIC facade tests enabled")

# Memory profiler and session timeout tests
add_executable(network_memory_profiler_session_timeout_test
    test_memory_profiler_session_timeout.cpp
    ${CMAKE_SOURCE_DIR}/src/internal/utils/memory_profiler.cpp
)

target_link_libraries(network_memory_profiler_session_timeout_test PRIVATE
    network_system
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
    network_system
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

# Network config and HTTP error utility tests
add_executable(network_config_http_error_test
    test_network_config_http_error.cpp
)

target_link_libraries(network_config_http_error_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO integration
setup_asio_integration(network_config_http_error_test)

# Add system integration paths
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_config_http_error_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_config_http_error_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_config_http_error_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_config_http_error_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network config and HTTP error utility tests enabled")

##################################################
# Socket Metrics and Data Mode Tests (Issue #725)
##################################################

add_executable(network_socket_metrics_test
    unit/socket_metrics_test.cpp
)

target_link_libraries(network_socket_metrics_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_socket_metrics_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_socket_metrics_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_socket_metrics_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_socket_metrics_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_socket_metrics_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Socket metrics and data mode tests enabled")

##################################################
# Callback Indices Tests (Issue #725)
##################################################

add_executable(network_callback_indices_test
    unit/callback_indices_test.cpp
)

target_link_libraries(network_callback_indices_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_callback_indices_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_callback_indices_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_callback_indices_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_callback_indices_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_callback_indices_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Callback indices tests enabled")

##################################################
# HTTP/2 Request Tests (Issue #725)
##################################################

add_executable(network_http2_request_test
    unit/http2_request_test.cpp
)

target_link_libraries(network_http2_request_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_http2_request_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_http2_request_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_http2_request_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_http2_request_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_http2_request_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "HTTP/2 request tests enabled")

##################################################
# QUIC Varint Unit Tests (Issue #727)
##################################################

add_executable(network_quic_varint_unit_test
    unit/quic_varint_test.cpp
)

target_link_libraries(network_quic_varint_unit_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_varint_unit_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_varint_unit_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_varint_unit_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_varint_unit_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_varint_unit_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC varint unit tests enabled")

##################################################
# QUIC Frame Types Unit Tests (Issue #727)
##################################################

add_executable(network_quic_frame_types_unit_test
    unit/quic_frame_types_test.cpp
)

target_link_libraries(network_quic_frame_types_unit_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_frame_types_unit_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_frame_types_unit_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_frame_types_unit_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_frame_types_unit_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_frame_types_unit_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC frame types unit tests enabled")

##################################################
# OpenSSL Compat Unit Tests (Issue #729)
##################################################

add_executable(network_openssl_compat_test
    unit/openssl_compat_test.cpp
)

target_link_libraries(network_openssl_compat_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_openssl_compat_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_openssl_compat_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_openssl_compat_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_openssl_compat_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_openssl_compat_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "OpenSSL compat unit tests enabled")

##################################################
# Network Context Unit Tests (Issue #729)
##################################################

add_executable(network_context_test
    unit/network_context_test.cpp
)

target_link_libraries(network_context_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_context_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_context_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_context_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_context_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_context_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network context unit tests enabled")

##################################################
# Network Metric Event Unit Tests (Issue #731)
##################################################

add_executable(network_metric_event_test
    unit/network_metric_event_test.cpp
)

target_link_libraries(network_metric_event_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_metric_event_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_metric_event_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_metric_event_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_metric_event_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_metric_event_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network metric event unit tests enabled")

##################################################
# Network Concepts Unit Tests (Issue #731)
##################################################

add_executable(network_concepts_test
    unit/network_concepts_test.cpp
)

target_link_libraries(network_concepts_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_concepts_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_concepts_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_concepts_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_concepts_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_concepts_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network concepts unit tests enabled")

add_executable(network_http_types_test
    unit/http_types_test.cpp
)

target_link_libraries(network_http_types_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_http_types_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_http_types_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_http_types_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_http_types_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_http_types_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network http_types unit tests enabled")

##################################################
# QUIC Connection ID Unit Tests (Issue #735)
##################################################

add_executable(network_quic_connection_id_test
    unit/quic_connection_id_test.cpp
)

target_link_libraries(network_quic_connection_id_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_connection_id_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_connection_id_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_connection_id_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_connection_id_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_connection_id_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC connection_id unit tests enabled")

##################################################
# QUIC Keys Unit Tests (Issue #737)
##################################################

add_executable(network_quic_keys_test
    unit/quic_keys_test.cpp
)

target_link_libraries(network_quic_keys_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_keys_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_keys_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_keys_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_keys_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_keys_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC keys unit tests enabled")

##################################################
# Startable Base Lifecycle Unit Tests (Issue #739)
##################################################

add_executable(network_startable_base_test
    unit/startable_base_test.cpp
)

target_link_libraries(network_startable_base_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_startable_base_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_startable_base_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_startable_base_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_startable_base_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_startable_base_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Startable base lifecycle unit tests enabled")

##################################################
# Session Info & Traits Unit Tests (Issue #741)
##################################################

add_executable(network_session_info_traits_test
    unit/session_info_traits_test.cpp
)

target_link_libraries(network_session_info_traits_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_session_info_traits_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_session_info_traits_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_session_info_traits_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_session_info_traits_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_session_info_traits_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Session info and traits unit tests enabled")

##################################################
# Result Types Unit Tests (Issue #741)
##################################################

add_executable(network_result_types_test
    unit/result_types_test.cpp
)

target_link_libraries(network_result_types_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_result_types_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_result_types_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_result_types_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_result_types_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_result_types_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Result types unit tests enabled")

add_executable(network_common_defs_test
    unit/common_defs_test.cpp
)

target_link_libraries(network_common_defs_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_common_defs_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_common_defs_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_common_defs_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_common_defs_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_common_defs_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Common defs unit tests enabled")

add_executable(network_quic_transport_params_test
    unit/quic_transport_params_test.cpp
)

target_link_libraries(network_quic_transport_params_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_transport_params_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_transport_params_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_transport_params_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_transport_params_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_transport_params_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC transport params unit tests enabled")

add_executable(network_quic_transport_params_coverage_test
    unit/quic_transport_params_coverage_test.cpp
)

target_link_libraries(network_quic_transport_params_coverage_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_transport_params_coverage_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_transport_params_coverage_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_transport_params_coverage_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_transport_params_coverage_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_transport_params_coverage_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC transport params coverage unit tests enabled")

##################################################
# QUIC Transport Params Branch Coverage Tests (Issue #1024)
##################################################

add_executable(network_quic_transport_params_branch_test
    unit/quic_transport_params_branch_test.cpp
)

target_link_libraries(network_quic_transport_params_branch_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_transport_params_branch_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_transport_params_branch_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_transport_params_branch_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_transport_params_branch_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_transport_params_branch_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC transport params branch coverage unit tests enabled (Issue #1024)")

##################################################
# QUIC Packet Unit Tests (Issue #954)
##################################################

add_executable(network_quic_packet_unit_test
    unit/quic_packet_test.cpp
)

target_link_libraries(network_quic_packet_unit_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_packet_unit_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_packet_unit_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_packet_unit_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_packet_unit_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_packet_unit_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC packet unit tests enabled")

##################################################
# QUIC Packet Coverage Unit Tests (Issue #1028)
##################################################

add_executable(network_quic_packet_coverage_test
    unit/quic_packet_coverage_test.cpp
)

target_link_libraries(network_quic_packet_coverage_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_packet_coverage_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_packet_coverage_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_packet_coverage_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_packet_coverage_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_packet_coverage_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC packet coverage unit tests enabled (Issue #1028)")

##################################################
# QUIC RTT Estimator Unit Tests (Issue #954)
##################################################

add_executable(network_quic_rtt_estimator_test
    unit/quic_rtt_estimator_test.cpp
)

target_link_libraries(network_quic_rtt_estimator_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_rtt_estimator_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_rtt_estimator_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_rtt_estimator_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_rtt_estimator_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_rtt_estimator_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC RTT estimator unit tests enabled")

##################################################
# QUIC Flow Control Unit Tests (Issue #954)
##################################################

add_executable(network_quic_flow_control_test
    unit/quic_flow_control_test.cpp
)

target_link_libraries(network_quic_flow_control_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_flow_control_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_flow_control_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_flow_control_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_flow_control_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_flow_control_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC flow control unit tests enabled")

##################################################
# QUIC Stream Unit Tests (Issue #954)
##################################################

add_executable(network_quic_stream_unit_test
    unit/quic_stream_test.cpp
)

target_link_libraries(network_quic_stream_unit_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_stream_unit_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_stream_unit_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_stream_unit_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_stream_unit_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_stream_unit_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC stream unit tests enabled")

##################################################
# QUIC Stream Manager Unit Tests (Issue #954)
##################################################

add_executable(network_quic_stream_manager_test
    unit/quic_stream_manager_test.cpp
)

target_link_libraries(network_quic_stream_manager_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_stream_manager_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_stream_manager_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_stream_manager_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_stream_manager_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_stream_manager_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC stream manager unit tests enabled")

##################################################
# QUIC Loss Detector Unit Tests (Issue #954)
##################################################

add_executable(network_quic_loss_detector_unit_test
    unit/quic_loss_detector_test.cpp
)

target_link_libraries(network_quic_loss_detector_unit_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_loss_detector_unit_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_loss_detector_unit_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_loss_detector_unit_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_loss_detector_unit_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_loss_detector_unit_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC loss detector unit tests enabled")

##################################################
# QUIC Congestion Controller Unit Tests (Issue #954)
##################################################

add_executable(network_quic_congestion_controller_test
    unit/quic_congestion_controller_test.cpp
)

target_link_libraries(network_quic_congestion_controller_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_congestion_controller_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_congestion_controller_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_congestion_controller_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_congestion_controller_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_congestion_controller_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC congestion controller unit tests enabled")

##################################################
# QUIC ECN Tracker Unit Tests (Issue #954)
##################################################

add_executable(network_quic_ecn_tracker_test
    unit/quic_ecn_tracker_test.cpp
)

target_link_libraries(network_quic_ecn_tracker_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_ecn_tracker_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_ecn_tracker_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_ecn_tracker_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_ecn_tracker_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_ecn_tracker_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC ECN tracker unit tests enabled")

##################################################
# QUIC Connection ID Manager Unit Tests (Issue #954)
##################################################

add_executable(network_quic_cid_manager_test
    unit/quic_connection_id_manager_test.cpp
)

target_link_libraries(network_quic_cid_manager_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_cid_manager_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_cid_manager_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_cid_manager_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_cid_manager_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_cid_manager_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "QUIC connection ID manager unit tests enabled")

##################################################
# Session Lifecycle Tests (Issue #956)
##################################################

add_executable(network_session_lifecycle_test
    unit/test_session_lifecycle.cpp
)
target_link_libraries(network_session_lifecycle_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_session_lifecycle_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_session_lifecycle_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_session_lifecycle_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_session_lifecycle_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_session_lifecycle_test DISCOVERY_TIMEOUT 60)
message(STATUS "Session lifecycle tests enabled")

##################################################
# TCP Server Adapter Tests (Issue #955)
##################################################

add_executable(network_tcp_server_adapter_test
    unit/test_tcp_server_adapter.cpp
)
target_link_libraries(network_tcp_server_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_tcp_server_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_tcp_server_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_tcp_server_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_tcp_server_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_tcp_server_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "TCP server adapter tests enabled")

##################################################
# UDP Server Adapter Tests (Issue #955)
##################################################

add_executable(network_udp_server_adapter_test
    unit/test_udp_server_adapter.cpp
)
target_link_libraries(network_udp_server_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_udp_server_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_udp_server_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_udp_server_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_udp_server_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_udp_server_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "UDP server adapter tests enabled")

##################################################
# HTTP Server Adapter Tests (Issue #955)
##################################################

add_executable(network_http_server_adapter_test
    unit/test_http_server_adapter.cpp
)
target_link_libraries(network_http_server_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_http_server_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_http_server_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_http_server_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_http_server_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_http_server_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "HTTP server adapter tests enabled")

##################################################
# QUIC Server Adapter Tests (Issue #955)
##################################################

add_executable(network_quic_server_adapter_test
    unit/test_quic_server_adapter.cpp
)
target_link_libraries(network_quic_server_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_quic_server_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_server_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_server_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_quic_server_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_quic_server_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "QUIC server adapter tests enabled")

##################################################
# WebSocket Server Adapter Tests (Issue #955)
##################################################

add_executable(network_ws_server_adapter_test
    unit/test_ws_server_adapter.cpp
)
target_link_libraries(network_ws_server_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_ws_server_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_ws_server_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_ws_server_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_ws_server_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_ws_server_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "WebSocket server adapter tests enabled")

##################################################
# UDP Client Adapter Tests (Issue #955)
##################################################

add_executable(network_udp_client_adapter_test
    unit/test_udp_client_adapter.cpp
)
target_link_libraries(network_udp_client_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_udp_client_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_udp_client_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_udp_client_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_udp_client_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_udp_client_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "UDP client adapter tests enabled")

##################################################
# HTTP Client Adapter Tests (Issue #955)
##################################################

add_executable(network_http_client_adapter_test
    unit/test_http_client_adapter.cpp
)
target_link_libraries(network_http_client_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_http_client_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_http_client_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_http_client_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_http_client_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_http_client_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "HTTP client adapter tests enabled")

##################################################
# QUIC Client Adapter Tests (Issue #955)
##################################################

add_executable(network_quic_client_adapter_test
    unit/test_quic_client_adapter.cpp
)
target_link_libraries(network_quic_client_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_quic_client_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_client_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_client_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_quic_client_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_quic_client_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "QUIC client adapter tests enabled")

##################################################
# WebSocket Client Adapter Tests (Issue #955)
##################################################

add_executable(network_ws_client_adapter_test
    unit/test_ws_client_adapter.cpp
)
target_link_libraries(network_ws_client_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_ws_client_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_ws_client_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_ws_client_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_ws_client_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_ws_client_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "WebSocket client adapter tests enabled")

##################################################
# TCP Connection Adapter Tests (Issue #955)
##################################################

add_executable(network_tcp_connection_adapter_test
    unit/test_tcp_connection_adapter.cpp
)
target_link_libraries(network_tcp_connection_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_tcp_connection_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_tcp_connection_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_tcp_connection_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_tcp_connection_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_tcp_connection_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "TCP connection adapter tests enabled")

##################################################
# TCP Listener Adapter Tests (Issue #955)
##################################################

add_executable(network_tcp_listener_adapter_test
    unit/test_tcp_listener_adapter.cpp
)
target_link_libraries(network_tcp_listener_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_tcp_listener_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_tcp_listener_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_tcp_listener_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_tcp_listener_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_tcp_listener_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "TCP listener adapter tests enabled")

##################################################
# UDP Connection Adapter Tests (Issue #955)
##################################################

add_executable(network_udp_connection_adapter_test
    unit/test_udp_connection_adapter.cpp
)
target_link_libraries(network_udp_connection_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_udp_connection_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_udp_connection_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_udp_connection_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_udp_connection_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_udp_connection_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "UDP connection adapter tests enabled")

##################################################
# UDP Listener Adapter Tests (Issue #955)
##################################################

add_executable(network_udp_listener_adapter_test
    unit/test_udp_listener_adapter.cpp
)
target_link_libraries(network_udp_listener_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_udp_listener_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_udp_listener_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_udp_listener_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_udp_listener_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_udp_listener_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "UDP listener adapter tests enabled")

##################################################
# WebSocket Connection Adapter Tests (Issue #967)
##################################################

add_executable(network_ws_connection_adapter_test
    unit/test_ws_connection_adapter.cpp
)
target_link_libraries(network_ws_connection_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_ws_connection_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_ws_connection_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_ws_connection_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_ws_connection_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_ws_connection_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "WebSocket connection adapter tests enabled")

##################################################
# WebSocket Listener Adapter Tests (Issue #967)
##################################################

add_executable(network_ws_listener_adapter_test
    unit/test_ws_listener_adapter.cpp
)
target_link_libraries(network_ws_listener_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_ws_listener_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_ws_listener_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_ws_listener_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_ws_listener_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_ws_listener_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "WebSocket listener adapter tests enabled")

##################################################
# QUIC Connection Adapter Tests (Issue #967)
##################################################

add_executable(network_quic_connection_adapter_test
    unit/test_quic_connection_adapter.cpp
)
target_link_libraries(network_quic_connection_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_quic_connection_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_connection_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_connection_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_quic_connection_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_quic_connection_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "QUIC connection adapter tests enabled")

##################################################
# QUIC Listener Adapter Tests (Issue #967)
##################################################

add_executable(network_quic_listener_adapter_test
    unit/test_quic_listener_adapter.cpp
)
target_link_libraries(network_quic_listener_adapter_test PRIVATE
    network_system GTest::gtest GTest::gtest_main Threads::Threads)
setup_asio_integration(network_quic_listener_adapter_test)
if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_listener_adapter_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_listener_adapter_test PRIVATE WITH_COMMON_SYSTEM)
endif()
set_target_properties(network_quic_listener_adapter_test PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
network_gtest_discover_tests(network_quic_listener_adapter_test DISCOVERY_TIMEOUT 60)
message(STATUS "QUIC listener adapter tests enabled")

##################################################
# Sliding Histogram Tests (Issue #873)
##################################################

add_executable(network_sliding_histogram_test
    unit/sliding_histogram_test.cpp
)

target_link_libraries(network_sliding_histogram_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_sliding_histogram_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_sliding_histogram_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_sliding_histogram_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_sliding_histogram_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_sliding_histogram_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Sliding histogram unit tests enabled")

##################################################
# Network System Lifecycle Tests (Issue #873)
##################################################

add_executable(network_system_lifecycle_test
    unit/network_system_test.cpp
)

target_link_libraries(network_system_lifecycle_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_system_lifecycle_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_system_lifecycle_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_system_lifecycle_test PRIVATE WITH_COMMON_SYSTEM)
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_system_lifecycle_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_system_lifecycle_test PRIVATE WITH_THREAD_SYSTEM)
endif()

if(LOGGER_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_system_lifecycle_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_system_lifecycle_test PRIVATE WITH_LOGGER_SYSTEM)
endif()

if(CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_system_lifecycle_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_system_lifecycle_test PRIVATE WITH_CONTAINER_SYSTEM)
endif()

set_target_properties(network_system_lifecycle_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_system_lifecycle_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network system lifecycle tests enabled")

##################################################
# Message Validator Extended Tests (Issue #873)
##################################################

add_executable(network_message_validator_extended_test
    unit/message_validator_extended_test.cpp
)

target_link_libraries(network_message_validator_extended_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_message_validator_extended_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_message_validator_extended_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_message_validator_extended_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_message_validator_extended_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_message_validator_extended_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Message validator extended tests enabled")

# Network metrics histogram extended tests (Phase 2)
add_executable(network_metrics_histogram_test
    unit/network_metrics_histogram_test.cpp
)

target_link_libraries(network_metrics_histogram_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_metrics_histogram_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_metrics_histogram_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_metrics_histogram_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_metrics_histogram_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_metrics_histogram_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Network metrics histogram tests enabled")

# Facade ID generation and extended validation tests (Phase 2)
add_executable(network_facade_extended_test
    unit/facade_id_generation_test.cpp
)

target_link_libraries(network_facade_extended_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_facade_extended_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_facade_extended_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_facade_extended_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_facade_extended_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_facade_extended_test
    DISCOVERY_TIMEOUT 60
)
message(STATUS "Facade extended tests enabled")

##################################################
# Tracing Unit Tests (Issue #965)
##################################################

add_executable(network_trace_context_test
    unit/trace_context_test.cpp
)

target_link_libraries(network_trace_context_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_trace_context_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_trace_context_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_trace_context_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_trace_context_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_trace_context_test
    DISCOVERY_TIMEOUT 60
)

add_executable(network_span_test
    unit/span_test.cpp
)

target_link_libraries(network_span_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_span_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_span_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_span_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_span_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_span_test
    DISCOVERY_TIMEOUT 60
)

add_executable(network_tracing_exporters_test
    unit/tracing_exporters_test.cpp
)

target_link_libraries(network_tracing_exporters_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_tracing_exporters_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_tracing_exporters_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_tracing_exporters_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_tracing_exporters_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_tracing_exporters_test
    DISCOVERY_TIMEOUT 60
)

add_executable(network_tracing_exporters_branch_test
    unit/tracing_exporters_branch_test.cpp
)

target_link_libraries(network_tracing_exporters_branch_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_tracing_exporters_branch_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_tracing_exporters_branch_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_tracing_exporters_branch_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_tracing_exporters_branch_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_tracing_exporters_branch_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "Tracing unit tests enabled (trace_context, span, exporters)")

##################################################
# Session Unit Tests (Issue #965)
##################################################

add_executable(network_messaging_session_test
    unit/messaging_session_test.cpp
)

target_link_libraries(network_messaging_session_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_messaging_session_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_messaging_session_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_messaging_session_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_messaging_session_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_messaging_session_test
    DISCOVERY_TIMEOUT 60
)

add_executable(network_secure_session_test
    unit/secure_session_test.cpp
)

target_link_libraries(network_secure_session_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

setup_asio_integration(network_secure_session_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_secure_session_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_secure_session_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_secure_session_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_secure_session_test
    DISCOVERY_TIMEOUT 60
)

add_executable(network_quic_session_test
    unit/quic_session_test.cpp
)

target_link_libraries(network_quic_session_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_quic_session_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_quic_session_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_quic_session_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_quic_session_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_quic_session_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "Session unit tests enabled (messaging, secure, quic)")

##################################################
# Experimental Unit Tests (Issue #965)
##################################################

# reliable_udp_client_test excluded: source moved out of main library (Issue #561)

add_executable(network_experimental_quic_client_test
    unit/experimental_quic_client_test.cpp
)

target_link_libraries(network_experimental_quic_client_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

target_compile_definitions(network_experimental_quic_client_test PRIVATE NETWORK_USE_EXPERIMENTAL)

setup_asio_integration(network_experimental_quic_client_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_experimental_quic_client_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_experimental_quic_client_test PRIVATE WITH_COMMON_SYSTEM)
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_experimental_quic_client_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_experimental_quic_client_test PRIVATE WITH_THREAD_SYSTEM)
endif()

set_target_properties(network_experimental_quic_client_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_experimental_quic_client_test
    DISCOVERY_TIMEOUT 60
)

add_executable(network_experimental_quic_server_test
    unit/experimental_quic_server_test.cpp
)

target_link_libraries(network_experimental_quic_server_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

target_compile_definitions(network_experimental_quic_server_test PRIVATE NETWORK_USE_EXPERIMENTAL)

setup_asio_integration(network_experimental_quic_server_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_experimental_quic_server_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_experimental_quic_server_test PRIVATE WITH_COMMON_SYSTEM)
endif()

if(THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_experimental_quic_server_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_experimental_quic_server_test PRIVATE WITH_THREAD_SYSTEM)
endif()

set_target_properties(network_experimental_quic_server_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_experimental_quic_server_test
    DISCOVERY_TIMEOUT 60
)

message(STATUS "Experimental unit tests enabled (quic_client, quic_server)")

##################################################
# Core Messaging Unit Tests (Issue #966)
##################################################

# Helper macro to create a standard test target
macro(add_network_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})

    target_link_libraries(${TEST_NAME} PRIVATE
        network_system
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(${TEST_NAME})

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(${TEST_NAME} PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(${TEST_NAME} PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(${TEST_NAME} PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(${TEST_NAME} PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(${TEST_NAME} PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(${TEST_NAME} PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    network_gtest_discover_tests(${TEST_NAME}
        DISCOVERY_TIMEOUT 60
    )
endmacro()

add_network_test(network_messaging_client_test unit/messaging_client_test.cpp)
add_network_test(network_messaging_server_test unit/messaging_server_test.cpp)
add_network_test(network_messaging_udp_client_test unit/messaging_udp_client_test.cpp)
add_network_test(network_messaging_udp_server_test unit/messaging_udp_server_test.cpp)
# secure_messaging_udp tests excluded: dtls_socket symbols in libs/network-udp, not main library

message(STATUS "Core messaging unit tests enabled (tcp, udp)")

# WebSocket tests already exist: test_messaging_ws_client.cpp, test_messaging_ws_server.cpp

##################################################
# Integration Module Unit Tests (Issue #968)
##################################################

add_network_test(network_thread_integration_test unit/thread_integration_test.cpp)
add_network_test(network_logger_integration_test unit/logger_integration_test.cpp)
add_network_test(network_monitoring_integration_test unit/monitoring_integration_test.cpp)
add_network_test(network_container_integration_test unit/container_integration_test.cpp)
add_network_test(network_io_context_thread_manager_test unit/io_context_thread_manager_test.cpp)
message(STATUS "Integration module unit tests enabled (Issue #968)")

##################################################
# Protocol Factory Unit Tests (Issue #968)
##################################################

add_network_test(network_protocol_tcp_factory_test unit/protocol_tcp_factory_test.cpp)
add_network_test(network_protocol_udp_factory_test unit/protocol_udp_factory_test.cpp)
add_network_test(network_protocol_websocket_factory_test unit/protocol_websocket_factory_test.cpp)
add_network_test(network_protocol_quic_factory_test unit/protocol_quic_factory_test.cpp)
message(STATUS "Protocol factory unit tests enabled (Issue #968)")

##################################################
# HTTP/2 Server Stream Unit Tests (Issue #968)
##################################################

add_network_test(network_http2_server_stream_test unit/http2_server_stream_test.cpp)
message(STATUS "HTTP/2 server stream unit tests enabled (Issue #968)")

##################################################
# WebSocket Socket Unit Tests (Issue #968)
##################################################

add_network_test(network_websocket_socket_test unit/websocket_socket_test.cpp)
message(STATUS "WebSocket socket unit tests enabled (Issue #968)")

##################################################
# QUIC Protocol Module Unit Tests (Issue #974)
##################################################

add_network_test(network_session_ticket_store_test unit/session_ticket_store_test.cpp)
add_network_test(network_quic_pmtud_controller_test unit/quic_pmtud_controller_test.cpp)
add_network_test(network_quic_crypto_module_test unit/quic_crypto_test.cpp)
add_network_test(network_quic_crypto_extended_test unit/quic_crypto_extended_test.cpp)
add_network_test(network_quic_connection_module_test unit/quic_connection_test.cpp)
add_network_test(network_quic_connection_extended_test unit/quic_connection_extended_test.cpp)
add_network_test(network_quic_socket_construction_test unit/quic_socket_construction_test.cpp)

# QUIC frame extended coverage: parser error paths (truncated inputs at every
# varint boundary), unknown frame-type dispatch, varint edge values (0, 63, 64,
# 16383, 2^62-1), stream flag matrix (FIN/LEN/OFF 8 combinations), new_connection_id
# CID length boundaries, connection_close transport vs application variants,
# round-trip coverage for frame_builder::build(variant) dispatch (Issue #1011)
add_network_test(network_quic_frame_coverage_test unit/quic_frame_coverage_test.cpp)

message(STATUS "QUIC protocol module unit tests enabled (Issue #974)")

##################################################
# Facade Layer Module Unit Tests (Issue #975)
##################################################

add_network_test(network_http_facade_module_test unit/http_facade_test.cpp)
add_network_test(network_tcp_facade_module_test unit/tcp_facade_test.cpp)
add_network_test(network_udp_facade_module_test unit/udp_facade_test.cpp)
add_network_test(network_websocket_facade_module_test unit/websocket_facade_test.cpp)
add_network_test(network_quic_facade_module_test unit/quic_facade_test.cpp)
message(STATUS "Facade layer module unit tests enabled (Issue #975)")

##################################################
# HTTP/2, Secure Transport, WebSocket Unit Tests (Issue #976)
##################################################

# HTTP/2 module tests
add_network_test(network_hpack_module_test unit/hpack_test.cpp)

# HPACK extended coverage: encoder static-table representation, dynamic-table
# eviction under tight max_size, decoder error paths (codes 100-107),
# integer-length boundary round-trips (Issue #1009)
add_network_test(network_hpack_coverage_test unit/hpack_coverage_test.cpp)

# HPACK branch coverage: literal-without-indexing indexed-name paths,
# encoder same-call dynamic-table reuse, dynamic_table find/get edge cases
# (Issue #1031)
add_network_test(network_hpack_branch_test unit/hpack_branch_test.cpp)

# HPACK additional coverage: static-table boundary entries, decoder happy paths
# for literal-without-indexing / never-indexed prefixes, multi-byte integer
# non-overflow path, huffman stub contract, mixed static/dynamic round-trips
# (Issue #1031)
add_network_test(network_hpack_extra_coverage_test unit/hpack_extra_coverage_test.cpp)

# tcp_socket extra coverage: try_send rejection, async_send-on-closed, start_read
# idempotence, reset_metrics, backpressure activation/release, multi-observer
# delivery, default-state invariants (Issue #1032)
add_network_test(network_tcp_socket_extra_coverage_test unit/tcp_socket_extra_coverage_test.cpp)

# unified_session_manager extra coverage: default config, backpressure short-circuit,
# zero max_sessions guard, pre-wrapped session_handle add path, with_id rejection /
# explicit / collision, set_max_sessions narrowing, generate_id concurrency,
# cleanup accumulation, const get_session, full stats population (Issue #1034)
add_network_test(network_unified_session_manager_extra_coverage_test
                 unit/unified_session_manager_extra_coverage_test.cpp)

# quic frame extra coverage: longer varint type prefixes, heterogeneous parse_all,
# every concrete STREAM type byte (0x08-0x0f), ACK_ECN dispatch via parse() with
# truncation matrix, crypto offset+large-data round-trips, new_connection_id
# minimal/large sequence variants, connection_close transport-with-frame-type and
# multi-byte error_code, build_padding(0) edge, frame_type_to_string for branches
# not previously asserted, stream-helper per-bit consistency (Issue #1033)
add_network_test(network_quic_frame_extra_coverage_test
                 unit/quic_frame_extra_coverage_test.cpp)

add_network_test(network_http2_client_module_test unit/http2_client_test.cpp)
add_network_test(network_http2_server_module_test unit/http2_server_test.cpp)

# HTTP/2 server extended coverage: lifecycle, start_tls error paths,
# connection preface handling via raw TCP clients (Issue #992)
add_network_test(network_http2_server_coverage_test unit/http2_server_coverage_test.cpp)

# HTTP/2 client extended coverage: connect guard clauses, network error paths,
# settings/timeout edge cases, disconnected request/stream operations (Issue #991)
add_network_test(network_http2_client_coverage_test unit/http2_client_coverage_test.cpp)

# HTTP/2 client additional coverage: TLS handshake failure branches, ALPN
# mismatch, concurrent disconnect, settings round-trip, stream callback move
# semantics (Issue #991 follow-up)
add_network_test(network_http2_client_extended_coverage_test
                 unit/http2_client_extended_coverage_test.cpp)

# HTTP/2 client branch coverage: response header lookup case-insensitivity,
# settings round-trip boundaries, http2_stream move semantics with populated
# state, connect() error paths, disconnected helper early-return paths,
# concurrent state queries (Issue #1048)
add_network_test(network_http2_client_branch_test
                 unit/http2_client_branch_test.cpp)
target_link_libraries(network_http2_client_branch_test PRIVATE network::test_support)

# gRPC client branch coverage: grpc_channel_config full-field round-trip with
# boundary values, call_options::set_timeout duration variants, metadata
# growth, varied target construction, wait_for_connected timeout budget,
# concurrent disconnect / is_connected, move semantics, malformed-target
# connect coverage, call_raw / streaming guard-clause variants, async
# callback stress, grpc_metadata copy/move/clear semantics (Issue #1049)
add_network_test(network_grpc_client_branch_test
                 unit/grpc_client_branch_test.cpp)
target_link_libraries(network_grpc_client_branch_test PRIVATE network::test_support)

# HTTP/2 server branch coverage: tls_config field round-trips with boundary
# values, http2_server construction with empty/long/binary server IDs,
# http2_settings round-trip with zero/UINT32_MAX boundaries plus repeated
# header_table_size updates, pre-listen state queries (is_running,
# active_connections, active_streams, server_id), repeated start/stop
# cycles, already-running rejection for both clear and TLS starts,
# additional start_tls rejection paths (empty cert/key, missing files,
# verify_client without ca_file), handler registration with empty / shared-
# state-capture / replaced functions, concurrent state polling under
# shared_ptr lifetime, many concurrent server instances (Issue #1050)
add_network_test(network_http2_server_branch_test
                 unit/http2_server_branch_test.cpp)
target_link_libraries(network_http2_server_branch_test PRIVATE network::test_support)

# QUIC socket branch coverage: many short-lived client / server instances,
# default invariants for state / role / is_connected / is_handshake_complete /
# remote_endpoint / remote_connection_id, local_connection_id RFC 9000 length
# bounds and pairwise uniqueness, callback registration with default-
# constructed std::function / replaced lambdas / shared_ptr-captured state for
# all four callback types, mutable + const socket() accessor reachability,
# role-guard rejection branches in connect()/accept() (server-cannot-connect
# with and without SNI, client-cannot-accept with empty + missing paths,
# repeated server-connect rejection idempotency), close() idempotency on
# idle/draining/closed including triple-close, application error code branch
# (is_application_error=true), max-uint64 error code, long reason string,
# empty reason, send_stream_data() not-connected guard for empty / populated
# / large payloads with and without FIN, create_stream() not-connected guard
# for client-bidi / client-uni / server-bidi / server-uni, close_stream()
# not_found early-return for unknown / max-uint64 / nine separate ids,
# stop_receive() before start, repeated stop_receive() idempotency,
# start_receive()/stop_receive() flag transition, move construction with
# populated callbacks preserving role + connection ID + idle state, server-
# role move construction, move assignment overwriting client target with
# server source preserving connection ID, three-step move-assignment chain,
# self-move-assignment via reference (this!=&other guard), concurrent state
# query polling (state / is_connected / is_handshake_complete / role /
# local_connection_id / remote_connection_id) under shared_ptr lifetime,
# concurrent callback replacement with reader thread, multi-instance
# independent state across 4+4 client/server pairs, destructor cleanup after
# close / without close / after start_receive (Issue #1051)
add_network_test(network_quic_socket_branch_test
                 unit/quic_socket_branch_test.cpp)
target_link_libraries(network_quic_socket_branch_test PRIVATE network::test_support)

# QUIC server branch coverage: quic_server_config full-field round-trip
# (cert/key/ca paths, ALPN vector, all numeric settings at zero/max
# boundaries, retry_key vector, full-config copy round-trip), construction
# with empty / 512-char / binary-byte server IDs, default-state queries
# (is_running / session_count / connection_count / sessions / get_session)
# on never-started server, stop_server() rejection branch when never
# started, disconnect_session() not_found branch for empty / long /
# binary-byte ids, disconnect_all() empty-map no-op, broadcast() and
# multicast() empty-map ok-return for empty / small / 64 KiB payloads
# and many unknown ids, legacy callback registration with empty
# std::function / replaced lambdas / shared_ptr-captured state for all
# five callback types, interface (i_quic_server) callback adapters for
# connection / disconnection / receive / stream / error, concurrent
# polling of is_running / session_count / server_id / sessions list and
# concurrent callback replacement, multi-instance independent state,
# destructor on never-started / after failed-stop / with callbacks
# registered (Issue #1052)
add_network_test(network_quic_server_branch_test
                 unit/quic_server_branch_test.cpp)
target_link_libraries(network_quic_server_branch_test PRIVATE network::test_support)

# WebSocket server branch coverage: ws_server_config full-field round-trip
# (port / max_connections / ping_interval / max_message_size at zero / 1 /
# max boundaries, path with empty / 1024-char / binary-byte values,
# auto_pong toggle), messaging_ws_server construction with empty / 512-char
# / binary-byte server IDs, server_id() reference stability, default-state
# queries (is_running / connection_count / get_connection / get_all_connections)
# on never-started server, stop_server() / i_websocket_server::stop()
# returning ok() on prepare_stop() early-return when never started,
# get_connection() not_found branch for unknown / empty / 2048-char /
# binary-byte ids, broadcast_text() / broadcast_binary() session_mgr_ ==
# nullptr early-return for empty / small / 64 KiB payloads, interface
# (i_websocket_server) callback adapters for connection / disconnection /
# text / binary / error with null callbacks (empty-function branch),
# populated lambdas (wrap-and-store branch), triple replacement,
# null-then-populated-then-null toggle, shared_ptr-captured state,
# concurrent polling of is_running / connection_count / server_id /
# get_all_connections under shared_ptr lifetime, concurrent callback
# replacement under reader thread, multi-instance independent state,
# type alias coverage (ws_server / secure_ws_server / interop),
# destructor on never-started / after stop / with callbacks /
# after broadcast (Issue #1053)
add_network_test(network_websocket_server_branch_test
                 unit/websocket_server_branch_test.cpp)
target_link_libraries(network_websocket_server_branch_test PRIVATE network::test_support)

# WebSocket server in-process loopback coverage: drives messaging_ws_server
# end-to-end against a real RFC 6455 client (built from internal::websocket_socket)
# to reach branches that are unreachable without a live handshake — namely
# do_start_impl / do_stop_impl success paths, do_accept loop, handle_new_connection
# success / max_connections-zero limit / handshake-failure branches, on_message
# text+binary dispatch, on_close, broadcast_text / broadcast_binary populated-
# session paths, get_connection / get_all_connections success, the auto_pong
# true and false branches, ~messaging_ws_server while running, bind_failed
# catch arm of do_start_impl, and ws_connection / ws_connection_impl methods
# reachable only after a real connection (id / is_connected / path /
# remote_endpoint / send / send_text / send_binary / close / close(code,reason))
# (Issue #1067)
add_network_test(network_websocket_server_loopback_test
                 unit/websocket_server_loopback_test.cpp)

# HTTP parser extended coverage: url_encode/decode edge cases (percent
# truncation, high-bit bytes, hex case), query string delimiter handling
# (leading/trailing '&', duplicate keys, URL-encoded delimiters), cookie
# malformed-pair handling, parse_request_line / parse_status_line error
# branches, multipart form-data edge cases, chunked encoding boundary
# conditions (Issue #1013)
add_network_test(network_http_parser_coverage_test unit/http_parser_coverage_test.cpp)

# HTTP parser branch-coverage extension targeting parse_multipart_form_data
# error paths (missing Content-Type / boundary, headers without CRLF
# terminator, name/filename without closing quote, Content-Type whitespace
# trim fall-through), parse_request/parse_response edge branches, and
# additional serialize_chunked_response / trim cases (Issue #1025)
add_network_test(network_http_parser_branch_test unit/http_parser_branch_test.cpp)

# HTTP error extended coverage: get_error_status_text default branch,
# escape_json_string switch branches (all special chars, low control bytes,
# plain ASCII, high-bit bytes), escape_html_string switch branches, and the
# detail / request_id / message empty-vs-non-empty branches of
# http_error_response::build_json_error and build_html_error (Issue #1023)
add_network_test(network_http_error_coverage_test unit/http_error_coverage_test.cpp)

# Secure transport module tests
add_network_test(network_secure_tcp_socket_module_test unit/secure_tcp_socket_test.cpp)
# secure_messaging_udp_client/server excluded: depend on dtls_socket from
# libs/network-udp, require special link config beyond add_network_test macro

# WebSocket module tests
add_network_test(network_websocket_client_module_test unit/websocket_client_test.cpp)
add_network_test(network_websocket_server_module_test unit/websocket_server_test.cpp)

message(STATUS "HTTP/2, secure transport, and WebSocket module unit tests enabled (Issue #976)")

##################################################
# Infrastructure & Service Module Unit Tests (Issue #977)
##################################################

# gRPC client config, call_options, construction
add_network_test(network_grpc_client_unit_test unit/core_client_test.cpp)

# gRPC client extended coverage: lifecycle, guard clauses, async callback
# delivery, and streaming error paths (Issue #994)
add_network_test(network_grpc_client_coverage_test unit/grpc_client_test.cpp)

# gRPC server config, status, construction
add_network_test(network_grpc_server_unit_test unit/core_server_test.cpp)

# health_monitor construction, state, callbacks
add_network_test(network_health_monitor_unit_test unit/health_monitor_test.cpp)

# service_registry, generic_service, health_service, utility functions
add_network_test(network_service_registry_unit_test unit/service_registry_test.cpp)

# memory_profiler: NOT in main library, needs explicit source inclusion
add_executable(network_memory_profiler_unit_test
    unit/memory_profiler_test.cpp
    ${CMAKE_SOURCE_DIR}/src/internal/utils/memory_profiler.cpp
)

target_link_libraries(network_memory_profiler_unit_test PRIVATE
    network_system
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(network_memory_profiler_unit_test)

if(COMMON_SYSTEM_INCLUDE_DIR)
    target_include_directories(network_memory_profiler_unit_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
    target_compile_definitions(network_memory_profiler_unit_test PRIVATE WITH_COMMON_SYSTEM)
endif()

set_target_properties(network_memory_profiler_unit_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

network_gtest_discover_tests(network_memory_profiler_unit_test
    DISCOVERY_TIMEOUT 60
)

# reliable_udp_client header-only types (enums, stats structs)
# Implementation is in libs/network-udp (Issue #561), so only header-only
# types can be tested via add_network_test without linker errors.
add_network_test(network_reliable_udp_client_unit_test unit/reliable_udp_client_test.cpp)

message(STATUS "Infrastructure and service module unit tests enabled (Issue #977)")

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
