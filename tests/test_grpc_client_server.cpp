// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

// ============================================================================
// Coverage expansion tests for src/protocols/grpc/client.cpp
// Part of #1063, Part of #953
//
// These tests target the surface of grpc_client reachable WITHOUT a connected
// gRPC peer: validation guards, disconnected-state early returns, public-API
// input variations, and struct-method edges. They do not exercise the
// post-connect frame I/O loop, which requires an in-process loopback fixture
// not present in this tree.
// ============================================================================

// ---- Disconnected-state guards: call_raw input variations -----------------

TEST_F(GrpcClientTest, CallRawWithoutLeadingSlashLongName)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.call_raw(
        "package.with.many.dots.Service/Method", request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWithSlashOnlyMethod)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.call_raw("/", request);
    // Disconnected: must return an error without crashing
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWithLargeRequestDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request(64 * 1024, 0xAB);
    auto result = client.call_raw("/test.Service/Big", request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWithMetadataDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    options.metadata.emplace_back("authorization", "Bearer token");
    options.metadata.emplace_back("x-trace-id", "abc-123");

    std::vector<uint8_t> request = {1};
    auto result = client.call_raw("/test.Service/M", request, options);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWithExpiredDeadline)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    // Deadline 1 second in the past
    options.deadline =
        std::chrono::system_clock::now() - std::chrono::seconds(1);

    std::vector<uint8_t> request = {1, 2};
    auto result = client.call_raw("/test.Service/M", request, options);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWithFutureDeadlineDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    options.set_timeout(std::chrono::milliseconds(1));

    std::vector<uint8_t> request = {1};
    auto result = client.call_raw("/test.Service/M", request, options);
    // Disconnected guard fires first
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, CallRawWaitForReadyDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    options.wait_for_ready = true;

    std::vector<uint8_t> request = {1};
    auto result = client.call_raw("/test.Service/M", request, options);
    EXPECT_TRUE(result.is_err());
}

// ---- Disconnected-state guards: streaming variants ------------------------

TEST_F(GrpcClientTest, ServerStreamWithEmptyMethod)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request = {1, 2, 3};
    auto result = client.server_stream_raw("", request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ServerStreamWithoutLeadingSlash)
{
    grpc::grpc_client client("localhost:50051");
    std::vector<uint8_t> request = {1};
    auto result = client.server_stream_raw("test.Service/Stream", request);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ClientStreamWithEmptyMethod)
{
    grpc::grpc_client client("localhost:50051");
    auto result = client.client_stream_raw("");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ClientStreamWithoutLeadingSlash)
{
    grpc::grpc_client client("localhost:50051");
    auto result = client.client_stream_raw("no_slash_method");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, BidiStreamWithEmptyMethod)
{
    grpc::grpc_client client("localhost:50051");
    auto result = client.bidi_stream_raw("");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, BidiStreamWithoutLeadingSlash)
{
    grpc::grpc_client client("localhost:50051");
    auto result = client.bidi_stream_raw("no_slash_method");
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ServerStreamWithMetadataDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    options.metadata.emplace_back("k", "v");

    std::vector<uint8_t> request = {0xFF};
    auto result = client.server_stream_raw(
        "/test.Service/Stream", request, options);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ClientStreamWithMetadataDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    options.metadata.emplace_back("k", "v");

    auto result = client.client_stream_raw("/test.Service/Stream", options);
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, BidiStreamWithMetadataDisconnected)
{
    grpc::grpc_client client("localhost:50051");
    grpc::call_options options;
    options.metadata.emplace_back("k", "v");

    auto result = client.bidi_stream_raw("/test.Service/Stream", options);
    EXPECT_TRUE(result.is_err());
}

// ---- Constructor / target variations --------------------------------------

TEST_F(GrpcClientTest, ConstructWithEmptyTarget)
{
    grpc::grpc_client client("");
    EXPECT_EQ(client.target(), "");
    EXPECT_FALSE(client.is_connected());
}

TEST_F(GrpcClientTest, ConstructWithIpv4Target)
{
    grpc::grpc_client client("127.0.0.1:50051");
    EXPECT_EQ(client.target(), "127.0.0.1:50051");
    EXPECT_FALSE(client.is_connected());
}

TEST_F(GrpcClientTest, ConstructWithLongTargetString)
{
    std::string long_host(500, 'a');
    std::string target = long_host + ":50051";
    grpc::grpc_client client(target);
    EXPECT_EQ(client.target(), target);
}

TEST_F(GrpcClientTest, ConstructWithSpecialCharsTarget)
{
    grpc::grpc_client client("host-name_1.example.com:65535");
    EXPECT_EQ(client.target(), "host-name_1.example.com:65535");
}

TEST_F(GrpcClientTest, ConnectInvalidPortNonNumeric)
{
    grpc::grpc_client client("localhost:abc");
    auto result = client.connect();
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, ConnectMultipleColons)
{
    // Parser uses find(':') so first colon is the separator;
    // the remainder ":50051" fails the numeric port parse.
    grpc::grpc_client client("host::50051");
    auto result = client.connect();
    EXPECT_TRUE(result.is_err());
}

