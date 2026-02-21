/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/http_client.h"
#include <gtest/gtest.h>

using namespace kcenon::network::core;
using namespace kcenon::network::internal;

/**
 * @file http_client_test.cpp
 * @brief Unit tests for http_client and http_url
 *
 * Tests validate:
 * - http_url::parse() with valid and invalid URLs
 * - http_client construction with default and custom timeouts
 * - Timeout getter and setter
 * - HTTP methods with invalid/unreachable URLs return errors
 * - HTTPS not supported returns error
 *
 * Note: Tests that require a running HTTP server are covered by
 * integration tests. These tests focus on offline behavior.
 */

// ============================================================================
// URL Parsing Tests
// ============================================================================

class HttpUrlParseTest : public ::testing::Test
{
};

TEST_F(HttpUrlParseTest, ParsesSimpleHttpUrl)
{
	auto result = http_url::parse("http://example.com/path");
	ASSERT_TRUE(result.is_ok()) << result.error().message;

	auto& url = result.value();
	EXPECT_EQ(url.scheme, "http");
	EXPECT_EQ(url.host, "example.com");
	EXPECT_EQ(url.port, 80);
	EXPECT_EQ(url.path, "/path");
}

TEST_F(HttpUrlParseTest, ParsesUrlWithPort)
{
	auto result = http_url::parse("http://localhost:8080/api/v1");
	ASSERT_TRUE(result.is_ok()) << result.error().message;

	auto& url = result.value();
	EXPECT_EQ(url.host, "localhost");
	EXPECT_EQ(url.port, 8080);
	EXPECT_EQ(url.path, "/api/v1");
}

TEST_F(HttpUrlParseTest, ParsesUrlWithQueryParams)
{
	auto result = http_url::parse("http://example.com/search?q=test&page=1");
	ASSERT_TRUE(result.is_ok()) << result.error().message;

	auto& url = result.value();
	EXPECT_EQ(url.path, "/search");
	EXPECT_EQ(url.query["q"], "test");
	EXPECT_EQ(url.query["page"], "1");
}

TEST_F(HttpUrlParseTest, ParsesHttpsUrlWithDefaultPort)
{
	auto result = http_url::parse("https://secure.example.com/api");
	ASSERT_TRUE(result.is_ok()) << result.error().message;

	auto& url = result.value();
	EXPECT_EQ(url.scheme, "https");
	EXPECT_EQ(url.port, 443);
}

TEST_F(HttpUrlParseTest, ParsesUrlWithNoPath)
{
	auto result = http_url::parse("http://example.com");
	ASSERT_TRUE(result.is_ok()) << result.error().message;

	auto& url = result.value();
	EXPECT_EQ(url.path, "/");
}

TEST_F(HttpUrlParseTest, FailsOnInvalidUrl)
{
	auto result = http_url::parse("not-a-url");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpUrlParseTest, FailsOnEmptyUrl)
{
	auto result = http_url::parse("");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpUrlParseTest, FailsOnFtpScheme)
{
	auto result = http_url::parse("ftp://files.example.com/file.txt");
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// HTTP Client Construction Tests
// ============================================================================

class HttpClientConstructionTest : public ::testing::Test
{
};

TEST_F(HttpClientConstructionTest, ConstructsWithDefaultTimeout)
{
	auto client = std::make_shared<http_client>();

	EXPECT_EQ(client->get_timeout(), std::chrono::milliseconds(30000));
}

TEST_F(HttpClientConstructionTest, ConstructsWithCustomTimeout)
{
	auto client = std::make_shared<http_client>(std::chrono::milliseconds(5000));

	EXPECT_EQ(client->get_timeout(), std::chrono::milliseconds(5000));
}

TEST_F(HttpClientConstructionTest, SetTimeoutChangesValue)
{
	auto client = std::make_shared<http_client>();

	client->set_timeout(std::chrono::milliseconds(10000));
	EXPECT_EQ(client->get_timeout(), std::chrono::milliseconds(10000));
}

// ============================================================================
// HTTP Request Error Tests
// ============================================================================

class HttpClientRequestErrorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Use a short timeout to avoid long waits on connection failures
		client_ = std::make_shared<http_client>(std::chrono::milliseconds(1000));
	}

	std::shared_ptr<http_client> client_;
};

TEST_F(HttpClientRequestErrorTest, GetWithInvalidUrlReturnsError)
{
	auto result = client_->get("not-a-valid-url");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, PostWithInvalidUrlReturnsError)
{
	auto result = client_->post("invalid-url", "body");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, PutWithInvalidUrlReturnsError)
{
	auto result = client_->put("invalid-url", "body");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, DeleteWithInvalidUrlReturnsError)
{
	auto result = client_->del("invalid-url");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, HeadWithInvalidUrlReturnsError)
{
	auto result = client_->head("invalid-url");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, PatchWithInvalidUrlReturnsError)
{
	auto result = client_->patch("invalid-url", "body");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, HttpsNotSupportedReturnsError)
{
	auto result = client_->get("https://example.com/api");
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpClientRequestErrorTest, GetWithUnreachableHostReturnsError)
{
	// Connect to port 1 on loopback - should fail quickly
	auto result = client_->get("http://127.0.0.1:1/unreachable");
	EXPECT_TRUE(result.is_err());
}
