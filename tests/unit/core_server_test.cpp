/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file core_server_test.cpp
 * @brief Unit tests for gRPC server configuration, grpc_status, and
 *        grpc_server construction
 *
 * Tests validate:
 * - grpc_server_config default values
 * - grpc_server_config custom values
 * - grpc_status construction and accessors
 * - grpc_status static factories (ok_status, error_status)
 * - grpc_trailers conversion
 * - grpc_server construction and initial state
 * - grpc_server move semantics
 */

#include "kcenon/network/detail/protocols/grpc/server.h"
#include "kcenon/network/detail/protocols/grpc/status.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

using namespace kcenon::network::protocols::grpc;

// ============================================================================
// grpc_server_config Tests
// ============================================================================

TEST(GrpcServerConfigTest, DefaultValues)
{
	grpc_server_config config;

	EXPECT_EQ(config.max_concurrent_streams, 100u);
	EXPECT_EQ(config.max_message_size, default_max_message_size);
	EXPECT_EQ(config.keepalive_time, std::chrono::milliseconds{7200000});
	EXPECT_EQ(config.keepalive_timeout, std::chrono::milliseconds{20000});
	EXPECT_EQ(config.max_connection_idle, std::chrono::milliseconds{0});
	EXPECT_EQ(config.max_connection_age, std::chrono::milliseconds{0});
	EXPECT_EQ(config.num_threads, 0u);
}

TEST(GrpcServerConfigTest, CustomValues)
{
	grpc_server_config config;
	config.max_concurrent_streams = 50;
	config.max_message_size = 1024 * 1024;
	config.keepalive_time = std::chrono::milliseconds{60000};
	config.keepalive_timeout = std::chrono::milliseconds{5000};
	config.max_connection_idle = std::chrono::milliseconds{300000};
	config.max_connection_age = std::chrono::milliseconds{3600000};
	config.num_threads = 4;

	EXPECT_EQ(config.max_concurrent_streams, 50u);
	EXPECT_EQ(config.max_message_size, 1024u * 1024);
	EXPECT_EQ(config.keepalive_time, std::chrono::milliseconds{60000});
	EXPECT_EQ(config.keepalive_timeout, std::chrono::milliseconds{5000});
	EXPECT_EQ(config.max_connection_idle, std::chrono::milliseconds{300000});
	EXPECT_EQ(config.max_connection_age, std::chrono::milliseconds{3600000});
	EXPECT_EQ(config.num_threads, 4u);
}

// ============================================================================
// grpc_status Tests
// ============================================================================

TEST(GrpcStatusTest, DefaultConstructionIsOk)
{
	grpc_status status;

	EXPECT_TRUE(status.is_ok());
	EXPECT_FALSE(status.is_error());
	EXPECT_EQ(status.code, status_code::ok);
	EXPECT_TRUE(status.message.empty());
	EXPECT_FALSE(status.details.has_value());
}

TEST(GrpcStatusTest, ConstructWithCodeOnly)
{
	grpc_status status(status_code::not_found);

	EXPECT_FALSE(status.is_ok());
	EXPECT_TRUE(status.is_error());
	EXPECT_EQ(status.code, status_code::not_found);
	EXPECT_TRUE(status.message.empty());
}

TEST(GrpcStatusTest, ConstructWithCodeAndMessage)
{
	grpc_status status(status_code::internal, "server error");

	EXPECT_TRUE(status.is_error());
	EXPECT_EQ(status.code, status_code::internal);
	EXPECT_EQ(status.message, "server error");
	EXPECT_FALSE(status.details.has_value());
}

TEST(GrpcStatusTest, ConstructWithCodeMessageAndDetails)
{
	grpc_status status(status_code::invalid_argument, "bad request", "field X");

	EXPECT_TRUE(status.is_error());
	EXPECT_EQ(status.code, status_code::invalid_argument);
	EXPECT_EQ(status.message, "bad request");
	ASSERT_TRUE(status.details.has_value());
	EXPECT_EQ(status.details.value(), "field X");
}

TEST(GrpcStatusTest, OkStatusFactory)
{
	auto status = grpc_status::ok_status();

	EXPECT_TRUE(status.is_ok());
	EXPECT_EQ(status.code, status_code::ok);
}

TEST(GrpcStatusTest, ErrorStatusFactory)
{
	auto status = grpc_status::error_status(status_code::unavailable, "service down");

	EXPECT_TRUE(status.is_error());
	EXPECT_EQ(status.code, status_code::unavailable);
	EXPECT_EQ(status.message, "service down");
}

