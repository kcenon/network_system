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

#include "kcenon/network/metrics/network_metrics.h"
#include "kcenon/network/integration/monitoring_integration.h"
#include "kcenon/network/config/feature_flags.h"

#if KCENON_WITH_COMMON_SYSTEM
#include "kcenon/network/events/network_metric_event.h"
#include <kcenon/common/patterns/event_bus.h>
#endif

namespace kcenon::network::metrics {

namespace {

#if KCENON_WITH_COMMON_SYSTEM
/**
 * @brief Publish a metric event to the EventBus
 * @param name Metric name
 * @param value Metric value
 * @param type Metric type
 * @param labels Additional labels
 *
 * Note: Only available when common_system is enabled (provides EventBus).
 */
void publish_metric(const std::string& name, double value,
					events::network_metric_type type,
					const std::map<std::string, std::string>& labels = {})
{
	auto& bus = kcenon::common::get_event_bus();
	bus.publish(events::network_metric_event{name, value, type, labels});
}
#endif // KCENON_WITH_COMMON_SYSTEM

} // anonymous namespace

void metric_reporter::report_connection_accepted()
{
#if KCENON_WITH_COMMON_SYSTEM
	// Publish via EventBus for external consumers (e.g., monitoring_system)
	publish_metric(metric_names::CONNECTIONS_TOTAL, 1.0,
				   events::network_metric_type::counter,
				   {{"event", "accepted"}});
#endif

	// Report to local monitoring integration
	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::CONNECTIONS_TOTAL, 1.0);
}

void metric_reporter::report_connection_failed(const std::string& reason)
{
#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::CONNECTIONS_FAILED, 1.0,
				   events::network_metric_type::counter,
				   {{"reason", reason}});
#endif

	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::CONNECTIONS_FAILED, 1.0, {{"reason", reason}});
}

void metric_reporter::report_bytes_sent(size_t bytes)
{
	auto byte_value = static_cast<double>(bytes);

#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::BYTES_SENT, byte_value,
				   events::network_metric_type::counter);
	publish_metric(metric_names::PACKETS_SENT, 1.0,
				   events::network_metric_type::counter);
#endif

	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::BYTES_SENT, byte_value);
	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::PACKETS_SENT, 1.0);
}

void metric_reporter::report_bytes_received(size_t bytes)
{
	auto byte_value = static_cast<double>(bytes);

#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::BYTES_RECEIVED, byte_value,
				   events::network_metric_type::counter);
	publish_metric(metric_names::PACKETS_RECEIVED, 1.0,
				   events::network_metric_type::counter);
#endif

	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::BYTES_RECEIVED, byte_value);
	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::PACKETS_RECEIVED, 1.0);
}

void metric_reporter::report_latency(double ms)
{
#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::LATENCY_MS, ms,
				   events::network_metric_type::histogram);
#endif

	integration::monitoring_integration_manager::instance().report_histogram(
		metric_names::LATENCY_MS, ms);
}

void metric_reporter::report_error(const std::string& error_type)
{
#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::ERRORS_TOTAL, 1.0,
				   events::network_metric_type::counter,
				   {{"error_type", error_type}});
#endif

	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::ERRORS_TOTAL, 1.0, {{"error_type", error_type}});
}

void metric_reporter::report_timeout()
{
#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::TIMEOUTS_TOTAL, 1.0,
				   events::network_metric_type::counter);
#endif

	integration::monitoring_integration_manager::instance().report_counter(
		metric_names::TIMEOUTS_TOTAL, 1.0);
}

void metric_reporter::report_active_connections(size_t count)
{
	auto count_value = static_cast<double>(count);

#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::CONNECTIONS_ACTIVE, count_value,
				   events::network_metric_type::gauge);
#endif

	integration::monitoring_integration_manager::instance().report_gauge(
		metric_names::CONNECTIONS_ACTIVE, count_value);
}

void metric_reporter::report_session_duration(double ms)
{
#if KCENON_WITH_COMMON_SYSTEM
	publish_metric(metric_names::SESSION_DURATION_MS, ms,
				   events::network_metric_type::histogram);
#endif

	integration::monitoring_integration_manager::instance().report_histogram(
		metric_names::SESSION_DURATION_MS, ms);
}

} // namespace kcenon::network::metrics
