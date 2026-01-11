/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#pragma once

#include "kcenon/network/integration/monitoring_integration.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace kcenon::network::testing
{

/**
 * @brief Recorded metric data for test verification
 */
struct recorded_metric
{
	std::string name;
	double value;
	std::map<std::string, std::string> labels;
};

/**
 * @brief Recorded health data for test verification
 */
struct recorded_health
{
	std::string connection_id;
	bool is_alive;
	double response_time_ms;
	size_t missed_heartbeats;
	double packet_loss_rate;
};

/**
 * @brief Mock implementation of monitoring_interface for testing
 *
 * This mock records all reported metrics for later verification in tests.
 * Thread-safe for concurrent metric reporting tests.
 */
class mock_monitor : public integration::monitoring_interface
{
public:
	mock_monitor() = default;
	~mock_monitor() override = default;

	// Non-copyable and non-movable (contains mutex)
	mock_monitor(const mock_monitor&) = delete;
	mock_monitor& operator=(const mock_monitor&) = delete;
	mock_monitor(mock_monitor&&) = delete;
	mock_monitor& operator=(mock_monitor&&) = delete;

	// monitoring_interface implementation
	void report_counter(const std::string& name, double value,
	                    const std::map<std::string, std::string>& labels = {}) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		counters_.push_back({name, value, labels});
		counter_call_count_.fetch_add(1, std::memory_order_relaxed);
	}

	void report_gauge(const std::string& name, double value,
	                  const std::map<std::string, std::string>& labels = {}) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		gauges_.push_back({name, value, labels});
		gauge_call_count_.fetch_add(1, std::memory_order_relaxed);
	}

	void report_histogram(const std::string& name, double value,
	                      const std::map<std::string, std::string>& labels = {}) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		histograms_.push_back({name, value, labels});
		histogram_call_count_.fetch_add(1, std::memory_order_relaxed);
	}

	void report_health(const std::string& connection_id, bool is_alive,
	                   double response_time_ms, size_t missed_heartbeats,
	                   double packet_loss_rate) override
	{
		std::lock_guard<std::mutex> lock(mutex_);
		health_reports_.push_back(
			{connection_id, is_alive, response_time_ms, missed_heartbeats, packet_loss_rate});
		health_call_count_.fetch_add(1, std::memory_order_relaxed);
	}

	// Test verification methods
	[[nodiscard]] auto get_counters() const -> std::vector<recorded_metric>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return counters_;
	}

	[[nodiscard]] auto get_gauges() const -> std::vector<recorded_metric>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return gauges_;
	}

	[[nodiscard]] auto get_histograms() const -> std::vector<recorded_metric>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return histograms_;
	}

	[[nodiscard]] auto get_health_reports() const -> std::vector<recorded_health>
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return health_reports_;
	}

	[[nodiscard]] auto counter_call_count() const noexcept -> size_t
	{
		return counter_call_count_.load(std::memory_order_relaxed);
	}

	[[nodiscard]] auto gauge_call_count() const noexcept -> size_t
	{
		return gauge_call_count_.load(std::memory_order_relaxed);
	}

	[[nodiscard]] auto histogram_call_count() const noexcept -> size_t
	{
		return histogram_call_count_.load(std::memory_order_relaxed);
	}

	[[nodiscard]] auto health_call_count() const noexcept -> size_t
	{
		return health_call_count_.load(std::memory_order_relaxed);
	}

	[[nodiscard]] auto total_call_count() const noexcept -> size_t
	{
		return counter_call_count() + gauge_call_count() + histogram_call_count()
		       + health_call_count();
	}

	void clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		counters_.clear();
		gauges_.clear();
		histograms_.clear();
		health_reports_.clear();
		counter_call_count_.store(0, std::memory_order_relaxed);
		gauge_call_count_.store(0, std::memory_order_relaxed);
		histogram_call_count_.store(0, std::memory_order_relaxed);
		health_call_count_.store(0, std::memory_order_relaxed);
	}

	// Helper methods for test assertions
	[[nodiscard]] auto has_counter(const std::string& name) const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (const auto& metric : counters_)
		{
			if (metric.name == name)
			{
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] auto has_gauge(const std::string& name) const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (const auto& metric : gauges_)
		{
			if (metric.name == name)
			{
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] auto has_histogram(const std::string& name) const -> bool
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (const auto& metric : histograms_)
		{
			if (metric.name == name)
			{
				return true;
			}
		}
		return false;
	}

	[[nodiscard]] auto get_counter_value(const std::string& name) const -> double
	{
		std::lock_guard<std::mutex> lock(mutex_);
		double total = 0.0;
		for (const auto& metric : counters_)
		{
			if (metric.name == name)
			{
				total += metric.value;
			}
		}
		return total;
	}

	[[nodiscard]] auto get_gauge_value(const std::string& name) const -> double
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (auto it = gauges_.rbegin(); it != gauges_.rend(); ++it)
		{
			if (it->name == name)
			{
				return it->value;
			}
		}
		return 0.0;
	}

private:
	mutable std::mutex mutex_;
	std::vector<recorded_metric> counters_;
	std::vector<recorded_metric> gauges_;
	std::vector<recorded_metric> histograms_;
	std::vector<recorded_health> health_reports_;

	std::atomic<size_t> counter_call_count_{0};
	std::atomic<size_t> gauge_call_count_{0};
	std::atomic<size_t> histogram_call_count_{0};
	std::atomic<size_t> health_call_count_{0};
};

} // namespace kcenon::network::testing
