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

/**
 * @file test_udp_composition.cpp
 * @brief Unit tests for UDP composition pattern implementation
 *
 * Tests cover:
 * - Interface API compliance (i_udp_client, i_udp_server)
 * - Lifecycle management (start/stop)
 * - Callback management (receive, error)
 * - ID accessors (client_id, server_id)
 * - Target endpoint management (set_target)
 * - Server send functionality (send_to)
 */

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "internal/core/messaging_udp_client.h"
#include "internal/core/messaging_udp_server.h"
// Note: i_udp_client and i_udp_server interfaces are already included
// via the internal headers above (internal/interfaces/i_udp_*.h)

using namespace kcenon::network::core;
using namespace kcenon::network::interfaces;
using namespace std::chrono_literals;

namespace
{
	// Helper to yield for async operations
	void wait_for_ready()
	{
		std::this_thread::sleep_for(50ms);
	}

	// Helper to find available port
	uint16_t find_available_port(uint16_t start = 19000)
	{
		for (uint16_t port = start; port < 65535; ++port)
		{
			try
			{
				asio::io_context io_context;
				asio::ip::udp::socket socket(io_context);
				asio::ip::udp::endpoint endpoint(asio::ip::udp::v4(), port);
				socket.open(endpoint.protocol());
				socket.bind(endpoint);
				socket.close();
				return port;
			}
			catch (...)
			{
				continue;
			}
		}
		return 0;
	}
} // namespace

// ============================================================================
// UDP Client ID Tests
// ============================================================================

class UdpClientIdTest : public ::testing::Test
{
};

TEST_F(UdpClientIdTest, ClientIdReturnsCorrectValue)
{
	auto client = std::make_shared<messaging_udp_client>("test_client_id");
	EXPECT_EQ(client->client_id(), "test_client_id");
}

TEST_F(UdpClientIdTest, ClientIdPreservedAfterStart)
{
	auto port = find_available_port();
	ASSERT_NE(port, 0);

	auto client = std::make_shared<messaging_udp_client>("preserved_id");

	auto result = client->start_client("127.0.0.1", port);
	EXPECT_TRUE(result.is_ok());
	wait_for_ready();

	EXPECT_EQ(client->client_id(), "preserved_id");

	[[maybe_unused]] auto stop_result = client->stop_client();
}

// ============================================================================
// UDP Server ID Tests
// ============================================================================

class UdpServerIdTest : public ::testing::Test
{
};

TEST_F(UdpServerIdTest, ServerIdReturnsCorrectValue)
{
	auto server = std::make_shared<messaging_udp_server>("test_server_id");
	EXPECT_EQ(server->server_id(), "test_server_id");
}

TEST_F(UdpServerIdTest, ServerIdPreservedAfterStart)
{
	auto port = find_available_port();
	ASSERT_NE(port, 0);

	auto server = std::make_shared<messaging_udp_server>("preserved_server_id");

	auto result = server->start_server(port);
	EXPECT_TRUE(result.is_ok());
	wait_for_ready();

	EXPECT_EQ(server->server_id(), "preserved_server_id");

	[[maybe_unused]] auto stop_result = server->stop_server();
}

// ============================================================================
// UDP Client Interface API Tests
// ============================================================================

class UdpClientInterfaceTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		test_port_ = find_available_port();
		ASSERT_NE(test_port_, 0);
	}

	uint16_t test_port_;
};

TEST_F(UdpClientInterfaceTest, InterfaceStartStop)
{
	auto client = std::make_shared<messaging_udp_client>("interface_client");

	// Use interface methods start/stop
	auto start_result = client->start("127.0.0.1", test_port_);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(client->is_running());
	wait_for_ready();

	auto stop_result = client->stop();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(client->is_running());
}

