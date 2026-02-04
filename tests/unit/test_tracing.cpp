/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file test_tracing.cpp
 * @brief Unit tests for OpenTelemetry-compatible tracing classes
 *
 * Tests validate:
 * - trace_context generation and propagation
 * - W3C Trace Context format parsing
 * - span lifecycle management (RAII)
 * - span attributes and events
 * - tracing configuration
 */

#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/span.h"
#include "kcenon/network/detail/tracing/tracing_config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace kcenon::network::tracing;

// ============================================================================
// trace_context Basic Tests
// ============================================================================

TEST(TraceContextTest, DefaultConstruction)
{
	trace_context ctx;
	EXPECT_FALSE(ctx.is_valid());
	EXPECT_FALSE(ctx.is_sampled());
}

TEST(TraceContextTest, GenerateTraceId)
{
	auto id1 = generate_trace_id();
	auto id2 = generate_trace_id();

	// IDs should be different
	EXPECT_NE(id1, id2);

	// IDs should not be all zeros
	bool all_zero = true;
	for (auto b : id1)
	{
		if (b != 0)
		{
			all_zero = false;
			break;
		}
	}
	EXPECT_FALSE(all_zero);
}

TEST(TraceContextTest, GenerateSpanId)
{
	auto id1 = generate_span_id();
	auto id2 = generate_span_id();

	// IDs should be different
	EXPECT_NE(id1, id2);

	// IDs should not be all zeros
	bool all_zero = true;
	for (auto b : id1)
	{
		if (b != 0)
		{
			all_zero = false;
			break;
		}
	}
	EXPECT_FALSE(all_zero);
}

TEST(TraceContextTest, BytesToHex)
{
	uint8_t data[] = {0xde, 0xad, 0xbe, 0xef};
	auto hex = bytes_to_hex(data, 4);
	EXPECT_EQ(hex, "deadbeef");
}

TEST(TraceContextTest, HexToBytes)
{
	uint8_t out[4];
	EXPECT_TRUE(hex_to_bytes("deadbeef", out, 4));
	EXPECT_EQ(out[0], 0xde);
	EXPECT_EQ(out[1], 0xad);
	EXPECT_EQ(out[2], 0xbe);
	EXPECT_EQ(out[3], 0xef);
}

TEST(TraceContextTest, HexToBytesInvalid)
{
	uint8_t out[4];
	// Too short
	EXPECT_FALSE(hex_to_bytes("dead", out, 4));
	// Invalid characters
	EXPECT_FALSE(hex_to_bytes("deadzzef", out, 4));
}

TEST(TraceContextTest, ConstructWithIds)
{
	trace_id_t trace_id = {0x0a, 0xf7, 0x65, 0x19, 0x16, 0xcd, 0x43, 0xdd,
	                       0x84, 0x48, 0xeb, 0x21, 0x1c, 0x80, 0x31, 0x9c};
	span_id_t span_id = {0xb7, 0xad, 0x6b, 0x71, 0x69, 0x20, 0x33, 0x31};

	trace_context ctx(trace_id, span_id, trace_flags::sampled);

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_TRUE(ctx.is_sampled());
	EXPECT_EQ(ctx.trace_id(), trace_id);
	EXPECT_EQ(ctx.span_id(), span_id);
	EXPECT_FALSE(ctx.parent_span_id().has_value());
}

TEST(TraceContextTest, ConstructWithParentSpanId)
{
	trace_id_t trace_id = {0x0a, 0xf7, 0x65, 0x19, 0x16, 0xcd, 0x43, 0xdd,
	                       0x84, 0x48, 0xeb, 0x21, 0x1c, 0x80, 0x31, 0x9c};
	span_id_t span_id = {0xb7, 0xad, 0x6b, 0x71, 0x69, 0x20, 0x33, 0x31};
	span_id_t parent_id = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

	trace_context ctx(trace_id, span_id, trace_flags::sampled, parent_id);

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_TRUE(ctx.parent_span_id().has_value());
	EXPECT_EQ(*ctx.parent_span_id(), parent_id);
}

// ============================================================================
// W3C Trace Context Format Tests
// ============================================================================

TEST(TraceContextTest, ToTraceparent)
{
	trace_id_t trace_id = {0x0a, 0xf7, 0x65, 0x19, 0x16, 0xcd, 0x43, 0xdd,
	                       0x84, 0x48, 0xeb, 0x21, 0x1c, 0x80, 0x31, 0x9c};
	span_id_t span_id = {0xb7, 0xad, 0x6b, 0x71, 0x69, 0x20, 0x33, 0x31};

	trace_context ctx(trace_id, span_id, trace_flags::sampled);

	auto traceparent = ctx.to_traceparent();
	EXPECT_EQ(traceparent, "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");
}

