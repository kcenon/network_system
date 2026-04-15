/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file tracing_exporters_test.cpp
 * @brief Unit tests for tracing configuration and exporter infrastructure
 *
 * Tests validate:
 * - configure_tracing / shutdown_tracing lifecycle
 * - is_tracing_enabled state transitions
 * - tracing_config factory methods (console, disabled, otlp_grpc, otlp_http)
 * - register_span_processor callback invocation
 * - flush_tracing behavior
 * - Sampling configuration
 */

#include "kcenon/network/detail/tracing/tracing_config.h"
#include "kcenon/network/detail/tracing/span.h"
#include "kcenon/network/detail/tracing/trace_context.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <vector>

using namespace kcenon::network::tracing;

// ============================================================================
// Test Fixture
// ============================================================================

class TracingExportersTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		shutdown_tracing();
	}

	void TearDown() override
	{
		shutdown_tracing();
	}
};

// ============================================================================
// Configuration Lifecycle Tests
// ============================================================================

TEST_F(TracingExportersTest, DefaultTracingDisabled)
{
	EXPECT_FALSE(is_tracing_enabled());
}

TEST_F(TracingExportersTest, EnableWithConsoleExporter)
{
	auto config = tracing_config::console();
	configure_tracing(config);

	EXPECT_TRUE(is_tracing_enabled());
}

TEST_F(TracingExportersTest, DisableWithNoneExporter)
{
	auto config = tracing_config::console();
	configure_tracing(config);
	EXPECT_TRUE(is_tracing_enabled());

	auto disabled = tracing_config::disabled();
	configure_tracing(disabled);
	EXPECT_FALSE(is_tracing_enabled());
}

TEST_F(TracingExportersTest, ShutdownDisablesTracing)
{
	auto config = tracing_config::console();
	configure_tracing(config);
	EXPECT_TRUE(is_tracing_enabled());

	shutdown_tracing();
	EXPECT_FALSE(is_tracing_enabled());
}

TEST_F(TracingExportersTest, DoubleShutdownIsNoOp)
{
	auto config = tracing_config::console();
	configure_tracing(config);

	shutdown_tracing();
	EXPECT_NO_FATAL_FAILURE(shutdown_tracing());
	EXPECT_FALSE(is_tracing_enabled());
}

TEST_F(TracingExportersTest, ReconfigureTracing)
{
	auto console_config = tracing_config::console();
	configure_tracing(console_config);
	EXPECT_TRUE(is_tracing_enabled());

	auto disabled_config = tracing_config::disabled();
	configure_tracing(disabled_config);
	EXPECT_FALSE(is_tracing_enabled());

	configure_tracing(console_config);
	EXPECT_TRUE(is_tracing_enabled());
}

// ============================================================================
// Config Factory Method Tests
// ============================================================================

TEST_F(TracingExportersTest, ConsoleConfigFactory)
{
	auto config = tracing_config::console();
	EXPECT_EQ(config.exporter, exporter_type::console);
	EXPECT_EQ(config.service_name, "network_system");
}

TEST_F(TracingExportersTest, DisabledConfigFactory)
{
	auto config = tracing_config::disabled();
	EXPECT_EQ(config.exporter, exporter_type::none);
}

TEST_F(TracingExportersTest, OtlpGrpcConfigFactory)
{
	auto config = tracing_config::otlp_grpc();
	EXPECT_EQ(config.exporter, exporter_type::otlp_grpc);
	EXPECT_EQ(config.otlp.endpoint, "http://localhost:4317");
}

TEST_F(TracingExportersTest, OtlpGrpcConfigWithCustomEndpoint)
{
	auto config = tracing_config::otlp_grpc("http://collector:4317");
	EXPECT_EQ(config.exporter, exporter_type::otlp_grpc);
	EXPECT_EQ(config.otlp.endpoint, "http://collector:4317");
}

TEST_F(TracingExportersTest, OtlpHttpConfigFactory)
{
	auto config = tracing_config::otlp_http();
	EXPECT_EQ(config.exporter, exporter_type::otlp_http);
	EXPECT_EQ(config.otlp.endpoint, "http://localhost:4318");
}

TEST_F(TracingExportersTest, JaegerConfigFactory)
{
	auto config = tracing_config::jaeger();
	EXPECT_EQ(config.exporter, exporter_type::jaeger);
	EXPECT_EQ(config.jaeger_endpoint, "http://localhost:14268/api/traces");
}

// ============================================================================
// Config Default Values Tests
// ============================================================================

