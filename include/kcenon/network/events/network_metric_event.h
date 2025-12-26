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
 * @file network_metric_event.h
 * @brief Network metric events for EventBus-based metric publishing
 *
 * This header defines event structures for publishing network metrics
 * via common_system's EventBus. External consumers (like monitoring_system)
 * can subscribe to these events without creating compile-time dependencies.
 *
 * @author kcenon
 * @date 2025-01-26
 */

#pragma once

#include <chrono>
#include <map>
#include <string>

namespace kcenon::network::events {

/**
 * @enum network_metric_type
 * @brief Types of network metrics
 */
enum class network_metric_type
{
	counter,   /*!< Monotonically increasing value (e.g., total bytes sent) */
	gauge,     /*!< Value that can increase or decrease (e.g., active connections) */
	histogram, /*!< Distribution of values (e.g., latency) */
	summary    /*!< Statistical summary of values */
};

/**
 * @struct network_metric_event
 * @brief Event for publishing network metrics via EventBus
 *
 * This event satisfies common_system's EventType concept and can be
 * published to the EventBus for consumption by external systems like
 * monitoring_system.
 *
 * Example usage:
 * @code
 * auto& bus = kcenon::common::get_event_bus();
 * bus.publish(network_metric_event{
 *     "network.connections.total", 1.0, network_metric_type::counter,
 *     {{"protocol", "tcp"}, {"status", "accepted"}}
 * });
 * @endcode
 */
struct network_metric_event
{
	std::string name;                              /*!< Metric name */
	double value;                                  /*!< Metric value */
	std::string unit;                              /*!< Unit of measurement */
	network_metric_type type;                      /*!< Metric type */
	std::chrono::steady_clock::time_point timestamp; /*!< Event timestamp */
	std::map<std::string, std::string> labels;     /*!< Additional labels/tags */

	/**
	 * @brief Construct a network metric event
	 * @param metric_name Metric name
	 * @param metric_value Metric value
	 * @param metric_type Type of metric (default: counter)
	 * @param metric_labels Additional labels/tags (default: empty)
	 * @param metric_unit Unit of measurement (default: empty)
	 */
	explicit network_metric_event(
		const std::string& metric_name,
		double metric_value,
		network_metric_type metric_type = network_metric_type::counter,
		const std::map<std::string, std::string>& metric_labels = {},
		const std::string& metric_unit = "")
		: name(metric_name)
		, value(metric_value)
		, unit(metric_unit)
		, type(metric_type)
		, timestamp(std::chrono::steady_clock::now())
		, labels(metric_labels)
	{
	}

	/**
	 * @brief Default constructor
	 */
	network_metric_event()
		: value(0.0)
		, type(network_metric_type::counter)
		, timestamp(std::chrono::steady_clock::now())
	{
	}

	/**
	 * @brief Copy constructor (required for EventType concept)
	 */
	network_metric_event(const network_metric_event&) = default;

	/**
	 * @brief Copy assignment operator
	 */
	network_metric_event& operator=(const network_metric_event&) = default;

	/**
	 * @brief Move constructor
	 */
	network_metric_event(network_metric_event&&) noexcept = default;

	/**
	 * @brief Move assignment operator
	 */
	network_metric_event& operator=(network_metric_event&&) noexcept = default;
};

/**
 * @struct network_connection_event
 * @brief Specialized event for connection-related metrics
 */
struct network_connection_event
{
	std::string connection_id;                     /*!< Connection identifier */
	std::string event_type;                        /*!< Event type (accepted, closed, failed) */
	std::string protocol;                          /*!< Protocol (tcp, udp, websocket, quic) */
	std::string remote_address;                    /*!< Remote endpoint address */
	std::chrono::steady_clock::time_point timestamp;
	std::map<std::string, std::string> labels;

	explicit network_connection_event(
		const std::string& conn_id,
		const std::string& evt_type,
		const std::string& proto = "tcp",
		const std::string& remote = "",
		const std::map<std::string, std::string>& lbls = {})
		: connection_id(conn_id)
		, event_type(evt_type)
		, protocol(proto)
		, remote_address(remote)
		, timestamp(std::chrono::steady_clock::now())
		, labels(lbls)
	{
	}

	network_connection_event()
		: timestamp(std::chrono::steady_clock::now())
	{
	}

