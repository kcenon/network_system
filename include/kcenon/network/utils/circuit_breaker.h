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

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>

namespace kcenon::network::utils
{

	/*!
	 * \class circuit_breaker
	 * \brief Implements the Circuit Breaker pattern for fault tolerance.
	 *
	 * The circuit breaker prevents cascade failures by failing fast when
	 * a backend service is unavailable, allowing the system to recover gracefully.
	 *
	 * ### States
	 * - **Closed**: Normal operation, requests pass through
	 * - **Open**: Failures exceeded threshold, requests fail immediately
	 * - **Half-Open**: Testing if service recovered
	 *
	 * ### State Transitions
	 * ```
	 *        success
	 * ┌──────────────────┐
	 * │                  │
	 * ▼                  │
	 * ┌───────┐  failure  ┌──────┐
	 * │Closed │──────────►│ Open │
	 * └───────┘threshold  └──────┘
	 *     ▲                  │
	 *     │                  │ timeout
	 *     │    success       ▼
	 *     │            ┌──────────┐
	 *     └────────────│Half-Open │
	 *                  └──────────┘
	 *                       │ failure
	 *                       └───► Open
	 * ```
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - State transitions are atomic where possible
	 * - Time-sensitive operations use mutex protection
	 *
	 * ### Usage Example
	 * \code
	 * circuit_breaker cb({
	 *     .failure_threshold = 5,
	 *     .open_duration = std::chrono::seconds(30),
	 *     .half_open_successes = 2,
	 *     .half_open_max_calls = 3
	 * });
	 *
	 * cb.set_state_change_callback([](auto from, auto to) {
	 *     std::cout << "Circuit state changed\n";
	 * });
	 *
	 * if (cb.allow_call()) {
	 *     try {
	 *         // Make the call
	 *         cb.record_success();
	 *     } catch (...) {
	 *         cb.record_failure();
	 *     }
	 * }
	 * \endcode
	 */
	class circuit_breaker
	{
	public:
		/*!
		 * \enum state
		 * \brief Represents the possible states of the circuit breaker.
		 */
		enum class state
		{
			closed,    /*!< Normal operation, requests pass through */
			open,      /*!< Circuit is open, requests fail immediately */
			half_open  /*!< Testing if service recovered */
		};

		/*!
		 * \struct config
		 * \brief Configuration parameters for the circuit breaker.
		 */
		struct config
		{
			size_t failure_threshold = 5;                    /*!< Consecutive failures before opening */
			std::chrono::seconds open_duration{30};          /*!< Duration before attempting half-open */
			size_t half_open_successes = 2;                  /*!< Successes needed to close */
			size_t half_open_max_calls = 3;                  /*!< Max calls during half-open */
		};

		/*!
		 * \brief State change callback type.
		 *
		 * Called when the circuit breaker transitions between states.
		 * \param from Previous state
		 * \param to New state
		 */
		using state_change_callback = std::function<void(state from, state to)>;

		/*!
		 * \brief Constructs a circuit breaker with the given configuration.
		 * \param cfg Configuration parameters (default values used if not specified)
		 */
		explicit circuit_breaker(config cfg = {});

		/*!
		 * \brief Default destructor.
		 */
		~circuit_breaker() = default;

		// Non-copyable, non-movable (contains mutex)
		circuit_breaker(const circuit_breaker&) = delete;
		circuit_breaker& operator=(const circuit_breaker&) = delete;
		circuit_breaker(circuit_breaker&&) = delete;
		circuit_breaker& operator=(circuit_breaker&&) = delete;

		/*!
		 * \brief Checks if a call should be allowed through the circuit.
		 * \return true if the call is allowed, false if circuit is open
		 *
		 * When closed: always returns true
		 * When open: returns false (fast-fail)
		 * When half-open: returns true if under max calls limit
		 */
		[[nodiscard]] auto allow_call() -> bool;

		/*!
		 * \brief Records a successful call.
		 *
		 * When closed: resets failure count
		 * When half-open: increments success count, may close circuit
		 * When open: no effect
		 */
		auto record_success() -> void;

		/*!
		 * \brief Records a failed call.
		 *
		 * When closed: increments failure count, may open circuit
		 * When half-open: opens circuit immediately
		 * When open: no effect
		 */
		auto record_failure() -> void;

		/*!
		 * \brief Gets the current state of the circuit breaker.
		 * \return Current state (closed, open, or half_open)
		 */
		[[nodiscard]] auto current_state() const -> state;

		/*!
		 * \brief Gets the current failure count.
		 * \return Number of consecutive failures in closed state
		 */
		[[nodiscard]] auto failure_count() const -> size_t;

		/*!
		 * \brief Gets the time when the next call attempt will be allowed.
		 * \return Time point when half-open transition will occur (only valid when open)
		 */
		[[nodiscard]] auto next_attempt_time() const -> std::chrono::steady_clock::time_point;

		/*!
		 * \brief Sets the callback for state changes.
		 * \param cb Callback function to invoke on state transitions
		 */
		auto set_state_change_callback(state_change_callback cb) -> void;

		/*!
		 * \brief Resets the circuit breaker to closed state.
		 *
		 * Clears all counters and transitions to closed state.
		 */
		auto reset() -> void;

		/*!
		 * \brief Converts state enum to string representation.
		 * \param s State to convert
		 * \return String name of the state
		 */
		[[nodiscard]] static auto state_to_string(state s) -> std::string;

	private:
		/*!
		 * \brief Transitions to a new state.
		 * \param new_state Target state
		 *
		 * Handles state-specific setup and invokes callback if set.
		 */
		auto transition_to(state new_state) -> void;

		/*!
		 * \brief Checks if the open timeout has elapsed.
		 * \return true if it's time to transition to half-open
		 */
		[[nodiscard]] auto should_attempt_reset() const -> bool;

		config config_;                                          /*!< Configuration parameters */
		std::atomic<state> state_{state::closed};                /*!< Current circuit state */
		std::atomic<size_t> failure_count_{0};                   /*!< Consecutive failures in closed */
		std::atomic<size_t> success_count_{0};                   /*!< Successes in half-open */
		std::atomic<size_t> half_open_calls_{0};                 /*!< Calls made in half-open */
		std::chrono::steady_clock::time_point open_time_;        /*!< When circuit was opened */
		mutable std::mutex mutex_;                               /*!< Protects time-sensitive ops */
		state_change_callback callback_;                         /*!< State change callback */
	};

} // namespace kcenon::network::utils
