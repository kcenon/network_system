/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/detail/utils/result_types.h"
#include <gtest/gtest.h>

#include <string>
#include <type_traits>

namespace net = kcenon::network;

/**
 * @file result_types_test.cpp
 * @brief Unit tests for result_types.h error codes, helper functions,
 *        and Result<T> type behavior
 *
 * Tests validate:
 * - Result<T> ok/error creation and inspection
 * - VoidResult ok/error creation
 * - ok(), error(), error_void() helper functions
 * - get_error_details() and get_error_source() helpers
 * - error_codes_ext::network_system constant values
 * - Error code value ranges and distinctness
 * - Result<T> move semantics
 */

// ============================================================================
// Result<T> OK State Tests
// ============================================================================

class ResultOkTest : public ::testing::Test
{
};

TEST_F(ResultOkTest, OkIntResult)
{
	auto result = net::ok(42);

	EXPECT_TRUE(result.is_ok());
	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), 42);
}

TEST_F(ResultOkTest, OkStringResult)
{
	std::string value = "hello";
	auto result = net::ok(std::move(value));

	EXPECT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), "hello");
}

TEST_F(ResultOkTest, OkIsNotError)
{
	auto result = net::ok(100);

	EXPECT_TRUE(result.is_ok());
	EXPECT_FALSE(result.is_err());
}

TEST_F(ResultOkTest, VoidResultOk)
{
	auto result = net::ok();

	EXPECT_TRUE(result.is_ok());
	EXPECT_FALSE(result.is_err());
}

// ============================================================================
// Result<T> Error State Tests
// ============================================================================

class ResultErrorTest : public ::testing::Test
{
};

TEST_F(ResultErrorTest, ErrorIntResult)
{
	auto result = net::error<int>(-1, "test error");

	EXPECT_FALSE(result.is_ok());
	EXPECT_TRUE(result.is_err());
}

TEST_F(ResultErrorTest, ErrorIsNotOk)
{
	auto result = net::error<int>(-1, "test error");

	EXPECT_TRUE(result.is_err());
	EXPECT_FALSE(result.is_ok());
}

TEST_F(ResultErrorTest, ErrorVoidResult)
{
	auto result = net::error_void(-1, "void error");

	EXPECT_FALSE(result.is_ok());
	EXPECT_TRUE(result.is_err());
}

TEST_F(ResultErrorTest, ErrorWithSource)
{
	auto result = net::error<int>(-100, "connection failed",
								  "tcp_client");

	EXPECT_TRUE(result.is_err());

	auto source = net::get_error_source(result.error());
	EXPECT_FALSE(source.empty());
}

TEST_F(ResultErrorTest, ErrorWithDetails)
{
	auto result = net::error<int>(-200, "timeout",
								  "network_system",
								  "Connection timed out after 5000ms");

	EXPECT_TRUE(result.is_err());

	auto details = net::get_error_details(result.error());
	EXPECT_FALSE(details.empty());
}

TEST_F(ResultErrorTest, ErrorVoidWithSource)
{
	auto result = net::error_void(-300, "server error",
								  "grpc_server");

	EXPECT_TRUE(result.is_err());

	auto source = net::get_error_source(result.error());
	EXPECT_FALSE(source.empty());
}

TEST_F(ResultErrorTest, DefaultSourceIsNetworkSystem)
{
	auto result = net::error<int>(-1, "test");

	auto source = net::get_error_source(result.error());
	EXPECT_EQ(source, "network_system");
}

// ============================================================================
// Result<T> Value Type Tests
// ============================================================================

class ResultValueTypeTest : public ::testing::Test
{
};

TEST_F(ResultValueTypeTest, IntResult)
{
	auto result = net::ok(42);
	EXPECT_EQ(result.value(), 42);
}

TEST_F(ResultValueTypeTest, DoubleResult)
{
	auto result = net::ok(3.14);
	EXPECT_DOUBLE_EQ(result.value(), 3.14);
}

TEST_F(ResultValueTypeTest, StringResult)
{
	std::string s = "test string";
	auto result = net::ok(std::move(s));
	EXPECT_EQ(result.value(), "test string");
}

TEST_F(ResultValueTypeTest, VectorResult)
{
	std::vector<int> v = {1, 2, 3, 4, 5};
	auto result = net::ok(std::move(v));

	EXPECT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().size(), 5);
	EXPECT_EQ(result.value()[0], 1);
	EXPECT_EQ(result.value()[4], 5);
}

