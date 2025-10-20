# Network System - Improvement Plan

> **Language:** **English** | [한국어](IMPROVEMENTS_KO.md)

## Current Status

**Version:** 1.0.0
**Last Review:** 2025-01-20
**Overall Score:** 3.5/5

### Critical Issues

## 1. Session Vector Memory Leak

**Location:** `include/network_system/core/messaging_server.h:254`

**Current Issue:**
```cpp
class messaging_server {
private:
    std::vector<std::shared_ptr<messaging_session>> sessions_;  // ❌ Never cleaned!
};
```

**Problem:**
- Closed sessions remain in vector indefinitely
- Memory grows unbounded in long-running servers
- No automatic cleanup mechanism

**Solution:**
```cpp
class messaging_server {
public:
    auto on_accept(std::error_code ec, asio::ip::tcp::socket socket) -> void {
        if (!ec && is_running_) {
            // Clean up dead sessions before adding new one
            cleanup_dead_sessions();

            auto session = std::make_shared<messaging_session>(
                std::move(socket), server_id_);

            // Set completion handler
            session->on_complete([this, weak_session = std::weak_ptr(session)]() {
                // Auto-remove on completion
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
        std::lock_guard lock(sessions_mutex_);  // Add mutex!
        auto it = std::find(sessions_.begin(), sessions_.end(), session);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }

    // Add periodic cleanup
    void start_cleanup_timer() {
        cleanup_timer_.expires_after(std::chrono::minutes(1));
        cleanup_timer_.async_wait([this](std::error_code ec) {
            if (!ec && is_running_) {
                cleanup_dead_sessions();
                start_cleanup_timer();
            }
        });
    }

    std::mutex sessions_mutex_;  // Add mutex for thread safety
    asio::steady_timer cleanup_timer_;  // Add timer
};
```

**Priority:** P0
**Effort:** 1-2 days
**Impact:** Critical (memory leak prevention)

---

## 2. No Backpressure on Fast Senders

**Problem:**
- Client can overwhelm server with rapid messages
- No flow control

**Solution:**
```cpp
class messaging_session {
public:
    void on_message_received(const message& msg) {
        // Check queue size
        if (pending_messages_.size() >= max_pending_messages_) {
            // Apply backpressure
            send_flow_control_message(flow_control::slow_down);

            // Or disconnect abusive client
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

**Priority:** P1
**Effort:** 2-3 days

---

## High Priority Improvements

### 3. Add Connection Pooling for Client

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

**Priority:** P2
**Effort:** 3-4 days

---

### 4. Add TLS/SSL Support

```cpp
class secure_messaging_server : public messaging_server {
public:
    secure_messaging_server(const std::string& server_id,
                          const std::string& cert_file,
                          const std::string& key_file)
        : messaging_server(server_id)
        , ssl_context_(asio::ssl::context::sslv23) {
        ssl_context_.use_certificate_chain_file(cert_file);
        ssl_context_.use_private_key_file(key_file, asio::ssl::context::pem);
    }

private:
    asio::ssl::context ssl_context_;
};
```

**Priority:** P2
**Effort:** 5-7 days

---

### 5. Add Reconnection Logic for Client

```cpp
class resilient_client {
public:
    void send_with_retry(const message& msg, size_t max_retries = 3) {
        for (size_t attempt = 0; attempt < max_retries; ++attempt) {
            if (!client_->is_connected()) {
                auto reconnect_result = reconnect();
                if (!reconnect_result) {
                    std::this_thread::sleep_for(
                        std::chrono::seconds(1 << attempt));  // Exponential backoff
                    continue;
                }
            }

            auto result = client_->send(msg);
            if (result) {
                return;  // Success
            }

            // Failed, will retry
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

**Priority:** P3
**Effort:** 2-3 days

---

## Testing Requirements

```cpp
TEST(MessagingServer, NoSessionLeak) {
    messaging_server server("test");
    server.start_server(5555);

    // Connect and disconnect many clients
    for (int i = 0; i < 1000; ++i) {
        messaging_client client("localhost", 5555);
        client.connect();
        client.send_message("test");
        client.disconnect();
    }

    // Wait for cleanup
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Server should have cleaned up sessions
    EXPECT_LE(server.active_session_count(), 10);  // Some lingering OK
}

TEST(MessagingSession, BackpressureWorks) {
    auto session = create_test_session();

    // Flood with messages
    for (int i = 0; i < 2000; ++i) {
        session->on_message_received(create_test_message());
    }

    // Should trigger backpressure or disconnect
    EXPECT_TRUE(session->is_backpressure_active() || session->is_closed());
}
```

**Total Effort:** 13-19 days
