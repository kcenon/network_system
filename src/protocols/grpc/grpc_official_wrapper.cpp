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

#include "kcenon/network/detail/protocols/grpc/grpc_official_wrapper.h"
#include "kcenon/network/detail/protocols/grpc/frame.h"
#include "kcenon/network/detail/protocols/grpc/status.h"
#include "kcenon/network/detail/utils/result_types.h"

#if NETWORK_GRPC_OFFICIAL

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/support/byte_buffer.h>

namespace kcenon::network::protocols::grpc
{

// ============================================================================
// Error Code Mapping
// ============================================================================

namespace detail
{

auto map_grpc_code_to_error(int code) -> int
{
    switch (static_cast<::grpc::StatusCode>(code))
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

auto map_status_to_grpc_code(status_code code) -> int
{
    switch (code)
    {
        case status_code::ok:
            return static_cast<int>(::grpc::StatusCode::OK);
        case status_code::cancelled:
            return static_cast<int>(::grpc::StatusCode::CANCELLED);
        case status_code::unknown:
            return static_cast<int>(::grpc::StatusCode::UNKNOWN);
        case status_code::invalid_argument:
            return static_cast<int>(::grpc::StatusCode::INVALID_ARGUMENT);
        case status_code::deadline_exceeded:
            return static_cast<int>(::grpc::StatusCode::DEADLINE_EXCEEDED);
        case status_code::not_found:
            return static_cast<int>(::grpc::StatusCode::NOT_FOUND);
        case status_code::already_exists:
            return static_cast<int>(::grpc::StatusCode::ALREADY_EXISTS);
        case status_code::permission_denied:
            return static_cast<int>(::grpc::StatusCode::PERMISSION_DENIED);
        case status_code::resource_exhausted:
            return static_cast<int>(::grpc::StatusCode::RESOURCE_EXHAUSTED);
        case status_code::failed_precondition:
            return static_cast<int>(::grpc::StatusCode::FAILED_PRECONDITION);
        case status_code::aborted:
            return static_cast<int>(::grpc::StatusCode::ABORTED);
        case status_code::out_of_range:
            return static_cast<int>(::grpc::StatusCode::OUT_OF_RANGE);
        case status_code::unimplemented:
            return static_cast<int>(::grpc::StatusCode::UNIMPLEMENTED);
        case status_code::internal:
            return static_cast<int>(::grpc::StatusCode::INTERNAL);
        case status_code::unavailable:
            return static_cast<int>(::grpc::StatusCode::UNAVAILABLE);
        case status_code::data_loss:
            return static_cast<int>(::grpc::StatusCode::DATA_LOSS);
        case status_code::unauthenticated:
            return static_cast<int>(::grpc::StatusCode::UNAUTHENTICATED);
        default:
            return static_cast<int>(::grpc::StatusCode::UNKNOWN);
    }
}

} // namespace detail

// ============================================================================
// Status Conversion
// ============================================================================

auto from_grpc_status(const ::grpc::Status& status) -> grpc_status
{
    if (status.ok())
    {
        return grpc_status::ok_status();
    }

    auto code = static_cast<status_code>(detail::map_grpc_code_to_error(
        static_cast<int>(status.error_code())));
    return grpc_status::error_status(code, std::string(status.error_message()));
}

auto to_grpc_status(const grpc_status& status) -> ::grpc::Status
{
    if (status.is_ok())
    {
        return ::grpc::Status::OK;
    }

    auto grpc_code = static_cast<::grpc::StatusCode>(
        detail::map_status_to_grpc_code(status.code));
    return ::grpc::Status(grpc_code, status.message);
}

template<typename T>
auto result_to_grpc_status(const Result<T>& result) -> ::grpc::Status
{
    if (result.is_ok())
    {
        return ::grpc::Status::OK;
    }

    const auto& err = result.error();

    // Map error code to appropriate gRPC status code
    ::grpc::StatusCode grpc_code = ::grpc::StatusCode::INTERNAL;

    // Check for specific error codes
    if (err.code == error_codes::common_errors::invalid_argument)
    {
        grpc_code = ::grpc::StatusCode::INVALID_ARGUMENT;
    }
    else if (err.code == error_codes::common_errors::not_found)
    {
        grpc_code = ::grpc::StatusCode::NOT_FOUND;
    }
    else if (err.code == error_codes::common_errors::already_exists)
    {
        grpc_code = ::grpc::StatusCode::ALREADY_EXISTS;
    }
    else if (err.code == error_codes::common_errors::permission_denied)
    {
        grpc_code = ::grpc::StatusCode::PERMISSION_DENIED;
    }
    else if (err.code == error_codes::network_system::connection_failed)
    {
        grpc_code = ::grpc::StatusCode::UNAVAILABLE;
    }
    else if (err.code == static_cast<int>(status_code::deadline_exceeded))
    {
        grpc_code = ::grpc::StatusCode::DEADLINE_EXCEEDED;
    }
    else if (err.code == static_cast<int>(status_code::cancelled))
    {
        grpc_code = ::grpc::StatusCode::CANCELLED;
    }
    else if (err.code == static_cast<int>(status_code::unauthenticated))
    {
        grpc_code = ::grpc::StatusCode::UNAUTHENTICATED;
    }
    else if (err.code == static_cast<int>(status_code::resource_exhausted))
    {
        grpc_code = ::grpc::StatusCode::RESOURCE_EXHAUSTED;
    }
    else if (err.code == static_cast<int>(status_code::unimplemented))
    {
        grpc_code = ::grpc::StatusCode::UNIMPLEMENTED;
    }

    return ::grpc::Status(grpc_code, err.message);
}

auto void_result_to_grpc_status(const VoidResult& result) -> ::grpc::Status
{
    if (result.is_ok())
    {
        return ::grpc::Status::OK;
    }

    const auto& err = result.error();

    // Map error code to appropriate gRPC status code
    ::grpc::StatusCode grpc_code = ::grpc::StatusCode::INTERNAL;

    if (err.code == error_codes::common_errors::invalid_argument)
    {
        grpc_code = ::grpc::StatusCode::INVALID_ARGUMENT;
    }
    else if (err.code == error_codes::common_errors::not_found)
    {
        grpc_code = ::grpc::StatusCode::NOT_FOUND;
    }
    else if (err.code == error_codes::common_errors::already_exists)
    {
        grpc_code = ::grpc::StatusCode::ALREADY_EXISTS;
    }
    else if (err.code == error_codes::common_errors::permission_denied)
    {
        grpc_code = ::grpc::StatusCode::PERMISSION_DENIED;
    }
    else if (err.code == error_codes::network_system::connection_failed)
    {
        grpc_code = ::grpc::StatusCode::UNAVAILABLE;
    }

    return ::grpc::Status(grpc_code, err.message);
}

auto grpc_status_to_void_result(const ::grpc::Status& status) -> VoidResult
{
    if (status.ok())
    {
        return ok();
    }

    return error_void(
        detail::map_grpc_code_to_error(static_cast<int>(status.error_code())),
        std::string(status.error_message()),
        "grpc");
}

// Explicit template instantiations for common types
template auto result_to_grpc_status(const Result<std::vector<uint8_t>>&) -> ::grpc::Status;
template auto result_to_grpc_status(const Result<grpc_message>&) -> ::grpc::Status;
template auto result_to_grpc_status(const Result<std::string>&) -> ::grpc::Status;

// ============================================================================
// Deadline Utilities
// ============================================================================

auto set_deadline(::grpc::ClientContext* ctx,
                  std::chrono::system_clock::time_point deadline) -> void
{
    if (ctx != nullptr)
    {
        ctx->set_deadline(deadline);
    }
}

auto get_remaining_time(const ::grpc::ServerContext* ctx)
    -> std::optional<std::chrono::milliseconds>
{
    if (ctx == nullptr)
    {
        return std::nullopt;
    }

    auto deadline = ctx->deadline();

    // Check for infinite deadline
    if (deadline == std::chrono::system_clock::time_point::max())
    {
        return std::nullopt;
    }

    auto now = std::chrono::system_clock::now();
    if (deadline <= now)
    {
        return std::chrono::milliseconds(0);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
}

// ============================================================================
// Channel Management
// ============================================================================

auto create_channel(const std::string& target,
                    const channel_credentials_config& config)
    -> std::shared_ptr<::grpc::Channel>
{
    if (config.insecure)
    {
        return create_insecure_channel(target);
    }

    // Build SSL credentials options
    ::grpc::SslCredentialsOptions ssl_opts;

    if (!config.root_certificates.empty())
    {
        ssl_opts.pem_root_certs = config.root_certificates;
    }

    if (config.client_certificate.has_value() && config.client_key.has_value())
    {
        ssl_opts.pem_cert_chain = config.client_certificate.value();
        ssl_opts.pem_private_key = config.client_key.value();
    }

    auto creds = ::grpc::SslCredentials(ssl_opts);

    // Create channel arguments
    ::grpc::ChannelArguments args;

    return ::grpc::CreateCustomChannel(target, creds, args);
}

auto create_insecure_channel(const std::string& target)
    -> std::shared_ptr<::grpc::Channel>
{
    return ::grpc::CreateChannel(target, ::grpc::InsecureChannelCredentials());
}

auto wait_for_channel_ready(const std::shared_ptr<::grpc::Channel>& channel,
                            std::chrono::milliseconds timeout) -> bool
{
    if (!channel)
    {
        return false;
    }

    auto deadline = std::chrono::system_clock::now() + timeout;
    return channel->WaitForConnected(deadline);
}

// ============================================================================
// ByteBuffer Utilities
// ============================================================================

namespace detail
{

/*!
 * \brief Convert vector<uint8_t> to gRPC ByteBuffer
 * \param data Input data
 * \return gRPC ByteBuffer
 */
auto vector_to_byte_buffer(const std::vector<uint8_t>& data) -> ::grpc::ByteBuffer
{
    ::grpc::Slice slice(data.data(), data.size());
    return ::grpc::ByteBuffer(&slice, 1);
}

/*!
 * \brief Convert gRPC ByteBuffer to vector<uint8_t>
 * \param buffer gRPC ByteBuffer
 * \return Data vector
 */
auto byte_buffer_to_vector(const ::grpc::ByteBuffer& buffer) -> std::vector<uint8_t>
{
    std::vector<::grpc::Slice> slices;
    buffer.Dump(&slices);

    std::vector<uint8_t> result;
    result.reserve(buffer.Length());

    for (const auto& slice : slices)
    {
        const auto* begin = reinterpret_cast<const uint8_t*>(slice.begin());
        result.insert(result.end(), begin, begin + slice.size());
    }

    return result;
}

/*!
 * \brief Serialize grpc_message to ByteBuffer
 * \param msg gRPC message
 * \return gRPC ByteBuffer
 */
auto message_to_byte_buffer(const grpc_message& msg) -> ::grpc::ByteBuffer
{
    auto serialized = msg.serialize();
    return vector_to_byte_buffer(serialized);
}

/*!
 * \brief Deserialize ByteBuffer to grpc_message
 * \param buffer gRPC ByteBuffer
 * \return Result containing message or error
 */
auto byte_buffer_to_message(const ::grpc::ByteBuffer& buffer) -> Result<grpc_message>
{
    auto data = byte_buffer_to_vector(buffer);
    return grpc_message::parse(data);
}

} // namespace detail

// ============================================================================
// Server Streaming Adapters
// ============================================================================

/*!
 * \class official_server_writer_impl
 * \brief Implementation of server_writer wrapping gRPC ServerWriter
 */
template<typename W>
class official_server_writer_impl : public server_writer
{
public:
    explicit official_server_writer_impl(W* writer)
        : writer_(writer)
    {
    }

    auto write(const std::vector<uint8_t>& message) -> VoidResult override
    {
        if (writer_ == nullptr)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Writer is null",
                "grpc::server_writer");
        }

        ::grpc::ByteBuffer buffer = detail::vector_to_byte_buffer(message);
        bool success = writer_->Write(buffer);

        if (!success)
        {
            return error_void(
                error_codes::network_system::connection_failed,
                "Failed to write message",
                "grpc::server_writer");
        }

        return ok();
    }

private:
    W* writer_;
};

/*!
 * \class official_server_reader_impl
 * \brief Implementation of server_reader wrapping gRPC ServerReader
 */
template<typename R>
class official_server_reader_impl : public server_reader
{
public:
    explicit official_server_reader_impl(R* reader)
        : reader_(reader)
        , has_more_(true)
    {
    }

