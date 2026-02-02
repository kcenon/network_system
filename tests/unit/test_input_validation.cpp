// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Unit tests for network input validation (TICKET-006)

#include <gtest/gtest.h>
#include <internal/utils/message_validator.h>
#include <internal/utils/rate_limiter.h>

#include <thread>
#include <vector>
#include <chrono>

namespace kcenon::network::test {

// ============================================================================
// Message Validator Tests
// ============================================================================

class MessageValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageValidatorTest, ValidateSizeWithinLimit) {
    EXPECT_TRUE(message_validator::validate_size(1024));
    EXPECT_TRUE(message_validator::validate_size(0));
    EXPECT_TRUE(message_validator::validate_size(message_limits::MAX_MESSAGE_SIZE));
}

TEST_F(MessageValidatorTest, ValidateSizeExceedsLimit) {
    EXPECT_FALSE(message_validator::validate_size(message_limits::MAX_MESSAGE_SIZE + 1));
    EXPECT_FALSE(message_validator::validate_size(100 * 1024 * 1024)); // 100MB
}

TEST_F(MessageValidatorTest, ValidateSizeCustomLimit) {
    EXPECT_TRUE(message_validator::validate_size(100, 1000));
    EXPECT_FALSE(message_validator::validate_size(1001, 1000));
}

TEST_F(MessageValidatorTest, ValidateSizeOrThrow) {
    EXPECT_NO_THROW(message_validator::validate_size_or_throw(1024));
    EXPECT_THROW(
        message_validator::validate_size_or_throw(message_limits::MAX_MESSAGE_SIZE + 1),
        std::length_error);
}

TEST_F(MessageValidatorTest, SafeCopyNormalCase) {
    const char src[] = "Hello, World!";
    char dest[20] = {0};

    size_t copied = message_validator::safe_copy(dest, sizeof(dest), src, strlen(src));

    EXPECT_EQ(copied, strlen(src));
    EXPECT_STREQ(dest, "Hello, World!");
}

TEST_F(MessageValidatorTest, SafeCopyTruncation) {
    const char src[] = "Hello, World!";
    char dest[5] = {0};

    size_t copied = message_validator::safe_copy(dest, sizeof(dest), src, strlen(src));

    EXPECT_EQ(copied, 5);
    EXPECT_EQ(strncmp(dest, "Hello", 5), 0);
}

TEST_F(MessageValidatorTest, SafeCopyNullPointers) {
    char dest[10];
    const char src[] = "test";

    EXPECT_EQ(message_validator::safe_copy(nullptr, 10, src, 4), 0);
    EXPECT_EQ(message_validator::safe_copy(dest, 10, nullptr, 4), 0);
    EXPECT_EQ(message_validator::safe_copy(dest, 0, src, 4), 0);
}

TEST_F(MessageValidatorTest, SafeStrcpy) {
    const char src[] = "Hello";
    char dest[10] = {0};

    size_t copied = message_validator::safe_strcpy(dest, sizeof(dest), src);

    EXPECT_EQ(copied, 5);
    EXPECT_STREQ(dest, "Hello");
}

TEST_F(MessageValidatorTest, SafeStrcpyTruncation) {
    const char src[] = "Hello, World!";
    char dest[6] = {0};

    size_t copied = message_validator::safe_strcpy(dest, sizeof(dest), src);

    EXPECT_EQ(copied, 5);
    EXPECT_STREQ(dest, "Hello");
    EXPECT_EQ(dest[5], '\0'); // Null terminator
}

TEST_F(MessageValidatorTest, ValidateHttpHeaderValid) {
    EXPECT_EQ(
        message_validator::validate_http_header("Content-Type: application/json"),
        validation_result::ok);

    EXPECT_EQ(
        message_validator::validate_http_header("Authorization: Bearer token123"),
        validation_result::ok);
}

TEST_F(MessageValidatorTest, ValidateHttpHeaderTooLarge) {
    std::string large_header(message_limits::MAX_HEADER_SIZE + 1, 'A');
    EXPECT_EQ(
        message_validator::validate_http_header(large_header),
        validation_result::size_exceeded);
}

TEST_F(MessageValidatorTest, ValidateHttpHeaderNullByte) {
    std::string header_with_null = "Content-Type: text";
    header_with_null += '\0';
    header_with_null += "evil";

    EXPECT_EQ(
        message_validator::validate_http_header(header_with_null),
        validation_result::null_byte_detected);
}

TEST_F(MessageValidatorTest, ValidateHttpHeaderInvalidChars) {
    std::string header_with_control = "Content-Type: ";
    header_with_control += '\x01'; // SOH control character

    EXPECT_EQ(
        message_validator::validate_http_header(header_with_control),
        validation_result::invalid_character);
}

TEST_F(MessageValidatorTest, ValidateWebSocketFrame) {
    EXPECT_TRUE(message_validator::validate_websocket_frame(1024));
    EXPECT_TRUE(message_validator::validate_websocket_frame(message_limits::MAX_WEBSOCKET_FRAME));
    EXPECT_FALSE(message_validator::validate_websocket_frame(message_limits::MAX_WEBSOCKET_FRAME + 1));
}

