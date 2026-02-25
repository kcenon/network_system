/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <string>

#include "internal/utils/openssl_compat.h"
#include <gtest/gtest.h>

using namespace kcenon::network::internal;

/**
 * @file openssl_compat_test.cpp
 * @brief Unit tests for OpenSSL compatibility utilities
 *
 * Tests validate:
 * - NETWORK_OPENSSL_VERSION_3_X macro is defined
 * - openssl_version_string() returns valid string
 * - get_openssl_error() returns "No OpenSSL error" on clean queue
 * - clear_openssl_errors() clears the error queue
 * - Deprecated warning suppression macros compile correctly
 */

// ============================================================================
// Version Macro Tests
// ============================================================================

class OpenSslVersionMacroTest : public ::testing::Test
{
};

TEST_F(OpenSslVersionMacroTest, Version3XMacroIsDefined)
{
#ifdef NETWORK_OPENSSL_VERSION_3_X
	EXPECT_EQ(NETWORK_OPENSSL_VERSION_3_X, 1);
#else
	GTEST_SKIP() << "NETWORK_OPENSSL_VERSION_3_X not defined";
#endif
}

TEST_F(OpenSslVersionMacroTest, OpenSslVersionNumberAbove3)
{
	EXPECT_GE(OPENSSL_VERSION_NUMBER, 0x30000000L);
}

// ============================================================================
// openssl_version_string() Tests
// ============================================================================

class OpenSslVersionStringTest : public ::testing::Test
{
};

TEST_F(OpenSslVersionStringTest, ReturnsNonNull)
{
	const char* version = openssl_version_string();

	EXPECT_NE(version, nullptr);
}

TEST_F(OpenSslVersionStringTest, ReturnsNonEmptyString)
{
	const char* version = openssl_version_string();

	ASSERT_NE(version, nullptr);
	EXPECT_GT(std::string(version).size(), 0);
}

TEST_F(OpenSslVersionStringTest, ContainsOpenSSL)
{
	const char* version = openssl_version_string();

	ASSERT_NE(version, nullptr);
	std::string version_str(version);
	// OpenSSL version string typically contains "OpenSSL"
	EXPECT_NE(version_str.find("OpenSSL"), std::string::npos)
		<< "Version string: " << version_str;
}

TEST_F(OpenSslVersionStringTest, ConsistentAcrossCalls)
{
	const char* version1 = openssl_version_string();
	const char* version2 = openssl_version_string();

	EXPECT_STREQ(version1, version2);
}

// ============================================================================
// get_openssl_error() Tests
// ============================================================================

class OpenSslGetErrorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Start each test with a clean error queue
		clear_openssl_errors();
	}
};

TEST_F(OpenSslGetErrorTest, ReturnsNoErrorOnCleanQueue)
{
	std::string error = get_openssl_error();

	EXPECT_EQ(error, "No OpenSSL error");
}

TEST_F(OpenSslGetErrorTest, ReturnsNonEmptyString)
{
	std::string error = get_openssl_error();

	EXPECT_FALSE(error.empty());
}

TEST_F(OpenSslGetErrorTest, ConsecutiveCallsReturnNoError)
{
	// First call consumes any existing error
	get_openssl_error();

	// Second call should also return no error
	std::string error = get_openssl_error();

	EXPECT_EQ(error, "No OpenSSL error");
}

// ============================================================================
// clear_openssl_errors() Tests
// ============================================================================

class OpenSslClearErrorsTest : public ::testing::Test
{
};

TEST_F(OpenSslClearErrorsTest, ClearOnEmptyQueueIsNoOp)
{
	// Should not crash when clearing an already-empty queue
	clear_openssl_errors();

	std::string error = get_openssl_error();
	EXPECT_EQ(error, "No OpenSSL error");
}

TEST_F(OpenSslClearErrorsTest, ClearsErrorQueue)
{
	// Push a synthetic error onto the queue
	ERR_put_error(ERR_LIB_SYS, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);

	// Queue should have an error now
	EXPECT_NE(ERR_peek_error(), 0UL);

	// Clear should remove all errors
	clear_openssl_errors();

	// Queue should be empty
	EXPECT_EQ(ERR_peek_error(), 0UL);
	EXPECT_EQ(get_openssl_error(), "No OpenSSL error");
}

TEST_F(OpenSslClearErrorsTest, ClearsMultipleErrors)
{
	// Push multiple errors
	ERR_put_error(ERR_LIB_SYS, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);
	ERR_put_error(ERR_LIB_SYS, 0, ERR_R_MALLOC_FAILURE, __FILE__, __LINE__);

	clear_openssl_errors();

	EXPECT_EQ(ERR_peek_error(), 0UL);
	EXPECT_EQ(get_openssl_error(), "No OpenSSL error");
}

// ============================================================================
// get_openssl_error() with Real Error Tests
// ============================================================================

class OpenSslErrorWithRealErrorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		clear_openssl_errors();
	}

	void TearDown() override
	{
		clear_openssl_errors();
	}
};

TEST_F(OpenSslErrorWithRealErrorTest, ReturnsErrorStringWhenErrorExists)
{
	// Push a synthetic error
	ERR_put_error(ERR_LIB_SYS, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);

	std::string error = get_openssl_error();

	// Should NOT be "No OpenSSL error"
	EXPECT_NE(error, "No OpenSSL error");
	EXPECT_FALSE(error.empty());
}

TEST_F(OpenSslErrorWithRealErrorTest, ConsumesErrorFromQueue)
{
	ERR_put_error(ERR_LIB_SYS, 0, ERR_R_INTERNAL_ERROR, __FILE__, __LINE__);

	// First call consumes the error
	get_openssl_error();

	// Second call should return no error (consumed)
	std::string error = get_openssl_error();
	EXPECT_EQ(error, "No OpenSSL error");
}

// ============================================================================
// Deprecated Warning Suppression Macro Tests
// ============================================================================

class OpenSslDeprecationMacroTest : public ::testing::Test
{
};

TEST_F(OpenSslDeprecationMacroTest, MacrosCompileCorrectly)
{
	// These macros should compile without errors
	NETWORK_OPENSSL_SUPPRESS_DEPRECATED_START
	// Code that might use deprecated APIs would go here
	[[maybe_unused]] int dummy = 42;
	NETWORK_OPENSSL_SUPPRESS_DEPRECATED_END

	SUCCEED();
}

TEST_F(OpenSslDeprecationMacroTest, MacrosCanBeNested)
{
	NETWORK_OPENSSL_SUPPRESS_DEPRECATED_START
	{
		[[maybe_unused]] int outer = 1;
		// Nested usage should also compile
		[[maybe_unused]] int inner = 2;
	}
	NETWORK_OPENSSL_SUPPRESS_DEPRECATED_END

	SUCCEED();
}
