// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file tcp_loopback_bench.cpp
 * @brief Real TCP loopback I/O benchmarks using ASIO
 *
 * Measures actual network I/O performance using a lightweight TCP echo server
 * on the loopback interface. Unlike synthetic benchmarks that measure CPU-only
 * operations, these exercise the full kernel TCP stack.
 *
 * Uses raw ASIO for minimal dependency footprint — no internal messaging APIs.
 *
 * Run: ./build/benchmarks/network_benchmarks --benchmark_filter=TCP
 */

#include <benchmark/benchmark.h>

#include <asio.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

// ============================================================================
// Echo Server
// ============================================================================

/**
 * @brief Lightweight TCP echo server for benchmarking.
 *
 * Runs in a background thread with its own io_context. Accepts connections
 * and echoes received data back. Uses asynchronous accept with synchronous
 * per-session echoing in dedicated threads for simplicity.
 */
class echo_server {
public:
    echo_server()
        : acceptor_(io_context_)
        , running_(false)
    {
    }

    ~echo_server()
    {
        stop();
    }

    /**
     * @brief Start the echo server on an OS-assigned port.
     * @return The assigned port number, or 0 on failure.
     */
    uint16_t start()
    {
        try
        {
            // Bind to port 0 to let OS assign an available port
            asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), 0);
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();

            port_ = acceptor_.local_endpoint().port();
            running_ = true;

            server_thread_ = std::thread([this]() { run(); });
            return port_;
        }
        catch (...)
        {
            return 0;
        }
    }

    void stop()
    {
        if (!running_.exchange(false))
        {
            return;
        }

        asio::error_code ec;
        acceptor_.close(ec);
        io_context_.stop();

        if (server_thread_.joinable())
        {
            server_thread_.join();
        }
    }

    uint16_t port() const { return port_; }

private:
    void run()
    {
        while (running_)
        {
            try
            {
                asio::ip::tcp::socket socket(io_context_);
                acceptor_.accept(socket);

                if (!running_)
                {
                    break;
                }

                // Handle session in a detached thread to allow concurrent
                // connections. Each session echoes until the client disconnects.
                std::thread([this, s = std::move(socket)]() mutable {
                    handle_session(std::move(s));
                }).detach();
            }
            catch (const asio::system_error&)
            {
                // Acceptor closed during stop() — expected
                break;
            }
        }
    }

    void handle_session(asio::ip::tcp::socket socket)
    {
        try
        {
            std::vector<char> buffer(65536);
            asio::error_code ec;

            while (running_)
            {
                auto bytes_read = socket.read_some(
                    asio::buffer(buffer), ec);

                if (ec == asio::error::eof || ec == asio::error::connection_reset)
                {
                    break;
                }
                if (ec)
                {
                    break;
                }

                asio::write(socket, asio::buffer(buffer.data(), bytes_read), ec);
                if (ec)
                {
                    break;
                }
            }
        }
        catch (...)
        {
            // Session ended
        }
    }

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    uint16_t port_{0};
};

// ============================================================================
// Benchmark Fixture
// ============================================================================

/**
 * @brief Google Benchmark fixture managing the echo server lifecycle.
 *
 * The server is created per benchmark registration. SetUp/TearDown run
 * per benchmark function invocation (once per parameter combination).
 */
class TcpLoopbackFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State&) override
    {
        server_ = std::make_unique<echo_server>();
        port_ = server_->start();

        if (port_ == 0)
        {
            setup_failed_ = true;
            return;
        }

        // Brief wait for server to be ready to accept
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown(const ::benchmark::State&) override
    {
        if (server_)
        {
            server_->stop();
        }
    }

protected:
    std::unique_ptr<echo_server> server_;
    uint16_t port_{0};
    bool setup_failed_{false};
};

// ============================================================================
// BM_TCP_ConnectionEstablish
// Measures TCP connect + disconnect roundtrip on loopback.
// ============================================================================

