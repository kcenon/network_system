##################################################
# network_system_modules.cmake
#
# C++20 module library target.
#
# This is gated on NETWORK_BUILD_MODULES (declared in
# network_system_options.cmake) because the module target
# requires CMake 3.28+ and a module-capable compiler
# (Clang 16+ / GCC 14+ / MSVC 17.4+).
#
# When the option is OFF (the default), this module is a
# no-op. When ON, it creates the network_system_modules
# library and an alias kcenon::network_modules.
##################################################

if(NOT NETWORK_BUILD_MODULES)
    return()
endif()

if(CMAKE_VERSION VERSION_LESS "3.28")
    message(WARNING "C++20 modules require CMake 3.28+. Disabling module build.")
    set(NETWORK_BUILD_MODULES OFF PARENT_SCOPE)
    return()
endif()

message(STATUS "C++20 module build enabled")

# Create module library target
add_library(network_system_modules)
add_library(kcenon::network_modules ALIAS network_system_modules)

# Set C++20 standard for module target
target_compile_features(network_system_modules PUBLIC cxx_std_20)

# Enable module scanning
set_target_properties(network_system_modules PROPERTIES
    CXX_SCAN_FOR_MODULES ON
)

# Add module source files
target_sources(network_system_modules
    PUBLIC FILE_SET CXX_MODULES
    FILES
        # Primary module
        src/modules/network.cppm
        # Partitions
        src/modules/core.cppm
        src/modules/tcp.cppm
        src/modules/udp.cppm
        src/modules/ssl.cppm
)

# Include directories for module target
target_include_directories(network_system_modules PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link dependencies
# Note: Module targets for common_system and thread_system must be available
if(TARGET kcenon::common_modules)
    target_link_libraries(network_system_modules PUBLIC kcenon::common_modules)
endif()
if(TARGET kcenon::thread_modules)
    target_link_libraries(network_system_modules PUBLIC kcenon::thread_modules)
endif()

# Link ASIO
if(TARGET asio::asio)
    target_link_libraries(network_system_modules PRIVATE asio::asio)
elseif(TARGET asio)
    target_link_libraries(network_system_modules PRIVATE asio)
elseif(ASIO_INCLUDE_DIR)
    target_include_directories(network_system_modules PRIVATE ${ASIO_INCLUDE_DIR})
endif()

# Link OpenSSL for SSL partition
if(BUILD_TLS_SUPPORT AND OPENSSL_FOUND)
    target_link_libraries(network_system_modules PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# Define compile-time flags
target_compile_definitions(network_system_modules PUBLIC
    KCENON_NETWORK_MODULES=1
    KCENON_USE_MODULES=1
)

message(STATUS "C++20 module target: network_system_modules")
