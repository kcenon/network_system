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

/**
 * @file udp_example.cpp
 * @brief Simple example demonstrating UDP client and server usage
 *
 * This example shows:
 * 1. Creating a UDP server that echoes received messages
 * 2. Creating a UDP client that sends messages and receives responses
 * 3. Basic error handling
 * 4. Graceful shutdown
 */

#include "network_system/core/messaging_udp_server.h"
#include "network_system/core/messaging_udp_client.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>

using namespace network_system::core;

// Shared flag for graceful shutdown
std::atomic<bool> should_stop{false};

/**
 * @brief Run UDP echo server
 *
 * The server receives datagrams and echoes them back to the sender.
 */
void run_server()
{
    std::cout << "[Server] Starting UDP echo server...\n";

    auto server = std::make_shared<messaging_udp_server>("EchoServer");

    // Set up receive callback to echo messages back
    server->set_receive_callback(
        [server](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender)
        {
            std::string message(data.begin(), data.end());
            std::cout << "[Server] Received: \"" << message << "\" from "
                      << sender.address().to_string() << ":" << sender.port() << "\n";

            // Echo back
            std::string response = "Echo: " + message;
            std::vector<uint8_t> response_data(response.begin(), response.end());

            server->async_send_to(
                std::move(response_data),
                sender,
                [](std::error_code ec, std::size_t bytes)
                {
                    if (!ec)
                    {
                        std::cout << "[Server] Sent echo response: " << bytes << " bytes\n";
                    }
                    else
                    {
                        std::cerr << "[Server] Send error: " << ec.message() << "\n";
                    }
                });
        });

    // Set up error callback
    server->set_error_callback(
        [](std::error_code ec)
        {
            std::cerr << "[Server] Error: " << ec.message() << "\n";
        });

    // Start server on port 5555
    auto result = server->start_server(5555);
    if (result.is_err())
    {
        std::cerr << "[Server] Failed to start: " << result.error().message << "\n";
        return;
    }

    std::cout << "[Server] Running on port 5555. Press Ctrl+C to stop.\n";

    // Wait until signaled to stop
    while (!should_stop.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[Server] Stopping...\n";
    server->stop_server();
    std::cout << "[Server] Stopped.\n";
}

/**
 * @brief Run UDP client
 *
 * The client sends messages to the server and prints responses.
 */
void run_client()
{
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "[Client] Starting UDP client...\n";

    auto client = std::make_shared<messaging_udp_client>("TestClient");

    // Set up receive callback to handle echo responses
    client->set_receive_callback(
        [](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender)
        {
            std::string message(data.begin(), data.end());
            std::cout << "[Client] Received response: \"" << message << "\"\n";
        });

    // Set up error callback
    client->set_error_callback(
        [](std::error_code ec)
        {
            std::cerr << "[Client] Error: " << ec.message() << "\n";
        });

    // Start client targeting localhost:5555
    auto result = client->start_client("localhost", 5555);
    if (result.is_err())
    {
        std::cerr << "[Client] Failed to start: " << result.error().message << "\n";
        return;
    }

    std::cout << "[Client] Connected to localhost:5555\n";

    // Send a few test messages
    const std::vector<std::string> messages = {
        "Hello, UDP!",
        "This is a test message",
        "UDP is fast!",
        "Final message"
    };

    for (const auto& msg : messages)
    {
        std::vector<uint8_t> data(msg.begin(), msg.end());

        std::cout << "[Client] Sending: \"" << msg << "\"\n";

        auto send_result = client->send_packet(
            std::move(data),
            [](std::error_code ec, std::size_t bytes)
            {
                if (!ec)
                {
                    std::cout << "[Client] Sent " << bytes << " bytes\n";
                }
                else
                {
                    std::cerr << "[Client] Send error: " << ec.message() << "\n";
                }
            });

        if (send_result.is_err())
        {
            std::cerr << "[Client] Send failed: " << send_result.error().message << "\n";
        }

        // Wait between messages
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Wait for responses
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "[Client] Stopping...\n";
    client->stop_client();
    std::cout << "[Client] Stopped.\n";

    // Signal server to stop
    should_stop.store(true);
}

int main()
{
    std::cout << "=== UDP Example ===\n";
    std::cout << "This example demonstrates basic UDP client/server communication.\n\n";

    try
    {
        // Start server in separate thread
        std::thread server_thread(run_server);

        // Start client in main thread
        run_client();

        // Wait for server to finish
        if (server_thread.joinable())
        {
            server_thread.join();
        }

        std::cout << "\n=== Example completed successfully ===\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
