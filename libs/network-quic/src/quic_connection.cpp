// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_connection.cpp
 * @brief QUIC connection utilities for network-quic library
 *
 * This file provides the QUIC connection management utilities.
 * The actual QUIC connection handling is implemented in the
 * main library's protocols/quic/connection.h header.
 *
 * This compilation unit ensures that the necessary symbols are linked
 * when using the network-quic library standalone.
 */

#include "network_quic/quic_connection.h"

// Include the QUIC protocol internals (moved to internal)
#include "internal/protocols/quic/connection.h"
#include "internal/protocols/quic/stream.h"
#include "internal/protocols/quic/stream_manager.h"

// This compilation unit serves as a linkage point for the QUIC connection
// functionality when used as a standalone library. The actual implementation
// of QUIC connection management lives in the header-only templates in
// include/kcenon/network/protocols/quic/connection.h.
