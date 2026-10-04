// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_session_lifecycle.cpp
 * @brief Unit tests for session lifecycle: create, send, receive, close, timeout
 *
 * Tests validate the i_session interface contract and session lifecycle
 * patterns using mock sessions. These tests do not require ASIO.
 *
 * Covers:
 * - Session construction and identity
 * - Connection state transitions (connected -> closed)
 * - Send behavior in connected/disconnected states
 * - Receive callback invocation
 * - Disconnection callback invocation
 * - Error callback invocation
 * - Message queue backpressure
 * - Session close idempotency
 * - Thread-safe close from multiple threads
 */

#include <gtest/gtest.h>

#include "kcenon/network/interfaces/i_session.h"
#include "kcenon/network/types/result.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network;

// ============================================================================
// Mock Session Implementation
// ============================================================================

namespace {

/**
 * @brief Mock session implementing i_session for lifecycle testing
 *
 * Tracks all operations (sends, closes, callbacks) without requiring ASIO.
 * Simulates message queue and backpressure behavior.
 */
class mock_session : public interfaces::i_session,
                     public std::enable_shared_from_this<mock_session>
{
public:
    explicit mock_session(std::string id = "test-session-001",
                          std::size_t max_pending = 1000)
        : id_(std::move(id)), max_pending_messages_(max_pending)
    {
    }

    ~mock_session() noexcept override = default;

    // i_session interface
    [[nodiscard]] auto id() const -> std::string_view override { return id_; }

    [[nodiscard]] auto is_connected() const -> bool override
    {
        return !stopped_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override
    {
        if (stopped_.load(std::memory_order_acquire))
        {
            return VoidResult::err(-1, "Session is closed");
        }

        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (sent_messages_.size() >= max_pending_messages_)
        {
            return VoidResult::err(-2, "Message queue full");
        }

        sent_messages_.push_back(std::move(data));
        return VoidResult::ok(std::monostate{});
    }

    auto close() -> void override
    {
        bool expected = false;
        if (stopped_.compare_exchange_strong(expected, true,
                                             std::memory_order_release))
        {
            close_count_.fetch_add(1, std::memory_order_relaxed);

            if (disconnection_callback_)
            {
                disconnection_callback_(id_);
            }
        }
    }

    // Test helpers
    void set_receive_callback(
        std::function<void(const std::vector<uint8_t>&)> callback)
    {
        receive_callback_ = std::move(callback);
    }

    void set_disconnection_callback(
        std::function<void(const std::string&)> callback)
    {
        disconnection_callback_ = std::move(callback);
    }

    void set_error_callback(std::function<void(std::error_code)> callback)
    {
        error_callback_ = std::move(callback);
    }

    // Simulate receiving data (triggers receive callback)
    void simulate_receive(const std::vector<uint8_t>& data)
    {
        if (receive_callback_ && !stopped_.load(std::memory_order_acquire))
        {
            receive_callback_(data);
        }
    }

    // Simulate an error (triggers error callback)
    void simulate_error(std::error_code ec)
    {
        if (error_callback_)
        {
            error_callback_(ec);
        }
    }

    // Accessors for test verification
    [[nodiscard]] auto sent_messages() const -> const std::deque<std::vector<uint8_t>>&
    {
        return sent_messages_;
    }

    [[nodiscard]] auto sent_count() const -> std::size_t
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return sent_messages_.size();
    }

    [[nodiscard]] auto close_count() const -> int
    {
        return close_count_.load(std::memory_order_relaxed);
    }

private:
    std::string id_;
    std::atomic<bool> stopped_{false};
    std::atomic<int> close_count_{0};

    mutable std::mutex queue_mutex_;
    std::deque<std::vector<uint8_t>> sent_messages_;
    std::size_t max_pending_messages_;

    std::function<void(const std::vector<uint8_t>&)> receive_callback_;
    std::function<void(const std::string&)> disconnection_callback_;
    std::function<void(std::error_code)> error_callback_;
};

} // namespace

// ============================================================================
// Test Fixture
// ============================================================================

class SessionLifecycleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        session_ = std::make_shared<mock_session>("test-session-001");
    }

    std::shared_ptr<mock_session> session_;
};

// ============================================================================
// Construction and Identity Tests
// ============================================================================

