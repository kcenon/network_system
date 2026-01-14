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

#include "kcenon/network/metrics/sliding_histogram.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace kcenon::network::metrics {

sliding_histogram::sliding_histogram(sliding_histogram_config cfg)
	: config_(std::move(cfg))
	, bucket_duration_(std::chrono::duration_cast<std::chrono::milliseconds>(config_.window_duration)
					   / config_.bucket_count)
{
	// Ensure at least 1 bucket
	if (config_.bucket_count == 0)
	{
		config_.bucket_count = 1;
		bucket_duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(config_.window_duration);
	}
}

sliding_histogram::sliding_histogram(sliding_histogram&& other) noexcept
	: config_(std::move(other.config_))
	, bucket_duration_(other.bucket_duration_)
	, buckets_(std::move(other.buckets_))
{
}

auto sliding_histogram::operator=(sliding_histogram&& other) noexcept -> sliding_histogram&
{
	if (this != &other)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		config_ = std::move(other.config_);
		bucket_duration_ = other.bucket_duration_;
		buckets_ = std::move(other.buckets_);
	}
	return *this;
}

void sliding_histogram::record(double value)
{
	std::lock_guard<std::mutex> lock(mutex_);
	expire_old_buckets();
	auto& bucket = get_current_bucket();
	bucket.hist.record(value);
}

auto sliding_histogram::count() const -> uint64_t
{
	std::lock_guard<std::mutex> lock(mutex_);
	const_cast<sliding_histogram*>(this)->expire_old_buckets();

	uint64_t total = 0;
	for (const auto& bucket : buckets_)
	{
		total += bucket->hist.count();
	}
	return total;
}

auto sliding_histogram::sum() const -> double
{
	std::lock_guard<std::mutex> lock(mutex_);
	const_cast<sliding_histogram*>(this)->expire_old_buckets();

	double total = 0.0;
	for (const auto& bucket : buckets_)
	{
		total += bucket->hist.sum();
	}
	return total;
}

auto sliding_histogram::mean() const -> double
{
	uint64_t c = count();
	if (c == 0)
	{
		return 0.0;
	}
	return sum() / static_cast<double>(c);
}

auto sliding_histogram::percentile(double p) const -> double
{
	auto snap = aggregate();
	if (snap.count == 0)
	{
		return 0.0;
	}

	// Clamp percentile
	p = std::clamp(p, 0.0, 1.0);

	// Use pre-calculated percentiles if available
	auto it = snap.percentiles.find(p);
	if (it != snap.percentiles.end())
	{
		return it->second;
	}

	// Calculate from buckets
	double target = p * static_cast<double>(snap.count);

	for (size_t i = 0; i < snap.buckets.size(); ++i)
	{
		if (static_cast<double>(snap.buckets[i].second) >= target)
		{
			double lower_bound = (i == 0) ? 0.0 : snap.buckets[i - 1].first;
			double upper_bound = snap.buckets[i].first;

			if (std::isinf(upper_bound))
			{
				return lower_bound;
			}

			uint64_t lower_count = (i == 0) ? 0 : snap.buckets[i - 1].second;
			uint64_t upper_count = snap.buckets[i].second;

			if (upper_count == lower_count)
			{
				return lower_bound;
			}

			double fraction = (target - static_cast<double>(lower_count))
							  / (static_cast<double>(upper_count) - static_cast<double>(lower_count));

			return lower_bound + (fraction * (upper_bound - lower_bound));
		}
	}

	return 0.0;
}

auto sliding_histogram::snapshot(const std::map<std::string, std::string>& labels) const
	-> histogram_snapshot
{
	auto snap = aggregate();
	snap.labels = labels;
	return snap;
}

void sliding_histogram::reset()
{
	std::lock_guard<std::mutex> lock(mutex_);
	buckets_.clear();
}

void sliding_histogram::expire_old_buckets()
{
	auto now = std::chrono::steady_clock::now();
	auto cutoff = now - config_.window_duration;

	while (!buckets_.empty() && buckets_.front()->start_time < cutoff)
	{
		buckets_.pop_front();
	}
}

