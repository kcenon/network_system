/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

/*!
 * \file interfaces.h
 * \brief Convenience header that includes core network interfaces.
 *
 * This header provides a single include point for core interface definitions
 * used in the composition-based architecture.
 *
 * ### Recommended Usage
 *
 * For most use cases, prefer the facade API which provides a simpler interface:
 *
 * \code
 * #include <kcenon/network/facade/tcp_facade.h>
 * #include <kcenon/network/facade/udp_facade.h>
 * #include <kcenon/network/facade/websocket_facade.h>
 *
 * auto client = kcenon::network::facade::tcp_facade{}.create_client({...});
 * \endcode
 *
 * ### Direct Interface Usage
 *
 * If you need to implement custom components:
 *
 * \code
 * #include <kcenon/network/interfaces/interfaces.h>
 *
 * class my_component : public kcenon::network::interfaces::i_network_component {
 *     // ...
 * };
 * \endcode
 *
 * \see i_protocol_client - Common interface for all protocol clients
 * \see i_protocol_server - Common interface for all protocol servers
 * \see i_network_component - Base interface for network components
 * \see i_client - Base client interface
 * \see i_server - Base server interface
 */

// Core interfaces (always included)
#include "i_network_component.h"
#include "i_session.h"
#include "connection_observer.h"
#include "i_client.h"
#include "i_server.h"

// Common protocol interfaces (recommended for facade users)
#include "i_protocol_client.h"
#include "i_protocol_server.h"

// Protocol-specific interfaces (deprecated - use facades instead)
// These will be moved to internal in a future release.
// Kept for backward compatibility only.
#ifdef NETWORK_INCLUDE_DEPRECATED_INTERFACES
#include "i_udp_client.h"
#include "i_udp_server.h"
#include "i_websocket_client.h"
#include "i_websocket_server.h"
#include "i_quic_client.h"
#include "i_quic_server.h"
#endif
