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
 * @file trace_context.h
 * @brief Distributed tracing context for OpenTelemetry-compatible tracing
 *
 * Provides trace context management with W3C Trace Context propagation
 * support for distributed tracing across network operations.
 *
 * @author kcenon
 * @date 2025-01-15
 */

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kcenon::network::tracing {

// Forward declarations
class span;

/**
 * @brief Trace ID type (128-bit identifier)
 */
using trace_id_t = std::array<uint8_t, 16>;

/**
 * @brief Span ID type (64-bit identifier)
 */
using span_id_t = std::array<uint8_t, 8>;

/**
 * @brief Trace flags (8-bit)
 */
enum class trace_flags : uint8_t
{
	none = 0x00,
	sampled = 0x01
};

/**
 * @class trace_context
 * @brief Immutable trace context for distributed tracing
 *
 * This class represents a W3C Trace Context compatible trace context
 * that can be propagated across network boundaries. It stores trace ID,
 * span ID, parent span ID, and sampling decision.
 *
 * Thread-local storage is used to maintain the current trace context,
 * enabling automatic context propagation within a thread.
 *
 * @par Example usage:
 * @code
 * // Create a new root span
 * auto span = trace_context::create_span("http.request");
 *
 * // Get current context for propagation
 * auto ctx = trace_context::current();
 * auto headers = ctx.to_headers();
 *
 * // Parse context from incoming request
 * auto parsed_ctx = trace_context::from_headers(incoming_headers);
 * @endcode
 *
 * @see span
 * @see https://www.w3.org/TR/trace-context/
 */
class trace_context
{
public:
	/**
	 * @brief Default constructor creates an invalid context
	 */
	trace_context() = default;

	/**
	 * @brief Construct a trace context with all components
	 * @param trace_id 128-bit trace identifier
	 * @param span_id 64-bit span identifier
	 * @param flags Trace flags (sampling decision)
	 * @param parent_span_id Optional parent span identifier
	 */
	trace_context(trace_id_t trace_id, span_id_t span_id, trace_flags flags,
	              std::optional<span_id_t> parent_span_id = std::nullopt);

	/**
	 * @brief Get the current trace context from thread-local storage
	 * @return Current trace context, or invalid context if none set
	 */
	[[nodiscard]] static auto current() -> trace_context;

	/**
	 * @brief Create a new root span with a new trace context
	 * @param name Name of the span
	 * @return New span with a fresh trace context
	 */
	[[nodiscard]] static auto create_span(std::string_view name) -> span;

	/**
	 * @brief Create a child span inheriting this context
	 * @param name Name of the child span
	 * @return New span that is a child of this context
	 */
	[[nodiscard]] auto create_child_span(std::string_view name) const -> span;

	/**
	 * @brief Convert context to W3C traceparent header value
	 * @return traceparent header value string
	 *
	 * Format: {version}-{trace-id}-{parent-id}-{trace-flags}
	 * Example: 00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01
	 */
	[[nodiscard]] auto to_traceparent() const -> std::string;

	/**
	 * @brief Convert context to HTTP headers for propagation
	 * @return Vector of header name-value pairs
	 *
	 * Returns headers conforming to W3C Trace Context specification.
	 */
	[[nodiscard]] auto to_headers() const
	    -> std::vector<std::pair<std::string, std::string>>;

	/**
	 * @brief Parse trace context from W3C traceparent header
	 * @param traceparent The traceparent header value
	 * @return Parsed trace context, or invalid context if parsing fails
	 */
	[[nodiscard]] static auto from_traceparent(std::string_view traceparent)
	    -> trace_context;

	/**
	 * @brief Parse trace context from HTTP headers
	 * @param headers Vector of header name-value pairs
	 * @return Parsed trace context, or invalid context if not found
	 */
	[[nodiscard]] static auto from_headers(
	    const std::vector<std::pair<std::string, std::string>>& headers)
	    -> trace_context;

	/**
	 * @brief Check if this context is valid
	 * @return true if the context has valid trace and span IDs
	 */
	[[nodiscard]] auto is_valid() const noexcept -> bool;

	/**
	 * @brief Check if this trace is sampled
	 * @return true if the trace should be recorded
	 */
	[[nodiscard]] auto is_sampled() const noexcept -> bool;

	/**
	 * @brief Get the trace ID
	 * @return 128-bit trace identifier
	 */
	[[nodiscard]] auto trace_id() const noexcept -> const trace_id_t&;

	/**
	 * @brief Get the span ID
	 * @return 64-bit span identifier
	 */
	[[nodiscard]] auto span_id() const noexcept -> const span_id_t&;

	/**
	 * @brief Get the parent span ID
	 * @return Optional parent span identifier
	 */
	[[nodiscard]] auto parent_span_id() const noexcept
	    -> const std::optional<span_id_t>&;

	/**
	 * @brief Get the trace flags
	 * @return Trace flags
	 */
	[[nodiscard]] auto flags() const noexcept -> trace_flags;

	/**
	 * @brief Convert trace ID to hex string
	 * @return 32-character lowercase hex string
	 */
	[[nodiscard]] auto trace_id_hex() const -> std::string;

	/**
	 * @brief Convert span ID to hex string
	 * @return 16-character lowercase hex string
	 */
	[[nodiscard]] auto span_id_hex() const -> std::string;

	/**
	 * @brief Equality comparison
	 */
	[[nodiscard]] auto operator==(const trace_context& other) const noexcept
	    -> bool;

	/**
	 * @brief Inequality comparison
	 */
	[[nodiscard]] auto operator!=(const trace_context& other) const noexcept
	    -> bool;

private:
	trace_id_t trace_id_{};
	span_id_t span_id_{};
	std::optional<span_id_t> parent_span_id_;
	trace_flags flags_{trace_flags::none};
	bool valid_{false};

	/**
	 * @brief Set the current thread-local trace context
	 * @param ctx Context to set
	 */
	static void set_current(const trace_context& ctx);

	/**
	 * @brief Clear the current thread-local trace context
	 */
	static void clear_current();

	// span class needs access to set_current
	friend class span;
};

/**
 * @brief Generate a random trace ID
 * @return Randomly generated 128-bit trace ID
 */
[[nodiscard]] auto generate_trace_id() -> trace_id_t;

/**
 * @brief Generate a random span ID
 * @return Randomly generated 64-bit span ID
 */
[[nodiscard]] auto generate_span_id() -> span_id_t;

/**
 * @brief Convert bytes to lowercase hex string
 * @param data Byte array
 * @param size Number of bytes
 * @return Hex string representation
 */
[[nodiscard]] auto bytes_to_hex(const uint8_t* data, size_t size) -> std::string;

/**
 * @brief Parse hex string to bytes
 * @param hex Hex string
 * @param out Output byte array
 * @param size Expected number of bytes
 * @return true if parsing succeeded
 */
[[nodiscard]] auto hex_to_bytes(std::string_view hex, uint8_t* out, size_t size)
    -> bool;

} // namespace kcenon::network::tracing
