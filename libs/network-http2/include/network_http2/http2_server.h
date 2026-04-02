// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file http2_server.h
 * @brief HTTP/2 server for network-http2 library
 *
 * This header re-exports the http2_server from the main network_system
 * for use in the standalone network-http2 library.
 */

// Re-export http2_server from main network_system
#include "internal/protocols/http2/http2_server.h"
