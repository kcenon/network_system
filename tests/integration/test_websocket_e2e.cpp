// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

/**
 * @file test_websocket_e2e.cpp
 * @brief WebSocket End-to-End integration tests
 *
 * NET-201: WebSocket E2E Integration Tests
 *
 * Tests include:
 * - Handshake (client/server)
 * - Text/Binary message exchange
 * - Ping/Pong control frames
 * - Close handshake
 * - Fragmented messages
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "kcenon/network/internal/tcp_socket.h"
#include "kcenon/network/internal/websocket_socket.h"

using namespace network_system::internal;
using namespace std::chrono_literals;

// Free function for yielding to allow async operations to complete
inline void wait_for_ready() {
    for (int i = 0; i < 100; ++i) {
        std::this_thread::yield();
    }
}


namespace
{

// Test configuration
constexpr uint16_t BASE_TEST_PORT = 9300;
std::atomic<uint16_t> g_port_counter{0};

uint16_t get_test_port()
{
    return BASE_TEST_PORT + g_port_counter++;
}

// Helper class to manage test server
class TestWebSocketServer
{
public:
    TestWebSocketServer(uint16_t port) : port_(port), running_(false) {}

    ~TestWebSocketServer()
    {
        stop();
    }

    bool start()
    {
        try
        {
            io_context_ = std::make_unique<asio::io_context>();
            acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
                *io_context_,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_));

            running_ = true;
            start_accept();

            io_thread_ = std::thread([this]() {
                io_context_->run();
            });

            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Server start failed: " << e.what() << std::endl;
            return false;
        }
    }

    void stop()
    {
        running_ = false;
        if (io_context_)
        {
            io_context_->stop();
        }
        if (io_thread_.joinable())
        {
            io_thread_.join();
        }
    }

    void set_message_handler(std::function<void(const ws_message&)> handler)
    {
        message_handler_ = std::move(handler);
    }

    void set_on_client_connected(std::function<void(std::shared_ptr<websocket_socket>)> handler)
    {
        on_client_connected_ = std::move(handler);
    }

    std::shared_ptr<websocket_socket> get_client_socket()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, 5s, [this]() { return client_socket_ != nullptr; });
        return client_socket_;
    }

private:
    void start_accept()
    {
        auto tcp_socket = std::make_shared<asio::ip::tcp::socket>(*io_context_);

        acceptor_->async_accept(*tcp_socket, [this, tcp_socket](std::error_code ec) {
            if (!ec && running_)
            {
                handle_accept(tcp_socket);
                start_accept();
            }
        });
    }

    void handle_accept(std::shared_ptr<asio::ip::tcp::socket> raw_socket)
    {
        auto socket = std::make_shared<tcp_socket>(std::move(*raw_socket));
        auto ws = std::make_shared<websocket_socket>(socket, false /* is_client */);

        ws->async_accept([this, ws](std::error_code ec) {
            if (!ec)
            {
                ws->set_message_callback([this](const ws_message& msg) {
                    if (message_handler_)
                    {
                        message_handler_(msg);
                    }
                });

                ws->start_read();

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    client_socket_ = ws;
                }
                cv_.notify_all();

                if (on_client_connected_)
                {
                    on_client_connected_(ws);
                }
            }
        });
    }

    uint16_t port_;
    std::atomic<bool> running_;
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::thread io_thread_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::shared_ptr<websocket_socket> client_socket_;

    std::function<void(const ws_message&)> message_handler_;
    std::function<void(std::shared_ptr<websocket_socket>)> on_client_connected_;
};

// Helper class to manage test client
class TestWebSocketClient
{
public:
    TestWebSocketClient() : connected_(false) {}

    ~TestWebSocketClient()
    {
        disconnect();
    }

    bool connect(const std::string& host, uint16_t port, const std::string& path = "/")
    {
        try
        {
            io_context_ = std::make_unique<asio::io_context>();

            asio::ip::tcp::resolver resolver(*io_context_);
            auto endpoints = resolver.resolve(host, std::to_string(port));

            auto raw_socket = std::make_shared<asio::ip::tcp::socket>(*io_context_);
            asio::connect(*raw_socket, endpoints);

            socket_ = std::make_shared<tcp_socket>(std::move(*raw_socket));
            ws_ = std::make_shared<websocket_socket>(socket_, true /* is_client */);

            // Create work guard to prevent io_context from returning early
            work_guard_ = std::make_unique<work_guard_type>(asio::make_work_guard(*io_context_));

            io_thread_ = std::thread([this]() {
                io_context_->run();
            });

            std::promise<bool> handshake_promise;
            auto handshake_future = handshake_promise.get_future();

            ws_->async_handshake(host, path, port, [this, &handshake_promise](std::error_code ec) {
                if (!ec)
                {
                    connected_ = true;
                    ws_->start_read();
                    handshake_promise.set_value(true);
                }
                else
                {
                    handshake_promise.set_value(false);
                }
            });

            auto status = handshake_future.wait_for(5s);
            return status == std::future_status::ready && handshake_future.get();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Client connect failed: " << e.what() << std::endl;
            return false;
        }
    }

