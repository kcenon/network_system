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

// Include all interfaces
#include "kcenon/network/interfaces/i_network_component.h"
#include "kcenon/network/interfaces/i_session.h"
#include "kcenon/network/interfaces/connection_observer.h"
#include "kcenon/network/interfaces/i_client.h"
#include "kcenon/network/interfaces/i_server.h"
#include "kcenon/network/interfaces/i_udp_client.h"
#include "kcenon/network/interfaces/i_udp_server.h"
#include "kcenon/network/interfaces/i_websocket_client.h"
#include "kcenon/network/interfaces/i_websocket_server.h"
#include "kcenon/network/interfaces/i_quic_client.h"
#include "kcenon/network/interfaces/i_quic_server.h"

using namespace kcenon::network::interfaces;

// ============================================================================
// Interface Type Trait Tests
// ============================================================================

class InterfaceTypeTraitsTest : public ::testing::Test
{
};

TEST_F(InterfaceTypeTraitsTest, NetworkComponentIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_network_component>);
}

TEST_F(InterfaceTypeTraitsTest, SessionIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_session>);
}

TEST_F(InterfaceTypeTraitsTest, ClientIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_client>);
}

TEST_F(InterfaceTypeTraitsTest, ServerIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_server>);
}

TEST_F(InterfaceTypeTraitsTest, UdpClientIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_udp_client>);
}

TEST_F(InterfaceTypeTraitsTest, UdpServerIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_udp_server>);
}

TEST_F(InterfaceTypeTraitsTest, WebSocketClientIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_websocket_client>);
}

TEST_F(InterfaceTypeTraitsTest, WebSocketServerIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_websocket_server>);
}

TEST_F(InterfaceTypeTraitsTest, QuicClientIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_quic_client>);
}

TEST_F(InterfaceTypeTraitsTest, QuicServerIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<i_quic_server>);
}

// ============================================================================
// Interface Hierarchy Tests
// ============================================================================

class InterfaceHierarchyTest : public ::testing::Test
{
};

TEST_F(InterfaceHierarchyTest, ClientExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_client>));
}

TEST_F(InterfaceHierarchyTest, ServerExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_server>));
}

TEST_F(InterfaceHierarchyTest, UdpClientExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_udp_client>));
}

TEST_F(InterfaceHierarchyTest, UdpServerExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_udp_server>));
}

TEST_F(InterfaceHierarchyTest, WebSocketClientExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_websocket_client>));
}

TEST_F(InterfaceHierarchyTest, WebSocketServerExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_websocket_server>));
}

TEST_F(InterfaceHierarchyTest, QuicClientExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_quic_client>));
}

TEST_F(InterfaceHierarchyTest, QuicServerExtendsNetworkComponent)
{
	EXPECT_TRUE((std::is_base_of_v<i_network_component, i_quic_server>));
}

TEST_F(InterfaceHierarchyTest, WebSocketSessionExtendsSession)
{
	EXPECT_TRUE((std::is_base_of_v<i_session, i_websocket_session>));
}

TEST_F(InterfaceHierarchyTest, QuicSessionExtendsSession)
{
	EXPECT_TRUE((std::is_base_of_v<i_session, i_quic_session>));
}

// ============================================================================
// Interface Non-Copyable Tests
// ============================================================================

class InterfaceNonCopyableTest : public ::testing::Test
{
};

TEST_F(InterfaceNonCopyableTest, NetworkComponentIsNotCopyable)
{
	EXPECT_FALSE(std::is_copy_constructible_v<i_network_component>);
	EXPECT_FALSE(std::is_copy_assignable_v<i_network_component>);
}

TEST_F(InterfaceNonCopyableTest, SessionIsNotCopyable)
{
	EXPECT_FALSE(std::is_copy_constructible_v<i_session>);
	EXPECT_FALSE(std::is_copy_assignable_v<i_session>);
}

