// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file metrics.h
 * @brief Unified metrics header for network_system
 *
 * This is the main entry point for network metrics in network_system.
 * It includes all metrics-related headers:
 * - network_metrics: Core metrics definitions and reporting
 * - histogram: Histogram implementation for latency tracking
 * - sliding_histogram: Time-windowed histogram for recent data
 *
 * Usage:
 * @code
 * #include <kcenon/network/metrics/metrics.h>
 *
 * using namespace kcenon::network::metrics;
 *
 * // Create a histogram for latency tracking
 * histogram latency_hist;
 * latency_hist.record(50); // 50ms latency
 *
 * // Get histogram statistics
 * auto snapshot = latency_hist.get_snapshot();
 * std::cout << "p99 latency: " << snapshot.percentile(0.99) << "ms\n";
 *
 * // Use sliding histogram for time-windowed metrics
 * sliding_histogram recent_latency(std::chrono::minutes(5));
 * recent_latency.record(30);
 * @endcode
 *
 * @note Individual headers in detail/metrics/ are implementation details.
 * Please use this unified header for metrics needs.
 *
 * @see detail/metrics/network_metrics.h For core metrics definitions
 * @see detail/metrics/histogram.h For histogram implementation
 * @see detail/metrics/sliding_histogram.h For time-windowed histogram
 */

// Core metrics types (from detail directory)
#include "kcenon/network/detail/metrics/network_metrics.h"

// Histogram implementations
#include "kcenon/network/detail/metrics/histogram.h"
#include "kcenon/network/detail/metrics/sliding_histogram.h"
