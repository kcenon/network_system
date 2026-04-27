# BSD 3-Clause License
# Copyright (c) 2026, kcenon
#
# Hermetic transport fixture support library (Issue #1060).
# Linked into branch-coverage tests that need to drive private async/socket-
# bound methods on protocol clients and servers without a real network.

add_library(network_test_support STATIC
    hermetic_transport_fixture.cpp
    mock_tls_socket.cpp
    mock_udp_peer.cpp
    mock_ws_handshake.cpp
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

set_target_properties(network_test_support PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

add_library(network::test_support ALIAS network_test_support)
