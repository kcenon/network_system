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

#include <type_traits>

#include "kcenon/network/policy/tls_policy.h"
#include "kcenon/network/detail/protocol/protocol_tags.h"

using namespace kcenon::network::policy;
using namespace kcenon::network::protocol;

// ============================================================================
// TLS Policy Tests
// ============================================================================

class TlsPolicyTest : public ::testing::Test {};

TEST_F(TlsPolicyTest, NoTlsHasEnabledFalse) {
    EXPECT_FALSE(no_tls::enabled);
}

TEST_F(TlsPolicyTest, TlsEnabledHasEnabledTrue) {
    EXPECT_TRUE(tls_enabled::enabled);
}

TEST_F(TlsPolicyTest, NoTlsSatisfiesTlsPolicyConcept) {
    EXPECT_TRUE(TlsPolicy<no_tls>);
}

TEST_F(TlsPolicyTest, TlsEnabledSatisfiesTlsPolicyConcept) {
    EXPECT_TRUE(TlsPolicy<tls_enabled>);
}

TEST_F(TlsPolicyTest, IsTlsEnabledVariableTemplate) {
    EXPECT_FALSE(is_tls_enabled_v<no_tls>);
    EXPECT_TRUE(is_tls_enabled_v<tls_enabled>);
}

TEST_F(TlsPolicyTest, IsTlsEnabledTypeTrait) {
    EXPECT_FALSE(is_tls_enabled<no_tls>::value);
    EXPECT_TRUE(is_tls_enabled<tls_enabled>::value);
}

TEST_F(TlsPolicyTest, TlsEnabledDefaultConfiguration) {
    tls_enabled config{};
    EXPECT_TRUE(config.cert_path.empty());
    EXPECT_TRUE(config.key_path.empty());
    EXPECT_TRUE(config.ca_path.empty());
    EXPECT_TRUE(config.verify_peer);
}

TEST_F(TlsPolicyTest, TlsEnabledCustomConfiguration) {
    tls_enabled config{
        .cert_path = "/path/to/cert.pem",
        .key_path = "/path/to/key.pem",
        .ca_path = "/path/to/ca.pem",
        .verify_peer = false
    };

    EXPECT_EQ(config.cert_path, "/path/to/cert.pem");
    EXPECT_EQ(config.key_path, "/path/to/key.pem");
    EXPECT_EQ(config.ca_path, "/path/to/ca.pem");
    EXPECT_FALSE(config.verify_peer);
}

// ============================================================================
// Protocol Tags Tests
// ============================================================================

class ProtocolTagsTest : public ::testing::Test {};

TEST_F(ProtocolTagsTest, TcpProtocolName) {
    EXPECT_EQ(tcp_protocol::name, "tcp");
}

TEST_F(ProtocolTagsTest, UdpProtocolName) {
    EXPECT_EQ(udp_protocol::name, "udp");
}

TEST_F(ProtocolTagsTest, WebsocketProtocolName) {
    EXPECT_EQ(websocket_protocol::name, "websocket");
}

TEST_F(ProtocolTagsTest, QuicProtocolName) {
    EXPECT_EQ(quic_protocol::name, "quic");
}

TEST_F(ProtocolTagsTest, TcpProtocolIsConnectionOriented) {
    EXPECT_TRUE(tcp_protocol::is_connection_oriented);
}

TEST_F(ProtocolTagsTest, UdpProtocolIsNotConnectionOriented) {
    EXPECT_FALSE(udp_protocol::is_connection_oriented);
}

TEST_F(ProtocolTagsTest, WebsocketProtocolIsConnectionOriented) {
    EXPECT_TRUE(websocket_protocol::is_connection_oriented);
}

TEST_F(ProtocolTagsTest, QuicProtocolIsConnectionOriented) {
    EXPECT_TRUE(quic_protocol::is_connection_oriented);
}

TEST_F(ProtocolTagsTest, TcpProtocolIsReliable) {
    EXPECT_TRUE(tcp_protocol::is_reliable);
}

TEST_F(ProtocolTagsTest, UdpProtocolIsNotReliable) {
    EXPECT_FALSE(udp_protocol::is_reliable);
}

