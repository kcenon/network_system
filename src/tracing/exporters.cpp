/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/**
 * @file exporters.cpp
 * @brief Implementation of tracing configuration and exporters
 */

#include "kcenon/network/tracing/tracing_config.h"
#include "kcenon/network/tracing/span.h"
#include "kcenon/network/tracing/trace_context.h"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

namespace kcenon::network::tracing {

namespace {

// Global tracing state
struct tracing_state
{
	std::atomic<bool> enabled{false};
	tracing_config config;
	std::vector<span_processor_callback> processors;
	std::mutex mutex;

	// Batch queue for async export
	std::vector<std::string> batch_queue;
	std::mutex batch_mutex;
	std::atomic<size_t> queued_count{0};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
tracing_state g_tracing_state;

// Sampling decision based on trace ID
auto should_sample(const trace_context& ctx, double sample_rate) -> bool
{
	if (sample_rate >= 1.0)
	{
		return true;
	}
	if (sample_rate <= 0.0)
	{
		return false;
	}

	// Use first 8 bytes of trace ID for consistent sampling
	const auto& trace_id = ctx.trace_id();
	uint64_t hash = 0;
	for (size_t i = 0; i < 8 && i < trace_id.size(); ++i)
	{
		hash = (hash << 8) | trace_id[i];
	}

	// Normalize to 0.0-1.0 range
	double normalized = static_cast<double>(hash) / static_cast<double>(UINT64_MAX);
	return normalized < sample_rate;
}

// Convert attribute value to string
auto attribute_to_string(const attribute_value& value) -> std::string
{
	return std::visit(
	    [](auto&& arg) -> std::string
	    {
		    using T = std::decay_t<decltype(arg)>;
		    if constexpr (std::is_same_v<T, std::string>)
		    {
			    return "\"" + arg + "\"";
		    }
		    else if constexpr (std::is_same_v<T, bool>)
		    {
			    return arg ? "true" : "false";
		    }
		    else if constexpr (std::is_same_v<T, int64_t>)
		    {
			    return std::to_string(arg);
		    }
		    else if constexpr (std::is_same_v<T, double>)
		    {
			    std::ostringstream oss;
			    oss << std::fixed << std::setprecision(3) << arg;
			    return oss.str();
		    }
		    else
		    {
			    return "unknown";
		    }
	    },
	    value);
}

// Convert span kind to string
auto span_kind_to_string(span_kind kind) -> std::string_view
{
	switch (kind)
	{
	case span_kind::internal:
		return "INTERNAL";
	case span_kind::server:
		return "SERVER";
	case span_kind::client:
		return "CLIENT";
	case span_kind::producer:
		return "PRODUCER";
	case span_kind::consumer:
		return "CONSUMER";
	}
	return "UNKNOWN";
}

// Convert span status to string
auto span_status_to_string(span_status status) -> std::string_view
{
	switch (status)
	{
	case span_status::unset:
		return "UNSET";
	case span_status::ok:
		return "OK";
	case span_status::error:
		return "ERROR";
	}
	return "UNKNOWN";
}

// Console exporter implementation
void export_to_console(const span& s)
{
	const auto& ctx = s.context();
	auto duration_ns = s.duration().count();
	double duration_ms = static_cast<double>(duration_ns) / 1'000'000.0;

	std::ostringstream oss;
	oss << "\n";
	oss << "=== SPAN ===\n";
	oss << "Name:      " << s.name() << "\n";
	oss << "Trace ID:  " << ctx.trace_id_hex() << "\n";
	oss << "Span ID:   " << ctx.span_id_hex() << "\n";

	if (ctx.parent_span_id().has_value())
	{
		oss << "Parent ID: " << bytes_to_hex(ctx.parent_span_id()->data(), 8) << "\n";
	}

	oss << "Kind:      " << span_kind_to_string(s.kind()) << "\n";
	oss << "Status:    " << span_status_to_string(s.status());

	if (!s.status_description().empty())
	{
		oss << " (" << s.status_description() << ")";
	}
	oss << "\n";

	oss << std::fixed << std::setprecision(3);
	oss << "Duration:  " << duration_ms << " ms\n";

	// Attributes
	const auto& attrs = s.attributes();
	if (!attrs.empty())
	{
		oss << "Attributes:\n";
		for (const auto& [key, value] : attrs)
		{
			oss << "  " << key << ": " << attribute_to_string(value) << "\n";
		}
	}

	// Events
	const auto& events = s.events();
	if (!events.empty())
	{
		oss << "Events:\n";
		for (const auto& event : events)
		{
			oss << "  - " << event.name;
			if (!event.attributes.empty())
			{
				oss << " {";
				bool first = true;
				for (const auto& [key, value] : event.attributes)
				{
					if (!first)
					{
						oss << ", ";
					}
					oss << key << ": " << attribute_to_string(value);
					first = false;
				}
				oss << "}";
			}
			oss << "\n";
		}
	}

	oss << "============\n";

	std::cout << oss.str() << std::flush;
}

// Escape JSON string
auto json_escape(const std::string& s) -> std::string
{
	std::ostringstream oss;
	for (char c : s)
	{
		switch (c)
		{
		case '"':
			oss << "\\\"";
			break;
		case '\\':
			oss << "\\\\";
			break;
		case '\b':
			oss << "\\b";
			break;
		case '\f':
			oss << "\\f";
			break;
		case '\n':
			oss << "\\n";
			break;
		case '\r':
			oss << "\\r";
			break;
		case '\t':
			oss << "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20)
			{
				oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
				    << static_cast<int>(c);
			}
			else
			{
				oss << c;
			}
		}
	}
	return oss.str();
}

// Convert attribute value to JSON
auto attribute_to_json(const attribute_value& value) -> std::string
{
	return std::visit(
	    [](auto&& arg) -> std::string
	    {
		    using T = std::decay_t<decltype(arg)>;
		    if constexpr (std::is_same_v<T, std::string>)
		    {
			    return "{\"stringValue\":\"" + json_escape(arg) + "\"}";
		    }
		    else if constexpr (std::is_same_v<T, bool>)
		    {
			    return std::string("{\"boolValue\":") + (arg ? "true" : "false") + "}";
		    }
		    else if constexpr (std::is_same_v<T, int64_t>)
		    {
			    return "{\"intValue\":\"" + std::to_string(arg) + "\"}";
		    }
		    else if constexpr (std::is_same_v<T, double>)
		    {
			    std::ostringstream oss;
			    oss << std::fixed << std::setprecision(6) << arg;
			    return "{\"doubleValue\":" + oss.str() + "}";
		    }
		    else
		    {
			    return "{\"stringValue\":\"unknown\"}";
		    }
	    },
	    value);
}

// Convert span to OTLP JSON format
auto span_to_otlp_json(const span& s) -> std::string
{
	const auto& ctx = s.context();
	std::ostringstream oss;

	// Get timestamps
	auto start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
	                    s.start_time().time_since_epoch())
	                    .count();
	auto end_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
	                  s.end_time().time_since_epoch())
	                  .count();

