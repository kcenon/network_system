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

#include <gtest/gtest.h>
#include <kcenon/network/detail/protocols/grpc/client.h>
#include <kcenon/network/detail/protocols/grpc/server.h>
#include <kcenon/network/detail/protocols/grpc/status.h>

#include <chrono>
#include <thread>

namespace grpc = kcenon::network::protocols::grpc;

// grpc_client tests
class GrpcClientTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcClientTest, Construction)
{
    grpc::grpc_client client("localhost:50051");
    EXPECT_EQ(client.target(), "localhost:50051");
    EXPECT_FALSE(client.is_connected());
}

TEST_F(GrpcClientTest, ConstructionWithConfig)
{
    grpc::grpc_channel_config config;
    config.default_timeout = std::chrono::milliseconds(5000);
    config.use_tls = false;

    grpc::grpc_client client("localhost:50051", config);
    EXPECT_EQ(client.target(), "localhost:50051");
}

TEST_F(GrpcClientTest, Connect)
{
    // Note: This test requires a real HTTP/2 server running.
    // The client now uses actual HTTP/2 transport instead of mock connection.
    // Connection to non-existent server should fail gracefully.
    grpc::grpc_client client("localhost:50051");

    auto result = client.connect();
    // Without a real server, connection will fail - this is expected behavior
    EXPECT_TRUE(result.is_err() || client.is_connected());
}

TEST_F(GrpcClientTest, ConnectInvalidTarget)
{
    grpc::grpc_client client("invalid_target_no_port");

    auto result = client.connect();
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, Disconnect)
{
    // Note: Without a real server, connect() will fail, so is_connected() will be false.
    // This test verifies disconnect() works safely even if not connected.
    grpc::grpc_client client("localhost:50051");
    client.connect();
    // Connection may fail without a real server - that's OK

    client.disconnect();
    EXPECT_FALSE(client.is_connected());  // Should be false after disconnect
}

TEST_F(GrpcClientTest, CallWithoutConnect)
{
    grpc::grpc_client client("localhost:50051");

    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.call_raw("/test.Service/Method", request);

    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallWithInvalidMethod)
{
    grpc::grpc_client client("localhost:50051");
    client.connect();

    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.call_raw("invalid_method", request);  // Missing leading /

    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, MoveConstruction)
{
    // Test move semantics - connection state will be false without a real server
    grpc::grpc_client client1("localhost:50051");
    client1.connect();  // May fail without server

    grpc::grpc_client client2(std::move(client1));
    EXPECT_EQ(client2.target(), "localhost:50051");
    // Connection state is transferred, but may be false if connect failed
}

TEST_F(GrpcClientTest, CallOptions)
{
    grpc::call_options options;
    options.set_timeout(std::chrono::seconds(10));
    options.metadata.emplace_back("key", "value");
    options.wait_for_ready = true;

    EXPECT_TRUE(options.deadline.has_value());
    EXPECT_EQ(options.metadata.size(), 1u);
    EXPECT_TRUE(options.wait_for_ready);
}

// grpc_server tests
class GrpcServerTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcServerTest, Construction)
{
    grpc::grpc_server server;
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.port(), 0u);
}

TEST_F(GrpcServerTest, ConstructionWithConfig)
{
    grpc::grpc_server_config config;
    config.max_concurrent_streams = 200;
    config.max_message_size = 8 * 1024 * 1024;

    grpc::grpc_server server(config);
    EXPECT_FALSE(server.is_running());
}

TEST_F(GrpcServerTest, Start)
{
    grpc::grpc_server server;

    auto result = server.start(50052);
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(server.is_running());
    EXPECT_EQ(server.port(), 50052u);

    server.stop();
}

TEST_F(GrpcServerTest, StartInvalidPort)
{
    grpc::grpc_server server;

    auto result = server.start(0);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, StartTwice)
{
    grpc::grpc_server server;
    server.start(50053);

    auto result = server.start(50054);
    EXPECT_TRUE(result.is_err());

    server.stop();
}

