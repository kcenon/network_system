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

#include "internal/core/messaging_client.h"
#include "internal/http/http_types.h"
#include "internal/http/http_parser.h"
#include "kcenon/network/utils/result_types.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>

namespace kcenon::network::core
{
    /*!
     * \struct http_url
     * \brief Parsed URL components
     */
    struct http_url
    {
        std::string scheme;      // http or https
        std::string host;        // hostname or IP
        unsigned short port = 0; // port number (0 = default)
        std::string path;        // URI path
        std::map<std::string, std::string> query; // query parameters

        /*!
         * \brief Parse URL string into components
         * \param url URL string (e.g., "http://example.com:8080/path?key=value")
         * \return Result containing parsed URL or error
         */
        static auto parse(const std::string& url) -> Result<http_url>;

        /*!
         * \brief Get default port for scheme
         * \return Default port (80 for http, 443 for https)
         */
        auto get_default_port() const -> unsigned short;
    };

    /*!
     * \class http_client
     * \brief HTTP/1.1 client built on top of messaging_client
     *
     * ### Thread Safety
     * - All public methods are thread-safe
     * - Multiple requests can be made concurrently from different threads
     * - Each request uses its own messaging_client instance
     *
     * ### Features
     * - HTTP/1.1 protocol support
     * - GET, POST, PUT, DELETE, HEAD, OPTIONS, PATCH methods
     * - Custom headers support
     * - Query parameters support
     * - Request/response body handling
     * - Timeout configuration
     * - Automatic Content-Length calculation
     * - Connection: close header for proper cleanup
     *
     * ### Usage Example
     * \code
     * auto client = std::make_shared<http_client>();
     *
     * // Simple GET request
     * auto response = client->get("http://example.com/api/users");
     * if (response) {
     *     std::cout << "Status: " << response->status_code << "\n";
     *     std::cout << "Body: " << response->get_body_string() << "\n";
     * }
     *
     * // POST with JSON body
     * std::map<std::string, std::string> headers = {
     *     {"Content-Type", "application/json"}
     * };
     * std::string json_body = R"({"name": "John", "age": 30})";
     * auto post_response = client->post("http://example.com/api/users",
     *                                   json_body, headers);
     * \endcode
     *
     * ### Limitations
     * - Only HTTP/1.1 supported (no HTTP/2)
     * - No persistent connections (Connection: close)
     * - No chunked transfer encoding
     * - No automatic redirect following
     * - No cookie management
     * - No compression support
     */
    class http_client : public std::enable_shared_from_this<http_client>
    {
    public:
        /*!
         * \brief Construct HTTP client with default timeout
         */
        http_client();

        /*!
         * \brief Construct HTTP client with custom timeout
         * \param timeout_ms Timeout in milliseconds
         */
        explicit http_client(std::chrono::milliseconds timeout_ms);

        /*!
         * \brief Destructor
         */
        ~http_client() = default;

        /*!
         * \brief Perform HTTP GET request
         * \param url Full URL (e.g., "http://example.com/path")
         * \param query Query parameters (optional)
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto get(const std::string& url,
                 const std::map<std::string, std::string>& query = {},
                 const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Perform HTTP POST request
         * \param url Full URL
         * \param body Request body
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto post(const std::string& url,
                  const std::string& body,
                  const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Perform HTTP POST request with binary body
         * \param url Full URL
         * \param body Request body as bytes
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto post(const std::string& url,
                  const std::vector<uint8_t>& body,
                  const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Perform HTTP PUT request
         * \param url Full URL
         * \param body Request body
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto put(const std::string& url,
                 const std::string& body,
                 const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Perform HTTP DELETE request
         * \param url Full URL
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto del(const std::string& url,
                 const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Perform HTTP HEAD request
         * \param url Full URL
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto head(const std::string& url,
                  const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Perform HTTP PATCH request
         * \param url Full URL
         * \param body Request body
         * \param headers Custom headers (optional)
         * \return Result containing HTTP response or error
         */
        auto patch(const std::string& url,
                   const std::string& body,
                   const std::map<std::string, std::string>& headers = {})
            -> Result<internal::http_response>;

        /*!
         * \brief Set request timeout
         * \param timeout_ms Timeout in milliseconds
         */
        auto set_timeout(std::chrono::milliseconds timeout_ms) -> void;

        /*!
         * \brief Get current timeout setting
         * \return Timeout in milliseconds
         */
        auto get_timeout() const -> std::chrono::milliseconds;

    private:
        /*!
         * \brief Perform generic HTTP request
         * \param method HTTP method
         * \param url Full URL
         * \param body Request body (optional)
         * \param headers Custom headers
         * \param query Query parameters
         * \return Result containing HTTP response or error
         */
        auto request(internal::http_method method,
                     const std::string& url,
                     const std::vector<uint8_t>& body,
                     const std::map<std::string, std::string>& headers,
                     const std::map<std::string, std::string>& query)
            -> Result<internal::http_response>;

        /*!
         * \brief Build HTTP request from components
         * \param method HTTP method
         * \param url_info Parsed URL
         * \param body Request body
         * \param headers Custom headers
         * \return HTTP request object
         */
        auto build_request(internal::http_method method,
                           const http_url& url_info,
                           const std::vector<uint8_t>& body,
                           const std::map<std::string, std::string>& headers)
            -> internal::http_request;

        std::chrono::milliseconds timeout_;
    };

} // namespace kcenon::network::core