TEST(TraceContextTest, ToTraceparentUnsampled)
{
	trace_id_t trace_id = {0x0a, 0xf7, 0x65, 0x19, 0x16, 0xcd, 0x43, 0xdd,
	                       0x84, 0x48, 0xeb, 0x21, 0x1c, 0x80, 0x31, 0x9c};
	span_id_t span_id = {0xb7, 0xad, 0x6b, 0x71, 0x69, 0x20, 0x33, 0x31};

	trace_context ctx(trace_id, span_id, trace_flags::none);

	auto traceparent = ctx.to_traceparent();
	EXPECT_EQ(traceparent, "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-00");
}

TEST(TraceContextTest, FromTraceparent)
{
	auto ctx = trace_context::from_traceparent(
	    "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_TRUE(ctx.is_sampled());
	EXPECT_EQ(ctx.trace_id_hex(), "0af7651916cd43dd8448eb211c80319c");
	EXPECT_EQ(ctx.span_id_hex(), "b7ad6b7169203331");
}

TEST(TraceContextTest, FromTraceparentInvalid)
{
	// Too short
	EXPECT_FALSE(trace_context::from_traceparent("00-abc").is_valid());

	// Invalid version
	EXPECT_FALSE(trace_context::from_traceparent(
	                 "ff-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01")
	                 .is_valid());

	// Empty
	EXPECT_FALSE(trace_context::from_traceparent("").is_valid());
}

TEST(TraceContextTest, ToHeaders)
{
	trace_id_t trace_id = {0x0a, 0xf7, 0x65, 0x19, 0x16, 0xcd, 0x43, 0xdd,
	                       0x84, 0x48, 0xeb, 0x21, 0x1c, 0x80, 0x31, 0x9c};
	span_id_t span_id = {0xb7, 0xad, 0x6b, 0x71, 0x69, 0x20, 0x33, 0x31};

	trace_context ctx(trace_id, span_id, trace_flags::sampled);

	auto headers = ctx.to_headers();
	EXPECT_EQ(headers.size(), 1);
	EXPECT_EQ(headers[0].first, "traceparent");
	EXPECT_EQ(headers[0].second, "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");
}

TEST(TraceContextTest, FromHeaders)
{
	std::vector<std::pair<std::string, std::string>> headers = {
	    {"content-type", "application/json"},
	    {"traceparent", "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01"},
	    {"accept", "*/*"}};

	auto ctx = trace_context::from_headers(headers);

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_TRUE(ctx.is_sampled());
}

TEST(TraceContextTest, FromHeadersCaseInsensitive)
{
	std::vector<std::pair<std::string, std::string>> headers = {
	    {"TRACEPARENT", "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01"}};

	auto ctx = trace_context::from_headers(headers);
	EXPECT_TRUE(ctx.is_valid());
}

// ============================================================================
// span Basic Tests
// ============================================================================

TEST(SpanTest, CreateRootSpan)
{
	auto span = trace_context::create_span("test_operation");

	EXPECT_FALSE(span.is_ended());
	EXPECT_EQ(span.name(), "test_operation");
	EXPECT_EQ(span.kind(), span_kind::internal);
	EXPECT_EQ(span.status(), span_status::unset);
	EXPECT_TRUE(span.context().is_valid());
}

TEST(SpanTest, SpanEndsOnDestruction)
{
	trace_context ctx;
	{
		auto span = trace_context::create_span("test_operation");
		ctx = span.context();
		EXPECT_FALSE(span.is_ended());
	}
	// After scope, span should be ended (destructed)
}

TEST(SpanTest, ManualEnd)
{
	auto span = trace_context::create_span("test_operation");
	EXPECT_FALSE(span.is_ended());

	span.end();
	EXPECT_TRUE(span.is_ended());

	// Multiple ends should be safe
	span.end();
	EXPECT_TRUE(span.is_ended());
}

TEST(SpanTest, SetStringAttribute)
{
	auto span = trace_context::create_span("test_operation");
	span.set_attribute("http.method", "GET");

	const auto& attrs = span.attributes();
	EXPECT_EQ(attrs.size(), 1);
	EXPECT_EQ(std::get<std::string>(attrs.at("http.method")), "GET");
}

TEST(SpanTest, SetIntAttribute)
{
	auto span = trace_context::create_span("test_operation");
	span.set_attribute("http.status_code", int64_t{200});

	const auto& attrs = span.attributes();
	EXPECT_EQ(std::get<int64_t>(attrs.at("http.status_code")), 200);
}

TEST(SpanTest, SetDoubleAttribute)
{
	auto span = trace_context::create_span("test_operation");
	span.set_attribute("duration.ms", 123.45);

	const auto& attrs = span.attributes();
	EXPECT_DOUBLE_EQ(std::get<double>(attrs.at("duration.ms")), 123.45);
}

TEST(SpanTest, SetBoolAttribute)
{
	auto span = trace_context::create_span("test_operation");
	span.set_attribute("cache.hit", true);

	const auto& attrs = span.attributes();
	EXPECT_TRUE(std::get<bool>(attrs.at("cache.hit")));
}

TEST(SpanTest, ChainedAttributes)
{
	auto span = trace_context::create_span("test_operation");
	span.set_attribute("key1", "value1")
	    .set_attribute("key2", int64_t{42})
	    .set_attribute("key3", 3.14);

	EXPECT_EQ(span.attributes().size(), 3);
}

TEST(SpanTest, AddEvent)
{
	auto span = trace_context::create_span("test_operation");
	span.add_event("processing_started");

	const auto& events = span.events();
	EXPECT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].name, "processing_started");
}