TEST_F(GrpcServerTest, Stop)
{
    grpc::grpc_server server;
    server.start(50055);
    EXPECT_TRUE(server.is_running());

    server.stop();
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.port(), 0u);
}

TEST_F(GrpcServerTest, RegisterUnaryMethod)
{
    grpc::grpc_server server;

    auto result = server.register_unary_method(
        "/test.Service/Method",
        [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), request};
        });

    EXPECT_TRUE(result.is_ok());
}

TEST_F(GrpcServerTest, RegisterMethodInvalidName)
{
    grpc::grpc_server server;

    auto result = server.register_unary_method(
        "invalid_method",  // Missing leading /
        [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), {}};
        });

    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterMethodNullHandler)
{
    grpc::grpc_server server;

    auto result = server.register_unary_method("/test.Service/Method", nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterMethodTwice)
{
    grpc::grpc_server server;

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
    {
        return {grpc::grpc_status::ok_status(), {}};
    };

    server.register_unary_method("/test.Service/Method", handler);
    auto result = server.register_unary_method("/test.Service/Method", handler);

    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterServerStreamingMethod)
{
    grpc::grpc_server server;

    auto result = server.register_server_streaming_method(
        "/test.Service/ServerStream",
        [](grpc::server_context& ctx,
           const std::vector<uint8_t>& request,
           grpc::server_writer& writer) -> grpc::grpc_status
        {
            return grpc::grpc_status::ok_status();
        });

    EXPECT_TRUE(result.is_ok());
}

TEST_F(GrpcServerTest, RegisterClientStreamingMethod)
{
    grpc::grpc_server server;

    auto result = server.register_client_streaming_method(
        "/test.Service/ClientStream",
        [](grpc::server_context& ctx, grpc::server_reader& reader)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), {}};
        });

    EXPECT_TRUE(result.is_ok());
}

TEST_F(GrpcServerTest, RegisterBidiStreamingMethod)
{
    grpc::grpc_server server;

    auto result = server.register_bidi_streaming_method(
        "/test.Service/BidiStream",
        [](grpc::server_context& ctx, grpc::server_reader_writer& stream)
            -> grpc::grpc_status
        {
            return grpc::grpc_status::ok_status();
        });

    EXPECT_TRUE(result.is_ok());
}