	network_connection_event(const network_connection_event&) = default;
	network_connection_event& operator=(const network_connection_event&) = default;
	network_connection_event(network_connection_event&&) noexcept = default;
	network_connection_event& operator=(network_connection_event&&) noexcept = default;
};

/**
 * @struct network_transfer_event
 * @brief Specialized event for data transfer metrics
 */
struct network_transfer_event
{
	std::string connection_id;                     /*!< Connection identifier */
	std::string direction;                         /*!< Transfer direction (sent, received) */
	std::size_t bytes;                             /*!< Number of bytes transferred */
	std::size_t packets;                           /*!< Number of packets (optional) */
	std::chrono::steady_clock::time_point timestamp;
	std::map<std::string, std::string> labels;

	explicit network_transfer_event(
		const std::string& conn_id,
		const std::string& dir,
		std::size_t byte_count,
		std::size_t packet_count = 1,
		const std::map<std::string, std::string>& lbls = {})
		: connection_id(conn_id)
		, direction(dir)
		, bytes(byte_count)
		, packets(packet_count)
		, timestamp(std::chrono::steady_clock::now())
		, labels(lbls)
	{
	}

	network_transfer_event()
		: bytes(0)
		, packets(0)
		, timestamp(std::chrono::steady_clock::now())
	{
	}

	network_transfer_event(const network_transfer_event&) = default;
	network_transfer_event& operator=(const network_transfer_event&) = default;
	network_transfer_event(network_transfer_event&&) noexcept = default;
	network_transfer_event& operator=(network_transfer_event&&) noexcept = default;
};

/**
 * @struct network_latency_event
 * @brief Specialized event for latency measurements
 */
struct network_latency_event
{
	std::string connection_id;                     /*!< Connection identifier */
	double latency_ms;                             /*!< Latency in milliseconds */
	std::string operation;                         /*!< Operation type (request, response, roundtrip) */
	std::chrono::steady_clock::time_point timestamp;
	std::map<std::string, std::string> labels;

	explicit network_latency_event(
		const std::string& conn_id,
		double latency,
		const std::string& op = "roundtrip",
		const std::map<std::string, std::string>& lbls = {})
		: connection_id(conn_id)
		, latency_ms(latency)
		, operation(op)
		, timestamp(std::chrono::steady_clock::now())
		, labels(lbls)
	{
	}

	network_latency_event()
		: latency_ms(0.0)
		, timestamp(std::chrono::steady_clock::now())
	{
	}

	network_latency_event(const network_latency_event&) = default;
	network_latency_event& operator=(const network_latency_event&) = default;
	network_latency_event(network_latency_event&&) noexcept = default;
	network_latency_event& operator=(network_latency_event&&) noexcept = default;
};

/**
 * @struct network_health_event
 * @brief Specialized event for connection health status
 */
struct network_health_event
{
	std::string connection_id;                     /*!< Connection identifier */
	bool is_alive;                                 /*!< Connection alive status */
	double response_time_ms;                       /*!< Response time in milliseconds */
	std::size_t missed_heartbeats;                 /*!< Number of missed heartbeats */
	double packet_loss_rate;                       /*!< Packet loss rate (0.0-1.0) */
	std::chrono::steady_clock::time_point timestamp;
	std::map<std::string, std::string> labels;

	explicit network_health_event(
		const std::string& conn_id,
		bool alive,
		double response_time = 0.0,
		std::size_t missed = 0,
		double loss_rate = 0.0,
		const std::map<std::string, std::string>& lbls = {})
		: connection_id(conn_id)
		, is_alive(alive)
		, response_time_ms(response_time)
		, missed_heartbeats(missed)
		, packet_loss_rate(loss_rate)
		, timestamp(std::chrono::steady_clock::now())
		, labels(lbls)
	{
	}

	network_health_event()
		: is_alive(false)
		, response_time_ms(0.0)
		, missed_heartbeats(0)
		, packet_loss_rate(0.0)
		, timestamp(std::chrono::steady_clock::now())
	{
	}

	network_health_event(const network_health_event&) = default;
	network_health_event& operator=(const network_health_event&) = default;
	network_health_event(network_health_event&&) noexcept = default;
	network_health_event& operator=(network_health_event&&) noexcept = default;
};

} // namespace kcenon::network::events
