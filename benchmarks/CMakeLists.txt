# Network System Benchmarks
# Phase 0, Task 0.2: Baseline Performance Benchmarking

cmake_minimum_required(VERSION 3.20)

find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(BENCHMARK benchmark)
    endif()
endif()

if(benchmark_FOUND OR BENCHMARK_FOUND)
    message(STATUS "Google Benchmark found. Building network_system benchmarks...")

    add_executable(network_benchmarks
        message_throughput_bench.cpp
        connection_bench.cpp
        session_bench.cpp
        http_bench.cpp
        tcp_receive_bench.cpp
        main_bench.cpp
    )

    target_link_libraries(network_benchmarks
        PRIVATE
            NetworkSystem
            benchmark::benchmark
            benchmark::benchmark_main
    )

    # Include directories for network_system headers
    target_include_directories(network_benchmarks
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    # Setup integrations for benchmarks (same as NetworkSystem target)
    setup_asio_integration(network_benchmarks)

    # Add system integration paths and definitions
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmarks PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmarks PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmarks PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmarks PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmarks PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmarks PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(network_benchmarks PRIVATE ${COMMON_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(network_benchmarks PRIVATE WITH_COMMON_SYSTEM)
    endif()

    if(BUILD_MESSAGING_BRIDGE)
        target_compile_definitions(network_benchmarks PRIVATE BUILD_MESSAGING_BRIDGE)
    endif()

    target_compile_features(network_benchmarks PRIVATE cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(network_benchmarks PRIVATE
            -Wall -Wextra -Wpedantic -O3 -DNDEBUG)
    elseif(MSVC)
        target_compile_options(network_benchmarks PRIVATE /W4 /O2 /DNDEBUG)
    endif()

    add_custom_target(run_network_benchmarks
        COMMAND network_benchmarks --benchmark_format=json --benchmark_out=results.json
        COMMAND network_benchmarks
        DEPENDS network_benchmarks
        COMMENT "Running network_system benchmarks"
    )

    install(TARGETS network_benchmarks RUNTIME DESTINATION bin/benchmarks)
else()
    message(WARNING "Google Benchmark not found. Skipping benchmarks.")
endif()