TEST_F(SessionLifecycleTest, SessionConstructionSetsId)
{
    EXPECT_EQ(session_->id(), "test-session-001");
}

TEST_F(SessionLifecycleTest, SessionConstructionStartsConnected)
{
    EXPECT_TRUE(session_->is_connected());
}

TEST_F(SessionLifecycleTest, SessionWithCustomId)
{
    auto session = std::make_shared<mock_session>("custom-id-42");
    EXPECT_EQ(session->id(), "custom-id-42");
    EXPECT_TRUE(session->is_connected());
}

TEST_F(SessionLifecycleTest, SessionWithEmptyId)
{
    auto session = std::make_shared<mock_session>("");
    EXPECT_EQ(session->id(), "");
    EXPECT_TRUE(session->is_connected());
}

// ============================================================================
// Connection State Transition Tests
// ============================================================================

TEST_F(SessionLifecycleTest, CloseTransitionsToDisconnected)
{
    EXPECT_TRUE(session_->is_connected());

    session_->close();

    EXPECT_FALSE(session_->is_connected());
}

TEST_F(SessionLifecycleTest, CloseIsIdempotent)
{
    session_->close();
    session_->close();
    session_->close();

    EXPECT_FALSE(session_->is_connected());
    // Only the first close should trigger the disconnection callback
    EXPECT_EQ(session_->close_count(), 1);
}

TEST_F(SessionLifecycleTest, IsConnectedReturnsCorrectState)
{
    EXPECT_TRUE(session_->is_connected());

    session_->close();

    EXPECT_FALSE(session_->is_connected());
}

// ============================================================================
// Send Behavior Tests
// ============================================================================

TEST_F(SessionLifecycleTest, SendOnConnectedSessionSucceeds)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};

    auto result = session_->send(std::move(data));

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(session_->sent_count(), 1u);
}

TEST_F(SessionLifecycleTest, SendOnClosedSessionFails)
{
    session_->close();

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto result = session_->send(std::move(data));

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(session_->sent_count(), 0u);
}

TEST_F(SessionLifecycleTest, SendEmptyDataSucceeds)
{
    std::vector<uint8_t> empty_data;

    auto result = session_->send(std::move(empty_data));

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(session_->sent_count(), 1u);
}

TEST_F(SessionLifecycleTest, SendMultipleMessagesQueuesAll)
{
    for (int i = 0; i < 10; ++i)
    {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        auto result = session_->send(std::move(data));
        EXPECT_TRUE(result.is_ok());
    }

    EXPECT_EQ(session_->sent_count(), 10u);
}

TEST_F(SessionLifecycleTest, SendPreservesMessageData)
{
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};

    auto result = session_->send(std::vector<uint8_t>(data));
    EXPECT_TRUE(result.is_ok());

    const auto& messages = session_->sent_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0], data);
}

TEST_F(SessionLifecycleTest, SendLargeMessage)
{
    // 64KB message
    std::vector<uint8_t> large_data(65536, 0xAA);

    auto result = session_->send(std::move(large_data));

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(session_->sent_count(), 1u);
    EXPECT_EQ(session_->sent_messages().back().size(), 65536u);
}

// ============================================================================
// Message Queue Backpressure Tests
// ============================================================================

TEST_F(SessionLifecycleTest, BackpressureRejectsWhenQueueFull)
{
    // Create session with small queue limit
    auto limited_session = std::make_shared<mock_session>("limited", 5);

    // Fill the queue
    for (int i = 0; i < 5; ++i)
    {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i)};
        auto result = limited_session->send(std::move(data));
        EXPECT_TRUE(result.is_ok());
    }

    // Next send should fail
    std::vector<uint8_t> overflow_data = {0xFF};
    auto result = limited_session->send(std::move(overflow_data));

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(limited_session->sent_count(), 5u);
}

TEST_F(SessionLifecycleTest, BackpressureAtBoundary)
{
    auto limited_session = std::make_shared<mock_session>("boundary", 1);

    // First send succeeds
    std::vector<uint8_t> data1 = {0x01};
    EXPECT_TRUE(limited_session->send(std::move(data1)).is_ok());

    // Second send fails (queue full)
    std::vector<uint8_t> data2 = {0x02};
    EXPECT_TRUE(limited_session->send(std::move(data2)).is_err());
}

