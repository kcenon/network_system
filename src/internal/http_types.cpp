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

#include "network_system/internal/http_types.h"
#include <algorithm>
#include <cctype>

namespace network_system::internal
{
    namespace
    {
        // Convert string to lowercase for case-insensitive comparison
        auto to_lower(std::string str) -> std::string
        {
            std::transform(str.begin(), str.end(), str.begin(),
                [](unsigned char c) { return std::tolower(c); });
            return str;
        }
    }

    // http_request methods

    auto http_request::get_header(const std::string& name) const -> std::optional<std::string>
    {
        auto lower_name = to_lower(name);
        for (const auto& [key, value] : headers)
        {
            if (to_lower(key) == lower_name)
            {
                return value;
            }
        }
        return std::nullopt;
    }

    auto http_request::set_header(const std::string& name, const std::string& value) -> void
    {
        // Remove existing header with same name (case-insensitive)
        auto lower_name = to_lower(name);
        for (auto it = headers.begin(); it != headers.end();)
        {
            if (to_lower(it->first) == lower_name)
            {
                it = headers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Add new header
        headers[name] = value;
    }

    auto http_request::get_body_string() const -> std::string
    {
        return std::string(body.begin(), body.end());
    }

    auto http_request::set_body_string(const std::string& content) -> void
    {
        body.assign(content.begin(), content.end());
    }

    // http_response methods

    auto http_response::get_header(const std::string& name) const -> std::optional<std::string>
    {
        auto lower_name = to_lower(name);
        for (const auto& [key, value] : headers)
        {
            if (to_lower(key) == lower_name)
            {
                return value;
            }
        }
        return std::nullopt;
    }

    auto http_response::set_header(const std::string& name, const std::string& value) -> void
    {
        // Remove existing header with same name (case-insensitive)
        auto lower_name = to_lower(name);
        for (auto it = headers.begin(); it != headers.end();)
        {
            if (to_lower(it->first) == lower_name)
            {
                it = headers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Add new header
        headers[name] = value;
    }

    auto http_response::get_body_string() const -> std::string
    {
        return std::string(body.begin(), body.end());
    }

    auto http_response::set_body_string(const std::string& content) -> void
    {
        body.assign(content.begin(), content.end());
    }

    // Helper functions

    auto http_method_to_string(http_method method) -> std::string
    {
        switch (method)
        {
            case http_method::GET:     return "GET";
            case http_method::POST:    return "POST";
            case http_method::PUT:     return "PUT";
            case http_method::DELETE:  return "DELETE";
            case http_method::HEAD:    return "HEAD";
            case http_method::OPTIONS: return "OPTIONS";
            case http_method::PATCH:   return "PATCH";
            case http_method::CONNECT: return "CONNECT";
            case http_method::TRACE:   return "TRACE";
            default:                   return "GET";
        }
    }

    auto string_to_http_method(const std::string& method_str) -> std::optional<http_method>
    {
        auto upper_method = method_str;
        std::transform(upper_method.begin(), upper_method.end(), upper_method.begin(),
            [](unsigned char c) { return std::toupper(c); });

        if (upper_method == "GET")     return http_method::GET;
        if (upper_method == "POST")    return http_method::POST;
        if (upper_method == "PUT")     return http_method::PUT;
        if (upper_method == "DELETE")  return http_method::DELETE;
        if (upper_method == "HEAD")    return http_method::HEAD;
        if (upper_method == "OPTIONS") return http_method::OPTIONS;
        if (upper_method == "PATCH")   return http_method::PATCH;
        if (upper_method == "CONNECT") return http_method::CONNECT;
        if (upper_method == "TRACE")   return http_method::TRACE;

        return std::nullopt;
    }

    auto http_version_to_string(http_version version) -> std::string
    {
        switch (version)
        {
            case http_version::HTTP_1_0: return "HTTP/1.0";
            case http_version::HTTP_1_1: return "HTTP/1.1";
            case http_version::HTTP_2_0: return "HTTP/2.0";
            default:                     return "HTTP/1.1";
        }
    }

    auto string_to_http_version(const std::string& version_str) -> std::optional<http_version>
    {
        if (version_str == "HTTP/1.0") return http_version::HTTP_1_0;
        if (version_str == "HTTP/1.1") return http_version::HTTP_1_1;
        if (version_str == "HTTP/2.0") return http_version::HTTP_2_0;
        if (version_str == "HTTP/2")   return http_version::HTTP_2_0;

        return std::nullopt;
    }

    auto get_status_message(int status_code) -> std::string
    {
        switch (status_code)
        {
            // 1xx Informational
            case 100: return "Continue";
            case 101: return "Switching Protocols";

            // 2xx Success
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 203: return "Non-Authoritative Information";
            case 204: return "No Content";
            case 205: return "Reset Content";
            case 206: return "Partial Content";

            // 3xx Redirection
            case 300: return "Multiple Choices";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";

            // 4xx Client Error
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 402: return "Payment Required";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 407: return "Proxy Authentication Required";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 411: return "Length Required";
            case 412: return "Precondition Failed";
            case 413: return "Payload Too Large";
            case 414: return "URI Too Long";
            case 415: return "Unsupported Media Type";
            case 416: return "Range Not Satisfiable";
            case 417: return "Expectation Failed";
            case 429: return "Too Many Requests";

            // 5xx Server Error
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";

            default:  return "Unknown";
        }
    }

} // namespace network_system::internal