TEST(SpanTest, AddEventWithAttributes)
{
	auto span = trace_context::create_span("test_operation");
	std::map<std::string, attribute_value> event_attrs = {{"retry_count", int64_t{3}}};
	span.add_event("retry_attempt", event_attrs);

	const auto& events = span.events();
	EXPECT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].name, "retry_attempt");
	EXPECT_EQ(std::get<int64_t>(events[0].attributes.at("retry_count")), 3);
}

TEST(SpanTest, SetStatus)
{
	auto span = trace_context::create_span("test_operation");
	span.set_status(span_status::ok);

	EXPECT_EQ(span.status(), span_status::ok);
}

TEST(SpanTest, SetStatusWithDescription)
{
	auto span = trace_context::create_span("test_operation");
	span.set_status(span_status::error, "Connection refused");

	EXPECT_EQ(span.status(), span_status::error);
	EXPECT_EQ(span.status_description(), "Connection refused");
}

TEST(SpanTest, SetError)
{
	auto span = trace_context::create_span("test_operation");
	span.set_error("Network timeout");

	EXPECT_EQ(span.status(), span_status::error);
	EXPECT_EQ(span.status_description(), "Network timeout");

	// Should also add an exception event
	const auto& events = span.events();
	EXPECT_EQ(events.size(), 1);
	EXPECT_EQ(events[0].name, "exception");
}

TEST(SpanTest, Duration)
{
	auto span = trace_context::create_span("test_operation");

	// Wait a bit
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	auto duration = span.duration();
	EXPECT_GE(duration.count(), 10'000'000); // At least 10ms in nanoseconds
}

// ============================================================================
// Context Propagation Tests
// ============================================================================

TEST(SpanTest, ChildSpanInheritsTraceId)
{
	auto parent = trace_context::create_span("parent_operation");
	auto child = parent.context().create_child_span("child_operation");

	// Same trace ID
	EXPECT_EQ(parent.context().trace_id(), child.context().trace_id());

	// Different span IDs
	EXPECT_NE(parent.context().span_id(), child.context().span_id());

	// Child has parent's span ID as parent
	EXPECT_TRUE(child.context().parent_span_id().has_value());
	EXPECT_EQ(*child.context().parent_span_id(), parent.context().span_id());
}

TEST(SpanTest, CurrentContextPropagation)
{
	// Clear any existing context
	{
		auto ctx = trace_context::current();
		// Initially should be invalid or from previous test
	}

	{
		auto span = trace_context::create_span("operation");
		auto current = trace_context::current();

		// Current context should match the span's context
		EXPECT_EQ(current.trace_id(), span.context().trace_id());
		EXPECT_EQ(current.span_id(), span.context().span_id());
	}

	// After span ends, current context may be cleared or restored
}

// ============================================================================
// Tracing Configuration Tests
// ============================================================================

TEST(TracingConfigTest, DefaultConfig)
{
	tracing_config config;
	EXPECT_EQ(config.exporter, exporter_type::none);
	EXPECT_EQ(config.service_name, "network_system");
	EXPECT_EQ(config.sample_rate, 1.0);
}

