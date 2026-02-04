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

#include "kcenon/network/detail/protocols/grpc/frame.h"
#include "kcenon/network/detail/protocols/grpc/status.h"
#include "kcenon/network/detail/utils/result_types.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::network::protocols::grpc
{
    /*!
     * \brief Metadata key-value pair for gRPC requests/responses
     */
    using grpc_metadata = std::vector<std::pair<std::string, std::string>>;

    /*!
     * \struct grpc_channel_config
     * \brief Configuration for gRPC channel
     */
    struct grpc_channel_config
    {
        //! Default timeout for RPC calls
        std::chrono::milliseconds default_timeout{30000};

        //! Whether to use TLS
        bool use_tls = true;

        //! Root certificates for TLS (PEM format)
        std::string root_certificates;

        //! Client certificate for mutual TLS (PEM format)
        std::optional<std::string> client_certificate;

        //! Client private key for mutual TLS (PEM format)
        std::optional<std::string> client_key;

        //! Maximum message size in bytes
        size_t max_message_size = default_max_message_size;

        //! Keepalive time in milliseconds (0 = disabled)
        std::chrono::milliseconds keepalive_time{0};

        //! Keepalive timeout in milliseconds
        std::chrono::milliseconds keepalive_timeout{20000};

        //! Maximum number of retry attempts
        uint32_t max_retry_attempts = 3;
    };

    /*!
     * \struct call_options
     * \brief Options for individual RPC calls
     */
    struct call_options
    {
        //! Deadline for this call
        std::optional<std::chrono::system_clock::time_point> deadline;

        //! Metadata to send with the request
        grpc_metadata metadata;

        //! Whether to wait for the server to be ready
        bool wait_for_ready = false;

        //! Compression algorithm to use
        std::string compression_algorithm;

        /*!
         * \brief Set deadline from timeout duration
         * \param timeout Timeout duration
         */
        template<typename Duration>
        void set_timeout(Duration timeout)
        {
            deadline = std::chrono::system_clock::now() +
                       std::chrono::duration_cast<std::chrono::system_clock::duration>(timeout);
        }
    };

    /*!
     * \class grpc_client
     * \brief gRPC client for making RPC calls
     *
     * This class provides a client interface for making gRPC calls
     * over HTTP/2 transport. It supports unary and streaming RPC calls.
     *
     * \note This is a prototype implementation. For production use,
     * consider wrapping the official gRPC library.
     */
    class grpc_client
    {
    public:
        /*!
         * \brief Construct gRPC client
         * \param target Target address (e.g., "localhost:50051")
         * \param config Channel configuration
         */
        explicit grpc_client(const std::string& target,
                             const grpc_channel_config& config = {});

        /*!
         * \brief Destructor
         */
        ~grpc_client();

        // Non-copyable
        grpc_client(const grpc_client&) = delete;
        grpc_client& operator=(const grpc_client&) = delete;

        // Movable
        grpc_client(grpc_client&&) noexcept;
        grpc_client& operator=(grpc_client&&) noexcept;

        /*!
         * \brief Connect to the server
         * \return Success or error
         */
        auto connect() -> VoidResult;

        /*!
         * \brief Disconnect from the server
         */
        auto disconnect() -> void;

        /*!
         * \brief Check if connected
         * \return True if connected
         */
        auto is_connected() const -> bool;

        /*!
         * \brief Wait for connection to be ready
         * \param timeout Maximum time to wait
         * \return True if connected within timeout
         */
        auto wait_for_connected(std::chrono::milliseconds timeout) -> bool;

        /*!
         * \brief Get the target address
         * \return Target address string
         */
        auto target() const -> const std::string&;

        /*!
         * \brief Make a unary RPC call
         * \param method Full method name (e.g., "/package.Service/Method")
         * \param request Serialized request message
         * \param options Call options
         * \return Result containing serialized response or error status
         *
         * Example:
         * \code
         * auto result = client.call_raw("/helloworld.Greeter/SayHello",
         *                               serialize(request));
         * if (result.is_ok()) {
         *     auto response = deserialize<HelloReply>(result.value().data);
         * }
         * \endcode
         */
        auto call_raw(const std::string& method,
                      const std::vector<uint8_t>& request,
                      const call_options& options = {}) -> Result<grpc_message>;

        /*!
         * \brief Make an async unary RPC call
         * \param method Full method name
         * \param request Serialized request message
         * \param callback Callback with result
         * \param options Call options
         */
        auto call_raw_async(const std::string& method,
                            const std::vector<uint8_t>& request,
                            std::function<void(Result<grpc_message>)> callback,
                            const call_options& options = {}) -> void;

        /*!
         * \class server_stream_reader
         * \brief Reader for server streaming RPC
         */
        class server_stream_reader
        {
        public:
            /*!
             * \brief Read next message from stream
             * \return Result containing message or error/end-of-stream
             */
            virtual auto read() -> Result<grpc_message> = 0;

            /*!
             * \brief Check if stream has more messages
             * \return True if more messages available
             */
            virtual auto has_more() const -> bool = 0;

            /*!
             * \brief Get final status after stream ends
             * \return Final gRPC status
             */
            virtual auto finish() -> grpc_status = 0;

            virtual ~server_stream_reader() = default;
        };

        /*!
         * \brief Start a server streaming RPC call
         * \param method Full method name
         * \param request Serialized request message
         * \param options Call options
         * \return Stream reader or error
         */
        auto server_stream_raw(const std::string& method,
                               const std::vector<uint8_t>& request,
                               const call_options& options = {})
            -> Result<std::unique_ptr<server_stream_reader>>;

        /*!
         * \class client_stream_writer
         * \brief Writer for client streaming RPC
         */
        class client_stream_writer
        {
        public:
            /*!
             * \brief Write message to stream
             * \param message Serialized message
             * \return Success or error
             */
            virtual auto write(const std::vector<uint8_t>& message) -> VoidResult = 0;

            /*!
             * \brief Signal that writing is done
             * \return Success or error
             */
            virtual auto writes_done() -> VoidResult = 0;

            /*!
             * \brief Finish the call and get response
             * \return Result containing response or error
             */
            virtual auto finish() -> Result<grpc_message> = 0;

            virtual ~client_stream_writer() = default;
        };

        /*!
         * \brief Start a client streaming RPC call
         * \param method Full method name
         * \param options Call options
         * \return Stream writer or error
         */
        auto client_stream_raw(const std::string& method,
                               const call_options& options = {})
            -> Result<std::unique_ptr<client_stream_writer>>;

        /*!
         * \class bidi_stream
         * \brief Bidirectional streaming RPC handle
         */
        class bidi_stream
        {
        public:
            /*!
             * \brief Write message to stream
             * \param message Serialized message
             * \return Success or error
             */
            virtual auto write(const std::vector<uint8_t>& message) -> VoidResult = 0;

            /*!
             * \brief Read next message from stream
             * \return Result containing message or error
             */
            virtual auto read() -> Result<grpc_message> = 0;

            /*!
             * \brief Signal that writing is done
             * \return Success or error
             */
            virtual auto writes_done() -> VoidResult = 0;

            /*!
             * \brief Finish the call and get final status
             * \return Final gRPC status
             */
            virtual auto finish() -> grpc_status = 0;

            virtual ~bidi_stream() = default;
        };

        /*!
         * \brief Start a bidirectional streaming RPC call
         * \param method Full method name
         * \param options Call options
         * \return Bidirectional stream handle or error
         */
        auto bidi_stream_raw(const std::string& method,
                             const call_options& options = {})
            -> Result<std::unique_ptr<bidi_stream>>;

    private:
        class impl;
        std::unique_ptr<impl> impl_;
    };

} // namespace kcenon::network::protocols::grpc
