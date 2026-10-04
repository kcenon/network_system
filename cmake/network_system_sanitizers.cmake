##################################################
# network_system_sanitizers.cmake
#
# Sanitizer + coverage flag wiring for network_system.
#
# Conventions:
#   - Only one of ENABLE_ASAN/ENABLE_TSAN/ENABLE_UBSAN may
#     be enabled at a time (mutually exclusive).
#   - When any sanitizer is active, NETWORK_SYSTEM_SANITIZER
#     is added as a compile definition, and NETWORK_TEST_ENVIRONMENT
#     gets NETWORK_SYSTEM_SANITIZER=1 so tests can detect
#     the build mode at runtime.
#   - LSAN/TSAN suppression files, if present, are wired
#     into the test environment automatically.
#   - SKIP_ASIO_RECYCLING_WORKAROUND is declared here because
#     it only matters when a sanitizer is enabled — keeping
#     it next to its consumers makes the relationship obvious.
##################################################

##################################################
# Coverage settings
##################################################
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -latomic")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -latomic")
        endif()
    endif()
endif()

##################################################
# Sanitizer mutual-exclusion check
##################################################
set(_SANITIZER_COUNT 0)
if(ENABLE_ASAN)
    math(EXPR _SANITIZER_COUNT "${_SANITIZER_COUNT} + 1")
endif()
if(ENABLE_TSAN)
    math(EXPR _SANITIZER_COUNT "${_SANITIZER_COUNT} + 1")
endif()
if(ENABLE_UBSAN)
    math(EXPR _SANITIZER_COUNT "${_SANITIZER_COUNT} + 1")
endif()

if(_SANITIZER_COUNT GREATER 1)
    message(FATAL_ERROR "Only one sanitizer can be enabled at a time. "
        "ENABLE_ASAN=${ENABLE_ASAN}, ENABLE_TSAN=${ENABLE_TSAN}, ENABLE_UBSAN=${ENABLE_UBSAN}")
endif()

##################################################
# AddressSanitizer
##################################################
if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
        message(STATUS "AddressSanitizer enabled")
    elseif(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fsanitize=address")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fsanitize=address")
        message(STATUS "AddressSanitizer enabled (MSVC)")
    endif()
endif()

##################################################
# ThreadSanitizer
##################################################
if(ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
        message(STATUS "ThreadSanitizer enabled")
    else()
        message(WARNING "ThreadSanitizer is only supported with GCC and Clang")
    endif()
endif()

##################################################
# UndefinedBehaviorSanitizer
##################################################
if(ENABLE_UBSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
        message(STATUS "UndefinedBehaviorSanitizer enabled")
    else()
        message(WARNING "UndefinedBehaviorSanitizer is only supported with GCC and Clang")
    endif()
endif()

##################################################
# Sanitizer-build identification + ASIO workaround
##################################################
if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN)
    add_compile_definitions(NETWORK_SYSTEM_SANITIZER)
endif()

# Disable ASIO's small block recycling allocator when any sanitizer is enabled.
# The recycling allocator uses thread-local storage which can interact poorly
# with sanitizers, causing false positives or undefined behavior reports.
# With this disabled, ASIO uses standard aligned_new() for all allocations.
#
# NOTE: ASIO 1.31.0+ has native fixes for the recycling allocator, so this
# workaround is only needed for older ASIO versions. ASIO 1.32.0 has a bug
# where ASIO_DISABLE_SMALL_BLOCK_RECYCLING causes a compilation error.
# Use SKIP_ASIO_RECYCLING_WORKAROUND=ON when building with ASIO 1.31.0+.
option(SKIP_ASIO_RECYCLING_WORKAROUND "Skip ASIO recycling allocator workaround (for ASIO 1.31.0+)" OFF)
if((ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN) AND NOT SKIP_ASIO_RECYCLING_WORKAROUND)
    add_compile_definitions(ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
    message(STATUS "ASIO small block recycling disabled for sanitizer build")
endif()

##################################################
# Test environment for sanitizer-aware suites
##################################################
set(NETWORK_TEST_ENVIRONMENT "")

if(ENABLE_ASAN OR ENABLE_TSAN OR ENABLE_UBSAN)
    list(APPEND NETWORK_TEST_ENVIRONMENT "NETWORK_SYSTEM_SANITIZER=1")
endif()

set(NETWORK_LSAN_SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/sanitizers/lsan_suppressions.txt")
if(ENABLE_ASAN AND EXISTS "${NETWORK_LSAN_SUPPRESSIONS}")
    list(APPEND NETWORK_TEST_ENVIRONMENT "LSAN_OPTIONS=suppressions=${NETWORK_LSAN_SUPPRESSIONS}")
    message(STATUS "LSAN suppressions configured: ${NETWORK_LSAN_SUPPRESSIONS}")
endif()

set(NETWORK_TSAN_SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/tsan_suppressions.txt")
if(ENABLE_TSAN AND EXISTS "${NETWORK_TSAN_SUPPRESSIONS}")
    list(APPEND NETWORK_TEST_ENVIRONMENT
        "TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1:suppressions=${NETWORK_TSAN_SUPPRESSIONS}")
    message(STATUS "TSAN suppressions configured: ${NETWORK_TSAN_SUPPRESSIONS}")
endif()

##################################################
# Test discovery / property helpers
#
# These functions are used by tests/, integration_tests/,
# and benchmarks/ to apply the assembled test environment.
##################################################
function(network_gtest_discover_tests target)
    if(NETWORK_TEST_ENVIRONMENT)
        gtest_discover_tests(${target}
            PROPERTIES ENVIRONMENT "${NETWORK_TEST_ENVIRONMENT}"
            ${ARGN}
        )
    else()
        gtest_discover_tests(${target} ${ARGN})
    endif()
endfunction()

function(network_apply_test_environment test_name)
    if(NETWORK_TEST_ENVIRONMENT)
        set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${NETWORK_TEST_ENVIRONMENT}")
    endif()
endfunction()
