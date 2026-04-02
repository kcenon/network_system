// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/*!
 * \file grpc.h
 * \brief Umbrella header for network-grpc library
 *
 * Include this header to access all gRPC functionality:
 * - grpc_client for making RPC calls
 * - grpc_server for handling RPC requests
 * - grpc_status for status codes and error handling
 * - grpc_frame for message framing
 * - service_registry for service management
 *
 * Example:
 * \code
 * #include <network_grpc/grpc.h>
 *
 * using namespace kcenon::network::protocols::grpc;
 *
 * // Create a gRPC server
 * grpc_server server;
 * server.register_unary_method("/mypackage.MyService/MyMethod",
 *     [](server_context& ctx, const std::vector<uint8_t>& request) {
 *         return std::make_pair(grpc_status::ok_status(), response_data);
 *     });
 * server.start(50051);
 *
 * // Create a gRPC client
 * grpc_client client("localhost:50051");
 * client.connect();
 * auto result = client.call_raw("/mypackage.MyService/MyMethod", request_data);
 * \endcode
 */

#include "network_grpc/grpc_status.h"
#include "network_grpc/grpc_frame.h"
#include "network_grpc/grpc_client.h"
#include "network_grpc/grpc_server.h"
#include "network_grpc/grpc_service_registry.h"
#include "network_grpc/grpc_official_wrapper.h"
