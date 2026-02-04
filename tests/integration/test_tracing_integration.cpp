/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file test_tracing_integration.cpp
 * @brief Integration tests for OpenTelemetry tracing system
 *
 * Tests validate:
 * - End-to-end tracing workflow without external dependencies
 * - Context propagation across simulated service boundaries
 * - Span export and collection via custom processor
 * - Complete trace lifecycle from creation to export
 */

#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/span.h"
#include "kcenon/network/detail/tracing/tracing_config.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace kcenon::network::tracing;

// ============================================================================
// Mock Span Collector for Integration Testing
// ============================================================================

namespace
{

/**
 * @brief Recorded span data for test verification
 */
struct recorded_span
{
	std::string name;
	std::string trace_id;
	std::string span_id;
	std::string parent_span_id;
	span_kind kind;
	span_status status;
	std::map<std::string, attribute_value> attributes;
	std::vector<span_event> events;
	std::chrono::nanoseconds duration;
};

/**
 * @brief Thread-safe collector for exported spans
 */
class span_collector
{
public:
	void record(const span& s)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		recorded_span record;
		record.name = s.name();
		record.trace_id = s.context().trace_id_hex();
		record.span_id = s.context().span_id_hex();
		if (s.context().parent_span_id())
		{
			record.parent_span_id = bytes_to_hex(s.context().parent_span_id()->data(), 8);
		}
		record.kind = s.kind();
		record.status = s.status();
		record.attributes = s.attributes();
		record.events = s.events();
		record.duration = s.duration();
		spans_.push_back(std::move(record));
	}

	[[nodiscard]] auto get_spans() const -> std::vector<recorded_span>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return spans_;
	}

	[[nodiscard]] auto span_count() const -> size_t
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return spans_.size();
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		spans_.clear();
	}

	[[nodiscard]] auto find_by_name(const std::string& name) const
	    -> std::vector<recorded_span>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<recorded_span> result;
		for (const auto& s : spans_)
		{
			if (s.name == name)
			{
				result.push_back(s);
			}
		}
		return result;
	}

	[[nodiscard]] auto find_by_trace_id(const std::string& trace_id) const
	    -> std::vector<recorded_span>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<recorded_span> result;
		for (const auto& s : spans_)
		{
			if (s.trace_id == trace_id)
			{
				result.push_back(s);
			}
		}
		return result;
	}

private:
	mutable std::mutex mutex_;
	std::vector<recorded_span> spans_;
};

// Global collector for tests
std::shared_ptr<span_collector> g_collector;

} // namespace

// ============================================================================
// Test Fixture for Integration Tests
// ============================================================================

class TracingIntegrationTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_collector = std::make_shared<span_collector>();

		// Shutdown any previous tracing state
		shutdown_tracing();

		// Configure tracing with console exporter (enables export callbacks)
		auto config = tracing_config::console();
		config.service_name = "integration_test_service";
		config.debug = false; // Suppress console output during tests
		configure_tracing(config);

		// Register custom processor to collect spans
		register_span_processor([](const span& s) { g_collector->record(s); });
	}

	void TearDown() override
	{
		shutdown_tracing();
		g_collector->clear();
	}
};

// ============================================================================
// End-to-End Tracing Flow Tests
// ============================================================================

TEST_F(TracingIntegrationTest, SingleSpanEndToEnd)
{
	// Create and complete a span
	{
		auto span = trace_context::create_span("e2e.single.operation");
		span.set_attribute("test.type", "integration");
		span.set_attribute("test.iteration", int64_t{1});
		span.add_event("operation_started");
		span.set_status(span_status::ok);
	}

	// Flush to ensure processing
	flush_tracing();

	// Verify span was collected
	EXPECT_EQ(g_collector->span_count(), 1);

	auto spans = g_collector->find_by_name("e2e.single.operation");
	ASSERT_EQ(spans.size(), 1);

	const auto& recorded = spans[0];
	EXPECT_EQ(recorded.name, "e2e.single.operation");
	EXPECT_EQ(recorded.status, span_status::ok);
	EXPECT_FALSE(recorded.trace_id.empty());
	EXPECT_FALSE(recorded.span_id.empty());
	EXPECT_TRUE(recorded.parent_span_id.empty()); // Root span has no parent
	EXPECT_GT(recorded.duration.count(), 0);
}

