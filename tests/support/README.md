# Hermetic Transport Fixture (Issue #1060)

A static helper library — `network::test_support` — that provides byte-level
loopback peers for HTTP/2, gRPC, QUIC, and WebSocket unit tests.

The four lower-coverage protocol files in the repository
(`http2_client.cpp`, `http2_server.cpp`, `grpc/client.cpp`,
`websocket_server.cpp`, `quic_socket.cpp`, `quic_server.cpp`) carry a class of
private async/socket-bound methods that cannot be exercised by pure public-API
tests. Sub-issues #1048-#1053 added 4,383 LOC of hermetic gtest cases for
these files yet moved overall filtered coverage by only +0.05pp / +0.07pp,
because the private methods named in each file's "Honest scope statement"
remained unreachable. This fixture is the structural complement that lets
follow-up tests drive those methods.

## What "hermetic" means here

- Every socket binds to `127.0.0.1` with a kernel-assigned port (`:0`).
- No DNS lookup, no external network interface, no on-disk secrets.
- TLS certificates are generated in memory at fixture init using the OpenSSL
  X509/EVP APIs (already a transitive dependency via `asio::ssl`).
- Concurrent test executions never collide because every port is ephemeral.

## Files

| File | Purpose |
|------|---------|
| `hermetic_transport_fixture.h/.cpp` | GTest fixture base with shared `io_context` + worker thread, plus loopback TCP/UDP pair builders. |
| `mock_tls_socket.h/.cpp` | RSA-2048 self-signed cert generation, server/client `ssl::context` factories, and a `tls_loopback_listener` RAII helper. |
| `mock_udp_peer.h/.cpp` | UDP peer wrapper for QUIC tests plus a stub QUIC long-header Initial-packet builder. |
| `mock_ws_handshake.h/.cpp` | RFC 6455 client upgrade request and frame builders for WebSocket server tests. |

## Composition pattern

```cpp
#include "hermetic_transport_fixture.h"
#include "mock_tls_socket.h"

class MyHttp2Test : public kcenon::network::tests::support::hermetic_transport_fixture
{
};

TEST_F(MyHttp2Test, ConnectsToLoopbackTlsListener)
{
    using namespace kcenon::network::tests::support;

    tls_loopback_listener listener(io());            // bind 127.0.0.1:0, accept one
    auto client = std::make_shared<http2::http2_client>();

    std::thread connector([&]() {
        client->set_timeout(std::chrono::seconds(2));
        (void)client->connect("127.0.0.1", listener.port());
    });

    EXPECT_TRUE(wait_for([&]() { return listener.accepted(); },
                         std::chrono::seconds(3)));
    client->disconnect();
    connector.join();
}
```

The same composition applies to the other three protocol families:

- HTTP/2 server / gRPC client → `tls_loopback_listener`
- HTTP/2 server connection → `make_loopback_tcp_pair(io())`
- QUIC socket / server → `make_loopback_udp_pair(io())` + `mock_udp_peer`
- WebSocket server → `make_loopback_tcp_pair(io())` + `mock_ws_handshake`
  builders

## Linking from a test target

The fixture is built as `network_test_support` (alias `network::test_support`)
and is added once via `add_subdirectory(support)` in `tests/CMakeLists.txt`.
Existing `add_network_test()` invocations link it explicitly:

```cmake
add_network_test(my_branch_test unit/my_branch_test.cpp)
target_link_libraries(my_branch_test PRIVATE network::test_support)
```

Header search paths and ASIO/OpenSSL/GTest include directories are
transitively provided by the support library's `PUBLIC` link.

## Honest-scope acknowledgements

This PR ships the fixture infrastructure plus one demonstration `TEST_F` per
target file (six branch_test files in total). Each demo verifies that the
fixture composes cleanly with the protocol class under test — full per-file
branch-coverage expansion lives in follow-up issues so each PR stays in the
S/M size band.

For the QUIC server and WebSocket server cases, `handle_packet()` and
`handle_new_connection()` remain private; the demo tests therefore show
byte-level synthesis (packet stub builder, RFC 6455 request builder) rather
than direct private-method invocation. A future change adding a friend-test
injection point or a full start_server() loop will let those drives complete.