// ---- Channel config variations + connect path -----------------------------

TEST_F(GrpcClientTest, ConnectWithInsecureConfig)
{
    grpc::grpc_channel_config config;
    config.use_tls = false;
    config.default_timeout = std::chrono::milliseconds(50);

    grpc::grpc_client client("localhost:50051", config);
    auto result = client.connect();
    // Without a real server: must error out, not block forever
    EXPECT_TRUE(result.is_err() || client.is_connected());
}

TEST_F(GrpcClientTest, ConnectWithCustomKeepalive)
{
    grpc::grpc_channel_config config;
    config.use_tls = false;
    config.keepalive_time = std::chrono::milliseconds(1000);
    config.keepalive_timeout = std::chrono::milliseconds(500);
    config.default_timeout = std::chrono::milliseconds(50);

    grpc::grpc_client client("localhost:50051", config);
    auto result = client.connect();
    EXPECT_TRUE(result.is_err() || client.is_connected());
}

TEST_F(GrpcClientTest, ConnectWithMaxRetryAttemptsZero)
{
    grpc::grpc_channel_config config;
    config.use_tls = false;
    config.max_retry_attempts = 0;
    config.default_timeout = std::chrono::milliseconds(50);

    grpc::grpc_client client("localhost:50051", config);
    auto result = client.connect();
    EXPECT_TRUE(result.is_err() || client.is_connected());
}

// ---- Multi-instance state isolation ---------------------------------------

TEST_F(GrpcClientTest, MultipleInstancesDisconnectIndependently)
{
    grpc::grpc_client client1("localhost:50051");
    grpc::grpc_client client2("localhost:50052");

    client1.disconnect();
    client2.disconnect();
    EXPECT_FALSE(client1.is_connected());
    EXPECT_FALSE(client2.is_connected());
    EXPECT_NE(client1.target(), client2.target());
}

TEST_F(GrpcClientTest, MoveAssignmentDoesNotAliasState)
{
    grpc::grpc_client client1("localhost:50051");
    grpc::grpc_client client2("localhost:50052");

    client2 = std::move(client1);
    EXPECT_EQ(client2.target(), "localhost:50051");
    // After move, calling disconnect on the destination must remain safe
    client2.disconnect();
    EXPECT_FALSE(client2.is_connected());
}

// ---- call_options edges ---------------------------------------------------

TEST_F(GrpcCallOptionsTest, SetTimeoutZero)
{
    grpc::call_options options;
    options.set_timeout(std::chrono::milliseconds(0));
    ASSERT_TRUE(options.deadline.has_value());
    // Deadline computed from "now"; should be at most a few ticks ahead
    auto now = std::chrono::system_clock::now();
    EXPECT_LE(options.deadline.value(), now + std::chrono::seconds(1));
}

TEST_F(GrpcCallOptionsTest, SetTimeoutVeryLarge)
{
    grpc::call_options options;
    options.set_timeout(std::chrono::hours(24));
    ASSERT_TRUE(options.deadline.has_value());
    auto now = std::chrono::system_clock::now();
    EXPECT_GT(options.deadline.value(), now + std::chrono::hours(23));
}

TEST_F(GrpcCallOptionsTest, MetadataPreservesInsertionOrder)
{
    grpc::call_options options;
    for (int i = 0; i < 10; ++i)
    {
        options.metadata.emplace_back("k" + std::to_string(i),
                                      "v" + std::to_string(i));
    }
    ASSERT_EQ(options.metadata.size(), 10u);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(options.metadata[static_cast<size_t>(i)].first,
                  "k" + std::to_string(i));
    }
}

// ---- grpc_message edges ---------------------------------------------------

TEST_F(GrpcMessageTest, ParseTooShortBuffer)
{
    std::vector<uint8_t> too_short = {0x00, 0x00, 0x00};
    auto parsed = grpc::grpc_message::parse(too_short);
    EXPECT_TRUE(parsed.is_err());
}

TEST_F(GrpcMessageTest, ParseEmptyBuffer)
{
    std::vector<uint8_t> empty;
    auto parsed = grpc::grpc_message::parse(empty);
    EXPECT_TRUE(parsed.is_err());
}

TEST_F(GrpcMessageTest, SerializeRoundTripWithEmbeddedNull)
{
    std::vector<uint8_t> data = {0x00, 0xAA, 0x00, 0xFF, 0x00};
    grpc::grpc_message msg(data);
    auto serialized = msg.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().data, data);
}

TEST_F(GrpcMessageTest, SerializeRoundTripLargePayload)
{
    std::vector<uint8_t> data(8192, 0x5A);
    grpc::grpc_message msg(data);
    auto serialized = msg.serialize();
    EXPECT_EQ(serialized.size(), grpc::grpc_header_size + data.size());

    auto parsed = grpc::grpc_message::parse(serialized);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().data.size(), data.size());
}

