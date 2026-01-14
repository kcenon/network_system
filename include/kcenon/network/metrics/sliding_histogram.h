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
 * @file sliding_histogram.h
 * @brief Sliding window histogram for time-based latency tracking
 *
 * Provides a histogram that automatically expires old data based on
 * a configurable time window, useful for real-time percentile monitoring.
 *
 * @author kcenon
 * @date 2025-01-15
 */

#pragma once

#include "kcenon/network/metrics/histogram.h"

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>

namespace kcenon::network::metrics {

/**
 * @struct sliding_histogram_config
 * @brief Configuration for sliding window histogram
 */
struct sliding_histogram_config
{
	histogram_config hist_config;                               ///< Histogram bucket configuration
	std::chrono::seconds window_duration{60};                   ///< Total window duration
	size_t bucket_count{6};                                     ///< Number of time buckets

	/**
	 * @brief Create default configuration (60 second window, 6 buckets = 10s each)
	 * @return Default sliding histogram configuration
	 */
	static auto default_config() -> sliding_histogram_config
	{
		return sliding_histogram_config{histogram_config::default_latency_config(),
										std::chrono::seconds{60}, 6};
	}
};

/**
 * @class sliding_histogram
 * @brief Time-windowed histogram for tracking recent latency distributions
 *
 * This class maintains a sliding window of histogram data, automatically
 * expiring old measurements. Useful for monitoring recent performance
 * without accumulating historical data indefinitely.
 *
 * @par Example usage:
 * @code
 * sliding_histogram sh(sliding_histogram_config::default_config());
 * sh.record(5.5);
 * sh.record(10.2);
 *
 * // Get percentiles for the last 60 seconds
 * double p99 = sh.p99();
 * @endcode
 */
class sliding_histogram
{
public:
	/**
	 * @brief Construct sliding histogram with configuration
	 * @param cfg Configuration for the sliding window
	 */
	explicit sliding_histogram(sliding_histogram_config cfg = sliding_histogram_config::default_config());

	/**
	 * @brief Destructor
	 */
	~sliding_histogram() = default;

	// Non-copyable
	sliding_histogram(const sliding_histogram&) = delete;
	auto operator=(const sliding_histogram&) -> sliding_histogram& = delete;

	// Movable
	sliding_histogram(sliding_histogram&& other) noexcept;
	auto operator=(sliding_histogram&& other) noexcept -> sliding_histogram&;

	/**
	 * @brief Record a value observation
	 * @param value The value to record
	 *
	 * Thread-safe. Automatically creates new time buckets as needed
	 * and expires old ones.
	 */
	void record(double value);

	/**
	 * @brief Get total number of observations in current window
	 * @return Count of recorded values
	 */
	[[nodiscard]] auto count() const -> uint64_t;

	/**
	 * @brief Get sum of all observations in current window
	 * @return Sum of recorded values
	 */
	[[nodiscard]] auto sum() const -> double;

	/**
	 * @brief Get mean of all observations in current window
	 * @return Mean value, or 0 if no observations
	 */
	[[nodiscard]] auto mean() const -> double;

	/**
	 * @brief Calculate percentile value for current window
	 * @param p Percentile (0.0 to 1.0)
	 * @return Estimated value at the given percentile
	 */
	[[nodiscard]] auto percentile(double p) const -> double;

	/**
	 * @brief Get 50th percentile (median) for current window
	 * @return Estimated median value
	 */
	[[nodiscard]] auto p50() const -> double { return percentile(0.50); }

	/**
	 * @brief Get 95th percentile for current window
	 * @return Estimated p95 value
	 */
	[[nodiscard]] auto p95() const -> double { return percentile(0.95); }

	/**
	 * @brief Get 99th percentile for current window
	 * @return Estimated p99 value
	 */
	[[nodiscard]] auto p99() const -> double { return percentile(0.99); }

	/**
	 * @brief Get 99.9th percentile for current window
	 * @return Estimated p999 value
	 */
	[[nodiscard]] auto p999() const -> double { return percentile(0.999); }

	/**
	 * @brief Create snapshot aggregating all time buckets in current window
	 * @param labels Additional labels to include in snapshot
	 * @return Aggregated snapshot with statistics
	 */
	[[nodiscard]] auto snapshot(const std::map<std::string, std::string>& labels = {}) const
		-> histogram_snapshot;

	/**
	 * @brief Get the window duration
	 * @return Configured window duration
	 */
	[[nodiscard]] auto window_duration() const -> std::chrono::seconds
	{
		return config_.window_duration;
	}

	/**
	 * @brief Reset all data
	 *
	 * Thread-safe. Clears all time buckets.
	 */
	void reset();

private:
	struct time_bucket
	{
		histogram hist;
		std::chrono::steady_clock::time_point start_time;

		explicit time_bucket(const histogram_config& cfg)
			: hist(cfg), start_time(std::chrono::steady_clock::now())
		{
		}
	};

	sliding_histogram_config config_;
	std::chrono::milliseconds bucket_duration_;
	std::deque<std::unique_ptr<time_bucket>> buckets_;
	mutable std::mutex mutex_;

	/**
	 * @brief Expire old buckets outside the window
	 */
	void expire_old_buckets();

	/**
	 * @brief Get or create current time bucket
	 * @return Reference to the current bucket
	 */
	auto get_current_bucket() -> time_bucket&;

	/**
	 * @brief Create aggregated histogram from all buckets
	 * @return Aggregated histogram snapshot
	 */
	[[nodiscard]] auto aggregate() const -> histogram_snapshot;
};

} // namespace kcenon::network::metrics
