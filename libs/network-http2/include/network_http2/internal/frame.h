// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file frame.h
 * @brief HTTP/2 frame types for network-http2 library
 *
 * This header re-exports the HTTP/2 frame types from the main network_system
 * for use in the standalone network-http2 library.
 */

// Re-export frame types from main network_system
#include "internal/protocols/http2/frame.h"
