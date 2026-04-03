cmake_minimum_required(VERSION 3.20)

##################################################
# Network System Examples CMakeLists.txt
#
# Focused, minimal examples for learning
# the network_system API.
##################################################

project(network_system_examples
    VERSION 0.1.0
    DESCRIPTION "Network System Usage Examples"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include integration module for ASIO setup
include(network_system_integration)

# Find required packages
find_package(Threads REQUIRED)

##################################################
# Example Programs
##################################################

# Core examples (always built)
set(EXAMPLE_PROGRAMS
    tcp_echo_server
    tcp_client
    udp_echo
    connection_pool
    observer_pattern
)

# WebSocket example (requires BUILD_WEBSOCKET_SUPPORT)
if(BUILD_WEBSOCKET_SUPPORT)
    list(APPEND EXAMPLE_PROGRAMS websocket_chat)
endif()

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)

##################################################
# Build Each Example
##################################################

set(EXAMPLE_TARGETS)

foreach(EXAMPLE ${EXAMPLE_PROGRAMS})
    add_executable(example_${EXAMPLE} ${EXAMPLE}.cpp)
    list(APPEND EXAMPLE_TARGETS example_${EXAMPLE})

    set_target_properties(example_${EXAMPLE} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_link_libraries(example_${EXAMPLE} PRIVATE
        network_system
        Threads::Threads
    )

    # Configure ASIO
    setup_asio_integration(example_${EXAMPLE})

    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(example_${EXAMPLE} PRIVATE
            ws2_32
            wsock32
        )
        target_compile_definitions(example_${EXAMPLE} PRIVATE
            _WIN32_WINNT=0x0601
            WIN32_LEAN_AND_MEAN
            NOMINMAX
        )
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(example_${EXAMPLE} PRIVATE
            pthread
        )
    endif()

    # Compiler warnings
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(example_${EXAMPLE} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    elseif(MSVC)
        target_compile_options(example_${EXAMPLE} PRIVATE
            /W4
            /wd4100
        )
    endif()

    message(STATUS "Network example configured: ${EXAMPLE}")
endforeach()

##################################################
# Installation
##################################################

install(TARGETS ${EXAMPLE_TARGETS}
    RUNTIME DESTINATION bin/examples
    COMPONENT examples
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION share/network_system/examples
    COMPONENT examples
)

# Install example source files
foreach(EXAMPLE ${EXAMPLE_PROGRAMS})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${EXAMPLE}.cpp
        DESTINATION share/network_system/examples
        COMPONENT examples
    )
endforeach()

##################################################
# Summary
##################################################

message(STATUS "Network System Examples configured:")
message(STATUS "  Examples: ${EXAMPLE_PROGRAMS}")
message(STATUS "  Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
