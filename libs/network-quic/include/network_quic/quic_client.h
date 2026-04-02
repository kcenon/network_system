// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_client.h
 * @brief QUIC client interface for network-quic library
 *
 * This header provides the QUIC client interface. For actual client
 * implementations, use the factory functions from protocol::quic namespace.
 */

// Re-export the QUIC client interface from the main library
#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/detail/protocol/quic.h"
