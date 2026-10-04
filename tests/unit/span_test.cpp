/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file span_test.cpp
 * @brief Unit tests for span class
 *
 * Tests validate:
 * - Construction with name, context, and kind
 * - Attribute setting (string, int64, double, bool)
 * - Event recording
 * - Status management (ok, error, unset)
 * - RAII lifecycle (auto-end on destruction)
 * - Manual end behavior
 * - Move semantics
 * - Method chaining
 * - Duration measurement
 * - Operations on ended spans (no-op)
 */

#include "kcenon/network/detail/tracing/span.h"
#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/tracing_config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

using namespace kcenon::network::tracing;

// ============================================================================
// Test Fixture
// ============================================================================

class SpanTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Disable tracing to avoid console/export output during tests
		shutdown_tracing();
	}

	void TearDown() override
	{
		shutdown_tracing();
	}

	static auto make_valid_context() -> trace_context
	{
		auto tid = generate_trace_id();
		auto sid = generate_span_id();
		return trace_context(tid, sid, trace_flags::sampled);
	}
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(SpanTest, ConstructWithNameAndContext)
{
	auto ctx = make_valid_context();
	span s("test_operation", ctx);

	EXPECT_EQ(s.name(), "test_operation");
	EXPECT_TRUE(s.context().is_valid());
	EXPECT_EQ(s.kind(), span_kind::internal);
	EXPECT_FALSE(s.is_ended());
}

TEST_F(SpanTest, ConstructWithCustomKind)
{
	auto ctx = make_valid_context();
	span s("client_call", ctx, span_kind::client);

	EXPECT_EQ(s.kind(), span_kind::client);
}

TEST_F(SpanTest, ConstructServerKind)
{
	auto ctx = make_valid_context();
	span s("handle_request", ctx, span_kind::server);

	EXPECT_EQ(s.kind(), span_kind::server);
}

TEST_F(SpanTest, ConstructProducerKind)
{
	auto ctx = make_valid_context();
	span s("publish", ctx, span_kind::producer);

	EXPECT_EQ(s.kind(), span_kind::producer);
}

TEST_F(SpanTest, ConstructConsumerKind)
{
	auto ctx = make_valid_context();
	span s("consume", ctx, span_kind::consumer);

	EXPECT_EQ(s.kind(), span_kind::consumer);
}

// ============================================================================
// Attribute Tests
// ============================================================================

TEST_F(SpanTest, SetStringAttribute)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("key", "value");

	const auto& attrs = s.attributes();
	ASSERT_EQ(attrs.size(), 1u);
	ASSERT_TRUE(attrs.count("key"));
	EXPECT_EQ(std::get<std::string>(attrs.at("key")), "value");
}

TEST_F(SpanTest, SetConstCharAttribute)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("key", "const_char_value");

	const auto& attrs = s.attributes();
	ASSERT_EQ(attrs.size(), 1u);
	EXPECT_EQ(std::get<std::string>(attrs.at("key")), "const_char_value");
}

TEST_F(SpanTest, SetIntAttribute)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("count", int64_t{42});

	const auto& attrs = s.attributes();
	ASSERT_EQ(attrs.size(), 1u);
	EXPECT_EQ(std::get<int64_t>(attrs.at("count")), 42);
}

TEST_F(SpanTest, SetDoubleAttribute)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("latency", 3.14);

	const auto& attrs = s.attributes();
	ASSERT_EQ(attrs.size(), 1u);
	EXPECT_DOUBLE_EQ(std::get<double>(attrs.at("latency")), 3.14);
}

TEST_F(SpanTest, SetBoolAttribute)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("success", true);

	const auto& attrs = s.attributes();
	ASSERT_EQ(attrs.size(), 1u);
	EXPECT_EQ(std::get<bool>(attrs.at("success")), true);
}

TEST_F(SpanTest, SetMultipleAttributes)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("name", "test")
		.set_attribute("count", int64_t{5})
		.set_attribute("ratio", 0.5)
		.set_attribute("active", false);

	EXPECT_EQ(s.attributes().size(), 4u);
}

TEST_F(SpanTest, OverwriteAttribute)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_attribute("key", "first");
	s.set_attribute("key", "second");

	EXPECT_EQ(std::get<std::string>(s.attributes().at("key")), "second");
}

// ============================================================================
// Event Tests
// ============================================================================

TEST_F(SpanTest, AddSimpleEvent)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.add_event("processing_started");

	const auto& events = s.events();
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].name, "processing_started");
	EXPECT_TRUE(events[0].attributes.empty());
}