TEST(TracingConfigTest, ConsoleConfig)
{
	auto config = tracing_config::console();
	EXPECT_EQ(config.exporter, exporter_type::console);
}

TEST(TracingConfigTest, OtlpGrpcConfig)
{
	auto config = tracing_config::otlp_grpc("http://localhost:4317");
	EXPECT_EQ(config.exporter, exporter_type::otlp_grpc);
	EXPECT_EQ(config.otlp.endpoint, "http://localhost:4317");
}

TEST(TracingConfigTest, JaegerConfig)
{
	auto config = tracing_config::jaeger("http://jaeger:14268/api/traces");
	EXPECT_EQ(config.exporter, exporter_type::jaeger);
	EXPECT_EQ(config.jaeger_endpoint, "http://jaeger:14268/api/traces");
}

TEST(TracingConfigTest, DisabledConfig)
{
	auto config = tracing_config::disabled();
	EXPECT_EQ(config.exporter, exporter_type::none);
}

TEST(TracingConfigTest, ConfigureAndShutdown)
{
	auto config = tracing_config::console();
	config.service_name = "test_service";

	configure_tracing(config);
	EXPECT_TRUE(is_tracing_enabled());

	shutdown_tracing();
	EXPECT_FALSE(is_tracing_enabled());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST(SpanTest, ConcurrentSpanCreation)
{
	std::vector<std::thread> threads;
	std::atomic<int> span_count{0};

	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back(
		    [&span_count]()
		    {
			    for (int j = 0; j < 100; ++j)
			    {
				    auto span = trace_context::create_span("concurrent_test");
				    span.set_attribute("thread_id", static_cast<int64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())));
				    ++span_count;
			    }
		    });
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(span_count.load(), 1000);
}

// ============================================================================
// RAII Macro Tests
// ============================================================================

TEST(SpanTest, NetworkTraceSpanMacro)
{
	NETWORK_TRACE_SPAN("macro_test");

	_span.set_attribute("test", "value");
	EXPECT_EQ(_span.name(), "macro_test");
	EXPECT_FALSE(_span.is_ended());
}

// ============================================================================
// Exporter Tests
// ============================================================================

TEST(ExporterTest, ConsoleExporterWithSpan)
{
	// Configure console exporter for testing
	auto config = tracing_config::console();
	config.service_name = "test_service";
	config.debug = false; // Disable debug output during test

	configure_tracing(config);
	EXPECT_TRUE(is_tracing_enabled());

	// Create and complete a span
	{
		auto span = trace_context::create_span("test_console_export");
		span.set_attribute("test.key", "test_value");
		span.set_attribute("test.number", int64_t{42});
		span.add_event("test_event");
		span.set_status(span_status::ok);
	}

	shutdown_tracing();
	EXPECT_FALSE(is_tracing_enabled());
}

TEST(ExporterTest, DisabledExporter)
{
	auto config = tracing_config::disabled();
	configure_tracing(config);

	EXPECT_FALSE(is_tracing_enabled());

	// Spans should still work (just not exported)
	{
		auto span = trace_context::create_span("disabled_test");
		span.set_attribute("key", "value");
		EXPECT_TRUE(span.context().is_valid());
	}

	shutdown_tracing();
}

TEST(ExporterTest, SamplerAlwaysOff)
{
	auto config = tracing_config::console();
	config.sampler = sampler_type::always_off;
	configure_tracing(config);

	EXPECT_TRUE(is_tracing_enabled());

	// Span is created but won't be sampled
	{
		auto span = trace_context::create_span("unsampled_span");
		EXPECT_TRUE(span.context().is_valid());
	}

	shutdown_tracing();
}

TEST(ExporterTest, SamplerTraceIdBased)
{
	// Note: The sampling decision is made when exporting, not when creating spans.
	// All spans are created with is_sampled() = true by default.
	// The sampler_type::trace_id sampler applies during export.
	auto config = tracing_config::disabled(); // Use disabled to avoid console output
	config.sampler = sampler_type::trace_id;
	config.sample_rate = 0.5; // 50% sampling

	// Verify the configuration is properly set
	EXPECT_EQ(config.sampler, sampler_type::trace_id);
	EXPECT_DOUBLE_EQ(config.sample_rate, 0.5);

	// The sampling rate affects export decisions, not span creation
	// Spans are always created with valid contexts
	auto span = trace_context::create_span("sampling_test");
	EXPECT_TRUE(span.context().is_valid());
}

