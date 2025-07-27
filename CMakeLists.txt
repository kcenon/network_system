cmake_minimum_required(VERSION 3.16)

##################################################
# Network System Project
# Independent, reusable network component
##################################################

project(NetworkSystem 
    VERSION 1.0.0
    DESCRIPTION "High-performance asynchronous network messaging library"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_NETWORK_TESTS "Build network tests" ON)
option(BUILD_NETWORK_SAMPLES "Build network samples" ON)
option(BUILD_NETWORK_DOCS "Build network documentation" OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(Threads REQUIRED)

##################################################
# External Dependencies Management
##################################################

# Option to use external container_system
option(USE_EXTERNAL_CONTAINER_SYSTEM "Use external container system" ON)

if(USE_EXTERNAL_CONTAINER_SYSTEM)
    # Try to find external ContainerSystem
    find_package(ContainerSystem QUIET)
    if(ContainerSystem_FOUND)
        message(STATUS "Found external ContainerSystem: ${ContainerSystem_VERSION}")
        set(CONTAINER_TARGET ContainerSystem::container)
    else()
        # Try to find local container_system
        find_path(CONTAINER_SYSTEM_DIR
            NAMES container.h
            PATHS
                ${CMAKE_CURRENT_SOURCE_DIR}/../container_system
                ${CMAKE_PREFIX_PATH}/include/container_system
            NO_DEFAULT_PATH
        )
        
        if(CONTAINER_SYSTEM_DIR)
            message(STATUS "Found local container_system at: ${CONTAINER_SYSTEM_DIR}")
            add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../container_system container_system)
            set(CONTAINER_TARGET container)
        else()
            message(FATAL_ERROR "ContainerSystem is required but not found")
        endif()
    endif()
else()
    message(FATAL_ERROR "External container system is required")
endif()

# Option to use external thread_system utilities
option(USE_EXTERNAL_THREAD_UTILITIES "Use external thread utilities" ON)

