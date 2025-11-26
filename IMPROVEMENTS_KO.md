# Network System - 개선 계획

> **Language:** [English](IMPROVEMENTS.md) | **한국어**

## 현재 상태

**버전:** 1.0.0
**최종 검토:** 2025-01-20
**전체 점수:** 3.5/5

### 치명적 이슈

## ~~1. Session Vector 메모리 누수~~ ✅ 완료

**상태:** v1.3.0에서 구현됨 (Phase 8.1)
**위치:** `include/network_system/core/messaging_server.h:254`

**원래 문제:**
```cpp
class messaging_server {
private:
    std::vector<std::shared_ptr<messaging_session>> sessions_;  // ❌ 절대 정리 안됨!
};
```

**문제점:**
- 종료된 세션이 벡터에 무한정 남음
- 장기 실행 서버에서 메모리 무제한 증가
- 자동 정리 메커니즘 없음

**해결책:**
```cpp
class messaging_server {
public:
    auto on_accept(std::error_code ec, asio::ip::tcp::socket socket) -> void {
        if (!ec && is_running_) {
            // 새 세션 추가 전에 죽은 세션 정리
            cleanup_dead_sessions();

            auto session = std::make_shared<messaging_session>(
                std::move(socket), server_id_);

            // 완료 핸들러 설정
            session->on_complete([this, weak_session = std::weak_ptr(session)]() {
                // 완료 시 자동 제거
                if (auto session = weak_session.lock()) {
                    remove_session(session);
                }
            });

            sessions_.push_back(session);
            session->start();
        }

        if (is_running_) {
            do_accept();
        }
    }

private:
    void cleanup_dead_sessions() {
        sessions_.erase(
            std::remove_if(sessions_.begin(), sessions_.end(),
                [](const auto& session) {
                    return !session || session->is_closed();
                }),
            sessions_.end()
        );
    }

    void remove_session(std::shared_ptr<messaging_session> session) {
        std::lock_guard lock(sessions_mutex_);  // 뮤텍스 추가!
        auto it = std::find(sessions_.begin(), sessions_.end(), session);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

    // 주기적 정리 추가
    void start_cleanup_timer() {
        cleanup_timer_.expires_after(std::chrono::minutes(1));
        cleanup_timer_.async_wait([this](std::error_code ec) {
            if (!ec && is_running_) {
                cleanup_dead_sessions();
                start_cleanup_timer();
            }
        });
    }

    std::mutex sessions_mutex_;  // 스레드 안전을 위한 뮤텍스 추가
    asio::steady_timer cleanup_timer_;  // 타이머 추가
};
```

**우선순위:** P0
**작업량:** 1-2일
**영향:** 치명적 (메모리 누수 방지)

---

## ~~2. 빠른 Sender에 대한 배압(Backpressure) 없음~~ ✅ 완료

**상태:** v1.3.0에서 구현됨 (Phase 8.2)

**원래 문제:**
- 클라이언트가 빠른 메시지로 서버를 압도 가능
- 흐름 제어 없음

**해결책:**
```cpp
class messaging_session {
public:
    void on_message_received(const message& msg) {
        // 큐 크기 확인
        if (pending_messages_.size() >= max_pending_messages_) {
            // 배압 적용
            send_flow_control_message(flow_control::slow_down);

            // 또는 악의적인 클라이언트 연결 해제
            if (pending_messages_.size() >= max_pending_messages_ * 2) {
                disconnect("Queue overflow");
                return;
            }
        }

        pending_messages_.push_back(msg);
        process_next_message();
    }

private:
    static constexpr size_t max_pending_messages_ = 1000;
    std::deque<message> pending_messages_;
};
```

**우선순위:** P1
**작업량:** 2-3일

---

## 고우선순위 개선사항

### ~~3. Client용 Connection Pooling 추가~~ ✅ 완료

**상태:** v1.3.0에서 구현됨 (Phase 8.3)

