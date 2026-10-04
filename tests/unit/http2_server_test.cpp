/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/http2/http2_server.h"
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

namespace http2 = kcenon::network::protocols::http2;

/**
 * @file http2_server_test.cpp
 * @brief Unit tests for http2_server
 *
 * Tests validate:
 * - tls_config struct default values
 * - http2_server construction and initial state
 * - is_running() after construction
 * - server_id() accessor
 * - set_settings() / get_settings()
 * - set_request_handler() and set_error_handler()
 * - active_connections() and active_streams() initial values
 * - stop() while not running
 */

// ============================================================================
// tls_config Tests
// ============================================================================

class TlsConfigTest : public ::testing::Test
{
};

TEST_F(TlsConfigTest, DefaultValues)
{
	http2::tls_config config;
	EXPECT_TRUE(config.cert_file.empty());
	EXPECT_TRUE(config.key_file.empty());
	EXPECT_TRUE(config.ca_file.empty());
	EXPECT_FALSE(config.verify_client);
}

TEST_F(TlsConfigTest, CustomValues)
{
	http2::tls_config config;
	config.cert_file = "/path/to/cert.pem";
	config.key_file = "/path/to/key.pem";
	config.ca_file = "/path/to/ca.pem";
	config.verify_client = true;

	EXPECT_EQ(config.cert_file, "/path/to/cert.pem");
	EXPECT_EQ(config.key_file, "/path/to/key.pem");
	EXPECT_EQ(config.ca_file, "/path/to/ca.pem");
	EXPECT_TRUE(config.verify_client);
}

// ============================================================================
// http2_server Construction and State Tests
// ============================================================================

class Http2ServerTest : public ::testing::Test
{
protected:
	std::shared_ptr<http2::http2_server> server_
		= std::make_shared<http2::http2_server>("test-server");
};

TEST_F(Http2ServerTest, InitiallyNotRunning)
{
	EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTest, ServerId)
{
	EXPECT_EQ(server_->server_id(), "test-server");
}

TEST_F(Http2ServerTest, ActiveConnectionsInitially)
{
	EXPECT_EQ(server_->active_connections(), 0u);
}

TEST_F(Http2ServerTest, ActiveStreamsInitially)
{
	EXPECT_EQ(server_->active_streams(), 0u);
}

TEST_F(Http2ServerTest, GetDefaultSettings)
{
	auto settings = server_->get_settings();
	EXPECT_EQ(settings.header_table_size, 4096u);
	EXPECT_EQ(settings.max_concurrent_streams, 100u);
	EXPECT_EQ(settings.initial_window_size, 65535u);
	EXPECT_EQ(settings.max_frame_size, 16384u);
}

TEST_F(Http2ServerTest, SetSettings)
{
	http2::http2_settings custom;
	custom.max_concurrent_streams = 256;
	custom.max_frame_size = 32768;
	custom.max_header_list_size = 16384;
	server_->set_settings(custom);

	auto retrieved = server_->get_settings();
	EXPECT_EQ(retrieved.max_concurrent_streams, 256u);
	EXPECT_EQ(retrieved.max_frame_size, 32768u);
	EXPECT_EQ(retrieved.max_header_list_size, 16384u);
}

TEST_F(Http2ServerTest, SetRequestHandler)
{
	bool handler_set = false;
	server_->set_request_handler(
		[&handler_set](http2::http2_server_stream&,
					   const http2::http2_request&) {
			handler_set = true;
		});
	// Handler set should not crash; it is only invoked on incoming requests
	EXPECT_FALSE(handler_set);
}

TEST_F(Http2ServerTest, SetErrorHandler)
{
	bool handler_set = false;
	server_->set_error_handler(
		[&handler_set](const std::string&) {
			handler_set = true;
		});
	// Handler set should not crash
	EXPECT_FALSE(handler_set);
}

TEST_F(Http2ServerTest, StopWhileNotRunning)
{
	// Stopping a server that is not running should not crash
	auto result = server_->stop();
	// May succeed or return error; either is acceptable
	(void)result;
}

// ============================================================================
// Multiple Server Instances
// ============================================================================

class Http2ServerMultiInstanceTest : public ::testing::Test
{
};

TEST_F(Http2ServerMultiInstanceTest, DifferentIds)
{
	auto server1 = std::make_shared<http2::http2_server>("server-1");
	auto server2 = std::make_shared<http2::http2_server>("server-2");

	EXPECT_EQ(server1->server_id(), "server-1");
	EXPECT_EQ(server2->server_id(), "server-2");
	EXPECT_NE(server1->server_id(), server2->server_id());
}

TEST_F(Http2ServerMultiInstanceTest, IndependentSettings)
{
	auto server1 = std::make_shared<http2::http2_server>("s1");
	auto server2 = std::make_shared<http2::http2_server>("s2");

	http2::http2_settings custom;
	custom.max_concurrent_streams = 50;
	server1->set_settings(custom);

	EXPECT_EQ(server1->get_settings().max_concurrent_streams, 50u);
	EXPECT_EQ(server2->get_settings().max_concurrent_streams, 100u);
}
