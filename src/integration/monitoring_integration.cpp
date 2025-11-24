/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include "kcenon/network/integration/monitoring_integration.h"
#include "kcenon/network/integration/logger_integration.h"

#include <atomic>
#include <mutex>
#include <sstream>
#include <unordered_map>

#ifdef BUILD_WITH_MONITORING_SYSTEM
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/health/health_monitor.h>
#include <kcenon/monitoring/utils/metric_types.h>
#endif

namespace kcenon::network::integration
{

	//===========================================================================
	// basic_monitoring implementation
	//===========================================================================

	class basic_monitoring::impl
	{
	public:
		explicit impl(bool enable_logging) : logging_enabled_(enable_logging) {}

		void report_counter(const std::string& name, double value,
							const std::map<std::string, std::string>& labels)
		{
			if (!logging_enabled_)
				return;

			std::ostringstream oss;
			oss << "[MONITORING] Counter: " << name << " = " << value;
			if (!labels.empty())
			{
				oss << " {";
				bool first = true;
				for (const auto& [key, val] : labels)
				{
					if (!first)
						oss << ", ";
					oss << key << "=" << val;
					first = false;
				}
				oss << "}";
			}
			NETWORK_LOG_DEBUG(oss.str());
		}

		void report_gauge(const std::string& name, double value,
						  const std::map<std::string, std::string>& labels)
		{
			if (!logging_enabled_)
				return;

			std::ostringstream oss;
			oss << "[MONITORING] Gauge: " << name << " = " << value;
			if (!labels.empty())
			{
				oss << " {";
				bool first = true;
				for (const auto& [key, val] : labels)
				{
					if (!first)
						oss << ", ";
					oss << key << "=" << val;
					first = false;
				}
				oss << "}";
			}
			NETWORK_LOG_DEBUG(oss.str());
		}

		void report_histogram(const std::string& name, double value,
							  const std::map<std::string, std::string>& labels)
		{
			if (!logging_enabled_)
				return;

			std::ostringstream oss;
			oss << "[MONITORING] Histogram: " << name << " = " << value;
			if (!labels.empty())
			{
				oss << " {";
				bool first = true;
				for (const auto& [key, val] : labels)
				{
					if (!first)
						oss << ", ";
					oss << key << "=" << val;
					first = false;
				}
				oss << "}";
			}
			NETWORK_LOG_DEBUG(oss.str());
		}

		void report_health(const std::string& connection_id, bool is_alive,
						   double response_time_ms, size_t missed_heartbeats,
						   double packet_loss_rate)
		{
			if (!logging_enabled_)
				return;

			std::ostringstream oss;
			oss << "[MONITORING] Health: " << connection_id << " - alive=" << is_alive
				<< ", response_time=" << response_time_ms << "ms"
				<< ", missed_heartbeats=" << missed_heartbeats
				<< ", packet_loss=" << (packet_loss_rate * 100.0) << "%";
			NETWORK_LOG_DEBUG(oss.str());
		}

		void set_logging_enabled(bool enabled) { logging_enabled_ = enabled; }

		bool is_logging_enabled() const { return logging_enabled_; }

	private:
		bool logging_enabled_;
	};

	basic_monitoring::basic_monitoring(bool enable_logging)
		: pimpl_(std::make_unique<impl>(enable_logging))
	{
	}

	basic_monitoring::~basic_monitoring() = default;

	void basic_monitoring::report_counter(const std::string& name, double value,
										  const std::map<std::string, std::string>& labels)
	{
		pimpl_->report_counter(name, value, labels);
	}

	void basic_monitoring::report_gauge(const std::string& name, double value,
										const std::map<std::string, std::string>& labels)
	{
		pimpl_->report_gauge(name, value, labels);
	}

	void basic_monitoring::report_histogram(const std::string& name, double value,
											const std::map<std::string, std::string>& labels)
	{
		pimpl_->report_histogram(name, value, labels);
	}

	void basic_monitoring::report_health(const std::string& connection_id, bool is_alive,
										 double response_time_ms, size_t missed_heartbeats,
										 double packet_loss_rate)
	{
		pimpl_->report_health(connection_id, is_alive, response_time_ms, missed_heartbeats,
							  packet_loss_rate);
	}

	void basic_monitoring::set_logging_enabled(bool enabled)
	{
		pimpl_->set_logging_enabled(enabled);
	}

