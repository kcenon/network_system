// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_server.cpp
 * @brief QUIC server implementation for network-quic library
 *
 * This file provides the QUIC server implementation. The actual QUIC
 * protocol handling is delegated to the main library's experimental
 * quic_server and unified adapter implementations.
 *
 * This compilation unit ensures that the necessary symbols are linked
 * when using the network-quic library standalone.
 */

#include "network_quic/quic_server.h"

// Include the protocol factory implementation
#include "kcenon/network/detail/protocol/quic.h"

// This compilation unit serves as a linkage point for the QUIC server
// functionality when used as a standalone library. The actual implementation
// lives in the main network_system library's src/protocol/quic.cpp and
// src/experimental/quic_server.cpp files.