TEST_F(MessageValidatorTest, ValidateUrl) {
    EXPECT_EQ(
        message_validator::validate_url("/api/users"),
        validation_result::ok);

    std::string long_url(message_limits::MAX_URL_LENGTH + 1, 'a');
    EXPECT_EQ(
        message_validator::validate_url(long_url),
        validation_result::size_exceeded);
}

TEST_F(MessageValidatorTest, ContainsSuspiciousPattern) {
    // NULL byte
    std::string with_null = "hello";
    with_null += '\0';
    with_null += "world";
    EXPECT_TRUE(message_validator::contains_suspicious_pattern(with_null));

    // HTTP response splitting
    EXPECT_TRUE(message_validator::contains_suspicious_pattern("test\r\n\r\nevil"));

    // Normal string
    EXPECT_FALSE(message_validator::contains_suspicious_pattern("normal string"));
}

TEST_F(MessageValidatorTest, SanitizeString) {
    std::string input = "Hello";
    input += '\x01'; // Control character
    input += "World";

    std::string sanitized = message_validator::sanitize_string(input);
    EXPECT_EQ(sanitized, "HelloWorld");
}

TEST_F(MessageValidatorTest, SafeBufferSize) {
    EXPECT_EQ(message_validator::safe_buffer_size(1024), 1024);
    EXPECT_EQ(message_validator::safe_buffer_size(1024, 500), 500);
    EXPECT_EQ(
        message_validator::safe_buffer_size(100 * 1024 * 1024),
        message_limits::MAX_MESSAGE_SIZE);
}

// ============================================================================
// Rate Limiter Tests
// ============================================================================

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RateLimiterTest, AllowsRequestsWithinLimit) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 5
    });

    // Should allow burst_size requests immediately
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(limiter.allow("client1"))
            << "Request " << i << " should be allowed";
    }
}

TEST_F(RateLimiterTest, BlocksExcessiveRequests) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 5
    });

    // Exhaust burst
    for (int i = 0; i < 5; ++i) {
        limiter.allow("client1");
    }

    // Should be rate limited
    EXPECT_FALSE(limiter.allow("client1"));
}

TEST_F(RateLimiterTest, RefillsOverTime) {
    rate_limiter limiter({
        .max_requests_per_second = 1000,
        .burst_size = 1
    });

    // Use the one token
    EXPECT_TRUE(limiter.allow("client1"));
    EXPECT_FALSE(limiter.allow("client1"));

    // Wait for refill (should get at least 1 token in 10ms at 1000/sec)
    std::this_thread::yield();

    EXPECT_TRUE(limiter.allow("client1"));
}

TEST_F(RateLimiterTest, IndependentClients) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 2
    });

    // Client 1 uses its tokens
    EXPECT_TRUE(limiter.allow("client1"));
    EXPECT_TRUE(limiter.allow("client1"));

    // Client 2 should still have tokens
    EXPECT_TRUE(limiter.allow("client2"));
    EXPECT_TRUE(limiter.allow("client2"));
}

TEST_F(RateLimiterTest, WouldAllow) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 1
    });

    EXPECT_TRUE(limiter.would_allow("client1"));

    // Consume the token
    limiter.allow("client1");

    EXPECT_FALSE(limiter.would_allow("client1"));
}

TEST_F(RateLimiterTest, RemainingTokens) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 5
    });

    EXPECT_EQ(limiter.remaining_tokens("client1"), 5.0);

    limiter.allow("client1");
    EXPECT_LT(limiter.remaining_tokens("client1"), 5.0);
}

TEST_F(RateLimiterTest, Reset) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 1
    });

    limiter.allow("client1");
    EXPECT_FALSE(limiter.allow("client1"));

    limiter.reset("client1");
    EXPECT_TRUE(limiter.allow("client1"));
}

TEST_F(RateLimiterTest, ResetAll) {
    rate_limiter limiter({
        .max_requests_per_second = 10,
        .burst_size = 1
    });

    limiter.allow("client1");
    limiter.allow("client2");

    limiter.reset_all();

    EXPECT_TRUE(limiter.allow("client1"));
    EXPECT_TRUE(limiter.allow("client2"));
}

TEST_F(RateLimiterTest, ClientCount) {
    rate_limiter limiter;

    EXPECT_EQ(limiter.client_count(), 0);

    limiter.allow("client1");
    EXPECT_EQ(limiter.client_count(), 1);

    limiter.allow("client2");
    EXPECT_EQ(limiter.client_count(), 2);

    limiter.reset_all();
    EXPECT_EQ(limiter.client_count(), 0);
}

// ============================================================================
// Connection Limiter Tests
// ============================================================================

class ConnectionLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConnectionLimiterTest, CanAccept) {
    connection_limiter limiter(2);

    EXPECT_TRUE(limiter.can_accept());
    EXPECT_EQ(limiter.current(), 0);
}

