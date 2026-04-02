// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "internal/integration/monitoring_integration.h"
#include "internal/integration/logger_integration.h"

#include <atomic>
#include <mutex>
#include <sstream>
#include <unordered_map>

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

	// Note: monitoring_system_adapter has been removed in favor of EventBus-based
	// metric publishing. See issue #342 for details.

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