TEST_F(UdpClientInterfaceTest, InterfaceSend)
{
	auto server = std::make_shared<messaging_udp_server>("recv_server");
	auto server_result = server->start_server(test_port_);
	ASSERT_TRUE(server_result.is_ok());
	wait_for_ready();

	auto client = std::make_shared<messaging_udp_client>("send_client");
	auto client_result = client->start("127.0.0.1", test_port_);
	ASSERT_TRUE(client_result.is_ok());
	wait_for_ready();

	// Use interface send method
	std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
	std::atomic<bool> sent{false};

	auto send_result = client->send(std::move(data), [&sent](std::error_code ec, std::size_t) {
		if (!ec)
		{
			sent.store(true);
		}
	});
	EXPECT_TRUE(send_result.is_ok());

	wait_for_ready();
	EXPECT_TRUE(sent.load());

	[[maybe_unused]] auto client_stop = client->stop();
	[[maybe_unused]] auto server_stop = server->stop_server();
}

TEST_F(UdpClientInterfaceTest, InterfaceSetTarget)
{
	auto client = std::make_shared<messaging_udp_client>("target_client");

	// Set target before start should fail
	auto result_before = client->set_target("127.0.0.1", test_port_);
	EXPECT_FALSE(result_before.is_ok());

	// Start client
	auto start_result = client->start("127.0.0.1", test_port_);
	ASSERT_TRUE(start_result.is_ok());
	wait_for_ready();

	// Set target after start should succeed
	auto new_port = find_available_port(test_port_ + 1);
	ASSERT_NE(new_port, 0);

	auto result_after = client->set_target("127.0.0.1", new_port);
	EXPECT_TRUE(result_after.is_ok());

	[[maybe_unused]] auto stop_result = client->stop();
}

TEST_F(UdpClientInterfaceTest, InterfaceSetReceiveCallback)
{
	auto server = std::make_shared<messaging_udp_server>("echo_server");
	auto server_result = server->start_server(test_port_);
	ASSERT_TRUE(server_result.is_ok());
	wait_for_ready();

	auto client = std::make_shared<messaging_udp_client>("callback_client");

	std::atomic<bool> callback_invoked{false};
	i_udp_client::endpoint_info received_endpoint;

	// Use interface callback version
	client->set_receive_callback([&callback_invoked, &received_endpoint](const std::vector<uint8_t>&,
	                                                                     const i_udp_client::endpoint_info& ep) {
		callback_invoked.store(true);
		received_endpoint = ep;
	});

	auto start_result = client->start("127.0.0.1", test_port_);
	ASSERT_TRUE(start_result.is_ok());
	wait_for_ready();

	[[maybe_unused]] auto client_stop = client->stop();
	[[maybe_unused]] auto server_stop = server->stop_server();
}

TEST_F(UdpClientInterfaceTest, InterfaceSetErrorCallback)
{
	auto client = std::make_shared<messaging_udp_client>("error_callback_client");

	std::atomic<bool> error_callback_set{false};

	// Use interface error callback
	client->set_error_callback([&error_callback_set](std::error_code) { error_callback_set.store(true); });

	auto start_result = client->start("127.0.0.1", test_port_);
	ASSERT_TRUE(start_result.is_ok());
	wait_for_ready();

	[[maybe_unused]] auto stop_result = client->stop();
}

// ============================================================================
// UDP Server Interface API Tests
// ============================================================================

class UdpServerInterfaceTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		test_port_ = find_available_port(20000);
		ASSERT_NE(test_port_, 0);
	}

	uint16_t test_port_;
};

