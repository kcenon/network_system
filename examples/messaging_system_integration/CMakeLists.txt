cmake_minimum_required(VERSION 3.16)
project(MessagingSystemIntegration)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find network_system (assuming it's built in ../../build)
set(network_system_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(network_system_BUILD_DIR "${network_system_DIR}/build")

# Add include directories
include_directories(${network_system_DIR}/include)

# Link to network_system library
link_directories(${network_system_BUILD_DIR})

# Modern usage example
add_executable(modern_usage modern_usage.cpp)
target_link_libraries(modern_usage
    PRIVATE
    ${network_system_BUILD_DIR}/libnetwork_system.a
    pthread
)

# Add container_system include if available
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../container_system")
    target_include_directories(modern_usage
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../container_system
    )
    target_compile_definitions(modern_usage PRIVATE WITH_CONTAINER_SYSTEM)
endif()

# Platform-specific settings
if(APPLE)
    # macOS specific settings
    # Add ASIO include path
    target_include_directories(modern_usage PRIVATE /opt/homebrew/include)

    target_compile_definitions(modern_usage PRIVATE ASIO_STANDALONE)
elseif(WIN32)
    # Windows specific settings
    target_link_libraries(modern_usage PRIVATE ws2_32 mswsock)
    target_compile_definitions(modern_usage PRIVATE _WIN32_WINNT=0x0601)
endif()

# Output directory
set_target_properties(modern_usage
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# Installation rules (optional)
install(TARGETS modern_usage
    RUNTIME DESTINATION bin
)

message(STATUS "Messaging System Integration Examples configured")
message(STATUS "  - modern_usage: Shows modern API features")