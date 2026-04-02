// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic.h
 * @brief Main public header for network-quic library
 *
 * This header provides access to the QUIC protocol implementation including:
 * - QUIC client for connecting to QUIC servers
 * - QUIC server for accepting QUIC connections
 * - QUIC connection management utilities
 *
 * For detailed protocol-level headers (streams, crypto), include internal headers.
 */

#include "network_quic/quic_client.h"
#include "network_quic/quic_server.h"
#include "network_quic/quic_connection.h"

// Re-export the protocol factory functions from the main library
// This provides backward compatibility with existing code using protocol::quic
#include "kcenon/network/detail/protocol/quic.h"
