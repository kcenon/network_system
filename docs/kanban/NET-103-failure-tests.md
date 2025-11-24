# NET-103: Expand Test Coverage - Failure Scenarios

| Field | Value |
|-------|-------|
| **ID** | NET-103 |
| **Title** | Expand Test Coverage - Failure Scenarios |
| **Category** | TEST |
| **Priority** | HIGH |
| **Status** | DONE |
| **Est. Duration** | 7-10 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (Problem Statement)

### Current Problem
Current test coverage focuses on happy paths, with insufficient failure scenario testing:
- No network failure simulation tests
- No out-of-memory condition tests
- Insufficient concurrency issue tests
- Inadequate boundary condition tests

### Goal
Add comprehensive failure scenario tests to:
- Verify network failure resilience
- Confirm resource exhaustion handling
- Detect race conditions
- Achieve edge case coverage

### Test Categories
1. **Network Failure Tests** - Connection drops, timeouts, packet loss
2. **Resource Exhaustion Tests** - Memory, file descriptors, thread pools
3. **Concurrency Tests** - Data races, deadlocks
4. **Boundary Tests** - Maximum sizes, empty values, special characters

---

## How (Implementation Plan)

### Implementation Plan

#### Step 1: Network Failure Tests
```cpp
// tests/failure/network_failure_test.cpp

TEST(NetworkFailure, HandlesConnectionReset) {
    messaging_server server("test");
    server.start_server(5555);

    messaging_client client("client");
    client.start_client("localhost", 5555);

    // Forcefully reset connection (simulate network failure)
    client.force_close_socket();

    // Server should handle gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server.active_session_count(), 0);
    EXPECT_FALSE(server.has_error());
}

TEST(NetworkFailure, HandlesConnectionTimeout) {
    messaging_server server("test");
    server.set_connection_timeout(std::chrono::seconds(1));
    server.start_server(5555);

    // Connect but don't send any data
    auto socket = create_raw_socket("localhost", 5555);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Connection should be closed due to timeout
    EXPECT_FALSE(socket.is_connected());
}

TEST(NetworkFailure, RecoversFromPartialSend) {
    messaging_server server("test");
    server.start_server(5555);

    messaging_client client("client");
    client.start_client("localhost", 5555);

    // Send partial message then disconnect
    client.send_partial_data({0x00, 0x00, 0x00, 0x10});  // Header only
    client.disconnect();

    // Server should clean up properly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(server.active_session_count(), 0);
}
```

#### Step 2: Resource Exhaustion Tests
```cpp
// tests/failure/resource_exhaustion_test.cpp

TEST(ResourceExhaustion, HandlesMaxConnections) {
    messaging_server server("test");
    server.set_max_connections(10);
    server.start_server(5555);

    std::vector<std::unique_ptr<messaging_client>> clients;

    // Connect max clients
    for (int i = 0; i < 10; ++i) {
        auto client = std::make_unique<messaging_client>("client" + std::to_string(i));
        EXPECT_TRUE(client->start_client("localhost", 5555).has_value());
        clients.push_back(std::move(client));
    }

    // 11th connection should be rejected
    messaging_client extra_client("extra");
    auto result = extra_client.start_client("localhost", 5555);
    EXPECT_FALSE(result.has_value());
}

TEST(ResourceExhaustion, HandlesLargeMessageQueue) {
    messaging_server server("test");
    server.set_max_pending_messages(100);
    server.start_server(5555);

    messaging_client client("client");
    client.start_client("localhost", 5555);

    // Flood with messages
    for (int i = 0; i < 200; ++i) {
        client.send_packet(create_test_message());
    }

    // Backpressure should be active or client disconnected
    EXPECT_TRUE(client.is_backpressure_active() || !client.is_connected());
}

TEST(ResourceExhaustion, HandlesOutOfMemory) {
    // This test requires special setup with memory limits
    // Use ASAN or custom allocator to simulate OOM

    messaging_server server("test");
    server.start_server(5555);

    // Configure allocator to fail after N allocations
    test_allocator::set_fail_after(1000);

    messaging_client client("client");
    auto result = client.start_client("localhost", 5555);

    // Should handle gracefully
    if (!result.has_value()) {
        EXPECT_EQ(result.error().code(), error_code::out_of_memory);
    }
}
```

