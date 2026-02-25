/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/session_info.h"
#include "internal/core/session_traits.h"
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>

namespace core = kcenon::network::core;

/**
 * @file session_info_traits_test.cpp
 * @brief Unit tests for session_info_base template specializations and
 *        session_traits compile-time properties
 *
 * Tests validate:
 * - session_traits default template values
 * - session_traits specialization for messaging_session
 * - session_traits specialization for ws_connection
 * - session_traits custom specialization for mock types
 * - session_info_base<T, true> activity tracking specialization
 * - session_info_base<T, false> minimal specialization
 * - session_info_t alias resolution via session_traits
 * - Copy and move semantics for session_info_base
 */

// ============================================================================
// Mock Types for Testing
// ============================================================================

struct mock_tracked_session
{
	int id = 0;
	std::string name;

	mock_tracked_session() = default;
	explicit mock_tracked_session(int i, std::string n = "")
		: id(i), name(std::move(n))
	{
	}
};

struct mock_simple_session
{
	int value = 0;
};

// Specialize session_traits for mock types
namespace kcenon::network::core {

template <>
struct session_traits<mock_tracked_session>
{
	static constexpr bool has_activity_tracking = true;
	static constexpr bool stop_on_clear = true;
	static constexpr const char* id_prefix = "tracked_";
};

template <>
struct session_traits<mock_simple_session>
{
	static constexpr bool has_activity_tracking = false;
	static constexpr bool stop_on_clear = false;
	static constexpr const char* id_prefix = "simple_";
};

} // namespace kcenon::network::core

// ============================================================================
// session_traits Default Template Tests
// ============================================================================

class SessionTraitsDefaultTest : public ::testing::Test
{
};

struct unknown_session_type
{
};

TEST_F(SessionTraitsDefaultTest, DefaultHasNoActivityTracking)
{
	static_assert(!core::session_traits<unknown_session_type>::has_activity_tracking,
				  "Default session_traits should have activity tracking disabled");
	SUCCEED();
}

TEST_F(SessionTraitsDefaultTest, DefaultHasNoStopOnClear)
{
	static_assert(!core::session_traits<unknown_session_type>::stop_on_clear,
				  "Default session_traits should have stop_on_clear disabled");
	SUCCEED();
}

TEST_F(SessionTraitsDefaultTest, DefaultIdPrefix)
{
	constexpr auto prefix = core::session_traits<unknown_session_type>::id_prefix;
	EXPECT_STREQ(prefix, "session_");
}

TEST_F(SessionTraitsDefaultTest, DefaultTraitsAreConstexpr)
{
	static_assert(core::session_traits<unknown_session_type>::has_activity_tracking == false);
	static_assert(core::session_traits<unknown_session_type>::stop_on_clear == false);
	SUCCEED();
}

// ============================================================================
// session_traits<messaging_session> Specialization Tests
// ============================================================================

class SessionTraitsMessagingTest : public ::testing::Test
{
};

TEST_F(SessionTraitsMessagingTest, HasActivityTracking)
{
	using traits = core::session_traits<kcenon::network::session::messaging_session>;
	static_assert(traits::has_activity_tracking,
				  "messaging_session should have activity tracking enabled");
	SUCCEED();
}

TEST_F(SessionTraitsMessagingTest, HasStopOnClear)
{
	using traits = core::session_traits<kcenon::network::session::messaging_session>;
	static_assert(traits::stop_on_clear,
				  "messaging_session should have stop_on_clear enabled");
	SUCCEED();
}

TEST_F(SessionTraitsMessagingTest, IdPrefixIsSession)
{
	using traits = core::session_traits<kcenon::network::session::messaging_session>;
	EXPECT_STREQ(traits::id_prefix, "session_");
}

// ============================================================================
// session_traits<ws_connection> Specialization Tests
// ============================================================================

class SessionTraitsWsConnectionTest : public ::testing::Test
{
};

TEST_F(SessionTraitsWsConnectionTest, HasNoActivityTracking)
{
	using traits = core::session_traits<core::ws_connection>;
	static_assert(!traits::has_activity_tracking,
				  "ws_connection should have activity tracking disabled");
	SUCCEED();
}

TEST_F(SessionTraitsWsConnectionTest, HasNoStopOnClear)
{
	using traits = core::session_traits<core::ws_connection>;
	static_assert(!traits::stop_on_clear,
				  "ws_connection should have stop_on_clear disabled");
	SUCCEED();
}

TEST_F(SessionTraitsWsConnectionTest, IdPrefixIsWsConn)
{
	using traits = core::session_traits<core::ws_connection>;
	EXPECT_STREQ(traits::id_prefix, "ws_conn_");
}

// ============================================================================
// session_traits Custom Mock Specialization Tests
// ============================================================================

class SessionTraitsCustomTest : public ::testing::Test
{
};

TEST_F(SessionTraitsCustomTest, TrackedSessionHasActivityTracking)
{
	using traits = core::session_traits<mock_tracked_session>;
	static_assert(traits::has_activity_tracking,
				  "mock_tracked_session should have activity tracking enabled");
	SUCCEED();
}