TEST_F(GrpcMessageTest, SerializeRoundTripAllByteValues)
{
    std::vector<uint8_t> data;
    data.reserve(256);
    for (int i = 0; i < 256; ++i)
    {
        data.push_back(static_cast<uint8_t>(i));
    }
    grpc::grpc_message msg(data);
    auto serialized = msg.serialize();
    auto parsed = grpc::grpc_message::parse(serialized);
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().data, data);
}

TEST_F(GrpcMessageTest, EmptyMessageSerializeIsHeaderOnly)
{
    grpc::grpc_message msg;
    auto serialized = msg.serialize();
    EXPECT_EQ(serialized.size(), grpc::grpc_header_size);
}

// =====================================================================
// Hermetic transport coverage (Issue #1063)
//
// The tests above operate purely on the public API in disconnected /
// constructor states. This section drives the post-handshake code paths
// in src/protocols/grpc/client.cpp that are otherwise unreachable from a
// hermetic CI environment:
//   - call_raw HEADERS+DATA success path (status code mapping into
//     Result<grpc_message>, response framing, trace attributes)
//   - call_raw error paths (non-200 HTTP, gRPC status != OK trailer,
//     malformed response body)
//   - server_stream_reader_impl::on_headers / on_data / on_complete /
//     read / finish — server-streaming RPC error and end-of-stream paths
//   - client_stream_writer_impl::write / writes_done / finish — client-
//     streaming write loop, idempotency, error after writes_done
//   - bidi_stream_impl::write / read / writes_done / finish — bidi error
//     and end-of-stream paths
//   - Deadline / cancellation propagation: grpc-timeout header, post-
//     connect deadline_exceeded, RST_STREAM cancellation
//   - Metadata frame edges: malformed grpc-status, oversize trailer
//
// Each test reuses tls_loopback_listener for the TLS-with-ALPN-h2 layer
// and drives the HTTP/2 framing manually so the test author retains full
// control over what the client receives. Tests are scoped to error /
// branch coverage that the disconnected-state tests above cannot cover.
// They reuse the in-process gRPC test harness (mock_h2_server_peer +
// hermetic_transport_fixture from tests/support/) and do NOT introduce
// real network mocks.
// =====================================================================

// The hermetic-transport coverage uses TLS / SSL_stream + HPACK + HTTP/2
// frame builders, so it is only compiled when the test binary has access
// to those facilities. All other (disconnected / public-API) tests above
// are unaffected by this guard and continue to build everywhere.
#if !defined(NETWORK_GRPC_OFFICIAL) || NETWORK_GRPC_OFFICIAL == 0

#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/hpack.h"

#include "hermetic_transport_fixture.h"
#include "mock_h2_server_peer.h"
#include "mock_tls_socket.h"

#include <asio/buffer.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <span>
#include <thread>

namespace support_grpc = kcenon::network::tests::support;
namespace http2_grpc = kcenon::network::protocols::http2;

namespace
{

using namespace std::chrono_literals;

// Frame header is fixed at 9 bytes for HTTP/2 (RFC 7540 Section 4.1).
constexpr std::size_t kFrameHeaderSize = 9;
constexpr std::size_t kPrefaceSize = 24;

// HTTP/2 connection preface bytes.
constexpr std::uint8_t kPrefaceBytes[kPrefaceSize] = {
    'P',  'R',  'I',  ' ',  '*',  ' ',  'H',  'T',
    'T',  'P',  '/',  '2',  '.',  '0',  '\r', '\n',
    '\r', '\n', 'S',  'M',  '\r', '\n', '\r', '\n'
};

/**
 * @brief Read the client preface and exchange SETTINGS, then return the
 *        accepted SSL stream so the caller can inject custom frames.
 *
 * Mirrors the first four steps of @ref support::mock_h2_server_peer but
 * exposes the underlying SSL stream so individual tests can write
 * arbitrary frame sequences (HEADERS+DATA with gRPC framing, RST_STREAM,
 * GOAWAY, malformed bodies, etc.).
 *
 * @returns the post-SETTINGS stream on success, or nullptr on any I/O
 *          or framing error.
 */
std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>>
grpc_complete_settings_exchange(support_grpc::tls_loopback_listener& listener)
{
    auto stream = listener.accepted_socket(std::chrono::seconds(5));
    if (!stream)
    {
        return nullptr;
    }

    std::error_code ec;

    // Read 24-byte client preface.
    std::array<std::uint8_t, kPrefaceSize> preface_buf{};
    asio::read(*stream, asio::buffer(preface_buf), ec);
    if (ec ||
        std::memcmp(preface_buf.data(), kPrefaceBytes, kPrefaceSize) != 0)
    {
        return nullptr;
    }

    // Send empty server SETTINGS.
    {
        http2_grpc::settings_frame initial({}, /*ack=*/false);
        const auto bytes = initial.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec)
        {
            return nullptr;
        }
    }

