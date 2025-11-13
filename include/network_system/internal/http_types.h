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

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <optional>

namespace network_system::internal
{
    /*!
     * \enum http_method
     * \brief HTTP request methods (verbs)
     *
     * Note: Uses HTTP_ prefix to avoid conflicts with Windows macros (DELETE)
     */
    enum class http_method
    {
        HTTP_GET,
        HTTP_POST,
        HTTP_PUT,
        HTTP_DELETE,
        HTTP_HEAD,
        HTTP_OPTIONS,
        HTTP_PATCH,
        HTTP_CONNECT,
        HTTP_TRACE
    };

    /*!
     * \enum http_version
     * \brief HTTP protocol version
     */
    enum class http_version
    {
        HTTP_1_0,
        HTTP_1_1,
        HTTP_2_0
    };

    /*!
     * \struct cookie
     * \brief Represents an HTTP cookie
     *
     * ### Structure
     * - name: Cookie name
     * - value: Cookie value
     * - path: Path scope for the cookie
     * - domain: Domain scope for the cookie
     * - expires: Expiration date (HTTP date format)
     * - max_age: Maximum age in seconds (-1 = session cookie)
     * - secure: Secure flag (HTTPS only)
     * - http_only: HttpOnly flag (not accessible via JavaScript)
     * - same_site: SameSite attribute ("Strict", "Lax", "None")
     */
    struct cookie
    {
        std::string name;
        std::string value;
        std::string path;
        std::string domain;
        std::string expires;
        int max_age = -1;  // -1 = session cookie
        bool secure = false;
        bool http_only = false;
        std::string same_site;  // "Strict", "Lax", "None"

        /*!
         * \brief Convert cookie to Set-Cookie header value
         * \return Cookie as Set-Cookie header string
         */
        auto to_header_value() const -> std::string;
    };

    /*!
     * \struct multipart_file
     * \brief Represents a file uploaded via multipart/form-data
     *
     * ### Structure
     * - field_name: Form field name
     * - filename: Original filename
     * - content_type: MIME type of the file
     * - content: File content as raw bytes
     */
    struct multipart_file
    {
        std::string field_name;
        std::string filename;
        std::string content_type;
        std::vector<uint8_t> content;
    };

    /*!
     * \struct http_request
     * \brief Represents an HTTP request message
     *
     * ### Structure
     * - method: HTTP method (GET, POST, etc.)
     * - uri: Request URI (e.g., "/api/users")
     * - version: HTTP version (1.0, 1.1, 2.0)
     * - headers: Map of header name to value
     * - body: Request body as raw bytes
     * - query_params: Parsed query parameters from URI
     */
    struct http_request
    {
        http_method method = http_method::HTTP_GET;
        std::string uri;
        http_version version = http_version::HTTP_1_1;
        std::map<std::string, std::string> headers;
        std::vector<uint8_t> body;
        std::map<std::string, std::string> query_params;
        std::map<std::string, std::string> cookies;  // Parsed cookies from Cookie header
        std::map<std::string, std::string> form_data;  // Form fields from multipart/form-data
        std::map<std::string, multipart_file> files;  // Uploaded files from multipart/form-data

        /*!
         * \brief Get the value of a header (case-insensitive)
         * \param name Header name
         * \return Optional header value
         */
        auto get_header(const std::string& name) const -> std::optional<std::string>;

        /*!
         * \brief Set a header value
         * \param name Header name
         * \param value Header value
         */
        auto set_header(const std::string& name, const std::string& value) -> void;

        /*!
         * \brief Get body as string
         * \return Body content as UTF-8 string
         */
        auto get_body_string() const -> std::string;

        /*!
         * \brief Set body from string
         * \param content Body content as UTF-8 string
         */
        auto set_body_string(const std::string& content) -> void;
    };

    /*!
     * \struct http_response
     * \brief Represents an HTTP response message
     *
     * ### Structure
     * - status_code: HTTP status code (200, 404, etc.)
     * - status_message: Status message ("OK", "Not Found", etc.)
     * - version: HTTP version (1.0, 1.1, 2.0)
     * - headers: Map of header name to value
     * - body: Response body as raw bytes
     */
    struct http_response
    {
        int status_code = 200;
        std::string status_message = "OK";
        http_version version = http_version::HTTP_1_1;
        std::map<std::string, std::string> headers;
        std::vector<uint8_t> body;
        std::vector<cookie> set_cookies;  // Cookies to set in Set-Cookie headers

        /*!
         * \brief Get the value of a header (case-insensitive)
         * \param name Header name
         * \return Optional header value
         */
        auto get_header(const std::string& name) const -> std::optional<std::string>;

        /*!
         * \brief Set a header value
         * \param name Header name
         * \param value Header value
         */
        auto set_header(const std::string& name, const std::string& value) -> void;

        /*!
         * \brief Get body as string
         * \return Body content as UTF-8 string
         */
        auto get_body_string() const -> std::string;

        /*!
         * \brief Set body from string
         * \param content Body content as UTF-8 string
         */
        auto set_body_string(const std::string& content) -> void;

        /*!
         * \brief Set a cookie in the response
         * \param name Cookie name
         * \param value Cookie value
         * \param path Cookie path (default: "/")
         * \param max_age Maximum age in seconds (default: -1 for session cookie)
         * \param http_only HttpOnly flag (default: true)
         * \param secure Secure flag (default: false)
         * \param same_site SameSite attribute (default: empty)
         */
        auto set_cookie(const std::string& name, const std::string& value,
                       const std::string& path = "/", int max_age = -1,
                       bool http_only = true, bool secure = false,
                       const std::string& same_site = "") -> void;
    };

    /*!
     * \brief Convert HTTP method enum to string
     * \param method HTTP method enum value
     * \return Method as string (e.g., "GET", "POST")
     */
    auto http_method_to_string(http_method method) -> std::string;

    /*!
     * \brief Convert string to HTTP method enum
     * \param method_str Method string (e.g., "GET", "POST")
     * \return HTTP method enum value
     */
    auto string_to_http_method(const std::string& method_str) -> std::optional<http_method>;

    /*!
     * \brief Convert HTTP version enum to string
     * \param version HTTP version enum value
     * \return Version as string (e.g., "HTTP/1.1")
     */
    auto http_version_to_string(http_version version) -> std::string;

    /*!
     * \brief Convert string to HTTP version enum
     * \param version_str Version string (e.g., "HTTP/1.1")
     * \return HTTP version enum value
     */
    auto string_to_http_version(const std::string& version_str) -> std::optional<http_version>;

    /*!
     * \brief Get HTTP status message for a status code
     * \param status_code HTTP status code
     * \return Status message (e.g., "OK" for 200)
     */
    auto get_status_message(int status_code) -> std::string;

} // namespace network_system::internal
