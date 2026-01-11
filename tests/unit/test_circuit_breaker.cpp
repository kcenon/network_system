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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "kcenon/network/utils/circuit_breaker.h"

using namespace kcenon::network::utils;

// ============================================================================
// Circuit Breaker Basic Tests
// ============================================================================

class CircuitBreakerTest : public ::testing::Test
{
protected:
	void SetUp() override {}
	void TearDown() override {}
};

TEST_F(CircuitBreakerTest, DefaultConfigStartsClosed)
{
	circuit_breaker cb;
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::closed);
	EXPECT_EQ(cb.failure_count(), 0);
}

TEST_F(CircuitBreakerTest, CustomConfigStartsClosed)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 10;

	circuit_breaker cb(cfg);
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::closed);
}

TEST_F(CircuitBreakerTest, AllowCallWhenClosed)
{
	circuit_breaker cb;
	EXPECT_TRUE(cb.allow_call());
}

TEST_F(CircuitBreakerTest, SuccessResetsFailureCount)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 5;
	circuit_breaker cb(cfg);

	// Record some failures
	(void)cb.allow_call();
	cb.record_failure();
	(void)cb.allow_call();
	cb.record_failure();
	EXPECT_EQ(cb.failure_count(), 2);

	// Record success
	(void)cb.allow_call();
	cb.record_success();
	EXPECT_EQ(cb.failure_count(), 0);
}

// ============================================================================
// State Transition Tests
// ============================================================================

TEST_F(CircuitBreakerTest, OpensAfterFailureThreshold)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;

	circuit_breaker cb(cfg);

	// Record failures up to threshold
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		EXPECT_TRUE(cb.allow_call());
		cb.record_failure();
	}

	// Circuit should now be open
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::open);
}

TEST_F(CircuitBreakerTest, OpenCircuitBlocksCalls)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;

	circuit_breaker cb(cfg);

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	EXPECT_EQ(cb.current_state(), circuit_breaker::state::open);
	EXPECT_FALSE(cb.allow_call());
}

TEST_F(CircuitBreakerTest, TransitionsToHalfOpenAfterTimeout)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;
	cfg.open_duration = std::chrono::seconds(0); // Immediate transition

	circuit_breaker cb(cfg);

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	EXPECT_EQ(cb.current_state(), circuit_breaker::state::open);

	// Wait a tiny bit for timeout
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	// Next call should transition to half-open
	EXPECT_TRUE(cb.allow_call());
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::half_open);
}

TEST_F(CircuitBreakerTest, HalfOpenClosesAfterSuccesses)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;
	cfg.open_duration = std::chrono::seconds(0);
	cfg.half_open_successes = 2;

	circuit_breaker cb(cfg);

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	// Wait for transition to half-open
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	(void)cb.allow_call(); // Triggers transition to half-open

	EXPECT_EQ(cb.current_state(), circuit_breaker::state::half_open);

	// Record successful calls
	cb.record_success();
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::half_open);

	(void)cb.allow_call();
	cb.record_success();
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::closed);
}

TEST_F(CircuitBreakerTest, HalfOpenReopensOnFailure)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;
	cfg.open_duration = std::chrono::seconds(0);

	circuit_breaker cb(cfg);

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	// Wait for transition to half-open
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	(void)cb.allow_call(); // Triggers transition to half-open

	EXPECT_EQ(cb.current_state(), circuit_breaker::state::half_open);

	// Any failure should re-open immediately
	cb.record_failure();
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::open);
}

TEST_F(CircuitBreakerTest, HalfOpenLimitsMaxCalls)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;
	cfg.open_duration = std::chrono::seconds(0);
	cfg.half_open_max_calls = 2;

	circuit_breaker cb(cfg);

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	// Wait for transition to half-open
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	// First call should succeed and trigger transition
	EXPECT_TRUE(cb.allow_call());
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::half_open);

	// Second call allowed (within max)
	EXPECT_TRUE(cb.allow_call());

	// Third call blocked (exceeds max)
	EXPECT_FALSE(cb.allow_call());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(CircuitBreakerTest, CallbackOnStateChange)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 2;

	circuit_breaker cb(cfg);

	std::atomic<int> callback_count{0};
	circuit_breaker::state last_from = circuit_breaker::state::closed;
	circuit_breaker::state last_to = circuit_breaker::state::closed;

	cb.set_state_change_callback([&callback_count, &last_from, &last_to](auto from, auto to) {
		callback_count++;
		last_from = from;
		last_to = to;
	});

	// Open the circuit
	(void)cb.allow_call();
	cb.record_failure();
	(void)cb.allow_call();
	cb.record_failure();

	EXPECT_EQ(callback_count, 1);
	EXPECT_EQ(last_from, circuit_breaker::state::closed);
	EXPECT_EQ(last_to, circuit_breaker::state::open);
}