if(USE_EXTERNAL_THREAD_UTILITIES)
    find_path(THREAD_UTILITIES_INCLUDE_DIR
        NAMES utilities/conversion/convert_string.h
        PATHS
            ${CMAKE_PREFIX_PATH}/include/thread_system
            ${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/sources
            ${CMAKE_CURRENT_SOURCE_DIR}/../messaging_system/thread_system/sources
        NO_DEFAULT_PATH
    )
    
    if(THREAD_UTILITIES_INCLUDE_DIR)
        message(STATUS "Found thread utilities at: ${THREAD_UTILITIES_INCLUDE_DIR}")
        set(THREAD_UTILITIES_FOUND TRUE)
    else()
        message(WARNING "Thread utilities not found - some features may be limited")
        set(THREAD_UTILITIES_FOUND FALSE)
    endif()
else()
    set(THREAD_UTILITIES_FOUND FALSE)
endif()

##################################################
# Source Files Configuration
##################################################

# Main include file
set(MAIN_HEADER
    ${CMAKE_CURRENT_SOURCE_DIR}/network.h
)

# Core files (public API)
set(CORE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/core/messaging_client.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/messaging_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/messaging_server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/messaging_server.cpp
)

# Session files (public API)
set(SESSION_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/session/messaging_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/session/messaging_session.cpp
)

# Internal implementation files
set(INTERNAL_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/common_defs.h
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/tcp_socket.h
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/tcp_socket.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/send_coroutine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/send_coroutine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/pipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/internal/pipeline.cpp
)

# Combine all files
set(ALL_FILES
    ${MAIN_HEADER}
    ${CORE_FILES}
    ${SESSION_FILES}
    ${INTERNAL_FILES}
)

##################################################
# Library Target Configuration
##################################################

# Create library
if(BUILD_SHARED_LIBS)
    add_library(network SHARED ${ALL_FILES})
else()
    add_library(network STATIC ${ALL_FILES})
endif()

# Create namespace alias
add_library(NetworkSystem::network ALIAS network)

# Set target properties
set_target_properties(network PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

##################################################
# Include Directories
##################################################

# Public include directories
target_include_directories(network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/network_system>
)

# Thread utilities include (if available)
if(THREAD_UTILITIES_FOUND)
    target_include_directories(network PRIVATE
        ${THREAD_UTILITIES_INCLUDE_DIR}
    )
    target_compile_definitions(network PRIVATE
        HAS_THREAD_UTILITIES
    )
endif()

##################################################
# Dependencies
##################################################

# ASIO dependency
find_package(asio CONFIG QUIET)
if(asio_FOUND)
    target_link_libraries(network PUBLIC asio::asio)
    message(STATUS "Found asio via CMake config")
else()
    # Try to find asio headers via find_path
    find_path(ASIO_INCLUDE_DIR
        NAMES asio.hpp
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
    )
    
    if(ASIO_INCLUDE_DIR)
        message(STATUS "Found asio headers at: ${ASIO_INCLUDE_DIR}")
        target_include_directories(network PUBLIC ${ASIO_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "asio library is required but not found")
    endif()
endif()

# fmt dependency (use vcpkg)  
find_package(fmt CONFIG REQUIRED)
target_link_libraries(network PUBLIC fmt::fmt)

# Container system dependency
target_link_libraries(network PUBLIC 
    ${CONTAINER_TARGET}
    Threads::Threads
)

##################################################
# Compile Definitions
##################################################

target_compile_definitions(network PRIVATE
    NETWORK_INTERNAL_FEATURES
)

# Thread utilities feature flag
if(THREAD_UTILITIES_FOUND)
    target_compile_definitions(network PUBLIC
        NETWORK_HAS_THREAD_UTILITIES
    )
endif()

# ASIO configuration
target_compile_definitions(network PUBLIC
    ASIO_STANDALONE
    ASIO_NO_DEPRECATED
)

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(network PUBLIC
        _WIN32_WINNT=0x0601  # Windows 7 or later
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
    )
    
    # Windows sockets
    target_link_libraries(network PUBLIC ws2_32 mswsock)
endif()

# Enable coroutines support if available
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <coroutine>
    int main() {
        return 0;
    }
" HAS_STD_COROUTINES)

if(HAS_STD_COROUTINES)
    target_compile_definitions(network PUBLIC
        ASIO_HAS_CO_AWAIT
        ASIO_HAS_STD_COROUTINE
    )
endif()

##################################################
# Compiler Options
##################################################

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(network PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-missing-field-initializers
    )
elseif(MSVC)
    target_compile_options(network PRIVATE
        /W4
        /WX-
        /wd4100  # unreferenced formal parameter
        /wd4458  # declaration hides class member
        /wd4996  # deprecated functions
    )
endif()

##################################################
# Installation Rules
##################################################

include(GNUInstallDirs)

# Install library
install(TARGETS network
    EXPORT NetworkSystemTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${MAIN_HEADER}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/network_system
)

install(DIRECTORY core/ session/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/network_system
    FILES_MATCHING PATTERN "*.h"
)

# Install internal headers separately
install(DIRECTORY internal/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/network_system/internal
    FILES_MATCHING PATTERN "*.h"
)

##################################################
# Package Configuration (for find_package)
##################################################

include(CMakePackageConfigHelpers)

# Create config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NetworkSystemConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

write_basic_package_version_file(
    NetworkSystemConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

install(EXPORT NetworkSystemTargets
    FILE NetworkSystemTargets.cmake
    NAMESPACE NetworkSystem::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

##################################################
# Summary
##################################################

message(STATUS "NetworkSystem configured:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Container system: ${CONTAINER_TARGET}")
message(STATUS "  Thread utilities: ${THREAD_UTILITIES_FOUND}")
message(STATUS "  Coroutines support: ${HAS_STD_COROUTINES}")
message(STATUS "  Tests: ${BUILD_NETWORK_TESTS}")
message(STATUS "  Samples: ${BUILD_NETWORK_SAMPLES}")
message(STATUS "  Documentation: ${BUILD_NETWORK_DOCS}")