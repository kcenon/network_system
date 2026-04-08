/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file network_metrics_example.cpp
 * @brief Demonstrates network metrics collection and reporting.
 * @example network_metrics_example.cpp
 *
 * @par Category
 * Observability - Metrics and monitoring
 *
 * Demonstrates:
 * - histogram for latency tracking
 * - metric_reporter for standard network metrics
 * - Metric name constants
 *
 * @see kcenon::network::metrics::histogram
 * @see kcenon::network::metrics::metric_reporter
 */

#include <kcenon/network/metrics/metrics.h>

#include <cstdlib>
#include <iostream>
#include <string>

using namespace kcenon::network;

int main()
{
	std::cout << "=== Network Metrics Example ===" << std::endl;

	// 1. Histogram for latency tracking
	std::cout << "\n1. Histogram (latency tracking):" << std::endl;
	metrics::histogram latency_hist;

	// Simulate latency measurements
	double latencies[] = {1.2, 2.5, 1.8, 3.1, 2.0, 1.5, 4.2, 2.8, 1.9, 2.3};
	for (double lat : latencies)
	{
		latency_hist.record(lat);
	}

	auto snapshot = latency_hist.get_snapshot();
	std::cout << "   Recorded 10 latency measurements" << std::endl;
	std::cout << "   Snapshot retrieved" << std::endl;

	// 2. Metric reporter (static convenience methods)
	std::cout << "\n2. Metric reporter:" << std::endl;
	metrics::metric_reporter::report_connection_accepted();
	std::cout << "   Reported: connection accepted" << std::endl;

	metrics::metric_reporter::report_bytes_sent(1024);
	std::cout << "   Reported: 1024 bytes sent" << std::endl;

	metrics::metric_reporter::report_latency(2.5);
	std::cout << "   Reported: 2.5ms latency" << std::endl;

	metrics::metric_reporter::report_connection_failed("timeout");
	std::cout << "   Reported: connection failed (timeout)" << std::endl;

	// 3. Standard metric names
	std::cout << "\n3. Standard metric names:" << std::endl;
	std::cout << "   Active connections: " << metrics::metric_names::CONNECTIONS_ACTIVE << std::endl;
	std::cout << "   Bytes sent: " << metrics::metric_names::BYTES_SENT << std::endl;
	std::cout << "   Latency: " << metrics::metric_names::LATENCY_MS << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
