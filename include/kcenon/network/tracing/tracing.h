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
 * @note For backward compatibility, individual headers can still be included
 * directly, but using this unified header is recommended.
 *
 * @see trace_context.h For context propagation
 * @see span.h For RAII span implementation
 * @see tracing_config.h For tracing configuration
 */

// Core tracing types (order matters: trace_context first, span depends on it)
#include "trace_context.h"
#include "span.h"

// Configuration
#include "tracing_config.h"
