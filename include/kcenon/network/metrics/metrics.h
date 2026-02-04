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
 * @note For backward compatibility, individual headers can still be included
 * directly, but using this unified header is recommended.
 *
 * @see network_metrics.h For core metrics definitions
 * @see histogram.h For histogram implementation
 * @see sliding_histogram.h For time-windowed histogram
 */

// Core metrics types
#include "network_metrics.h"

// Histogram implementations
#include "histogram.h"
#include "sliding_histogram.h"