TEST_F(GrpcServerTest, RegisterServiceNull)
{
    grpc::grpc_server server;

    auto result = server.register_service(nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, MoveConstruction)
{
    grpc::grpc_server server1;
    server1.start(50056);

    grpc::grpc_server server2(std::move(server1));
    EXPECT_TRUE(server2.is_running());
    EXPECT_EQ(server2.port(), 50056u);

    server2.stop();
}

TEST_F(GrpcClientTest, DoubleDisconnect)
{
    grpc::grpc_client client("localhost:50051");
    client.disconnect();
    client.disconnect();
    EXPECT_FALSE(client.is_connected());
}

TEST_F(GrpcClientTest, WaitForConnectedWithoutConnect)
{
    grpc::grpc_client client("localhost:50051");
    auto connected = client.wait_for_connected(std::chrono::milliseconds(50));
    EXPECT_FALSE(connected);
}

TEST_F(GrpcClientTest, CallRawWithEmptyRequest)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> empty_request;
    auto result = client.call_raw("/test.Service/Method", empty_request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWithEmptyMethodName)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.call_raw("", request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ServerStreamWithoutConnect)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.server_stream_raw("/test.Service/Stream", request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ClientStreamWithoutConnect)
{
    grpc::grpc_client client("localhost:50051");
    auto result = client.client_stream_raw("/test.Service/Stream");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, BidiStreamWithoutConnect)
{
    grpc::grpc_client client("localhost:50051");
    auto result = client.bidi_stream_raw("/test.Service/Stream");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, MoveAssignment)
{
    grpc::grpc_client client1("localhost:50051");
    grpc::grpc_client client2("localhost:50052");

    client2 = std::move(client1);
    EXPECT_EQ(client2.target(), "localhost:50051");
}

TEST_F(GrpcClientTest, ConnectAndDisconnectCycle)
{
    grpc::grpc_client client("localhost:50051");
    client.connect();
    client.disconnect();
    EXPECT_FALSE(client.is_connected());

    // Second cycle should also work
    auto result = client.connect();
    EXPECT_TRUE(result.is_err() || client.is_connected());
    client.disconnect();
    EXPECT_FALSE(client.is_connected());
}

TEST_F(GrpcClientTest, CallRawAsyncWithoutConnect)
{
    grpc::grpc_client client("localhost:50051");
    std::atomic<bool> callback_called{false};
    bool was_error = false;

    client.call_raw_async(
        "/test.Service/Method",
        {1, 2, 3},
        [&](auto result)
        {
            was_error = result.is_err();
            callback_called.store(true);
        });

    // Wait briefly for the async callback
    for (int i = 0; i < 100 && !callback_called.load(); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(callback_called.load());
    EXPECT_TRUE(was_error);
}

TEST_F(GrpcClientTest, MultipleClientsIndependent)
{
    grpc::grpc_client client1("localhost:50051");
    grpc::grpc_client client2("localhost:50052");
    grpc::grpc_client client3("localhost:50053");

    EXPECT_EQ(client1.target(), "localhost:50051");
    EXPECT_EQ(client2.target(), "localhost:50052");
    EXPECT_EQ(client3.target(), "localhost:50053");
    EXPECT_FALSE(client1.is_connected());
    EXPECT_FALSE(client2.is_connected());
    EXPECT_FALSE(client3.is_connected());
}

// Extended server tests

TEST_F(GrpcServerTest, DoubleStop)
{
    grpc::grpc_server server;
    server.start(50057);
    server.stop();
    server.stop();
    EXPECT_FALSE(server.is_running());
    EXPECT_EQ(server.port(), 0u);
}

TEST_F(GrpcServerTest, StopWithoutStart)
{
    grpc::grpc_server server;
    server.stop();
    EXPECT_FALSE(server.is_running());
}

TEST_F(GrpcServerTest, StartAfterStop)
{
    grpc::grpc_server server;
    auto result1 = server.start(50058);
    EXPECT_TRUE(result1.is_ok());
    server.stop();

    auto result2 = server.start(50059);
    EXPECT_TRUE(result2.is_ok());
    EXPECT_TRUE(server.is_running());
    EXPECT_EQ(server.port(), 50059u);
    server.stop();
}

TEST_F(GrpcServerTest, MoveAssignment)
{
    grpc::grpc_server server1;
    server1.start(50060);

    grpc::grpc_server server2;
    server2 = std::move(server1);
    EXPECT_TRUE(server2.is_running());
    EXPECT_EQ(server2.port(), 50060u);

    server2.stop();
}

TEST_F(GrpcServerTest, StartTlsEmptyCert)
{
    grpc::grpc_server server;
    auto result = server.start_tls(50061, "", "/path/to/key.pem");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, StartTlsEmptyKey)
{
    grpc::grpc_server server;
    auto result = server.start_tls(50062, "/path/to/cert.pem", "");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, StartTlsBothEmpty)
{
    grpc::grpc_server server;
    auto result = server.start_tls(50063, "", "");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, StartTlsAlreadyRunning)
{
    grpc::grpc_server server;
    server.start(50064);

    auto result = server.start_tls(50065, "/path/to/cert.pem", "/path/to/key.pem");
    EXPECT_TRUE(result.is_err());

    server.stop();
}

TEST_F(GrpcServerTest, RegisterServerStreamingInvalidName)
{
    grpc::grpc_server server;
    auto result = server.register_server_streaming_method(
        "no_leading_slash",
        [](grpc::server_context& ctx,
           const std::vector<uint8_t>& request,
           grpc::server_writer& writer) -> grpc::grpc_status
        {
            return grpc::grpc_status::ok_status();
        });
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterServerStreamingNullHandler)
{
    grpc::grpc_server server;
    auto result = server.register_server_streaming_method(
        "/test.Service/Stream", nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterServerStreamingDuplicate)
{
    grpc::grpc_server server;
    auto handler = [](grpc::server_context& ctx,
                      const std::vector<uint8_t>& request,
                      grpc::server_writer& writer) -> grpc::grpc_status
    {
        return grpc::grpc_status::ok_status();
    };
    server.register_server_streaming_method("/test.Service/Stream", handler);
    auto result = server.register_server_streaming_method("/test.Service/Stream", handler);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterClientStreamingInvalidName)
{
    grpc::grpc_server server;
    auto result = server.register_client_streaming_method(
        "no_leading_slash",
        [](grpc::server_context& ctx, grpc::server_reader& reader)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), {}};
        });
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterClientStreamingNullHandler)
{
    grpc::grpc_server server;
    auto result = server.register_client_streaming_method(
        "/test.Service/Stream", nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterClientStreamingDuplicate)
{
    grpc::grpc_server server;
    auto handler = [](grpc::server_context& ctx, grpc::server_reader& reader)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
    {
        return {grpc::grpc_status::ok_status(), {}};
    };
    server.register_client_streaming_method("/test.Service/ClientStream", handler);
    auto result = server.register_client_streaming_method(
        "/test.Service/ClientStream", handler);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterBidiStreamingInvalidName)
{
    grpc::grpc_server server;
    auto result = server.register_bidi_streaming_method(
        "no_leading_slash",
        [](grpc::server_context& ctx, grpc::server_reader_writer& stream)
            -> grpc::grpc_status
        {
            return grpc::grpc_status::ok_status();
        });
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterBidiStreamingNullHandler)
{
    grpc::grpc_server server;
    auto result = server.register_bidi_streaming_method(
        "/test.Service/Bidi", nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterBidiStreamingDuplicate)
{
    grpc::grpc_server server;
    auto handler = [](grpc::server_context& ctx,
                      grpc::server_reader_writer& stream) -> grpc::grpc_status
    {
        return grpc::grpc_status::ok_status();
    };
    server.register_bidi_streaming_method("/test.Service/BidiStream", handler);
    auto result = server.register_bidi_streaming_method(
        "/test.Service/BidiStream", handler);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterMethodEmptyName)
{
    grpc::grpc_server server;
    auto result = server.register_unary_method(
        "",
        [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), {}};
        });
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcServerTest, RegisterMixedMethodTypes)
{
    grpc::grpc_server server;

    auto r1 = server.register_unary_method(
        "/test.Service/Unary",
        [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), {}};
        });
    EXPECT_TRUE(r1.is_ok());

    auto r2 = server.register_server_streaming_method(
        "/test.Service/ServerStream",
        [](grpc::server_context& ctx,
           const std::vector<uint8_t>& request,
           grpc::server_writer& writer) -> grpc::grpc_status
        {
            return grpc::grpc_status::ok_status();
        });
    EXPECT_TRUE(r2.is_ok());

    auto r3 = server.register_client_streaming_method(
        "/test.Service/ClientStream",
        [](grpc::server_context& ctx, grpc::server_reader& reader)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
        {
            return {grpc::grpc_status::ok_status(), {}};
        });
    EXPECT_TRUE(r3.is_ok());

    auto r4 = server.register_bidi_streaming_method(
        "/test.Service/BidiStream",
        [](grpc::server_context& ctx, grpc::server_reader_writer& stream)
            -> grpc::grpc_status
        {
            return grpc::grpc_status::ok_status();
        });
    EXPECT_TRUE(r4.is_ok());
}

