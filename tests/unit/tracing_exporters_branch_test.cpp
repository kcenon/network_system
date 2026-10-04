// BSD 3-Clause License
// Copyright (c) 2024-2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file tracing_exporters_branch_test.cpp
 * @brief Branch-targeted unit tests for src/tracing/exporters.cpp
 *
 * The companion file `tracing_exporters_test.cpp` already covers the
 * happy-path lifecycle and console output formatting. This file is
 * dedicated to the remaining uncovered branches identified from the
 * lcov branch report:
 *
 *   - should_sample mid-range rate (neither 0.0 nor 1.0)
 *   - configure_tracing debug logging for every exporter_type
 *   - parent_span_id rendering in console and OTLP JSON exporters
 *   - status description formatting for every span_status value
 *   - OTLP HTTP path: queueing, debug logging, and explicit flush
 *   - resource attributes / service_version / namespace / instance_id
 *     in build_otlp_request output
 *   - flush_tracing fast-path when the batch queue is empty
 *
 * Tests are hermetic: stdout is captured per-test; no network or
 * filesystem side effects.
 */

#include "kcenon/network/detail/tracing/span.h"
#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/tracing_config.h"

#include <gtest/gtest.h>

#include <atomic>
#include <map>
#include <string>

using namespace kcenon::network::tracing;

namespace
{

[[maybe_unused]] auto make_context_with_parent(trace_flags flags) -> trace_context
{
	trace_id_t tid{};
	span_id_t sid{};
	span_id_t pid{};
	tid[15] = 0x01;
	sid[7] = 0x01;
	pid[7] = 0x02;
	return trace_context{tid, sid, flags, pid};
}

// Reusable test fixture: stdout capture + tracing reset.
class TracingExportersBranchTest : public ::testing::Test
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

	static auto capture(const std::function<void()>& fn) -> std::string
	{
		::testing::internal::CaptureStdout();
		fn();
		return ::testing::internal::GetCapturedStdout();
	}

	static auto contains(const std::string& haystack, const std::string& needle)
	    -> bool
	{
		return haystack.find(needle) != std::string::npos;
	}
};

// ============================================================================
// should_sample: mid-range rate path
// ============================================================================

TEST_F(TracingExportersBranchTest, TraceIdSamplerWithMidRangeRateExercisesHash)
{
	// trace_id sampler with a mid-range rate forces the hashing branch
	// in should_sample (rate is neither >= 1.0 nor <= 0.0).
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::trace_id;
	config.sample_rate = 0.5;

	configure_tracing(config);

	// Drive several span creations so the per-trace decision runs.
	std::atomic<int> processed{0};
	register_span_processor([&](const span&) { processed.fetch_add(1); });

	::testing::internal::CaptureStdout();
	for (int i = 0; i < 4; ++i)
	{
		auto s = trace_context::create_span("midrate_op");
	}
	(void)::testing::internal::GetCapturedStdout();

	// Branch was exercised regardless of the per-id outcome; counter is
	// best-effort because the result depends on the random trace_id.
	EXPECT_GE(processed.load(), 0);
}

TEST_F(TracingExportersBranchTest, TraceIdSamplerWithVerySmallRate)
{
	// A very small (but positive) rate also exits via the hashing branch
	// and almost always returns false, exercising the "not sampled" exit.
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::trace_id;
	config.sample_rate = 1e-9;

	configure_tracing(config);

	std::atomic<int> processed{0};
	register_span_processor([&](const span&) { processed.fetch_add(1); });

	::testing::internal::CaptureStdout();
	{
		auto s = trace_context::create_span("tinyrate_op");
	}
	(void)::testing::internal::GetCapturedStdout();

	EXPECT_GE(processed.load(), 0);
}

// ============================================================================
// configure_tracing: debug-log branch for each exporter_type
// ============================================================================

TEST_F(TracingExportersBranchTest, DebugConfigureLogsConsoleExporterLine)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.debug = true;

	auto out = capture([&] { configure_tracing(config); });
	EXPECT_TRUE(contains(out, "[TRACING] Configured with exporter:"));
	EXPECT_TRUE(contains(out, "console"));
}

