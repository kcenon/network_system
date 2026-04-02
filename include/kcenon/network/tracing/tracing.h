// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file tracing.h
 * @brief Unified tracing header for network_system
 *
 * This is the main entry point for distributed tracing in network_system.
 * It includes all tracing-related headers:
 * - trace_context: Context propagation for distributed traces
 * - span: RAII span implementation for timing operations
 * - tracing_config: Configuration for tracing behavior
 *
 * Usage:
 * @code
 * #include <kcenon/network/tracing/tracing.h>
 *
 * using namespace kcenon::network::tracing;
 *
 * // Create a trace context for a new request
 * auto ctx = trace_context::create("request-processing");
 *
 * // Create a span for timing an operation
 * {
 *     span operation_span(ctx, "database-query");
 *     // ... perform operation ...
 * } // span automatically ends and reports duration
 *
 * // Configure tracing behavior
 * tracing_config config;
 * config.sampling_rate = 0.1; // Sample 10% of traces
 * @endcode
 *
 * @note Individual headers in detail/tracing/ are implementation details.
 * Please use this unified header for tracing needs.
 *
 * @see detail/tracing/trace_context.h For context propagation
 * @see detail/tracing/span.h For RAII span implementation
 * @see detail/tracing/tracing_config.h For tracing configuration
 */

// Core tracing types (from detail directory)
#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/span.h"

// Configuration
#include "kcenon/network/detail/tracing/tracing_config.h"
