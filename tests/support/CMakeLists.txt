# BSD 3-Clause License
# Copyright (c) 2026, kcenon
#
# Hermetic transport fixture support library (Issue #1060).
# Linked into branch-coverage tests that need to drive private async/socket-
# bound methods on protocol clients and servers without a real network.

add_library(network_test_support STATIC
    hermetic_transport_fixture.cpp
    messaging_loopback_fixture.cpp
    mock_tls_socket.cpp
    mock_udp_peer.cpp
    mock_ws_handshake.cpp
    mock_h2_server_peer.cpp
    mock_grpc_server_peer.cpp
    mock_quic_peer_loop.cpp
    quic_server_probe.cpp
    ws_server_probe.cpp
    frame_injector.cpp
)

target_include_directories(network_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Same dependency surface as add_network_test() so tests pick this up by
# linking the alias and inherit ASIO + OpenSSL + GTest transitively.
target_link_libraries(network_test_support
    PUBLIC
        network_system
        GTest::gtest
        Threads::Threads
)

setup_asio_integration(network_test_support)

# Issue #1112: Under coverage instrumentation (-fprofile-arcs -ftest-coverage)
# the SETTINGS handshake driving Http2ClientHermeticTransportTest TEST_F cases
# takes 5.1-5.4s vs 100-300ms in Debug/Release, exceeding the fixture-wide
# wait_for(..., 3s) budget and aborting all instrumented dispatcher tests
# before they reach the branches under measurement. We scale every hermetic-
# fixture wait budget by NETWORK_COVERAGE_TIMEOUT_MULTIPLIER, defined here
# only when ENABLE_COVERAGE is ON. Outside coverage builds the macro defaults
# to 1 in the header, so Debug/Release behaviour is unchanged. PUBLIC so test
# executables consuming hermetic_transport_fixture.h via this library inherit
# the same multiplier at the point of header inclusion.
if(ENABLE_COVERAGE)
    target_compile_definitions(network_test_support PUBLIC
        NETWORK_COVERAGE_TIMEOUT_MULTIPLIER=5)
endif()

set_target_properties(network_test_support PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

add_library(network::test_support ALIAS network_test_support)
