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

#pragma once

/**
 * @file monitoring_integration.h
 * @brief Monitoring system integration interface for network_system
 *
 * This interface provides integration with monitoring_system for metrics
 * collection and performance monitoring capabilities.
 *
 * @author kcenon
 * @date 2025-01-26
 */

#include <map>
#include <memory>
#include <string>

#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::integration
{

	/**
	 * @enum metric_type
	 * @brief Types of metrics supported
	 */
	enum class metric_type
	{
		counter,   /*!< Monotonically increasing value */
		gauge,     /*!< Value that can increase or decrease */
		histogram, /*!< Distribution of values */
		summary    /*!< Statistical summary of values */
	};

	/**
	 * @class monitoring_interface
	 * @brief Abstract interface for monitoring integration
	 *
	 * This interface allows network_system to work with any monitoring
	 * implementation, including the monitoring_system module.
	 */
	class monitoring_interface
	{
	public:
		virtual ~monitoring_interface() = default;

		/**
		 * @brief Report a counter metric
		 * @param name Metric name
		 * @param value Counter value
		 * @param labels Optional labels for the metric
		 */
		virtual void report_counter(const std::string& name, double value,
									const std::map<std::string, std::string>& labels = {}) = 0;

		/**
		 * @brief Report a gauge metric
		 * @param name Metric name
		 * @param value Gauge value
		 * @param labels Optional labels for the metric
		 */
		virtual void report_gauge(const std::string& name, double value,
								  const std::map<std::string, std::string>& labels = {}) = 0;

		/**
		 * @brief Report a histogram metric
		 * @param name Metric name
		 * @param value Histogram value
		 * @param labels Optional labels for the metric
		 */
		virtual void report_histogram(const std::string& name, double value,
									  const std::map<std::string, std::string>& labels = {}) = 0;

		/**
		 * @brief Report connection health metrics
		 * @param connection_id Connection identifier
		 * @param is_alive Connection alive status
		 * @param response_time_ms Response time in milliseconds
		 * @param missed_heartbeats Number of missed heartbeats
		 * @param packet_loss_rate Packet loss rate (0.0-1.0)
		 */
		virtual void report_health(const std::string& connection_id, bool is_alive,
								   double response_time_ms, size_t missed_heartbeats,
								   double packet_loss_rate) = 0;
	};

	/**
	 * @class basic_monitoring
	 * @brief Basic monitoring implementation for standalone use
	 *
	 * This provides a simple monitoring implementation for when
	 * monitoring_system is not available. Logs metrics to console.
	 */
	class basic_monitoring : public monitoring_interface
	{
	public:
		/**
		 * @brief Constructor
		 * @param enable_logging Enable console logging (default: true)
		 */
		explicit basic_monitoring(bool enable_logging = true);

		~basic_monitoring() override;

		// monitoring_interface implementation
		void report_counter(const std::string& name, double value,
							const std::map<std::string, std::string>& labels = {}) override;

		void report_gauge(const std::string& name, double value,
						  const std::map<std::string, std::string>& labels = {}) override;

		void report_histogram(const std::string& name, double value,
							  const std::map<std::string, std::string>& labels = {}) override;

		void report_health(const std::string& connection_id, bool is_alive,
						   double response_time_ms, size_t missed_heartbeats,
						   double packet_loss_rate) override;

		/**
		 * @brief Enable or disable logging
		 * @param enabled New logging state
		 */
		void set_logging_enabled(bool enabled);

		/**
		 * @brief Check if logging is enabled
		 * @return true if logging is enabled
		 */
		bool is_logging_enabled() const;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};

#ifdef BUILD_WITH_MONITORING_SYSTEM
	/**
	 * @class monitoring_system_adapter
	 * @brief Adapter for monitoring_system integration
	 *
	 * This adapter wraps monitoring_system to provide
	 * the monitoring_interface implementation.
	 */
	class monitoring_system_adapter : public monitoring_interface
	{
	public:
		/**
		 * @brief Constructor
		 * @param service_name Name of the service being monitored
		 */
		explicit monitoring_system_adapter(const std::string& service_name = "network_system");

		~monitoring_system_adapter() override;

		// monitoring_interface implementation
		void report_counter(const std::string& name, double value,
							const std::map<std::string, std::string>& labels = {}) override;

		void report_gauge(const std::string& name, double value,
						  const std::map<std::string, std::string>& labels = {}) override;

		void report_histogram(const std::string& name, double value,
							  const std::map<std::string, std::string>& labels = {}) override;

		void report_health(const std::string& connection_id, bool is_alive,
						   double response_time_ms, size_t missed_heartbeats,
						   double packet_loss_rate) override;

		/**
		 * @brief Start the monitoring system
		 */
		void start();

		/**
		 * @brief Stop the monitoring system
		 */
		void stop();

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};
#endif // BUILD_WITH_MONITORING_SYSTEM

	/**
	 * @class monitoring_integration_manager
	 * @brief Manager for monitoring system integration
	 *
	 * This class manages the integration between network_system and
	 * monitoring implementations.
	 */
	class monitoring_integration_manager
	{
	public:
		/**
		 * @brief Get the singleton instance
		 * @return Reference to the singleton instance
		 */
		static monitoring_integration_manager& instance();

		/**
		 * @brief Set the monitoring implementation
		 * @param monitoring Monitoring implementation to use
		 */
		void set_monitoring(std::shared_ptr<monitoring_interface> monitoring);

		/**
		 * @brief Get the current monitoring implementation
		 * @return Current monitoring implementation (creates basic monitoring if none set)
		 */
		std::shared_ptr<monitoring_interface> get_monitoring();

		/**
		 * @brief Report a counter metric
		 * @param name Metric name
		 * @param value Counter value
		 * @param labels Optional labels
		 */
		void report_counter(const std::string& name, double value,
							const std::map<std::string, std::string>& labels = {});

		/**
		 * @brief Report a gauge metric
		 * @param name Metric name
		 * @param value Gauge value
		 * @param labels Optional labels
		 */
		void report_gauge(const std::string& name, double value,
						  const std::map<std::string, std::string>& labels = {});

		/**
		 * @brief Report a histogram metric
		 * @param name Metric name
		 * @param value Histogram value
		 * @param labels Optional labels
		 */
		void report_histogram(const std::string& name, double value,
							  const std::map<std::string, std::string>& labels = {});

		/**
		 * @brief Report connection health metrics
		 * @param connection_id Connection identifier
		 * @param is_alive Connection alive status
		 * @param response_time_ms Response time in milliseconds
		 * @param missed_heartbeats Number of missed heartbeats
		 * @param packet_loss_rate Packet loss rate (0.0-1.0)
		 */
		void report_health(const std::string& connection_id, bool is_alive,
						   double response_time_ms, size_t missed_heartbeats,
						   double packet_loss_rate);

	private:
		monitoring_integration_manager();
		~monitoring_integration_manager();

		class impl;
		std::unique_ptr<impl> pimpl_;
	};

} // namespace kcenon::network::integration

// Backward compatibility namespace alias is defined in thread_integration.h