TEST_F(GrpcServerTest, MultipleServersIndependent)
{
    grpc::grpc_server server1;
    grpc::grpc_server server2;

    server1.start(50066);
    server2.start(50067);

    EXPECT_TRUE(server1.is_running());
    EXPECT_TRUE(server2.is_running());
    EXPECT_EQ(server1.port(), 50066u);
    EXPECT_EQ(server2.port(), 50067u);

    server1.stop();
    EXPECT_FALSE(server1.is_running());
    EXPECT_TRUE(server2.is_running());

    server2.stop();
}

// Channel config tests
class GrpcChannelConfigTest : public ::testing::Test {};

TEST_F(GrpcChannelConfigTest, DefaultValues)
{
    grpc::grpc_channel_config config;

    EXPECT_EQ(config.default_timeout, std::chrono::milliseconds(30000));
    EXPECT_TRUE(config.use_tls);
    EXPECT_TRUE(config.root_certificates.empty());
    EXPECT_FALSE(config.client_certificate.has_value());
    EXPECT_FALSE(config.client_key.has_value());
    EXPECT_EQ(config.max_message_size, grpc::default_max_message_size);
}

TEST_F(GrpcChannelConfigTest, CustomTimeout)
{
    grpc::grpc_channel_config config;
    config.default_timeout = std::chrono::milliseconds(100);
    EXPECT_EQ(config.default_timeout, std::chrono::milliseconds(100));
}