TEST_F(TracingIntegrationTest, ParentChildSpanRelationship)
{
	std::string parent_trace_id;
	std::string parent_span_id;

	// Create parent span
	{
		auto parent = trace_context::create_span("e2e.parent.operation");
		parent_trace_id = parent.context().trace_id_hex();
		parent_span_id = parent.context().span_id_hex();
		parent.set_attribute("role", "parent");

		// Create child span
		{
			auto child = parent.context().create_child_span("e2e.child.operation");
			child.set_attribute("role", "child");
			child.set_status(span_status::ok);
		}

		parent.set_status(span_status::ok);
	}

	flush_tracing();

	// Verify both spans collected
	EXPECT_EQ(g_collector->span_count(), 2);

	// Verify parent span
	auto parents = g_collector->find_by_name("e2e.parent.operation");
	ASSERT_EQ(parents.size(), 1);
	EXPECT_EQ(parents[0].trace_id, parent_trace_id);
	EXPECT_TRUE(parents[0].parent_span_id.empty());

	// Verify child span
	auto children = g_collector->find_by_name("e2e.child.operation");
	ASSERT_EQ(children.size(), 1);
	EXPECT_EQ(children[0].trace_id, parent_trace_id); // Same trace
	EXPECT_EQ(children[0].parent_span_id, parent_span_id); // Parent link
}

TEST_F(TracingIntegrationTest, MultiLevelSpanHierarchy)
{
	std::string trace_id;

	// Create three-level hierarchy: root -> service -> database
	{
		auto root = trace_context::create_span("http.server.request");
		trace_id = root.context().trace_id_hex();
		root.set_attribute("http.method", "POST");

		{
			auto service = root.context().create_child_span("service.process");
			service.set_attribute("service.name", "order_service");

			{
				auto db = service.context().create_child_span("database.query");
				db.set_attribute("db.system", "postgresql");
				db.set_attribute("db.statement", "SELECT * FROM orders");
				db.set_status(span_status::ok);
			}

			service.set_status(span_status::ok);
		}

		root.set_status(span_status::ok);
	}

	flush_tracing();

	// Verify all three spans collected with same trace ID
	auto trace_spans = g_collector->find_by_trace_id(trace_id);
	EXPECT_EQ(trace_spans.size(), 3);

	// Verify span names
	std::vector<std::string> names;
	for (const auto& s : trace_spans)
	{
		names.push_back(s.name);
	}
	EXPECT_TRUE(std::find(names.begin(), names.end(), "http.server.request") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "service.process") != names.end());
	EXPECT_TRUE(std::find(names.begin(), names.end(), "database.query") != names.end());
}

// ============================================================================
// Cross-Service Context Propagation Tests
// ============================================================================

TEST_F(TracingIntegrationTest, ContextPropagationViaHeaders)
{
	std::string trace_id;
	std::vector<std::pair<std::string, std::string>> propagated_headers;

	// Simulate Service A (client side)
	{
		auto client_span = trace_context::create_span("http.client.request");
		trace_id = client_span.context().trace_id_hex();
		client_span.set_attribute("http.url", "http://service-b/api/data");

		// Extract headers for propagation
		propagated_headers = client_span.context().to_headers();
		client_span.set_status(span_status::ok);
	}

	// Simulate network transfer (headers arrive at Service B)
	ASSERT_FALSE(propagated_headers.empty());

	// Simulate Service B (server side)
	{
		// Parse context from incoming headers
		auto incoming_ctx = trace_context::from_headers(propagated_headers);
		EXPECT_TRUE(incoming_ctx.is_valid());

		// Create server span as child of incoming context
		auto server_span = incoming_ctx.create_child_span("http.server.handler");
		server_span.set_attribute("http.route", "/api/data");

		// Verify same trace
		EXPECT_EQ(server_span.context().trace_id_hex(), trace_id);

		server_span.set_status(span_status::ok);
	}

	flush_tracing();

	// Verify distributed trace
	auto trace_spans = g_collector->find_by_trace_id(trace_id);
	EXPECT_EQ(trace_spans.size(), 2);

	// Verify client and server spans exist
	auto clients = g_collector->find_by_name("http.client.request");
	auto servers = g_collector->find_by_name("http.server.handler");
	EXPECT_EQ(clients.size(), 1);
	EXPECT_EQ(servers.size(), 1);

	// Server span should have client span as parent
	EXPECT_EQ(servers[0].parent_span_id, clients[0].span_id);
}

