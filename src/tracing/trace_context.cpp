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
 * @file trace_context.cpp
 * @brief Implementation of trace context for distributed tracing
 */

#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/span.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <random>
#include <sstream>

namespace kcenon::network::tracing {

namespace {

// Thread-local storage for current trace context
thread_local trace_context tls_current_context;

// Hex character lookup table
constexpr std::array<char, 16> hex_chars = {'0', '1', '2', '3', '4', '5', '6', '7',
                                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

// Convert single hex character to value
auto hex_char_to_value(char c) -> int
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 10;
	}
	return -1;
}

// Check if trace ID is all zeros (invalid)
auto is_zero_trace_id(const trace_id_t& id) -> bool
{
	return std::all_of(id.begin(), id.end(), [](uint8_t b) { return b == 0; });
}

// Check if span ID is all zeros (invalid)
auto is_zero_span_id(const span_id_t& id) -> bool
{
	return std::all_of(id.begin(), id.end(), [](uint8_t b) { return b == 0; });
}

// Thread-safe random number generator
auto get_random_bytes(uint8_t* buffer, size_t size) -> void
{
	static thread_local std::random_device rd;
	static thread_local std::mt19937_64 gen(rd());
	static thread_local std::uniform_int_distribution<uint64_t> dist;

	size_t offset = 0;
	while (offset < size)
	{
		uint64_t value = dist(gen);
		size_t to_copy = std::min(sizeof(value), size - offset);
		std::memcpy(buffer + offset, &value, to_copy);
		offset += to_copy;
	}
}

} // anonymous namespace

trace_context::trace_context(trace_id_t trace_id, span_id_t span_id, trace_flags flags,
                             std::optional<span_id_t> parent_span_id)
    : trace_id_(trace_id)
    , span_id_(span_id)
    , parent_span_id_(std::move(parent_span_id))
    , flags_(flags)
    , valid_(!is_zero_trace_id(trace_id) && !is_zero_span_id(span_id))
{
}

auto trace_context::current() -> trace_context
{
	return tls_current_context;
}

void trace_context::set_current(const trace_context& ctx)
{
	tls_current_context = ctx;
}

void trace_context::clear_current()
{
	tls_current_context = trace_context{};
}

auto trace_context::create_span(std::string_view name) -> span
{
	// If there's a current context, create a child span
	if (tls_current_context.is_valid())
	{
		return tls_current_context.create_child_span(name);
	}

	// Create a new root span with a fresh trace context
	auto new_trace_id = generate_trace_id();
	auto new_span_id = generate_span_id();

	trace_context new_ctx(new_trace_id, new_span_id, trace_flags::sampled);
	return span(name, new_ctx);
}

auto trace_context::create_child_span(std::string_view name) const -> span
{
	if (!is_valid())
	{
		// If parent context is invalid, create a new root span
		auto new_trace_id = generate_trace_id();
		auto new_span_id = generate_span_id();

		trace_context new_ctx(new_trace_id, new_span_id, trace_flags::sampled);
		return span(name, new_ctx);
	}

	// Create child span with same trace ID but new span ID
	auto new_span_id = generate_span_id();
	trace_context child_ctx(trace_id_, new_span_id, flags_, span_id_);

	return span(name, child_ctx);
}

auto trace_context::to_traceparent() const -> std::string
{
	if (!is_valid())
	{
		return {};
	}

	// Format: {version}-{trace-id}-{parent-id}-{trace-flags}
	// Example: 00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01
	std::ostringstream oss;
	oss << "00-";
	oss << trace_id_hex();
	oss << "-";
	oss << span_id_hex();
	oss << "-";
	oss << (is_sampled() ? "01" : "00");

	return oss.str();
}

auto trace_context::to_headers() const
    -> std::vector<std::pair<std::string, std::string>>
{
	std::vector<std::pair<std::string, std::string>> headers;

	if (is_valid())
	{
		headers.emplace_back("traceparent", to_traceparent());
	}

	return headers;
}

