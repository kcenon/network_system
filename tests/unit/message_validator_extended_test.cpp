/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file message_validator_extended_test.cpp
 * @brief Extended unit tests for message_validator and rate_limiter
 *
 * Tests validate additional coverage paths:
 * - validation_result to_string() for all enum values
 * - message_validator edge cases (empty inputs, boundary values)
 * - rate_limiter session-aware overload
 * - rate_limiter config getter/setter
 * - rate_limiter auto_cleanup disabled
 * - connection_limiter underflow prevention (on_disconnect at 0)
 * - connection_limiter set_max
 * - connection_guard move assignment
 * - per_client_connection_limiter total_connections
 * - per_client_connection_limiter release of unknown client
 */

#include "internal/utils/message_validator.h"
#include "internal/utils/rate_limiter.h"

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network;

// ============================================================================
// validation_result to_string Tests
// ============================================================================

TEST(ValidationResultToStringTest, AllEnumValues)
{
	EXPECT_STREQ(to_string(validation_result::ok), "ok");
	EXPECT_STREQ(to_string(validation_result::size_exceeded), "size_exceeded");
	EXPECT_STREQ(to_string(validation_result::null_byte_detected), "null_byte_detected");
	EXPECT_STREQ(to_string(validation_result::invalid_format), "invalid_format");
	EXPECT_STREQ(to_string(validation_result::invalid_character), "invalid_character");
	EXPECT_STREQ(to_string(validation_result::header_count_exceeded), "header_count_exceeded");
}

// ============================================================================
// message_validator Extended Edge Cases
// ============================================================================

TEST(MessageValidatorExtendedTest, ValidateZeroSize)
{
	EXPECT_EQ(message_validator::validate_size(0), validation_result::ok);
	EXPECT_EQ(message_validator::validate_size(0, 0), validation_result::ok);
}

TEST(MessageValidatorExtendedTest, ValidateSizeResultCustomLimit)
{
	EXPECT_EQ(message_validator::validate_size(100, 200), validation_result::ok);
	EXPECT_EQ(message_validator::validate_size(201, 200), validation_result::size_exceeded);
}

TEST(MessageValidatorExtendedTest, SafeCopyZeroSourceSize)
{
	char dest[10] = {0};
	const char src[] = "hello";

	size_t copied = message_validator::safe_copy(dest, sizeof(dest), src, 0);
	EXPECT_EQ(copied, 0);
}

TEST(MessageValidatorExtendedTest, SafeStrcpyNullDest)
{
	EXPECT_EQ(message_validator::safe_strcpy(nullptr, 10, "hello"), 0);
}

TEST(MessageValidatorExtendedTest, SafeStrcpyZeroDestSize)
{
	char dest[10];
	EXPECT_EQ(message_validator::safe_strcpy(dest, 0, "hello"), 0);
}

TEST(MessageValidatorExtendedTest, SafeStrcpyNullSrc)
{
	char dest[10];
	EXPECT_EQ(message_validator::safe_strcpy(dest, sizeof(dest), nullptr), 0);
}

TEST(MessageValidatorExtendedTest, SafeStrcpyEmptySource)
{
	char dest[10] = {'X'};
	size_t copied = message_validator::safe_strcpy(dest, sizeof(dest), "");
	EXPECT_EQ(copied, 0);
	EXPECT_EQ(dest[0], '\0');
}

TEST(MessageValidatorExtendedTest, ValidateHttpHeaderWithTabsAndNewlines)
{
	// Tab is allowed
	EXPECT_EQ(
		message_validator::validate_http_header("Content-Type:\tapplication/json"),
		validation_result::ok);

	// CR LF is allowed
	EXPECT_EQ(
		message_validator::validate_http_header("Host: example.com\r\n"),
		validation_result::ok);
}

TEST(MessageValidatorExtendedTest, ValidateHttpHeaderEmptyString)
{
	EXPECT_EQ(
		message_validator::validate_http_header(""),
		validation_result::ok);
}

TEST(MessageValidatorExtendedTest, ValidateHttpHeaderExactMaxSize)
{
	std::string exact_max(message_limits::MAX_HEADER_SIZE, 'A');
	EXPECT_EQ(
		message_validator::validate_http_header(exact_max),
		validation_result::ok);
}

