/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "kcenon/network/utils/circuit_breaker.h"

#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::utils
{

	circuit_breaker::circuit_breaker(config cfg)
		: config_(cfg)
		, open_time_(std::chrono::steady_clock::now())
	{
		NETWORK_LOG_INFO("[circuit_breaker] Created with failure_threshold=" +
			std::to_string(config_.failure_threshold) +
			", open_duration=" + std::to_string(config_.open_duration.count()) + "s" +
			", half_open_successes=" + std::to_string(config_.half_open_successes) +
			", half_open_max_calls=" + std::to_string(config_.half_open_max_calls));
	}

	auto circuit_breaker::allow_call() -> bool
	{
		auto current = state_.load(std::memory_order_acquire);

		switch (current)
		{
		case state::closed:
			return true;

		case state::open:
			{
				std::lock_guard<std::mutex> lock(mutex_);
				if (should_attempt_reset())
				{
					NETWORK_LOG_INFO("[circuit_breaker] Open timeout elapsed, transitioning to half-open");
					transition_to(state::half_open);
					half_open_calls_.store(1, std::memory_order_release);
					return true;
				}
				return false;
			}

		case state::half_open:
			{
				auto calls = half_open_calls_.fetch_add(1, std::memory_order_acq_rel);
				if (calls < config_.half_open_max_calls)
				{
					return true;
				}
				// Max calls reached, don't allow more
				half_open_calls_.fetch_sub(1, std::memory_order_release);
				return false;
			}
		}

		return false;
	}

	auto circuit_breaker::record_success() -> void
	{
		auto current = state_.load(std::memory_order_acquire);

		switch (current)
		{
		case state::closed:
			// Reset failure count on success
			failure_count_.store(0, std::memory_order_release);
			break;

		case state::half_open:
			{
				auto successes = success_count_.fetch_add(1, std::memory_order_acq_rel) + 1;
				NETWORK_LOG_DEBUG("[circuit_breaker] Half-open success " +
					std::to_string(successes) + "/" +
					std::to_string(config_.half_open_successes));

				if (successes >= config_.half_open_successes)
				{
					std::lock_guard<std::mutex> lock(mutex_);
					// Double-check state hasn't changed
					if (state_.load(std::memory_order_acquire) == state::half_open)
					{
						NETWORK_LOG_INFO("[circuit_breaker] Required successes reached, closing circuit");
						transition_to(state::closed);
					}
				}
			}
			break;

		case state::open:
			// Shouldn't happen - calls not allowed when open
			break;
		}
	}

	auto circuit_breaker::record_failure() -> void
	{
		auto current = state_.load(std::memory_order_acquire);

		switch (current)
		{
		case state::closed:
			{
				auto failures = failure_count_.fetch_add(1, std::memory_order_acq_rel) + 1;
				NETWORK_LOG_DEBUG("[circuit_breaker] Failure recorded, count=" +
					std::to_string(failures) + "/" +
					std::to_string(config_.failure_threshold));

				if (failures >= config_.failure_threshold)
				{
					std::lock_guard<std::mutex> lock(mutex_);
					// Double-check state hasn't changed
					if (state_.load(std::memory_order_acquire) == state::closed)
					{
						NETWORK_LOG_WARN("[circuit_breaker] Failure threshold reached, opening circuit");
						transition_to(state::open);
					}
				}
			}
			break;

		case state::half_open:
			{
				std::lock_guard<std::mutex> lock(mutex_);
				// Any failure in half-open immediately opens the circuit
				if (state_.load(std::memory_order_acquire) == state::half_open)
				{
					NETWORK_LOG_WARN("[circuit_breaker] Failure in half-open state, re-opening circuit");
					transition_to(state::open);
				}
			}
			break;

		case state::open:
			// Already open, no effect
			break;
		}
	}

	auto circuit_breaker::current_state() const -> state
	{
		return state_.load(std::memory_order_acquire);
	}

	auto circuit_breaker::failure_count() const -> size_t
	{
		return failure_count_.load(std::memory_order_acquire);
	}

	auto circuit_breaker::next_attempt_time() const -> std::chrono::steady_clock::time_point
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return open_time_ + config_.open_duration;
	}

	auto circuit_breaker::set_state_change_callback(state_change_callback cb) -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		callback_ = std::move(cb);
	}

	auto circuit_breaker::reset() -> void
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto old_state = state_.load(std::memory_order_acquire);

		failure_count_.store(0, std::memory_order_release);
		success_count_.store(0, std::memory_order_release);
		half_open_calls_.store(0, std::memory_order_release);
		state_.store(state::closed, std::memory_order_release);

		NETWORK_LOG_INFO("[circuit_breaker] Reset to closed state");

		if (callback_ && old_state != state::closed)
		{
			callback_(old_state, state::closed);
		}
	}

	auto circuit_breaker::state_to_string(state s) -> std::string
	{
		switch (s)
		{
		case state::closed:
			return "closed";
		case state::open:
			return "open";
		case state::half_open:
			return "half_open";
		default:
			return "unknown";
		}
	}

	auto circuit_breaker::transition_to(state new_state) -> void
	{
		auto old_state = state_.load(std::memory_order_acquire);
		if (old_state == new_state)
		{
			return;
		}

		NETWORK_LOG_INFO("[circuit_breaker] State transition: " +
			state_to_string(old_state) + " -> " + state_to_string(new_state));

		// Reset counters based on target state
		switch (new_state)
		{
		case state::closed:
			failure_count_.store(0, std::memory_order_release);
			success_count_.store(0, std::memory_order_release);
			half_open_calls_.store(0, std::memory_order_release);
			break;

		case state::open:
			open_time_ = std::chrono::steady_clock::now();
			success_count_.store(0, std::memory_order_release);
			half_open_calls_.store(0, std::memory_order_release);
			break;

		case state::half_open:
			success_count_.store(0, std::memory_order_release);
			half_open_calls_.store(0, std::memory_order_release);
			break;
		}

		state_.store(new_state, std::memory_order_release);

		if (callback_)
		{
			callback_(old_state, new_state);
		}
	}

	auto circuit_breaker::should_attempt_reset() const -> bool
	{
		auto now = std::chrono::steady_clock::now();
		return now >= (open_time_ + config_.open_duration);
	}

} // namespace kcenon::network::utils