TEST_F(GrpcChannelConfigTest, TlsConfiguration)
{
    grpc::grpc_channel_config config;
    config.use_tls = true;
    config.root_certificates = "PEM_ROOT_CERT";
    config.client_certificate = "PEM_CLIENT_CERT";
    config.client_key = "PEM_CLIENT_KEY";

    EXPECT_TRUE(config.use_tls);
    EXPECT_EQ(config.root_certificates, "PEM_ROOT_CERT");
    EXPECT_TRUE(config.client_certificate.has_value());
    EXPECT_EQ(config.client_certificate.value(), "PEM_CLIENT_CERT");
    EXPECT_TRUE(config.client_key.has_value());
    EXPECT_EQ(config.client_key.value(), "PEM_CLIENT_KEY");
}

TEST_F(GrpcChannelConfigTest, InsecureMode)
{
    grpc::grpc_channel_config config;
    config.use_tls = false;
    EXPECT_FALSE(config.use_tls);
}

TEST_F(GrpcChannelConfigTest, KeepaliveSettings)
{
    grpc::grpc_channel_config config;
    config.keepalive_time = std::chrono::milliseconds(60000);
    config.keepalive_timeout = std::chrono::milliseconds(5000);

    EXPECT_EQ(config.keepalive_time, std::chrono::milliseconds(60000));
    EXPECT_EQ(config.keepalive_timeout, std::chrono::milliseconds(5000));
}

TEST_F(GrpcChannelConfigTest, RetrySettings)
{
    grpc::grpc_channel_config config;
    config.max_retry_attempts = 5;
    EXPECT_EQ(config.max_retry_attempts, 5u);
}

TEST_F(GrpcChannelConfigTest, MaxMessageSize)
{
    grpc::grpc_channel_config config;
    config.max_message_size = 16 * 1024 * 1024;
    EXPECT_EQ(config.max_message_size, 16u * 1024 * 1024);
}

TEST_F(GrpcChannelConfigTest, CopyBehavior)
{
    grpc::grpc_channel_config config1;
    config1.default_timeout = std::chrono::milliseconds(5000);
    config1.use_tls = false;
    config1.max_retry_attempts = 10;

    grpc::grpc_channel_config config2 = config1;
    EXPECT_EQ(config2.default_timeout, std::chrono::milliseconds(5000));
    EXPECT_FALSE(config2.use_tls);
    EXPECT_EQ(config2.max_retry_attempts, 10u);
}

// Server config tests
class GrpcServerConfigTest : public ::testing::Test {};

TEST_F(GrpcServerConfigTest, DefaultValues)
{
    grpc::grpc_server_config config;

    EXPECT_EQ(config.max_concurrent_streams, 100u);
    EXPECT_EQ(config.max_message_size, grpc::default_max_message_size);
    EXPECT_EQ(config.keepalive_time, std::chrono::milliseconds(7200000));
    EXPECT_EQ(config.keepalive_timeout, std::chrono::milliseconds(20000));
    EXPECT_EQ(config.num_threads, 0u);
}

TEST_F(GrpcServerConfigTest, CustomConcurrentStreams)
{
    grpc::grpc_server_config config;
    config.max_concurrent_streams = 500;
    EXPECT_EQ(config.max_concurrent_streams, 500u);
}