TEST_F(ProtocolTagsTest, WebsocketProtocolIsReliable) {
    EXPECT_TRUE(websocket_protocol::is_reliable);
}

TEST_F(ProtocolTagsTest, QuicProtocolIsReliable) {
    EXPECT_TRUE(quic_protocol::is_reliable);
}

// ============================================================================
// Protocol Concept Tests
// ============================================================================

TEST_F(ProtocolTagsTest, TcpProtocolSatisfiesProtocolConcept) {
    EXPECT_TRUE(Protocol<tcp_protocol>);
}

TEST_F(ProtocolTagsTest, UdpProtocolSatisfiesProtocolConcept) {
    EXPECT_TRUE(Protocol<udp_protocol>);
}

TEST_F(ProtocolTagsTest, WebsocketProtocolSatisfiesProtocolConcept) {
    EXPECT_TRUE(Protocol<websocket_protocol>);
}

TEST_F(ProtocolTagsTest, QuicProtocolSatisfiesProtocolConcept) {
    EXPECT_TRUE(Protocol<quic_protocol>);
}

// ============================================================================
// Protocol Helper Variable Templates Tests
// ============================================================================

TEST_F(ProtocolTagsTest, IsConnectionOrientedVariableTemplate) {
    EXPECT_TRUE(is_connection_oriented_v<tcp_protocol>);
    EXPECT_FALSE(is_connection_oriented_v<udp_protocol>);
    EXPECT_TRUE(is_connection_oriented_v<websocket_protocol>);
    EXPECT_TRUE(is_connection_oriented_v<quic_protocol>);
}

TEST_F(ProtocolTagsTest, IsReliableVariableTemplate) {
    EXPECT_TRUE(is_reliable_v<tcp_protocol>);
    EXPECT_FALSE(is_reliable_v<udp_protocol>);
    EXPECT_TRUE(is_reliable_v<websocket_protocol>);
    EXPECT_TRUE(is_reliable_v<quic_protocol>);
}

TEST_F(ProtocolTagsTest, ProtocolNameVariableTemplate) {
    EXPECT_EQ(protocol_name_v<tcp_protocol>, "tcp");
    EXPECT_EQ(protocol_name_v<udp_protocol>, "udp");
    EXPECT_EQ(protocol_name_v<websocket_protocol>, "websocket");
    EXPECT_EQ(protocol_name_v<quic_protocol>, "quic");
}

// ============================================================================
// Protocol Type Trait Tests
// ============================================================================

TEST_F(ProtocolTagsTest, IsConnectionOrientedTypeTrait) {
    EXPECT_TRUE(is_connection_oriented<tcp_protocol>::value);
    EXPECT_FALSE(is_connection_oriented<udp_protocol>::value);
}

TEST_F(ProtocolTagsTest, IsReliableTypeTrait) {
    EXPECT_TRUE(is_reliable<tcp_protocol>::value);
    EXPECT_FALSE(is_reliable<udp_protocol>::value);
}

// ============================================================================
// Compile-Time Usage Tests
// ============================================================================

namespace {

template <Protocol P, TlsPolicy T>
struct mock_client {
    static constexpr std::string_view protocol_name() { return P::name; }
    static constexpr bool uses_tls() { return T::enabled; }
};

}  // namespace

TEST_F(ProtocolTagsTest, TemplateInstantiationWithPolicies) {
    using plain_tcp_client = mock_client<tcp_protocol, no_tls>;
    using secure_tcp_client = mock_client<tcp_protocol, tls_enabled>;
    using plain_udp_client = mock_client<udp_protocol, no_tls>;

    EXPECT_EQ(plain_tcp_client::protocol_name(), "tcp");
    EXPECT_FALSE(plain_tcp_client::uses_tls());

    EXPECT_EQ(secure_tcp_client::protocol_name(), "tcp");
    EXPECT_TRUE(secure_tcp_client::uses_tls());

    EXPECT_EQ(plain_udp_client::protocol_name(), "udp");
    EXPECT_FALSE(plain_udp_client::uses_tls());
}
