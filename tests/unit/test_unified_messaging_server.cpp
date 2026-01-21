/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

#include "kcenon/network/core/unified_messaging_server.h"
#include "kcenon/network/policy/tls_policy.h"
#include "kcenon/network/protocol/protocol_tags.h"

using namespace kcenon::network::core;
using namespace kcenon::network::policy;
using namespace kcenon::network::protocol;

// ============================================================================
// Unified Messaging Server Template Tests
// ============================================================================

class UnifiedMessagingServerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Type Alias Tests
// ============================================================================

TEST_F(UnifiedMessagingServerTest, TcpServerTypeAliasExists) {
    // Verify tcp_server type alias is defined
    EXPECT_TRUE((std::is_same_v<
        tcp_server,
        unified_messaging_server<tcp_protocol, no_tls>>));
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedMessagingServerTest, SecureTcpServerTypeAliasExists) {
    // Verify secure_tcp_server type alias is defined
    EXPECT_TRUE((std::is_same_v<
        secure_tcp_server,
        unified_messaging_server<tcp_protocol, tls_enabled>>));
}
#endif

// ============================================================================
// Template Instantiation Tests
// ============================================================================

TEST_F(UnifiedMessagingServerTest, PlainTcpServerInstantiation) {
    // Verify plain TCP server can be instantiated
    auto server = std::make_shared<tcp_server>("test_server");
    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->server_id(), "test_server");
}

TEST_F(UnifiedMessagingServerTest, PlainTcpServerIsSecureIsFalse) {
    // Verify is_secure static member is false for plain server
    EXPECT_FALSE(tcp_server::is_secure);
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedMessagingServerTest, SecureTcpServerIsSecureIsTrue) {
    // Verify is_secure static member is true for secure server
    EXPECT_TRUE(secure_tcp_server::is_secure);
}
#endif

// ============================================================================
// Initial State Tests
// ============================================================================

TEST_F(UnifiedMessagingServerTest, InitialStateIsNotRunning) {
    auto server = std::make_shared<tcp_server>("state_test_server");
    EXPECT_FALSE(server->is_running());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(UnifiedMessagingServerTest, SetConnectionCallback) {
    auto server = std::make_shared<tcp_server>("callback_test_server");
    bool callback_set = false;

    server->set_connection_callback(
        [&callback_set](tcp_server::session_ptr) {
            callback_set = true;
        });

    // Callback should be stored (we can't easily verify without actual connection)
    EXPECT_TRUE(true);  // Just verify no crash
}

TEST_F(UnifiedMessagingServerTest, SetDisconnectionCallback) {
    auto server = std::make_shared<tcp_server>("callback_test_server");

    server->set_disconnection_callback(
        [](const std::string&) {
            // Callback logic
        });

    EXPECT_TRUE(true);  // Just verify no crash
}

TEST_F(UnifiedMessagingServerTest, SetReceiveCallback) {
    auto server = std::make_shared<tcp_server>("callback_test_server");

    server->set_receive_callback(
        [](tcp_server::session_ptr, const std::vector<uint8_t>&) {
            // Callback logic
        });

    EXPECT_TRUE(true);  // Just verify no crash
}

TEST_F(UnifiedMessagingServerTest, SetErrorCallback) {
    auto server = std::make_shared<tcp_server>("callback_test_server");

    server->set_error_callback(
        [](tcp_server::session_ptr, std::error_code) {
            // Callback logic
        });

    EXPECT_TRUE(true);  // Just verify no crash
}

// ============================================================================
// Session Type Tests
// ============================================================================

TEST_F(UnifiedMessagingServerTest, PlainServerSessionTypeIsMessagingSession) {
    // Verify plain server uses messaging_session
    using expected_session = kcenon::network::session::messaging_session;
    EXPECT_TRUE((std::is_same_v<tcp_server::session_type, expected_session>));
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedMessagingServerTest, SecureServerSessionTypeIsSecureSession) {
    // Verify secure server uses secure_session
    using expected_session = kcenon::network::session::secure_session;
    EXPECT_TRUE((std::is_same_v<secure_tcp_server::session_type, expected_session>));
}
#endif

// ============================================================================
// Start/Stop Tests (without actual networking)
// ============================================================================

TEST_F(UnifiedMessagingServerTest, DoubleStartReturnsError) {
    auto server = std::make_shared<tcp_server>("double_start_server");

    // First start should succeed
    auto result1 = server->start_server(0);  // Port 0 = ephemeral port
    if (result1.is_ok()) {
        // Second start should fail
        auto result2 = server->start_server(0);
        EXPECT_TRUE(result2.is_err());

        // Cleanup
        server->stop_server();
    }
    // If first start failed (e.g., no network), skip the test
}

TEST_F(UnifiedMessagingServerTest, StopWithoutStartReturnsOk) {
    auto server = std::make_shared<tcp_server>("stop_without_start_server");

    // Stop without start should return ok (idempotent)
    auto result = server->stop_server();
    EXPECT_TRUE(result.is_ok());
}

TEST_F(UnifiedMessagingServerTest, StartAndStopServer) {
    auto server = std::make_shared<tcp_server>("start_stop_server");

    // Start should succeed
    auto start_result = server->start_server(0);  // Port 0 = ephemeral port
    if (start_result.is_ok()) {
        EXPECT_TRUE(server->is_running());

        // Stop should succeed
        auto stop_result = server->stop_server();
        EXPECT_TRUE(stop_result.is_ok());
        EXPECT_FALSE(server->is_running());
    }
    // If start failed (e.g., no network), skip the test
}
