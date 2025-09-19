cmake_minimum_required(VERSION 3.16)

##################################################
# Network System CMakeLists.txt
#
# Independent high-performance network system
# Separated from messaging_system for modularity
##################################################

# Project definition
project(NetworkSystem
    VERSION 2.0.0
    DESCRIPTION "Independent High-Performance Network System"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration" ON)
option(BUILD_MESSAGING_BRIDGE "Build messaging_system bridge" ON)

# Global compile features
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find packages
find_path(ASIO_INCLUDE_DIR
    NAMES asio.hpp
    PATHS /opt/homebrew/include /usr/local/include
    NO_DEFAULT_PATH
)

find_path(FMT_INCLUDE_DIR
    NAMES fmt/format.h
    PATHS /opt/homebrew/include /usr/local/include
    NO_DEFAULT_PATH
)

if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "ASIO not found. Please install ASIO library.")
endif()

if(NOT FMT_INCLUDE_DIR)
    message(STATUS "FMT not found, will use header-only fmt")
endif()

# Conditional dependencies
if(BUILD_WITH_CONTAINER_SYSTEM)
    find_path(CONTAINER_SYSTEM_INCLUDE_DIR
        NAMES container_system/container.h
        PATHS ../container_system/include
        NO_DEFAULT_PATH
    )
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found container_system at: ${CONTAINER_SYSTEM_INCLUDE_DIR}")
        add_compile_definitions(BUILD_WITH_CONTAINER_SYSTEM)
    else()
        message(WARNING "container_system not found, integration disabled")
        set(BUILD_WITH_CONTAINER_SYSTEM OFF)
    endif()
endif()

if(BUILD_WITH_THREAD_SYSTEM)
    find_path(THREAD_SYSTEM_INCLUDE_DIR
        NAMES thread_system/thread_pool.h
        PATHS ../thread_system/include
        NO_DEFAULT_PATH
    )
    if(THREAD_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found thread_system at: ${THREAD_SYSTEM_INCLUDE_DIR}")
        add_compile_definitions(BUILD_WITH_THREAD_SYSTEM)
    else()
        message(WARNING "thread_system not found, integration disabled")
        set(BUILD_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

# Create main library
add_library(NetworkSystem
    # Core implementation
    src/core/messaging_client.cpp
    src/core/messaging_server.cpp

    # Session management
    src/session/messaging_session.cpp

    # Internal implementation
    src/internal/tcp_socket.cpp
    src/internal/send_coroutine.cpp
    src/internal/pipeline.cpp
)

# Set target properties
set_target_properties(NetworkSystem PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Include directories
target_include_directories(NetworkSystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# External system include directories
if(BUILD_WITH_CONTAINER_SYSTEM AND CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(BUILD_WITH_THREAD_SYSTEM AND THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

# Include directories for external libraries
if(ASIO_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${ASIO_INCLUDE_DIR})
endif()

if(FMT_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${FMT_INCLUDE_DIR})
endif()

# Link libraries (system libraries)
target_link_libraries(NetworkSystem
    PUBLIC
        pthread
)

# Integration layer (conditional compilation)
if(BUILD_MESSAGING_BRIDGE)
    target_sources(NetworkSystem PRIVATE
        src/integration/messaging_bridge.cpp
    )
    add_compile_definitions(BUILD_MESSAGING_BRIDGE)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Samples
if(BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# Installation rules
include(GNUInstallDirs)

# Install library
install(TARGETS NetworkSystem
    EXPORT NetworkSystemTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Config file generation
include(CMakePackageConfigHelpers)

# Generate config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NetworkSystemConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

# Install export targets
install(EXPORT NetworkSystemTargets
    FILE NetworkSystemTargets.cmake
    NAMESPACE NetworkSystem::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

# Print configuration summary
message(STATUS "NetworkSystem Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build samples: ${BUILD_SAMPLES}")
message(STATUS "  Container system integration: ${BUILD_WITH_CONTAINER_SYSTEM}")
message(STATUS "  Thread system integration: ${BUILD_WITH_THREAD_SYSTEM}")
message(STATUS "  Messaging bridge: ${BUILD_MESSAGING_BRIDGE}")

# Cleanup old files
set(OLD_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/session
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/network.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_old.txt
)

# Note: We'll clean these manually after successful build