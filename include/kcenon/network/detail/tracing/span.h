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
 * @file span.h
 * @brief RAII span implementation for distributed tracing
 *
 * Provides a span class that automatically manages its lifecycle
 * and integrates with the trace context for distributed tracing.
 *
 * @author kcenon
 * @date 2025-01-15
 */

#pragma once

#include "trace_context.h"

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace kcenon::network::tracing {

/**
 * @brief Span status codes following OpenTelemetry conventions
 */
enum class span_status
{
	unset = 0, ///< Default status, span completed without explicit status
	ok = 1,    ///< Operation completed successfully
	error = 2  ///< Operation failed
};

/**
 * @brief Span kind following OpenTelemetry conventions
 */
enum class span_kind
{
	internal = 0, ///< Default, represents internal operations
	server = 1,   ///< Server-side handling of a request
	client = 2,   ///< Client-side request
	producer = 3, ///< Message producer (e.g., queue publisher)
	consumer = 4  ///< Message consumer (e.g., queue subscriber)
};

/**
 * @brief Attribute value type (supports string, int64, double, bool)
 */
using attribute_value = std::variant<std::string, int64_t, double, bool>;

/**
 * @brief Span event (timestamped annotation)
 */
struct span_event
{
	std::string name;
	std::chrono::steady_clock::time_point timestamp;
	std::map<std::string, attribute_value> attributes;
};

/**
 * @class span
 * @brief RAII span for distributed tracing
 *
 * A span represents a single operation within a trace. It has a name,
 * start and end time, attributes, and can have events recorded during
 * its lifetime. The span automatically ends when destroyed (RAII).
 *
 * Spans maintain the current trace context via thread-local storage,
 * enabling automatic parent-child relationship tracking.
 *
 * @par Example usage:
 * @code
 * void process_request() {
 *     auto span = trace_context::create_span("process_request");
 *     span.set_attribute("request.id", "12345");
 *
 *     // Do work...
 *     span.add_event("processing_started");
 *
 *     if (error) {
 *         span.set_error("Failed to process request");
 *     }
 *     // span automatically ends on destruction
 * }
 * @endcode
 *
 * @par RAII Macro:
 * @code
 * NETWORK_TRACE_SPAN("operation.name");
 * _span.set_attribute("key", "value");
 * @endcode
 *
 * @see trace_context
 */
class span
{
public:
	/**
	 * @brief Construct a new span
	 * @param name Span name (operation name)
	 * @param ctx Trace context for this span
	 * @param kind Kind of span (default: internal)
	 */
	explicit span(std::string_view name, trace_context ctx,
	              span_kind kind = span_kind::internal);

	/**
	 * @brief Destructor - automatically ends the span if not ended
	 */
	~span();

	// Non-copyable
	span(const span&) = delete;
	auto operator=(const span&) -> span& = delete;

	// Movable
	span(span&& other) noexcept;
	auto operator=(span&& other) noexcept -> span&;

	/**
	 * @brief Set a string attribute
	 * @param key Attribute key
	 * @param value Attribute value
	 * @return Reference to this span for chaining
	 */
	auto set_attribute(std::string_view key, std::string_view value) -> span&;

	/**
	 * @brief Set a string attribute (const char* overload)
	 * @param key Attribute key
	 * @param value Attribute value
	 * @return Reference to this span for chaining
	 *
	 * This overload prevents implicit conversion of const char* to bool.
	 */
	auto set_attribute(std::string_view key, const char* value) -> span&;

	/**
	 * @brief Set an integer attribute
	 * @param key Attribute key
	 * @param value Attribute value
	 * @return Reference to this span for chaining
	 */
	auto set_attribute(std::string_view key, int64_t value) -> span&;

	/**
	 * @brief Set a double attribute
	 * @param key Attribute key
	 * @param value Attribute value
	 * @return Reference to this span for chaining
	 */
	auto set_attribute(std::string_view key, double value) -> span&;

	/**
	 * @brief Set a boolean attribute
	 * @param key Attribute key
	 * @param value Attribute value
	 * @return Reference to this span for chaining
	 */
	auto set_attribute(std::string_view key, bool value) -> span&;

	/**
	 * @brief Add an event to the span
	 * @param name Event name
	 * @return Reference to this span for chaining
	 */
	auto add_event(std::string_view name) -> span&;

	/**
	 * @brief Add an event with attributes
	 * @param name Event name
	 * @param attributes Event attributes
	 * @return Reference to this span for chaining
	 */
	auto add_event(std::string_view name,
	               const std::map<std::string, attribute_value>& attributes)
	    -> span&;

	/**
	 * @brief Set the span status
	 * @param status Status code
	 * @return Reference to this span for chaining
	 */
	auto set_status(span_status status) -> span&;

	/**
	 * @brief Set the span status with description
	 * @param status Status code
	 * @param description Status description
	 * @return Reference to this span for chaining
	 */
	auto set_status(span_status status, std::string_view description) -> span&;

	/**
	 * @brief Set error status with message
	 * @param message Error message
	 * @return Reference to this span for chaining
	 */
	auto set_error(std::string_view message) -> span&;

	/**
	 * @brief Manually end the span
	 *
	 * After calling end(), the span will not be ended again in destructor.
	 */
	void end();

	/**
	 * @brief Check if the span has ended
	 * @return true if the span has ended
	 */
	[[nodiscard]] auto is_ended() const noexcept -> bool;

	/**
	 * @brief Get the trace context for this span
	 * @return Trace context
	 */
	[[nodiscard]] auto context() const noexcept -> const trace_context&;

	/**
	 * @brief Get the span name
	 * @return Span name
	 */
	[[nodiscard]] auto name() const noexcept -> const std::string&;

	/**
	 * @brief Get the span kind
	 * @return Span kind
	 */
	[[nodiscard]] auto kind() const noexcept -> span_kind;

	/**
	 * @brief Get the span status
	 * @return Span status
	 */
	[[nodiscard]] auto status() const noexcept -> span_status;

	/**
	 * @brief Get the status description
	 * @return Status description (empty if not set)
	 */
	[[nodiscard]] auto status_description() const noexcept -> const std::string&;

	/**
	 * @brief Get the span attributes
	 * @return Map of attributes
	 */
	[[nodiscard]] auto attributes() const noexcept
	    -> const std::map<std::string, attribute_value>&;

	/**
	 * @brief Get the span events
	 * @return Vector of events
	 */
	[[nodiscard]] auto events() const noexcept -> const std::vector<span_event>&;

	/**
	 * @brief Get the span start time
	 * @return Start timestamp
	 */
	[[nodiscard]] auto start_time() const noexcept
	    -> std::chrono::steady_clock::time_point;

	/**
	 * @brief Get the span end time
	 * @return End timestamp (start_time if not ended)
	 */
	[[nodiscard]] auto end_time() const noexcept
	    -> std::chrono::steady_clock::time_point;

	/**
	 * @brief Get the span duration
	 * @return Duration in nanoseconds
	 */
	[[nodiscard]] auto duration() const noexcept -> std::chrono::nanoseconds;

private:
	struct impl;
	std::unique_ptr<impl> impl_;
};

/**
 * @brief RAII helper macro for creating spans
 *
 * Creates a span named '_span' that automatically ends on scope exit.
 *
 * @param name Span name (string literal)
 *
 * @par Example:
 * @code
 * void my_function() {
 *     NETWORK_TRACE_SPAN("my_function");
 *     _span.set_attribute("key", "value");
 *     // span ends when function returns
 * }
 * @endcode
 */
#define NETWORK_TRACE_SPAN(name) \
	auto _span = ::kcenon::network::tracing::trace_context::create_span(name)

/**
 * @brief RAII helper macro for creating client spans
 */
#define NETWORK_TRACE_CLIENT_SPAN(name)                                     \
	auto _span = ::kcenon::network::tracing::span(                          \
	    name, ::kcenon::network::tracing::trace_context::current().is_valid() \
	              ? ::kcenon::network::tracing::trace_context::current()      \
	                    .create_child_span(name)                              \
	                    .context()                                            \
	              : ::kcenon::network::tracing::trace_context(),              \
	    ::kcenon::network::tracing::span_kind::client)

/**
 * @brief RAII helper macro for creating server spans
 */
#define NETWORK_TRACE_SERVER_SPAN(name)                                     \
	auto _span = ::kcenon::network::tracing::span(                          \
	    name, ::kcenon::network::tracing::trace_context::current().is_valid() \
	              ? ::kcenon::network::tracing::trace_context::current()      \
	                    .create_child_span(name)                              \
	                    .context()                                            \
	              : ::kcenon::network::tracing::trace_context(),              \
	    ::kcenon::network::tracing::span_kind::server)

} // namespace kcenon::network::tracing
