// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <benchmark/benchmark.h>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>

#include "kcenon/network/compatibility.h"
#include "kcenon/network/detail/metrics/histogram.h"
#include <container.h>

using namespace network_module;
using namespace container_module;
using namespace std::chrono_literals;

// Free function for yielding to allow async operations to complete
inline void wait_for_ready() {
    for (int i = 0; i < 1000; ++i) {
        std::this_thread::yield();
    }
}


// Global server for benchmarks
static std::shared_ptr<messaging_server> g_benchmark_server;
static unsigned short g_server_port = 0;

// Helper to find an available port
static unsigned short FindAvailablePort(unsigned short start = 6000) {
    for (unsigned short port = start; port < 65535; ++port) {
        try {
            asio::io_context io_context;
            asio::ip::tcp::acceptor acceptor(io_context);
            asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
            acceptor.open(endpoint.protocol());
            acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            acceptor.bind(endpoint);
            acceptor.close();
            return port;
        } catch (...) {
            // Port is in use, try next
        }
    }
    return 0;
}

// Setup and teardown for benchmarks
class NetworkBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        if (!g_benchmark_server && g_server_port == 0) {
            g_server_port = FindAvailablePort();
            if (g_server_port == 0) {
                state.SkipWithError("No available port found");
                return;
            }
            
            g_benchmark_server = std::make_shared<messaging_server>("benchmark_server");
            g_benchmark_server->start_server(g_server_port);
            
            // Give server time to start
            wait_for_ready();
        }
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // Server cleanup is done in main()
    }
};

// Connection benchmarks
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_ClientConnection)(benchmark::State& state) {
    for (auto _ : state) {
        asio::io_context io_context;
        auto client = std::make_shared<messaging_client>(
            io_context, 
            "bench_client", 
            "bench_key"
        );
        
        auto connected = client->connect("127.0.0.1", g_server_port);
        benchmark::DoNotOptimize(connected);
        
        if (connected) {
            client->disconnect();
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_ClientConnection);

// Message creation benchmarks
static void BM_MessageCreation(benchmark::State& state) {
    const int field_count = state.range(0);
    
    for (auto _ : state) {
        auto message = std::make_shared<value_container>();
        
        for (int i = 0; i < field_count; ++i) {
            message->add_value(std::make_shared<string_value>(
                "field_" + std::to_string(i),
                "value_" + std::to_string(i)
            ));
        }
        
        benchmark::DoNotOptimize(message);
    }
    
    state.SetItemsProcessed(state.iterations() * field_count);
}
BENCHMARK(BM_MessageCreation)
    ->RangeMultiplier(10)
    ->Range(1, 1000);

// Message sending benchmarks
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_MessageSending)(benchmark::State& state) {
    const int message_size = state.range(0);
    
    asio::io_context io_context;
    auto client = std::make_shared<messaging_client>(
        io_context, 
        "send_bench_client",
        "send_bench_key"
    );
    
    if (!client->connect("127.0.0.1", g_server_port)) {
        state.SkipWithError("Failed to connect to server");
        return;
    }
    
    // Pre-create message
    auto message = std::make_shared<value_container>();
    message->add_value(std::make_shared<string_value>("type", "benchmark"));
    
    // Add payload of specified size
    std::string payload(message_size, 'X');
    message->add_value(std::make_shared<string_value>("payload", payload));
    
    // Start io_context in background
    std::atomic<bool> stop_io{false};
    std::thread io_thread([&io_context, &stop_io]() {
        while (!stop_io) {
            io_context.run_one_for(10ms);
        }
    });
    
    for (auto _ : state) {
        auto sent = client->send(message);
        benchmark::DoNotOptimize(sent);
    }
    
    stop_io = true;
    io_thread.join();
    
    client->disconnect();
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * message_size);
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_MessageSending)
    ->RangeMultiplier(10)
    ->Range(100, 1000000);