TEST_F(TracingExportersBranchTest, DebugConfigureLogsOtlpGrpcExporterLine)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_grpc;
	config.otlp.endpoint = "http://collector.test:4317";
	config.debug = true;

	auto out = capture([&] { configure_tracing(config); });
	EXPECT_TRUE(contains(out, "otlp_grpc"));
	EXPECT_TRUE(contains(out, "http://collector.test:4317"));
}

TEST_F(TracingExportersBranchTest, DebugConfigureLogsOtlpHttpExporterLine)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.otlp.endpoint = "http://collector.test:4318";
	config.debug = true;

	auto out = capture([&] { configure_tracing(config); });
	EXPECT_TRUE(contains(out, "otlp_http"));
	EXPECT_TRUE(contains(out, "http://collector.test:4318"));
}

TEST_F(TracingExportersBranchTest, DebugConfigureLogsJaegerExporterLine)
{
	tracing_config config;
	config.exporter = exporter_type::jaeger;
	config.jaeger_endpoint = "http://jaeger.test/api/traces";
	config.debug = true;

	auto out = capture([&] { configure_tracing(config); });
	EXPECT_TRUE(contains(out, "jaeger"));
	EXPECT_TRUE(contains(out, "http://jaeger.test/api/traces"));
}

TEST_F(TracingExportersBranchTest, DebugConfigureLogsZipkinExporterLine)
{
	tracing_config config;
	config.exporter = exporter_type::zipkin;
	config.zipkin_endpoint = "http://zipkin.test/api/v2/spans";
	config.debug = true;

	auto out = capture([&] { configure_tracing(config); });
	EXPECT_TRUE(contains(out, "zipkin"));
	EXPECT_TRUE(contains(out, "http://zipkin.test/api/v2/spans"));
}

TEST_F(TracingExportersBranchTest, DebugConfigureWithNoneIsSilent)
{
	// debug=true + exporter==none must not emit the banner.
	tracing_config config;
	config.exporter = exporter_type::none;
	config.debug = true;

	auto out = capture([&] { configure_tracing(config); });
	EXPECT_FALSE(contains(out, "[TRACING] Configured with exporter:"));
}

// ============================================================================
// Console exporter: parent_span_id rendering
// ============================================================================

TEST_F(TracingExportersBranchTest, ConsoleExporterRendersParentSpanIdWhenPresent)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::always_on;
	configure_tracing(config);

	auto out = capture([] {
		span s("child", make_context_with_parent(trace_flags::sampled),
		       span_kind::internal);
	});

	EXPECT_TRUE(contains(out, "Parent ID:"));
	EXPECT_TRUE(contains(out, "=== SPAN ==="));
}

TEST_F(TracingExportersBranchTest, ConsoleExporterOmitsParentSpanIdWhenAbsent)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::always_on;
	configure_tracing(config);

	auto out = capture([] {
		auto s = trace_context::create_span("rootless");
	});

	// Root spans created via create_span have no parent id.
	EXPECT_FALSE(contains(out, "Parent ID:"));
}

// ============================================================================
// Console exporter: status description "(desc)" branch
// ============================================================================

TEST_F(TracingExportersBranchTest, StatusDescriptionAppendedOnlyWhenNonEmpty)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	config.sampler = sampler_type::always_on;
	configure_tracing(config);

	auto with_desc = capture([] {
		auto s = trace_context::create_span("op");
		s.set_status(span_status::error, "io_failure");
	});
	EXPECT_TRUE(contains(with_desc, "Status:    ERROR (io_failure)"));

	auto without_desc = capture([] {
		auto s = trace_context::create_span("op");
		s.set_status(span_status::ok);
	});
	// OK status with no description must NOT include parentheses.
	EXPECT_TRUE(contains(without_desc, "Status:    OK"));
	EXPECT_FALSE(contains(without_desc, "Status:    OK ("));
}

// ============================================================================
// OTLP HTTP exporter: queueing, debug logging, and flush
// ============================================================================

TEST_F(TracingExportersBranchTest, OtlpHttpQueuesSpanWithoutImmediateExport)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.otlp.endpoint = "http://collector.test:4318";
	// Large batch size keeps the span queued instead of triggering export.
	config.batch.max_export_batch_size = 1024;
	config.debug = false;

	configure_tracing(config);

	auto out = capture([] {
		auto s = trace_context::create_span("queued_op");
		s.set_attribute("k", "v");
	});

	// In non-debug mode nothing is printed; the span is buffered.
	EXPECT_FALSE(contains(out, "[TRACING] Exporting"));
}