TEST(GrpcStatusTest, CodeString)
{
	EXPECT_EQ(grpc_status(status_code::ok).code_string(), "OK");
	EXPECT_EQ(grpc_status(status_code::cancelled).code_string(), "CANCELLED");
	EXPECT_EQ(grpc_status(status_code::unknown).code_string(), "UNKNOWN");
	EXPECT_EQ(grpc_status(status_code::invalid_argument).code_string(), "INVALID_ARGUMENT");
	EXPECT_EQ(grpc_status(status_code::deadline_exceeded).code_string(), "DEADLINE_EXCEEDED");
	EXPECT_EQ(grpc_status(status_code::not_found).code_string(), "NOT_FOUND");
	EXPECT_EQ(grpc_status(status_code::already_exists).code_string(), "ALREADY_EXISTS");
	EXPECT_EQ(grpc_status(status_code::permission_denied).code_string(), "PERMISSION_DENIED");
	EXPECT_EQ(grpc_status(status_code::resource_exhausted).code_string(), "RESOURCE_EXHAUSTED");
	EXPECT_EQ(grpc_status(status_code::failed_precondition).code_string(), "FAILED_PRECONDITION");
	EXPECT_EQ(grpc_status(status_code::aborted).code_string(), "ABORTED");
	EXPECT_EQ(grpc_status(status_code::out_of_range).code_string(), "OUT_OF_RANGE");
	EXPECT_EQ(grpc_status(status_code::unimplemented).code_string(), "UNIMPLEMENTED");
	EXPECT_EQ(grpc_status(status_code::internal).code_string(), "INTERNAL");
	EXPECT_EQ(grpc_status(status_code::unavailable).code_string(), "UNAVAILABLE");
	EXPECT_EQ(grpc_status(status_code::data_loss).code_string(), "DATA_LOSS");
	EXPECT_EQ(grpc_status(status_code::unauthenticated).code_string(), "UNAUTHENTICATED");
}

// ============================================================================
// grpc_trailers Tests
// ============================================================================

TEST(GrpcTrailersTest, DefaultValues)
{
	grpc_trailers trailers;

	EXPECT_EQ(trailers.status, status_code::ok);
	EXPECT_TRUE(trailers.status_message.empty());
	EXPECT_FALSE(trailers.status_details.has_value());
}

TEST(GrpcTrailersTest, ToStatusWithoutDetails)
{
	grpc_trailers trailers;
	trailers.status = status_code::not_found;
	trailers.status_message = "not found";

	auto status = trailers.to_status();

	EXPECT_EQ(status.code, status_code::not_found);
	EXPECT_EQ(status.message, "not found");
	EXPECT_FALSE(status.details.has_value());
}

TEST(GrpcTrailersTest, ToStatusWithDetails)
{
	grpc_trailers trailers;
	trailers.status = status_code::internal;
	trailers.status_message = "internal error";
	trailers.status_details = "stack trace";

	auto status = trailers.to_status();

	EXPECT_EQ(status.code, status_code::internal);
	EXPECT_EQ(status.message, "internal error");
	ASSERT_TRUE(status.details.has_value());
	EXPECT_EQ(status.details.value(), "stack trace");
}

// ============================================================================
// status_code_to_string Tests
// ============================================================================

TEST(StatusCodeToStringTest, AllCodes)
{
	EXPECT_EQ(status_code_to_string(status_code::ok), "OK");
	EXPECT_EQ(status_code_to_string(status_code::unauthenticated), "UNAUTHENTICATED");
}

// ============================================================================
// grpc_server Construction Tests
// ============================================================================

TEST(GrpcServerTest, DefaultConstruction)
{
	grpc_server server;

	EXPECT_FALSE(server.is_running());
	EXPECT_EQ(server.port(), 0u);
}

TEST(GrpcServerTest, ConstructWithConfig)
{
	grpc_server_config config;
	config.max_concurrent_streams = 200;
	config.num_threads = 8;

	grpc_server server(config);

	EXPECT_FALSE(server.is_running());
	EXPECT_EQ(server.port(), 0u);
}

TEST(GrpcServerTest, MoveConstruct)
{
	grpc_server original;
	EXPECT_FALSE(original.is_running());

	grpc_server moved(std::move(original));
	EXPECT_FALSE(moved.is_running());
}

TEST(GrpcServerTest, MoveAssign)
{
	grpc_server server1;
	grpc_server server2;

	server2 = std::move(server1);
	EXPECT_FALSE(server2.is_running());
}

TEST(GrpcServerTest, StopWhenNotRunning)
{
	grpc_server server;
	EXPECT_NO_FATAL_FAILURE(server.stop());
}
