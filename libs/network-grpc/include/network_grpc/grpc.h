/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
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