	oss << "{";
	oss << "\"traceId\":\"" << ctx.trace_id_hex() << "\",";
	oss << "\"spanId\":\"" << ctx.span_id_hex() << "\",";

	if (ctx.parent_span_id().has_value())
	{
		oss << "\"parentSpanId\":\""
		    << bytes_to_hex(ctx.parent_span_id()->data(), 8) << "\",";
	}

	oss << "\"name\":\"" << json_escape(s.name()) << "\",";
	oss << "\"kind\":" << (static_cast<int>(s.kind()) + 1) << ",";
	oss << "\"startTimeUnixNano\":\"" << start_ns << "\",";
	oss << "\"endTimeUnixNano\":\"" << end_ns << "\",";

	// Attributes
	oss << "\"attributes\":[";
	const auto& attrs = s.attributes();
	bool first = true;
	for (const auto& [key, value] : attrs)
	{
		if (!first)
		{
			oss << ",";
		}
		oss << "{\"key\":\"" << json_escape(key)
		    << "\",\"value\":" << attribute_to_json(value) << "}";
		first = false;
	}
	oss << "],";

	// Events
	oss << "\"events\":[";
	const auto& events = s.events();
	first = true;
	for (const auto& event : events)
	{
		if (!first)
		{
			oss << ",";
		}
		auto event_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
		                    event.timestamp.time_since_epoch())
		                    .count();
		oss << "{\"name\":\"" << json_escape(event.name) << "\",";
		oss << "\"timeUnixNano\":\"" << event_ns << "\",";
		oss << "\"attributes\":[";
		bool attr_first = true;
		for (const auto& [key, value] : event.attributes)
		{
			if (!attr_first)
			{
				oss << ",";
			}
			oss << "{\"key\":\"" << json_escape(key)
			    << "\",\"value\":" << attribute_to_json(value) << "}";
			attr_first = false;
		}
		oss << "]}";
		first = false;
	}
	oss << "],";