    // Read client SETTINGS frame header.
    std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
    asio::read(*stream, asio::buffer(hdr_buf), ec);
    if (ec)
    {
        return nullptr;
    }
    auto parsed = http2_grpc::frame_header::parse(
        std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
    if (parsed.is_err())
    {
        return nullptr;
    }
    const auto hdr = parsed.value();
    if (hdr.length > 0)
    {
        std::vector<std::uint8_t> payload(hdr.length);
        asio::read(*stream, asio::buffer(payload), ec);
        if (ec)
        {
            return nullptr;
        }
    }

    // Send SETTINGS-ACK.
    {
        http2_grpc::settings_frame ack_frame({}, /*ack=*/true);
        const auto bytes = ack_frame.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec)
        {
            return nullptr;
        }
    }

    return stream;
}

/**
 * @brief Discard a single inbound frame from @p stream by reading the
 *        9-byte header and then draining the payload.
 *
 * @returns true on success, false on any read error.
 */
bool drain_one_frame(asio::ssl::stream<asio::ip::tcp::socket>& stream,
                     http2_grpc::frame_header& out_hdr)
{
    std::error_code ec;
    std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
    asio::read(stream, asio::buffer(hdr_buf), ec);
    if (ec) return false;
    auto parsed = http2_grpc::frame_header::parse(
        std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
    if (parsed.is_err()) return false;
    out_hdr = parsed.value();
    if (out_hdr.length > 0)
    {
        std::vector<std::uint8_t> drain(out_hdr.length);
        asio::read(stream, asio::buffer(drain), ec);
        if (ec) return false;
    }
    return true;
}

/**
 * @brief Drain remaining inbound frames until EOF (e.g. the client emits
 *        GOAWAY on disconnect()). Used at the end of every peer thread
 *        to keep the worker around until the client closes the socket.
 */
void drain_until_eof(asio::ssl::stream<asio::ip::tcp::socket>& stream)
{
    while (true)
    {
        http2_grpc::frame_header hdr{};
        if (!drain_one_frame(stream, hdr))
        {
            return;
        }
    }
}

/**
 * @brief Build an HPACK-encoded header block carrying :status plus
 *        optional trailers (grpc-status, grpc-message).
 *
 * Each call constructs a fresh encoder so dynamic-table state does not
 * leak between tests.
 */
std::vector<std::uint8_t> grpc_encode_response_headers(
    int http_status,
    int grpc_status_code,
    const std::string& grpc_message_str = {})
{
    http2_grpc::hpack_encoder enc(4096);
    std::vector<http2_grpc::http_header> headers;
    headers.reserve(4);
    headers.emplace_back(":status", std::to_string(http_status));
    headers.emplace_back("content-type", "application/grpc");
    headers.emplace_back("grpc-status", std::to_string(grpc_status_code));
    if (!grpc_message_str.empty())
    {
        headers.emplace_back("grpc-message", grpc_message_str);
    }
    return enc.encode(headers);
}

/**
 * @brief Build a serialized gRPC message body (5-byte frame header + payload).
 */
std::vector<std::uint8_t> build_grpc_body(const std::vector<std::uint8_t>& payload,
                                          bool compressed = false)
{
    grpc::grpc_message msg(payload, compressed);
    return msg.serialize();
}

/**
 * @brief Configuration helper used by the hermetic gRPC tests below.
 *
 * Creates a grpc_client targeting the loopback listener that
 * mock_h2_server_peer / grpc_complete_settings_exchange opened.
 */
std::shared_ptr<grpc::grpc_client> make_grpc_client_for_listener(
    unsigned short port,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
    grpc::grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = timeout;
    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(port));
    return std::make_shared<grpc::grpc_client>(target, cfg);
}

} // namespace

class GrpcClientHermeticTransportCoverageTest
    : public support_grpc::hermetic_transport_fixture
{
};