    void disconnect()
    {
        connected_ = false;
        // Reset work guard to allow io_context to exit
        work_guard_.reset();
        if (io_context_)
        {
            io_context_->stop();
        }
        if (io_thread_.joinable())
        {
            io_thread_.join();
        }
    }

    bool is_connected() const
    {
        return connected_ && ws_ && ws_->is_open();
    }

    void set_message_handler(std::function<void(const ws_message&)> handler)
    {
        if (ws_)
        {
            ws_->set_message_callback(std::move(handler));
        }
    }

    void set_pong_handler(std::function<void(const std::vector<uint8_t>&)> handler)
    {
        if (ws_)
        {
            ws_->set_pong_callback(std::move(handler));
        }
    }

    void set_close_handler(std::function<void(ws_close_code, const std::string&)> handler)
    {
        if (ws_)
        {
            ws_->set_close_callback(std::move(handler));
        }
    }

    bool send_text(const std::string& message)
    {
        if (!ws_ || !ws_->is_open())
        {
            return false;
        }

        std::promise<bool> send_promise;
        auto send_future = send_promise.get_future();

        auto result = ws_->async_send_text(std::string(message), [&send_promise](std::error_code ec, std::size_t) {
            send_promise.set_value(!ec);
        });

        if (!result.is_ok())
        {
            return false;
        }

        auto status = send_future.wait_for(5s);
        return status == std::future_status::ready && send_future.get();
    }

    bool send_binary(const std::vector<uint8_t>& data)
    {
        if (!ws_ || !ws_->is_open())
        {
            return false;
        }

        std::promise<bool> send_promise;
        auto send_future = send_promise.get_future();

        auto data_copy = data;
        auto result = ws_->async_send_binary(std::move(data_copy), [&send_promise](std::error_code ec, std::size_t) {
            send_promise.set_value(!ec);
        });

        if (!result.is_ok())
        {
            return false;
        }

        auto status = send_future.wait_for(5s);
        return status == std::future_status::ready && send_future.get();
    }

    bool send_ping(const std::vector<uint8_t>& payload = {})
    {
        if (!ws_ || !ws_->is_open())
        {
            return false;
        }

        std::promise<bool> send_promise;
        auto send_future = send_promise.get_future();

        ws_->async_send_ping(payload, [&send_promise](std::error_code ec) {
            send_promise.set_value(!ec);
        });

        auto status = send_future.wait_for(5s);
        return status == std::future_status::ready && send_future.get();
    }

    bool close(ws_close_code code = ws_close_code::normal, const std::string& reason = "")
    {
        if (!ws_)
        {
            return false;
        }

        std::promise<bool> close_promise;
        auto close_future = close_promise.get_future();

        ws_->async_close(code, reason, [&close_promise](std::error_code ec) {
            close_promise.set_value(!ec);
        });

        auto status = close_future.wait_for(5s);
        return status == std::future_status::ready && close_future.get();
    }

    std::shared_ptr<websocket_socket> socket() { return ws_; }

private:
    using work_guard_type = asio::executor_work_guard<asio::io_context::executor_type>;

    std::atomic<bool> connected_;
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<work_guard_type> work_guard_;
    std::shared_ptr<tcp_socket> socket_;
    std::shared_ptr<websocket_socket> ws_;
    std::thread io_thread_;
};

}  // namespace

// Test Fixture
class WebSocketE2ETest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        port_ = get_test_port();
    }

    void TearDown() override
    {
        // Allow cleanup
        wait_for_ready();
    }

    uint16_t port_;
};

// ============================================================================
// Handshake Tests
// ============================================================================

TEST_F(WebSocketE2ETest, HandshakeSuccess)
{
    TestWebSocketServer server(port_);
    ASSERT_TRUE(server.start());

    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));
    EXPECT_TRUE(client.is_connected());
}

