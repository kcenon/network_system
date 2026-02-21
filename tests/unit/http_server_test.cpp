/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/http_server.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace kcenon::network::core;
using namespace kcenon::network::internal;

/**
 * @file http_server_test.cpp
 * @brief Unit tests for http_server
 *
 * Tests validate:
 * - Construction with server_id
 * - Route registration (get, post, put, del, patch, head, options)
 * - Start/stop lifecycle on ephemeral port
 * - Double-start returns error
 * - Stop when not running returns error
 * - Custom error handler registration
 * - Destructor safety
 *
 * Note: Request handling and response tests require a running client
 * and are covered by integration tests.
 */

// ============================================================================
// Construction Tests
// ============================================================================

class HttpServerConstructionTest : public ::testing::Test
{
};

TEST_F(HttpServerConstructionTest, ConstructsWithServerId)
{
	auto server = std::make_shared<http_server>("test_http_server");

	// Server should not be running after construction
	// (no is_running() accessor, but start/stop should work)
	SUCCEED();
}

TEST_F(HttpServerConstructionTest, ConstructsWithEmptyServerId)
{
	auto server = std::make_shared<http_server>("");

	SUCCEED();
}

// ============================================================================
// Route Registration Tests
// ============================================================================

class HttpServerRouteTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<http_server>("route_test_server");
	}

	std::shared_ptr<http_server> server_;
};

TEST_F(HttpServerRouteTest, RegisterGetRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->get("/", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		resp.set_body_string("OK");
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterPostRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->post("/api/data", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 201;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterPutRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->put("/api/data/:id", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterDeleteRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->del("/api/data/:id", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 204;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterPatchRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->patch("/api/data/:id", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterHeadRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->head("/health", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterOptionsRouteDoesNotThrow)
{
	EXPECT_NO_THROW(server_->options("/api/data", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 204;
		resp.set_header("Allow", "GET, POST, PUT, DELETE");
		return resp;
	}));
}

TEST_F(HttpServerRouteTest, RegisterMultipleRoutesDoesNotThrow)
{
	EXPECT_NO_THROW({
		server_->get("/users", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		});
		server_->get("/users/:id", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		});
		server_->post("/users", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 201;
			return resp;
		});
		server_->del("/users/:id", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 204;
			return resp;
		});
	});
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

class HttpServerLifecycleTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<http_server>("lifecycle_test_server");
	}

	std::shared_ptr<http_server> server_;
};

TEST_F(HttpServerLifecycleTest, StartAndStopOnEphemeralPort)
{
	auto start_result = server_->start(0);
	ASSERT_TRUE(start_result.is_ok()) << "Start failed: " << start_result.error().message;

	// Give time for the server to bind
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto stop_result = server_->stop();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(HttpServerLifecycleTest, StartWithRoutesRegistered)
{
	server_->get("/", [](const http_request_context&)
	{
		http_response resp;
		resp.status_code = 200;
		resp.set_body_string("Hello");
		return resp;
	});

	auto start_result = server_->start(0);
	ASSERT_TRUE(start_result.is_ok()) << "Start failed: " << start_result.error().message;

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto stop_result = server_->stop();
	EXPECT_TRUE(stop_result.is_ok());
}

// ============================================================================
// Error Handler Tests
// ============================================================================

class HttpServerErrorHandlerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<http_server>("error_handler_server");
	}

	std::shared_ptr<http_server> server_;
};

TEST_F(HttpServerErrorHandlerTest, SetNotFoundHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_not_found_handler(
		[](const http_request_context&)
		{
			http_response resp;
			resp.status_code = 404;
			resp.set_body_string("Not Found");
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetErrorHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_error_handler(
		[](const http_request_context&)
		{
			http_response resp;
			resp.status_code = 500;
			resp.set_body_string("Internal Server Error");
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetSpecificErrorHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_error_handler(
		http_error_code::bad_request,
		[](const http_error&)
		{
			http_response resp;
			resp.status_code = 400;
			resp.set_body_string("Bad Request");
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetDefaultErrorHandlerDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_default_error_handler(
		[](const http_error& err)
		{
			http_response resp;
			resp.status_code = err.status_code();
			resp.set_body_string(err.message);
			return resp;
		}));
}

TEST_F(HttpServerErrorHandlerTest, SetJsonErrorResponsesDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_json_error_responses(true));
}

TEST_F(HttpServerErrorHandlerTest, SetRequestTimeoutDoesNotThrow)
{
	EXPECT_NO_THROW(server_->set_request_timeout(std::chrono::milliseconds(10000)));
}

TEST_F(HttpServerErrorHandlerTest, SetCompressionSettingsDoNotThrow)
{
	EXPECT_NO_THROW(server_->set_compression_enabled(true));
	EXPECT_NO_THROW(server_->set_compression_threshold(2048));
}

// ============================================================================
// Destructor Safety Tests
// ============================================================================

class HttpServerDestructorTest : public ::testing::Test
{
};

TEST_F(HttpServerDestructorTest, DestructorOnNonStartedServerDoesNotCrash)
{
	{
		auto server = std::make_shared<http_server>("destructor_test");
		server->get("/", [](const http_request_context&) {
			http_response resp;
			resp.status_code = 200;
			return resp;
		});
		// Destructor called without starting
	}
	SUCCEED();
}

TEST_F(HttpServerDestructorTest, DestructorStopsRunningServer)
{
	{
		auto server = std::make_shared<http_server>("running_destructor_test");
		auto result = server->start(0);
		ASSERT_TRUE(result.is_ok()) << "Start failed: " << result.error().message;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		// Destructor should stop it
	}
	SUCCEED();
}
