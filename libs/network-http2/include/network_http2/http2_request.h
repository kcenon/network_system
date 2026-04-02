// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file http2_request.h
 * @brief HTTP/2 request types for network-http2 library
 *
 * This header re-exports the http2_request from the main network_system
 * for use in the standalone network-http2 library.
 */

// Re-export http2_request from main network_system
#include "internal/protocols/http2/http2_request.h"
