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
 * @file tracing_config.h
 * @brief Configuration structures for OpenTelemetry tracing
 *
 * Provides configuration options for tracing exporters, sampling,
 * and service identification.
 *
 * @author kcenon
 * @date 2025-01-15
 */

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <string>

namespace kcenon::network::tracing {

// Forward declarations
class span;
struct span_event;

/**
 * @brief Exporter types for trace data
 */
enum class exporter_type
{
	none,      ///< Tracing disabled
	console,   ///< Console/stdout output (for debugging)
	otlp_grpc, ///< OTLP over gRPC (OpenTelemetry Collector)
	otlp_http, ///< OTLP over HTTP (OpenTelemetry Collector)
	jaeger,    ///< Jaeger native format
	zipkin     ///< Zipkin format
};

/**
 * @brief Sampler types for trace sampling decisions
 */
enum class sampler_type
{
	always_on,    ///< Sample all traces
	always_off,   ///< Sample no traces
	trace_id,     ///< Sample based on trace ID ratio
	parent_based  ///< Sample based on parent span's sampling decision
};

/**
 * @brief Batch export configuration
 */
struct batch_config
{
	/**
	 * @brief Maximum number of spans to batch before export
	 * @default 512
	 */
	size_t max_queue_size = 512;

	/**
	 * @brief Maximum time to wait before exporting a batch
	 * @default 5000ms
	 */
	std::chrono::milliseconds schedule_delay{5000};

	/**
	 * @brief Maximum batch size for a single export
	 * @default 512
	 */
	size_t max_export_batch_size = 512;

	/**
	 * @brief Timeout for export operations
	 * @default 30000ms
	 */
	std::chrono::milliseconds export_timeout{30000};
};

/**
 * @brief OTLP exporter configuration
 */
struct otlp_config
{
	/**
	 * @brief Endpoint URL for OTLP exporter
	 * @default "http://localhost:4317" for gRPC, "http://localhost:4318" for HTTP
	 */
	std::string endpoint;

	/**
	 * @brief Custom headers for OTLP requests
	 */
	std::map<std::string, std::string> headers;

	/**
	 * @brief Connection timeout
	 * @default 10000ms
	 */
	std::chrono::milliseconds timeout{10000};

	/**
	 * @brief Use insecure connection (no TLS)
	 * @default false
	 */
	bool insecure = false;

	/**
	 * @brief Certificate file path for TLS (optional)
	 */
	std::string certificate_path;
};

/**
 * @struct tracing_config
 * @brief Main configuration structure for tracing
 *
 * This structure contains all configuration options for the tracing system,
 * including exporter selection, sampling configuration, and service metadata.
 *
 * @par Example usage:
 * @code
 * tracing_config config;
 * config.service_name = "my-service";
 * config.exporter = exporter_type::otlp_grpc;
 * config.otlp.endpoint = "http://otel-collector:4317";
 * config.sample_rate = 0.1;  // Sample 10% of traces
 *
 * configure_tracing(config);
 * @endcode
 */
struct tracing_config
{
	/**
	 * @brief Exporter type to use
	 * @default exporter_type::none
	 */
	exporter_type exporter = exporter_type::none;

	/**
	 * @brief Service name for trace identification
	 * @default "network_system"
	 */
	std::string service_name = "network_system";

	/**
	 * @brief Service version
	 * @default ""
	 */
	std::string service_version;

	/**
	 * @brief Service namespace
	 * @default ""
	 */
	std::string service_namespace;

	/**
	 * @brief Service instance ID (unique per instance)
	 * @default "" (auto-generated if empty)
	 */
	std::string service_instance_id;

	/**
	 * @brief Additional resource attributes
	 */
	std::map<std::string, std::string> resource_attributes;

	/**
	 * @brief Sampler type to use
	 * @default sampler_type::always_on
	 */
	sampler_type sampler = sampler_type::always_on;

	/**
	 * @brief Sampling rate (0.0 to 1.0)
	 *
	 * Only used when sampler is trace_id.
	 * @default 1.0 (sample all traces)
	 */
	double sample_rate = 1.0;