    auto read() -> Result<std::vector<uint8_t>> override
    {
        if (reader_ == nullptr)
        {
            return error<std::vector<uint8_t>>(
                error_codes::common_errors::invalid_argument,
                "Reader is null",
                "grpc::server_reader");
        }

        ::grpc::ByteBuffer buffer;
        bool success = reader_->Read(&buffer);

        if (!success)
        {
            has_more_ = false;
            return error<std::vector<uint8_t>>(
                static_cast<int>(status_code::ok),
                "End of stream",
                "grpc::server_reader");
        }

        return ok(detail::byte_buffer_to_vector(buffer));
    }

    auto has_more() const -> bool override
    {
        return has_more_;
    }

private:
    R* reader_;
    bool has_more_;
};

/*!
 * \class official_server_reader_writer_impl
 * \brief Implementation of server_reader_writer wrapping gRPC ServerReaderWriter
 */
template<typename RW>
class official_server_reader_writer_impl : public server_reader_writer
{
public:
    explicit official_server_reader_writer_impl(RW* stream)
        : stream_(stream)
        , has_more_(true)
    {
    }

    auto read() -> Result<std::vector<uint8_t>> override
    {
        if (stream_ == nullptr)
        {
            return error<std::vector<uint8_t>>(
                error_codes::common_errors::invalid_argument,
                "Stream is null",
                "grpc::server_reader_writer");
        }

        ::grpc::ByteBuffer buffer;
        bool success = stream_->Read(&buffer);

        if (!success)
        {
            has_more_ = false;
            return error<std::vector<uint8_t>>(
                static_cast<int>(status_code::ok),
                "End of stream",
                "grpc::server_reader_writer");
        }

        return ok(detail::byte_buffer_to_vector(buffer));
    }

    auto write(const std::vector<uint8_t>& message) -> VoidResult override
    {
        if (stream_ == nullptr)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Stream is null",
                "grpc::server_reader_writer");
        }

        ::grpc::ByteBuffer buffer = detail::vector_to_byte_buffer(message);
        bool success = stream_->Write(buffer);

        if (!success)
        {
            return error_void(
                error_codes::network_system::connection_failed,
                "Failed to write message",
                "grpc::server_reader_writer");
        }

        return ok();
    }

    auto has_more() const -> bool override
    {
        return has_more_;
    }

private:
    RW* stream_;
    bool has_more_;
};

} // namespace kcenon::network::protocols::grpc

#endif // NETWORK_GRPC_OFFICIAL
