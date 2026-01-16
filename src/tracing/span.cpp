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
 * @file span.cpp
 * @brief Implementation of RAII span for distributed tracing
 */

#include "kcenon/network/tracing/span.h"
#include "kcenon/network/tracing/tracing_config.h"

#include <utility>

namespace kcenon::network::tracing {

/**
 * @brief Internal implementation structure for span
 */
struct span::impl
{
	std::string name;
	trace_context context;
	span_kind kind;
	span_status status{span_status::unset};
	std::string status_description;
	std::map<std::string, attribute_value> attributes;
	std::vector<span_event> events;
	std::chrono::steady_clock::time_point start_time;
	std::chrono::steady_clock::time_point end_time;
	bool ended{false};
	trace_context previous_context;
	span* owner{nullptr};

	impl(std::string_view span_name, trace_context ctx, span_kind span_kind_value)
	    : name(span_name)
	    , context(std::move(ctx))
	    , kind(span_kind_value)
	    , start_time(std::chrono::steady_clock::now())
	    , end_time(start_time)
	    , previous_context(trace_context::current())
	{
		// Set this span's context as the current context
		trace_context::set_current(context);
	}

	~impl()
	{
		// Note: do_end() is now called from span::~span() before impl_ destruction
		// to ensure owner->impl_ is still valid when export_span is called.
		// This is necessary because libc++'s unique_ptr sets its internal pointer
		// to null before calling the deleter, which would cause owner->impl_ to
		// be null when export_span tries to access span data.
	}

	void do_end()
	{
		if (ended)
		{
			return;
		}

		ended = true;
		end_time = std::chrono::steady_clock::now();

		// Restore previous context
		if (previous_context.is_valid())
		{
			trace_context::set_current(previous_context);
		}
		else
		{
			trace_context::clear_current();
		}

		// Export the span
		if (owner)
		{
			export_span(*owner);
		}
	}
};

span::span(std::string_view name, trace_context ctx, span_kind kind)
    : impl_(std::make_unique<impl>(name, std::move(ctx), kind))
{
	impl_->owner = this;
}

span::~span()
{
	// End span before impl_ destruction to ensure owner->impl_ is still valid
	if (impl_ && !impl_->ended)
	{
		impl_->do_end();
	}
}

span::span(span&& other) noexcept
    : impl_(std::move(other.impl_))
{
	if (impl_)
	{
		impl_->owner = this;
	}
}

auto span::operator=(span&& other) noexcept -> span&
{
	if (this != &other)
	{
		impl_ = std::move(other.impl_);
		if (impl_)
		{
			impl_->owner = this;
		}
	}
	return *this;
}

auto span::set_attribute(std::string_view key, std::string_view value) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->attributes[std::string(key)] = std::string(value);
	}
	return *this;
}

auto span::set_attribute(std::string_view key, const char* value) -> span&
{
	return set_attribute(key, std::string_view(value));
}

auto span::set_attribute(std::string_view key, int64_t value) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->attributes[std::string(key)] = value;
	}
	return *this;
}

auto span::set_attribute(std::string_view key, double value) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->attributes[std::string(key)] = value;
	}
	return *this;
}

auto span::set_attribute(std::string_view key, bool value) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->attributes[std::string(key)] = value;
	}
	return *this;
}

auto span::add_event(std::string_view name) -> span&
{
	if (impl_ && !impl_->ended)
	{
		span_event event;
		event.name = std::string(name);
		event.timestamp = std::chrono::steady_clock::now();
		impl_->events.push_back(std::move(event));
	}
	return *this;
}

auto span::add_event(std::string_view name,
                     const std::map<std::string, attribute_value>& attributes) -> span&
{
	if (impl_ && !impl_->ended)
	{
		span_event event;
		event.name = std::string(name);
		event.timestamp = std::chrono::steady_clock::now();
		event.attributes = attributes;
		impl_->events.push_back(std::move(event));
	}
	return *this;
}

auto span::set_status(span_status status) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->status = status;
	}
	return *this;
}

auto span::set_status(span_status status, std::string_view description) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->status = status;
		impl_->status_description = std::string(description);
	}
	return *this;
}

auto span::set_error(std::string_view message) -> span&
{
	if (impl_ && !impl_->ended)
	{
		impl_->status = span_status::error;
		impl_->status_description = std::string(message);

		// Also add an exception event following OpenTelemetry conventions
		std::map<std::string, attribute_value> event_attrs;
		event_attrs["exception.message"] = std::string(message);
		add_event("exception", event_attrs);
	}
	return *this;
}

void span::end()
{
	if (impl_)
	{
		impl_->do_end();
	}
}

auto span::is_ended() const noexcept -> bool
{
	return impl_ ? impl_->ended : true;
}

auto span::context() const noexcept -> const trace_context&
{
	static const trace_context empty_context;
	return impl_ ? impl_->context : empty_context;
}

auto span::name() const noexcept -> const std::string&
{
	static const std::string empty_name;
	return impl_ ? impl_->name : empty_name;
}

auto span::kind() const noexcept -> span_kind
{
	return impl_ ? impl_->kind : span_kind::internal;
}

auto span::status() const noexcept -> span_status
{
	return impl_ ? impl_->status : span_status::unset;
}

auto span::status_description() const noexcept -> const std::string&
{
	static const std::string empty_description;
	return impl_ ? impl_->status_description : empty_description;
}

auto span::attributes() const noexcept
    -> const std::map<std::string, attribute_value>&
{
	static const std::map<std::string, attribute_value> empty_attrs;
	return impl_ ? impl_->attributes : empty_attrs;
}

auto span::events() const noexcept -> const std::vector<span_event>&
{
	static const std::vector<span_event> empty_events;
	return impl_ ? impl_->events : empty_events;
}

auto span::start_time() const noexcept -> std::chrono::steady_clock::time_point
{
	return impl_ ? impl_->start_time : std::chrono::steady_clock::time_point{};
}

auto span::end_time() const noexcept -> std::chrono::steady_clock::time_point
{
	return impl_ ? impl_->end_time : std::chrono::steady_clock::time_point{};
}

auto span::duration() const noexcept -> std::chrono::nanoseconds
{
	if (!impl_)
	{
		return std::chrono::nanoseconds::zero();
	}

	auto end = impl_->ended ? impl_->end_time : std::chrono::steady_clock::now();
	return std::chrono::duration_cast<std::chrono::nanoseconds>(end - impl_->start_time);
}

} // namespace kcenon::network::tracing
