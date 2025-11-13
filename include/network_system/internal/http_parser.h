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

#include "network_system/internal/http_types.h"
#include "network_system/utils/result_types.h"
#include <vector>
#include <string>
#include <string_view>

namespace network_system::internal
{
    /*!
     * \class http_parser
     * \brief Parser and serializer for HTTP messages (requests and responses)
     *
     * ### Thread Safety
     * - All methods are stateless and can be called from multiple threads
     * - No internal state is maintained between calls
     *
     * ### Features
     * - Parse HTTP requests from raw bytes
     * - Parse HTTP responses from raw bytes
     * - Serialize HTTP requests to raw bytes
     * - Serialize HTTP responses to raw bytes
     * - URL encoding/decoding for query parameters
     * - Chunked transfer encoding support (future)
     *
     * ### Usage Example
     * \code
     * // Parse HTTP request
     * std::vector<uint8_t> raw_data = ...;
     * auto result = http_parser::parse_request(raw_data);
     * if (result) {
     *     const auto& request = result.value();
     *     std::cout << "Method: " << http_method_to_string(request.method) << "\n";
     *     std::cout << "URI: " << request.uri << "\n";
     * }
     *
     * // Serialize HTTP response
     * http_response response;
     * response.status_code = 200;
     * response.set_body_string("Hello, World!");
     * auto bytes = http_parser::serialize_response(response);
     * \endcode
     */
    class http_parser
    {
    public:
        /*!
         * \brief Parse HTTP request from raw bytes
         * \param data Raw HTTP request data
         * \return Result containing parsed request or error
         *
         * Parses the following format:
         * ```
         * METHOD URI HTTP/VERSION\r\n
         * Header1: Value1\r\n
         * Header2: Value2\r\n
         * \r\n
         * [body]
         * ```
         */
        static auto parse_request(const std::vector<uint8_t>& data) -> Result<http_request>;

        /*!
         * \brief Parse HTTP request from string view
         * \param data String view of HTTP request
         * \return Result containing parsed request or error
         */
        static auto parse_request(std::string_view data) -> Result<http_request>;

        /*!
         * \brief Parse HTTP response from raw bytes
         * \param data Raw HTTP response data
         * \return Result containing parsed response or error
         *
         * Parses the following format:
         * ```
         * HTTP/VERSION STATUS_CODE STATUS_MESSAGE\r\n
         * Header1: Value1\r\n
         * Header2: Value2\r\n
         * \r\n
         * [body]
         * ```
         */
        static auto parse_response(const std::vector<uint8_t>& data) -> Result<http_response>;

        /*!
         * \brief Parse HTTP response from string view
         * \param data String view of HTTP response
         * \return Result containing parsed response or error
         */
        static auto parse_response(std::string_view data) -> Result<http_response>;

        /*!
         * \brief Serialize HTTP request to raw bytes
         * \param request HTTP request to serialize
         * \return Serialized request as byte vector
         */
        static auto serialize_request(const http_request& request) -> std::vector<uint8_t>;

        /*!
         * \brief Serialize HTTP response to raw bytes
         * \param response HTTP response to serialize
         * \return Serialized response as byte vector
         */
        static auto serialize_response(const http_response& response) -> std::vector<uint8_t>;

        /*!
         * \brief URL encode a string
         * \param value String to encode
         * \return URL-encoded string
         *
         * Encodes special characters as %XX where XX is hex code
         * Spaces are encoded as %20 (not +)
         */
        static auto url_encode(const std::string& value) -> std::string;

        /*!
         * \brief URL decode a string
         * \param value String to decode
         * \return URL-decoded string
         *
         * Decodes %XX sequences back to original characters
         * Handles both %20 and + as spaces
         */
        static auto url_decode(const std::string& value) -> std::string;

        /*!
         * \brief Parse query string into key-value pairs
         * \param query_string Query string (e.g., "key1=value1&key2=value2")
         * \return Map of query parameters
         */
        static auto parse_query_string(const std::string& query_string)
            -> std::map<std::string, std::string>;

        /*!
         * \brief Build query string from key-value pairs
         * \param params Map of query parameters
         * \return Query string (e.g., "key1=value1&key2=value2")
         */
        static auto build_query_string(const std::map<std::string, std::string>& params)
            -> std::string;

        /*!
         * \brief Parse cookies from Cookie header
         * \param request HTTP request to parse cookies into
         *
         * Parses the Cookie header (if present) and populates request.cookies
         * Cookie format: "name1=value1; name2=value2"
         */
        static auto parse_cookies(http_request& request) -> void;

        /*!
         * \brief Parse multipart/form-data from request body
         * \param request HTTP request to parse multipart data from
         * \return Result indicating success or error
         *
         * Parses multipart/form-data body and populates request.form_data and request.files
         * Requires Content-Type header with boundary parameter
         */
        static auto parse_multipart_form_data(http_request& request) -> VoidResult;

    private:
        // Helper functions for parsing
        static auto parse_request_line(std::string_view line) -> Result<http_request>;
        static auto parse_status_line(std::string_view line) -> Result<http_response>;
        static auto parse_headers(std::string_view headers_section,
                                   std::map<std::string, std::string>& headers) -> bool;
        static auto trim(std::string_view str) -> std::string_view;
        static auto split_line(std::string_view data) -> std::pair<std::string_view, std::string_view>;
    };

} // namespace network_system::internal
