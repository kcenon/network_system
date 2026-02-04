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

#include "internal/core/messaging_server.h"
#include "internal/http/http_types.h"
#include "internal/http/http_parser.h"
#include "internal/http/http_error.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "internal/utils/compression_pipeline.h"
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <regex>
#include <vector>
#include <chrono>

namespace kcenon::network::core
{
    /*!
     * \struct http_request_buffer
     * \brief Buffer for accumulating HTTP request data received in chunks
     */
    struct http_request_buffer
    {
        std::vector<uint8_t> data;
        bool headers_complete = false;
        std::size_t headers_end_pos = 0;
        std::size_t content_length = 0;

        static constexpr std::size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024;  // 10MB
        static constexpr std::size_t MAX_HEADER_SIZE = 64 * 1024;          // 64KB

        /*!
         * \brief Append new data chunk to buffer
         * \param chunk New data to append
         * \return true if chunk was appended successfully, false if size limit exceeded
         */
        auto append(const std::vector<uint8_t>& chunk) -> bool;

        /*!
         * \brief Check if complete HTTP request has been received
         * \return true if headers and body are complete
         */
        auto is_complete() const -> bool;

        /*!
         * \brief Find end of HTTP headers (\\r\\n\\r\\n marker)
         * \return Position of headers end, or npos if not found
         */
        static auto find_header_end(const std::vector<uint8_t>& data) -> std::size_t;

        /*!
         * \brief Parse Content-Length from headers
         * \param data Request data with headers
         * \param headers_end Position where headers end
         * \return Content-Length value, or 0 if not found
         */
        static auto parse_content_length(const std::vector<uint8_t>& data,
                                         std::size_t headers_end) -> std::size_t;
    };

    /*!
     * \struct http_request_context
     * \brief Context for an HTTP request with parsed components
     */
    struct http_request_context
    {
        internal::http_request request;
        std::map<std::string, std::string> path_params; // Extracted path parameters (e.g., /users/:id)

