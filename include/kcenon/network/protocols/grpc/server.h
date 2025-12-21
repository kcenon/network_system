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

#include "kcenon/network/protocols/grpc/frame.h"
#include "kcenon/network/protocols/grpc/status.h"
#include "kcenon/network/utils/result_types.h"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kcenon::network::protocols::grpc
{
    /*!
     * \brief Metadata key-value pair for gRPC requests/responses
     */
    using grpc_metadata = std::vector<std::pair<std::string, std::string>>;
    /*!
     * \struct grpc_server_config
     * \brief Configuration for gRPC server
     */
    struct grpc_server_config
    {
        //! Maximum number of concurrent streams per connection
        size_t max_concurrent_streams = 100;

        //! Maximum message size in bytes
        size_t max_message_size = default_max_message_size;

        //! Keepalive time (time between keepalive pings)
        std::chrono::milliseconds keepalive_time{7200000};  // 2 hours

        //! Keepalive timeout
        std::chrono::milliseconds keepalive_timeout{20000};

        //! Maximum connection idle time
        std::chrono::milliseconds max_connection_idle{0};  // 0 = unlimited

        //! Maximum connection age
        std::chrono::milliseconds max_connection_age{0};  // 0 = unlimited

        //! Number of worker threads (0 = auto-detect)
        size_t num_threads = 0;
    };

    /*!
     * \class server_context
     * \brief Context for handling a single RPC request
     *
     * Provides access to client metadata, allows setting response metadata,
     * and provides information about the current request.
     */
    class server_context
    {
    public:
        /*!
         * \brief Get client metadata
         * \return Reference to client metadata
         */
        virtual auto client_metadata() const -> const grpc_metadata& = 0;

        /*!
         * \brief Add trailing metadata
         * \param key Metadata key
         * \param value Metadata value
         */
        virtual auto add_trailing_metadata(const std::string& key,
                                           const std::string& value) -> void = 0;

        /*!
         * \brief Set trailing metadata
         * \param metadata Trailing metadata
         */
        virtual auto set_trailing_metadata(grpc_metadata metadata) -> void = 0;

        /*!
         * \brief Check if the request has been cancelled
         * \return True if cancelled
         */
        virtual auto is_cancelled() const -> bool = 0;

        /*!
         * \brief Get request deadline
         * \return Deadline if set
         */
        virtual auto deadline() const
            -> std::optional<std::chrono::system_clock::time_point> = 0;

        /*!
         * \brief Get peer address
         * \return Peer address string
         */
        virtual auto peer() const -> std::string = 0;

        /*!
         * \brief Get authentication context (for TLS)
         * \return Authentication context string (e.g., client certificate CN)
         */
        virtual auto auth_context() const -> std::string = 0;

        virtual ~server_context() = default;
    };

    /*!
     * \brief Handler function type for unary RPC
     *
     * \param ctx Server context
     * \param request Serialized request message
     * \return Pair of status and serialized response
     */
    using unary_handler = std::function<
        std::pair<grpc_status, std::vector<uint8_t>>(
            server_context& ctx,
            const std::vector<uint8_t>& request)>;

    /*!
     * \brief Writer interface for server streaming
     */
    class server_writer
    {
    public:
        /*!
         * \brief Write a message to the stream
         * \param message Serialized message
         * \return Success or error
         */
        virtual auto write(const std::vector<uint8_t>& message) -> VoidResult = 0;

        virtual ~server_writer() = default;
    };

    /*!
     * \brief Handler function type for server streaming RPC
     *
     * \param ctx Server context
     * \param request Serialized request message
     * \param writer Writer to send response messages
     * \return Final status
     */
    using server_streaming_handler = std::function<
        grpc_status(
            server_context& ctx,
            const std::vector<uint8_t>& request,
            server_writer& writer)>;

    /*!
     * \brief Reader interface for client streaming
     */
    class server_reader
    {
    public:
        /*!
         * \brief Read next message from stream
         * \return Result containing message or end-of-stream
         */
        virtual auto read() -> Result<std::vector<uint8_t>> = 0;

        /*!
         * \brief Check if more messages are available
         * \return True if more messages
         */
        virtual auto has_more() const -> bool = 0;

        virtual ~server_reader() = default;
    };

    /*!
     * \brief Handler function type for client streaming RPC
     *
     * \param ctx Server context
     * \param reader Reader for incoming messages
     * \return Pair of status and serialized response
     */
    using client_streaming_handler = std::function<
        std::pair<grpc_status, std::vector<uint8_t>>(
            server_context& ctx,
            server_reader& reader)>;

    /*!
     * \brief Reader/writer interface for bidirectional streaming
     */
    class server_reader_writer
    {
    public:
        virtual auto read() -> Result<std::vector<uint8_t>> = 0;
        virtual auto write(const std::vector<uint8_t>& message) -> VoidResult = 0;
        virtual auto has_more() const -> bool = 0;

        virtual ~server_reader_writer() = default;
    };

    /*!
     * \brief Handler function type for bidirectional streaming RPC
     *
     * \param ctx Server context
     * \param stream Reader/writer for messages
     * \return Final status
     */
    using bidi_streaming_handler = std::function<
        grpc_status(
            server_context& ctx,
            server_reader_writer& stream)>;

    /*!
     * \class grpc_service
     * \brief Base class for gRPC service implementations
     *
     * Derive from this class to implement your gRPC service.
     * Override the service_name() method and register handlers
     * for your RPC methods.
     */
    class grpc_service
    {
    public:
        /*!
         * \brief Get the full service name (e.g., "package.ServiceName")
         * \return Service name
         */
        virtual auto service_name() const -> std::string_view = 0;

        virtual ~grpc_service() = default;
    };

    /*!
     * \class grpc_server
     * \brief gRPC server for handling RPC requests
     *
     * This class provides a server for handling gRPC requests
     * over HTTP/2 transport.
     *
     * \note This is a prototype implementation. For production use,
     * consider wrapping the official gRPC library.
     *
     * Example usage:
     * \code
     * grpc_server server;
     *
     * server.register_unary_method("/package.Service/Method",
     *     [](server_context& ctx, const std::vector<uint8_t>& request)
     *         -> std::pair<grpc_status, std::vector<uint8_t>> {
     *         // Handle request
     *         return {grpc_status::ok_status(), response_data};
     *     });
     *
     * server.start(50051);
     * server.wait();
     * \endcode
     */
    class grpc_server
    {
    public:
        /*!
         * \brief Construct gRPC server
         * \param config Server configuration
         */
        explicit grpc_server(const grpc_server_config& config = {});

        /*!
         * \brief Destructor
         */
        ~grpc_server();

        // Non-copyable
        grpc_server(const grpc_server&) = delete;
        grpc_server& operator=(const grpc_server&) = delete;

        // Movable
        grpc_server(grpc_server&&) noexcept;
        grpc_server& operator=(grpc_server&&) noexcept;

        /*!
         * \brief Start the server on specified port
         * \param port Port number to listen on
         * \return Success or error
         */
        auto start(uint16_t port) -> VoidResult;

        /*!
         * \brief Start the server with TLS
         * \param port Port number to listen on
         * \param cert_path Path to server certificate (PEM)
         * \param key_path Path to server private key (PEM)
         * \param ca_path Optional path to CA certificates for client auth
         * \return Success or error
         */
        auto start_tls(uint16_t port,
                       const std::string& cert_path,
                       const std::string& key_path,
                       const std::string& ca_path = "") -> VoidResult;

        /*!
         * \brief Stop the server
         */
        auto stop() -> void;

        /*!
         * \brief Wait for server to finish (blocks)
         */
        auto wait() -> void;

        /*!
         * \brief Check if server is running
         * \return True if running
         */
        auto is_running() const -> bool;

        /*!
         * \brief Get the port the server is listening on
         * \return Port number, or 0 if not running
         */
        auto port() const -> uint16_t;

        /*!
         * \brief Register a service
         * \param service Service to register
         * \return Success or error
         */
        auto register_service(grpc_service* service) -> VoidResult;

        /*!
         * \brief Register a unary RPC method handler
         * \param full_method_name Full method name (e.g., "/package.Service/Method")
         * \param handler Handler function
         * \return Success or error
         */
        auto register_unary_method(const std::string& full_method_name,
                                   unary_handler handler) -> VoidResult;

        /*!
         * \brief Register a server streaming RPC method handler
         * \param full_method_name Full method name
         * \param handler Handler function
         * \return Success or error
         */
        auto register_server_streaming_method(const std::string& full_method_name,
                                              server_streaming_handler handler) -> VoidResult;

        /*!
         * \brief Register a client streaming RPC method handler
         * \param full_method_name Full method name
         * \param handler Handler function
         * \return Success or error
         */
        auto register_client_streaming_method(const std::string& full_method_name,
                                              client_streaming_handler handler) -> VoidResult;

        /*!
         * \brief Register a bidirectional streaming RPC method handler
         * \param full_method_name Full method name
         * \param handler Handler function
         * \return Success or error
         */
        auto register_bidi_streaming_method(const std::string& full_method_name,
                                            bidi_streaming_handler handler) -> VoidResult;

    private:
        class impl;
        std::unique_ptr<impl> impl_;
    };

} // namespace kcenon::network::protocols::grpc
