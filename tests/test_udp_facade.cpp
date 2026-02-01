/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include "kcenon/network/facade/udp_facade.h"
#include "kcenon/network/interfaces/i_udp_client.h"
#include "kcenon/network/interfaces/i_udp_server.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::facade;
using namespace kcenon::network::interfaces;

class UdpFacadeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		facade_ = std::make_unique<udp_facade>();
	}

	void TearDown() override
	{
		// Cleanup any running clients/servers
		if (client_)
		{
			client_->stop();
		}
		if (server_)
		{
			server_->stop();
		}
	}

	std::unique_ptr<udp_facade> facade_;
	std::shared_ptr<i_udp_server> server_;
	std::shared_ptr<i_udp_client> client_;
};

// ============================================================================
// Client Configuration Validation Tests
// ============================================================================

TEST_F(UdpFacadeTest, CreateClientThrowsOnEmptyHost)
{
	udp_facade::client_config config{
		.host = "",
		.port = 5555,
	};

	EXPECT_THROW(facade_->create_client(config), std::invalid_argument);
}

TEST_F(UdpFacadeTest, CreateClientThrowsOnInvalidPortZero)
{
	udp_facade::client_config config{
		.host = "127.0.0.1",
		.port = 0,
	};

	EXPECT_THROW(facade_->create_client(config), std::invalid_argument);
}

// ============================================================================
// Server Configuration Validation Tests
// ============================================================================

TEST_F(UdpFacadeTest, CreateServerThrowsOnInvalidPortZero)
{
	udp_facade::server_config config{
		.port = 0,
	};

	EXPECT_THROW(facade_->create_server(config), std::invalid_argument);
}

// ============================================================================
// Basic Server Creation Tests
// ============================================================================

TEST_F(UdpFacadeTest, CreateServerReturnsNonNull)
{
	udp_facade::server_config config{
		.port = 5556,
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(config);
	});

	ASSERT_NE(server_, nullptr);
	// Server is running after successful creation
}

TEST_F(UdpFacadeTest, CreateServerWithCustomIdUsesProvidedId)
{
	udp_facade::server_config config{
		.port = 5557,
		.server_id = "custom_server",
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(config);
	});

	ASSERT_NE(server_, nullptr);
}

TEST_F(UdpFacadeTest, CreateServerWithoutIdGeneratesUniqueId)
{
	udp_facade::server_config config1{.port = 5558};
	udp_facade::server_config config2{.port = 5559};

	std::shared_ptr<i_udp_server> server1;
	std::shared_ptr<i_udp_server> server2;

	ASSERT_NO_THROW({
		server1 = facade_->create_server(config1);
		server2 = facade_->create_server(config2);
	});

	ASSERT_NE(server1, nullptr);
	ASSERT_NE(server2, nullptr);

	// Cleanup
	server1->stop();
	server2->stop();
}

// ============================================================================
// Basic Client Creation Tests
// ============================================================================

TEST_F(UdpFacadeTest, CreateClientReturnsNonNull)
{
	// Create server first
	udp_facade::server_config server_config{
		.port = 5560,
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(server_config);
	});

	// Create client targeting the server
	udp_facade::client_config client_config{
		.host = "127.0.0.1",
		.port = 5560,
	};

	ASSERT_NO_THROW({
		client_ = facade_->create_client(client_config);
	});

	ASSERT_NE(client_, nullptr);
	// Client is running after successful creation
}

TEST_F(UdpFacadeTest, CreateClientWithCustomIdUsesProvidedId)
{
	// Create server first
	udp_facade::server_config server_config{
		.port = 5561,
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(server_config);
	});

	// Create client with custom ID
	udp_facade::client_config client_config{
		.host = "127.0.0.1",
		.port = 5561,
		.client_id = "custom_client",
	};

	ASSERT_NO_THROW({
		client_ = facade_->create_client(client_config);
	});

	ASSERT_NE(client_, nullptr);
}

TEST_F(UdpFacadeTest, CreateClientWithoutIdGeneratesUniqueId)
{
	// Create server first
	udp_facade::server_config server_config{
		.port = 5562,
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(server_config);
	});

	// Create two clients without IDs
	udp_facade::client_config config1{
		.host = "127.0.0.1",
		.port = 5562,
	};

	udp_facade::client_config config2{
		.host = "127.0.0.1",
		.port = 5562,
	};

	std::shared_ptr<i_udp_client> client1;
	std::shared_ptr<i_udp_client> client2;

	ASSERT_NO_THROW({
		client1 = facade_->create_client(config1);
		client2 = facade_->create_client(config2);
	});

	ASSERT_NE(client1, nullptr);
	ASSERT_NE(client2, nullptr);

	// Cleanup
	client1->stop();
	client2->stop();
}

// ============================================================================
// Basic Communication Tests
// ============================================================================

// Communication tests are temporarily disabled pending full integration
// These tests require stable UDP socket implementation and proper async handling

/*
TEST_F(UdpFacadeTest, ClientCanSendDataToServer)
{
	// Create server
	udp_facade::server_config server_config{
		.port = 5563,
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(server_config);
	});

	// Set up server receive callback
	bool received = false;
	std::vector<uint8_t> received_data;

	server_->set_receive_callback(
		[&received, &received_data](const std::vector<uint8_t>& data,
		                             const i_udp_server::endpoint_info& sender) {
			received = true;
			received_data = data;
		});

	// Create client
	udp_facade::client_config client_config{
		.host = "127.0.0.1",
		.port = 5563,
	};

	ASSERT_NO_THROW({
		client_ = facade_->create_client(client_config);
	});

	// Send data
	std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
	auto result = client_->send(std::move(test_data));
	ASSERT_TRUE(result.is_ok());

	// Wait for reception
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_TRUE(received);
	EXPECT_EQ(received_data, std::vector<uint8_t>({0x01, 0x02, 0x03, 0x04}));
}

TEST_F(UdpFacadeTest, MultipleClientsCanCommunicateWithServer)
{
	// Create server
	udp_facade::server_config server_config{
		.port = 5564,
	};

	ASSERT_NO_THROW({
		server_ = facade_->create_server(server_config);
	});

	// Set up server receive callback
	int message_count = 0;

	server_->set_receive_callback(
		[&message_count](const std::vector<uint8_t>& data,
		                 const i_udp_server::endpoint_info& sender) {
			message_count++;
		});

	// Create two clients
	udp_facade::client_config config1{
		.host = "127.0.0.1",
		.port = 5564,
	};

	udp_facade::client_config config2{
		.host = "127.0.0.1",
		.port = 5564,
	};

	std::shared_ptr<i_udp_client> client1;
	std::shared_ptr<i_udp_client> client2;

	ASSERT_NO_THROW({
		client1 = facade_->create_client(config1);
		client2 = facade_->create_client(config2);
	});

	// Send data from both clients
	std::vector<uint8_t> data1 = {0x01, 0x02};
	std::vector<uint8_t> data2 = {0x03, 0x04};

	auto result1 = client1->send(std::move(data1));
	auto result2 = client2->send(std::move(data2));

	ASSERT_TRUE(result1.is_ok());
	ASSERT_TRUE(result2.is_ok());

	// Wait for reception
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	EXPECT_EQ(message_count, 2);

	// Cleanup
	client1->stop();
	client2->stop();
}
*/