TEST_F(InterfaceNonCopyableTest, NetworkComponentIsNotMovable)
{
	EXPECT_FALSE(std::is_move_constructible_v<i_network_component>);
	EXPECT_FALSE(std::is_move_assignable_v<i_network_component>);
}

TEST_F(InterfaceNonCopyableTest, SessionIsNotMovable)
{
	EXPECT_FALSE(std::is_move_constructible_v<i_session>);
	EXPECT_FALSE(std::is_move_assignable_v<i_session>);
}

// ============================================================================
// Callback Type Tests
// ============================================================================

class CallbackTypeTest : public ::testing::Test
{
};

TEST_F(CallbackTypeTest, ClientCallbackTypesAreDefined)
{
	// Verify callback types exist and are invocable
	EXPECT_TRUE((std::is_invocable_v<i_client::receive_callback_t, const std::vector<uint8_t>&>));
	EXPECT_TRUE((std::is_invocable_v<i_client::connected_callback_t>));
	EXPECT_TRUE((std::is_invocable_v<i_client::disconnected_callback_t>));
	EXPECT_TRUE((std::is_invocable_v<i_client::error_callback_t, std::error_code>));
}

TEST_F(CallbackTypeTest, UdpClientCallbackTypesAreDefined)
{
	// UDP receive callback includes endpoint info
	EXPECT_TRUE((std::is_invocable_v<i_udp_client::receive_callback_t, const std::vector<uint8_t>&,
	                                 const i_udp_client::endpoint_info&>));
	EXPECT_TRUE((std::is_invocable_v<i_udp_client::error_callback_t, std::error_code>));
}

TEST_F(CallbackTypeTest, WebSocketClientCallbackTypesAreDefined)
{
	EXPECT_TRUE((std::is_invocable_v<i_websocket_client::text_callback_t, const std::string&>));
	EXPECT_TRUE((std::is_invocable_v<i_websocket_client::binary_callback_t, const std::vector<uint8_t>&>));
	EXPECT_TRUE((std::is_invocable_v<i_websocket_client::connected_callback_t>));
	EXPECT_TRUE((std::is_invocable_v<i_websocket_client::disconnected_callback_t, uint16_t, std::string_view>));
}

TEST_F(CallbackTypeTest, QuicClientCallbackTypesAreDefined)
{
	EXPECT_TRUE((std::is_invocable_v<i_quic_client::receive_callback_t, const std::vector<uint8_t>&>));
	EXPECT_TRUE((std::is_invocable_v<i_quic_client::stream_callback_t, uint64_t, const std::vector<uint8_t>&, bool>));
	EXPECT_TRUE((std::is_invocable_v<i_quic_client::session_ticket_callback_t, std::vector<uint8_t>, uint32_t,
	                                 uint32_t>));
}

// ============================================================================
// Endpoint Info Tests
// ============================================================================

class EndpointInfoTest : public ::testing::Test
{
};

TEST_F(EndpointInfoTest, UdpEndpointInfoHasRequiredFields)
{
	i_udp_client::endpoint_info ep;
	ep.address = "127.0.0.1";
	ep.port = 8080;

	EXPECT_EQ(ep.address, "127.0.0.1");
	EXPECT_EQ(ep.port, 8080);
}

TEST_F(EndpointInfoTest, UdpServerUsesClientEndpointInfo)
{
	// Verify that UDP server uses the same endpoint_info type as UDP client
	EXPECT_TRUE((std::is_same_v<i_udp_server::endpoint_info, i_udp_client::endpoint_info>));
}

// ============================================================================
// Connection Observer Tests
// ============================================================================

class ConnectionObserverTest : public ::testing::Test
{
};

TEST_F(ConnectionObserverTest, ConnectionObserverIsAbstract)
{
	EXPECT_TRUE(std::is_abstract_v<connection_observer>);
}

TEST_F(ConnectionObserverTest, NullConnectionObserverIsNotAbstract)
{
	EXPECT_FALSE(std::is_abstract_v<null_connection_observer>);
}