TEST_F(TracingIntegrationTest, TraceparentRoundTrip)
{
	// Create span and get traceparent
	auto original = trace_context::create_span("original.operation");
	auto traceparent = original.context().to_traceparent();
	auto original_trace_id = original.context().trace_id_hex();
	auto original_span_id = original.context().span_id_hex();
	original.end();

	// Parse traceparent back
	auto parsed = trace_context::from_traceparent(traceparent);
	EXPECT_TRUE(parsed.is_valid());
	EXPECT_EQ(parsed.trace_id_hex(), original_trace_id);
	EXPECT_EQ(parsed.span_id_hex(), original_span_id);

	// Create child from parsed context
	{
		auto child = parsed.create_child_span("continued.operation");
		EXPECT_EQ(child.context().trace_id_hex(), original_trace_id);
		child.set_status(span_status::ok);
	}

	flush_tracing();

	auto trace_spans = g_collector->find_by_trace_id(original_trace_id);
	EXPECT_EQ(trace_spans.size(), 2);
}

// ============================================================================
// Concurrent Tracing Tests
// ============================================================================

TEST_F(TracingIntegrationTest, ConcurrentSpanCreation)
{
	constexpr int num_threads = 8;
	constexpr int spans_per_thread = 50;
	std::atomic<int> completed{0};

	std::vector<std::thread> threads;
	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back(
		    [t, &completed]()
		    {
			    for (int i = 0; i < spans_per_thread; ++i)
			    {
				    auto span =
				        trace_context::create_span("concurrent.thread." + std::to_string(t));
				    span.set_attribute("thread.id", int64_t{t});
				    span.set_attribute("iteration", int64_t{i});
				    span.add_event("processing");
				    span.set_status(span_status::ok);
				    ++completed;
			    }
		    });
	}

	for (auto& thread : threads)
	{
		thread.join();
	}

	flush_tracing();

	// Verify all spans were collected
	EXPECT_EQ(completed.load(), num_threads * spans_per_thread);
	EXPECT_EQ(g_collector->span_count(), static_cast<size_t>(num_threads * spans_per_thread));
}

TEST_F(TracingIntegrationTest, ConcurrentTracesWithChildren)
{
	constexpr int num_traces = 10;
	constexpr int children_per_trace = 5;
	std::vector<std::string> trace_ids;
	std::mutex ids_mutex;

	std::vector<std::thread> threads;
	for (int t = 0; t < num_traces; ++t)
	{
		threads.emplace_back(
		    [t, &trace_ids, &ids_mutex]()
		    {
			    auto root = trace_context::create_span("concurrent.root." + std::to_string(t));

			    {
				    std::lock_guard<std::mutex> lock(ids_mutex);
				    trace_ids.push_back(root.context().trace_id_hex());
			    }

			    root.set_attribute("root.id", int64_t{t});

			    for (int c = 0; c < children_per_trace; ++c)
			    {
				    auto child = root.context().create_child_span(
				        "concurrent.child." + std::to_string(t) + "." + std::to_string(c));
				    child.set_attribute("child.id", int64_t{c});
				    child.set_status(span_status::ok);
			    }

			    root.set_status(span_status::ok);
		    });
	}

	for (auto& thread : threads)
	{
		thread.join();
	}

	flush_tracing();

	// Verify total span count: num_traces roots + (num_traces * children_per_trace) children
	size_t expected = num_traces * (1 + children_per_trace);
	EXPECT_EQ(g_collector->span_count(), expected);

	// Verify each trace has correct number of spans
	for (const auto& trace_id : trace_ids)
	{
		auto trace_spans = g_collector->find_by_trace_id(trace_id);
		EXPECT_EQ(trace_spans.size(), 1 + children_per_trace);
	}
}

