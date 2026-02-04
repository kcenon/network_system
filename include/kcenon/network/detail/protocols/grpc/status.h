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

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kcenon::network::protocols::grpc
{
    /*!
     * \enum status_code
     * \brief gRPC status codes (as defined in grpc/status.h)
     *
     * Standard gRPC status codes for RPC operations.
     * See: https://grpc.github.io/grpc/core/md_doc_statuscodes.html
     */
    enum class status_code : uint32_t
    {
        ok = 0,                  //!< Not an error; returned on success
        cancelled = 1,           //!< The operation was cancelled
        unknown = 2,             //!< Unknown error
        invalid_argument = 3,    //!< Client specified an invalid argument
        deadline_exceeded = 4,   //!< Deadline expired before operation completed
        not_found = 5,           //!< Some requested entity was not found
        already_exists = 6,      //!< Entity already exists
        permission_denied = 7,   //!< Permission denied
        resource_exhausted = 8,  //!< Resource exhausted
        failed_precondition = 9, //!< Operation rejected due to system state
        aborted = 10,            //!< Operation aborted
        out_of_range = 11,       //!< Operation attempted past valid range
        unimplemented = 12,      //!< Operation not implemented
        internal = 13,           //!< Internal error
        unavailable = 14,        //!< Service unavailable
        data_loss = 15,          //!< Unrecoverable data loss
        unauthenticated = 16     //!< Request lacks valid authentication
    };

    /*!
     * \brief Convert status code to string
     * \param code Status code to convert
     * \return String representation of the status code
     */
    constexpr auto status_code_to_string(status_code code) -> std::string_view
    {
        switch (code)
        {
            case status_code::ok:                  return "OK";
            case status_code::cancelled:           return "CANCELLED";
            case status_code::unknown:             return "UNKNOWN";
            case status_code::invalid_argument:    return "INVALID_ARGUMENT";
            case status_code::deadline_exceeded:   return "DEADLINE_EXCEEDED";
            case status_code::not_found:           return "NOT_FOUND";
            case status_code::already_exists:      return "ALREADY_EXISTS";
            case status_code::permission_denied:   return "PERMISSION_DENIED";
            case status_code::resource_exhausted:  return "RESOURCE_EXHAUSTED";
            case status_code::failed_precondition: return "FAILED_PRECONDITION";
            case status_code::aborted:             return "ABORTED";
            case status_code::out_of_range:        return "OUT_OF_RANGE";
            case status_code::unimplemented:       return "UNIMPLEMENTED";
            case status_code::internal:            return "INTERNAL";
            case status_code::unavailable:         return "UNAVAILABLE";
            case status_code::data_loss:           return "DATA_LOSS";
            case status_code::unauthenticated:     return "UNAUTHENTICATED";
            default:                               return "UNKNOWN";
        }
    }

    /*!
     * \struct grpc_status
     * \brief gRPC status with code, message, and optional details
     *
     * Represents the status of a gRPC operation, including the status code,
     * an optional error message, and optional encoded details (typically
     * google.rpc.Status in binary format).
     */
    struct grpc_status
    {
        status_code code = status_code::ok;
        std::string message;
        std::optional<std::string> details;

        /*!
         * \brief Default constructor (OK status)
         */
        grpc_status() = default;

        /*!
         * \brief Construct with status code
         * \param c Status code
         */
        explicit grpc_status(status_code c)
            : code(c) {}

        /*!
         * \brief Construct with status code and message
         * \param c Status code
         * \param msg Error message
         */
        grpc_status(status_code c, std::string msg)
            : code(c), message(std::move(msg)) {}

        /*!
         * \brief Construct with status code, message, and details
         * \param c Status code
         * \param msg Error message
         * \param det Encoded details
         */
        grpc_status(status_code c, std::string msg, std::string det)
            : code(c), message(std::move(msg)), details(std::move(det)) {}

        /*!
         * \brief Check if status is OK
         * \return True if status code is OK
         */
        auto is_ok() const -> bool { return code == status_code::ok; }

        /*!
         * \brief Check if status represents an error
         * \return True if status code is not OK
         */
        auto is_error() const -> bool { return code != status_code::ok; }

        /*!
         * \brief Get status code as string
         * \return String representation of status code
         */
        auto code_string() const -> std::string_view
        {
            return status_code_to_string(code);
        }

        /*!
         * \brief Create OK status
         * \return OK status
         */
        static auto ok_status() -> grpc_status
        {
            return grpc_status{status_code::ok};
        }

        /*!
         * \brief Create error status
         * \param c Error code
         * \param msg Error message
         * \return Error status
         */
        static auto error_status(status_code c, std::string msg) -> grpc_status
        {
            return grpc_status{c, std::move(msg)};
        }
    };

    /*!
     * \struct grpc_trailers
     * \brief gRPC trailing metadata containing status information
     *
     * Used to convey the final status of a gRPC call in HTTP/2 trailers.
     */
    struct grpc_trailers
    {
        status_code status = status_code::ok;
        std::string status_message;
        std::optional<std::string> status_details;

        /*!
         * \brief Convert to grpc_status
         * \return Equivalent grpc_status
         */
        auto to_status() const -> grpc_status
        {
            if (status_details.has_value())
            {
                return grpc_status{status, status_message, status_details.value()};
            }
            return grpc_status{status, status_message};
        }
    };

} // namespace kcenon::network::protocols::grpc
