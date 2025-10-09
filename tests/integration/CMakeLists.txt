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

    set_target_properties(benchmark PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    message(STATUS "Integration test configured: benchmark")
endif()

##################################################
# Test Data Setup
##################################################

# Copy test data if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_data)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data
         DESTINATION ${CMAKE_BINARY_DIR}/bin)
endif()