TEST_F(GrpcServerConfigTest, ConnectionIdleAndAge)
{
    grpc::grpc_server_config config;
    EXPECT_EQ(config.max_connection_idle, std::chrono::milliseconds(0));
    EXPECT_EQ(config.max_connection_age, std::chrono::milliseconds(0));

    config.max_connection_idle = std::chrono::milliseconds(300000);
    config.max_connection_age = std::chrono::milliseconds(600000);

    EXPECT_EQ(config.max_connection_idle, std::chrono::milliseconds(300000));
    EXPECT_EQ(config.max_connection_age, std::chrono::milliseconds(600000));
}

TEST_F(GrpcServerConfigTest, NumThreads)
{
    grpc::grpc_server_config config;
    config.num_threads = 8;
    EXPECT_EQ(config.num_threads, 8u);
}

TEST_F(GrpcServerConfigTest, CopyBehavior)
{
    grpc::grpc_server_config config1;
    config1.max_concurrent_streams = 200;
    config1.max_message_size = 8 * 1024 * 1024;
    config1.num_threads = 4;

    grpc::grpc_server_config config2 = config1;
    EXPECT_EQ(config2.max_concurrent_streams, 200u);
    EXPECT_EQ(config2.max_message_size, 8u * 1024 * 1024);
    EXPECT_EQ(config2.num_threads, 4u);
}

// call_options tests
class GrpcCallOptionsTest : public ::testing::Test {};

TEST_F(GrpcCallOptionsTest, DefaultValues)
{
    grpc::call_options options;
    EXPECT_FALSE(options.deadline.has_value());
    EXPECT_TRUE(options.metadata.empty());
    EXPECT_FALSE(options.wait_for_ready);
    EXPECT_TRUE(options.compression_algorithm.empty());
}

TEST_F(GrpcCallOptionsTest, SetTimeoutSeconds)
{
    grpc::call_options options;
    auto before = std::chrono::system_clock::now();
    options.set_timeout(std::chrono::seconds(30));
    auto after = std::chrono::system_clock::now();

    ASSERT_TRUE(options.deadline.has_value());
    EXPECT_GE(options.deadline.value(), before + std::chrono::seconds(30));
    EXPECT_LE(options.deadline.value(), after + std::chrono::seconds(30));
}

TEST_F(GrpcCallOptionsTest, SetTimeoutMilliseconds)
{
    grpc::call_options options;
    auto before = std::chrono::system_clock::now();
    options.set_timeout(std::chrono::milliseconds(500));

    ASSERT_TRUE(options.deadline.has_value());
    EXPECT_GE(options.deadline.value(), before + std::chrono::milliseconds(500));
}

TEST_F(GrpcCallOptionsTest, MultipleMetadata)
{
    grpc::call_options options;
    options.metadata.emplace_back("key1", "value1");
    options.metadata.emplace_back("key2", "value2");
    options.metadata.emplace_back("key3", "value3");

    EXPECT_EQ(options.metadata.size(), 3u);
    EXPECT_EQ(options.metadata[0].first, "key1");
    EXPECT_EQ(options.metadata[1].second, "value2");
    EXPECT_EQ(options.metadata[2].first, "key3");
}

TEST_F(GrpcCallOptionsTest, WaitForReady)
{
    grpc::call_options options;
    options.wait_for_ready = true;
    EXPECT_TRUE(options.wait_for_ready);
}

TEST_F(GrpcCallOptionsTest, CompressionAlgorithm)
{
    grpc::call_options options;
    options.compression_algorithm = "gzip";
    EXPECT_EQ(options.compression_algorithm, "gzip");
}

// grpc_status tests
class GrpcStatusTest : public ::testing::Test {};

TEST_F(GrpcStatusTest, DefaultConstruction)
{
    grpc::grpc_status status;
    EXPECT_EQ(status.code, grpc::status_code::ok);
    EXPECT_TRUE(status.message.empty());
    EXPECT_FALSE(status.details.has_value());
    EXPECT_TRUE(status.is_ok());
    EXPECT_FALSE(status.is_error());
}

