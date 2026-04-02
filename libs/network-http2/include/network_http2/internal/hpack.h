// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file hpack.h
 * @brief HPACK header compression for network-http2 library
 *
 * This header re-exports the HPACK types from the main network_system
 * for use in the standalone network-http2 library.
 */

// Re-export HPACK types from main network_system
#include "internal/protocols/http2/hpack.h"
