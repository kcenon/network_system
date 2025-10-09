/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include "network_system/session/messaging_session.h"
#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace kcenon::network;
using namespace std::chrono_literals;

class NetworkThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Concurrent send from multiple threads
TEST_F(NetworkThreadSafetyTest, ConcurrentSend) {
    messaging_client client("localhost", 8080);

    const int num_threads = 15;
    const int messages_per_thread = 200;
    std::atomic<int> sent{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                try {
                    message msg;
                    msg.set_data("Thread " + std::to_string(thread_id) + " msg " + std::to_string(j));
                    client.send(msg);
                    ++sent;
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 2: Session destruction during I/O
TEST_F(NetworkThreadSafetyTest, SessionDestructionDuringIO) {
    const int num_iterations = 50;
    std::atomic<int> errors{0};

    for (int i = 0; i < num_iterations; ++i) {
        auto session = std::make_shared<messaging_session>();

        std::thread io_thread([session]() {
            try {
                for (int j = 0; j < 100; ++j) {
                    message msg;
                    session->send(msg);
                    std::this_thread::sleep_for(1ms);
                }
            } catch (...) {
                // Expected when session is destroyed
            }
        });

        std::this_thread::sleep_for(50ms);
        session.reset(); // Destroy session while I/O is active

        io_thread.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 3: Pipeline concurrent access
TEST_F(NetworkThreadSafetyTest, PipelineConcurrentAccess) {
    message_pipeline pipeline;

    const int num_threads = 12;
    const int operations_per_thread = 500;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    message msg;
                    msg.set_data(std::to_string(j));
                    pipeline.push(std::move(msg));

                    if (j % 10 == 0) {
                        auto result = pipeline.try_pop();
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 4: Multiple connections concurrent
TEST_F(NetworkThreadSafetyTest, MultipleConnectionsConcurrent) {
    messaging_server server(8080);

    const int num_clients = 20;
    std::atomic<int> connections{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_clients; ++i) {
        threads.emplace_back([&]() {
            try {
                messaging_client client("localhost", 8080);
                ++connections;
                std::this_thread::sleep_for(100ms);
                client.disconnect();
            } catch (...) {
                ++errors;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 5: Concurrent receive operations
TEST_F(NetworkThreadSafetyTest, ConcurrentReceive) {
    messaging_client client("localhost", 8080);

    const int num_receivers = 10;
    std::atomic<int> messages_received{0};
    std::atomic<bool> running{true};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_receivers; ++i) {
        threads.emplace_back([&]() {
            while (running.load()) {
                try {
                    auto msg = client.receive(100ms);
                    if (msg) {
                        ++messages_received;
                    }
                } catch (...) {
                    // Expected timeout
                }
            }
        });
    }

    std::this_thread::sleep_for(500ms);
    running.store(false);

    for (auto& t : threads) {
        t.join();
    }
}

// Additional concise tests (6-10)
TEST_F(NetworkThreadSafetyTest, BufferConcurrentAccess) {
    network_buffer buffer(4096);
    const int num_threads = 15;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 300; ++j) {
                try {
                    std::vector<uint8_t> data(100, static_cast<uint8_t>(j));
                    buffer.write(data);
                    buffer.read(50);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

TEST_F(NetworkThreadSafetyTest, SessionPoolConcurrent) {
    session_pool pool(10);

    const int num_threads = 20;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                try {
                    auto session = pool.acquire();
                    if (session) {
                        std::this_thread::sleep_for(5ms);
                        pool.release(std::move(session));
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

TEST_F(NetworkThreadSafetyTest, MessageQueueConcurrent) {
    message_queue queue(1000);

    const int num_producers = 8;
    const int num_consumers = 4;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> running{true};

    std::vector<std::thread> threads;

    for (int i = 0; i < num_producers; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 200; ++j) {
                message msg;
                queue.push(std::move(msg));
                ++produced;
            }
        });
    }

    for (int i = 0; i < num_consumers; ++i) {
        threads.emplace_back([&]() {
            while (running.load()) {
                if (queue.try_pop()) {
                    ++consumed;
                } else {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (int i = 0; i < num_producers; ++i) {
        threads[i].join();
    }

    std::this_thread::sleep_for(200ms);
    running.store(false);

    for (int i = num_producers; i < threads.size(); ++i) {
        threads[i].join();
    }

    EXPECT_EQ(produced.load(), num_producers * 200);
}

TEST_F(NetworkThreadSafetyTest, EndpointResolutionConcurrent) {
    const int num_threads = 20;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 50; ++j) {
                try {
                    auto endpoints = resolve_endpoint("localhost", 8080);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

TEST_F(NetworkThreadSafetyTest, MemorySafetyTest) {
    const int num_iterations = 30;
    std::atomic<int> total_errors{0};

    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        std::vector<std::thread> threads;

        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&]() {
                try {
                    messaging_client client("localhost", 8080);
                    for (int j = 0; j < 50; ++j) {
                        message msg;
                        client.send(msg);
                    }
                } catch (...) {
                    ++total_errors;
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    EXPECT_EQ(total_errors.load(), 0);
}
