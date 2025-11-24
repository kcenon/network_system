# NET-201: WebSocket E2E Integration Tests

| Field | Value |
|-------|-------|
| **ID** | NET-201 |
| **Title** | WebSocket E2E Integration Tests |
| **Category** | TEST |
| **Priority** | MEDIUM |
| **Status** | TODO |
| **Est. Duration** | 4-5 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (무엇을 바꾸려는가)

### Current Problem
WebSocket 프로토콜에 대한 End-to-End 통합 테스트가 부족합니다:
- 핸드셰이크 검증 테스트 부족
- 프레임 타입별 테스트 미흡
- 연결 유지 (ping/pong) 테스트 없음
- 실제 브라우저/클라이언트 호환성 미검증

### Goal
WebSocket 프로토콜의 완전한 E2E 테스트 스위트 구축:
- RFC 6455 준수 검증
- 모든 프레임 타입 테스트
- 연결 생명주기 전체 테스트
- 외부 클라이언트 호환성 테스트

### Test Scope
1. **Handshake Tests** - 연결 수립 및 업그레이드
2. **Frame Tests** - Text, Binary, Ping, Pong, Close
3. **Fragmentation Tests** - 메시지 분할 및 조립
4. **Connection Management** - Keep-alive, 재연결
5. **Compatibility Tests** - 외부 WebSocket 클라이언트

---

## How (어떻게 바꾸려는가)

### Implementation Plan

#### Step 1: Handshake Tests
```cpp
// tests/integration/websocket_e2e_test.cpp

TEST(WebSocketE2E, HandshakeSuccess) {
    http_server server;
    server.enable_websocket("/ws");
    server.start(8080);

    // Use raw HTTP client for handshake
    tcp_client client;
    client.connect("localhost", 8080);

    std::string handshake =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    client.send(handshake);
    auto response = client.receive();

    EXPECT_TRUE(response.find("101 Switching Protocols") != std::string::npos);
    EXPECT_TRUE(response.find("Sec-WebSocket-Accept") != std::string::npos);
}

TEST(WebSocketE2E, HandshakeRejectsInvalidVersion) {
    http_server server;
    server.enable_websocket("/ws");
    server.start(8080);

    tcp_client client;
    client.connect("localhost", 8080);

    std::string handshake =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 8\r\n"  // Invalid version
        "\r\n";

    client.send(handshake);
    auto response = client.receive();

    EXPECT_TRUE(response.find("426") != std::string::npos);  // Upgrade Required
}

TEST(WebSocketE2E, HandshakeRejectsMissingKey) {
    http_server server;
    server.enable_websocket("/ws");
    server.start(8080);

    tcp_client client;
    client.connect("localhost", 8080);

    std::string handshake =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";  // Missing Sec-WebSocket-Key

    client.send(handshake);
    auto response = client.receive();

    EXPECT_TRUE(response.find("400") != std::string::npos);
}
```

#### Step 2: Frame Type Tests
```cpp
TEST(WebSocketE2E, TextMessageRoundTrip) {
    websocket_server server;
    server.start(8080);

    std::string received_message;
    server.set_message_handler([&](auto id, auto msg, auto is_binary) {
        received_message = std::string(msg.begin(), msg.end());
        server.send(id, msg, is_binary);  // Echo back
    });

    websocket_client client;
    client.connect("localhost", 8080);

    client.send_text("Hello, WebSocket!");
    auto response = client.receive();

    EXPECT_EQ(received_message, "Hello, WebSocket!");
    EXPECT_EQ(response.text(), "Hello, WebSocket!");
}

TEST(WebSocketE2E, BinaryMessageRoundTrip) {
    websocket_server server;
    server.start(8080);

    server.set_message_handler([&](auto id, auto msg, auto is_binary) {
        server.send(id, msg, is_binary);  // Echo back
    });

    websocket_client client;
    client.connect("localhost", 8080);

    std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE};
    client.send_binary(binary_data);
    auto response = client.receive();

    EXPECT_TRUE(response.is_binary());
    EXPECT_EQ(response.binary(), binary_data);
}

TEST(WebSocketE2E, PingPongExchange) {
    websocket_server server;
    server.start(8080);

    websocket_client client;
    client.connect("localhost", 8080);

    // Client sends ping
    client.send_ping("ping-data");

    // Should receive pong with same data
    auto response = client.receive();
    EXPECT_TRUE(response.is_pong());
    EXPECT_EQ(response.payload(), "ping-data");
}

TEST(WebSocketE2E, CloseHandshake) {
    websocket_server server;
    server.start(8080);

    websocket_client client;
    client.connect("localhost", 8080);

    // Initiate close
    client.close(1000, "Normal closure");

    // Should receive close frame back
    auto response = client.receive();
    EXPECT_TRUE(response.is_close());
    EXPECT_EQ(response.close_code(), 1000);
}
```

