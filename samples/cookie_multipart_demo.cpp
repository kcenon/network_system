/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "network_system/core/http_server.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace network_system::core;
using namespace network_system::internal;

int main()
{
    std::cout << "=== Cookie and Multipart Demo ===\n" << std::endl;

    // Create HTTP server
    auto server = std::make_shared<http_server>("cookie_multipart_demo");

    // Cookie demo endpoint
    server->get("/cookie-test", [](const http_request_context& ctx)
    {
        http_response response;
        response.status_code = 200;

        std::string body = "<!DOCTYPE html><html><body>\n";
        body += "<h2>Cookie Test</h2>\n";

        // Display received cookies
        if (!ctx.request.cookies.empty())
        {
            body += "<h3>Received Cookies:</h3><ul>\n";
            for (const auto& [name, value] : ctx.request.cookies)
            {
                body += "<li><b>" + name + "</b>: " + value + "</li>\n";
            }
            body += "</ul>\n";
        }
        else
        {
            body += "<p>No cookies received</p>\n";
        }

        body += "<h3>Set New Cookies:</h3>\n";
        body += "<a href='/set-cookie'>Click to set cookies</a><br>\n";
        body += "<a href='/'>Back to home</a>\n";
        body += "</body></html>";

        response.set_body_string(body);
        response.set_header("Content-Type", "text/html");

        return response;
    });

    // Set cookie endpoint
    server->get("/set-cookie", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        // Set multiple cookies with different attributes
        response.set_cookie("session_id", "abc123", "/", 3600);  // 1 hour

        cookie user_pref;
        user_pref.name = "theme";
        user_pref.value = "dark";
        user_pref.path = "/";
        user_pref.max_age = 86400;  // 24 hours
        user_pref.http_only = true;
        user_pref.same_site = "Strict";
        response.set_cookie(user_pref);

        std::string body = "<!DOCTYPE html><html><body>\n";
        body += "<h2>Cookies Set!</h2>\n";
        body += "<p>Two cookies have been set:</p>\n";
        body += "<ul>\n";
        body += "<li>session_id=abc123 (Max-Age: 3600)</li>\n";
        body += "<li>theme=dark (Max-Age: 86400, HttpOnly, SameSite=Strict)</li>\n";
        body += "</ul>\n";
        body += "<a href='/cookie-test'>View cookies</a><br>\n";
        body += "<a href='/'>Back to home</a>\n";
        body += "</body></html>";

        response.set_body_string(body);
        response.set_header("Content-Type", "text/html");

        return response;
    });

    // File upload form
    server->get("/upload", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        std::string body = R"(<!DOCTYPE html>
<html>
<head><title>File Upload Demo</title></head>
<body>
<h2>File Upload Demo</h2>
<form action="/upload" method="POST" enctype="multipart/form-data">
    <label>Name: <input type="text" name="username"></label><br><br>
    <label>Email: <input type="email" name="email"></label><br><br>
    <label>File: <input type="file" name="uploaded_file"></label><br><br>
    <label>Description: <textarea name="description" rows="4" cols="40"></textarea></label><br><br>
    <button type="submit">Upload</button>
</form>
<br><a href="/">Back to home</a>
</body>
</html>)";

        response.set_body_string(body);
        response.set_header("Content-Type", "text/html");

        return response;
    });

    // Handle file upload
    server->post("/upload", [](const http_request_context& ctx)
    {
        http_response response;
        response.status_code = 200;

        std::ostringstream body;
        body << "<!DOCTYPE html><html><body>\n";
        body << "<h2>Upload Result</h2>\n";

        // Display form fields
        if (!ctx.request.form_data.empty())
        {
            body << "<h3>Form Fields:</h3><ul>\n";
            for (const auto& [name, value] : ctx.request.form_data)
            {
                body << "<li><b>" << name << "</b>: " << value << "</li>\n";
            }
            body << "</ul>\n";
        }

        // Display uploaded files
        if (!ctx.request.files.empty())
        {
            body << "<h3>Uploaded Files:</h3><ul>\n";
            for (const auto& [field_name, file] : ctx.request.files)
            {
                body << "<li>\n";
                body << "  <b>Field:</b> " << field_name << "<br>\n";
                body << "  <b>Filename:</b> " << file.filename << "<br>\n";
                body << "  <b>Content-Type:</b> " << file.content_type << "<br>\n";
                body << "  <b>Size:</b> " << file.content.size() << " bytes<br>\n";

                // Show first 100 bytes as preview if it's text
                if (file.content_type.find("text/") == 0 && file.content.size() > 0)
                {
                    std::string preview(file.content.begin(),
                                       file.content.begin() + std::min(size_t(100), file.content.size()));
                    body << "  <b>Preview:</b> <pre>" << preview << "</pre>\n";
                }
                body << "</li>\n";
            }
            body << "</ul>\n";
        }
        else
        {
            body << "<p>No files uploaded</p>\n";
        }

        body << "<br><a href='/upload'>Upload another file</a><br>\n";
        body << "<a href='/'>Back to home</a>\n";
        body << "</body></html>";

        response.set_body_string(body.str());
        response.set_header("Content-Type", "text/html");

        return response;
    });

    // Home page
    server->get("/", [](const http_request_context&)
    {
        http_response response;
        response.status_code = 200;

        std::string body = R"(<!DOCTYPE html>
<html>
<head><title>Cookie & Multipart Demo</title></head>
<body>
<h1>HTTP Features Demo</h1>

<h2>Cookie Management</h2>
<ul>
    <li><a href="/cookie-test">Test Cookies</a> - View and test cookie handling</li>
    <li><a href="/set-cookie">Set Cookies</a> - Set example cookies</li>
</ul>

<h2>File Upload (multipart/form-data)</h2>
<ul>
    <li><a href="/upload">Upload File</a> - Test file upload with form data</li>
</ul>

<h3>Test with curl:</h3>
<pre>
# Cookie test
curl -v -b "session=test123; user=john" http://localhost:8080/cookie-test

# File upload test
curl -F "username=john" -F "email=john@example.com" \
     -F "uploaded_file=@/path/to/file.txt" \
     -F "description=Test file" \
     http://localhost:8080/upload
</pre>
</body>
</html>)";

        response.set_body_string(body);
        response.set_header("Content-Type", "text/html");

        return response;
    });

    // Start server on port 8080
    std::cout << "Starting HTTP server on port 8080..." << std::endl;
    auto result = server->start(8080);
    if (result.is_err())
    {
        std::cerr << "Failed to start server: " << result.error().message << std::endl;
        return 1;
    }

    std::cout << "Server started successfully!" << std::endl;
    std::cout << "Visit http://localhost:8080/ for the demo" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;

    // Wait for server to stop
    server->wait_for_stop();

    return 0;
}