TEST_F(SessionTraitsCustomTest, TrackedSessionHasStopOnClear)
{
	using traits = core::session_traits<mock_tracked_session>;
	static_assert(traits::stop_on_clear,
				  "mock_tracked_session should have stop_on_clear enabled");
	SUCCEED();
}

TEST_F(SessionTraitsCustomTest, TrackedSessionIdPrefix)
{
	using traits = core::session_traits<mock_tracked_session>;
	EXPECT_STREQ(traits::id_prefix, "tracked_");
}

TEST_F(SessionTraitsCustomTest, SimpleSessionHasNoActivityTracking)
{
	using traits = core::session_traits<mock_simple_session>;
	static_assert(!traits::has_activity_tracking,
				  "mock_simple_session should have activity tracking disabled");
	SUCCEED();
}

TEST_F(SessionTraitsCustomTest, SimpleSessionIdPrefix)
{
	using traits = core::session_traits<mock_simple_session>;
	EXPECT_STREQ(traits::id_prefix, "simple_");
}

// ============================================================================
// session_info_base<T, true> Activity Tracking Tests
// ============================================================================

class SessionInfoTrackedTest : public ::testing::Test
{
protected:
	using tracked_info = core::session_info_base<mock_tracked_session, true>;
};

TEST_F(SessionInfoTrackedTest, ConstructionStoresSession)
{
	auto session = std::make_shared<mock_tracked_session>(42, "test");

	tracked_info info(session);

	ASSERT_NE(info.session, nullptr);
	EXPECT_EQ(info.session->id, 42);
	EXPECT_EQ(info.session->name, "test");
}

TEST_F(SessionInfoTrackedTest, ConstructionSetsCreatedAt)
{
	auto before = std::chrono::steady_clock::now();
	auto session = std::make_shared<mock_tracked_session>(1);

	tracked_info info(session);
	auto after = std::chrono::steady_clock::now();

	EXPECT_GE(info.created_at, before);
	EXPECT_LE(info.created_at, after);
}

TEST_F(SessionInfoTrackedTest, LastActivityEqualsCreatedAtInitially)
{
	auto session = std::make_shared<mock_tracked_session>(1);

	tracked_info info(session);

	EXPECT_EQ(info.last_activity, info.created_at);
}

TEST_F(SessionInfoTrackedTest, UpdateActivityChangesTimestamp)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	tracked_info info(session);

	auto initial_activity = info.last_activity;

	// Small delay to ensure time progresses
	std::this_thread::sleep_for(std::chrono::milliseconds(2));

	info.update_activity();

	EXPECT_GT(info.last_activity, initial_activity);
}

TEST_F(SessionInfoTrackedTest, UpdateActivityDoesNotChangeCreatedAt)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	tracked_info info(session);

	auto original_created = info.created_at;

	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	info.update_activity();

	EXPECT_EQ(info.created_at, original_created);
}

TEST_F(SessionInfoTrackedTest, IdleDurationInitiallySmall)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	tracked_info info(session);

	auto idle = info.idle_duration();

	// Just created, idle time should be very small (< 100ms)
	EXPECT_LT(idle.count(), 100);
}

TEST_F(SessionInfoTrackedTest, IdleDurationIncreasesOverTime)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	tracked_info info(session);

	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	auto idle = info.idle_duration();

	EXPECT_GE(idle.count(), 5); // At least 5ms
}

TEST_F(SessionInfoTrackedTest, IdleDurationResetsAfterUpdateActivity)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	tracked_info info(session);

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	auto idle_before = info.idle_duration();

	info.update_activity();
	auto idle_after = info.idle_duration();

	EXPECT_LT(idle_after.count(), idle_before.count());
}

TEST_F(SessionInfoTrackedTest, SharedPtrOwnership)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	ASSERT_EQ(session.use_count(), 1);

	{
		tracked_info info(session);
		EXPECT_EQ(session.use_count(), 2);
	}

	// After info goes out of scope, use count drops back to 1
	EXPECT_EQ(session.use_count(), 1);
}

TEST_F(SessionInfoTrackedTest, MoveConstructedSessionPointer)
{
	auto session = std::make_shared<mock_tracked_session>(99, "movable");
	auto raw_ptr = session.get();

	tracked_info info(std::move(session));

	EXPECT_EQ(info.session.get(), raw_ptr);
	EXPECT_EQ(info.session->id, 99);
}

TEST_F(SessionInfoTrackedTest, MultipleUpdateActivityCalls)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	tracked_info info(session);

	for (int i = 0; i < 5; ++i)
	{
		auto before_update = std::chrono::steady_clock::now();
		info.update_activity();
		auto after_update = std::chrono::steady_clock::now();

		EXPECT_GE(info.last_activity, before_update);
		EXPECT_LE(info.last_activity, after_update);
	}
}

// ============================================================================
// session_info_base<T, false> Minimal Specialization Tests
// ============================================================================

class SessionInfoSimpleTest : public ::testing::Test
{
protected:
	using simple_info = core::session_info_base<mock_simple_session, false>;
};