TEST_F(WebSocketE2ETest, HandshakeWithPath)
{
    TestWebSocketServer server(port_);
    ASSERT_TRUE(server.start());

    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_, "/chat"));
    EXPECT_TRUE(client.is_connected());
}

TEST_F(WebSocketE2ETest, ServerAcceptsClient)
{
    TestWebSocketServer server(port_);

    std::atomic<bool> client_connected{false};
    server.set_on_client_connected([&client_connected](auto) {
        client_connected = true;
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    wait_for_ready();
    EXPECT_TRUE(client_connected);
}

// ============================================================================
// Text Message Tests
// ============================================================================

TEST_F(WebSocketE2ETest, TextMessageRoundTrip)
{
    TestWebSocketServer server(port_);

    std::string received_message;
    std::mutex mutex;
    std::condition_variable cv;

    server.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::text)
        {
            received_message = std::string(msg.data.begin(), msg.data.end());
        }
        cv.notify_all();
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    ASSERT_TRUE(client.send_text("Hello, WebSocket!"));

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&]() { return !received_message.empty(); }));

    EXPECT_EQ(received_message, "Hello, WebSocket!");
}

TEST_F(WebSocketE2ETest, LargeTextMessage)
{
    TestWebSocketServer server(port_);

    std::string received_message;
    std::mutex mutex;
    std::condition_variable cv;

    server.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::text)
        {
            received_message = std::string(msg.data.begin(), msg.data.end());
        }
        cv.notify_all();
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    // Send 10KB message
    std::string large_message(10 * 1024, 'X');
    ASSERT_TRUE(client.send_text(large_message));

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]() { return received_message.size() == large_message.size(); }));

    EXPECT_EQ(received_message, large_message);
}

TEST_F(WebSocketE2ETest, MultipleTextMessages)
{
    TestWebSocketServer server(port_);

    std::vector<std::string> received_messages;
    std::mutex mutex;
    std::condition_variable cv;

    server.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::text)
        {
            received_messages.push_back(std::string(msg.data.begin(), msg.data.end()));
        }
        cv.notify_all();
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    const int num_messages = 10;
    for (int i = 0; i < num_messages; ++i)
    {
        ASSERT_TRUE(client.send_text("Message " + std::to_string(i)));
    }

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]() { return received_messages.size() >= num_messages; }));

    EXPECT_EQ(received_messages.size(), num_messages);
}

// ============================================================================
// Binary Message Tests
// ============================================================================

TEST_F(WebSocketE2ETest, BinaryMessageRoundTrip)
{
    TestWebSocketServer server(port_);

    std::vector<uint8_t> received_data;
    std::mutex mutex;
    std::condition_variable cv;

    server.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::binary)
        {
            received_data = msg.data;
        }
        cv.notify_all();
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
    ASSERT_TRUE(client.send_binary(binary_data));

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&]() { return !received_data.empty(); }));

    EXPECT_EQ(received_data, binary_data);
}

TEST_F(WebSocketE2ETest, LargeBinaryMessage)
{
    TestWebSocketServer server(port_);

    std::vector<uint8_t> received_data;
    std::mutex mutex;
    std::condition_variable cv;

    server.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::binary)
        {
            received_data = msg.data;
        }
        cv.notify_all();
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    // Send 64KB binary data
    std::vector<uint8_t> large_data(64 * 1024);
    for (size_t i = 0; i < large_data.size(); ++i)
    {
        large_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    ASSERT_TRUE(client.send_binary(large_data));

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 5s, [&]() { return received_data.size() == large_data.size(); }));

    EXPECT_EQ(received_data, large_data);
}

// ============================================================================
// Ping/Pong Tests
// ============================================================================

TEST_F(WebSocketE2ETest, PingPongExchange)
{
    TestWebSocketServer server(port_);
    ASSERT_TRUE(server.start());

    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    std::vector<uint8_t> received_pong_payload;
    std::mutex mutex;
    std::condition_variable cv;

    client.set_pong_handler([&](const std::vector<uint8_t>& payload) {
        std::lock_guard<std::mutex> lock(mutex);
        received_pong_payload = payload;
        cv.notify_all();
    });

    std::vector<uint8_t> ping_payload = {'p', 'i', 'n', 'g'};
    ASSERT_TRUE(client.send_ping(ping_payload));

    std::unique_lock<std::mutex> lock(mutex);
    bool received = cv.wait_for(lock, 2s, [&]() { return !received_pong_payload.empty(); });

    EXPECT_TRUE(received);
    EXPECT_EQ(received_pong_payload, ping_payload);
}