TEST(ExporterTest, CustomSpanProcessor)
{
	// First shutdown any previous tracing state
	shutdown_tracing();

	auto config = tracing_config::console();
	config.debug = false;
	configure_tracing(config);

	std::atomic<int> processed_count{0};

	// Register custom processor
	// Note: Due to the span's RAII design, the processor receives the span
	// during destruction. The processor callback is invoked with the span
	// reference, allowing access to all span data.
	register_span_processor([&processed_count](const span& /*s*/) { ++processed_count; });

	// Create and end spans explicitly
	for (int i = 0; i < 5; ++i)
	{
		auto span = trace_context::create_span("processor_test_" + std::to_string(i));
		span.set_attribute("index", int64_t{i});
		// span will be exported when destroyed
	}

	// Flush to ensure processing
	flush_tracing();

	// Verify that processor was called for each span
	EXPECT_EQ(processed_count.load(), 5);

	shutdown_tracing();
}

TEST(ExporterTest, BatchConfig)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.otlp.endpoint = "http://localhost:4318/v1/traces";
	config.batch.max_queue_size = 1024;
	config.batch.max_export_batch_size = 256;
	config.batch.schedule_delay = std::chrono::milliseconds{1000};

	EXPECT_EQ(config.batch.max_queue_size, 1024);
	EXPECT_EQ(config.batch.max_export_batch_size, 256);
	EXPECT_EQ(config.batch.schedule_delay, std::chrono::milliseconds{1000});
}

TEST(ExporterTest, OtlpConfigValues)
{
	auto config = tracing_config::otlp_http("http://collector:4318");

	EXPECT_EQ(config.exporter, exporter_type::otlp_http);
	EXPECT_EQ(config.otlp.endpoint, "http://collector:4318");
	EXPECT_EQ(config.otlp.timeout, std::chrono::milliseconds{10000});
	EXPECT_FALSE(config.otlp.insecure);
}

TEST(ExporterTest, ResourceAttributes)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.service_name = "my-service";
	config.service_version = "1.0.0";
	config.service_namespace = "production";
	config.service_instance_id = "instance-001";
	config.resource_attributes["deployment.environment"] = "production";
	config.resource_attributes["host.name"] = "server-01";

	EXPECT_EQ(config.service_name, "my-service");
	EXPECT_EQ(config.service_version, "1.0.0");
	EXPECT_EQ(config.resource_attributes.size(), 2);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST(SpanTest, MoveConstruction)
{
	auto span1 = trace_context::create_span("move_test");
	span1.set_attribute("key", "value");
	auto ctx = span1.context();

	span span2(std::move(span1));

	EXPECT_EQ(span2.name(), "move_test");
	EXPECT_EQ(span2.context().trace_id(), ctx.trace_id());
	EXPECT_FALSE(span2.is_ended());
}

TEST(SpanTest, MoveAssignment)
{
	auto span1 = trace_context::create_span("move_assign_test");
	auto span2 = trace_context::create_span("to_be_replaced");

	auto original_name = span1.name();
	span2 = std::move(span1);

	EXPECT_EQ(span2.name(), original_name);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(TraceContextTest, InvalidContextOperations)
{
	trace_context invalid_ctx;

	EXPECT_FALSE(invalid_ctx.is_valid());
	EXPECT_TRUE(invalid_ctx.to_traceparent().empty());
	EXPECT_TRUE(invalid_ctx.to_headers().empty());
}

TEST(SpanTest, SpanWithKinds)
{
	// Test client span kind
	{
		NETWORK_TRACE_CLIENT_SPAN("client_request");
		EXPECT_EQ(_span.kind(), span_kind::client);
	}

	// Test server span kind
	{
		NETWORK_TRACE_SERVER_SPAN("server_handler");
		EXPECT_EQ(_span.kind(), span_kind::server);
	}
}

TEST(SpanTest, MultipleAttributeOverwrite)
{
	auto span = trace_context::create_span("overwrite_test");

	span.set_attribute("key", "value1");
	span.set_attribute("key", "value2");

	const auto& attrs = span.attributes();
	EXPECT_EQ(std::get<std::string>(attrs.at("key")), "value2");
}

TEST(SpanTest, ConstCharAttributeNotBool)
{
	auto span = trace_context::create_span("const_char_test");

	// This should create a string attribute, not bool
	span.set_attribute("message", "hello world");

	const auto& attrs = span.attributes();
	EXPECT_EQ(std::get<std::string>(attrs.at("message")), "hello world");
}