TEST_F(CircuitBreakerTest, CallbackOnHalfOpenTransition)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 2;
	cfg.open_duration = std::chrono::seconds(0);

	circuit_breaker cb(cfg);

	std::atomic<int> callback_count{0};
	circuit_breaker::state last_from = circuit_breaker::state::closed;
	circuit_breaker::state last_to = circuit_breaker::state::closed;

	cb.set_state_change_callback([&callback_count, &last_from, &last_to](auto from, auto to) {
		callback_count++;
		last_from = from;
		last_to = to;
	});

	// Open the circuit
	(void)cb.allow_call();
	cb.record_failure();
	(void)cb.allow_call();
	cb.record_failure();

	EXPECT_EQ(callback_count, 1);

	// Wait for timeout
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	(void)cb.allow_call(); // Triggers half-open

	EXPECT_EQ(callback_count, 2);
	EXPECT_EQ(last_from, circuit_breaker::state::open);
	EXPECT_EQ(last_to, circuit_breaker::state::half_open);
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(CircuitBreakerTest, ResetClosesOpenCircuit)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;

	circuit_breaker cb(cfg);

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	EXPECT_EQ(cb.current_state(), circuit_breaker::state::open);

	// Reset
	cb.reset();

	EXPECT_EQ(cb.current_state(), circuit_breaker::state::closed);
	EXPECT_EQ(cb.failure_count(), 0);
	EXPECT_TRUE(cb.allow_call());
}

TEST_F(CircuitBreakerTest, ResetClearsFailureCount)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 5;

	circuit_breaker cb(cfg);

	// Accumulate some failures
	(void)cb.allow_call();
	cb.record_failure();
	(void)cb.allow_call();
	cb.record_failure();

	EXPECT_EQ(cb.failure_count(), 2);

	cb.reset();

	EXPECT_EQ(cb.failure_count(), 0);
}

// ============================================================================
// State to String Tests
// ============================================================================

TEST_F(CircuitBreakerTest, StateToString)
{
	EXPECT_EQ(circuit_breaker::state_to_string(circuit_breaker::state::closed), "closed");
	EXPECT_EQ(circuit_breaker::state_to_string(circuit_breaker::state::open), "open");
	EXPECT_EQ(circuit_breaker::state_to_string(circuit_breaker::state::half_open), "half_open");
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(CircuitBreakerTest, ConcurrentAllowCalls)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 1000; // High threshold

	circuit_breaker cb(cfg);

	std::atomic<int> allowed_count{0};
	std::vector<std::thread> threads;

	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back([&cb, &allowed_count]() {
			for (int j = 0; j < 100; ++j)
			{
				if (cb.allow_call())
				{
					allowed_count++;
					cb.record_success();
				}
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// All calls should have been allowed (circuit stays closed)
	EXPECT_EQ(allowed_count, 1000);
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::closed);
}

TEST_F(CircuitBreakerTest, ConcurrentFailures)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 50;

	circuit_breaker cb(cfg);

	std::vector<std::thread> threads;

	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back([&cb]() {
			for (int j = 0; j < 10; ++j)
			{
				if (cb.allow_call())
				{
					cb.record_failure();
				}
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// Circuit should be open after concurrent failures
	EXPECT_EQ(cb.current_state(), circuit_breaker::state::open);
}

// ============================================================================
// Next Attempt Time Tests
// ============================================================================

TEST_F(CircuitBreakerTest, NextAttemptTimeIsValid)
{
	circuit_breaker_config cfg;
	cfg.failure_threshold = 3;
	cfg.open_duration = std::chrono::seconds(30);

	circuit_breaker cb(cfg);

	auto before_open = std::chrono::steady_clock::now();

	// Open the circuit
	for (size_t i = 0; i < cfg.failure_threshold; ++i)
	{
		(void)cb.allow_call();
		cb.record_failure();
	}

	auto after_open = std::chrono::steady_clock::now();
	auto next_attempt = cb.next_attempt_time();

	// Next attempt time should be after open time + open_duration
	EXPECT_GE(next_attempt, before_open + cfg.open_duration);
	EXPECT_LE(next_attempt, after_open + cfg.open_duration + std::chrono::milliseconds(100));
}
