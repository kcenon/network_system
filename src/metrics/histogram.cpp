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

#include "kcenon/network/detail/metrics/histogram.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace kcenon::network::metrics {

// ============================================================================
// histogram_snapshot implementation
// ============================================================================

auto histogram_snapshot::to_prometheus(const std::string& name) const -> std::string
{
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6);

	// Build label string
	std::string label_str;
	if (!labels.empty())
	{
		std::ostringstream label_oss;
		label_oss << "{";
		bool first = true;
		for (const auto& [key, value] : labels)
		{
			if (!first)
			{
				label_oss << ",";
			}
			label_oss << key << "=\"" << value << "\"";
			first = false;
		}
		label_oss << "}";
		label_str = label_oss.str();
	}

	// Output bucket counts (cumulative)
	for (const auto& [boundary, bucket_count] : buckets)
	{
		oss << name << "_bucket{le=\"";
		if (boundary == std::numeric_limits<double>::infinity())
		{
			oss << "+Inf";
		}
		else
		{
			oss << boundary;
		}
		oss << "\"";
		if (!labels.empty())
		{
			oss << "," << label_str.substr(1, label_str.length() - 2);
		}
		oss << "} " << bucket_count << "\n";
	}

	// Output sum and count
	oss << name << "_sum";
	if (!labels.empty())
	{
		oss << label_str;
	}
	oss << " " << sum << "\n";

	oss << name << "_count";
	if (!labels.empty())
	{
		oss << label_str;
	}
	oss << " " << count << "\n";

	return oss.str();
}

auto histogram_snapshot::to_json() const -> std::string
{
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6);

	oss << "{";
	oss << "\"count\":" << count << ",";
	oss << "\"sum\":" << sum << ",";
	oss << "\"min\":" << min_value << ",";
	oss << "\"max\":" << max_value << ",";

	// Percentiles
	oss << "\"percentiles\":{";
	bool first = true;
	for (const auto& [p, v] : percentiles)
	{
		if (!first)
		{
			oss << ",";
		}
		oss << "\"" << p << "\":" << v;
		first = false;
	}
	oss << "},";

	// Buckets
	oss << "\"buckets\":[";
	first = true;
	for (const auto& [boundary, bucket_count] : buckets)
	{
		if (!first)
		{
			oss << ",";
		}
		oss << "{\"le\":";
		if (boundary == std::numeric_limits<double>::infinity())
		{
			oss << "\"+Inf\"";
		}
		else
		{
			oss << boundary;
		}
		oss << ",\"count\":" << bucket_count << "}";
		first = false;
	}
	oss << "],";

	// Labels
	oss << "\"labels\":{";
	first = true;
	for (const auto& [key, value] : labels)
	{
		if (!first)
		{
			oss << ",";
		}
		oss << "\"" << key << "\":\"" << value << "\"";
		first = false;
	}
	oss << "}";

	oss << "}";

	return oss.str();
}

// ============================================================================
// histogram implementation
// ============================================================================

histogram::histogram(histogram_config cfg)
{
	if (cfg.bucket_boundaries.empty())
	{
		cfg = histogram_config::default_latency_config();
	}

	boundaries_ = std::move(cfg.bucket_boundaries);
	std::sort(boundaries_.begin(), boundaries_.end());

	// Add +Inf bucket
	boundaries_.push_back(std::numeric_limits<double>::infinity());

	// Initialize bucket counts
	bucket_count_ = boundaries_.size();
	bucket_counts_ = std::make_unique<std::atomic<uint64_t>[]>(bucket_count_);
	for (size_t i = 0; i < bucket_count_; ++i)
	{
		bucket_counts_[i].store(0, std::memory_order_relaxed);
	}
}

histogram::histogram(histogram&& other) noexcept
	: boundaries_(std::move(other.boundaries_))
	, bucket_counts_(std::move(other.bucket_counts_))
	, bucket_count_(other.bucket_count_)
	, count_(other.count_.load(std::memory_order_relaxed))
	, sum_(other.sum_.load(std::memory_order_relaxed))
	, min_(other.min_.load(std::memory_order_relaxed))
	, max_(other.max_.load(std::memory_order_relaxed))
{
	other.bucket_count_ = 0;
}

