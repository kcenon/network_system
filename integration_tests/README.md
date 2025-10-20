# Network System Integration Tests

> **Language:** **English** | [한국어](README_KO.md)

This directory contains comprehensive integration tests for the network_system library. These tests verify the complete behavior of network components working together in realistic scenarios.

## Test Structure

```
integration_tests/
├── framework/              # Test infrastructure
│   ├── system_fixture.h   # Base test fixtures
│   └── test_helpers.h     # Helper functions and utilities
├── scenarios/             # Scenario-based tests
│   ├── connection_lifecycle_test.cpp
│   └── protocol_integration_test.cpp
├── performance/           # Performance tests
│   └── network_performance_test.cpp
└── failures/              # Error handling tests
    └── error_handling_test.cpp
```

## Test Suites

### 1. Connection Lifecycle Tests (17 tests)
Tests the complete lifecycle of network connections:
- Server initialization and startup
- Client connection establishment
- Connection acceptance handling
- Message send and receive
- Connection close and cleanup
- Multiple concurrent connections
- Server shutdown

**Key Tests:**
- `ServerInitialization` - Verify server creation
- `ServerStartupSuccess` - Server starts correctly
- `ClientConnectionSuccess` - Client connects to server
- `MultipleConcurrentConnections` - Handle multiple clients
- `ServerShutdownWithActiveConnections` - Graceful shutdown

### 2. Protocol Integration Tests (13 tests)
Tests protocol-level functionality:
- Message serialization/deserialization
- Request-response patterns
- Message fragmentation and reassembly
- Data integrity verification

**Key Tests:**
- `SmallMessageTransmission` - Send small messages
- `LargeMessageTransmission` - Send large messages
- `MessageFragmentation` - Handle message fragmentation
- `BinaryDataTransmission` - Binary data integrity
- `SequentialMessages` - Sequential message handling

### 3. Network Performance Tests (10 tests)
Measures performance characteristics:
- Connection throughput
- Message latency (P50, P95, P99)
- Bandwidth utilization
- Concurrent connection scalability
- Message throughput

**Performance Baselines:**
- Connection establishment: < 100ms (P50)
- Message latency P50: < 10ms
- Message latency P95: < 50ms
- Message throughput: > 1,000 msg/s
- Concurrent connections: > 20 simultaneous

**Key Tests:**
- `ConnectionThroughput` - Measure connection rate
- `SmallMessageLatency` - Measure latency for small messages
- `MessageThroughput` - Measure message processing rate
- `ConcurrentConnectionScalability` - Test with varying loads
- `SustainedLoad` - Continuous load handling

### 4. Error Handling Tests (18 tests)
Tests error conditions and recovery:
- Connection failures and timeouts
- Invalid operations
- Network errors
- Resource exhaustion
- Recovery scenarios

**Key Tests:**
- `ConnectToInvalidHost` - Handle connection failures
- `SendWithoutConnection` - Prevent invalid operations
- `ServerShutdownDuringTransmission` - Handle disruptions
- `LargeMessageHandling` - Buffer limit handling
- `RecoveryAfterConnectionFailure` - Test recovery

## Running the Tests

### Build and Run All Integration Tests

```bash
# Configure with integration tests enabled
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DNETWORK_BUILD_INTEGRATION_TESTS=ON

# Build
cmake --build build

# Run all integration tests
cd build
ctest -R "ConnectionLifecycle|ProtocolIntegration|NetworkPerformance|ErrorHandling"
```

### Run Specific Test Suite

```bash
# Connection lifecycle tests
./build/bin/integration_tests/connection_lifecycle_test

# Protocol integration tests
./build/bin/integration_tests/protocol_integration_test

# Performance tests
./build/bin/integration_tests/network_performance_test

# Error handling tests
./build/bin/integration_tests/error_handling_test
```

### Run with Verbose Output

```bash
ctest -V -R "ConnectionLifecycle"
```

## Coverage Analysis

### Generate Coverage Report

```bash
# Configure with coverage enabled
cmake -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DNETWORK_BUILD_INTEGRATION_TESTS=ON

# Build and run tests
cmake --build build
cd build
ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info \
  --ignore-errors gcov,source,unused
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/integration_tests/*' \
  --output-file coverage_filtered.info --ignore-errors unused
genhtml coverage_filtered.info --output-directory coverage_html

# View coverage
open coverage_html/index.html  # macOS
xdg-open coverage_html/index.html  # Linux
```

### Coverage Targets

- **Line Coverage:** > 80%
- **Branch Coverage:** > 70%
- **Function Coverage:** > 90%