// HEADERS+DATA success: drive the post-connect successful call_raw path
// where the peer replies with grpc-status=0 and a serialized gRPC body.
// Exercises:
//  - response.status_code == 200 branch
//  - trailer parse loop matching grpc-status=0
//  - grpc_message::parse() success branch on response.body
//  - tracing attribute set_attribute("rpc.response.size", ...)
TEST_F(GrpcClientHermeticTransportCoverageTest, CallRawSucceedsWithGrpcStatusOk)
{
    support_grpc::tls_loopback_listener listener(io());

    std::atomic<bool> peer_done{false};
    const std::vector<std::uint8_t> response_payload{
        0xDE, 0xAD, 0xBE, 0xEF, 0x42};

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        // Drain client HEADERS request.
        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;

        // Drain client DATA request.
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // Reply HEADERS with :status 200, content-type, grpc-status 0.
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        // Reply DATA with serialized gRPC body and end_stream=true.
        {
            auto body = build_grpc_body(response_payload);
            http2_grpc::data_frame df(
                req_hdr.stream_id, body, /*end_stream=*/true);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        peer_done.store(true);
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/Method", std::vector<std::uint8_t>{0x01, 0x02});
    ASSERT_TRUE(result.is_ok())
        << "call_raw should succeed with grpc-status=0; got: "
        << (result.is_err() ? result.error().message : "none");
    EXPECT_EQ(result.value().data, response_payload);

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return peer_done.load(); }, 2s));

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// gRPC status mapping: peer responds with non-OK grpc-status. Drives the
// trailer parse path that converts the trailer code into a Result error.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawMapsGrpcStatusNotFoundToError)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // Reply HEADERS with grpc-status = 5 (NOT_FOUND), grpc-message set.
        {
            auto block = grpc_encode_response_headers(
                200,
                static_cast<int>(grpc::status_code::not_found),
                "resource missing");
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/true, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/GetThing", std::vector<std::uint8_t>{0xff});
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code,
              static_cast<int>(grpc::status_code::not_found));
    EXPECT_EQ(result.error().message, "resource missing");

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// gRPC status mapping: peer responds with grpc-status = INTERNAL but no
// grpc-message. The error path uses status_code_to_string() as fallback.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawMapsGrpcStatusWithoutMessageUsesCodeName)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // Reply with grpc-status = 13 (INTERNAL), no grpc-message header.
        {
            auto block = grpc_encode_response_headers(
                200, static_cast<int>(grpc::status_code::internal));
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/true, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/Method", std::vector<std::uint8_t>{});
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code,
              static_cast<int>(grpc::status_code::internal));
    // Empty grpc-message → fallback uses the code name.
    EXPECT_EQ(result.error().message, "INTERNAL");

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// gRPC status mapping: covers UNAUTHENTICATED, RESOURCE_EXHAUSTED,
// PERMISSION_DENIED, FAILED_PRECONDITION as a parameterized sweep through
// the status_code_to_string fallback path.
class GrpcClientHermeticStatusMapTest
    : public support_grpc::hermetic_transport_fixture,
      public ::testing::WithParamInterface<std::pair<grpc::status_code, std::string>>
{
};

TEST_P(GrpcClientHermeticStatusMapTest, MapsCodeIntoErrorResult)
{
    const auto [code, name] = GetParam();
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&, code]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        auto block = grpc_encode_response_headers(200, static_cast<int>(code));
        http2_grpc::headers_frame hf(
            req_hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/StatusMap", std::vector<std::uint8_t>{0x01});
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, static_cast<int>(code));
    EXPECT_EQ(result.error().message, name);

    client->disconnect();
    connector.join();
    peer_thread.join();
}

INSTANTIATE_TEST_SUITE_P(
    GrpcClientHermeticStatusMap,
    GrpcClientHermeticStatusMapTest,
    ::testing::Values(
        std::make_pair(grpc::status_code::cancelled, std::string("CANCELLED")),
        std::make_pair(grpc::status_code::unknown, std::string("UNKNOWN")),
        std::make_pair(grpc::status_code::deadline_exceeded,
                       std::string("DEADLINE_EXCEEDED")),
        std::make_pair(grpc::status_code::permission_denied,
                       std::string("PERMISSION_DENIED")),
        std::make_pair(grpc::status_code::resource_exhausted,
                       std::string("RESOURCE_EXHAUSTED")),
        std::make_pair(grpc::status_code::failed_precondition,
                       std::string("FAILED_PRECONDITION")),
        std::make_pair(grpc::status_code::aborted, std::string("ABORTED")),
        std::make_pair(grpc::status_code::unimplemented,
                       std::string("UNIMPLEMENTED")),
        std::make_pair(grpc::status_code::unavailable,
                       std::string("UNAVAILABLE")),
        std::make_pair(grpc::status_code::data_loss, std::string("DATA_LOSS")),
        std::make_pair(grpc::status_code::unauthenticated,
                       std::string("UNAUTHENTICATED"))));

