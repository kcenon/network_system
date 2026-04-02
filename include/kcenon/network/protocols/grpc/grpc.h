// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

// Core gRPC types (from detail directory)
#include "kcenon/network/detail/protocols/grpc/status.h"
#include "kcenon/network/detail/protocols/grpc/frame.h"

// Client and server APIs (from detail directory)
#include "kcenon/network/detail/protocols/grpc/client.h"
#include "kcenon/network/detail/protocols/grpc/server.h"

// Service registration and management (from detail directory)
#include "kcenon/network/detail/protocols/grpc/service_registry.h"

// Official gRPC library wrapper (from detail directory)
#include "kcenon/network/detail/protocols/grpc/grpc_official_wrapper.h"

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