auto trace_context::from_traceparent(std::string_view traceparent) -> trace_context
{
	// Minimum length check: 00-{32}-{16}-{2} = 55 characters
	if (traceparent.size() < 55)
	{
		return trace_context{};
	}

	// Parse version
	if (traceparent[0] != '0' || traceparent[1] != '0' || traceparent[2] != '-')
	{
		return trace_context{};
	}

	// Parse trace ID (32 hex chars)
	trace_id_t trace_id{};
	if (!hex_to_bytes(traceparent.substr(3, 32), trace_id.data(), 16))
	{
		return trace_context{};
	}

	// Check separator
	if (traceparent[35] != '-')
	{
		return trace_context{};
	}

	// Parse span ID (16 hex chars)
	span_id_t span_id{};
	if (!hex_to_bytes(traceparent.substr(36, 16), span_id.data(), 8))
	{
		return trace_context{};
	}

	// Check separator
	if (traceparent[52] != '-')
	{
		return trace_context{};
	}

	// Parse flags (2 hex chars)
	int high = hex_char_to_value(traceparent[53]);
	int low = hex_char_to_value(traceparent[54]);
	if (high < 0 || low < 0)
	{
		return trace_context{};
	}

	auto flags = static_cast<trace_flags>((high << 4) | low);

	return trace_context(trace_id, span_id, flags);
}

auto trace_context::from_headers(
    const std::vector<std::pair<std::string, std::string>>& headers) -> trace_context
{
	for (const auto& [name, value] : headers)
	{
		// Case-insensitive comparison
		std::string lower_name = name;
		std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (lower_name == "traceparent")
		{
			auto ctx = from_traceparent(value);
			if (ctx.is_valid())
			{
				return ctx;
			}
		}
	}

	return trace_context{};
}

auto trace_context::is_valid() const noexcept -> bool
{
	return valid_;
}

auto trace_context::is_sampled() const noexcept -> bool
{
	return (static_cast<uint8_t>(flags_) & static_cast<uint8_t>(trace_flags::sampled)) != 0;
}

auto trace_context::trace_id() const noexcept -> const trace_id_t&
{
	return trace_id_;
}

auto trace_context::span_id() const noexcept -> const span_id_t&
{
	return span_id_;
}

auto trace_context::parent_span_id() const noexcept -> const std::optional<span_id_t>&
{
	return parent_span_id_;
}

auto trace_context::flags() const noexcept -> trace_flags
{
	return flags_;
}

auto trace_context::trace_id_hex() const -> std::string
{
	return bytes_to_hex(trace_id_.data(), trace_id_.size());
}

auto trace_context::span_id_hex() const -> std::string
{
	return bytes_to_hex(span_id_.data(), span_id_.size());
}

auto trace_context::operator==(const trace_context& other) const noexcept -> bool
{
	return trace_id_ == other.trace_id_ && span_id_ == other.span_id_ &&
	       parent_span_id_ == other.parent_span_id_ && flags_ == other.flags_ &&
	       valid_ == other.valid_;
}

auto trace_context::operator!=(const trace_context& other) const noexcept -> bool
{
	return !(*this == other);
}

auto generate_trace_id() -> trace_id_t
{
	trace_id_t id{};
	get_random_bytes(id.data(), id.size());
	return id;
}

auto generate_span_id() -> span_id_t
{
	span_id_t id{};
	get_random_bytes(id.data(), id.size());
	return id;
}

auto bytes_to_hex(const uint8_t* data, size_t size) -> std::string
{
	std::string result;
	result.reserve(size * 2);

	for (size_t i = 0; i < size; ++i)
	{
		result.push_back(hex_chars[(data[i] >> 4) & 0x0F]);
		result.push_back(hex_chars[data[i] & 0x0F]);
	}

	return result;
}

auto hex_to_bytes(std::string_view hex, uint8_t* out, size_t size) -> bool
{
	if (hex.size() != size * 2)
	{
		return false;
	}

	for (size_t i = 0; i < size; ++i)
	{
		int high = hex_char_to_value(hex[i * 2]);
		int low = hex_char_to_value(hex[i * 2 + 1]);

		if (high < 0 || low < 0)
		{
			return false;
		}

		out[i] = static_cast<uint8_t>((high << 4) | low);
	}

	return true;
}

} // namespace kcenon::network::tracing