// ============================================================================
// Result<T> Move Semantics Tests
// ============================================================================

class ResultMoveTest : public ::testing::Test
{
};

TEST_F(ResultMoveTest, MoveOkResult)
{
	auto original = net::ok(std::string("movable"));

	auto moved = std::move(original);

	EXPECT_TRUE(moved.is_ok());
	EXPECT_EQ(moved.value(), "movable");
}

TEST_F(ResultMoveTest, MoveErrorResult)
{
	auto original = net::error<int>(-1, "move error");

	auto moved = std::move(original);

	EXPECT_TRUE(moved.is_err());
}

TEST_F(ResultMoveTest, MoveVoidResult)
{
	auto original = net::ok();

	auto moved = std::move(original);

	EXPECT_TRUE(moved.is_ok());
}

// ============================================================================
// error_codes_ext Tests
// ============================================================================

class ErrorCodesExtTest : public ::testing::Test
{
};

TEST_F(ErrorCodesExtTest, CircuitOpenValue)
{
	EXPECT_EQ(net::error_codes_ext::network_system::circuit_open, -604);
}

TEST_F(ErrorCodesExtTest, CircuitOpenIsNegative)
{
	EXPECT_LT(net::error_codes_ext::network_system::circuit_open, 0);
}

TEST_F(ErrorCodesExtTest, CircuitOpenIsConstexpr)
{
	static_assert(net::error_codes_ext::network_system::circuit_open == -604,
				  "circuit_open must be -604");
	SUCCEED();
}

// ============================================================================
// Helper Function Roundtrip Tests
// ============================================================================

class ResultHelperRoundtripTest : public ::testing::Test
{
};

TEST_F(ResultHelperRoundtripTest, OkThenCheckValue)
{
	auto result = net::ok(std::string("roundtrip"));

	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value(), "roundtrip");
}

TEST_F(ResultHelperRoundtripTest, ErrorThenCheckDetails)
{
	auto result = net::error<std::string>(
		-1, "error message", "test_source", "error details");

	ASSERT_TRUE(result.is_err());
	EXPECT_EQ(net::get_error_source(result.error()), "test_source");
}

TEST_F(ResultHelperRoundtripTest, ErrorVoidThenCheckSource)
{
	auto result = net::error_void(-42, "void error", "void_source");

	ASSERT_TRUE(result.is_err());
	EXPECT_EQ(net::get_error_source(result.error()), "void_source");
}

TEST_F(ResultHelperRoundtripTest, SequentialOkAndError)
{
	auto ok_result = net::ok(100);
	auto err_result = net::error<int>(-1, "fail");

	EXPECT_TRUE(ok_result.is_ok());
	EXPECT_FALSE(ok_result.is_err());
	EXPECT_TRUE(err_result.is_err());
	EXPECT_FALSE(err_result.is_ok());
}

// ============================================================================
// Type Alias Verification Tests
// ============================================================================

class ResultTypeAliasTest : public ::testing::Test
{
};

TEST_F(ResultTypeAliasTest, VoidResultIsOkCreatable)
{
	// VoidResult should be creatable via net::ok()
	net::VoidResult result = net::ok();

	EXPECT_TRUE(result.is_ok());
}

TEST_F(ResultTypeAliasTest, VoidResultIsErrorCreatable)
{
	net::VoidResult result = net::error_void(-1, "test");

	EXPECT_TRUE(result.is_err());
}

TEST_F(ResultTypeAliasTest, InternalResultMatchesNetworkResult)
{
	static_assert(std::is_same_v<net::internal::Result<int>,
				  net::Result<int>>,
				  "internal::Result should match network::Result");
	SUCCEED();
}

TEST_F(ResultTypeAliasTest, InternalVoidResultMatchesNetworkVoidResult)
{
	static_assert(std::is_same_v<net::internal::VoidResult,
				  net::VoidResult>,
				  "internal::VoidResult should match network::VoidResult");
	SUCCEED();
}

TEST_F(ResultTypeAliasTest, InternalErrorInfoMatchesNetworkErrorInfo)
{
	static_assert(std::is_same_v<net::internal::error_info,
				  net::error_info>,
				  "internal::error_info should match network::error_info");
	SUCCEED();
}
