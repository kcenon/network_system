/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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
 * \file grpc_official_wrapper.h
 * \brief Official gRPC library wrapper interfaces
 *
 * This file provides wrapper classes around the official gRPC library (grpc++)
 * to integrate with the network_system API. It implements the adapter pattern
 * to convert between network_system's Result<T> types and gRPC's Status types.
 *
 * \see docs/adr/ADR-001-grpc-official-library-wrapper.md
 */

#include "kcenon/network/protocols/grpc/frame.h"
#include "kcenon/network/protocols/grpc/status.h"
#include "kcenon/network/utils/result_types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declarations for gRPC types
#if NETWORK_GRPC_OFFICIAL
namespace grpc {
class Status;
class ServerContext;
class ClientContext;
class Channel;
}
#endif

namespace kcenon::network::protocols::grpc
{

// ============================================================================
// Channel Credentials Configuration
// ============================================================================

/*!
 * \struct channel_credentials_config
 * \brief Configuration for gRPC channel credentials
 *
 * This structure is available regardless of whether the official gRPC
 * library is present, allowing configuration to be prepared in advance.
 */
struct channel_credentials_config
{
    //! Use insecure credentials (no TLS)
    bool insecure = false;

    //! Root certificates for TLS (PEM format)
    std::string root_certificates;

    //! Client certificate for mutual TLS (PEM format)
    std::optional<std::string> client_certificate;

    //! Client private key for mutual TLS (PEM format)
    std::optional<std::string> client_key;
};

// ============================================================================
// Status Conversion Utilities
// ============================================================================

#if NETWORK_GRPC_OFFICIAL

/*!
 * \brief Convert gRPC status to network_system grpc_status
 * \param status gRPC status
 * \return network_system grpc_status
 */
auto from_grpc_status(const ::grpc::Status& status) -> grpc_status;

/*!
 * \brief Convert network_system grpc_status to gRPC status
 * \param status network_system grpc_status
 * \return gRPC status
 */
auto to_grpc_status(const grpc_status& status) -> ::grpc::Status;

/*!
 * \brief Convert Result<T> to gRPC status
 * \tparam T Value type
 * \param result Result to convert
 * \return gRPC status (OK if result is success, error status otherwise)
 */
template<typename T>
auto result_to_grpc_status(const Result<T>& result) -> ::grpc::Status;

/*!
 * \brief Convert VoidResult to gRPC status
 * \param result VoidResult to convert
 * \return gRPC status (OK if result is success, error status otherwise)
 */
auto void_result_to_grpc_status(const VoidResult& result) -> ::grpc::Status;

/*!
 * \brief Convert gRPC status to Result<T>
 * \tparam T Value type for success case
 * \param status gRPC status
 * \param value Value to use on success
 * \return Result with value if OK, error otherwise
 */
template<typename T>
auto grpc_status_to_result(const ::grpc::Status& status, T&& value) -> Result<T>;

/*!
 * \brief Convert gRPC status to VoidResult
 * \param status gRPC status
 * \return VoidResult (OK if status is OK, error otherwise)
 */
auto grpc_status_to_void_result(const ::grpc::Status& status) -> VoidResult;

// ============================================================================
// Error Code Mapping
// ============================================================================

namespace detail
{

/*!
 * \brief Map gRPC status code to network_system error code
 * \param code gRPC status code
 * \return Corresponding network_system error code
 */
auto map_grpc_code_to_error(int code) -> int;

/*!
 * \brief Map network_system status code to gRPC status code
 * \param code network_system status code
 * \return Corresponding gRPC status code
 */
auto map_status_to_grpc_code(status_code code) -> int;

} // namespace detail

// ============================================================================
// Deadline Utilities
// ============================================================================

/*!
 * \brief Set deadline on gRPC client context
 * \param ctx gRPC client context
 * \param deadline Deadline time point
 */
auto set_deadline(::grpc::ClientContext* ctx,
                  std::chrono::system_clock::time_point deadline) -> void;

/*!
 * \brief Set deadline from timeout on gRPC client context
 * \param ctx gRPC client context
 * \param timeout Timeout duration
 */
template<typename Rep, typename Period>
auto set_timeout(::grpc::ClientContext* ctx,
                 std::chrono::duration<Rep, Period> timeout) -> void
{
    set_deadline(ctx, std::chrono::system_clock::now() + timeout);
}

/*!
 * \brief Get remaining time until deadline from server context
 * \param ctx gRPC server context
 * \return Remaining time in milliseconds, or nullopt if no deadline
 */
auto get_remaining_time(const ::grpc::ServerContext* ctx)
    -> std::optional<std::chrono::milliseconds>;

// ============================================================================
// Channel Management
// ============================================================================

/*!
 * \brief Create gRPC channel with specified credentials
 * \param target Target address (e.g., "localhost:50051")
 * \param config Credentials configuration
 * \return Shared pointer to channel
 */
auto create_channel(const std::string& target,
                    const channel_credentials_config& config)
    -> std::shared_ptr<::grpc::Channel>;

/*!
 * \brief Create insecure gRPC channel
 * \param target Target address (e.g., "localhost:50051")
 * \return Shared pointer to channel
 */
auto create_insecure_channel(const std::string& target)
    -> std::shared_ptr<::grpc::Channel>;

/*!
 * \brief Wait for channel to be ready
 * \param channel gRPC channel
 * \param timeout Maximum time to wait
 * \return True if channel is ready within timeout
 */
auto wait_for_channel_ready(const std::shared_ptr<::grpc::Channel>& channel,
                            std::chrono::milliseconds timeout) -> bool;

// ============================================================================
// Server Streaming Adapters
// ============================================================================

/*!
 * \class official_server_writer
 * \brief Adapter wrapping gRPC ServerWriter for streaming responses
 */
class official_server_writer;

/*!
 * \class official_server_reader
 * \brief Adapter wrapping gRPC ServerReader for streaming requests
 */
class official_server_reader;

/*!
 * \class official_server_reader_writer
 * \brief Adapter wrapping gRPC ServerReaderWriter for bidirectional streaming
 */
class official_server_reader_writer;

// ============================================================================
// Client Streaming Adapters
// ============================================================================

/*!
 * \class official_client_reader
 * \brief Adapter wrapping gRPC ClientReader for server streaming
 */
class official_client_reader;

/*!
 * \class official_client_writer
 * \brief Adapter wrapping gRPC ClientWriter for client streaming
 */
class official_client_writer;

/*!
 * \class official_client_reader_writer
 * \brief Adapter wrapping gRPC ClientReaderWriter for bidirectional streaming
 */
class official_client_reader_writer;

#endif // NETWORK_GRPC_OFFICIAL

} // namespace kcenon::network::protocols::grpc
