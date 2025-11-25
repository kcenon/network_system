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
#include <kcenon/network/protocols/grpc/client.h>
#include <kcenon/network/protocols/grpc/server.h>

namespace grpc = network_system::protocols::grpc;

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
    grpc::grpc_client client("localhost:50051");

    auto result = client.connect();
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(client.is_connected());
}

TEST_F(GrpcClientTest, ConnectInvalidTarget)
{
    grpc::grpc_client client("invalid_target_no_port");

    auto result = client.connect();
    EXPECT_TRUE(result.is_err());
}

TEST_F(GrpcClientTest, Disconnect)
{
    grpc::grpc_client client("localhost:50051");
    client.connect();
    EXPECT_TRUE(client.is_connected());

    client.disconnect();
    EXPECT_FALSE(client.is_connected());
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
    grpc::grpc_client client1("localhost:50051");
    client1.connect();

    grpc::grpc_client client2(std::move(client1));
    EXPECT_EQ(client2.target(), "localhost:50051");
    EXPECT_TRUE(client2.is_connected());
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