BENCHMARK_DEFINE_F(TcpLoopbackFixture, TCP_ConnectionEstablish)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("Echo server setup failed");
        return;
    }

    asio::io_context io;
    asio::ip::tcp::endpoint endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_);

    for (auto _ : state)
    {
        asio::ip::tcp::socket socket(io);
        asio::error_code ec;

        socket.connect(endpoint, ec);
        if (ec)
        {
            state.SkipWithError("Connect failed");
            return;
        }

        socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket.close(ec);
        benchmark::ClobberMemory();
    }
}
BENCHMARK_REGISTER_F(TcpLoopbackFixture, TCP_ConnectionEstablish);

// ============================================================================
// BM_TCP_EchoRoundtrip
// Sends N bytes to echo server, reads them back, measures full roundtrip.
// ============================================================================

BENCHMARK_DEFINE_F(TcpLoopbackFixture, TCP_EchoRoundtrip)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("Echo server setup failed");
        return;
    }

    const auto payload_size = static_cast<size_t>(state.range(0));

    // Prepare payload
    std::vector<char> send_buf(payload_size);
    for (size_t i = 0; i < payload_size; ++i)
    {
        send_buf[i] = static_cast<char>(i & 0x7F);
    }
    std::vector<char> recv_buf(payload_size);

    // Establish a single persistent connection for roundtrip measurements
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::endpoint endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_);

    asio::error_code ec;
    socket.connect(endpoint, ec);
    if (ec)
    {
        state.SkipWithError("Connect failed");
        return;
    }

    // Disable Nagle's algorithm for accurate latency measurement
    socket.set_option(asio::ip::tcp::no_delay(true), ec);

    for (auto _ : state)
    {
        // Send
        asio::write(socket, asio::buffer(send_buf), ec);
        if (ec)
        {
            state.SkipWithError("Write failed");
            return;
        }

        // Receive exact echo
        asio::read(socket, asio::buffer(recv_buf), ec);
        if (ec)
        {
            state.SkipWithError("Read failed");
            return;
        }

        benchmark::DoNotOptimize(recv_buf.data());
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(payload_size) * 2);  // send + receive

    socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket.close(ec);
}
BENCHMARK_REGISTER_F(TcpLoopbackFixture, TCP_EchoRoundtrip)
    ->Arg(64)
    ->Arg(1024)
    ->Arg(8192);

// ============================================================================
// BM_TCP_StreamThroughput
// Sends data continuously and measures bytes/sec throughput.
// Uses a persistent connection and reports throughput metrics.
// ============================================================================

BENCHMARK_DEFINE_F(TcpLoopbackFixture, TCP_StreamThroughput)(benchmark::State& state)
{
    if (setup_failed_)
    {
        state.SkipWithError("Echo server setup failed");
        return;
    }

    const auto chunk_size = static_cast<size_t>(state.range(0));

    std::vector<char> send_buf(chunk_size, 'X');
    std::vector<char> recv_buf(chunk_size);

    // Establish persistent connection
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    asio::ip::tcp::endpoint endpoint(
        asio::ip::address::from_string("127.0.0.1"), port_);

    asio::error_code ec;
    socket.connect(endpoint, ec);
    if (ec)
    {
        state.SkipWithError("Connect failed");
        return;
    }

    socket.set_option(asio::ip::tcp::no_delay(true), ec);

    for (auto _ : state)
    {
        asio::write(socket, asio::buffer(send_buf), ec);
        if (ec)
        {
            state.SkipWithError("Write failed");
            return;
        }

        asio::read(socket, asio::buffer(recv_buf), ec);
        if (ec)
        {
            state.SkipWithError("Read failed");
            return;
        }

        benchmark::DoNotOptimize(recv_buf.data());
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) *
        static_cast<int64_t>(chunk_size) * 2);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));

    socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket.close(ec);
}
BENCHMARK_REGISTER_F(TcpLoopbackFixture, TCP_StreamThroughput)
    ->Arg(1024)      // 1 KB
    ->Arg(65536);    // 64 KB

}  // namespace
