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
#include "network_system/integration/logger_integration.h"
#include "network_system/utils/compression_pipeline.h"
#include <sstream>
#include <algorithm>

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
            handle_request_data(session, data);
        });

        // Note: We don't use disconnection_callback because it passes server_id (shared by all sessions)
        // not a unique session ID. Since HTTP uses Connection: close, sessions are short-lived and
        // buffers are automatically cleaned up when the session shared_ptr is destroyed after response.
        // TODO: Consider using weak_ptr or periodic cleanup for long-lived connections

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

    auto http_server::handle_request_data(std::shared_ptr<network_system::session::messaging_session> session,
                                          const std::vector<uint8_t>& chunk) -> void
    {
        if (!session)
        {
            return;
        }

        bool request_complete = false;
        std::vector<uint8_t> complete_request_data;

        // Scope for lock - we'll release it before calling send_packet
        {
            std::lock_guard<std::mutex> lock(buffers_mutex_);

            // Get or create buffer for this session
            auto& buffer = request_buffers_[session];

        // Append new data to buffer
        buffer.data.insert(buffer.data.end(), chunk.begin(), chunk.end());

        // Debug logging
        NETWORK_LOG_INFO("[http_server] Received " + std::to_string(chunk.size()) +
                        " bytes, total buffer size: " + std::to_string(buffer.data.size()));

        // Check if we've exceeded maximum request size
        if (buffer.data.size() > http_request_buffer::MAX_REQUEST_SIZE)
        {
            request_buffers_.erase(session);
            send_error_response(session, 413, "Request Entity Too Large");
            return;
        }

        // If headers not yet complete, look for end of headers marker
        if (!buffer.headers_complete)
        {
            // Look for "\r\n\r\n" marker
            const char* marker = "\r\n\r\n";
            auto it = std::search(buffer.data.begin(), buffer.data.end(),
                                marker, marker + 4);

            if (it != buffer.data.end())
            {
                buffer.headers_complete = true;
                buffer.headers_end_pos = std::distance(buffer.data.begin(), it) + 4;

                NETWORK_LOG_INFO("[http_server] Headers complete, headers_end_pos: " +
                               std::to_string(buffer.headers_end_pos));

                // Check if headers are too large
                if (buffer.headers_end_pos > http_request_buffer::MAX_HEADER_SIZE)
                {
                    request_buffers_.erase(session);
                    send_error_response(session, 431, "Request Header Fields Too Large");
                    return;
                }

                // Parse Content-Length from headers
                buffer.content_length = parse_content_length(buffer.data, buffer.headers_end_pos);

                NETWORK_LOG_INFO("[http_server] Parsed Content-Length: " +
                               std::to_string(buffer.content_length));

                // Validate Content-Length
                if (buffer.content_length > http_request_buffer::MAX_REQUEST_SIZE)
                {
                    request_buffers_.erase(session);
                    send_error_response(session, 413, "Request Entity Too Large");
                    return;
                }
            }
            else
            {
                // Headers not yet complete, check if we've exceeded header size limit
                if (buffer.data.size() > http_request_buffer::MAX_HEADER_SIZE)
                {
                    request_buffers_.erase(session);
                    send_error_response(session, 431, "Request Header Fields Too Large");
                    return;
                }
                // Wait for more data
                return;
            }
        }

            // Check if request is complete
            if (is_request_complete(buffer))
            {
                NETWORK_LOG_INFO("[http_server] Request complete, processing...");

                // Copy request data before erasing buffer
                complete_request_data = buffer.data;
                request_complete = true;

                // Clean up buffer
                request_buffers_.erase(session);
            }
            else
            {
                NETWORK_LOG_INFO("[http_server] Request incomplete, waiting for more data. " +
                               std::string("Expected: ") + std::to_string(buffer.headers_end_pos + buffer.content_length) +
                               ", Got: " + std::to_string(buffer.data.size()));
            }
        } // Lock released here

        // Process and send response outside the lock
        if (request_complete && session)
        {
            // Process complete request
            auto response_data = process_complete_request(complete_request_data);

            NETWORK_LOG_INFO("[http_server] Response generated, size: " +
                           std::to_string(response_data.size()) + " bytes");

            // Log first 200 bytes of response for debugging
            std::string response_preview(response_data.begin(),
                                        response_data.begin() + std::min(size_t(200), response_data.size()));
            NETWORK_LOG_INFO("[http_server] Response preview: " + response_preview);

            // Send response synchronously (no lock held)
            auto send_ec = session->send_packet_sync(std::move(response_data));
            if (send_ec)
            {
                NETWORK_LOG_ERROR("[http_server] Failed to send response: " + send_ec.message());
            }
            else
            {
                NETWORK_LOG_INFO("[http_server] Response sent successfully");
            }

            // For HTTP with Connection: close, close the session after sending
            // No sleep needed since send_packet_sync is synchronous
            session->stop_session();
            NETWORK_LOG_INFO("[http_server] Session stopped after response");
        }
    }

    auto http_server::process_complete_request(const std::vector<uint8_t>& request_data) -> std::vector<uint8_t>
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

        // Check Accept-Encoding header and compress response if requested
        constexpr size_t COMPRESSION_THRESHOLD = 1024; // 1KB - compress responses larger than this
        auto accept_encoding = ctx.request.get_header("Accept-Encoding");

        if (accept_encoding && response.body.size() > COMPRESSION_THRESHOLD)
        {
            std::string encoding = *accept_encoding;
            std::transform(encoding.begin(), encoding.end(), encoding.begin(), ::tolower);

            utils::compression_algorithm algo = utils::compression_algorithm::none;
            std::string encoding_name;

            // Check for gzip (preferred)
            if (encoding.find("gzip") != std::string::npos)
            {
                algo = utils::compression_algorithm::gzip;
                encoding_name = "gzip";
            }
            // Check for deflate
            else if (encoding.find("deflate") != std::string::npos)
            {
                algo = utils::compression_algorithm::deflate;
                encoding_name = "deflate";
            }

            // Apply compression if algorithm was selected
            if (algo != utils::compression_algorithm::none)
            {
                auto compressor = std::make_shared<utils::compression_pipeline>(algo, 0);
                auto compressed_result = compressor->compress(response.body);

                if (!compressed_result.is_err())
                {
                    auto compressed = compressed_result.value();
                    // Only use compression if it actually reduces size
                    if (compressed.size() < response.body.size())
                    {
                        response.body = std::move(compressed);
                        response.set_header("Content-Encoding", encoding_name);
                        NETWORK_LOG_DEBUG("[http_server] Compressed response with " + encoding_name);
                    }
                }
            }
        }

        // Enable chunked encoding for large responses (> 8KB)
        constexpr size_t CHUNKED_THRESHOLD = 8 * 1024; // 8KB
        if (response.body.size() > CHUNKED_THRESHOLD && response.version == internal::http_version::HTTP_1_1)
        {
            response.use_chunked_encoding = true;
        }

        // Add standard headers if not present
        if (response.use_chunked_encoding)
        {
            // Chunked encoding: use Transfer-Encoding header instead of Content-Length
            response.set_header("Transfer-Encoding", "chunked");
        }
        else
        {
            // Normal response: use Content-Length
            if (!response.get_header("Content-Length"))
            {
                response.set_header("Content-Length", std::to_string(response.body.size()));
            }
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

    auto http_server::parse_content_length(const std::vector<uint8_t>& data, std::size_t headers_end_pos) -> std::size_t
    {
        // Convert to string view for parsing
        std::string_view headers_str(reinterpret_cast<const char*>(data.data()), headers_end_pos);

        // Find "Content-Length:" header (case-insensitive)
        const char* content_length_header = "content-length:";
        std::size_t pos = 0;

        while (pos < headers_str.length())
        {
            auto line_end = headers_str.find("\r\n", pos);
            if (line_end == std::string_view::npos)
            {
                break;
            }

            auto line = headers_str.substr(pos, line_end - pos);

            // Check if this line starts with "Content-Length:" (case-insensitive)
            if (line.length() > 15) // "content-length:" is 15 chars
            {
                std::string line_lower;
                line_lower.reserve(15);
                for (std::size_t i = 0; i < 15 && i < line.length(); ++i)
                {
                    line_lower += static_cast<char>(std::tolower(static_cast<unsigned char>(line[i])));
                }

                if (line_lower == content_length_header)
                {
                    // Extract value after colon
                    auto value_start = line.find(':', 0);
                    if (value_start != std::string_view::npos)
                    {
                        auto value = line.substr(value_start + 1);

                        // Trim whitespace
                        auto first = value.find_first_not_of(" \t");
                        auto last = value.find_last_not_of(" \t");

                        if (first != std::string_view::npos && last != std::string_view::npos)
                        {
                            value = value.substr(first, last - first + 1);

                            // Security: Reject negative values explicitly
                            if (!value.empty() && value[0] == '-')
                            {
                                NETWORK_LOG_INFO("[http_server] Rejected negative Content-Length: " + std::string(value));
                                return 0;
                            }

                            try
                            {
                                // Use std::stoull for unsigned long long to handle large values
                                auto length = std::stoull(std::string(value));

                                // Check for overflow
                                if (length > std::numeric_limits<std::size_t>::max())
                                {
                                    NETWORK_LOG_INFO("[http_server] Content-Length overflow detected");
                                    return 0;
                                }

                                return static_cast<std::size_t>(length);
                            }
                            catch (...)
                            {
                                // Invalid Content-Length value
                                NETWORK_LOG_INFO("[http_server] Invalid Content-Length value: " + std::string(value));
                                return 0;
                            }
                        }
                    }
                }
            }

            pos = line_end + 2; // Move past "\r\n"
        }

        return 0; // Content-Length not found
    }

    auto http_server::is_request_complete(const http_request_buffer& buffer) const -> bool
    {
        if (!buffer.headers_complete)
        {
            return false;
        }

        // Calculate expected total size
        std::size_t expected_size = buffer.headers_end_pos + buffer.content_length;

        // Check if we have all the data
        return buffer.data.size() >= expected_size;
    }

    auto http_server::send_error_response(std::shared_ptr<network_system::session::messaging_session> session,
                                         int status_code,
                                         const std::string& message) -> void
    {
        auto error_response = create_error_response(status_code, message);
        auto response_data = internal::http_parser::serialize_response(error_response);

        if (session)
        {
            session->send_packet(std::move(response_data));
        }
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