TEST(MessageValidatorExtendedTest, ValidateHeaderCountBoundary)
{
	EXPECT_TRUE(message_validator::validate_header_count(0));
	EXPECT_TRUE(message_validator::validate_header_count(message_limits::MAX_HEADER_COUNT));
	EXPECT_FALSE(message_validator::validate_header_count(message_limits::MAX_HEADER_COUNT + 1));
}

TEST(MessageValidatorExtendedTest, ValidateWebSocketFrameCustomMax)
{
	EXPECT_TRUE(message_validator::validate_websocket_frame(500, 1000));
	EXPECT_TRUE(message_validator::validate_websocket_frame(1000, 1000));
	EXPECT_FALSE(message_validator::validate_websocket_frame(1001, 1000));
}

TEST(MessageValidatorExtendedTest, ValidateUrlWithNullByte)
{
	std::string url_with_null = "/api/test";
	url_with_null += '\0';
	url_with_null += "/evil";

	EXPECT_EQ(
		message_validator::validate_url(url_with_null),
		validation_result::null_byte_detected);
}

TEST(MessageValidatorExtendedTest, ValidateUrlEmpty)
{
	EXPECT_EQ(message_validator::validate_url(""), validation_result::ok);
}

TEST(MessageValidatorExtendedTest, ContainsSuspiciousPatternCleanString)
{
	EXPECT_FALSE(message_validator::contains_suspicious_pattern("GET /api/v1/users HTTP/1.1"));
}

TEST(MessageValidatorExtendedTest, ContainsSuspiciousPatternPartialCRLF)
{
	// Only \r\n is not enough for HTTP response splitting - needs \r\n\r\n
	EXPECT_FALSE(message_validator::contains_suspicious_pattern("line1\r\nline2"));
}

TEST(MessageValidatorExtendedTest, SanitizeStringKeepsWhitespace)
{
	std::string input = "Hello\tWorld\nNew line\rReturn";
	std::string sanitized = message_validator::sanitize_string(input);
	EXPECT_EQ(sanitized, input);
}

TEST(MessageValidatorExtendedTest, SanitizeStringRemovesMultipleControlChars)
{
	std::string input;
	input += '\x01';
	input += '\x02';
	input += "clean";
	input += '\x03';
	input += '\x04';

	std::string sanitized = message_validator::sanitize_string(input);
	EXPECT_EQ(sanitized, "clean");
}

TEST(MessageValidatorExtendedTest, SanitizeStringEmpty)
{
	EXPECT_EQ(message_validator::sanitize_string(""), "");
}

TEST(MessageValidatorExtendedTest, SafeBufferSizeZero)
{
	EXPECT_EQ(message_validator::safe_buffer_size(0), 0);
	EXPECT_EQ(message_validator::safe_buffer_size(0, 100), 0);
}

// ============================================================================
// rate_limiter Extended Tests
// ============================================================================

TEST(RateLimiterExtendedTest, SessionAwareAllowWithSession)
{
	rate_limiter limiter({
		.max_requests_per_second = 10,
		.burst_size = 2,
		.auto_cleanup = false
	});

	// Same client, different sessions should have separate buckets
	EXPECT_TRUE(limiter.allow("client1", "session1"));
	EXPECT_TRUE(limiter.allow("client1", "session1"));
	EXPECT_FALSE(limiter.allow("client1", "session1")); // exhausted for session1

	// Different session should still work
	EXPECT_TRUE(limiter.allow("client1", "session2"));
}

TEST(RateLimiterExtendedTest, SessionAwareAllowWithEmptySession)
{
	rate_limiter limiter({
		.max_requests_per_second = 10,
		.burst_size = 2,
		.auto_cleanup = false
	});

	// Empty session should fall back to client-only
	EXPECT_TRUE(limiter.allow("client1", ""));
	EXPECT_TRUE(limiter.allow("client1", ""));
	EXPECT_FALSE(limiter.allow("client1", ""));
}

