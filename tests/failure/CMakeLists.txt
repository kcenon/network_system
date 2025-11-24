##################################################
# Failure Scenario Tests
##################################################

if(GTest_FOUND OR GTEST_FOUND)
    # Network Failure Tests
    add_executable(network_failure_test
        network_failure_test.cpp
    )

    target_link_libraries(network_failure_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_failure_test)
    setup_fmt_integration(network_failure_test)

    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_failure_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_failure_test PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_failure_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_failure_test PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_failure_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_failure_test PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_failure_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_failure_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_failure_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_failure_test
        DISCOVERY_TIMEOUT 60
    )

    # Boundary Tests
    add_executable(network_boundary_test
        boundary_test.cpp
    )

    target_link_libraries(network_boundary_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(network_boundary_test)
    setup_fmt_integration(network_boundary_test)

    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_boundary_test PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_boundary_test PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_boundary_test PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_boundary_test PRIVATE BUILD_WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_boundary_test PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_boundary_test PRIVATE BUILD_WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_boundary_test PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_boundary_test PRIVATE BUILD_WITH_COMMON_SYSTEM)
    endif()

    set_target_properties(network_boundary_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(network_boundary_test
        DISCOVERY_TIMEOUT 60
    )

    message(STATUS "Failure scenario tests enabled")
endif()