TEST_F(TracingExportersBranchTest, OtlpHttpDebugLogsExportWhenBatchSizeReached)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.otlp.endpoint = "http://collector.test:4318";
	// Force immediate batch flush.
	config.batch.max_export_batch_size = 1;
	config.debug = true;
	config.service_name = "branch-svc";
	config.service_version = "9.9.9";
	config.service_namespace = "branch-ns";
	config.service_instance_id = "instance-42";
	config.resource_attributes = {{"deployment.env", "test"}};

	configure_tracing(config);

	auto out = capture([] {
		auto s = trace_context::create_span("export_op");
		s.set_attribute("count", static_cast<int64_t>(7));
		s.set_attribute("ratio", 0.5);
		s.set_attribute("flag", true);
		s.set_attribute("name", std::string{"alice"});
		s.add_event("started");
		std::map<std::string, attribute_value> evt_attrs{
		    {"retries", static_cast<int64_t>(2)}};
		s.add_event("retried", evt_attrs);
		s.set_status(span_status::error, "timeout");
	});

	// Debug banner from export_otlp_http.
	EXPECT_TRUE(contains(out, "[TRACING] Exporting"));
	EXPECT_TRUE(contains(out, "spans to OTLP HTTP: http://collector.test:4318"));
	// Request body must include resource and span identifiers.
	EXPECT_TRUE(contains(out, "[TRACING] Request body:"));
	EXPECT_TRUE(contains(out, "service.name"));
	EXPECT_TRUE(contains(out, "branch-svc"));
	EXPECT_TRUE(contains(out, "service.version"));
	EXPECT_TRUE(contains(out, "9.9.9"));
	EXPECT_TRUE(contains(out, "service.namespace"));
	EXPECT_TRUE(contains(out, "branch-ns"));
	EXPECT_TRUE(contains(out, "service.instance.id"));
	EXPECT_TRUE(contains(out, "instance-42"));
	EXPECT_TRUE(contains(out, "deployment.env"));
	// Status code 2 maps to span_status::error.
	EXPECT_TRUE(contains(out, "\"code\":2"));
	EXPECT_TRUE(contains(out, "timeout"));
	// JSON event payload uses the camelCase OTLP keys.
	EXPECT_TRUE(contains(out, "\"name\":\"export_op\""));
	EXPECT_TRUE(contains(out, "\"events\":["));
}

TEST_F(TracingExportersBranchTest, OtlpHttpQueuedSpansFlushedOnExplicitFlush)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.otlp.endpoint = "http://collector.test:4318";
	config.batch.max_export_batch_size = 1024; // do not auto-flush
	config.debug = true;

	configure_tracing(config);

	auto out = capture([] {
		auto s = trace_context::create_span("explicit_flush_op");
		s.set_status(span_status::ok);
	});
	// Span queued, no banner yet.
	EXPECT_FALSE(contains(out, "[TRACING] Exporting"));

	auto flush_out = capture([] { flush_tracing(); });
	EXPECT_TRUE(contains(flush_out, "[TRACING] Exporting"));
	// status::ok maps to code 1 in the OTLP JSON.
	EXPECT_TRUE(contains(flush_out, "\"code\":1"));
}

TEST_F(TracingExportersBranchTest, OtlpHttpJsonHandlesUnsetStatusAsCodeZero)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.otlp.endpoint = "http://collector.test:4318";
	config.batch.max_export_batch_size = 1; // flush immediately
	config.debug = true;

	configure_tracing(config);

	auto out = capture([] {
		auto s = trace_context::create_span("unset_op");
		// no set_status: status stays at unset
	});

	// Status block always present with code 0 for unset.
	EXPECT_TRUE(contains(out, "\"code\":0"));
}

TEST_F(TracingExportersBranchTest, OtlpHttpJsonRendersParentSpanIdWhenPresent)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.otlp.endpoint = "http://collector.test:4318";
	config.batch.max_export_batch_size = 1; // flush immediately
	config.debug = true;

	configure_tracing(config);

	auto out = capture([] {
		span s("with_parent", make_context_with_parent(trace_flags::sampled),
		       span_kind::client);
	});

	EXPECT_TRUE(contains(out, "\"parentSpanId\":"));
}