**원래 제안:**
```cpp
class connection_pool {
public:
    connection_pool(const std::string& host, unsigned short port,
                   size_t pool_size = 10)
        : host_(host), port_(port), pool_size_(pool_size) {
        for (size_t i = 0; i < pool_size_; ++i) {
            auto client = std::make_unique<messaging_client>(host_, port_);
            client->connect();
            available_.push(std::move(client));
        }
    }

    std::unique_ptr<messaging_client> acquire() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !available_.empty(); });

        auto client = std::move(available_.front());
        available_.pop();
        active_count_++;
        return client;
    }

    void release(std::unique_ptr<messaging_client> client) {
        std::lock_guard lock(mutex_);
        available_.push(std::move(client));
        active_count_--;
        cv_.notify_one();
    }

private:
    std::string host_;
    unsigned short port_;
    size_t pool_size_;
    std::queue<std::unique_ptr<messaging_client>> available_;
    std::atomic<size_t> active_count_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

**우선순위:** P2
**작업량:** 3-4일

---

## ~~4. TLS/SSL 지원 추가~~ ✅ 완료

**상태:** v1.4.0에서 구현됨 (Phase 9)

**해결책:**
```cpp
// SSL 스트림을 사용한 보안 소켓 래퍼
class secure_tcp_socket {
public:
    using ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>;

    secure_tcp_socket(asio::ip::tcp::socket socket,
                      asio::ssl::context& ssl_context);

    auto async_handshake(
        asio::ssl::stream_base::handshake_type type,
        std::function<void(std::error_code)> handler) -> void;
};

// SSL 핸드셰이크를 갖춘 보안 세션
class secure_session {
public:
    secure_session(asio::ip::tcp::socket socket,
                   asio::ssl::context& ssl_context,
                   std::string_view server_id);

    auto start_session() -> void;  // SSL 핸드셰이크 수행
    auto send_packet(std::vector<uint8_t>&& data) -> void;
};

// 인증서 로딩을 갖춘 보안 서버
class secure_messaging_server {
public:
    secure_messaging_server(const std::string& server_id,
                           const std::string& cert_file,
                           const std::string& key_file);

    auto start_server(unsigned short port) -> VoidResult;
};

// 선택적 인증서 검증을 갖춘 보안 클라이언트
class secure_messaging_client {
public:
    explicit secure_messaging_client(const std::string& client_id,
                                     bool verify_cert = true);

    auto start_client(const std::string& host, unsigned short port) -> VoidResult;
};
```

**기능:**
- TCP 연결을 위한 완전한 TLS/SSL 암호화
- 서버측 인증서 및 개인키 로딩
- 선택적 클라이언트측 인증서 검증
- 병렬 클래스 계층 구조 (tcp_socket → secure_tcp_socket)
- `BUILD_TLS_SUPPORT` 옵션을 통한 조건부 컴파일 (기본값: ON)
- 암호화 작업에 OpenSSL 사용
- Phase 8의 세션 정리 및 백프레셔 상속

**우선순위:** P2
**작업량:** 5-7일

---

### 5. Client 재연결 로직 추가

```cpp
class resilient_client {
public:
    void send_with_retry(const message& msg, size_t max_retries = 3) {
        for (size_t attempt = 0; attempt < max_retries; ++attempt) {
            if (!client_->is_connected()) {
                auto reconnect_result = reconnect();
                if (!reconnect_result) {
                    std::this_thread::sleep_for(
                        std::chrono::seconds(1 << attempt));  // 지수 백오프
                    continue;
                }
            }

            auto result = client_->send(msg);
            if (result) {
                return;  // 성공
            }

            // 실패, 재시도 예정
            client_->disconnect();
        }

        throw std::runtime_error("Failed to send after retries");
    }

private:
    result_void reconnect() {
        return client_->connect();
    }

    std::unique_ptr<messaging_client> client_;
};
```

**우선순위:** P3
**작업량:** 2-3일

---

## 테스트 요구사항

```cpp
TEST(MessagingServer, NoSessionLeak) {
    messaging_server server("test");
    server.start_server(5555);

    // 많은 클라이언트 연결 및 연결 해제
    for (int i = 0; i < 1000; ++i) {
        messaging_client client("localhost", 5555);
        client.connect();
        client.send_message("test");
        client.disconnect();
    }

    // 정리 대기
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 서버가 세션을 정리했어야 함
    EXPECT_LE(server.active_session_count(), 10);  // 약간의 지연은 허용
}

TEST(MessagingSession, BackpressureWorks) {
    auto session = create_test_session();

    // 메시지로 플러딩
    for (int i = 0; i < 2000; ++i) {
        session->on_message_received(create_test_message());
    }

    // 배압이 트리거되거나 연결 해제되어야 함
    EXPECT_TRUE(session->is_backpressure_active() || session->is_closed());
}
```

**총 작업량:** 13-19일

---

## 참고 자료

- [Asio Documentation](https://think-async.com/Asio/)
- [Network Programming Best Practices](https://beej.us/guide/bgnet/)