// Round-trip latency benchmark with percentile tracking
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_RoundTripLatency)(benchmark::State& state) {
    using namespace kcenon::network;

    asio::io_context io_context;
    auto client = std::make_shared<messaging_client>(
        io_context,
        "latency_client",
        "latency_key"
    );

    if (!client->connect("127.0.0.1", g_server_port)) {
        state.SkipWithError("Failed to connect to server");
        return;
    }

    // Setup echo handler
    std::promise<void> response_promise;
    std::atomic<bool> waiting{false};

    client->set_message_handler([&](std::shared_ptr<value_container> msg) {
        if (waiting) {
            response_promise.set_value();
        }
    });

    // Start io_context in background
    std::atomic<bool> stop_io{false};
    std::thread io_thread([&io_context, &stop_io]() {
        while (!stop_io) {
            io_context.run_one_for(1ms);
        }
    });

    auto message = std::make_shared<value_container>();
    message->add_value(std::make_shared<string_value>("type", "echo"));
    message->add_value(std::make_shared<string_value>("data", "test"));

    // Histogram for latency percentile tracking (microseconds)
    histogram latency_hist(histogram_config::default_latency_config());

    for (auto _ : state) {
        response_promise = std::promise<void>();
        auto response_future = response_promise.get_future();

        auto start = std::chrono::high_resolution_clock::now();

        waiting = true;
        client->send(message);

        // Wait for response (with timeout)
        if (response_future.wait_for(100ms) == std::future_status::ready) {
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            state.SetIterationTime(elapsed_us / 1e6);
            latency_hist.record(static_cast<double>(elapsed_us));
        } else {
            state.SkipWithError("Timeout waiting for response");
            break;
        }

        waiting = false;
    }

    // Report latency percentiles as custom counters (in microseconds)
    if (latency_hist.count() > 0) {
        state.counters["p50_us"] = latency_hist.p50();
        state.counters["p95_us"] = latency_hist.p95();
        state.counters["p99_us"] = latency_hist.p99();
    }

    stop_io = true;
    io_thread.join();

    client->disconnect();
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_RoundTripLatency)
    ->UseManualTime();

// Concurrent connections benchmark
static void BM_ConcurrentConnections(benchmark::State& state) {
    const int connection_count = state.range(0);
    
    for (auto _ : state) {
        std::vector<asio::io_context> io_contexts(connection_count);
        std::vector<std::shared_ptr<messaging_client>> clients;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Create and connect all clients
        for (int i = 0; i < connection_count; ++i) {
            auto client = std::make_shared<messaging_client>(
                io_contexts[i],
                "concurrent_" + std::to_string(i),
                "key_" + std::to_string(i)
            );
            
            if (client->connect("127.0.0.1", g_server_port)) {
                clients.push_back(client);
            }
        }
        
        // Disconnect all
        for (auto& client : clients) {
            client->disconnect();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        state.SetIterationTime(elapsed.count() / 1e6);
    }
    
    state.SetItemsProcessed(state.iterations() * connection_count);
}
BENCHMARK(BM_ConcurrentConnections)
    ->UseManualTime()
    ->RangeMultiplier(2)
    ->Range(1, 64);

// Throughput benchmark
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_MessageThroughput)(benchmark::State& state) {
    const int batch_size = state.range(0);
    
    asio::io_context io_context;
    auto client = std::make_shared<messaging_client>(
        io_context, 
        "throughput_client",
        "throughput_key"
    );
    
    if (!client->connect("127.0.0.1", g_server_port)) {
        state.SkipWithError("Failed to connect to server");
        return;
    }
    
    // Pre-create messages
    std::vector<std::shared_ptr<value_container>> messages;
    for (int i = 0; i < batch_size; ++i) {
        auto message = std::make_shared<value_container>();
        message->add_value(std::make_shared<string_value>("type", "throughput"));
        message->add_value(std::make_shared<int32_value>("sequence", i));
        message->add_value(std::make_shared<string_value>("data", "payload_data"));
        messages.push_back(message);
    }
    
    // Start io_context in background
    std::atomic<bool> stop_io{false};
    std::thread io_thread([&io_context, &stop_io]() {
        while (!stop_io) {
            io_context.run_one_for(1ms);
        }
    });
    
    for (auto _ : state) {
        int sent_count = 0;
        
        for (const auto& message : messages) {
            if (client->send(message)) {
                sent_count++;
            }
        }
        
        benchmark::DoNotOptimize(sent_count);
    }
    
    stop_io = true;
    io_thread.join();
    
    client->disconnect();
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_MessageThroughput)
    ->RangeMultiplier(10)
    ->Range(1, 1000);

// Message serialization benchmark
static void BM_MessageSerialization(benchmark::State& state) {
    const int field_count = state.range(0);
    
    // Create a complex message
    auto message = std::make_shared<value_container>();
    
    for (int i = 0; i < field_count; ++i) {
        message->add_value(std::make_shared<string_value>(
            "string_" + std::to_string(i),
            "value_" + std::to_string(i)
        ));
        message->add_value(std::make_shared<int32_value>(
            "int_" + std::to_string(i),
            i * 42
        ));
        message->add_value(std::make_shared<double_value>(
            "double_" + std::to_string(i),
            i * 3.14159
        ));
    }
    
    for (auto _ : state) {
        auto serialized = message->serialize();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations() * field_count * 3); // 3 fields per iteration
}
BENCHMARK(BM_MessageSerialization)
    ->RangeMultiplier(10)
    ->Range(1, 100);

