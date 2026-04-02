// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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
