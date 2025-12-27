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

/*!
 * \file grpc_official_wrapper.cpp
 * \brief Official gRPC library wrapper implementation
 *
 * This file provides the wrapper layer around the official gRPC library (grpc++)
 * to integrate with the network_system API. It implements the adapter pattern
 * to convert between network_system's Result<T> types and gRPC's Status types.
 *
 * \see docs/adr/ADR-001-grpc-official-library-wrapper.md
 */

#include "kcenon/network/protocols/grpc/frame.h"
#include "kcenon/network/protocols/grpc/status.h"
#include "kcenon/network/utils/result_types.h"

#if NETWORK_GRPC_OFFICIAL

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

namespace kcenon::network::protocols::grpc
{

namespace detail
{

/*!
 * \brief Map gRPC status code to network_system error code
 * \param code gRPC status code
 * \return Corresponding network_system error code
 */
inline auto map_grpc_code_to_error(::grpc::StatusCode code) -> int
{
    switch (code)
    {
        case ::grpc::StatusCode::OK:
            return 0;
        case ::grpc::StatusCode::CANCELLED:
            return static_cast<int>(status_code::cancelled);
        case ::grpc::StatusCode::UNKNOWN:
            return static_cast<int>(status_code::unknown);
        case ::grpc::StatusCode::INVALID_ARGUMENT:
            return static_cast<int>(status_code::invalid_argument);
        case ::grpc::StatusCode::DEADLINE_EXCEEDED:
            return static_cast<int>(status_code::deadline_exceeded);
        case ::grpc::StatusCode::NOT_FOUND:
            return static_cast<int>(status_code::not_found);
        case ::grpc::StatusCode::ALREADY_EXISTS:
            return static_cast<int>(status_code::already_exists);
        case ::grpc::StatusCode::PERMISSION_DENIED:
            return static_cast<int>(status_code::permission_denied);
        case ::grpc::StatusCode::RESOURCE_EXHAUSTED:
            return static_cast<int>(status_code::resource_exhausted);
        case ::grpc::StatusCode::FAILED_PRECONDITION:
            return static_cast<int>(status_code::failed_precondition);
        case ::grpc::StatusCode::ABORTED:
            return static_cast<int>(status_code::aborted);
        case ::grpc::StatusCode::OUT_OF_RANGE:
            return static_cast<int>(status_code::out_of_range);
        case ::grpc::StatusCode::UNIMPLEMENTED:
            return static_cast<int>(status_code::unimplemented);
        case ::grpc::StatusCode::INTERNAL:
            return static_cast<int>(status_code::internal);
        case ::grpc::StatusCode::UNAVAILABLE:
            return static_cast<int>(status_code::unavailable);
        case ::grpc::StatusCode::DATA_LOSS:
            return static_cast<int>(status_code::data_loss);
        case ::grpc::StatusCode::UNAUTHENTICATED:
            return static_cast<int>(status_code::unauthenticated);
        default:
            return static_cast<int>(status_code::unknown);
    }
}

/*!
 * \brief Map network_system status code to gRPC status code
 * \param code network_system status code
 * \return Corresponding gRPC status code
 */
inline auto map_status_to_grpc_code(status_code code) -> ::grpc::StatusCode
{
    switch (code)
    {
        case status_code::ok:
            return ::grpc::StatusCode::OK;
        case status_code::cancelled:
            return ::grpc::StatusCode::CANCELLED;
        case status_code::unknown:
            return ::grpc::StatusCode::UNKNOWN;
        case status_code::invalid_argument:
            return ::grpc::StatusCode::INVALID_ARGUMENT;
        case status_code::deadline_exceeded:
            return ::grpc::StatusCode::DEADLINE_EXCEEDED;
        case status_code::not_found:
            return ::grpc::StatusCode::NOT_FOUND;
        case status_code::already_exists:
            return ::grpc::StatusCode::ALREADY_EXISTS;
        case status_code::permission_denied:
            return ::grpc::StatusCode::PERMISSION_DENIED;
        case status_code::resource_exhausted:
            return ::grpc::StatusCode::RESOURCE_EXHAUSTED;
        case status_code::failed_precondition:
            return ::grpc::StatusCode::FAILED_PRECONDITION;
        case status_code::aborted:
            return ::grpc::StatusCode::ABORTED;
        case status_code::out_of_range:
            return ::grpc::StatusCode::OUT_OF_RANGE;
        case status_code::unimplemented:
            return ::grpc::StatusCode::UNIMPLEMENTED;
        case status_code::internal:
            return ::grpc::StatusCode::INTERNAL;
        case status_code::unavailable:
            return ::grpc::StatusCode::UNAVAILABLE;
        case status_code::data_loss:
            return ::grpc::StatusCode::DATA_LOSS;
        case status_code::unauthenticated:
            return ::grpc::StatusCode::UNAUTHENTICATED;
        default:
            return ::grpc::StatusCode::UNKNOWN;
    }
}

} // namespace detail

/*!
 * \brief Convert gRPC status to network_system grpc_status
 * \param status gRPC status
 * \return network_system grpc_status
 */
auto from_grpc_status(const ::grpc::Status& status) -> grpc_status
{
    if (status.ok())
    {
        return grpc_status::ok_status();
    }

    auto code = static_cast<status_code>(detail::map_grpc_code_to_error(status.error_code()));
    return grpc_status::error_status(code, std::string(status.error_message()));
}

/*!
 * \brief Convert network_system grpc_status to gRPC status
 * \param status network_system grpc_status
 * \return gRPC status
 */
auto to_grpc_status(const grpc_status& status) -> ::grpc::Status
{
    if (status.is_ok())
    {
        return ::grpc::Status::OK;
    }

    return ::grpc::Status(
        detail::map_status_to_grpc_code(status.code),
        status.message
    );
}

/*!
 * \brief Convert Result<T> to gRPC status
 * \tparam T Value type
 * \param result Result to convert
 * \return gRPC status (OK if result is success, error status otherwise)
 */
template<typename T>
auto result_to_grpc_status(const Result<T>& result) -> ::grpc::Status
{
    if (result.is_ok())
    {
        return ::grpc::Status::OK;
    }

    const auto& err = result.error();
    return ::grpc::Status(
        ::grpc::StatusCode::INTERNAL,
        err.message
    );
}

/*!
 * \brief Convert VoidResult to gRPC status
 * \param result VoidResult to convert
 * \return gRPC status (OK if result is success, error status otherwise)
 */
auto void_result_to_grpc_status(const VoidResult& result) -> ::grpc::Status
{
    if (result.is_ok())
    {
        return ::grpc::Status::OK;
    }

    const auto& err = result.error();
    return ::grpc::Status(
        ::grpc::StatusCode::INTERNAL,
        err.message
    );
}

// Explicit template instantiations for common types
template auto result_to_grpc_status(const Result<std::vector<uint8_t>>&) -> ::grpc::Status;
template auto result_to_grpc_status(const Result<grpc_message>&) -> ::grpc::Status;

} // namespace kcenon::network::protocols::grpc

#endif // NETWORK_GRPC_OFFICIAL