#### Step 3: Concurrency Tests
```cpp
// tests/failure/concurrency_test.cpp

TEST(Concurrency, HandlesRapidConnectDisconnect) {
    messaging_server server("test");
    server.start_server(5555);

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Rapid connect/disconnect from multiple threads
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 100; ++i) {
                messaging_client client("client-" + std::to_string(t) + "-" + std::to_string(i));
                if (client.start_client("localhost", 5555).has_value()) {
                    client.send_packet(create_test_message());
                    client.disconnect();
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All sessions should be cleaned up
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(server.active_session_count(), 0);
    EXPECT_GT(success_count.load(), 900);  // Most should succeed
}

TEST(Concurrency, HandlesSimultaneousServerShutdown) {
    messaging_server server("test");
    server.start_server(5555);

    std::vector<std::unique_ptr<messaging_client>> clients;
    for (int i = 0; i < 50; ++i) {
        auto client = std::make_unique<messaging_client>("client");
        client->start_client("localhost", 5555);
        clients.push_back(std::move(client));
    }

    // Start sending messages
    std::thread sender([&]() {
        for (auto& client : clients) {
            for (int i = 0; i < 10; ++i) {
                client->send_packet(create_test_message());
            }
        }
    });

    // Shutdown server while clients are sending
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    server.stop_server();

    sender.join();

    // No crashes, no hangs
    SUCCEED();
}

TEST(Concurrency, NoDataRaceOnMessageCallback) {
    messaging_server server("test");

    std::atomic<int> callback_count{0};
    std::vector<std::vector<uint8_t>> received_messages;
    std::mutex messages_mutex;

    server.set_message_handler([&](auto session_id, auto data) {
        callback_count++;
        std::lock_guard lock(messages_mutex);
        received_messages.push_back(std::move(data));
    });

    server.start_server(5555);

    // Multiple clients sending simultaneously
    std::vector<std::thread> threads;
    for (int t = 0; t < 5; ++t) {
        threads.emplace_back([t]() {
            messaging_client client("client-" + std::to_string(t));
            client.start_client("localhost", 5555);
            for (int i = 0; i < 100; ++i) {
                client.send_packet(create_test_message());
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_EQ(callback_count.load(), 500);
    EXPECT_EQ(received_messages.size(), 500);
}
```

#### Step 4: Boundary Tests
```cpp
// tests/failure/boundary_test.cpp

TEST(Boundary, HandlesEmptyMessage) {
    messaging_server server("test");
    server.start_server(5555);

    messaging_client client("client");
    client.start_client("localhost", 5555);

    auto result = client.send_packet({});
    // Empty message should be rejected or handled gracefully
    EXPECT_TRUE(!result.has_value() || result.value());
}

TEST(Boundary, HandlesMaxSizeMessage) {
    messaging_server server("test");
    server.set_max_message_size(1024 * 1024);  // 1MB
    server.start_server(5555);

    messaging_client client("client");
    client.start_client("localhost", 5555);

    // Send exactly max size
    std::vector<uint8_t> max_message(1024 * 1024, 0x42);
    auto result = client.send_packet(std::move(max_message));
    EXPECT_TRUE(result.has_value());

    // Send over max size
    std::vector<uint8_t> over_max(1024 * 1024 + 1, 0x42);
    result = client.send_packet(std::move(over_max));
    EXPECT_FALSE(result.has_value());
}

TEST(Boundary, HandlesSpecialCharactersInHeaders) {
    http_server server;
    server.start(8080);

    http_client client;

    // Test various special characters
    std::vector<std::string> special_headers = {
        "X-Test: value\0with\0null",
        "X-Test: value\r\ninjection",
        "X-Test: " + std::string(8192, 'A'),  // Very long header
    };

    for (const auto& header : special_headers) {
        http_request request;
        request.set_raw_header(header);

        auto response = client.send(request);
        EXPECT_EQ(response.status_code(), 400);  // Bad request
    }
}

TEST(Boundary, HandlesZeroLengthRead) {
    messaging_server server("test");
    server.start_server(5555);

    // Connect with raw socket and send zero-length data
    auto socket = create_raw_socket("localhost", 5555);
    socket.send({});  // Zero-length send

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Server should not crash
    EXPECT_TRUE(server.is_running());
}
```

---

## Test (Verification Plan)

### Test Execution
```bash
# Run all failure tests
./build/tests/failure_tests

# Run with TSAN (Thread Sanitizer)
cd build_tsan && ctest --output-on-failure

# Run with ASAN (Address Sanitizer)
cd build_asan && ctest --output-on-failure

# Run specific category
./build/tests/failure_tests --gtest_filter="NetworkFailure.*"
./build/tests/failure_tests --gtest_filter="Concurrency.*"
```

### Coverage Requirements
- Minimum 10 test cases per test category
- Maintain overall code coverage above 80%
- Execute all error paths at least once

---

## Acceptance Criteria

- [ ] Network Failure Tests: 10+ test cases
- [ ] Resource Exhaustion Tests: 10+ test cases
- [ ] Concurrency Tests: 10+ test cases (TSAN passing)
- [ ] Boundary Tests: 15+ test cases
- [ ] All tests passing in ASAN build
- [ ] All tests passing in TSAN build
- [ ] Existing tests maintained and passing

---

## Notes

- TSAN/ASAN build configuration required
- CI configuration update may be needed due to longer test execution time
- Some tests may require elevated privileges (network configuration)
