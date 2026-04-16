/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/integration/logger_integration.h"
#include <gtest/gtest.h>

#include <string>

namespace integration = kcenon::network::integration;

/**
 * @file logger_integration_test.cpp
 * @brief Unit tests for logger_integration module
 *
 * Tests validate:
 * - log_level enum values and ordering
 * - basic_logger construction with default and custom levels
 * - basic_logger level filtering (is_level_enabled)
 * - basic_logger logging operations
 * - basic_logger set/get min level
 * - logger_integration_manager singleton access
 * - logger_integration_manager set/get logger
 * - logger_integration_manager logging through manager
 * - static_destruction_guard safety check
 */

// ============================================================================
// log_level Enum Tests
// ============================================================================

class LogLevelTest : public ::testing::Test
{
};

TEST_F(LogLevelTest, LevelOrdering)
{
	EXPECT_LT(static_cast<int>(integration::log_level::trace),
			   static_cast<int>(integration::log_level::debug));
	EXPECT_LT(static_cast<int>(integration::log_level::debug),
			   static_cast<int>(integration::log_level::info));
	EXPECT_LT(static_cast<int>(integration::log_level::info),
			   static_cast<int>(integration::log_level::warn));
	EXPECT_LT(static_cast<int>(integration::log_level::warn),
			   static_cast<int>(integration::log_level::error));
	EXPECT_LT(static_cast<int>(integration::log_level::error),
			   static_cast<int>(integration::log_level::fatal));
}

TEST_F(LogLevelTest, LevelValues)
{
	EXPECT_EQ(static_cast<int>(integration::log_level::trace), 0);
	EXPECT_EQ(static_cast<int>(integration::log_level::debug), 1);
	EXPECT_EQ(static_cast<int>(integration::log_level::info), 2);
	EXPECT_EQ(static_cast<int>(integration::log_level::warn), 3);
	EXPECT_EQ(static_cast<int>(integration::log_level::error), 4);
	EXPECT_EQ(static_cast<int>(integration::log_level::fatal), 5);
}

// ============================================================================
// basic_logger Constructor Tests
// ============================================================================

class BasicLoggerConstructorTest : public ::testing::Test
{
};

TEST_F(BasicLoggerConstructorTest, DefaultMinLevel)
{
	integration::basic_logger logger;

	EXPECT_EQ(logger.get_min_level(), integration::log_level::info);
}

TEST_F(BasicLoggerConstructorTest, CustomMinLevel)
{
	integration::basic_logger logger(integration::log_level::debug);

	EXPECT_EQ(logger.get_min_level(), integration::log_level::debug);
}

// ============================================================================
// basic_logger Level Filtering Tests
// ============================================================================

class BasicLoggerFilterTest : public ::testing::Test
{
protected:
	integration::basic_logger logger_{integration::log_level::warn};
};

TEST_F(BasicLoggerFilterTest, LevelBelowMinDisabled)
{
	EXPECT_FALSE(logger_.is_level_enabled(integration::log_level::trace));
	EXPECT_FALSE(logger_.is_level_enabled(integration::log_level::debug));
	EXPECT_FALSE(logger_.is_level_enabled(integration::log_level::info));
}

TEST_F(BasicLoggerFilterTest, LevelAtMinEnabled)
{
	EXPECT_TRUE(logger_.is_level_enabled(integration::log_level::warn));
}

TEST_F(BasicLoggerFilterTest, LevelAboveMinEnabled)
{
	EXPECT_TRUE(logger_.is_level_enabled(integration::log_level::error));
	EXPECT_TRUE(logger_.is_level_enabled(integration::log_level::fatal));
}

// ============================================================================
// basic_logger Operations Tests
// ============================================================================

class BasicLoggerOperationsTest : public ::testing::Test
{
protected:
	integration::basic_logger logger_{integration::log_level::trace};
};

TEST_F(BasicLoggerOperationsTest, LogSimpleMessage)
{
	// Should not throw
	EXPECT_NO_THROW(logger_.log(integration::log_level::info, "Test message"));
}

TEST_F(BasicLoggerOperationsTest, LogWithLocation)
{
	EXPECT_NO_THROW(
		logger_.log(integration::log_level::error, "Error message",
					"test_file.cpp", 42, "test_function"));
}

TEST_F(BasicLoggerOperationsTest, FlushDoesNotThrow)
{
	EXPECT_NO_THROW(logger_.flush());
}

TEST_F(BasicLoggerOperationsTest, SetMinLevel)
{
	logger_.set_min_level(integration::log_level::error);

	EXPECT_EQ(logger_.get_min_level(), integration::log_level::error);
	EXPECT_FALSE(logger_.is_level_enabled(integration::log_level::warn));
	EXPECT_TRUE(logger_.is_level_enabled(integration::log_level::error));
}

// ============================================================================
// logger_integration_manager Tests
// ============================================================================

class LoggerIntegrationManagerTest : public ::testing::Test
{
};

TEST_F(LoggerIntegrationManagerTest, SingletonAccess)
{
	auto& mgr1 = integration::logger_integration_manager::instance();
	auto& mgr2 = integration::logger_integration_manager::instance();

	EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(LoggerIntegrationManagerTest, GetLoggerNotNull)
{
	auto& mgr = integration::logger_integration_manager::instance();

	auto logger = mgr.get_logger();

	EXPECT_NE(logger, nullptr);
}

TEST_F(LoggerIntegrationManagerTest, SetCustomLogger)
{
	auto& mgr = integration::logger_integration_manager::instance();
	auto custom = std::make_shared<integration::basic_logger>(
		integration::log_level::debug);

	mgr.set_logger(custom);

	auto retrieved = mgr.get_logger();
	EXPECT_NE(retrieved, nullptr);
}

TEST_F(LoggerIntegrationManagerTest, LogThroughManager)
{
	auto& mgr = integration::logger_integration_manager::instance();

	EXPECT_NO_THROW(mgr.log(integration::log_level::info, "Manager test"));
}

TEST_F(LoggerIntegrationManagerTest, LogWithLocationThroughManager)
{
	auto& mgr = integration::logger_integration_manager::instance();

	EXPECT_NO_THROW(
		mgr.log(integration::log_level::warn, "Warning message",
				"test.cpp", 10, "test_func"));
}

// ============================================================================
// static_destruction_guard Tests
// ============================================================================

class StaticDestructionGuardTest : public ::testing::Test
{
};

TEST_F(StaticDestructionGuardTest, LoggingIsSafeDuringNormalExecution)
{
	EXPECT_TRUE(
		integration::detail::static_destruction_guard::is_logging_safe());
}