// ============================================================================
// Receive Callback Tests
// ============================================================================

TEST_F(SessionLifecycleTest, ReceiveCallbackInvokedOnData)
{
    std::vector<uint8_t> received_data;
    int callback_count = 0;

    session_->set_receive_callback(
        [&](const std::vector<uint8_t>& data) {
            received_data = data;
            callback_count++;
        });

    std::vector<uint8_t> incoming = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    session_->simulate_receive(incoming);

    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(received_data, incoming);
}

TEST_F(SessionLifecycleTest, ReceiveCallbackNotInvokedAfterClose)
{
    int callback_count = 0;

    session_->set_receive_callback(
        [&](const std::vector<uint8_t>&) { callback_count++; });

    session_->close();

    std::vector<uint8_t> incoming = {0x01};
    session_->simulate_receive(incoming);

    EXPECT_EQ(callback_count, 0);
}

TEST_F(SessionLifecycleTest, ReceiveCallbackMultipleMessages)
{
    std::vector<std::vector<uint8_t>> received_messages;

    session_->set_receive_callback(
        [&](const std::vector<uint8_t>& data) {
            received_messages.push_back(data);
        });

    for (uint8_t i = 0; i < 5; ++i)
    {
        session_->simulate_receive({i, static_cast<uint8_t>(i + 1)});
    }

    ASSERT_EQ(received_messages.size(), 5u);
    EXPECT_EQ(received_messages[0], (std::vector<uint8_t>{0, 1}));
    EXPECT_EQ(received_messages[4], (std::vector<uint8_t>{4, 5}));
}

// ============================================================================
// Disconnection Callback Tests
// ============================================================================

TEST_F(SessionLifecycleTest, DisconnectionCallbackInvokedOnClose)
{
    std::string disconnected_id;

    session_->set_disconnection_callback(
        [&](const std::string& id) { disconnected_id = id; });

    session_->close();

    EXPECT_EQ(disconnected_id, "test-session-001");
}

TEST_F(SessionLifecycleTest, DisconnectionCallbackInvokedOnlyOnce)
{
    int callback_count = 0;

    session_->set_disconnection_callback(
        [&](const std::string&) { callback_count++; });

    session_->close();
    session_->close();
    session_->close();

    EXPECT_EQ(callback_count, 1);
}

TEST_F(SessionLifecycleTest, NoDisconnectionCallbackIfNotSet)
{
    // Should not crash when no callback is set
    EXPECT_NO_THROW(session_->close());
    EXPECT_FALSE(session_->is_connected());
}

// ============================================================================
// Error Callback Tests
// ============================================================================

TEST_F(SessionLifecycleTest, ErrorCallbackInvokedOnError)
{
    std::error_code received_error;

    session_->set_error_callback(
        [&](std::error_code ec) { received_error = ec; });

    auto ec = std::make_error_code(std::errc::connection_reset);
    session_->simulate_error(ec);

    EXPECT_EQ(received_error, ec);
}