TEST_F(UdpServerInterfaceTest, InterfaceStartStop)
{
	auto server = std::make_shared<messaging_udp_server>("interface_server");

	// Use interface methods start/stop
	auto start_result = server->start(test_port_);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(server->is_running());
	wait_for_ready();

	auto stop_result = server->stop();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

TEST_F(UdpServerInterfaceTest, InterfaceSendTo)
{
	auto server = std::make_shared<messaging_udp_server>("sendto_server");
	auto server_result = server->start(test_port_);
	ASSERT_TRUE(server_result.is_ok());
	wait_for_ready();

	// Use interface send_to method
	i_udp_server::endpoint_info target;
	target.address = "127.0.0.1";
	target.port = test_port_ + 1; // arbitrary target

	std::vector<uint8_t> data = {0x05, 0x06, 0x07, 0x08};
	std::atomic<bool> send_attempted{false};

	auto send_result =
		server->send_to(target, std::move(data), [&send_attempted](std::error_code, std::size_t) { send_attempted.store(true); });
	EXPECT_TRUE(send_result.is_ok());

	wait_for_ready();

	[[maybe_unused]] auto stop_result = server->stop();
}

TEST_F(UdpServerInterfaceTest, InterfaceSendToNotRunning)
{
	auto server = std::make_shared<messaging_udp_server>("not_running_server");

	i_udp_server::endpoint_info target;
	target.address = "127.0.0.1";
	target.port = test_port_;

	std::vector<uint8_t> data = {0x01};

	// send_to before start should fail
	auto send_result = server->send_to(target, std::move(data), nullptr);
	EXPECT_FALSE(send_result.is_ok());
}

TEST_F(UdpServerInterfaceTest, InterfaceSetReceiveCallback)
{
	auto server = std::make_shared<messaging_udp_server>("recv_callback_server");

	std::atomic<bool> callback_set{false};

	// Use interface callback version
	server->set_receive_callback([&callback_set](const std::vector<uint8_t>&, const i_udp_server::endpoint_info&) {
		callback_set.store(true);
	});

	auto start_result = server->start(test_port_);
	ASSERT_TRUE(start_result.is_ok());
	wait_for_ready();

	[[maybe_unused]] auto stop_result = server->stop();
}

TEST_F(UdpServerInterfaceTest, InterfaceSetErrorCallback)
{
	auto server = std::make_shared<messaging_udp_server>("error_callback_server");

	std::atomic<bool> error_callback_set{false};

	// Use interface error callback
	server->set_error_callback([&error_callback_set](std::error_code) { error_callback_set.store(true); });

	auto start_result = server->start(test_port_);
	ASSERT_TRUE(start_result.is_ok());
	wait_for_ready();

	[[maybe_unused]] auto stop_result = server->stop();
}

// ============================================================================
// Lifecycle Manager Integration Tests
// ============================================================================

class UdpLifecycleTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		test_port_ = find_available_port(21000);
		ASSERT_NE(test_port_, 0);
	}

	uint16_t test_port_;
};

TEST_F(UdpLifecycleTest, ClientDuplicateStartReturnsError)
{
	auto client = std::make_shared<messaging_udp_client>("dup_start_client");

	auto result1 = client->start("127.0.0.1", test_port_);
	EXPECT_TRUE(result1.is_ok());
	wait_for_ready();

	// Second start should fail
	auto result2 = client->start("127.0.0.1", test_port_);
	EXPECT_FALSE(result2.is_ok());

	[[maybe_unused]] auto stop_result = client->stop();
}

TEST_F(UdpLifecycleTest, ServerDuplicateStartReturnsError)
{
	auto server = std::make_shared<messaging_udp_server>("dup_start_server");

	auto result1 = server->start(test_port_);
	EXPECT_TRUE(result1.is_ok());
	wait_for_ready();

	// Second start should fail
	auto result2 = server->start(test_port_);
	EXPECT_FALSE(result2.is_ok());

	[[maybe_unused]] auto stop_result = server->stop();
}