TEST(RateLimiterExtendedTest, GetConfig)
{
	rate_limiter_config cfg;
	cfg.max_requests_per_second = 42;
	cfg.burst_size = 10;
	cfg.auto_cleanup = false;

	rate_limiter limiter(cfg);

	auto retrieved = limiter.config();
	EXPECT_EQ(retrieved.max_requests_per_second, 42);
	EXPECT_EQ(retrieved.burst_size, 10);
	EXPECT_FALSE(retrieved.auto_cleanup);
}

TEST(RateLimiterExtendedTest, SetConfig)
{
	rate_limiter limiter({
		.max_requests_per_second = 10,
		.burst_size = 1,
		.auto_cleanup = false
	});

	// Use up the burst
	EXPECT_TRUE(limiter.allow("client1"));
	EXPECT_FALSE(limiter.allow("client1"));

	// Increase burst size
	rate_limiter_config new_cfg;
	new_cfg.max_requests_per_second = 100;
	new_cfg.burst_size = 10;
	new_cfg.auto_cleanup = false;
	limiter.set_config(new_cfg);

	auto retrieved = limiter.config();
	EXPECT_EQ(retrieved.max_requests_per_second, 100);
	EXPECT_EQ(retrieved.burst_size, 10);
}

TEST(RateLimiterExtendedTest, AutoCleanupDisabled)
{
	rate_limiter limiter({
		.max_requests_per_second = 100,
		.burst_size = 5,
		.auto_cleanup = false,
		.stale_timeout = std::chrono::seconds(0)
	});

	(void)limiter.allow("client1");
	(void)limiter.allow("client2");

	EXPECT_EQ(limiter.client_count(), 2);

	// Even though stale_timeout is 0, cleanup is disabled
	(void)limiter.allow("client3");
	EXPECT_EQ(limiter.client_count(), 3);
}

TEST(RateLimiterExtendedTest, DefaultConfig)
{
	rate_limiter limiter;

	auto cfg = limiter.config();
	EXPECT_EQ(cfg.max_requests_per_second, 100);
	EXPECT_EQ(cfg.burst_size, 20);
	EXPECT_TRUE(cfg.auto_cleanup);
}

// ============================================================================
// connection_limiter Extended Tests
// ============================================================================

TEST(ConnectionLimiterExtendedTest, OnDisconnectAtZero)
{
	connection_limiter limiter(5);

	// Disconnecting at 0 should not underflow
	limiter.on_disconnect();
	EXPECT_EQ(limiter.current(), 0);
}

TEST(ConnectionLimiterExtendedTest, SetMax)
{
	connection_limiter limiter(2);

	EXPECT_EQ(limiter.max(), 2);

	limiter.set_max(10);
	EXPECT_EQ(limiter.max(), 10);
}

TEST(ConnectionLimiterExtendedTest, DefaultMaxConnections)
{
	connection_limiter limiter;
	EXPECT_EQ(limiter.max(), 1000);
}

TEST(ConnectionLimiterExtendedTest, OnConnectManual)
{
	connection_limiter limiter(5);

	limiter.on_connect();
	EXPECT_EQ(limiter.current(), 1);

	limiter.on_connect();
	EXPECT_EQ(limiter.current(), 2);

	// on_connect does not check capacity
	limiter.on_connect();
	limiter.on_connect();
	limiter.on_connect();
	limiter.on_connect(); // Beyond max, but on_connect doesn't check
	EXPECT_EQ(limiter.current(), 6);
}

TEST(ConnectionLimiterExtendedTest, AvailableAtCapacity)
{
	connection_limiter limiter(2);

	(void)limiter.try_accept();
	(void)limiter.try_accept();

	EXPECT_EQ(limiter.available(), 0);
}

// ============================================================================
// connection_guard Extended Tests
// ============================================================================

TEST(ConnectionGuardExtendedTest, MoveAssignment)
{
	connection_limiter limiter(2);

	connection_guard guard1(limiter);
	EXPECT_TRUE(guard1.accepted());
	EXPECT_EQ(limiter.current(), 1);

	connection_guard guard2(limiter);
	EXPECT_TRUE(guard2.accepted());
	EXPECT_EQ(limiter.current(), 2);

	// Move assign guard1 = guard2 - should release guard1's old connection
	guard1 = std::move(guard2);
	EXPECT_TRUE(guard1.accepted());
	EXPECT_FALSE(guard2.accepted());
	EXPECT_EQ(limiter.current(), 1); // One released, one still held
}

