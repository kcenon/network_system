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

#include "network_system/metrics/network_metrics.h"
#include "network_system/core/network_context.h"

#ifdef BUILD_WITH_MONITORING_SYSTEM
#include "network_system/integration/monitoring_integration.h"
#endif

namespace network_system::metrics {

void metric_reporter::report_connection_accepted() {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->increment_counter(metric_names::CONNECTIONS_TOTAL, 1);
    }
#endif
}

void metric_reporter::report_connection_failed(const std::string& reason) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->increment_counter(metric_names::CONNECTIONS_FAILED, 1);
    }
#else
    (void)reason; // Suppress unused parameter warning
#endif
}

void metric_reporter::report_bytes_sent(size_t bytes) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->increment_counter(metric_names::BYTES_SENT, bytes);
        monitoring->increment_counter(metric_names::PACKETS_SENT, 1);
    }
#else
    (void)bytes; // Suppress unused parameter warning
#endif
}

void metric_reporter::report_bytes_received(size_t bytes) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->increment_counter(metric_names::BYTES_RECEIVED, bytes);
        monitoring->increment_counter(metric_names::PACKETS_RECEIVED, 1);
    }
#else
    (void)bytes; // Suppress unused parameter warning
#endif
}

void metric_reporter::report_latency(double ms) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->record_value(metric_names::LATENCY_MS, ms);
    }
#else
    (void)ms; // Suppress unused parameter warning
#endif
}

void metric_reporter::report_error(const std::string& error_type) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->increment_counter(metric_names::ERRORS_TOTAL, 1);
    }
#else
    (void)error_type; // Suppress unused parameter warning
#endif
}

void metric_reporter::report_timeout() {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->increment_counter(metric_names::TIMEOUTS_TOTAL, 1);
    }
#endif
}

void metric_reporter::report_active_connections(size_t count) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->set_gauge(metric_names::CONNECTIONS_ACTIVE, static_cast<double>(count));
    }
#else
    (void)count; // Suppress unused parameter warning
#endif
}

void metric_reporter::report_session_duration(double ms) {
#ifdef BUILD_WITH_MONITORING_SYSTEM
    auto monitoring = core::network_context::instance().get_monitoring();
    if (monitoring) {
        monitoring->record_value(metric_names::SESSION_DURATION_MS, ms);
    }
#else
    (void)ms; // Suppress unused parameter warning
#endif
}

} // namespace network_system::metrics