	bool basic_monitoring::is_logging_enabled() const { return pimpl_->is_logging_enabled(); }

#ifdef BUILD_WITH_MONITORING_SYSTEM
	//===========================================================================
	// monitoring_system_adapter implementation
	//===========================================================================

	class monitoring_system_adapter::impl
	{
	public:
		explicit impl(const std::string& service_name)
			: service_name_(service_name)
			, performance_monitor_(service_name)
			, running_(false)
		{
			health_monitor_ = std::make_unique<kcenon::monitoring::health_monitor>();
			NETWORK_LOG_INFO("[monitoring_system_adapter] Created for service: " + service_name);
		}

		void report_counter(const std::string& name, double value,
							const std::map<std::string, std::string>& labels)
		{
			// Record counter as performance sample with value as duration (nanoseconds)
			auto duration = std::chrono::nanoseconds(static_cast<int64_t>(value));
			performance_monitor_.get_profiler().record_sample(
				build_metric_name(name, labels), duration, true);
			NETWORK_LOG_DEBUG("[monitoring_system_adapter] Counter: " + name + " = "
							  + std::to_string(value));
		}

		void report_gauge(const std::string& name, double value,
						  const std::map<std::string, std::string>& labels)
		{
			// Store gauge value and record to profiler
			{
				std::lock_guard<std::mutex> lock(gauges_mutex_);
				gauges_[build_metric_name(name, labels)] = value;
			}
			auto duration = std::chrono::nanoseconds(static_cast<int64_t>(value * 1000));
			performance_monitor_.get_profiler().record_sample(
				build_metric_name(name, labels), duration, true);
			NETWORK_LOG_DEBUG("[monitoring_system_adapter] Gauge: " + name + " = "
							  + std::to_string(value));
		}

		void report_histogram(const std::string& name, double value,
							  const std::map<std::string, std::string>& labels)
		{
			// Record histogram value as duration sample
			auto duration = std::chrono::nanoseconds(
				static_cast<int64_t>(value * 1000000)); // Convert to ns
			performance_monitor_.get_profiler().record_sample(
				build_metric_name(name, labels), duration, true);
			NETWORK_LOG_DEBUG("[monitoring_system_adapter] Histogram: " + name + " = "
							  + std::to_string(value));
		}

		void report_health(const std::string& connection_id, bool is_alive,
						   double response_time_ms, size_t missed_heartbeats,
						   double packet_loss_rate)
		{
			// Create health check for this connection
			auto check = std::make_shared<kcenon::monitoring::functional_health_check>(
				connection_id, kcenon::monitoring::health_check_type::liveness,
				[is_alive, response_time_ms, missed_heartbeats,
				 packet_loss_rate]() -> kcenon::monitoring::health_check_result {
					kcenon::monitoring::health_check_result result;
					if (!is_alive)
					{
						result.status = kcenon::monitoring::health_status::unhealthy;
						result.message = "Connection is not alive";
					}
					else if (missed_heartbeats > 3 || packet_loss_rate > 0.1)
					{
						result.status = kcenon::monitoring::health_status::degraded;
						result.message = "Connection degraded: missed_heartbeats="
										 + std::to_string(missed_heartbeats) + ", packet_loss="
										 + std::to_string(packet_loss_rate * 100) + "%";
					}
					else
					{
						result.status = kcenon::monitoring::health_status::healthy;
						result.message = "Connection healthy, response_time="
										 + std::to_string(response_time_ms) + "ms";
					}
					result.timestamp = std::chrono::system_clock::now();
					return result;
				});
			health_monitor_->register_check(connection_id, check);
			NETWORK_LOG_DEBUG("[monitoring_system_adapter] Health: " + connection_id
							  + " alive=" + (is_alive ? "true" : "false"));
		}

		void start()
		{
			if (running_.exchange(true))
			{
				return; // Already running
			}
			performance_monitor_.initialize();
			health_monitor_->start();
			NETWORK_LOG_INFO("[monitoring_system_adapter] Started");
		}

		void stop()
		{
			if (!running_.exchange(false))
			{
				return; // Already stopped
			}
			health_monitor_->stop();
			performance_monitor_.cleanup();
			NETWORK_LOG_INFO("[monitoring_system_adapter] Stopped");
		}