TEST(ConnectionGuardExtendedTest, BoolConversion)
{
	connection_limiter limiter(1);

	{
		connection_guard guard(limiter);
		EXPECT_TRUE(static_cast<bool>(guard));
	}

	{
		connection_guard guard1(limiter);
		connection_guard guard2(limiter);
		EXPECT_TRUE(static_cast<bool>(guard1));
		EXPECT_FALSE(static_cast<bool>(guard2));
	}
}

TEST(ConnectionGuardExtendedTest, DoubleRelease)
{
	connection_limiter limiter(1);

	connection_guard guard(limiter);
	EXPECT_TRUE(guard.accepted());

	guard.release();
	EXPECT_FALSE(guard.accepted());
	EXPECT_EQ(limiter.current(), 0);

	// Second release should be no-op
	guard.release();
	EXPECT_EQ(limiter.current(), 0);
}

// ============================================================================
// per_client_connection_limiter Extended Tests
// ============================================================================

TEST(PerClientConnectionLimiterExtendedTest, TotalConnections)
{
	per_client_connection_limiter limiter(5, 100);

	(void)limiter.try_accept("client1");
	(void)limiter.try_accept("client2");
	(void)limiter.try_accept("client3");

	EXPECT_EQ(limiter.total_connections(), 3);
}

TEST(PerClientConnectionLimiterExtendedTest, ClientConnectionsUnknownClient)
{
	per_client_connection_limiter limiter(5, 100);

	EXPECT_EQ(limiter.client_connections("unknown"), 0);
}

TEST(PerClientConnectionLimiterExtendedTest, ReleaseUnknownClient)
{
	per_client_connection_limiter limiter(5, 100);

	// Releasing unknown client should not crash
	limiter.release("unknown");
	EXPECT_EQ(limiter.total_connections(), 0);
}

TEST(PerClientConnectionLimiterExtendedTest, ReleaseToZeroRemovesEntry)
{
	per_client_connection_limiter limiter(5, 100);

	(void)limiter.try_accept("client1");
	EXPECT_EQ(limiter.client_connections("client1"), 1);

	limiter.release("client1");
	EXPECT_EQ(limiter.client_connections("client1"), 0);
}

TEST(PerClientConnectionLimiterExtendedTest, TotalLimitBlocksNewClients)
{
	per_client_connection_limiter limiter(10, 2); // Per-client high, total low

	EXPECT_TRUE(limiter.try_accept("client1"));
	EXPECT_TRUE(limiter.try_accept("client2"));
	EXPECT_FALSE(limiter.try_accept("client3")); // total limit hit

	limiter.release("client1");
	EXPECT_TRUE(limiter.try_accept("client3")); // now there's room
}

// ============================================================================
// Thread Safety Extended Tests
// ============================================================================

TEST(RateLimiterThreadSafetyTest, ConcurrentSessionAwareAccess)
{
	rate_limiter limiter({
		.max_requests_per_second = 10000,
		.burst_size = 100,
		.auto_cleanup = false
	});

	std::vector<std::thread> threads;

	for (int i = 0; i < 4; ++i)
	{
		threads.emplace_back([&limiter, i]() {
			std::string client = "client" + std::to_string(i);
			for (int j = 0; j < 50; ++j)
			{
				std::string session = "session" + std::to_string(j % 5);
				(void)limiter.allow(client, session);
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_GT(limiter.client_count(), 0);
}

TEST(ConnectionLimiterThreadSafetyTest, ConcurrentAcceptDisconnect)
{
	connection_limiter limiter(100);

	std::vector<std::thread> threads;

	// Accept threads
	for (int i = 0; i < 4; ++i)
	{
		threads.emplace_back([&limiter]() {
			for (int j = 0; j < 25; ++j)
			{
				if (limiter.try_accept())
				{
					std::this_thread::yield();
					limiter.on_disconnect();
				}
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(limiter.current(), 0);
}