// ============================================================================
// Error Handling Flow Tests
// ============================================================================

TEST_F(TracingIntegrationTest, ErrorSpanPropagation)
{
	std::string trace_id;

	// Simulate request with error
	{
		auto request = trace_context::create_span("http.request");
		trace_id = request.context().trace_id_hex();
		request.set_attribute("http.method", "GET");

		{
			auto db_query = request.context().create_child_span("database.query");
			db_query.set_attribute("db.statement", "SELECT * FROM non_existent");
			db_query.set_error("Table not found: non_existent");
			// Error status should propagate
		}

		request.set_status(span_status::error, "Database query failed");
	}

	flush_tracing();

	// Verify error spans
	auto trace_spans = g_collector->find_by_trace_id(trace_id);
	EXPECT_EQ(trace_spans.size(), 2);

	// Both spans should have error status
	for (const auto& s : trace_spans)
	{
		EXPECT_EQ(s.status, span_status::error);
	}
}

TEST_F(TracingIntegrationTest, MixedStatusSpans)
{
	std::string trace_id;

	// Request with partial success
	{
		auto request = trace_context::create_span("batch.request");
		trace_id = request.context().trace_id_hex();

		// Successful operation
		{
			auto op1 = request.context().create_child_span("batch.operation.1");
			op1.set_attribute("item.id", "item-1");
			op1.set_status(span_status::ok);
		}

		// Failed operation
		{
			auto op2 = request.context().create_child_span("batch.operation.2");
			op2.set_attribute("item.id", "item-2");
			op2.set_error("Processing failed for item-2");
		}

		// Successful operation
		{
			auto op3 = request.context().create_child_span("batch.operation.3");
			op3.set_attribute("item.id", "item-3");
			op3.set_status(span_status::ok);
		}

		request.set_status(span_status::ok); // Overall success
	}

	flush_tracing();

	auto trace_spans = g_collector->find_by_trace_id(trace_id);
	EXPECT_EQ(trace_spans.size(), 4);

	// Count status types
	int ok_count = 0;
	int error_count = 0;
	for (const auto& s : trace_spans)
	{
		if (s.status == span_status::ok)
			++ok_count;
		else if (s.status == span_status::error)
			++error_count;
	}
	EXPECT_EQ(ok_count, 3);
	EXPECT_EQ(error_count, 1);
}

// ============================================================================
// Real-World Scenario Tests
// ============================================================================

TEST_F(TracingIntegrationTest, HTTPRequestResponseFlow)
{
	// Simulate complete HTTP request/response flow
	{
		// Server receives request
		auto server = trace_context::create_span("http.server.receive");
		server.set_attribute("http.method", "POST");
		server.set_attribute("http.url", "/api/v1/orders");
		server.set_attribute("http.request_content_length", int64_t{256});

		// Parse and validate request
		{
			auto parse = server.context().create_child_span("http.request.parse");
			parse.add_event("json_parsed");
			parse.set_status(span_status::ok);
		}

		// Business logic
		{
			auto logic = server.context().create_child_span("order.create");
			logic.set_attribute("order.type", "standard");

			// Database insert
			{
				auto db = logic.context().create_child_span("database.insert");
				db.set_attribute("db.system", "postgresql");
				db.add_event("query_started");
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				db.add_event("query_completed");
				db.set_status(span_status::ok);
			}

			logic.set_attribute("order.id", "ORD-12345");
			logic.set_status(span_status::ok);
		}

		// Send response
		{
			auto response = server.context().create_child_span("http.response.send");
			response.set_attribute("http.status_code", int64_t{201});
			response.set_attribute("http.response_content_length", int64_t{128});
			response.set_status(span_status::ok);
		}

		server.set_status(span_status::ok);
	}

	flush_tracing();

	// Verify complete trace
	EXPECT_EQ(g_collector->span_count(), 5);

	// Verify all spans in same trace
	auto all_spans = g_collector->get_spans();
	std::string trace_id = all_spans[0].trace_id;
	for (const auto& s : all_spans)
	{
		EXPECT_EQ(s.trace_id, trace_id);
	}

	// Verify database span has events
	auto db_spans = g_collector->find_by_name("database.insert");
	ASSERT_EQ(db_spans.size(), 1);
	EXPECT_EQ(db_spans[0].events.size(), 2);
}