// Message deserialization benchmark
static void BM_MessageDeserialization(benchmark::State& state) {
    const int field_count = state.range(0);
    
    // Create and serialize a message
    auto original = std::make_shared<value_container>();
    
    for (int i = 0; i < field_count; ++i) {
        original->add_value(std::make_shared<string_value>(
            "field_" + std::to_string(i),
            "value_" + std::to_string(i)
        ));
    }
    
    auto serialized = original->serialize();
    
    for (auto _ : state) {
        value_container deserialized;
        auto result = deserialized.deserialize(serialized);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations() * field_count);
    state.SetBytesProcessed(state.iterations() * serialized.size());
}
BENCHMARK(BM_MessageDeserialization)
    ->RangeMultiplier(10)
    ->Range(1, 100);

// Memory allocation benchmark
static void BM_ClientAllocation(benchmark::State& state) {
    for (auto _ : state) {
        asio::io_context io_context;
        auto client = std::make_shared<messaging_client>(
            io_context,
            "alloc_client",
            "alloc_key"
        );
        benchmark::DoNotOptimize(client);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ClientAllocation);

// Multi-threaded stress test
static void BM_MultiThreadedStress(benchmark::State& state) {
    const int thread_count = state.range(0);
    const int messages_per_thread = 100;
    
    for (auto _ : state) {
        std::vector<std::thread> threads;
        std::atomic<int> total_sent{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                asio::io_context io_context;
                auto client = std::make_shared<messaging_client>(
                    io_context,
                    "stress_" + std::to_string(t),
                    "key_" + std::to_string(t)
                );
                
                if (client->connect("127.0.0.1", g_server_port)) {
                    // Start io processing
                    std::thread io_thread([&io_context]() {
                        io_context.run_for(500ms);
                    });
                    
                    // Send messages
                    for (int i = 0; i < messages_per_thread; ++i) {
                        auto message = std::make_shared<value_container>();
                        message->add_value(std::make_shared<string_value>("thread", std::to_string(t)));
                        message->add_value(std::make_shared<int32_value>("msg", i));
                        
                        if (client->send(message)) {
                            total_sent++;
                        }
                    }
                    
                    io_thread.join();
                    client->disconnect();
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        state.SetIterationTime(elapsed.count() / 1e3);
        benchmark::DoNotOptimize(total_sent.load());
    }
    
    state.SetItemsProcessed(state.iterations() * thread_count * messages_per_thread);
}
BENCHMARK(BM_MultiThreadedStress)
    ->UseManualTime()
    ->RangeMultiplier(2)
    ->Range(1, 16);

// Compression efficiency benchmark (if compression is enabled)
static void BM_CompressionEfficiency(benchmark::State& state) {
    const int data_size = state.range(0);
    
    // Create a message with compressible data
    auto message = std::make_shared<value_container>();
    
    // Add repetitive data (highly compressible)
    std::string repetitive_data;
    for (int i = 0; i < data_size / 10; ++i) {
        repetitive_data += "COMPRESS_ME";
    }
    message->add_value(std::make_shared<string_value>("data", repetitive_data));
    
    asio::io_context io_context;
    auto client = std::make_shared<messaging_client>(
        io_context, 
        "compress_client",
        "compress_key"
    );
    
    if (!client->connect("127.0.0.1", g_server_port)) {
        state.SkipWithError("Failed to connect to server");
        return;
    }
    
    // Start io_context in background
    std::atomic<bool> stop_io{false};
    std::thread io_thread([&io_context, &stop_io]() {
        while (!stop_io) {
            io_context.run_one_for(1ms);
        }
    });
    
    for (auto _ : state) {
        auto sent = client->send(message);
        benchmark::DoNotOptimize(sent);
    }
    
    stop_io = true;
    io_thread.join();
    
    client->disconnect();
    
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * data_size);
}
BENCHMARK(BM_CompressionEfficiency)
    ->RangeMultiplier(10)
    ->Range(1000, 100000);

// Message size latency benchmark with percentile tracking
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_MessageSizeLatency)(benchmark::State& state) {
    using namespace kcenon::network;
    const int payload_size = state.range(0);

    asio::io_context io_context;
    auto client = std::make_shared<messaging_client>(
        io_context,
        "size_latency_client",
        "size_latency_key"
    );

    if (!client->connect("127.0.0.1", g_server_port)) {
        state.SkipWithError("Failed to connect to server");
        return;
    }

    std::atomic<bool> stop_io{false};
    std::thread io_thread([&io_context, &stop_io]() {
        while (!stop_io) {
            io_context.run_one_for(1ms);
        }
    });

    // Create message with specified payload size
    auto message = std::make_shared<value_container>();
    message->add_value(std::make_shared<string_value>("type", "latency_test"));
    std::string payload(payload_size, 'A');
    message->add_value(std::make_shared<string_value>("payload", payload));

    histogram latency_hist(histogram_config::default_latency_config());

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        auto sent = client->send(message);
        auto end = std::chrono::high_resolution_clock::now();

        if (sent) {
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            state.SetIterationTime(elapsed_us / 1e6);
            latency_hist.record(static_cast<double>(elapsed_us));
        }

        benchmark::DoNotOptimize(sent);
    }

    if (latency_hist.count() > 0) {
        state.counters["p50_us"] = latency_hist.p50();
        state.counters["p95_us"] = latency_hist.p95();
        state.counters["p99_us"] = latency_hist.p99();
        state.counters["payload_bytes"] = payload_size;
    }

    stop_io = true;
    io_thread.join();

    client->disconnect();

    state.SetBytesProcessed(state.iterations() * payload_size);
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_MessageSizeLatency)
    ->UseManualTime()
    ->Arg(100)       // 100 B
    ->Arg(1024)      // 1 KB
    ->Arg(10240)     // 10 KB
    ->Arg(102400);   // 100 KB

