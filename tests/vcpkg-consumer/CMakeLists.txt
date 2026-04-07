cmake_minimum_required(VERSION 3.20)
project(vcpkg_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Track which packages are found
set(FOUND_PACKAGES "")
set(MISSING_PACKAGES "")

# --- Tier 0: common_system (transitive dep, required) ---

find_package(common_system CONFIG)
if(common_system_FOUND)
    message(STATUS "Found common_system: ${common_system_DIR}")
    list(APPEND FOUND_PACKAGES "common_system")
else()
    message(WARNING "common_system not found")
    list(APPEND MISSING_PACKAGES "common_system")
endif()

# --- Tier 1: thread_system (transitive dep, required) ---

find_package(thread_system CONFIG)
if(thread_system_FOUND)
    message(STATUS "Found thread_system: ${thread_system_DIR}")
    list(APPEND FOUND_PACKAGES "thread_system")
else()
    message(WARNING "thread_system not found")
    list(APPEND MISSING_PACKAGES "thread_system")
endif()

# --- Tier 4: network_system (primary target) ---

find_package(OpenSSL QUIET)

find_package(network_system CONFIG REQUIRED)
message(STATUS "Found network_system: ${network_system_DIR}")
list(APPEND FOUND_PACKAGES "network_system")

# --- Consumer binary ---

add_executable(vcpkg_consumer main.cpp)

# network_system is the primary link target; transitive deps are pulled automatically
target_link_libraries(vcpkg_consumer PRIVATE network_system::network_system)
target_compile_definitions(vcpkg_consumer PRIVATE HAS_NETWORK_SYSTEM)

if(TARGET kcenon::common_system)
    target_compile_definitions(vcpkg_consumer PRIVATE HAS_COMMON_SYSTEM)
endif()

if(TARGET thread_system::thread_system)
    target_compile_definitions(vcpkg_consumer PRIVATE HAS_THREAD_SYSTEM)
endif()

# --- Summary ---

list(LENGTH FOUND_PACKAGES _found_count)
list(LENGTH MISSING_PACKAGES _missing_count)

message(STATUS "")
message(STATUS "vcpkg consumer configuration:")
message(STATUS "  Found:   ${_found_count} packages (${FOUND_PACKAGES})")
if(_missing_count GREATER 0)
    message(STATUS "  Missing: ${_missing_count} packages (${MISSING_PACKAGES})")
endif()
message(STATUS "")
