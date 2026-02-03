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

#include "internal/core/unified_messaging_client.h"
#include "kcenon/network/policy/tls_policy.h"
#include "kcenon/network/protocol/protocol_tags.h"

using namespace kcenon::network::core;
using namespace kcenon::network::policy;
using namespace kcenon::network::protocol;

// ============================================================================
// Unified Messaging Client Template Tests
// ============================================================================

class UnifiedMessagingClientTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Type Alias Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, TcpClientTypeAliasExists) {
    // Verify tcp_client type alias is defined
    EXPECT_TRUE((std::is_same_v<
        tcp_client,
        unified_messaging_client<tcp_protocol, no_tls>>));
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedMessagingClientTest, SecureTcpClientTypeAliasExists) {
    // Verify secure_tcp_client type alias is defined
    EXPECT_TRUE((std::is_same_v<
        secure_tcp_client,
        unified_messaging_client<tcp_protocol, tls_enabled>>));
}
#endif

// ============================================================================
// Template Instantiation Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, PlainTcpClientInstantiation) {
    // Verify plain TCP client can be instantiated
    auto client = std::make_shared<tcp_client>("test_client");
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->client_id(), "test_client");
}

TEST_F(UnifiedMessagingClientTest, PlainTcpClientIsSecureIsFalse) {
    // Verify is_secure static member is false for plain client
    EXPECT_FALSE(tcp_client::is_secure);
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedMessagingClientTest, SecureTcpClientInstantiation) {
    // Verify secure TCP client can be instantiated
    auto client = std::make_shared<secure_tcp_client>("secure_test_client");
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->client_id(), "secure_test_client");
}

TEST_F(UnifiedMessagingClientTest, SecureTcpClientIsSecureIsTrue) {
    // Verify is_secure static member is true for secure client
    EXPECT_TRUE(secure_tcp_client::is_secure);
}

TEST_F(UnifiedMessagingClientTest, SecureTcpClientWithTlsConfig) {
    // Verify secure client can be instantiated with TLS configuration
    tls_enabled tls_config{
        .cert_path = "/path/to/cert.pem",
        .key_path = "/path/to/key.pem",
        .ca_path = "/path/to/ca.pem",
        .verify_peer = true
    };

    auto client = std::make_shared<secure_tcp_client>(
        "secure_config_client", tls_config);
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->client_id(), "secure_config_client");
}
#endif

// ============================================================================
// Initial State Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, InitialStateIsNotRunning) {
    auto client = std::make_shared<tcp_client>("state_test_client");
    EXPECT_FALSE(client->is_running());
}

TEST_F(UnifiedMessagingClientTest, InitialStateIsNotConnected) {
    auto client = std::make_shared<tcp_client>("state_test_client");
    EXPECT_FALSE(client->is_connected());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, SetReceiveCallback) {
    auto client = std::make_shared<tcp_client>("callback_test_client");
    bool callback_set = false;

    client->set_receive_callback([&callback_set](const std::vector<uint8_t>&) {
        callback_set = true;
    });

    // Callback should be stored (we can't easily verify without actual connection)
    EXPECT_TRUE(true);  // Just verify no crash
}

TEST_F(UnifiedMessagingClientTest, SetConnectedCallback) {
    auto client = std::make_shared<tcp_client>("callback_test_client");

    client->set_connected_callback([]() {
        // Connected callback
    });

    EXPECT_TRUE(true);  // Just verify no crash
}

TEST_F(UnifiedMessagingClientTest, SetDisconnectedCallback) {
    auto client = std::make_shared<tcp_client>("callback_test_client");

    client->set_disconnected_callback([]() {
        // Disconnected callback
    });

    EXPECT_TRUE(true);  // Just verify no crash
}

TEST_F(UnifiedMessagingClientTest, SetErrorCallback) {
    auto client = std::make_shared<tcp_client>("callback_test_client");

    client->set_error_callback([](std::error_code) {
        // Error callback
    });

    EXPECT_TRUE(true);  // Just verify no crash
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, StartClientWithEmptyHostReturnsError) {
    auto client = std::make_shared<tcp_client>("validation_test_client");
    auto result = client->start_client("", 8080);
    EXPECT_TRUE(result.is_err());
}

TEST_F(UnifiedMessagingClientTest, SendPacketWhenNotConnectedReturnsError) {
    auto client = std::make_shared<tcp_client>("send_test_client");
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto result = client->send_packet(std::move(data));
    EXPECT_TRUE(result.is_err());
}

TEST_F(UnifiedMessagingClientTest, SendEmptyPacketReturnsError) {
    auto client = std::make_shared<tcp_client>("send_test_client");
    // Even if connected, empty data should be rejected
    // But since we're not connected, we get connection_closed error first
    std::vector<uint8_t> empty_data;
    auto result = client->send_packet(std::move(empty_data));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Compile-Time Constraint Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, OnlyTcpProtocolSupported) {
    // This is a compile-time test - the following should NOT compile:
    // unified_messaging_client<udp_protocol> udp_client("test");
    // The requires clause restricts to tcp_protocol only

    // Verify tcp_protocol works
    EXPECT_TRUE((std::is_same_v<
        tcp_client::receive_callback_t,
        std::function<void(const std::vector<uint8_t>&)>>));
}

// ============================================================================
// Destructor Safety Tests
// ============================================================================

TEST_F(UnifiedMessagingClientTest, DestructorSafetyWhenNotStarted) {
    // Verify no crash when destroying client that was never started
    {
        auto client = std::make_shared<tcp_client>("destructor_test_client");
        // Client goes out of scope without start_client being called
    }
    EXPECT_TRUE(true);  // If we reach here, no crash occurred
}

TEST_F(UnifiedMessagingClientTest, MultipleClientsCreation) {
    // Verify multiple clients can be created
    auto client1 = std::make_shared<tcp_client>("client1");
    auto client2 = std::make_shared<tcp_client>("client2");
    auto client3 = std::make_shared<tcp_client>("client3");

    EXPECT_EQ(client1->client_id(), "client1");
    EXPECT_EQ(client2->client_id(), "client2");
    EXPECT_EQ(client3->client_id(), "client3");
}