auto histogram::operator=(histogram&& other) noexcept -> histogram&
{
	if (this != &other)
	{
		boundaries_ = std::move(other.boundaries_);
		bucket_counts_ = std::move(other.bucket_counts_);
		bucket_count_ = other.bucket_count_;
		count_.store(other.count_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		sum_.store(other.sum_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		min_.store(other.min_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		max_.store(other.max_.load(std::memory_order_relaxed), std::memory_order_relaxed);
		other.bucket_count_ = 0;
	}
	return *this;
}

void histogram::record(double value)
{
	size_t bucket_idx = find_bucket(value);
	bucket_counts_[bucket_idx].fetch_add(1, std::memory_order_relaxed);

	count_.fetch_add(1, std::memory_order_relaxed);
	add_to_sum(value);
	update_min(value);
	update_max(value);
}

auto histogram::count() const -> uint64_t
{
	return count_.load(std::memory_order_relaxed);
}

auto histogram::sum() const -> double
{
	return sum_.load(std::memory_order_relaxed);
}

auto histogram::min() const -> double
{
	return min_.load(std::memory_order_relaxed);
}

auto histogram::max() const -> double
{
	return max_.load(std::memory_order_relaxed);
}

auto histogram::mean() const -> double
{
	uint64_t c = count();
	if (c == 0)
	{
		return 0.0;
	}
	return sum() / static_cast<double>(c);
}

auto histogram::percentile(double p) const -> double
{
	std::lock_guard<std::mutex> lock(mutex_);

	uint64_t total = count_.load(std::memory_order_relaxed);
	if (total == 0)
	{
		return 0.0;
	}

	// Clamp percentile to valid range
	p = std::clamp(p, 0.0, 1.0);

	// Target count for this percentile
	double target = p * static_cast<double>(total);

	// Calculate cumulative counts
	std::vector<uint64_t> cumulative;
	cumulative.reserve(bucket_count_);
	uint64_t running_sum = 0;
	for (size_t i = 0; i < bucket_count_; ++i)
	{
		running_sum += bucket_counts_[i].load(std::memory_order_relaxed);
		cumulative.push_back(running_sum);
	}

	// Find the bucket containing the target count
	size_t bucket_idx = 0;
	for (size_t i = 0; i < cumulative.size(); ++i)
	{
		if (static_cast<double>(cumulative[i]) >= target)
		{
			bucket_idx = i;
			break;
		}
	}

	// Linear interpolation within the bucket
	double lower_bound = (bucket_idx == 0) ? 0.0 : boundaries_[bucket_idx - 1];
	double upper_bound = boundaries_[bucket_idx];

	// Handle infinity bucket
	if (std::isinf(upper_bound))
	{
		return lower_bound;
	}

	uint64_t lower_count = (bucket_idx == 0) ? 0 : cumulative[bucket_idx - 1];
	uint64_t upper_count = cumulative[bucket_idx];

	if (upper_count == lower_count)
	{
		return lower_bound;
	}

	double fraction = (target - static_cast<double>(lower_count))
					  / (static_cast<double>(upper_count) - static_cast<double>(lower_count));

	return lower_bound + (fraction * (upper_bound - lower_bound));
}

auto histogram::buckets() const -> std::vector<std::pair<double, uint64_t>>
{
	std::lock_guard<std::mutex> lock(mutex_);

	std::vector<std::pair<double, uint64_t>> result;
	result.reserve(bucket_count_);

	uint64_t cumulative = 0;
	for (size_t i = 0; i < bucket_count_; ++i)
	{
		cumulative += bucket_counts_[i].load(std::memory_order_relaxed);
		result.emplace_back(boundaries_[i], cumulative);
	}

	return result;
}

auto histogram::snapshot(const std::map<std::string, std::string>& labels) const
	-> histogram_snapshot
{
	histogram_snapshot snap;
	snap.count = count();
	snap.sum = sum();
	snap.min_value = min();
	snap.max_value = max();
	snap.labels = labels;

	// Calculate percentiles
	snap.percentiles[0.5] = p50();
	snap.percentiles[0.9] = percentile(0.9);
	snap.percentiles[0.95] = p95();
	snap.percentiles[0.99] = p99();
	snap.percentiles[0.999] = p999();

	// Get buckets
	snap.buckets = buckets();

	return snap;
}

void histogram::reset()
{
	std::lock_guard<std::mutex> lock(mutex_);

	count_.store(0, std::memory_order_relaxed);
	sum_.store(0.0, std::memory_order_relaxed);
	min_.store(std::numeric_limits<double>::infinity(), std::memory_order_relaxed);
	max_.store(-std::numeric_limits<double>::infinity(), std::memory_order_relaxed);

	for (size_t i = 0; i < bucket_count_; ++i)
	{
		bucket_counts_[i].store(0, std::memory_order_relaxed);
	}
}

auto histogram::find_bucket(double value) const -> size_t
{
	auto it = std::lower_bound(boundaries_.begin(), boundaries_.end(), value);
	if (it == boundaries_.end())
	{
		return boundaries_.size() - 1;
	}
	return static_cast<size_t>(std::distance(boundaries_.begin(), it));
}

void histogram::update_min(double value)
{
	double current = min_.load(std::memory_order_relaxed);
	while (value < current && !min_.compare_exchange_weak(current, value, std::memory_order_relaxed,
														  std::memory_order_relaxed))
	{
		// current is updated by compare_exchange_weak
	}
}

void histogram::update_max(double value)
{
	double current = max_.load(std::memory_order_relaxed);
	while (value > current && !max_.compare_exchange_weak(current, value, std::memory_order_relaxed,
														  std::memory_order_relaxed))
	{
		// current is updated by compare_exchange_weak
	}
}

void histogram::add_to_sum(double value)
{
	double current = sum_.load(std::memory_order_relaxed);
	while (!sum_.compare_exchange_weak(current, current + value, std::memory_order_relaxed,
									   std::memory_order_relaxed))
	{
		// current is updated by compare_exchange_weak
	}
}

} // namespace kcenon::network::metrics