	private:
		std::string build_metric_name(const std::string& name,
									  const std::map<std::string, std::string>& labels) const
		{
			if (labels.empty())
			{
				return service_name_ + "." + name;
			}
			std::ostringstream oss;
			oss << service_name_ << "." << name << "{";
			bool first = true;
			for (const auto& [key, val] : labels)
			{
				if (!first)
					oss << ",";
				oss << key << "=" << val;
				first = false;
			}
			oss << "}";
			return oss.str();
		}

		std::string service_name_;
		kcenon::monitoring::performance_monitor performance_monitor_;
		std::unique_ptr<kcenon::monitoring::health_monitor> health_monitor_;
		std::atomic<bool> running_;
		std::mutex gauges_mutex_;
		std::unordered_map<std::string, double> gauges_;
	};

	monitoring_system_adapter::monitoring_system_adapter(const std::string& service_name)
		: pimpl_(std::make_unique<impl>(service_name))
	{
	}

	monitoring_system_adapter::~monitoring_system_adapter() = default;

	void monitoring_system_adapter::report_counter(
		const std::string& name, double value, const std::map<std::string, std::string>& labels)
	{
		pimpl_->report_counter(name, value, labels);
	}

	void monitoring_system_adapter::report_gauge(const std::string& name, double value,
												 const std::map<std::string, std::string>& labels)
	{
		pimpl_->report_gauge(name, value, labels);
	}

	void monitoring_system_adapter::report_histogram(
		const std::string& name, double value, const std::map<std::string, std::string>& labels)
	{
		pimpl_->report_histogram(name, value, labels);
	}

	void monitoring_system_adapter::report_health(const std::string& connection_id, bool is_alive,
												  double response_time_ms,
												  size_t missed_heartbeats,
												  double packet_loss_rate)
	{
		pimpl_->report_health(connection_id, is_alive, response_time_ms, missed_heartbeats,
							  packet_loss_rate);
	}

	void monitoring_system_adapter::start() { pimpl_->start(); }

	void monitoring_system_adapter::stop() { pimpl_->stop(); }

#endif // BUILD_WITH_MONITORING_SYSTEM

	//===========================================================================
	// monitoring_integration_manager implementation
	//===========================================================================

	class monitoring_integration_manager::impl
	{
	public:
		impl() = default;

		void set_monitoring(std::shared_ptr<monitoring_interface> monitoring)
		{
			std::lock_guard<std::mutex> lock(mutex_);
			monitoring_ = monitoring;
		}

		std::shared_ptr<monitoring_interface> get_monitoring()
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (!monitoring_)
			{
				// Create default basic monitoring if none set
				monitoring_ = std::make_shared<basic_monitoring>(true);
			}
			return monitoring_;
		}

	private:
		std::mutex mutex_;
		std::shared_ptr<monitoring_interface> monitoring_;
	};

	monitoring_integration_manager::monitoring_integration_manager()
		: pimpl_(std::make_unique<impl>())
	{
	}

	monitoring_integration_manager::~monitoring_integration_manager() = default;

	monitoring_integration_manager& monitoring_integration_manager::instance()
	{
		static monitoring_integration_manager instance;
		return instance;
	}

	void monitoring_integration_manager::set_monitoring(
		std::shared_ptr<monitoring_interface> monitoring)
	{
		pimpl_->set_monitoring(monitoring);
	}

	std::shared_ptr<monitoring_interface> monitoring_integration_manager::get_monitoring()
	{
		return pimpl_->get_monitoring();
	}

	void monitoring_integration_manager::report_counter(
		const std::string& name, double value, const std::map<std::string, std::string>& labels)
	{
		get_monitoring()->report_counter(name, value, labels);
	}

	void monitoring_integration_manager::report_gauge(
		const std::string& name, double value, const std::map<std::string, std::string>& labels)
	{
		get_monitoring()->report_gauge(name, value, labels);
	}

	void monitoring_integration_manager::report_histogram(
		const std::string& name, double value, const std::map<std::string, std::string>& labels)
	{
		get_monitoring()->report_histogram(name, value, labels);
	}

	void monitoring_integration_manager::report_health(const std::string& connection_id,
													   bool is_alive, double response_time_ms,
													   size_t missed_heartbeats,
													   double packet_loss_rate)
	{
		get_monitoring()->report_health(connection_id, is_alive, response_time_ms,
										missed_heartbeats, packet_loss_rate);
	}

} // namespace kcenon::network::integration