## Test Framework

### NetworkSystemFixture

Base fixture providing:
- `StartServer()` - Start test server
- `StopServer()` - Stop test server
- `ConnectClient()` - Connect test client
- `SendMessage()` - Send message from client
- `CreateTestMessage()` - Create test data
- `WaitFor()` - Wait for specified duration

### MultiConnectionFixture

Extended fixture for multiple connections:
- `CreateClients()` - Create multiple clients
- `ConnectAllClients()` - Connect all clients

### PerformanceFixture

Performance testing fixture:
- `MeasureDuration()` - Measure operation duration
- `CalculateStats()` - Calculate statistics

### Test Helpers

Utility functions in `test_helpers.h`:
- `find_available_port()` - Find free port
- `wait_for_connection()` - Wait for connection
- `calculate_statistics()` - Compute performance stats
- `generate_random_data()` - Create test data
- `verify_message_data()` - Verify data integrity

## Performance Benchmarking

Performance tests provide detailed metrics:

```
Connection latency (ms):
  P50: 5.2
  P95: 12.8
  P99: 18.5

Small message latency (ms):
  P50: 0.8
  P95: 2.1
  P99: 4.3

Message throughput: 15,234 msg/s
Bandwidth: 125.5 MB/s
```

## CI/CD Integration

Integration tests run automatically on:
- Push to main/develop/feature branches
- Pull requests
- Manual workflow dispatch

### Test Matrix

- **Platforms:** Ubuntu, macOS
- **Build Types:** Debug, Release
- **Coverage:** Generated for Debug builds

### Artifacts

- Coverage reports (Debug builds)
- Test results
- Performance metrics

## Debugging Tests

### Run Single Test

```bash
./build/bin/integration_tests/connection_lifecycle_test \
  --gtest_filter="ConnectionLifecycleTest.ServerStartupSuccess"
```

### Enable Debug Output

```bash
./build/bin/integration_tests/network_performance_test --gtest_color=yes
```

### Run Under Debugger

```bash
lldb ./build/bin/integration_tests/error_handling_test
(lldb) run --gtest_filter="ErrorHandlingTest.ConnectToInvalidHost"
```

## Adding New Tests

### 1. Choose Test Suite

Determine which category your test belongs to:
- **scenarios/** - Connection and protocol behavior
- **performance/** - Performance measurements
- **failures/** - Error conditions

### 2. Add Test Case

```cpp
TEST_F(ConnectionLifecycleTest, YourTestName) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Your test logic
    auto message = CreateTestMessage(256);
    EXPECT_TRUE(SendMessage(message));
}
```

### 3. Update CMakeLists.txt

Add new test files to the appropriate executable in `CMakeLists.txt`.

### 4. Verify

```bash
cmake --build build
ctest -R "YourTestName" -V
```

## Network-Specific Considerations

### Port Allocation

Tests use dynamic port allocation to avoid conflicts:
```cpp
test_port_ = test_helpers::find_available_port();
```

### Localhost Testing

All tests use localhost loopback (no external network dependencies):
```cpp
client_->start_client("localhost", test_port_);
```

### Timeouts

Tests include appropriate timeouts for async operations:
```cpp
test_helpers::wait_for_connection(client_, std::chrono::seconds(5));
```

### Cleanup

Proper cleanup prevents port exhaustion:
```cpp
void TearDown() override {
    if (client_) client_->stop_client();
    if (server_) server_->stop_server();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
```

## Troubleshooting

### Tests Failing on Port Conflicts

Increase the port range in `find_available_port()` or use random ports.

### Timeouts on CI

CI environments may be slower; increase timeout values:
```cpp
EXPECT_LT(stats.p50, 100.0);  // More conservative for CI
```

### Memory Leaks

Run with sanitizers:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address"
```

## Performance Optimization

Tests are designed to:
- Minimize setup/teardown overhead
- Reuse connections when possible
- Use realistic message sizes
- Measure actual network behavior

## Contributing

When adding integration tests:
1. Follow existing patterns and fixtures
2. Include performance baselines
3. Document expected behavior
4. Test error conditions
5. Ensure tests are deterministic
6. Clean up resources properly

## References

- [Google Test Documentation](https://google.github.io/googletest/)
- [Network System API](../README.md)
- [Performance Baselines](../BASELINE.md)

## Total Test Count

- **Connection Lifecycle:** 17 tests
- **Protocol Integration:** 13 tests
- **Network Performance:** 10 tests
- **Error Handling:** 18 tests

**Total:** 58 integration tests