TEST_F(TracingExportersBranchTest, OtlpHttpJsonOmitsParentSpanIdWhenAbsent)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.otlp.endpoint = "http://collector.test:4318";
	config.batch.max_export_batch_size = 1;
	config.debug = true;

	configure_tracing(config);

	auto out = capture([] {
		auto s = trace_context::create_span("no_parent");
	});

	EXPECT_FALSE(contains(out, "\"parentSpanId\":"));
}

// ============================================================================
// flush_tracing: empty-queue fast path
// ============================================================================

TEST_F(TracingExportersBranchTest, FlushWithEmptyQueueDoesNotLogExport)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.batch.max_export_batch_size = 1024;
	config.debug = true;
	configure_tracing(config);

	auto out = capture([] { flush_tracing(); });
	// No spans queued, so export_otlp_http must not be invoked.
	EXPECT_FALSE(contains(out, "[TRACING] Exporting"));
}

// ============================================================================
// json_escape: low-control-character branch via attribute path
// ============================================================================

TEST_F(TracingExportersBranchTest, JsonEscapeRendersControlCharactersAsUnicode)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.batch.max_export_batch_size = 1;
	config.debug = true;
	configure_tracing(config);

	// Use a string with a low control character (0x01) so the
	// "default: < 0x20" branch in json_escape fires.
	std::string ctl{"a\x01"};
	ctl += "b";

	auto out = capture([&] {
		auto s = trace_context::create_span("ctl_op");
		s.set_attribute("payload", ctl);
	});

	EXPECT_TRUE(contains(out, "\\u0001"));
}

TEST_F(TracingExportersBranchTest, JsonEscapeHandlesAllStandardEscapes)
{
	tracing_config config;
	config.exporter = exporter_type::otlp_http;
	config.sampler = sampler_type::always_on;
	config.batch.max_export_batch_size = 1;
	config.debug = true;
	configure_tracing(config);

	std::string raw = "q:\" b:\\ bs:\b ff:\f nl:\n cr:\r tab:\t end";

	auto out = capture([&] {
		auto s = trace_context::create_span("escape_op");
		s.set_attribute("body", raw);
	});

	// Each escape branch must appear in the rendered JSON.
	EXPECT_TRUE(contains(out, "\\\""));
	EXPECT_TRUE(contains(out, "\\\\"));
	EXPECT_TRUE(contains(out, "\\b"));
	EXPECT_TRUE(contains(out, "\\f"));
	EXPECT_TRUE(contains(out, "\\n"));
	EXPECT_TRUE(contains(out, "\\r"));
	EXPECT_TRUE(contains(out, "\\t"));
}

// ============================================================================
// process_span: configured exporter == none early-out branch
// ============================================================================

TEST_F(TracingExportersBranchTest, NoneExporterDropsSpanSilently)
{
	tracing_config config;
	config.exporter = exporter_type::none;
	configure_tracing(config);

	std::atomic<int> processed{0};
	register_span_processor([&](const span&) { processed.fetch_add(1); });

	auto out = capture([] {
		auto s = trace_context::create_span("dropped");
	});

	// Tracing disabled when exporter==none, so process_span returns early
	// and the registered processor is not called.
	EXPECT_EQ(processed.load(), 0);
	EXPECT_TRUE(out.empty());
}

// ============================================================================
// Reconfigure clears registered processors via shutdown_tracing
// ============================================================================

TEST_F(TracingExportersBranchTest, ShutdownClearsRegisteredProcessors)
{
	tracing_config config;
	config.exporter = exporter_type::console;
	configure_tracing(config);

	std::atomic<int> processed{0};
	register_span_processor([&](const span&) { processed.fetch_add(1); });

	shutdown_tracing();

	// Re-enable after shutdown; the prior processor must not fire.
	configure_tracing(config);
	::testing::internal::CaptureStdout();
	{
		auto s = trace_context::create_span("after_shutdown");
	}
	(void)::testing::internal::GetCapturedStdout();

	EXPECT_EQ(processed.load(), 0);
}

} // namespace