	/**
	 * @brief OTLP exporter configuration
	 */
	otlp_config otlp;

	/**
	 * @brief Jaeger exporter endpoint
	 * @default "http://localhost:14268/api/traces"
	 */
	std::string jaeger_endpoint = "http://localhost:14268/api/traces";

	/**
	 * @brief Zipkin exporter endpoint
	 * @default "http://localhost:9411/api/v2/spans"
	 */
	std::string zipkin_endpoint = "http://localhost:9411/api/v2/spans";

	/**
	 * @brief Batch export configuration
	 */
	batch_config batch;

	/**
	 * @brief Enable debug output
	 * @default false
	 */
	bool debug = false;

	/**
	 * @brief Create default configuration with console exporter
	 * @return Configuration for console output
	 */
	[[nodiscard]] static auto console() -> tracing_config
	{
		tracing_config config;
		config.exporter = exporter_type::console;
		return config;
	}

	/**
	 * @brief Create default configuration for OTLP gRPC exporter
	 * @param endpoint OTLP endpoint URL
	 * @return Configuration for OTLP gRPC export
	 */
	[[nodiscard]] static auto otlp_grpc(
	    const std::string& endpoint = "http://localhost:4317") -> tracing_config
	{
		tracing_config config;
		config.exporter = exporter_type::otlp_grpc;
		config.otlp.endpoint = endpoint;
		return config;
	}

	/**
	 * @brief Create default configuration for OTLP HTTP exporter
	 * @param endpoint OTLP endpoint URL
	 * @return Configuration for OTLP HTTP export
	 */
	[[nodiscard]] static auto otlp_http(
	    const std::string& endpoint = "http://localhost:4318") -> tracing_config
	{
		tracing_config config;
		config.exporter = exporter_type::otlp_http;
		config.otlp.endpoint = endpoint;
		return config;
	}

	/**
	 * @brief Create default configuration for Jaeger exporter
	 * @param endpoint Jaeger collector endpoint
	 * @return Configuration for Jaeger export
	 */
	[[nodiscard]] static auto jaeger(
	    const std::string& endpoint = "http://localhost:14268/api/traces")
	    -> tracing_config
	{
		tracing_config config;
		config.exporter = exporter_type::jaeger;
		config.jaeger_endpoint = endpoint;
		return config;
	}

	/**
	 * @brief Create disabled tracing configuration
	 * @return Configuration with tracing disabled
	 */
	[[nodiscard]] static auto disabled() -> tracing_config
	{
		return tracing_config{};
	}
};

/**
 * @brief Span processor callback type
 *
 * Called when a span ends. Can be used for custom export or processing.
 */
using span_processor_callback = std::function<void(const span&)>;

/**
 * @brief Initialize the tracing system with configuration
 * @param config Tracing configuration
 *
 * This function must be called before creating any spans.
 * It initializes the tracing system with the specified configuration.
 *
 * @par Example:
 * @code
 * tracing_config config;
 * config.exporter = exporter_type::otlp_grpc;
 * config.otlp.endpoint = "http://localhost:4317";
 * config.service_name = "my-service";
 *
 * configure_tracing(config);
 * @endcode
 */
void configure_tracing(const tracing_config& config);

/**
 * @brief Shutdown the tracing system
 *
 * Flushes any pending spans and releases resources.
 * Should be called before application exit.
 *
 * @par Example:
 * @code
 * // At application shutdown
 * shutdown_tracing();
 * @endcode
 */
void shutdown_tracing();

/**
 * @brief Force flush all pending spans
 *
 * Blocks until all pending spans are exported.
 */
void flush_tracing();

/**
 * @brief Check if tracing is enabled
 * @return true if tracing is configured and enabled
 */
[[nodiscard]] auto is_tracing_enabled() -> bool;

/**
 * @brief Register a custom span processor
 * @param callback Callback function called when spans end
 *
 * Can be used to implement custom export logic or additional processing.
 */
void register_span_processor(span_processor_callback callback);

} // namespace kcenon::network::tracing