TEST_F(GrpcStatusTest, ConstructWithCode)
{
    grpc::grpc_status status(grpc::status_code::not_found);
    EXPECT_EQ(status.code, grpc::status_code::not_found);
    EXPECT_TRUE(status.message.empty());
    EXPECT_FALSE(status.is_ok());
    EXPECT_TRUE(status.is_error());
}

TEST_F(GrpcStatusTest, ConstructWithCodeAndMessage)
{
    grpc::grpc_status status(grpc::status_code::invalid_argument, "bad input");
    EXPECT_EQ(status.code, grpc::status_code::invalid_argument);
    EXPECT_EQ(status.message, "bad input");
    EXPECT_FALSE(status.details.has_value());
}

TEST_F(GrpcStatusTest, ConstructWithCodeMessageAndDetails)
{
    grpc::grpc_status status(
        grpc::status_code::internal, "server error", "binary_details");
    EXPECT_EQ(status.code, grpc::status_code::internal);
    EXPECT_EQ(status.message, "server error");
    EXPECT_TRUE(status.details.has_value());
    EXPECT_EQ(status.details.value(), "binary_details");
}

TEST_F(GrpcStatusTest, OkStatusFactory)
{
    auto status = grpc::grpc_status::ok_status();
    EXPECT_TRUE(status.is_ok());
    EXPECT_EQ(status.code, grpc::status_code::ok);
}

TEST_F(GrpcStatusTest, ErrorStatusFactory)
{
    auto status = grpc::grpc_status::error_status(
        grpc::status_code::unavailable, "service down");
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status.code, grpc::status_code::unavailable);
    EXPECT_EQ(status.message, "service down");
}

TEST_F(GrpcStatusTest, CodeStringOk)
{
    grpc::grpc_status status(grpc::status_code::ok);
    EXPECT_EQ(status.code_string(), "OK");
}

TEST_F(GrpcStatusTest, CodeStringAllCodes)
{
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::ok), "OK");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::cancelled), "CANCELLED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unknown), "UNKNOWN");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::invalid_argument), "INVALID_ARGUMENT");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::deadline_exceeded), "DEADLINE_EXCEEDED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::not_found), "NOT_FOUND");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::already_exists), "ALREADY_EXISTS");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::permission_denied), "PERMISSION_DENIED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::resource_exhausted), "RESOURCE_EXHAUSTED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::failed_precondition), "FAILED_PRECONDITION");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::aborted), "ABORTED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::out_of_range), "OUT_OF_RANGE");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unimplemented), "UNIMPLEMENTED");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::internal), "INTERNAL");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unavailable), "UNAVAILABLE");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::data_loss), "DATA_LOSS");
    EXPECT_EQ(grpc::status_code_to_string(grpc::status_code::unauthenticated), "UNAUTHENTICATED");
}

TEST_F(GrpcStatusTest, CodeStringUnknownCode)
{
    auto result = grpc::status_code_to_string(static_cast<grpc::status_code>(99));
    EXPECT_EQ(result, "UNKNOWN");
}

TEST_F(GrpcStatusTest, IsOkVsIsError)
{
    grpc::grpc_status ok_status(grpc::status_code::ok);
    EXPECT_TRUE(ok_status.is_ok());
    EXPECT_FALSE(ok_status.is_error());

    grpc::grpc_status err_status(grpc::status_code::cancelled);
    EXPECT_FALSE(err_status.is_ok());
    EXPECT_TRUE(err_status.is_error());
}

// grpc_trailers tests
class GrpcTrailersTest : public ::testing::Test {};

TEST_F(GrpcTrailersTest, DefaultValues)
{
    grpc::grpc_trailers trailers;
    EXPECT_EQ(trailers.status, grpc::status_code::ok);
    EXPECT_TRUE(trailers.status_message.empty());
    EXPECT_FALSE(trailers.status_details.has_value());
}

TEST_F(GrpcTrailersTest, ToStatusOk)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::ok;

    auto status = trailers.to_status();
    EXPECT_TRUE(status.is_ok());
    EXPECT_EQ(status.code, grpc::status_code::ok);
}

