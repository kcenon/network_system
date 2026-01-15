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
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
tracing_state g_tracing_state;

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

// Process completed span
void process_span(const span& s)
{
	if (!g_tracing_state.enabled.load(std::memory_order_acquire))
	{
		return;
	}

	// Check sampling
	if (!s.context().is_sampled())
	{
		return;
	}

	// Export based on configured exporter
	switch (g_tracing_state.config.exporter)
	{
	case exporter_type::console:
		export_to_console(s);
		break;

	case exporter_type::otlp_grpc:
	case exporter_type::otlp_http:
		// TODO: Implement OTLP export
		// For now, fall back to console if debug is enabled
		if (g_tracing_state.config.debug)
		{
			export_to_console(s);
		}
		break;

	case exporter_type::jaeger:
		// TODO: Implement Jaeger export
		if (g_tracing_state.config.debug)
		{
			export_to_console(s);
		}
		break;

	case exporter_type::zipkin:
		// TODO: Implement Zipkin export
		if (g_tracing_state.config.debug)
		{
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
	std::lock_guard<std::mutex> lock(g_tracing_state.mutex);

	g_tracing_state.enabled.store(false, std::memory_order_release);
	g_tracing_state.processors.clear();
	g_tracing_state.config = tracing_config{};
}

void flush_tracing()
{
	// For console exporter, flush is immediate
	// For batch exporters, this would force a flush of the batch queue
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

} // namespace kcenon::network::tracing