TEST_F(WebSocketE2ETest, EmptyPing)
{
    TestWebSocketServer server(port_);
    ASSERT_TRUE(server.start());

    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    std::atomic<bool> pong_received{false};
    std::mutex mutex;
    std::condition_variable cv;

    client.set_pong_handler([&](const std::vector<uint8_t>&) {
        pong_received = true;
        cv.notify_all();
    });

    ASSERT_TRUE(client.send_ping({}));

    std::unique_lock<std::mutex> lock(mutex);
    bool received = cv.wait_for(lock, 2s, [&]() { return pong_received.load(); });

    EXPECT_TRUE(received);
}

// ============================================================================
// Close Handshake Tests
// ============================================================================

TEST_F(WebSocketE2ETest, NormalClose)
{
    TestWebSocketServer server(port_);
    ASSERT_TRUE(server.start());

    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    ASSERT_TRUE(client.close(ws_close_code::normal, "Goodbye"));

    wait_for_ready();
    EXPECT_FALSE(client.is_connected());
}

TEST_F(WebSocketE2ETest, CloseWithCode)
{
    TestWebSocketServer server(port_);

    ws_close_code received_code = ws_close_code::normal;
    std::string received_reason;
    std::mutex mutex;
    std::condition_variable cv;

    server.set_on_client_connected([&](std::shared_ptr<websocket_socket> ws) {
        ws->set_close_callback([&](ws_close_code code, const std::string& reason) {
            std::lock_guard<std::mutex> lock(mutex);
            received_code = code;
            received_reason = reason;
            cv.notify_all();
        });
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    ASSERT_TRUE(client.close(ws_close_code::going_away, "Client leaving"));

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, 2s);

    EXPECT_EQ(received_code, ws_close_code::going_away);
}

// ============================================================================
// Echo Tests (Server sends back what it receives)
// ============================================================================

TEST_F(WebSocketE2ETest, EchoTextMessage)
{
    TestWebSocketServer server(port_);

    server.set_on_client_connected([](std::shared_ptr<websocket_socket> ws) {
        ws->set_message_callback([ws](const ws_message& msg) {
            if (msg.type == ws_message_type::text)
            {
                std::string text(msg.data.begin(), msg.data.end());
                ws->async_send_text(std::move(text), [](std::error_code, std::size_t) {});
            }
        });
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    std::string received_echo;
    std::mutex mutex;
    std::condition_variable cv;

    client.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::text)
        {
            received_echo = std::string(msg.data.begin(), msg.data.end());
        }
        cv.notify_all();
    });

    std::string message = "Echo this message!";
    ASSERT_TRUE(client.send_text(message));

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&]() { return !received_echo.empty(); }));

    EXPECT_EQ(received_echo, message);
}

TEST_F(WebSocketE2ETest, EchoBinaryMessage)
{
    TestWebSocketServer server(port_);

    server.set_on_client_connected([](std::shared_ptr<websocket_socket> ws) {
        ws->set_message_callback([ws](const ws_message& msg) {
            if (msg.type == ws_message_type::binary)
            {
                auto data = msg.data;
                ws->async_send_binary(std::move(data), [](std::error_code, std::size_t) {});
            }
        });
    });

    ASSERT_TRUE(server.start());
    wait_for_ready();

    TestWebSocketClient client;
    ASSERT_TRUE(client.connect("localhost", port_));

    std::vector<uint8_t> received_echo;
    std::mutex mutex;
    std::condition_variable cv;

    client.set_message_handler([&](const ws_message& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (msg.type == ws_message_type::binary)
        {
            received_echo = msg.data;
        }
        cv.notify_all();
    });

    std::vector<uint8_t> binary_data = {0x12, 0x34, 0x56, 0x78};
    ASSERT_TRUE(client.send_binary(binary_data));

    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(cv.wait_for(lock, 2s, [&]() { return !received_echo.empty(); }));

    EXPECT_EQ(received_echo, binary_data);
}

// ============================================================================
// Connection State Tests
// ============================================================================

TEST_F(WebSocketE2ETest, ConnectionState)
{
    TestWebSocketServer server(port_);
    ASSERT_TRUE(server.start());

    wait_for_ready();

    TestWebSocketClient client;
    EXPECT_FALSE(client.is_connected());

    ASSERT_TRUE(client.connect("localhost", port_));
    EXPECT_TRUE(client.is_connected());

    ASSERT_TRUE(client.close());
    wait_for_ready();
    EXPECT_FALSE(client.is_connected());
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