TEST_F(SessionInfoSimpleTest, ConstructionStoresSession)
{
	auto session = std::make_shared<mock_simple_session>();
	session->value = 42;

	simple_info info(session);

	ASSERT_NE(info.session, nullptr);
	EXPECT_EQ(info.session->value, 42);
}

TEST_F(SessionInfoSimpleTest, SharedPtrOwnership)
{
	auto session = std::make_shared<mock_simple_session>();
	ASSERT_EQ(session.use_count(), 1);

	{
		simple_info info(session);
		EXPECT_EQ(session.use_count(), 2);
	}

	EXPECT_EQ(session.use_count(), 1);
}

TEST_F(SessionInfoSimpleTest, MoveConstructedSessionPointer)
{
	auto session = std::make_shared<mock_simple_session>();
	session->value = 77;
	auto raw_ptr = session.get();

	simple_info info(std::move(session));

	EXPECT_EQ(info.session.get(), raw_ptr);
	EXPECT_EQ(info.session->value, 77);
}

TEST_F(SessionInfoSimpleTest, NullptrSession)
{
	simple_info info(nullptr);

	EXPECT_EQ(info.session, nullptr);
}

// ============================================================================
// session_info_t Alias Resolution Tests
// ============================================================================

class SessionInfoAliasTest : public ::testing::Test
{
};

TEST_F(SessionInfoAliasTest, TrackedTypeResolvesToTrueSpecialization)
{
	// session_info_t<mock_tracked_session> should be
	// session_info_base<mock_tracked_session, true>
	using info_type = core::session_info_t<mock_tracked_session>;
	using expected_type = core::session_info_base<mock_tracked_session, true>;

	static_assert(std::is_same_v<info_type, expected_type>,
				  "session_info_t for tracked session should resolve to "
				  "session_info_base<T, true>");
	SUCCEED();
}

TEST_F(SessionInfoAliasTest, SimpleTypeResolvesToFalseSpecialization)
{
	using info_type = core::session_info_t<mock_simple_session>;
	using expected_type = core::session_info_base<mock_simple_session, false>;

	static_assert(std::is_same_v<info_type, expected_type>,
				  "session_info_t for simple session should resolve to "
				  "session_info_base<T, false>");
	SUCCEED();
}

TEST_F(SessionInfoAliasTest, UnknownTypeResolvesToFalseSpecialization)
{
	using info_type = core::session_info_t<unknown_session_type>;
	using expected_type = core::session_info_base<unknown_session_type, false>;

	static_assert(std::is_same_v<info_type, expected_type>,
				  "session_info_t for unknown type should resolve to "
				  "session_info_base<T, false>");
	SUCCEED();
}

TEST_F(SessionInfoAliasTest, TrackedAliasHasUpdateActivity)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	core::session_info_t<mock_tracked_session> info(session);

	// Should compile and work - has activity tracking
	info.update_activity();
	auto idle = info.idle_duration();
	EXPECT_GE(idle.count(), 0);
}

// ============================================================================
// session_info_base Copy/Move Semantics Tests
// ============================================================================

class SessionInfoSemanticsTest : public ::testing::Test
{
};

TEST_F(SessionInfoSemanticsTest, TrackedInfoCopyable)
{
	auto session = std::make_shared<mock_tracked_session>(1, "original");
	core::session_info_base<mock_tracked_session, true> original(session);

	auto copy = original;

	EXPECT_EQ(copy.session, original.session);
	EXPECT_EQ(copy.created_at, original.created_at);
	EXPECT_EQ(copy.last_activity, original.last_activity);
}

TEST_F(SessionInfoSemanticsTest, TrackedInfoCopySharesSession)
{
	auto session = std::make_shared<mock_tracked_session>(1);
	core::session_info_base<mock_tracked_session, true> original(session);

	auto copy = original;

	// Both should point to the same session
	EXPECT_EQ(session.use_count(), 3); // session + original + copy
	EXPECT_EQ(copy.session.get(), original.session.get());
}

TEST_F(SessionInfoSemanticsTest, TrackedInfoMovable)
{
	auto session = std::make_shared<mock_tracked_session>(1, "movable");
	auto raw_ptr = session.get();
	core::session_info_base<mock_tracked_session, true> original(session);

	auto moved = std::move(original);

	EXPECT_EQ(moved.session.get(), raw_ptr);
	EXPECT_EQ(moved.session->name, "movable");
}

TEST_F(SessionInfoSemanticsTest, SimpleInfoCopyable)
{
	auto session = std::make_shared<mock_simple_session>();
	session->value = 42;
	core::session_info_base<mock_simple_session, false> original(session);

	auto copy = original;

	EXPECT_EQ(copy.session, original.session);
	EXPECT_EQ(copy.session->value, 42);
}

TEST_F(SessionInfoSemanticsTest, SimpleInfoMovable)
{
	auto session = std::make_shared<mock_simple_session>();
	session->value = 99;
	auto raw_ptr = session.get();
	core::session_info_base<mock_simple_session, false> original(session);

	auto moved = std::move(original);

	EXPECT_EQ(moved.session.get(), raw_ptr);
	EXPECT_EQ(moved.session->value, 99);
}