TEST_F(SessionLifecycleTest, ErrorCallbackWithDifferentErrorCodes)
{
    std::vector<std::error_code> errors;

    session_->set_error_callback(
        [&](std::error_code ec) { errors.push_back(ec); });

    session_->simulate_error(
        std::make_error_code(std::errc::connection_reset));
    session_->simulate_error(
        std::make_error_code(std::errc::connection_refused));
    session_->simulate_error(
        std::make_error_code(std::errc::timed_out));

    ASSERT_EQ(errors.size(), 3u);
    EXPECT_EQ(errors[0], std::make_error_code(std::errc::connection_reset));
    EXPECT_EQ(errors[1], std::make_error_code(std::errc::connection_refused));
    EXPECT_EQ(errors[2], std::make_error_code(std::errc::timed_out));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(SessionLifecycleTest, ConcurrentSendsAreThreadSafe)
{
    const int num_threads = 8;
    const int sends_per_thread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([this, t, sends_per_thread]() {
            for (int i = 0; i < sends_per_thread; ++i)
            {
                std::vector<uint8_t> data = {
                    static_cast<uint8_t>(t),
                    static_cast<uint8_t>(i)};
                session_->send(std::move(data));
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(session_->sent_count(),
              static_cast<std::size_t>(num_threads * sends_per_thread));
}

TEST_F(SessionLifecycleTest, ConcurrentCloseIsThreadSafe)
{
    const int num_threads = 10;
    std::vector<std::thread> threads;

    int disconnect_count = 0;
    std::mutex count_mutex;

    session_->set_disconnection_callback(
        [&](const std::string&) {
            std::lock_guard<std::mutex> lock(count_mutex);
            disconnect_count++;
        });

    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([this]() { session_->close(); });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_FALSE(session_->is_connected());
    EXPECT_EQ(session_->close_count(), 1);
    EXPECT_EQ(disconnect_count, 1);
}

TEST_F(SessionLifecycleTest, ConcurrentSendAndClose)
{
    const int num_senders = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    // Senders
    for (int t = 0; t < num_senders; ++t)
    {
        threads.emplace_back([this, &success_count, &fail_count]() {
            for (int i = 0; i < 100; ++i)
            {
                std::vector<uint8_t> data = {0x01};
                auto result = session_->send(std::move(data));
                if (result.is_ok())
                {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    fail_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Closer (delayed slightly)
    threads.emplace_back([this]() {
        std::this_thread::yield();
        session_->close();
    });

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_FALSE(session_->is_connected());
    // All sends should have either succeeded or failed -- no crashes
    EXPECT_EQ(success_count.load() + fail_count.load(),
              num_senders * 100);
}

// ============================================================================
// i_session Interface Polymorphism Tests
// ============================================================================

TEST_F(SessionLifecycleTest, SessionUsableThroughInterfacePointer)
{
    // Verify the mock correctly implements i_session
    interfaces::i_session* iface = session_.get();

    EXPECT_EQ(iface->id(), "test-session-001");
    EXPECT_TRUE(iface->is_connected());

    std::vector<uint8_t> data = {0x01, 0x02};
    auto result = iface->send(std::move(data));
    EXPECT_TRUE(result.is_ok());

    iface->close();
    EXPECT_FALSE(iface->is_connected());
}

TEST_F(SessionLifecycleTest, SendAfterCloseViaInterfaceFails)
{
    interfaces::i_session* iface = session_.get();

    iface->close();

    std::vector<uint8_t> data = {0x01};
    auto result = iface->send(std::move(data));
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Session Lifetime / Shared Pointer Tests
// ============================================================================

TEST_F(SessionLifecycleTest, SessionSurvivesWeakPtrCycle)
{
    auto session = std::make_shared<mock_session>("lifetime-test");
    std::weak_ptr<mock_session> weak = session;

    EXPECT_FALSE(weak.expired());
    EXPECT_TRUE(weak.lock()->is_connected());

    session->close();
    EXPECT_FALSE(weak.lock()->is_connected());

    session.reset();
    EXPECT_TRUE(weak.expired());
}

TEST_F(SessionLifecycleTest, SharedOwnershipKeepsSessionAlive)
{
    auto session1 = std::make_shared<mock_session>("shared-test");
    auto session2 = session1; // shared ownership

    session1->close();
    EXPECT_FALSE(session2->is_connected());

    session1.reset();
    // session2 still holds the session
    EXPECT_FALSE(session2->is_connected());
    EXPECT_EQ(session2->id(), "shared-test");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SessionLifecycleTest, SendAfterMultipleCloseAttempts)
{
    session_->close();
    session_->close();

    std::vector<uint8_t> data = {0x01};
    auto result = session_->send(std::move(data));

    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(session_->sent_count(), 0u);
}

TEST_F(SessionLifecycleTest, ZeroSizeQueueLimit)
{
    auto session = std::make_shared<mock_session>("zero-queue", 0);

    std::vector<uint8_t> data = {0x01};
    auto result = session->send(std::move(data));

    EXPECT_TRUE(result.is_err());
}

TEST_F(SessionLifecycleTest, CallbackReplacementWorks)
{
    int first_callback_count = 0;
    int second_callback_count = 0;

    session_->set_receive_callback(
        [&](const std::vector<uint8_t>&) { first_callback_count++; });
    session_->simulate_receive({0x01});

    session_->set_receive_callback(
        [&](const std::vector<uint8_t>&) { second_callback_count++; });
    session_->simulate_receive({0x02});

    EXPECT_EQ(first_callback_count, 1);
    EXPECT_EQ(second_callback_count, 1);
}
