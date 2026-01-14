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
 * @file histogram.h
 * @brief Histogram metric implementation for latency distribution tracking
 *
 * Provides histogram functionality for capturing latency distributions
 * with percentile calculations (p50, p95, p99) for network operations.
 *
 * @author kcenon
 * @date 2025-01-15
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace kcenon::network::metrics {

/**
 * @struct histogram_config
 * @brief Configuration for histogram bucket boundaries
 */
struct histogram_config
{
	/**
	 * @brief Explicit bucket boundaries (upper bounds)
	 *
	 * If empty, default network latency boundaries are used:
	 * {0.1, 0.5, 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000}
	 */
	std::vector<double> bucket_boundaries;

	/**
	 * @brief Create default configuration for network latencies
	 * @return Configuration with standard network latency buckets (milliseconds)
	 */
	static auto default_latency_config() -> histogram_config
	{
		return histogram_config{
			{0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0,
			 10000.0}};
	}
};

/**
 * @struct histogram_snapshot
 * @brief Immutable snapshot of histogram state for export
 */
struct histogram_snapshot
{
	uint64_t count;                                        ///< Total number of observations
	double sum;                                            ///< Sum of all observed values
	double min_value;                                      ///< Minimum observed value
	double max_value;                                      ///< Maximum observed value
	std::map<double, double> percentiles;                  ///< Percentile -> value mapping
	std::vector<std::pair<double, uint64_t>> buckets;      ///< Boundary -> cumulative count
	std::map<std::string, std::string> labels;             ///< Additional metric labels

	/**
	 * @brief Export histogram in Prometheus format
	 * @param name Metric name
	 * @return Prometheus-formatted string
	 */
	[[nodiscard]] auto to_prometheus(const std::string& name) const -> std::string;

	/**
	 * @brief Export histogram as JSON
	 * @return JSON-formatted string
	 */
	[[nodiscard]] auto to_json() const -> std::string;
};

/**
 * @class histogram
 * @brief Thread-safe histogram for capturing value distributions
 *
 * This histogram implementation uses predefined bucket boundaries to track
 * the distribution of values. It supports:
 * - Recording values with thread-safe atomic operations
 * - Calculating percentiles (p50, p95, p99, p999)
 * - Exporting to Prometheus and JSON formats
 *
 * @par Example usage:
 * @code
 * histogram h(histogram_config::default_latency_config());
 * h.record(5.5);
 * h.record(10.2);
 * h.record(3.1);
 *
 * double p99 = h.p99();
 * auto snapshot = h.snapshot();
 * std::cout << snapshot.to_prometheus("latency_ms") << std::endl;
 * @endcode
 */
class histogram
{
public:
	/**
	 * @brief Construct histogram with configuration
	 * @param cfg Configuration with bucket boundaries
	 */
	explicit histogram(histogram_config cfg = histogram_config::default_latency_config());

	/**
	 * @brief Destructor
	 */
	~histogram() = default;

	// Non-copyable (contains atomics)
	histogram(const histogram&) = delete;
	auto operator=(const histogram&) -> histogram& = delete;

	// Movable
	histogram(histogram&& other) noexcept;
	auto operator=(histogram&& other) noexcept -> histogram&;

	/**
	 * @brief Record a value observation
	 * @param value The value to record
	 *
	 * Thread-safe. Values are placed in the appropriate bucket
	 * based on configured boundaries.
	 */
	void record(double value);

	/**
	 * @brief Get total number of observations
	 * @return Count of recorded values
	 */
	[[nodiscard]] auto count() const -> uint64_t;

	/**
	 * @brief Get sum of all observations
	 * @return Sum of recorded values
	 */
	[[nodiscard]] auto sum() const -> double;

	/**
	 * @brief Get minimum observed value
	 * @return Minimum value, or +inf if no observations
	 */
	[[nodiscard]] auto min() const -> double;

	/**
	 * @brief Get maximum observed value
	 * @return Maximum value, or -inf if no observations
	 */
	[[nodiscard]] auto max() const -> double;

	/**
	 * @brief Get mean of all observations
	 * @return Mean value, or 0 if no observations
	 */
	[[nodiscard]] auto mean() const -> double;

	/**
	 * @brief Calculate percentile value
	 * @param p Percentile (0.0 to 1.0)
	 * @return Estimated value at the given percentile
	 *
	 * Uses linear interpolation within buckets for estimation.
	 */
	[[nodiscard]] auto percentile(double p) const -> double;

	/**
	 * @brief Get 50th percentile (median)
	 * @return Estimated median value
	 */
	[[nodiscard]] auto p50() const -> double { return percentile(0.50); }

	/**
	 * @brief Get 95th percentile
	 * @return Estimated p95 value
	 */
	[[nodiscard]] auto p95() const -> double { return percentile(0.95); }

	/**
	 * @brief Get 99th percentile
	 * @return Estimated p99 value
	 */
	[[nodiscard]] auto p99() const -> double { return percentile(0.99); }

	/**
	 * @brief Get 99.9th percentile
	 * @return Estimated p999 value
	 */
	[[nodiscard]] auto p999() const -> double { return percentile(0.999); }

	/**
	 * @brief Get all bucket counts
	 * @return Vector of (boundary, cumulative_count) pairs
	 */
	[[nodiscard]] auto buckets() const -> std::vector<std::pair<double, uint64_t>>;

	/**
	 * @brief Create immutable snapshot of current state
	 * @param labels Additional labels to include in snapshot
	 * @return Snapshot with all statistics and bucket counts
	 */
	[[nodiscard]] auto snapshot(const std::map<std::string, std::string>& labels = {}) const
		-> histogram_snapshot;

	/**
	 * @brief Reset all statistics
	 *
	 * Thread-safe. Clears all observations.
	 */
	void reset();

private:
	std::vector<double> boundaries_;
	std::unique_ptr<std::atomic<uint64_t>[]> bucket_counts_;
	size_t bucket_count_{0};
	std::atomic<uint64_t> count_{0};
	std::atomic<double> sum_{0.0};
	std::atomic<double> min_{std::numeric_limits<double>::infinity()};
	std::atomic<double> max_{-std::numeric_limits<double>::infinity()};
	mutable std::mutex mutex_;

	/**
	 * @brief Find bucket index for a value
	 * @param value The value to classify
	 * @return Index of the bucket containing this value
	 */
	[[nodiscard]] auto find_bucket(double value) const -> size_t;

	/**
	 * @brief Update min value atomically
	 * @param value New potential minimum
	 */
	void update_min(double value);

	/**
	 * @brief Update max value atomically
	 * @param value New potential maximum
	 */
	void update_max(double value);

	/**
	 * @brief Add to sum atomically
	 * @param value Value to add
	 */
	void add_to_sum(double value);
};

} // namespace kcenon::network::metrics
