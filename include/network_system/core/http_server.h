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

#include "network_system/core/messaging_server.h"
#include "network_system/internal/http_types.h"
#include "network_system/internal/http_parser.h"
#include "network_system/utils/result_types.h"
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <regex>
#include <vector>

namespace network_system::core
{
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

    private:
        /*!
         * \struct http_request_buffer
         * \brief Buffer for assembling HTTP requests from multiple TCP chunks
         */
        struct http_request_buffer
        {
            std::vector<uint8_t> data;           // Accumulated request data
            bool headers_complete = false;       // Whether headers have been fully received
            std::size_t content_length = 0;      // Parsed Content-Length value
            std::size_t headers_end_pos = 0;     // Position where headers end

            // Maximum allowed request size (10 MB default)
            static constexpr std::size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024;
            // Maximum allowed header size (64 KB)
            static constexpr std::size_t MAX_HEADER_SIZE = 64 * 1024;
        };

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
         * \brief Handle incoming HTTP request data chunk
         * \param session Session that received the data
         * \param chunk Raw request data chunk
         */
        auto handle_request_data(std::shared_ptr<network_system::session::messaging_session> session,
                                const std::vector<uint8_t>& chunk) -> void;

        /*!
         * \brief Process complete HTTP request
         * \param request_data Complete raw request data
         * \return HTTP response data
         */
        auto process_complete_request(const std::vector<uint8_t>& request_data) -> std::vector<uint8_t>;

        /*!
         * \brief Parse Content-Length from header buffer
         * \param data Request data containing headers
         * \param headers_end_pos Position where headers end
         * \return Content-Length value, or 0 if not found
         */
        auto parse_content_length(const std::vector<uint8_t>& data, std::size_t headers_end_pos) -> std::size_t;

        /*!
         * \brief Check if request is complete
         * \param buffer Request buffer
         * \return true if request is complete and ready to process
         */
        auto is_request_complete(const http_request_buffer& buffer) const -> bool;

        /*!
         * \brief Create default error response
         * \param status_code HTTP status code
         * \param message Error message
         * \return HTTP response
         */
        auto create_error_response(int status_code, const std::string& message)
            -> internal::http_response;

        /*!
         * \brief Send error response to session
         * \param session Session to send error to
         * \param status_code HTTP status code
         * \param message Error message
         */
        auto send_error_response(std::shared_ptr<network_system::session::messaging_session> session,
                                int status_code,
                                const std::string& message) -> void;

        /*!
         * \brief Convert route pattern to regex pattern
         * \param pattern Route pattern (e.g., "/users/:id")
         * \param param_names Output: parameter names
         * \return Regex pattern string
         */
        static auto pattern_to_regex(const std::string& pattern,
                                      std::vector<std::string>& param_names) -> std::string;

        std::shared_ptr<messaging_server> tcp_server_;
        std::vector<http_route> routes_;
        std::mutex routes_mutex_;
        http_handler not_found_handler_;
        http_handler error_handler_;

        // Session-specific request buffers
        std::map<std::shared_ptr<network_system::session::messaging_session>, http_request_buffer> request_buffers_;
        std::mutex buffers_mutex_;
    };

} // namespace network_system::core