TEST_F(UdpLifecycleTest, ClientEmptyHostReturnsError)
{
	auto client = std::make_shared<messaging_udp_client>("empty_host_client");

	auto result = client->start_client("", test_port_);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(UdpLifecycleTest, ClientStopWhenNotRunningSucceeds)
{
	auto client = std::make_shared<messaging_udp_client>("not_running_client");

	// Stop when not running should succeed (no-op)
	auto result = client->stop();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(UdpLifecycleTest, ServerStopWhenNotRunningSucceeds)
{
	auto server = std::make_shared<messaging_udp_server>("not_running_server");

	// Stop when not running should succeed (no-op)
	auto result = server->stop();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Send Error Condition Tests
// ============================================================================

class UdpSendErrorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		test_port_ = find_available_port(22000);
		ASSERT_NE(test_port_, 0);
	}

	uint16_t test_port_;
};

TEST_F(UdpSendErrorTest, ClientSendBeforeStartFails)
{
	auto client = std::make_shared<messaging_udp_client>("send_before_start");

	std::vector<uint8_t> data = {0x01, 0x02};
	auto result = client->send(std::move(data), nullptr);
	EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Type Compatibility Tests
// ============================================================================

class UdpTypeCompatibilityTest : public ::testing::Test
{
};

TEST_F(UdpTypeCompatibilityTest, ClientImplementsInterface)
{
	auto client = std::make_shared<messaging_udp_client>("interface_impl");

	// Should be assignable to interface pointer
	i_udp_client* interface_ptr = client.get();
	EXPECT_NE(interface_ptr, nullptr);

	// is_running() is protected in the interface, but public in the concrete class
	EXPECT_FALSE(client->is_running());
}

TEST_F(UdpTypeCompatibilityTest, ServerImplementsInterface)
{
	auto server = std::make_shared<messaging_udp_server>("interface_impl");

	// Should be assignable to interface pointer
	i_udp_server* interface_ptr = server.get();
	EXPECT_NE(interface_ptr, nullptr);

	// is_running() is protected in the interface, but public in the concrete class
	EXPECT_FALSE(server->is_running());
}

TEST_F(UdpTypeCompatibilityTest, ClientImplementsNetworkComponent)
{
	auto client = std::make_shared<messaging_udp_client>("network_component");

	// Should also implement i_network_component
	i_network_component* component_ptr = client.get();
	EXPECT_NE(component_ptr, nullptr);
}

TEST_F(UdpTypeCompatibilityTest, ServerImplementsNetworkComponent)
{
	auto server = std::make_shared<messaging_udp_server>("network_component");

	// Should also implement i_network_component
	i_network_component* component_ptr = server.get();
	EXPECT_NE(component_ptr, nullptr);
}

// ============================================================================
// Legacy API Backward Compatibility Tests
// ============================================================================

class UdpLegacyApiTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		test_port_ = find_available_port(23000);
		ASSERT_NE(test_port_, 0);
	}

	uint16_t test_port_;
};

TEST_F(UdpLegacyApiTest, ClientLegacyStartStop)
{
	auto client = std::make_shared<messaging_udp_client>("legacy_client");

	// Use legacy API
	auto start_result = client->start_client("127.0.0.1", test_port_);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(client->is_running());
	wait_for_ready();

	auto stop_result = client->stop_client();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(client->is_running());
}

TEST_F(UdpLegacyApiTest, ServerLegacyStartStop)
{
	auto server = std::make_shared<messaging_udp_server>("legacy_server");

	// Use legacy API
	auto start_result = server->start_server(test_port_);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(server->is_running());
	wait_for_ready();

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

TEST_F(UdpLegacyApiTest, ClientLegacyReceiveCallback)
{
	auto client = std::make_shared<messaging_udp_client>("legacy_callback_client");

	std::atomic<bool> callback_set{false};

	// Use legacy receive callback (asio endpoint version)
	client->set_receive_callback([&callback_set](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {
		callback_set.store(true);
	});

	auto result = client->start_client("127.0.0.1", test_port_);
	EXPECT_TRUE(result.is_ok());
	wait_for_ready();

	[[maybe_unused]] auto stop_result = client->stop_client();
}

TEST_F(UdpLegacyApiTest, ServerLegacyReceiveCallback)
{
	auto server = std::make_shared<messaging_udp_server>("legacy_callback_server");

	std::atomic<bool> callback_set{false};

	// Use legacy receive callback (asio endpoint version)
	server->set_receive_callback([&callback_set](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {
		callback_set.store(true);
	});

	auto result = server->start_server(test_port_);
	EXPECT_TRUE(result.is_ok());
	wait_for_ready();

	[[maybe_unused]] auto stop_result = server->stop_server();
}
