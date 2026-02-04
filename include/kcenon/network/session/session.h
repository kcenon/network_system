/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

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