TEST_F(ConnectionLimiterTest, TryAccept) {
    connection_limiter limiter(2);

    EXPECT_TRUE(limiter.try_accept());
    EXPECT_EQ(limiter.current(), 1);

    EXPECT_TRUE(limiter.try_accept());
    EXPECT_EQ(limiter.current(), 2);

    EXPECT_FALSE(limiter.try_accept());
    EXPECT_EQ(limiter.current(), 2);
}

TEST_F(ConnectionLimiterTest, OnDisconnect) {
    connection_limiter limiter(2);

    limiter.try_accept();
    limiter.try_accept();
    EXPECT_EQ(limiter.current(), 2);

    limiter.on_disconnect();
    EXPECT_EQ(limiter.current(), 1);
    EXPECT_TRUE(limiter.can_accept());
}

TEST_F(ConnectionLimiterTest, Available) {
    connection_limiter limiter(5);

    EXPECT_EQ(limiter.available(), 5);

    limiter.try_accept();
    limiter.try_accept();

    EXPECT_EQ(limiter.available(), 3);
}

TEST_F(ConnectionLimiterTest, AtCapacity) {
    connection_limiter limiter(1);

    EXPECT_FALSE(limiter.at_capacity());

    limiter.try_accept();
    EXPECT_TRUE(limiter.at_capacity());

    limiter.on_disconnect();
    EXPECT_FALSE(limiter.at_capacity());
}

// ============================================================================
// Connection Guard Tests
// ============================================================================

class ConnectionGuardTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConnectionGuardTest, AcceptsWhenAvailable) {
    connection_limiter limiter(1);

    {
        connection_guard guard(limiter);
        EXPECT_TRUE(guard.accepted());
        EXPECT_EQ(limiter.current(), 1);
    }

    // Should be released after guard destruction
    EXPECT_EQ(limiter.current(), 0);
}

TEST_F(ConnectionGuardTest, RejectsWhenFull) {
    connection_limiter limiter(1);

    connection_guard guard1(limiter);
    EXPECT_TRUE(guard1.accepted());

    connection_guard guard2(limiter);
    EXPECT_FALSE(guard2.accepted());
}

TEST_F(ConnectionGuardTest, MoveSemantics) {
    connection_limiter limiter(1);

    connection_guard guard1(limiter);
    EXPECT_TRUE(guard1.accepted());

    connection_guard guard2 = std::move(guard1);
    EXPECT_TRUE(guard2.accepted());
    EXPECT_FALSE(guard1.accepted());

    EXPECT_EQ(limiter.current(), 1);
}

TEST_F(ConnectionGuardTest, ExplicitRelease) {
    connection_limiter limiter(1);

    connection_guard guard(limiter);
    EXPECT_TRUE(guard.accepted());
    EXPECT_EQ(limiter.current(), 1);

    guard.release();
    EXPECT_FALSE(guard.accepted());
    EXPECT_EQ(limiter.current(), 0);
}

// ============================================================================
// Per-Client Connection Limiter Tests
// ============================================================================

class PerClientConnectionLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PerClientConnectionLimiterTest, LimitsPerClient) {
    per_client_connection_limiter limiter(2, 100);

    EXPECT_TRUE(limiter.try_accept("client1"));
    EXPECT_TRUE(limiter.try_accept("client1"));
    EXPECT_FALSE(limiter.try_accept("client1")); // Limit reached

    // Different client should still work
    EXPECT_TRUE(limiter.try_accept("client2"));
}

TEST_F(PerClientConnectionLimiterTest, LimitsTotal) {
    per_client_connection_limiter limiter(5, 3);

    EXPECT_TRUE(limiter.try_accept("client1"));
    EXPECT_TRUE(limiter.try_accept("client2"));
    EXPECT_TRUE(limiter.try_accept("client3"));
    EXPECT_FALSE(limiter.try_accept("client4")); // Total limit reached
}

TEST_F(PerClientConnectionLimiterTest, Release) {
    per_client_connection_limiter limiter(2, 100);

    limiter.try_accept("client1");
    limiter.try_accept("client1");

    EXPECT_EQ(limiter.client_connections("client1"), 2);

    limiter.release("client1");
    EXPECT_EQ(limiter.client_connections("client1"), 1);

    EXPECT_TRUE(limiter.try_accept("client1"));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThreadSafetyTest, RateLimiterConcurrentAccess) {
    rate_limiter limiter({
        .max_requests_per_second = 1000,
        .burst_size = 100
    });

    std::atomic<int> allowed_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&limiter, &allowed_count]() {
            for (int j = 0; j < 20; ++j) {
                if (limiter.allow("shared_client")) {
                    ++allowed_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have allowed at least burst_size requests
    EXPECT_GE(allowed_count.load(), 100);
}

TEST_F(ThreadSafetyTest, ConnectionLimiterConcurrentAccess) {
    connection_limiter limiter(50);

    std::atomic<int> accepted_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&limiter, &accepted_count]() {
            for (int j = 0; j < 10; ++j) {
                if (limiter.try_accept()) {
                    ++accepted_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have accepted exactly 50 connections
    EXPECT_EQ(accepted_count.load(), 50);
    EXPECT_EQ(limiter.current(), 50);
}

} // namespace kcenon::network::test
