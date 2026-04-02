// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file session.h
 * @brief Unified session header for network_system
 *
 * This is the main entry point for network session management.
 * It includes all session-related headers:
 * - messaging_session: TCP-based messaging sessions
 * - secure_session: TLS/SSL encrypted sessions
 * - quic_session: QUIC protocol sessions
 *
 * Usage:
 * @code
 * #include <kcenon/network/session/session.h>
 *
 * using namespace kcenon::network::session;
 *
 * // Create a messaging session for TCP communication
 * auto session = std::make_shared<messaging_session>(socket, io_context);
 *
 * // Create a secure session for encrypted communication
 * auto secure = std::make_shared<secure_session>(socket, ssl_context);
 *
 * // Create a QUIC session for modern transport
 * auto quic = std::make_shared<quic_session>(connection);
 * @endcode
 *
 * @note Individual headers in detail/session/ are implementation details.
 * Please use this unified header for session management needs.
 *
 * @see detail/session/messaging_session.h For TCP messaging sessions
 * @see detail/session/secure_session.h For TLS/SSL sessions
 * @see detail/session/quic_session.h For QUIC protocol sessions
 */

// Session implementations (from detail directory)
#include "kcenon/network/detail/session/messaging_session.h"
#include "kcenon/network/detail/session/secure_session.h"
#include "kcenon/network/detail/session/quic_session.h"