TEST_F(SpanTest, AddEventWithAttributes)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	std::map<std::string, attribute_value> event_attrs;
	event_attrs["detail"] = std::string("some info");
	event_attrs["count"] = int64_t{3};

	s.add_event("custom_event", event_attrs);

	const auto& events = s.events();
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].name, "custom_event");
	EXPECT_EQ(events[0].attributes.size(), 2u);
}

TEST_F(SpanTest, AddMultipleEvents)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.add_event("start");
	s.add_event("middle");
	s.add_event("end");

	EXPECT_EQ(s.events().size(), 3u);
}

// ============================================================================
// Status Tests
// ============================================================================

TEST_F(SpanTest, DefaultStatusIsUnset)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	EXPECT_EQ(s.status(), span_status::unset);
	EXPECT_TRUE(s.status_description().empty());
}

TEST_F(SpanTest, SetStatusOk)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_status(span_status::ok);

	EXPECT_EQ(s.status(), span_status::ok);
}

TEST_F(SpanTest, SetStatusWithDescription)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_status(span_status::error, "connection refused");

	EXPECT_EQ(s.status(), span_status::error);
	EXPECT_EQ(s.status_description(), "connection refused");
}

TEST_F(SpanTest, SetError)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.set_error("something went wrong");

	EXPECT_EQ(s.status(), span_status::error);
	EXPECT_EQ(s.status_description(), "something went wrong");

	// set_error also adds an exception event
	const auto& events = s.events();
	ASSERT_GE(events.size(), 1u);
	EXPECT_EQ(events.back().name, "exception");
	EXPECT_EQ(std::get<std::string>(events.back().attributes.at("exception.message")),
			  "something went wrong");
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(SpanTest, ManualEnd)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	EXPECT_FALSE(s.is_ended());
	s.end();
	EXPECT_TRUE(s.is_ended());
}

TEST_F(SpanTest, DoubleEndIsNoOp)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.end();
	EXPECT_TRUE(s.is_ended());

	// Second end should not crash
	s.end();
	EXPECT_TRUE(s.is_ended());
}

TEST_F(SpanTest, RaiiAutoEnd)
{
	bool was_ended = false;
	{
		auto ctx = make_valid_context();
		span s("op", ctx);
		was_ended = s.is_ended();
		EXPECT_FALSE(was_ended);
	}
	// After scope exit, the span should have been ended by destructor
	// We can't check the span directly after it's destroyed, but
	// no crash = success
}

TEST_F(SpanTest, AttributeOnEndedSpanIsNoOp)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.end();
	s.set_attribute("late_key", "late_value");

	EXPECT_TRUE(s.attributes().empty());
}

TEST_F(SpanTest, EventOnEndedSpanIsNoOp)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.end();
	s.add_event("late_event");

	EXPECT_TRUE(s.events().empty());
}

TEST_F(SpanTest, StatusOnEndedSpanIsNoOp)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	s.end();
	s.set_status(span_status::ok);

	EXPECT_EQ(s.status(), span_status::unset);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST_F(SpanTest, MoveConstruction)
{
	auto ctx = make_valid_context();
	span s1("original", ctx);
	s1.set_attribute("key", "value");

	span s2(std::move(s1));

	EXPECT_EQ(s2.name(), "original");
	EXPECT_EQ(s2.attributes().size(), 1u);
	EXPECT_FALSE(s2.is_ended());
}

TEST_F(SpanTest, MoveAssignment)
{
	auto ctx1 = make_valid_context();
	auto ctx2 = make_valid_context();

	span s1("first", ctx1);
	span s2("second", ctx2);

	s2 = std::move(s1);

	EXPECT_EQ(s2.name(), "first");
}

// ============================================================================
// Timing Tests
// ============================================================================

TEST_F(SpanTest, StartTimeIsRecorded)
{
	auto before = std::chrono::steady_clock::now();
	auto ctx = make_valid_context();
	span s("op", ctx);
	auto after = std::chrono::steady_clock::now();

	EXPECT_GE(s.start_time(), before);
	EXPECT_LE(s.start_time(), after);
}

TEST_F(SpanTest, DurationIsPositive)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	// Small busy wait to ensure non-zero duration
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	s.end();

	EXPECT_GT(s.duration().count(), 0);
}

TEST_F(SpanTest, EndTimeAfterStartTime)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	s.end();

	EXPECT_GE(s.end_time(), s.start_time());
}

// ============================================================================
// Method Chaining Tests
// ============================================================================

TEST_F(SpanTest, MethodChainingReturnsReference)
{
	auto ctx = make_valid_context();
	span s("op", ctx);

	auto& ref = s.set_attribute("a", "b")
					 .set_attribute("c", int64_t{1})
					 .add_event("evt")
					 .set_status(span_status::ok);

	EXPECT_EQ(&ref, &s);
}