TEST_F(ConnectionObserverTest, CallbackAdapterIsNotAbstract)
{
	EXPECT_FALSE(std::is_abstract_v<callback_adapter>);
}

TEST_F(ConnectionObserverTest, NullConnectionObserverExtendsConnectionObserver)
{
	EXPECT_TRUE((std::is_base_of_v<connection_observer, null_connection_observer>));
}

TEST_F(ConnectionObserverTest, CallbackAdapterExtendsConnectionObserver)
{
	EXPECT_TRUE((std::is_base_of_v<connection_observer, callback_adapter>));
}

TEST_F(ConnectionObserverTest, NullConnectionObserverDoesNothing)
{
	null_connection_observer observer;

	// These should not throw or crash
	std::array<uint8_t, 4> data = {1, 2, 3, 4};
	observer.on_receive(std::span<const uint8_t>(data));
	observer.on_connected();
	observer.on_disconnected(std::nullopt);
	observer.on_disconnected("test reason");
	observer.on_error(std::make_error_code(std::errc::connection_refused));
}

TEST_F(ConnectionObserverTest, CallbackAdapterInvokesCallbacks)
{
	callback_adapter adapter;

	bool receive_called = false;
	bool connected_called = false;
	bool disconnected_called = false;
	bool error_called = false;
	std::optional<std::string_view> disconnect_reason;

	adapter.on_receive([&receive_called](std::span<const uint8_t>) { receive_called = true; })
		.on_connected([&connected_called]() { connected_called = true; })
		.on_disconnected(
			[&disconnected_called, &disconnect_reason](std::optional<std::string_view> reason)
			{
				disconnected_called = true;
				disconnect_reason = reason;
			})
		.on_error([&error_called](std::error_code) { error_called = true; });

	std::array<uint8_t, 4> data = {1, 2, 3, 4};
	adapter.on_receive(std::span<const uint8_t>(data));
	EXPECT_TRUE(receive_called);

	adapter.on_connected();
	EXPECT_TRUE(connected_called);

	adapter.on_disconnected("test");
	EXPECT_TRUE(disconnected_called);
	EXPECT_TRUE(disconnect_reason.has_value());
	EXPECT_EQ(disconnect_reason.value(), "test");

	adapter.on_error(std::make_error_code(std::errc::connection_refused));
	EXPECT_TRUE(error_called);
}

TEST_F(ConnectionObserverTest, CallbackAdapterWithoutCallbacksDoesNotCrash)
{
	callback_adapter adapter;

	// These should not throw or crash even without callbacks set
	std::array<uint8_t, 4> data = {1, 2, 3, 4};
	adapter.on_receive(std::span<const uint8_t>(data));
	adapter.on_connected();
	adapter.on_disconnected(std::nullopt);
	adapter.on_error(std::make_error_code(std::errc::connection_refused));
}

TEST_F(ConnectionObserverTest, CallbackAdapterSupportsChainingAPI)
{
	callback_adapter adapter;

	// Verify chaining returns reference to the same adapter
	callback_adapter& ref1 = adapter.on_receive([](std::span<const uint8_t>) {});
	callback_adapter& ref2 = ref1.on_connected([]() {});
	callback_adapter& ref3 = ref2.on_disconnected([](std::optional<std::string_view>) {});
	callback_adapter& ref4 = ref3.on_error([](std::error_code) {});

	EXPECT_EQ(&adapter, &ref1);
	EXPECT_EQ(&adapter, &ref2);
	EXPECT_EQ(&adapter, &ref3);
	EXPECT_EQ(&adapter, &ref4);
}

TEST_F(ConnectionObserverTest, CallbackAdapterCanBeUsedAsSharedPtr)
{
	auto adapter = std::make_shared<callback_adapter>();

	bool called = false;
	adapter->on_receive([&called](std::span<const uint8_t>) { called = true; });

	std::shared_ptr<connection_observer> observer = adapter;
	std::array<uint8_t, 2> data = {1, 2};
	observer->on_receive(std::span<const uint8_t>(data));

	EXPECT_TRUE(called);
}
