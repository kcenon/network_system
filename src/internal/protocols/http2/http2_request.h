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

#include "hpack.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::network::protocols::http2
{
    /*!
     * \struct http2_request
     * \brief HTTP/2 request data structure
     *
     * Represents an incoming HTTP/2 request with all pseudo-headers
     * and regular headers parsed from HEADERS frames.
     *
     * ### Pseudo-Headers (RFC 7540 Section 8.1.2.3)
     * - :method - HTTP method (GET, POST, etc.)
     * - :path - Request path
     * - :scheme - URI scheme (http or https)
     * - :authority - Authority portion of target URI
     */
    struct http2_request
    {
        std::string method;                      //!< HTTP method (:method pseudo-header)
        std::string path;                        //!< Request path (:path pseudo-header)
        std::string authority;                   //!< Authority (:authority pseudo-header)
        std::string scheme;                      //!< Scheme (:scheme pseudo-header)
        std::vector<http_header> headers;        //!< Regular headers (non-pseudo)
        std::vector<uint8_t> body;               //!< Request body from DATA frames

        /*!
         * \brief Get header value by name (case-insensitive)
         * \param name Header name to search for
         * \return Header value if found, empty optional otherwise
         */
        [[nodiscard]] auto get_header(std::string_view name) const
            -> std::optional<std::string>
        {
            auto it = std::find_if(headers.begin(), headers.end(),
                [&name](const http_header& h) {
                    if (h.name.size() != name.size()) return false;
                    for (size_t i = 0; i < h.name.size(); ++i) {
                        if (std::tolower(static_cast<unsigned char>(h.name[i])) !=
                            std::tolower(static_cast<unsigned char>(name[i]))) {
                            return false;
                        }
                    }
                    return true;
                });
            if (it != headers.end()) {
                return it->value;
            }
            return std::nullopt;
        }

        /*!
         * \brief Get Content-Type header value
         * \return Content-Type value if present
         */
        [[nodiscard]] auto content_type() const -> std::optional<std::string>
        {
            return get_header("content-type");
        }

        /*!
         * \brief Get Content-Length header value as size_t
         * \return Content-Length value if present and valid
         */
        [[nodiscard]] auto content_length() const -> std::optional<size_t>
        {
            auto value = get_header("content-length");
            if (!value) {
                return std::nullopt;
            }
            try {
                return static_cast<size_t>(std::stoull(*value));
            } catch (...) {
                return std::nullopt;
            }
        }

        /*!
         * \brief Get request body as UTF-8 string
         * \return Body content as string
         */
        [[nodiscard]] auto get_body_string() const -> std::string
        {
            return std::string(body.begin(), body.end());
        }

        /*!
         * \brief Check if this is a valid HTTP/2 request
         * \return True if all required pseudo-headers are present
         *
         * Per RFC 7540 Section 8.1.2.3, requests must include:
         * - :method (except for CONNECT)
         * - :scheme (except for CONNECT)
         * - :path (except for CONNECT with authority-form)
         */
        [[nodiscard]] auto is_valid() const -> bool
        {
            if (method.empty()) {
                return false;
            }
            // CONNECT requests have special rules
            if (method == "CONNECT") {
                return !authority.empty();
            }
            // Normal requests require :method, :scheme, and :path
            return !scheme.empty() && !path.empty();
        }

        /*!
         * \brief Create http2_request from parsed headers
         * \param parsed_headers Headers decoded from HPACK
         * \return http2_request with pseudo-headers and regular headers separated
         *
         * This method separates pseudo-headers (starting with ':') from
         * regular headers and populates the appropriate fields.
         */
        static auto from_headers(const std::vector<http_header>& parsed_headers)
            -> http2_request
        {
            http2_request request;
            for (const auto& header : parsed_headers) {
                if (header.name.empty()) {
                    continue;
                }
                if (header.name[0] == ':') {
                    // Pseudo-header
                    if (header.name == ":method") {
                        request.method = header.value;
                    } else if (header.name == ":path") {
                        request.path = header.value;
                    } else if (header.name == ":scheme") {
                        request.scheme = header.value;
                    } else if (header.name == ":authority") {
                        request.authority = header.value;
                    }
                    // Ignore unknown pseudo-headers
                } else {
                    // Regular header
                    request.headers.push_back(header);
                }
            }
            return request;
        }
    };

} // namespace kcenon::network::protocols::http2