TEST_F(TracingExportersTest, DefaultConfigValues)
{
	tracing_config config;

	EXPECT_EQ(config.exporter, exporter_type::none);
	EXPECT_EQ(config.service_name, "network_system");
	EXPECT_TRUE(config.service_version.empty());
	EXPECT_TRUE(config.service_namespace.empty());
	EXPECT_TRUE(config.service_instance_id.empty());
	EXPECT_EQ(config.sampler, sampler_type::always_on);
	EXPECT_DOUBLE_EQ(config.sample_rate, 1.0);
	EXPECT_FALSE(config.debug);
}

TEST_F(TracingExportersTest, DefaultBatchConfig)
{
	batch_config config;

	EXPECT_EQ(config.max_queue_size, 512u);
	EXPECT_EQ(config.schedule_delay, std::chrono::milliseconds{5000});
	EXPECT_EQ(config.max_export_batch_size, 512u);
	EXPECT_EQ(config.export_timeout, std::chrono::milliseconds{30000});
}

TEST_F(TracingExportersTest, DefaultOtlpConfig)
{
	otlp_config config;

	EXPECT_TRUE(config.endpoint.empty());
	EXPECT_TRUE(config.headers.empty());
	EXPECT_EQ(config.timeout, std::chrono::milliseconds{10000});
	EXPECT_FALSE(config.insecure);
	EXPECT_TRUE(config.certificate_path.empty());
}

// ============================================================================
// Span Processor Registration Tests
// ============================================================================

TEST_F(TracingExportersTest, RegisterSpanProcessor)
{
	std::atomic<int> call_count{0};

	auto config = tracing_config::console();
	configure_tracing(config);

	register_span_processor([&](const span&) {
		call_count.fetch_add(1);
	});

	// Create and end a span to trigger the processor
	{
		auto s = trace_context::create_span("test");
		s.set_status(span_status::ok);
	}

	EXPECT_GE(call_count.load(), 1);
}

TEST_F(TracingExportersTest, RegisterNullProcessorIsNoOp)
{
	EXPECT_NO_FATAL_FAILURE(register_span_processor(nullptr));
}

TEST_F(TracingExportersTest, MultipleProcessorsInvoked)
{
	std::atomic<int> count1{0};
	std::atomic<int> count2{0};

	auto config = tracing_config::console();
	configure_tracing(config);

	register_span_processor([&](const span&) { count1.fetch_add(1); });
	register_span_processor([&](const span&) { count2.fetch_add(1); });

	{
		auto s = trace_context::create_span("test");
	}

	EXPECT_GE(count1.load(), 1);
	EXPECT_GE(count2.load(), 1);
}

// ============================================================================
// Flush Tests
// ============================================================================

TEST_F(TracingExportersTest, FlushWhenDisabledIsNoOp)
{
	EXPECT_NO_FATAL_FAILURE(flush_tracing());
}

TEST_F(TracingExportersTest, FlushAfterShutdownIsNoOp)
{
	auto config = tracing_config::console();
	configure_tracing(config);
	shutdown_tracing();

	EXPECT_NO_FATAL_FAILURE(flush_tracing());
}

// ============================================================================
// Custom Config Tests
// ============================================================================

TEST_F(TracingExportersTest, CustomServiceName)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.service_name = "custom-service";
	config.service_version = "1.0.0";

	configure_tracing(config);
	EXPECT_TRUE(is_tracing_enabled());
}

TEST_F(TracingExportersTest, SamplerAlwaysOff)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::always_off;

	configure_tracing(config);
	EXPECT_TRUE(is_tracing_enabled());

	std::atomic<int> call_count{0};
	register_span_processor([&](const span&) {
		call_count.fetch_add(1);
	});

	{
		auto s = trace_context::create_span("test");
	}

	// With always_off sampler, processor should NOT be called
	EXPECT_EQ(call_count.load(), 0);
}

TEST_F(TracingExportersTest, SamplerTraceIdWithZeroRate)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::trace_id;
	config.sample_rate = 0.0;

	configure_tracing(config);

	std::atomic<int> call_count{0};
	register_span_processor([&](const span&) {
		call_count.fetch_add(1);
	});

	{
		auto s = trace_context::create_span("test");
	}

	EXPECT_EQ(call_count.load(), 0);
}

TEST_F(TracingExportersTest, SamplerTraceIdWithFullRate)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::trace_id;
	config.sample_rate = 1.0;

	configure_tracing(config);

	std::atomic<int> call_count{0};
	register_span_processor([&](const span&) {
		call_count.fetch_add(1);
	});

	{
		auto s = trace_context::create_span("test");
	}

	EXPECT_GE(call_count.load(), 1);
}
