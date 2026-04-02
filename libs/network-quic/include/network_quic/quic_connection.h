// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_connection.h
 * @brief QUIC connection management for network-quic library
 *
 * This header provides access to QUIC connection management utilities
 * including connection state, stream management, and protocol internals.
 */

// Re-export the QUIC connection types from the main library internal headers
// Note: These are internal implementation details exposed for advanced use cases
#include "internal/protocols/quic/connection.h"
#include "internal/protocols/quic/stream.h"
#include "internal/protocols/quic/stream_manager.h"
