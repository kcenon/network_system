// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file internal/stream.h
 * @brief Internal QUIC stream management for network-quic library
 *
 * This header provides internal QUIC stream types and utilities.
 */

// Re-export QUIC stream types from the main library internal headers
#include "internal/protocols/quic/stream.h"
#include "internal/protocols/quic/stream_manager.h"
#include "internal/protocols/quic/flow_control.h"