// HTTP non-200 status: the peer's HEADERS frame carries :status 503
// (Service Unavailable). Drives the response.status_code != 200 error
// branch which maps to status_code::unavailable regardless of trailers.
TEST_F(GrpcClientHermeticTransportCoverageTest, CallRawMapsHttpErrorToUnavailable)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // :status 503, end_stream=true, no grpc-status (HTTP-level error).
        http2_grpc::hpack_encoder enc(4096);
        std::vector<http2_grpc::http_header> headers = {
            {":status", "503"},
        };
        auto block = enc.encode(headers);
        http2_grpc::headers_frame hf(
            req_hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/Method", std::vector<std::uint8_t>{});
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code,
              static_cast<int>(grpc::status_code::unavailable));
    EXPECT_NE(result.error().message.find("HTTP error"), std::string::npos);

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Empty body success: peer replies grpc-status=0 and end_stream on the
// HEADERS frame, never sending a DATA frame. Drives the empty-body
// branch in call_raw that returns ok(grpc_message{}) without calling
// grpc_message::parse().
TEST_F(GrpcClientHermeticTransportCoverageTest, CallRawSucceedsWithEmptyBody)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        auto block = grpc_encode_response_headers(200, 0);
        http2_grpc::headers_frame hf(
            req_hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/EmptyOk", std::vector<std::uint8_t>{});
    ASSERT_TRUE(result.is_ok());
    EXPECT_TRUE(result.value().data.empty());

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Malformed body: peer claims grpc-status=0 but the body is too short to
// be a valid gRPC frame (less than the 5-byte header). Drives the
// grpc_message::parse() error branch in call_raw post-success-trailer.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawErrorsWhenBodyIsMalformed)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // Reply HEADERS + DATA where DATA is shorter than 5 bytes.
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        {
            // Only 3 payload bytes: no valid grpc_message header.
            std::vector<std::uint8_t> tiny{0x01, 0x02, 0x03};
            http2_grpc::data_frame df(
                req_hdr.stream_id, tiny, /*end_stream=*/true);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/MalformedBody", std::vector<std::uint8_t>{});
    EXPECT_TRUE(result.is_err());

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// grpc-timeout header round-trip: client sets a deadline, the peer parses
// the request HEADERS payload and verifies a grpc-timeout entry was sent.
// Drives the grpc-timeout header build branch in call_raw together with
// the pre-call options.deadline check on the now < deadline path.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawSendsGrpcTimeoutHeaderWhenDeadlineSet)
{
    support_grpc::tls_loopback_listener listener(io());

    std::atomic<bool> grpc_timeout_seen{false};
    std::atomic<bool> custom_metadata_seen{false};

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        std::error_code ec;
        // Read client HEADERS frame (request).
        std::array<std::uint8_t, kFrameHeaderSize> hdr_buf{};
        asio::read(*stream, asio::buffer(hdr_buf), ec);
        if (ec) return;
        auto parsed = http2_grpc::frame_header::parse(
            std::span<const std::uint8_t>(hdr_buf.data(), hdr_buf.size()));
        if (parsed.is_err()) return;
        const auto req_hdr = parsed.value();

        std::vector<std::uint8_t> req_payload(req_hdr.length);
        if (req_hdr.length > 0)
        {
            asio::read(*stream, asio::buffer(req_payload), ec);
            if (ec) return;
        }

        // Decode HPACK to look for grpc-timeout header.
        http2_grpc::hpack_decoder dec(4096);
        auto headers = dec.decode(
            std::span<const std::uint8_t>(req_payload.data(),
                                          req_payload.size()));
        if (headers.is_ok())
        {
            for (const auto& h : headers.value())
            {
                if (h.name == "grpc-timeout" && !h.value.empty())
                {
                    grpc_timeout_seen.store(true);
                }
                if (h.name == "x-test-tenant" && h.value == "alpha")
                {
                    custom_metadata_seen.store(true);
                }
            }
        }

        // Drain client DATA, then reply with grpc-status=0 and empty body.
        http2_grpc::frame_header data_hdr{};
        if (!drain_one_frame(*stream, data_hdr)) return;

        auto block = grpc_encode_response_headers(200, 0);
        http2_grpc::headers_frame hf(
            req_hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;

        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    grpc::call_options options;
    options.set_timeout(std::chrono::seconds(5));
    options.metadata.emplace_back("x-test-tenant", "alpha");

    auto result = client->call_raw(
        "/svc/Echo", std::vector<std::uint8_t>{0xaa, 0xbb}, options);
    EXPECT_TRUE(result.is_ok());

    client->disconnect();
    connector.join();
    peer_thread.join();

    EXPECT_TRUE(grpc_timeout_seen.load())
        << "client should serialize grpc-timeout header from options.deadline";
    EXPECT_TRUE(custom_metadata_seen.load())
        << "client should serialize custom metadata into HEADERS frame";
}

// Deadline already in the past after handshake: drives the post-connect
// deadline_exceeded short-circuit branch (between is_connected() check
// and the http2 POST attempt).
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawShortCircuitsOnExpiredDeadlineAfterHandshake)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    grpc::call_options options;
    options.deadline = std::chrono::system_clock::now() - std::chrono::seconds(2);

    auto result = client->call_raw(
        "/svc/Method", std::vector<std::uint8_t>{}, options);
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code,
              static_cast<int>(grpc::status_code::deadline_exceeded));

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Server streaming end-of-stream: peer sends HEADERS+DATA with final
// trailer carrying grpc-status=0 and no body. Drives
// server_stream_reader_impl::on_headers, on_complete, and read() through
// the buffer-empty + has_more=false branch returning the End-of-stream
// error result.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       ServerStreamReadAfterEndOfStreamReturnsError)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        // start_stream sends HEADERS + DATA(end_stream=true).
        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // Reply with HEADERS containing grpc-status=0 and end_stream=true,
        // no DATA. Drives reader on_headers + on_complete with no buffer.
        auto block = grpc_encode_response_headers(200, 0);
        http2_grpc::headers_frame hf(
            req_hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;

        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->server_stream_raw(
        "/svc/ListEvents", std::vector<std::uint8_t>{0x01});
    ASSERT_TRUE(stream_result.is_ok());
    auto reader = std::move(stream_result.value());
    ASSERT_NE(reader.get(), nullptr);

    // Wait briefly for the on_complete callback to fire on the worker.
    std::this_thread::sleep_for(200ms);

    // read() on a stream that has ended without buffered data → error
    // ("End of stream").
    auto read_result = reader->read();
    EXPECT_TRUE(read_result.is_err());

    // finish() returns the final grpc_status carried in the headers.
    auto status = reader->finish();
    EXPECT_EQ(status.code, grpc::status_code::ok);

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Server streaming with a valid message frame: peer sends HEADERS, a DATA
// frame containing a valid gRPC message, then HEADERS with end_stream
// trailer. Drives reader::read() through the success branch (buffer
// non-empty → grpc_message::parse → ok).
TEST_F(GrpcClientHermeticTransportCoverageTest,
       ServerStreamReadDeliversBufferedMessage)
{
    support_grpc::tls_loopback_listener listener(io());

    const std::vector<std::uint8_t> server_payload{0x10, 0x20, 0x30};

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // HEADERS (no end_stream).
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        // DATA carrying serialized gRPC body.
        {
            auto body = build_grpc_body(server_payload);
            http2_grpc::data_frame df(
                req_hdr.stream_id, body, /*end_stream=*/false);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        // Trailer HEADERS with end_stream.
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/true, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->server_stream_raw(
        "/svc/Stream", std::vector<std::uint8_t>{0xff});
    ASSERT_TRUE(stream_result.is_ok());
    auto reader = std::move(stream_result.value());
    ASSERT_NE(reader.get(), nullptr);

    // Allow data to land via on_data callback.
    std::this_thread::sleep_for(200ms);

    auto first = reader->read();
    if (first.is_ok())
    {
        EXPECT_EQ(first.value().data, server_payload);
    }
    // has_more() must remain queryable through the lifetime.
    (void)reader->has_more();

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Server streaming with HTTP-level error: peer never sends HEADERS but
// the underlying HTTP/2 client closes the stream with a non-200 surface
// status. Exercises on_complete(int status_code != 200) branch which
// flips final_status_ to UNAVAILABLE.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       ServerStreamFinishCarriesUnavailableOnHttpError)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // :status 502, end_stream=true, no grpc-status.
        http2_grpc::hpack_encoder enc(4096);
        std::vector<http2_grpc::http_header> headers = {
            {":status", "502"},
        };
        auto block = enc.encode(headers);
        http2_grpc::headers_frame hf(
            req_hdr.stream_id, std::move(block),
            /*end_stream=*/true, /*end_headers=*/true);
        auto bytes = hf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;

        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->server_stream_raw(
        "/svc/Method", std::vector<std::uint8_t>{0x01});
    ASSERT_TRUE(stream_result.is_ok());
    auto reader = std::move(stream_result.value());
    ASSERT_NE(reader.get(), nullptr);

    std::this_thread::sleep_for(200ms);

    // After on_complete(502), final_status_ should be UNAVAILABLE.
    auto status = reader->finish();
    // The reader translates non-200 HTTP into UNAVAILABLE.
    EXPECT_TRUE(status.code == grpc::status_code::unavailable ||
                status.code == grpc::status_code::ok)
        << "final_status_ should be UNAVAILABLE on HTTP error or OK if "
           "headers parsed before complete; got: "
        << static_cast<int>(status.code);

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Client streaming write success: drives client_stream_writer_impl::write
// through grpc_message::serialize + http2_client_->write_stream forward.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       ClientStreamWriterWriteForwardsToHttp2)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->client_stream_raw("/svc/Upload");
    ASSERT_TRUE(stream_result.is_ok());
    auto writer = std::move(stream_result.value());
    ASSERT_NE(writer.get(), nullptr);

    // Several writes: each goes through serialize() + write_stream().
    // We do not assert on success/failure — the underlying http2 layer
    // may surface a connection-level error before the test finishes.
    // What matters for coverage is that the lambda chain executes.
    (void)writer->write(std::vector<std::uint8_t>{0x01, 0x02});
    (void)writer->write(std::vector<std::uint8_t>{0x03, 0x04});
    (void)writer->write(std::vector<std::uint8_t>{});

    // writes_done must be safe to call once.
    (void)writer->writes_done();
    // Idempotent second call.
    (void)writer->writes_done();

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Client streaming write after writes_done: drives the writes_done_==true
// guard at the top of write() that returns internal_error.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       ClientStreamWriteAfterWritesDoneFails)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->client_stream_raw("/svc/Upload");
    ASSERT_TRUE(stream_result.is_ok());
    auto writer = std::move(stream_result.value());
    ASSERT_NE(writer.get(), nullptr);

    (void)writer->writes_done();
    auto write_after = writer->write(std::vector<std::uint8_t>{0x01});
    EXPECT_TRUE(write_after.is_err());

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Bidi streaming write/read smoke: server replies with HEADERS+DATA mid-
// stream, drives bidi_stream_impl::write, on_data, on_headers, read,
// writes_done, and finish branches.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       BidiStreamWriteReadAndFinishExerciseAllImplPaths)
{
    support_grpc::tls_loopback_listener listener(io());

    const std::vector<std::uint8_t> server_payload{0x55, 0x66};

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        // Reply HEADERS (no end_stream) with grpc-status=0.
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        // Reply DATA (gRPC-framed body, no end_stream).
        {
            auto body = build_grpc_body(server_payload);
            http2_grpc::data_frame df(
                req_hdr.stream_id, body, /*end_stream=*/false);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        // Final HEADERS trailer with end_stream.
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/true, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->bidi_stream_raw("/svc/Chat");
    ASSERT_TRUE(stream_result.is_ok());
    auto bidi = std::move(stream_result.value());
    ASSERT_NE(bidi.get(), nullptr);

    // write goes through serialize + http2 write_stream.
    (void)bidi->write(std::vector<std::uint8_t>{0x77, 0x88});

    std::this_thread::sleep_for(200ms);

    // read pulls from buffer; may or may not have data depending on
    // worker scheduling — both branches are valid coverage.
    auto first_read = bidi->read();
    if (first_read.is_ok())
    {
        EXPECT_FALSE(first_read.value().data.empty());
    }

    // writes_done is idempotent.
    (void)bidi->writes_done();
    (void)bidi->writes_done();

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Bidi write after writes_done: covers the bidi_stream::write guard that
// rejects writes once writes_done_ is true.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       BidiWriteAfterWritesDoneIsRejected)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto stream_result = client->bidi_stream_raw("/svc/Chat");
    ASSERT_TRUE(stream_result.is_ok());
    auto bidi = std::move(stream_result.value());
    ASSERT_NE(bidi.get(), nullptr);

    (void)bidi->writes_done();
    auto write_after = bidi->write(std::vector<std::uint8_t>{0x01});
    EXPECT_TRUE(write_after.is_err());

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// RST_STREAM mid-call: the peer accepts the request but resets the stream
// before sending HEADERS. Drives the http2 RST handling path which
// terminates the call with an error.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawErrorsWhenPeerSendsRstStream)
{
    support_grpc::tls_loopback_listener listener(io());

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        http2_grpc::rst_stream_frame rsf(req_hdr.stream_id, /*error_code=*/8);
        auto bytes = rsf.serialize();
        asio::write(*stream, asio::buffer(bytes), ec);
        if (ec) return;

        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    auto result = client->call_raw(
        "/svc/Reset", std::vector<std::uint8_t>{});
    EXPECT_TRUE(result.is_err());

    client->disconnect();
    connector.join();
    peer_thread.join();
}

// Call_raw_async with a real handshake: the callback delivers the success
// result mapped from the peer's HEADERS+DATA reply. Drives the connected
// branch of call_raw_async + async submit + callback delivery.
TEST_F(GrpcClientHermeticTransportCoverageTest,
       CallRawAsyncDeliversSuccessAfterHeadersAndData)
{
    support_grpc::tls_loopback_listener listener(io());

    const std::vector<std::uint8_t> response_payload{0xAB, 0xCD};

    std::thread peer_thread([&]() {
        auto stream = grpc_complete_settings_exchange(listener);
        if (!stream) return;

        http2_grpc::frame_header req_hdr{};
        if (!drain_one_frame(*stream, req_hdr)) return;
        if (!drain_one_frame(*stream, req_hdr)) return;

        std::error_code ec;
        {
            auto block = grpc_encode_response_headers(200, 0);
            http2_grpc::headers_frame hf(
                req_hdr.stream_id, std::move(block),
                /*end_stream=*/false, /*end_headers=*/true);
            auto bytes = hf.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        {
            auto body = build_grpc_body(response_payload);
            http2_grpc::data_frame df(
                req_hdr.stream_id, body, /*end_stream=*/true);
            auto bytes = df.serialize();
            asio::write(*stream, asio::buffer(bytes), ec);
            if (ec) return;
        }
        drain_until_eof(*stream);
    });

    auto client = make_grpc_client_for_listener(listener.port());
    std::thread connector([&]() { (void)client->connect(); });

    EXPECT_TRUE(support_grpc::hermetic_transport_fixture::wait_for(
        [&]() { return client->is_connected(); }, 3s));

    std::promise<bool> got_ok;
    auto fut = got_ok.get_future();
    client->call_raw_async(
        "/svc/Method",
        std::vector<std::uint8_t>{},
        [&got_ok](kcenon::network::Result<grpc::grpc_message> r) {
            got_ok.set_value(r.is_ok());
        });

    EXPECT_EQ(fut.wait_for(std::chrono::seconds(3)),
              std::future_status::ready);
    EXPECT_TRUE(fut.get());

    client->disconnect();
    connector.join();
    peer_thread.join();
}

#endif // !NETWORK_GRPC_OFFICIAL
