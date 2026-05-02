##################################################
# network_system_protocol_libs.cmake
#
# add_subdirectory() orchestration for the modular
# protocol libraries (libs/network-tcp, libs/network-udp,
# libs/network-websocket, libs/network-http2, libs/network-quic,
# libs/network-grpc, libs/network-all).
#
# Each block follows the same pattern:
#   1. check the gating option (and the existence of the
#      directory's CMakeLists.txt where applicable)
#   2. add_subdirectory(...)
#   3. log a "<name> ... enabled" status line
#
# Preconditions:
#   - network_system_integration_objects.cmake has been
#     included (network-integration-objects must exist
#     before libs/network-tcp / libs/network-udp link
#     against $<TARGET_OBJECTS:network-integration-objects>)
#
# Postconditions: network-tcp, network-udp, etc. targets
# exist when present, so the subsequent
# `target_link_libraries(network_system PUBLIC network-tcp)`
# in network_system_targets.cmake takes effect.
##################################################

# network-tcp: TCP protocol implementation library (Issue #554)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-tcp/CMakeLists.txt)
    add_subdirectory(libs/network-tcp)
    message(STATUS "network-tcp library enabled")
endif()

# network-udp: UDP protocol implementation library (Issue #561)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-udp/CMakeLists.txt)
    add_subdirectory(libs/network-udp)
    message(STATUS "network-udp library enabled")
endif()

# network-websocket: WebSocket protocol implementation library (Issue #565)
if(BUILD_WEBSOCKET_SUPPORT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-websocket/CMakeLists.txt)
    add_subdirectory(libs/network-websocket)
    message(STATUS "network-websocket library enabled")
endif()

# network-http2: HTTP/2 protocol implementation library (Issue #567)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-http2/CMakeLists.txt)
    add_subdirectory(libs/network-http2)
    message(STATUS "network-http2 library enabled")
endif()

# network-quic: QUIC protocol implementation library (Issue #569)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-quic/CMakeLists.txt)
    add_subdirectory(libs/network-quic)
    message(STATUS "network-quic library enabled")
endif()

# network-grpc: gRPC protocol implementation library (Issue #571)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-grpc/CMakeLists.txt)
    add_subdirectory(libs/network-grpc)
    message(STATUS "network-grpc library enabled")
endif()

# network-all: Umbrella package aggregating all protocol libraries (Issue #573)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-all/CMakeLists.txt)
    add_subdirectory(libs/network-all)
    message(STATUS "network-all umbrella package enabled")
endif()