        /*!
         * \brief Get query parameter value
         * \param name Parameter name
         * \return Optional parameter value
         */
        auto get_query_param(const std::string& name) const -> std::optional<std::string>
        {
            auto it = request.query_params.find(name);
            if (it != request.query_params.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        /*!
         * \brief Get path parameter value
         * \param name Parameter name
         * \return Optional parameter value
         */
        auto get_path_param(const std::string& name) const -> std::optional<std::string>
        {
            auto it = path_params.find(name);
            if (it != path_params.end())
            {
                return it->second;
            }
            return std::nullopt;
        }
    };

    /*!
     * \typedef http_handler
     * \brief Handler function for HTTP requests
     *
     * \param ctx Request context with parsed request and parameters
     * \return HTTP response to send back to client
     */
    using http_handler = std::function<internal::http_response(const http_request_context& ctx)>;

    /*!
     * \typedef error_handler
     * \brief Handler function for HTTP errors
     *
     * \param error Error details
     * \return HTTP response to send back to client
     */
    using error_handler = std::function<internal::http_response(const internal::http_error& error)>;

    /*!
     * \struct http_route
     * \brief Route definition with pattern matching and handler
     */
    struct http_route
    {
        internal::http_method method;
        std::string pattern;        // e.g., "/users/:id"
        std::regex regex_pattern;   // Compiled regex for matching
        std::vector<std::string> param_names; // Parameter names extracted from pattern
        http_handler handler;

        /*!
         * \brief Check if request matches this route
         * \param method HTTP method
         * \param path URI path
         * \param path_params Output: extracted path parameters
         * \return true if route matches
         */
        auto matches(internal::http_method method, const std::string& path,
                     std::map<std::string, std::string>& path_params) const -> bool;
    };

    /*!
     * \class http_server
     * \brief HTTP/1.1 server built on top of messaging_server
     *
     * ### Thread Safety
     * - All public methods are thread-safe
     * - Routes can be registered before or after server starts
     * - Multiple requests are handled concurrently
     * - Route handlers should be thread-safe
     *
     * ### Features
     * - HTTP/1.1 protocol support
     * - GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH methods
     * - Route pattern matching with path parameters (e.g., /users/:id)
     * - Query parameter parsing
     * - Request body handling
     * - Automatic Content-Length header
     * - Custom error handlers (404, 500, etc.)
     * - Middleware support (future)
     *
     * ### Usage Example
     * \code
     * auto server = std::make_shared<http_server>("my_http_server");
     *
     * // Register routes
     * server->get("/", [](const http_request_context& ctx) {
     *     http_response response;
     *     response.status_code = 200;
     *     response.set_body_string("Hello, World!");
     *     response.set_header("Content-Type", "text/plain");
     *     return response;
     * });
     *
     * server->get("/users/:id", [](const http_request_context& ctx) {
     *     auto user_id = ctx.get_path_param("id").value_or("unknown");
     *     http_response response;
     *     response.status_code = 200;
     *     response.set_body_string("User ID: " + user_id);
     *     response.set_header("Content-Type", "text/plain");
     *     return response;
     * });
     *
     * // Start server
     * server->start(8080);
     * server->wait_for_stop();
     * \endcode
     *
     * ### Route Patterns
     * - Static routes: `/users`, `/api/v1/products`
     * - Parameter routes: `/users/:id`, `/products/:category/:id`
     * - Regex support for complex patterns
     *
     * ### Limitations
     * - Only HTTP/1.1 supported (no HTTP/2)
     * - No chunked transfer encoding
     * - No compression support
     * - No file upload support (multipart/form-data)
     * - No cookie management
     */
    class http_server : public std::enable_shared_from_this<http_server>
    {
    public:
        /*!
         * \brief Construct HTTP server with server ID
         * \param server_id Server identifier for logging
         */
        explicit http_server(const std::string& server_id);

        /*!
         * \brief Destructor - stops server if running
         */
        ~http_server();

        /*!
         * \brief Start HTTP server on specified port
         * \param port Port number to listen on
         * \return Result indicating success or error
         */
        auto start(unsigned short port) -> VoidResult;

        /*!
         * \brief Stop HTTP server
         * \return Result indicating success or error
         */
        auto stop() -> VoidResult;

        /*!
         * \brief Wait for server to stop (blocking)
         */
        auto wait_for_stop() -> void;

        /*!
         * \brief Register GET route handler
         * \param pattern Route pattern (e.g., "/users/:id")
         * \param handler Handler function
         */
        auto get(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Register POST route handler
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto post(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Register PUT route handler
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto put(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Register DELETE route handler
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto del(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Register PATCH route handler
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto patch(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Register HEAD route handler
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto head(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Register OPTIONS route handler
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto options(const std::string& pattern, http_handler handler) -> void;

        /*!
         * \brief Set custom 404 Not Found handler
         * \param handler Handler function
         */
        auto set_not_found_handler(http_handler handler) -> void;

        /*!
         * \brief Set custom 500 Internal Server Error handler
         * \param handler Handler function
         */
        auto set_error_handler(http_handler handler) -> void;

        /*!
         * \brief Set custom error handler for specific HTTP error code
         * \param code HTTP error code to handle
         * \param handler Error handler function
         */
        auto set_error_handler(internal::http_error_code code, error_handler handler) -> void;

        /*!
         * \brief Set default error handler for all unhandled error codes
         * \param handler Default error handler function
         */
        auto set_default_error_handler(error_handler handler) -> void;

        /*!
         * \brief Set request timeout duration
         * \param timeout Timeout duration
         */
        auto set_request_timeout(std::chrono::milliseconds timeout) -> void;

        /*!
         * \brief Enable JSON format for error responses
         * \param enable True to use JSON, false for HTML
         */
        auto set_json_error_responses(bool enable) -> void;

        /*!
         * \brief Enable automatic response compression
         * \param enable True to enable compression, false to disable
         */
        auto set_compression_enabled(bool enable) -> void;

        /*!
         * \brief Set minimum response size for compression
         * \param threshold_bytes Minimum size in bytes (default: 1024)
         */
        auto set_compression_threshold(size_t threshold_bytes) -> void;

    private:
        /*!
         * \brief Register route with method and pattern
         * \param method HTTP method
         * \param pattern Route pattern
         * \param handler Handler function
         */
        auto register_route(internal::http_method method,
                           const std::string& pattern,
                           http_handler handler) -> void;

        /*!
         * \brief Find matching route for request
         * \param method HTTP method
         * \param path URI path
         * \param path_params Output: extracted path parameters
         * \return Pointer to matching route or nullptr
         */
        auto find_route(internal::http_method method,
                       const std::string& path,
                       std::map<std::string, std::string>& path_params) const
            -> const http_route*;

        /*!
         * \brief Handle incoming HTTP request
         * \param request_data Raw request data
         * \return HTTP response data
         */
        auto handle_request(const std::vector<uint8_t>& request_data) -> std::vector<uint8_t>;

        /*!
         * \brief Process HTTP request and generate response object
         * \param request Parsed HTTP request
         * \return HTTP response object
         */
        auto process_http_request(const internal::http_request& request) -> internal::http_response;

        /*!
         * \brief Determine if connection should be closed after response
         * \param request HTTP request
         * \param response HTTP response
         * \return true if connection should be closed
         */
        auto should_close_connection(const internal::http_request& request,
                                     const internal::http_response& response) const -> bool;

        /*!
         * \brief Create default error response
         * \param status_code HTTP status code
         * \param message Error message
         * \return HTTP response
         */
        auto create_error_response(int status_code, const std::string& message)
            -> internal::http_response;

        /*!
         * \brief Convert route pattern to regex pattern
         * \param pattern Route pattern (e.g., "/users/:id")
         * \param param_names Output: parameter names
         * \return Regex pattern string
         */
        static auto pattern_to_regex(const std::string& pattern,
                                      std::vector<std::string>& param_names) -> std::string;

        /*!
         * \brief Apply compression to response if appropriate
         * \param request HTTP request (for Accept-Encoding header)
         * \param response HTTP response to potentially compress
         */
        auto apply_compression(const internal::http_request& request,
                              internal::http_response& response) -> void;

        /*!
         * \brief Determine compression algorithm from Accept-Encoding header
         * \param accept_encoding Accept-Encoding header value
         * \return Best supported compression algorithm
         */
        auto choose_compression_algorithm(const std::string& accept_encoding) const
            -> utils::compression_algorithm;

        /*!
         * \brief Build error response using registered handlers
         * \param error Error details
         * \return HTTP response
         */
        auto build_error_response(const internal::http_error& error) -> internal::http_response;

        std::shared_ptr<messaging_server> tcp_server_;
        std::vector<http_route> routes_;
        std::mutex routes_mutex_;
        http_handler not_found_handler_;
        http_handler error_handler_;

        // Request buffering support
        std::map<std::shared_ptr<session::messaging_session>, http_request_buffer> session_buffers_;
        std::mutex buffers_mutex_;

        // Compression settings
        bool compression_enabled_ = false;
        size_t compression_threshold_ = 1024;  // 1KB default
        std::mutex compression_mutex_;

        // Error handling settings
        std::map<internal::http_error_code, error_handler> error_handlers_;
        std::mutex error_handlers_mutex_;
        error_handler default_error_handler_;
        bool use_json_errors_ = false;
        std::chrono::milliseconds request_timeout_{30000};  // 30 seconds default
    };

} // namespace kcenon::network::core