TEST_F(TracingIntegrationTest, MicroservicesFlow)
{
	std::vector<std::pair<std::string, std::string>> propagated_headers;
	std::string original_trace_id;

	// API Gateway
	{
		auto gateway = trace_context::create_span("gateway.receive");
		original_trace_id = gateway.context().trace_id_hex();
		gateway.set_attribute("http.host", "api.example.com");

		// Route to user service
		{
			auto route = gateway.context().create_child_span("gateway.route");
			route.set_attribute("target.service", "user-service");
			propagated_headers = route.context().to_headers();
			route.set_status(span_status::ok);
		}

		gateway.set_status(span_status::ok);
	}

	// User Service (receives propagated context)
	{
		auto incoming = trace_context::from_headers(propagated_headers);
		auto service = incoming.create_child_span("user-service.handle");
		service.set_attribute("service.name", "user-service");

		// Call auth service
		auto auth_headers = service.context().to_headers();
		{
			auto auth_ctx = trace_context::from_headers(auth_headers);
			auto auth = auth_ctx.create_child_span("auth-service.validate");
			auth.set_attribute("service.name", "auth-service");
			auth.set_status(span_status::ok);
		}

		service.set_status(span_status::ok);
	}

	flush_tracing();

	// All 4 spans should share the same trace ID
	auto trace_spans = g_collector->find_by_trace_id(original_trace_id);
	EXPECT_EQ(trace_spans.size(), 4);

	// Verify span names for all services
	std::vector<std::string> expected_names = {"gateway.receive", "gateway.route",
	                                            "user-service.handle", "auth-service.validate"};

	for (const auto& expected : expected_names)
	{
		auto found = g_collector->find_by_name(expected);
		EXPECT_EQ(found.size(), 1) << "Missing span: " << expected;
	}
}

// ============================================================================
// Configuration Integration Tests
// ============================================================================

TEST_F(TracingIntegrationTest, ReconfigureWhileRunning)
{
	// Create initial span
	{
		auto span = trace_context::create_span("before.reconfigure");
		span.set_status(span_status::ok);
	}

	flush_tracing();
	size_t initial_count = g_collector->span_count();
	EXPECT_EQ(initial_count, 1);

	// Clear collector before reconfigure to track only new spans
	g_collector->clear();

	// Reconfigure (simulates runtime config change)
	// Use console exporter to keep tracing enabled
	auto new_config = tracing_config::console();
	new_config.service_name = "reconfigured_service";
	new_config.debug = false;
	configure_tracing(new_config);

	// Re-register processor (note: previous processors remain active)
	register_span_processor([](const span& s) { g_collector->record(s); });

	// Create span after reconfigure
	{
		auto span = trace_context::create_span("after.reconfigure");
		span.set_status(span_status::ok);
	}

	flush_tracing();

	// New span should be collected (may be recorded multiple times due to multiple processors)
	EXPECT_GE(g_collector->span_count(), 1);

	// Verify the span was recorded with correct name
	auto spans = g_collector->find_by_name("after.reconfigure");
	EXPECT_GE(spans.size(), 1);
}
