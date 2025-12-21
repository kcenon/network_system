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
 * @file network_metrics.h
 * @brief Network system metrics definitions and reporting utilities
 *
 * @author kcenon
 * @date 2025-01-13
 */

#pragma once

#include <string>
#include <cstddef>

namespace kcenon::network::metrics {

/**
 * @brief Standard metric names for network system monitoring
 */
namespace metric_names {
    // Connection metrics
    constexpr const char* CONNECTIONS_ACTIVE = "network.connections.active";
    constexpr const char* CONNECTIONS_TOTAL = "network.connections.total";
    constexpr const char* CONNECTIONS_FAILED = "network.connections.failed";

    // Transfer metrics
    constexpr const char* BYTES_SENT = "network.bytes.sent";
    constexpr const char* BYTES_RECEIVED = "network.bytes.received";
    constexpr const char* PACKETS_SENT = "network.packets.sent";
    constexpr const char* PACKETS_RECEIVED = "network.packets.received";

    // Performance metrics
    constexpr const char* LATENCY_MS = "network.latency.ms";
    constexpr const char* THROUGHPUT_MBPS = "network.throughput.mbps";
    constexpr const char* SESSION_DURATION_MS = "network.session.duration.ms";

    // Error metrics
    constexpr const char* ERRORS_TOTAL = "network.errors.total";
    constexpr const char* TIMEOUTS_TOTAL = "network.timeouts.total";

    // Server metrics
    constexpr const char* SERVER_START_TIME = "network.server.start_time.ms";
    constexpr const char* SERVER_ACCEPT_COUNT = "network.server.accept.count";
    constexpr const char* SERVER_ACCEPT_FAILED = "network.server.accept.failed";
}

/**
 * @class metric_reporter
 * @brief Helper class for reporting network metrics
 *
 * This class provides convenient methods to report network-related metrics
 * to the monitoring system (if available).
 */
class metric_reporter {
public:
    /**
     * @brief Report a new connection accepted
     */
    static void report_connection_accepted();

    /**
     * @brief Report a failed connection attempt
     * @param reason Reason for the failure
     */
    static void report_connection_failed(const std::string& reason);

    /**
     * @brief Report bytes sent
     * @param bytes Number of bytes sent
     */
    static void report_bytes_sent(size_t bytes);

    /**
     * @brief Report bytes received
     * @param bytes Number of bytes received
     */
    static void report_bytes_received(size_t bytes);

    /**
     * @brief Report network latency
     * @param ms Latency in milliseconds
     */
    static void report_latency(double ms);

    /**
     * @brief Report a network error
     * @param error_type Type of error
     */
    static void report_error(const std::string& error_type);

    /**
     * @brief Report a timeout
     */
    static void report_timeout();

    /**
     * @brief Report active connections count
     * @param count Number of active connections
     */
    static void report_active_connections(size_t count);

    /**
     * @brief Report session duration
     * @param ms Duration in milliseconds
     */
    static void report_session_duration(double ms);
};

} // namespace kcenon::network::metrics