// Connection setup time benchmark with percentile tracking
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_ConnectionSetupLatency)(benchmark::State& state) {
    using namespace kcenon::network;

    histogram setup_hist(histogram_config::default_latency_config());

    for (auto _ : state) {
        asio::io_context io_context;
        auto client = std::make_shared<messaging_client>(
            io_context,
            "setup_bench_client",
            "setup_bench_key"
        );

        auto start = std::chrono::high_resolution_clock::now();
        auto connected = client->connect("127.0.0.1", g_server_port);
        auto end = std::chrono::high_resolution_clock::now();

        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        state.SetIterationTime(elapsed_us / 1e6);
        setup_hist.record(static_cast<double>(elapsed_us));

        benchmark::DoNotOptimize(connected);

        if (connected) {
            client->disconnect();
        }
    }

    if (setup_hist.count() > 0) {
        state.counters["p50_us"] = setup_hist.p50();
        state.counters["p95_us"] = setup_hist.p95();
        state.counters["p99_us"] = setup_hist.p99();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_ConnectionSetupLatency)
    ->UseManualTime();

// Throughput baseline validation benchmark
// Measures sustained message throughput (messages/second) across batch sizes
BENCHMARK_DEFINE_F(NetworkBenchmarkFixture, BM_ThroughputBaseline)(benchmark::State& state) {
    const int batch_size = state.range(0);

    asio::io_context io_context;
    auto client = std::make_shared<messaging_client>(
        io_context,
        "throughput_baseline_client",
        "throughput_baseline_key"
    );

    if (!client->connect("127.0.0.1", g_server_port)) {
        state.SkipWithError("Failed to connect to server");
        return;
    }

    // Pre-create lightweight messages
    std::vector<std::shared_ptr<value_container>> messages;
    for (int i = 0; i < batch_size; ++i) {
        auto msg = std::make_shared<value_container>();
        msg->add_value(std::make_shared<string_value>("type", "throughput_baseline"));
        msg->add_value(std::make_shared<int32_value>("seq", i));
        messages.push_back(msg);
    }

    std::atomic<bool> stop_io{false};
    std::thread io_thread([&io_context, &stop_io]() {
        while (!stop_io) {
            io_context.run_one_for(1ms);
        }
    });

    int64_t total_sent = 0;

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        int sent_count = 0;

        for (const auto& msg : messages) {
            if (client->send(msg)) {
                ++sent_count;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_s = std::chrono::duration<double>(end - start).count();
        state.SetIterationTime(elapsed_s);
        total_sent += sent_count;

        // Report throughput as messages/second
        if (elapsed_s > 0) {
            state.counters["msgs_per_sec"] = sent_count / elapsed_s;
        }
    }

    stop_io = true;
    io_thread.join();
    client->disconnect();

    state.SetItemsProcessed(total_sent);
}
BENCHMARK_REGISTER_F(NetworkBenchmarkFixture, BM_ThroughputBaseline)
    ->UseManualTime()
    ->Arg(10)
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000);

// Compression impact on throughput benchmark
// Compares throughput of compressible vs random data
static void BM_CompressionThroughputImpact(benchmark::State& state) {
    const bool use_compressible = (state.range(0) == 1);
    const int message_count = 100;

    for (auto _ : state) {
        std::vector<std::shared_ptr<value_container>> messages;

        for (int i = 0; i < message_count; ++i) {
            auto msg = std::make_shared<value_container>();
            msg->add_value(std::make_shared<string_value>("type", "compress_test"));

            if (use_compressible) {
                // Highly compressible: repeating pattern
                std::string data(1000, 'A');
                msg->add_value(std::make_shared<string_value>("payload", data));
            } else {
                // Low compressibility: pseudo-random data
                std::string data(1000, '\0');
                for (size_t j = 0; j < data.size(); ++j) {
                    data[j] = static_cast<char>((i * 31 + j * 17) % 256);
                }
                msg->add_value(std::make_shared<string_value>("payload", data));
            }

            messages.push_back(msg);
        }

        // Serialize all messages and measure total size
        size_t total_bytes = 0;
        for (const auto& msg : messages) {
            auto serialized = msg->serialize();
            total_bytes += serialized.size();
            benchmark::DoNotOptimize(serialized);
        }

        state.counters["total_bytes"] = total_bytes;
        state.counters["avg_msg_bytes"] = total_bytes / message_count;
    }

    state.SetItemsProcessed(state.iterations() * message_count);
}
BENCHMARK(BM_CompressionThroughputImpact)
    ->Arg(0)  // Random data (low compressibility)
    ->Arg(1); // Repeating data (high compressibility)

// Serialization throughput comparison: small vs large messages
static void BM_SerializationThroughput(benchmark::State& state) {
    const int payload_size = state.range(0);

    // Create message with specified payload
    auto message = std::make_shared<value_container>();
    message->add_value(std::make_shared<string_value>("type", "serial_throughput"));
    std::string payload(payload_size, 'X');
    message->add_value(std::make_shared<string_value>("payload", payload));

    size_t serialized_size = 0;

    for (auto _ : state) {
        auto serialized = message->serialize();
        serialized_size = serialized.size();
        benchmark::DoNotOptimize(serialized);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * serialized_size);
    state.counters["serialized_bytes"] = serialized_size;
    state.counters["payload_bytes"] = payload_size;
    if (payload_size > 0) {
        state.counters["overhead_ratio"] =
            static_cast<double>(serialized_size) / payload_size;
    }
}
BENCHMARK(BM_SerializationThroughput)
    ->Arg(64)       // 64 B
    ->Arg(512)      // 512 B
    ->Arg(4096)     // 4 KB
    ->Arg(65536)    // 64 KB
    ->Arg(1048576); // 1 MB

// Batch size impact on throughput
// Measures how batch size affects per-message overhead
static void BM_BatchSizeOverhead(benchmark::State& state) {
    const int batch_size = state.range(0);

    // Pre-create batch of messages
    std::vector<std::shared_ptr<value_container>> batch;
    for (int i = 0; i < batch_size; ++i) {
        auto msg = std::make_shared<value_container>();
        msg->add_value(std::make_shared<string_value>("type", "batch_overhead"));
        msg->add_value(std::make_shared<int32_value>("seq", i));
        msg->add_value(std::make_shared<string_value>("data", "benchmark_payload_data"));
        batch.push_back(msg);
    }

    for (auto _ : state) {
        size_t total_bytes = 0;

        for (const auto& msg : batch) {
            auto serialized = msg->serialize();
            total_bytes += serialized.size();
            benchmark::DoNotOptimize(serialized);
        }

        state.counters["bytes_per_msg"] =
            static_cast<double>(total_bytes) / batch_size;
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_BatchSizeOverhead)
    ->Arg(1)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100)
    ->Arg(500);

int main(int argc, char** argv) {
    // Initialize benchmark
    ::benchmark::Initialize(&argc, argv);
    
    // Start global server for benchmarks
    g_server_port = FindAvailablePort();
    if (g_server_port == 0) {
        std::cerr << "Error: No available port found for benchmark server\n";
        return 1;
    }
    
    g_benchmark_server = std::make_shared<messaging_server>("benchmark_server");
    g_benchmark_server->start_server(g_server_port);
    
    // Give server time to start
    wait_for_ready();
    
    std::cout << "Benchmark server started on port " << g_server_port << "\n";
    
    // Run benchmarks
    ::benchmark::RunSpecifiedBenchmarks();
    
    // Cleanup
    if (g_benchmark_server) {
        g_benchmark_server->stop_server();
        g_benchmark_server->wait_for_stop();
    }
    
    return 0;
}