	// Status
	oss << "\"status\":{";
	if (s.status() == span_status::error)
	{
		oss << "\"code\":2";
		if (!s.status_description().empty())
		{
			oss << ",\"message\":\"" << json_escape(s.status_description()) << "\"";
		}
	}
	else if (s.status() == span_status::ok)
	{
		oss << "\"code\":1";
	}
	else
	{
		oss << "\"code\":0";
	}
	oss << "}";

	oss << "}";

	return oss.str();
}

// Build OTLP export request body
auto build_otlp_request(const std::vector<std::string>& spans_json,
                        const tracing_config& config) -> std::string
{
	std::ostringstream oss;

	oss << "{\"resourceSpans\":[{";

	// Resource
	oss << "\"resource\":{\"attributes\":[";
	oss << "{\"key\":\"service.name\",\"value\":{\"stringValue\":\""
	    << json_escape(config.service_name) << "\"}}";

	if (!config.service_version.empty())
	{
		oss << ",{\"key\":\"service.version\",\"value\":{\"stringValue\":\""
		    << json_escape(config.service_version) << "\"}}";
	}

	if (!config.service_namespace.empty())
	{
		oss << ",{\"key\":\"service.namespace\",\"value\":{\"stringValue\":\""
		    << json_escape(config.service_namespace) << "\"}}";
	}

	if (!config.service_instance_id.empty())
	{
		oss << ",{\"key\":\"service.instance.id\",\"value\":{\"stringValue\":\""
		    << json_escape(config.service_instance_id) << "\"}}";
	}

	for (const auto& [key, value] : config.resource_attributes)
	{
		oss << ",{\"key\":\"" << json_escape(key)
		    << "\",\"value\":{\"stringValue\":\"" << json_escape(value) << "\"}}";
	}

	oss << "]},";

	// Scope spans
	oss << "\"scopeSpans\":[{";
	oss << "\"scope\":{\"name\":\"network_system.tracing\",\"version\":\"1.0.0\"},";
	oss << "\"spans\":[";

	bool first = true;
	for (const auto& span_json : spans_json)
	{
		if (!first)
		{
			oss << ",";
		}
		oss << span_json;
		first = false;
	}

	oss << "]}]}]}";

	return oss.str();
}

// Export spans via OTLP HTTP
void export_otlp_http(const std::vector<std::string>& spans_json)
{
	if (spans_json.empty())
	{
		return;
	}

	const auto& config = g_tracing_state.config;
	std::string body = build_otlp_request(spans_json, config);

	if (config.debug)
	{
		std::cout << "[TRACING] Exporting " << spans_json.size()
		          << " spans to OTLP HTTP: " << config.otlp.endpoint << "\n";
		std::cout << "[TRACING] Request body: " << body << "\n";
	}

	// Note: Full HTTP implementation would use http2_client or ASIO
	// For now, we log the export attempt when debug is enabled
	// Production implementation should use async HTTP POST
}

