/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/secure_messaging_server.h"
#include "../helpers/dtls_test_helpers.h"
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace kcenon::network::core;
using namespace kcenon::network;

/**
 * @file secure_messaging_server_test.cpp
 * @brief Unit tests for secure_messaging_server
 *
 * Tests validate:
 * - Construction with server_id, cert_file, key_file
 * - server_id() accessor
 * - is_running() state transitions
 * - Callback setters (connection, disconnection, receive, error)
 * - Double-start returns server_already_running error
 * - Construction with invalid cert files throws
 * - stop_server() on non-running server returns error
 *
 * Uses test_certificate_generator to create self-signed certs on the fly.
 */

// ============================================================================
// Test Fixture with Self-Signed Certificates
// ============================================================================

class SecureMessagingServerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Generate self-signed test certificates
		auto cert_pair = kcenon::network::testing::test_certificate_generator::generate("localhost");

		// Write cert and key to temp files using filesystem::temp_directory_path
		auto tmp_dir = std::filesystem::temp_directory_path();
		cert_file_ = (tmp_dir / "test_cert_XXXXXX.crt").string();
		key_file_ = (tmp_dir / "test_key_XXXXXX.key").string();

		{
			std::ofstream cert_out(cert_file_, std::ios::binary);
			cert_out.write(reinterpret_cast<const char*>(cert_pair.certificate_pem.data()),
			               static_cast<std::streamsize>(cert_pair.certificate_pem.size()));
		}
		{
			std::ofstream key_out(key_file_, std::ios::binary);
			key_out.write(reinterpret_cast<const char*>(cert_pair.private_key_pem.data()),
			              static_cast<std::streamsize>(cert_pair.private_key_pem.size()));
		}
	}

	void TearDown() override
	{
		std::remove(cert_file_.c_str());
		std::remove(key_file_.c_str());
	}

	std::string cert_file_;
	std::string key_file_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(SecureMessagingServerTest, ConstructsWithValidCertificates)
{
	auto server = std::make_shared<secure_messaging_server>(
		"test_server", cert_file_, key_file_);

	EXPECT_EQ(server->server_id(), "test_server");
	EXPECT_FALSE(server->is_running());
}

TEST_F(SecureMessagingServerTest, ConstructsWithEmptyServerId)
{
	auto server = std::make_shared<secure_messaging_server>(
		"", cert_file_, key_file_);

	EXPECT_EQ(server->server_id(), "");
	EXPECT_FALSE(server->is_running());
}

TEST_F(SecureMessagingServerTest, ConstructionThrowsWithInvalidCertFile)
{
	EXPECT_THROW(
		std::make_shared<secure_messaging_server>(
			"bad_cert_server", "/nonexistent/cert.crt", key_file_),
		std::exception);
}

TEST_F(SecureMessagingServerTest, ConstructionThrowsWithInvalidKeyFile)
{
	EXPECT_THROW(
		std::make_shared<secure_messaging_server>(
			"bad_key_server", cert_file_, "/nonexistent/key.key"),
		std::exception);
}

// ============================================================================
// State Transition Tests
// ============================================================================

TEST_F(SecureMessagingServerTest, InitialStateIsNotRunning)
{
	auto server = std::make_shared<secure_messaging_server>(
		"state_server", cert_file_, key_file_);

	EXPECT_FALSE(server->is_running());
}

TEST_F(SecureMessagingServerTest, StopWhenNotRunningReturnsError)
{
	auto server = std::make_shared<secure_messaging_server>(
		"stop_test_server", cert_file_, key_file_);

	auto result = server->stop_server();

	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::network_system::server_not_started);
}

TEST_F(SecureMessagingServerTest, StartAndStopLifecycle)
{
	auto server = std::make_shared<secure_messaging_server>(
		"lifecycle_server", cert_file_, key_file_);

	// Start on an ephemeral port
	auto start_result = server->start_server(0);
	ASSERT_TRUE(start_result.is_ok()) << "Failed to start: " << start_result.error().message;
	EXPECT_TRUE(server->is_running());

	// Stop
	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

TEST_F(SecureMessagingServerTest, DoubleStartReturnsAlreadyRunning)
{
	auto server = std::make_shared<secure_messaging_server>(
		"double_start_server", cert_file_, key_file_);

	auto result1 = server->start_server(0);
	ASSERT_TRUE(result1.is_ok());

	auto result2 = server->start_server(0);
	EXPECT_TRUE(result2.is_err());
	EXPECT_EQ(result2.error().code, error_codes::network_system::server_already_running);

	// Cleanup
	(void)server->stop_server();
}

// ============================================================================
// Callback Setter Tests
// ============================================================================

TEST_F(SecureMessagingServerTest, SetConnectionCallbackDoesNotThrow)
{
	auto server = std::make_shared<secure_messaging_server>(
		"cb_server", cert_file_, key_file_);

	EXPECT_NO_THROW(server->set_connection_callback(
		[](std::shared_ptr<kcenon::network::session::secure_session>) {}));
}

TEST_F(SecureMessagingServerTest, SetDisconnectionCallbackDoesNotThrow)
{
	auto server = std::make_shared<secure_messaging_server>(
		"cb_server", cert_file_, key_file_);

	EXPECT_NO_THROW(server->set_disconnection_callback(
		[](const std::string&) {}));
}

TEST_F(SecureMessagingServerTest, SetReceiveCallbackDoesNotThrow)
{
	auto server = std::make_shared<secure_messaging_server>(
		"cb_server", cert_file_, key_file_);

	EXPECT_NO_THROW(server->set_receive_callback(
		[](std::shared_ptr<kcenon::network::session::secure_session>,
		   const std::vector<uint8_t>&) {}));
}

TEST_F(SecureMessagingServerTest, SetErrorCallbackDoesNotThrow)
{
	auto server = std::make_shared<secure_messaging_server>(
		"cb_server", cert_file_, key_file_);

	EXPECT_NO_THROW(server->set_error_callback(
		[](std::shared_ptr<kcenon::network::session::secure_session>,
		   std::error_code) {}));
}

TEST_F(SecureMessagingServerTest, SetNullCallbacksDoNotThrow)
{
	auto server = std::make_shared<secure_messaging_server>(
		"null_cb_server", cert_file_, key_file_);

	EXPECT_NO_THROW(server->set_connection_callback(nullptr));
	EXPECT_NO_THROW(server->set_disconnection_callback(nullptr));
	EXPECT_NO_THROW(server->set_receive_callback(nullptr));
	EXPECT_NO_THROW(server->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Safety Tests
// ============================================================================

TEST_F(SecureMessagingServerTest, DestructorStopsRunningServer)
{
	{
		auto server = std::make_shared<secure_messaging_server>(
			"destructor_server", cert_file_, key_file_);

		auto result = server->start_server(0);
		ASSERT_TRUE(result.is_ok());
		EXPECT_TRUE(server->is_running());

		// Destructor should stop it
	}
	SUCCEED();
}

TEST_F(SecureMessagingServerTest, DestructorOnNonRunningServerDoesNotCrash)
{
	{
		auto server = std::make_shared<secure_messaging_server>(
			"safe_destructor_server", cert_file_, key_file_);
		// Never started - destructor should be safe
	}
	SUCCEED();
}
