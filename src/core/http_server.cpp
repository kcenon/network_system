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

#include "network_system/core/http_server.h"
#include "network_system/session/messaging_session.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace network_system::core
{
    // http_route implementation

    auto http_route::matches(internal::http_method req_method, const std::string& path,
                              std::map<std::string, std::string>& extracted_params) const -> bool
    {
        // Check method
        if (method != req_method)
        {
            return false;
        }

        // Try to match path with regex
        std::smatch matches;
        if (!std::regex_match(path, matches, regex_pattern))
        {
            return false;
        }

        // Extract path parameters
        extracted_params.clear();
        for (std::size_t i = 0; i < param_names.size() && i + 1 < matches.size(); ++i)
        {
            extracted_params[param_names[i]] = matches[i + 1].str();
        }

        return true;
    }

    // http_request_buffer implementation

    auto http_request_buffer::append(const std::vector<uint8_t>& chunk) -> bool
    {
        // Check size limits
        if (data.size() + chunk.size() > MAX_REQUEST_SIZE)
        {
            return false;  // 413 Payload Too Large
        }

        // Append chunk to buffer
        data.insert(data.end(), chunk.begin(), chunk.end());

        // Find headers end if not yet found
        if (!headers_complete)
        {
            auto marker_pos = find_header_end(data);
            if (marker_pos != std::string::npos)
            {
                headers_complete = true;
                headers_end_pos = marker_pos + 4;  // "\r\n\r\n" length
                content_length = parse_content_length(data, headers_end_pos);
            }
            else if (data.size() > MAX_HEADER_SIZE)
            {
                return false;  // 431 Request Header Fields Too Large
            }
        }

        return true;
    }

    auto http_request_buffer::is_complete() const -> bool
    {
        return headers_complete &&
               data.size() >= (headers_end_pos + content_length);
    }

    auto http_request_buffer::find_header_end(const std::vector<uint8_t>& data) -> std::size_t
    {
        // Look for "\r\n\r\n" sequence
        const char* pattern = "\r\n\r\n";
        for (std::size_t i = 0; i + 3 < data.size(); ++i)
        {
            if (data[i] == '\r' && data[i+1] == '\n' &&
                data[i+2] == '\r' && data[i+3] == '\n')
            {
                return i;
            }
        }
        return std::string::npos;
    }

    auto http_request_buffer::parse_content_length(const std::vector<uint8_t>& data,
                                                    std::size_t headers_end) -> std::size_t
    {
        // Convert headers to string for parsing
        std::string headers_str(reinterpret_cast<const char*>(data.data()),
                               std::min(headers_end, data.size()));

        // Look for Content-Length header (case-insensitive)
        const std::string content_length_key = "content-length:";
        std::size_t pos = 0;

        while (pos < headers_str.size())
        {
            // Find line start
            std::size_t line_start = pos;
            std::size_t line_end = headers_str.find("\r\n", pos);
            if (line_end == std::string::npos)
            {
                break;
            }

            std::string line = headers_str.substr(line_start, line_end - line_start);

            // Convert to lowercase for case-insensitive comparison
            std::string line_lower = line;
            std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(),
                         [](unsigned char c) { return std::tolower(c); });

            // Check if this is Content-Length header
            if (line_lower.find(content_length_key) == 0)
            {
                // Extract value after colon
                std::size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos)
                {
                    std::string value = line.substr(colon_pos + 1);
                    // Trim whitespace
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t\r\n") + 1);

                    try
                    {
                        return std::stoull(value);
                    }
                    catch (...)
                    {
                        return 0;
                    }
                }
            }

            pos = line_end + 2;  // Skip "\r\n"
        }

        return 0;  // No Content-Length header found
    }

    // http_server implementation

    http_server::http_server(const std::string& server_id)
        : tcp_server_(std::make_shared<messaging_server>(server_id))
    {
        // Set up default 404 handler
        not_found_handler_ = [this](const http_request_context&) -> internal::http_response
        {
            return create_error_response(404, "Not Found");
        };

        // Set up default 500 handler
        error_handler_ = [this](const http_request_context&) -> internal::http_response
        {
            return create_error_response(500, "Internal Server Error");
        };
    }

    http_server::~http_server()
    {
        if (tcp_server_)
        {
            tcp_server_->stop_server();
        }
    }

    auto http_server::start(unsigned short port) -> VoidResult
    {
        // Set up receive callback to handle HTTP requests with buffering
        tcp_server_->set_receive_callback([this](std::shared_ptr<network_system::session::messaging_session> session,
                                                  const std::vector<uint8_t>& data)
        {
            if (!session)
            {
                return;
            }

            // Get or create buffer for this session
            std::unique_lock<std::mutex> lock(buffers_mutex_);
            auto& buffer = session_buffers_[session];
            lock.unlock();

            // Append new data to buffer
            if (!buffer.append(data))
            {
                // Buffer size limit exceeded
                lock.lock();
                session_buffers_.erase(session);
                lock.unlock();

                // Send error response
                if (buffer.data.size() > http_request_buffer::MAX_REQUEST_SIZE)
                {
                    auto error_response = create_error_response(413, "Payload Too Large");
                    auto response_data = internal::http_parser::serialize_response(error_response);
                    session->send_packet(std::move(response_data));
                }
                else
                {
                    auto error_response = create_error_response(431, "Request Header Fields Too Large");
                    auto response_data = internal::http_parser::serialize_response(error_response);
                    session->send_packet(std::move(response_data));
                }
                return;
            }

            // Check if request is complete
            if (buffer.is_complete())
            {
                // Process complete request
                auto response_data = handle_request(buffer.data);

                // Clean up buffer
                lock.lock();
                session_buffers_.erase(session);
                lock.unlock();

                // Send response
                session->send_packet(std::move(response_data));
            }
            // Otherwise, wait for more data
        });

        // Start TCP server
        return tcp_server_->start_server(port);
    }

    auto http_server::stop() -> VoidResult
    {
        return tcp_server_->stop_server();
    }

    auto http_server::wait_for_stop() -> void
    {
        tcp_server_->wait_for_stop();
    }

    auto http_server::get(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_GET, pattern, std::move(handler));
    }

    auto http_server::post(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_POST, pattern, std::move(handler));
    }

    auto http_server::put(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_PUT, pattern, std::move(handler));
    }

    auto http_server::del(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_DELETE, pattern, std::move(handler));
    }

    auto http_server::patch(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_PATCH, pattern, std::move(handler));
    }

    auto http_server::head(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_HEAD, pattern, std::move(handler));
    }

    auto http_server::options(const std::string& pattern, http_handler handler) -> void
    {
        register_route(internal::http_method::HTTP_OPTIONS, pattern, std::move(handler));
    }

    auto http_server::set_not_found_handler(http_handler handler) -> void
    {
        not_found_handler_ = std::move(handler);
    }

    auto http_server::set_error_handler(http_handler handler) -> void
    {
        error_handler_ = std::move(handler);
    }

    auto http_server::register_route(internal::http_method method,
                                      const std::string& pattern,
                                      http_handler handler) -> void
    {
        http_route route;
        route.method = method;
        route.pattern = pattern;
        route.handler = std::move(handler);

        // Convert pattern to regex
        auto regex_str = pattern_to_regex(pattern, route.param_names);
        route.regex_pattern = std::regex(regex_str);

        std::lock_guard<std::mutex> lock(routes_mutex_);
        routes_.push_back(std::move(route));
    }

    auto http_server::find_route(internal::http_method method,
                                  const std::string& path,
                                  std::map<std::string, std::string>& path_params) const
        -> const http_route*
    {
        // Note: routes_mutex_ is mutable, so we can lock it in const methods
        // But for now, we'll const_cast to avoid making mutex mutable
        auto& mutable_mutex = const_cast<std::mutex&>(routes_mutex_);
        std::lock_guard<std::mutex> lock(mutable_mutex);

        for (const auto& route : routes_)
        {
            if (route.matches(method, path, path_params))
            {
                return &route;
            }
        }

        return nullptr;
    }

    auto http_server::handle_request(const std::vector<uint8_t>& request_data) -> std::vector<uint8_t>
    {
        // Parse HTTP request
        auto request_result = internal::http_parser::parse_request(request_data);
        if (request_result.is_err())
        {
            // Failed to parse request - send 400 Bad Request
            auto error_response = create_error_response(400, "Bad Request");
            return internal::http_parser::serialize_response(error_response);
        }

        auto http_request = std::move(request_result.value());

        // Create request context
        http_request_context ctx;
        ctx.request = std::move(http_request);

        internal::http_response response;

        try
        {
            // Find matching route
            auto route = find_route(ctx.request.method, ctx.request.uri, ctx.path_params);

            if (route)
            {
                // Execute handler
                response = route->handler(ctx);
            }
            else
            {
                // No matching route - send 404
                response = not_found_handler_(ctx);
            }
        }
        catch (const std::exception& e)
        {
            // Handler threw exception - send 500
            response = error_handler_(ctx);
        }
        catch (...)
        {
            // Unknown exception - send 500
            response = error_handler_(ctx);
        }

        // Add standard headers if not present
        if (!response.get_header("Content-Length"))
        {
            response.set_header("Content-Length", std::to_string(response.body.size()));
        }

        if (!response.get_header("Server"))
        {
            response.set_header("Server", "NetworkSystem-HTTP-Server/1.0");
        }

        if (!response.get_header("Connection"))
        {
            response.set_header("Connection", "close");
        }

        // Serialize and return response
        return internal::http_parser::serialize_response(response);
    }

    auto http_server::create_error_response(int status_code, const std::string& message)
        -> internal::http_response
    {
        internal::http_response response;
        response.status_code = status_code;
        response.status_message = internal::get_status_message(status_code);

        std::ostringstream html;
        html << "<!DOCTYPE html>\n";
        html << "<html>\n";
        html << "<head><title>" << status_code << " " << response.status_message << "</title></head>\n";
        html << "<body>\n";
        html << "<h1>" << status_code << " " << response.status_message << "</h1>\n";
        html << "<p>" << message << "</p>\n";
        html << "<hr>\n";
        html << "<p><em>NetworkSystem HTTP Server</em></p>\n";
        html << "</body>\n";
        html << "</html>\n";

        response.set_body_string(html.str());
        response.set_header("Content-Type", "text/html; charset=utf-8");

        return response;
    }

    auto http_server::pattern_to_regex(const std::string& pattern,
                                        std::vector<std::string>& param_names) -> std::string
    {
        param_names.clear();

        std::ostringstream regex_str;
        regex_str << "^";

        std::size_t pos = 0;
        while (pos < pattern.length())
        {
            // Look for parameter placeholder (:param_name)
            auto param_start = pattern.find(':', pos);

            if (param_start == std::string::npos)
            {
                // No more parameters, add remaining literal part
                auto remaining = pattern.substr(pos);
                // Escape special regex characters
                for (char c : remaining)
                {
                    if (c == '.' || c == '*' || c == '+' || c == '?' ||
                        c == '[' || c == ']' || c == '(' || c == ')' ||
                        c == '{' || c == '}' || c == '^' || c == '$' ||
                        c == '|' || c == '\\')
                    {
                        regex_str << '\\';
                    }
                    regex_str << c;
                }
                break;
            }

            // Add literal part before parameter
            for (std::size_t i = pos; i < param_start; ++i)
            {
                char c = pattern[i];
                if (c == '.' || c == '*' || c == '+' || c == '?' ||
                    c == '[' || c == ']' || c == '(' || c == ')' ||
                    c == '{' || c == '}' || c == '^' || c == '$' ||
                    c == '|' || c == '\\')
                {
                    regex_str << '\\';
                }
                regex_str << c;
            }

            // Find end of parameter name
            auto param_end = param_start + 1;
            while (param_end < pattern.length() &&
                   (std::isalnum(pattern[param_end]) || pattern[param_end] == '_'))
            {
                ++param_end;
            }

            // Extract parameter name
            auto param_name = pattern.substr(param_start + 1, param_end - param_start - 1);
            param_names.push_back(param_name);

            // Add regex capture group for parameter (matches anything except /)
            regex_str << "([^/]+)";

            pos = param_end;
        }

        regex_str << "$";

        return regex_str.str();
    }

} // namespace network_system::core