// Process completed span
void process_span(const span& s)
{
	if (!g_tracing_state.enabled.load(std::memory_order_acquire))
	{
		return;
	}

	const auto& config = g_tracing_state.config;

	// Check sampling based on sampler type
	bool sampled = false;
	switch (config.sampler)
	{
	case sampler_type::always_on:
		sampled = true;
		break;
	case sampler_type::always_off:
		sampled = false;
		break;
	case sampler_type::trace_id:
		sampled = should_sample(s.context(), config.sample_rate);
		break;
	case sampler_type::parent_based:
		// Use parent's sampling decision (from context)
		sampled = s.context().is_sampled();
		break;
	}

	if (!sampled)
	{
		return;
	}

	// Export based on configured exporter
	switch (config.exporter)
	{
	case exporter_type::console:
		export_to_console(s);
		break;

	case exporter_type::otlp_http:
	{
		// Convert span to JSON and queue for batch export
		std::string span_json = span_to_otlp_json(s);
		{
			std::lock_guard<std::mutex> lock(g_tracing_state.batch_mutex);
			g_tracing_state.batch_queue.push_back(std::move(span_json));
			g_tracing_state.queued_count.fetch_add(1, std::memory_order_relaxed);
		}

		// Check if batch should be exported
		if (g_tracing_state.queued_count.load(std::memory_order_relaxed) >=
		    config.batch.max_export_batch_size)
		{
			std::vector<std::string> batch;
			{
				std::lock_guard<std::mutex> lock(g_tracing_state.batch_mutex);
				batch = std::move(g_tracing_state.batch_queue);
				g_tracing_state.batch_queue.clear();
				g_tracing_state.queued_count.store(0, std::memory_order_relaxed);
			}
			export_otlp_http(batch);
		}
		break;
	}

	case exporter_type::otlp_grpc:
		// gRPC export requires protobuf, fall back to debug console
		if (config.debug)
		{
			std::cout << "[TRACING] OTLP gRPC export not implemented, "
			          << "use otlp_http instead\n";
			export_to_console(s);
		}
		break;

	case exporter_type::jaeger:
		// Jaeger export uses Thrift, fall back to debug console
		if (config.debug)
		{
			std::cout << "[TRACING] Jaeger export not implemented, "
			          << "use otlp_http with Jaeger OTLP receiver\n";
			export_to_console(s);
		}
		break;

	case exporter_type::zipkin:
		// Zipkin export requires JSON v2 format
		if (config.debug)
		{
			std::cout << "[TRACING] Zipkin export not implemented, "
			          << "use otlp_http with Zipkin OTLP receiver\n";
			export_to_console(s);
		}
		break;

	case exporter_type::none:
	default:
		// No-op
		break;
	}

	// Call registered processors
	std::lock_guard<std::mutex> lock(g_tracing_state.mutex);
	for (const auto& processor : g_tracing_state.processors)
	{
		if (processor)
		{
			processor(s);
		}
	}
}

} // anonymous namespace

void configure_tracing(const tracing_config& config)
{
	std::lock_guard<std::mutex> lock(g_tracing_state.mutex);

	g_tracing_state.config = config;
	g_tracing_state.enabled.store(config.exporter != exporter_type::none,
	                               std::memory_order_release);

	if (config.debug && config.exporter != exporter_type::none)
	{
		std::cout << "[TRACING] Configured with exporter: ";
		switch (config.exporter)
		{
		case exporter_type::console:
			std::cout << "console";
			break;
		case exporter_type::otlp_grpc:
			std::cout << "otlp_grpc (" << config.otlp.endpoint << ")";
			break;
		case exporter_type::otlp_http:
			std::cout << "otlp_http (" << config.otlp.endpoint << ")";
			break;
		case exporter_type::jaeger:
			std::cout << "jaeger (" << config.jaeger_endpoint << ")";
			break;
		case exporter_type::zipkin:
			std::cout << "zipkin (" << config.zipkin_endpoint << ")";
			break;
		default:
			std::cout << "none";
			break;
		}
		std::cout << ", service: " << config.service_name
		          << ", sample_rate: " << config.sample_rate << "\n";
	}
}

void shutdown_tracing()
{
	// Flush any pending spans before shutdown
	flush_tracing();

	std::lock_guard<std::mutex> lock(g_tracing_state.mutex);

	g_tracing_state.enabled.store(false, std::memory_order_release);
	g_tracing_state.processors.clear();
	g_tracing_state.config = tracing_config{};

	// Clear batch queue
	{
		std::lock_guard<std::mutex> batch_lock(g_tracing_state.batch_mutex);
		g_tracing_state.batch_queue.clear();
		g_tracing_state.queued_count.store(0, std::memory_order_relaxed);
	}
}

void flush_tracing()
{
	// Flush batch queue
	std::vector<std::string> batch;
	{
		std::lock_guard<std::mutex> lock(g_tracing_state.batch_mutex);
		if (!g_tracing_state.batch_queue.empty())
		{
			batch = std::move(g_tracing_state.batch_queue);
			g_tracing_state.batch_queue.clear();
			g_tracing_state.queued_count.store(0, std::memory_order_relaxed);
		}
	}

	if (!batch.empty())
	{
		export_otlp_http(batch);
	}

	std::cout << std::flush;
}

auto is_tracing_enabled() -> bool
{
	return g_tracing_state.enabled.load(std::memory_order_acquire);
}

void register_span_processor(span_processor_callback callback)
{
	if (!callback)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(g_tracing_state.mutex);
	g_tracing_state.processors.push_back(std::move(callback));
}

void export_span(const span& s)
{
	process_span(s);
}

} // namespace kcenon::network::tracing