#### Step 3: Fragmentation Tests
```cpp
TEST(WebSocketE2E, FragmentedTextMessage) {
    websocket_server server;
    server.start(8080);

    std::string received_message;
    server.set_message_handler([&](auto id, auto msg, auto is_binary) {
        received_message = std::string(msg.begin(), msg.end());
    });

    websocket_client client;
    client.connect("localhost", 8080);

    // Send fragmented message
    client.send_fragment("Hello, ", false, true);   // First fragment
    client.send_fragment("Fragmented ", false, false); // Continuation
    client.send_fragment("World!", true, false);    // Final fragment

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(received_message, "Hello, Fragmented World!");
}

TEST(WebSocketE2E, InterleavedControlFrames) {
    websocket_server server;
    server.start(8080);

    websocket_client client;
    client.connect("localhost", 8080);

    // Start fragmented message
    client.send_fragment("Start", false, true);

    // Send ping in the middle (allowed by RFC 6455)
    client.send_ping("mid-fragment-ping");

    // Receive pong (control frames can interleave)
    auto pong = client.receive();
    EXPECT_TRUE(pong.is_pong());

    // Continue and finish message
    client.send_fragment("End", true, false);

    // No errors should occur
    EXPECT_TRUE(client.is_connected());
}
```

#### Step 4: Connection Management Tests
```cpp
TEST(WebSocketE2E, KeepAliveWithServerPing) {
    websocket_server server;
    server.set_ping_interval(std::chrono::seconds(1));
    server.start(8080);

    websocket_client client;
    client.connect("localhost", 8080);

    // Wait for server pings
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Should receive multiple pings
    EXPECT_GE(client.received_ping_count(), 2);
    EXPECT_TRUE(client.is_connected());
}

TEST(WebSocketE2E, ConnectionTimeoutOnMissingPong) {
    websocket_server server;
    server.set_ping_interval(std::chrono::seconds(1));
    server.set_pong_timeout(std::chrono::seconds(2));
    server.start(8080);

    // Connect with raw socket (won't respond to pings)
    tcp_client raw_client;
    raw_client.connect("localhost", 8080);
    raw_client.send(create_websocket_handshake());

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Connection should be closed
    EXPECT_FALSE(raw_client.is_connected());
}
```

#### Step 5: External Compatibility Tests
```cpp
TEST(WebSocketE2E, CompatibilityWithWebsocketpp) {
    // This test uses an external websocketpp client
    websocket_server server;
    server.start(8080);

    // Start external client process
    auto client_process = start_process(
        "external_websocket_client",
        "--url", "ws://localhost:8080",
        "--message", "test"
    );

    // Wait for completion
    auto result = client_process.wait();
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.output.find("success") != std::string::npos);
}

TEST(WebSocketE2E, CompatibilityWithAutobahn) {
    // Run Autobahn test suite
    websocket_server server;
    server.start(8080);

    auto autobahn = start_process(
        "wstest",
        "-m", "fuzzingclient",
        "-s", "autobahn_config.json"
    );

    auto result = autobahn.wait();

    // Parse Autobahn results
    auto report = parse_autobahn_report("reports/index.json");
    EXPECT_EQ(report.passed, report.total);
}
```

---

## Test (어떻게 테스트하는가)

### Test Execution
```bash
# Run all WebSocket E2E tests
./build/tests/websocket_e2e_test

# Run with verbose output
./build/tests/websocket_e2e_test --gtest_output=xml:results.xml

# Run Autobahn fuzzing test (optional)
docker run -it --rm \
  -v $(pwd)/autobahn:/config \
  crossbario/autobahn-testsuite \
  wstest -m fuzzingclient -s /config/fuzzingclient.json
```

### Manual Testing
1. 브라우저 JavaScript WebSocket 클라이언트로 연결 테스트
2. `websocat` 도구로 수동 프레임 전송 테스트
3. Wireshark로 프레임 형식 검증

---

## Acceptance Criteria

- [ ] 핸드셰이크 테스트 5+ 케이스
- [ ] 프레임 타입 테스트 10+ 케이스
- [ ] 분할 메시지 테스트 5+ 케이스
- [ ] 연결 관리 테스트 5+ 케이스
- [ ] 외부 클라이언트 호환성 검증
- [ ] 모든 테스트 CI에서 통과

---

## Notes

- Autobahn 테스트 스위트 설정 필요
- 일부 테스트는 네트워크 지연이 필요할 수 있음
- 브라우저 테스트는 별도 셋업 필요
