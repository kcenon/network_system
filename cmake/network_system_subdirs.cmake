##################################################
# network_system_subdirs.cmake
#
# add_subdirectory() orchestration for the auxiliary
# trees: tests, examples, benchmarks, fuzz.
#
# The protocol libraries themselves (libs/network-tcp etc.)
# are added in network_system_protocol_libs.cmake — they
# must come before network_system_targets.cmake so that
# network_system can link against them, which is too early
# to also pull in the test/example trees. Splitting these
# concerns mirrors that ordering requirement.
#
# Preconditions:
#   - network_system_targets.cmake has already been
#     included (so the network_system / verify_build
#     targets exist and BUILD_VERIFY_BUILD is defined).
##################################################

##################################################
# Tests
##################################################

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "Building network_system tests")
    if(BUILD_VERIFY_BUILD AND TARGET verify_build)
        add_test(NAME verify_build_test COMMAND verify_build)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
        add_subdirectory(tests)
        message(STATUS "Network system unit tests and thread safety tests enabled")
    endif()

    if(NETWORK_BUILD_INTEGRATION_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests/CMakeLists.txt)
        add_subdirectory(integration_tests)
        message(STATUS "Network system integration tests enabled")
    endif()
endif()

##################################################
# Examples
##################################################

if(BUILD_EXAMPLES)
    message(STATUS "Building network_system examples")

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt)
        add_subdirectory(examples)
        message(STATUS "Network system examples enabled")
    endif()
endif()

##################################################
# Benchmarks
##################################################

if(NETWORK_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
    add_subdirectory(benchmarks)
    message(STATUS "Network benchmarks will be built")
endif()

##################################################
# Fuzzing
##################################################

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/CMakeLists.txt)
    add_subdirectory(fuzz)
endif()
