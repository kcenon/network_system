// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_server.h
 * @brief QUIC server interface for network-quic library
 *
 * This header provides the QUIC server interface. For actual server
 * implementations, use the factory functions from protocol::quic namespace.
 */

// Re-export the QUIC server interface from the main library
#include "kcenon/network/interfaces/i_protocol_server.h"
#include "kcenon/network/detail/protocol/quic.h"
