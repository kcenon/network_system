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
 * \file grpc.h
 * \brief Unified gRPC protocol header
 *
 * This is the main entry point for using gRPC functionality in network_system.
 * It includes all necessary gRPC components:
 * - Status codes and error handling (status.h)
 * - Message framing and serialization (frame.h)
 * - Client API for making RPC calls (client.h)
 * - Server API for handling RPC requests (server.h)
 * - Service registration and management (service_registry.h)
 * - Official gRPC library wrappers (grpc_official_wrapper.h)
 *
 * Usage:
 * \code
 * #include <kcenon/network/protocols/grpc/grpc.h>
 *
 * using namespace kcenon::network::protocols::grpc;
 *
 * // Create and configure gRPC server
 * grpc_server server;
 * server.register_unary_method("/myservice.MyService/Echo", handler);
 * server.start(50051);
 *
 * // Create and use gRPC client
 * grpc_client client("localhost:50051");
 * client.connect();
 * auto result = client.call_raw("/myservice.MyService/Echo", request);
 * \endcode
 *
 * \note For backward compatibility, individual headers can still be included
 * directly, but using this unified header is recommended.
 *
 * \see docs/adr/ADR-001-grpc-official-library-wrapper.md
 */

// Core gRPC types
#include "kcenon/network/protocols/grpc/status.h"
#include "kcenon/network/protocols/grpc/frame.h"

// Client and server APIs
#include "kcenon/network/protocols/grpc/client.h"
#include "kcenon/network/protocols/grpc/server.h"

// Service registration and management
#include "kcenon/network/protocols/grpc/service_registry.h"

// Official gRPC library wrapper (optional, enabled with NETWORK_GRPC_OFFICIAL)
#include "kcenon/network/protocols/grpc/grpc_official_wrapper.h"

/*!
 * \namespace kcenon::network::protocols::grpc
 * \brief gRPC protocol implementation
 *
 * This namespace contains all gRPC-related types and functions:
 * - Core types: grpc_status, grpc_message
 * - Client types: grpc_client, call_options, grpc_channel_config
 * - Server types: grpc_server, server_context, grpc_server_config
 * - Service types: grpc_service, service_registry, generic_service
 * - Handler types: unary_handler, server_streaming_handler, etc.
 *
 * All types use the Result<T>/VoidResult pattern for error handling,
 * providing consistent error reporting across the library.
 */