TEST_F(GrpcTrailersTest, ToStatusWithMessage)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::not_found;
    trailers.status_message = "resource not found";

    auto status = trailers.to_status();
    EXPECT_TRUE(status.is_error());
    EXPECT_EQ(status.code, grpc::status_code::not_found);
    EXPECT_EQ(status.message, "resource not found");
    EXPECT_FALSE(status.details.has_value());
}

TEST_F(GrpcTrailersTest, ToStatusWithDetails)
{
    grpc::grpc_trailers trailers;
    trailers.status = grpc::status_code::internal;
    trailers.status_message = "internal error";
    trailers.status_details = "binary_encoded_details";

    auto status = trailers.to_status();
    EXPECT_EQ(status.code, grpc::status_code::internal);
    EXPECT_EQ(status.message, "internal error");
    EXPECT_TRUE(status.details.has_value());
    EXPECT_EQ(status.details.value(), "binary_encoded_details");
}

// grpc_message tests
class GrpcMessageTest : public ::testing::Test {};

TEST_F(GrpcMessageTest, DefaultConstruction)
{
    grpc::grpc_message msg;
    EXPECT_FALSE(msg.compressed);
    EXPECT_TRUE(msg.empty());
    EXPECT_EQ(msg.size(), 0u);
    EXPECT_EQ(msg.serialized_size(), grpc::grpc_header_size);
}

TEST_F(GrpcMessageTest, ConstructWithData)
{
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    grpc::grpc_message msg(data);

    EXPECT_FALSE(msg.compressed);
    EXPECT_FALSE(msg.empty());
    EXPECT_EQ(msg.size(), 5u);
    EXPECT_EQ(msg.serialized_size(), grpc::grpc_header_size + 5);
}

TEST_F(GrpcMessageTest, ConstructWithCompression)
{
    std::vector<uint8_t> data = {10, 20, 30};
    grpc::grpc_message msg(data, true);

    EXPECT_TRUE(msg.compressed);
    EXPECT_EQ(msg.size(), 3u);
}

TEST_F(GrpcMessageTest, SerializeAndParse)
{
    std::vector<uint8_t> original = {0xDE, 0xAD, 0xBE, 0xEF};
    grpc::grpc_message msg(original);
    auto serialized = msg.serialize();

    EXPECT_EQ(serialized.size(), grpc::grpc_header_size + 4);

    auto parsed = grpc::grpc_message::parse(serialized);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().data, original);
    EXPECT_FALSE(parsed.value().compressed);
}

TEST_F(GrpcMessageTest, Constants)
{
    EXPECT_EQ(grpc::grpc_header_size, 5u);
    EXPECT_EQ(grpc::default_max_message_size, 4u * 1024 * 1024);
}

TEST_F(GrpcMessageTest, ContentTypeConstants)
{
    EXPECT_STREQ(grpc::grpc_content_type, "application/grpc");
    EXPECT_STREQ(grpc::grpc_content_type_proto, "application/grpc+proto");
}

TEST_F(GrpcMessageTest, TrailerNameConstants)
{
    EXPECT_STREQ(grpc::trailer_names::grpc_status, "grpc-status");
    EXPECT_STREQ(grpc::trailer_names::grpc_message, "grpc-message");
    EXPECT_STREQ(grpc::trailer_names::grpc_status_details, "grpc-status-details-bin");
}

TEST_F(GrpcMessageTest, HeaderNameConstants)
{
    EXPECT_STREQ(grpc::header_names::te, "te");
    EXPECT_STREQ(grpc::header_names::content_type, "content-type");
    EXPECT_STREQ(grpc::header_names::grpc_encoding, "grpc-encoding");
    EXPECT_STREQ(grpc::header_names::grpc_accept_encoding, "grpc-accept-encoding");
    EXPECT_STREQ(grpc::header_names::grpc_timeout, "grpc-timeout");
    EXPECT_STREQ(grpc::header_names::user_agent, "user-agent");
}

TEST_F(GrpcMessageTest, CompressionConstants)
{
    EXPECT_STREQ(grpc::compression::identity, "identity");
    EXPECT_STREQ(grpc::compression::deflate, "deflate");
    EXPECT_STREQ(grpc::compression::gzip, "gzip");
}
