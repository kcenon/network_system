/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file core_client_test.cpp
 * @brief Unit tests for gRPC client configuration, call_options, and
 *        grpc_client construction
 *
 * Tests validate:
 * - grpc_channel_config default values
 * - grpc_channel_config custom values
 * - call_options default values and set_timeout
 * - grpc_client construction with target address
 * - grpc_client initial state (not connected)
 * - grpc_client target() accessor
 * - grpc_client move semantics
 */

#include "kcenon/network/detail/protocols/grpc/client.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

using namespace kcenon::network::protocols::grpc;

// ============================================================================
// grpc_channel_config Tests
// ============================================================================

TEST(GrpcChannelConfigTest, DefaultValues)
{
	grpc_channel_config config;

	EXPECT_EQ(config.default_timeout, std::chrono::milliseconds{30000});
	EXPECT_TRUE(config.use_tls);
	EXPECT_TRUE(config.root_certificates.empty());
	EXPECT_FALSE(config.client_certificate.has_value());
	EXPECT_FALSE(config.client_key.has_value());
	EXPECT_EQ(config.max_message_size, default_max_message_size);
	EXPECT_EQ(config.keepalive_time, std::chrono::milliseconds{0});
	EXPECT_EQ(config.keepalive_timeout, std::chrono::milliseconds{20000});
	EXPECT_EQ(config.max_retry_attempts, 3u);
}

TEST(GrpcChannelConfigTest, CustomValues)
{
	grpc_channel_config config;
	config.default_timeout = std::chrono::milliseconds{5000};
	config.use_tls = false;
	config.root_certificates = "-----BEGIN CERTIFICATE-----";
	config.client_certificate = "client-cert";
	config.client_key = "client-key";
	config.max_message_size = 1024;
	config.keepalive_time = std::chrono::milliseconds{60000};
	config.keepalive_timeout = std::chrono::milliseconds{10000};
	config.max_retry_attempts = 5;

	EXPECT_EQ(config.default_timeout, std::chrono::milliseconds{5000});
	EXPECT_FALSE(config.use_tls);
	EXPECT_EQ(config.root_certificates, "-----BEGIN CERTIFICATE-----");
	EXPECT_TRUE(config.client_certificate.has_value());
	EXPECT_EQ(config.client_certificate.value(), "client-cert");
	EXPECT_TRUE(config.client_key.has_value());
	EXPECT_EQ(config.client_key.value(), "client-key");
	EXPECT_EQ(config.max_message_size, 1024u);
	EXPECT_EQ(config.keepalive_time, std::chrono::milliseconds{60000});
	EXPECT_EQ(config.keepalive_timeout, std::chrono::milliseconds{10000});
	EXPECT_EQ(config.max_retry_attempts, 5u);
}

// ============================================================================
// call_options Tests
// ============================================================================

TEST(GrpcCallOptionsTest, DefaultValues)
{
	call_options options;

	EXPECT_FALSE(options.deadline.has_value());
	EXPECT_TRUE(options.metadata.empty());
	EXPECT_FALSE(options.wait_for_ready);
	EXPECT_TRUE(options.compression_algorithm.empty());
}

TEST(GrpcCallOptionsTest, SetTimeout)
{
	call_options options;
	auto before = std::chrono::system_clock::now();
	options.set_timeout(std::chrono::seconds{5});
	auto after = std::chrono::system_clock::now();

	ASSERT_TRUE(options.deadline.has_value());
	EXPECT_GE(options.deadline.value(), before + std::chrono::seconds{5});
	EXPECT_LE(options.deadline.value(), after + std::chrono::seconds{5});
}

TEST(GrpcCallOptionsTest, MetadataAssignment)
{
	call_options options;
	options.metadata.push_back({"authorization", "Bearer token"});
	options.metadata.push_back({"x-request-id", "abc-123"});

	EXPECT_EQ(options.metadata.size(), 2u);
	EXPECT_EQ(options.metadata[0].first, "authorization");
	EXPECT_EQ(options.metadata[0].second, "Bearer token");
	EXPECT_EQ(options.metadata[1].first, "x-request-id");
	EXPECT_EQ(options.metadata[1].second, "abc-123");
}

TEST(GrpcCallOptionsTest, WaitForReadyFlag)
{
	call_options options;
	options.wait_for_ready = true;

	EXPECT_TRUE(options.wait_for_ready);
}

// ============================================================================
// grpc_client Construction Tests
// ============================================================================

TEST(GrpcClientTest, ConstructWithTarget)
{
	grpc_client client("localhost:50051");

	EXPECT_EQ(client.target(), "localhost:50051");
	EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientTest, ConstructWithCustomConfig)
{
	grpc_channel_config config;
	config.use_tls = false;
	config.default_timeout = std::chrono::milliseconds{10000};

	grpc_client client("192.168.1.100:9090", config);

	EXPECT_EQ(client.target(), "192.168.1.100:9090");
	EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientTest, MoveConstruct)
{
	grpc_client original("localhost:50051");
	EXPECT_EQ(original.target(), "localhost:50051");

	grpc_client moved(std::move(original));
	EXPECT_EQ(moved.target(), "localhost:50051");
	EXPECT_FALSE(moved.is_connected());
}

TEST(GrpcClientTest, MoveAssign)
{
	grpc_client client1("host1:1234");
	grpc_client client2("host2:5678");

	client2 = std::move(client1);
	EXPECT_EQ(client2.target(), "host1:1234");
	EXPECT_FALSE(client2.is_connected());
}

TEST(GrpcClientTest, DisconnectWhenNotConnected)
{
	grpc_client client("localhost:50051");
	EXPECT_NO_FATAL_FAILURE(client.disconnect());
}

// ============================================================================
// grpc_metadata Type Tests
// ============================================================================

TEST(GrpcMetadataTest, EmptyMetadata)
{
	grpc_metadata metadata;
	EXPECT_TRUE(metadata.empty());
}

TEST(GrpcMetadataTest, AddEntries)
{
	grpc_metadata metadata;
	metadata.emplace_back("key1", "value1");
	metadata.emplace_back("key2", "value2");

	EXPECT_EQ(metadata.size(), 2u);
}