auto sliding_histogram::get_current_bucket() -> time_bucket&
{
	auto now = std::chrono::steady_clock::now();

	if (buckets_.empty())
	{
		buckets_.push_back(std::make_unique<time_bucket>(config_.hist_config));
		return *buckets_.back();
	}

	auto& last = buckets_.back();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last->start_time);

	if (elapsed >= bucket_duration_)
	{
		buckets_.push_back(std::make_unique<time_bucket>(config_.hist_config));
	}

	return *buckets_.back();
}

auto sliding_histogram::aggregate() const -> histogram_snapshot
{
	std::lock_guard<std::mutex> lock(mutex_);
	const_cast<sliding_histogram*>(this)->expire_old_buckets();

	histogram_snapshot result;
	result.count = 0;
	result.sum = 0.0;
	result.min_value = std::numeric_limits<double>::infinity();
	result.max_value = -std::numeric_limits<double>::infinity();

	if (buckets_.empty())
	{
		return result;
	}

	// Aggregate statistics
	std::vector<std::pair<double, uint64_t>> merged_buckets;
	bool first_snapshot = true;

	for (const auto& bucket : buckets_)
	{
		auto snap = bucket->hist.snapshot();
		result.count += snap.count;
		result.sum += snap.sum;

		if (snap.min_value < result.min_value)
		{
			result.min_value = snap.min_value;
		}
		if (snap.max_value > result.max_value)
		{
			result.max_value = snap.max_value;
		}

		// Merge bucket counts
		if (first_snapshot)
		{
			merged_buckets = snap.buckets;
			first_snapshot = false;
		}
		else
		{
			// Need to convert from cumulative to individual counts, add, then back to cumulative
			std::vector<uint64_t> individual_counts(snap.buckets.size(), 0);
			for (size_t i = 0; i < snap.buckets.size(); ++i)
			{
				uint64_t prev = (i == 0) ? 0 : snap.buckets[i - 1].second;
				individual_counts[i] = snap.buckets[i].second - prev;
			}

			std::vector<uint64_t> merged_individual(merged_buckets.size(), 0);
			for (size_t i = 0; i < merged_buckets.size(); ++i)
			{
				uint64_t prev = (i == 0) ? 0 : merged_buckets[i - 1].second;
				merged_individual[i] = merged_buckets[i].second - prev;
			}

			// Add individual counts
			for (size_t i = 0; i < merged_buckets.size() && i < individual_counts.size(); ++i)
			{
				merged_individual[i] += individual_counts[i];
			}

			// Convert back to cumulative
			uint64_t cumulative = 0;
			for (size_t i = 0; i < merged_buckets.size(); ++i)
			{
				cumulative += merged_individual[i];
				merged_buckets[i].second = cumulative;
			}
		}
	}

	result.buckets = std::move(merged_buckets);

	// Calculate percentiles from merged buckets
	if (result.count > 0 && !result.buckets.empty())
	{
		auto calc_percentile = [&](double p) -> double
		{
			double target = p * static_cast<double>(result.count);

			for (size_t i = 0; i < result.buckets.size(); ++i)
			{
				if (static_cast<double>(result.buckets[i].second) >= target)
				{
					double lower_bound = (i == 0) ? 0.0 : result.buckets[i - 1].first;
					double upper_bound = result.buckets[i].first;

					if (std::isinf(upper_bound))
					{
						return lower_bound;
					}

					uint64_t lower_count = (i == 0) ? 0 : result.buckets[i - 1].second;
					uint64_t upper_count = result.buckets[i].second;

					if (upper_count == lower_count)
					{
						return lower_bound;
					}

					double fraction =
						(target - static_cast<double>(lower_count))
						/ (static_cast<double>(upper_count) - static_cast<double>(lower_count));

					return lower_bound + (fraction * (upper_bound - lower_bound));
				}
			}
			return 0.0;
		};

		result.percentiles[0.5] = calc_percentile(0.5);
		result.percentiles[0.9] = calc_percentile(0.9);
		result.percentiles[0.95] = calc_percentile(0.95);
		result.percentiles[0.99] = calc_percentile(0.99);
		result.percentiles[0.999] = calc_percentile(0.999);
	}

	return result;
}

} // namespace kcenon::network::metrics
