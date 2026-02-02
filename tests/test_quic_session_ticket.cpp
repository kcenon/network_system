/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include <chrono>
#include <thread>

#include "internal/protocols/quic/session_ticket_store.h"

using namespace kcenon::network::protocols::quic;
using namespace std::chrono_literals;

// ============================================================================
// session_ticket_info Tests
// ============================================================================

class SessionTicketInfoTest : public ::testing::Test
{
protected:
    session_ticket_info create_valid_ticket()
    {
        session_ticket_info info;
        info.ticket_data = {0x01, 0x02, 0x03, 0x04, 0x05};
        info.expiry = std::chrono::system_clock::now() + 1h;
        info.server_name = "example.com";
        info.port = 443;
        info.max_early_data_size = 16384;
        info.ticket_age_add = 12345;
        info.received_time = std::chrono::system_clock::now();
        return info;
    }
};

TEST_F(SessionTicketInfoTest, EmptyTicketIsInvalid)
{
    session_ticket_info info;
    EXPECT_FALSE(info.is_valid());
}

TEST_F(SessionTicketInfoTest, ValidTicketIsValid)
{
    auto info = create_valid_ticket();
    EXPECT_TRUE(info.is_valid());
}

TEST_F(SessionTicketInfoTest, ExpiredTicketIsInvalid)
{
    auto info = create_valid_ticket();
    info.expiry = std::chrono::system_clock::now() - 1h;
    EXPECT_FALSE(info.is_valid());
}

TEST_F(SessionTicketInfoTest, ObfuscatedAgeCalculation)
{
    session_ticket_info info;
    info.ticket_data = {0x01};
    info.ticket_age_add = 1000;
    info.received_time = std::chrono::system_clock::now() - 100ms;

    auto age = info.get_obfuscated_age();
    // Age should be ~100ms + 1000 = ~1100
    EXPECT_GT(age, 1000u);
    EXPECT_LT(age, 2000u);
}

// ============================================================================
// session_ticket_store Tests
// ============================================================================

class SessionTicketStoreTest : public ::testing::Test
{
protected:
    session_ticket_store store;

    session_ticket_info create_ticket(const std::string& server,
                                       unsigned short port,
                                       int expiry_hours = 1)
    {
        session_ticket_info info;
        info.ticket_data = {0x01, 0x02, 0x03, 0x04};
        info.expiry = std::chrono::system_clock::now() +
                      std::chrono::hours(expiry_hours);
        info.server_name = server;
        info.port = port;
        info.received_time = std::chrono::system_clock::now();
        return info;
    }
};

TEST_F(SessionTicketStoreTest, InitiallyEmpty)
{
    EXPECT_EQ(store.size(), 0);
}

TEST_F(SessionTicketStoreTest, StoreAndRetrieve)
{
    auto ticket = create_ticket("example.com", 443);
    store.store("example.com", 443, ticket);

    EXPECT_EQ(store.size(), 1);
    EXPECT_TRUE(store.has_ticket("example.com", 443));

    auto retrieved = store.retrieve("example.com", 443);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->server_name, "example.com");
    EXPECT_EQ(retrieved->port, 443);
}

TEST_F(SessionTicketStoreTest, RetrieveNonExistent)
{
    auto retrieved = store.retrieve("nonexistent.com", 443);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(SessionTicketStoreTest, HasTicketReturnsFalseForNonExistent)
{
    EXPECT_FALSE(store.has_ticket("nonexistent.com", 443));
}

TEST_F(SessionTicketStoreTest, RemoveTicket)
{
    auto ticket = create_ticket("example.com", 443);
    store.store("example.com", 443, ticket);

    EXPECT_TRUE(store.remove("example.com", 443));
    EXPECT_EQ(store.size(), 0);
    EXPECT_FALSE(store.has_ticket("example.com", 443));
}

TEST_F(SessionTicketStoreTest, RemoveNonExistent)
{
    EXPECT_FALSE(store.remove("nonexistent.com", 443));
}

TEST_F(SessionTicketStoreTest, ReplaceExistingTicket)
{
    auto ticket1 = create_ticket("example.com", 443);
    ticket1.ticket_data = {0x01};
    store.store("example.com", 443, ticket1);

    auto ticket2 = create_ticket("example.com", 443);
    ticket2.ticket_data = {0x02};
    store.store("example.com", 443, ticket2);

    EXPECT_EQ(store.size(), 1);

    auto retrieved = store.retrieve("example.com", 443);
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->ticket_data.size(), 1);
    EXPECT_EQ(retrieved->ticket_data[0], 0x02);
}

TEST_F(SessionTicketStoreTest, MultipleServers)
{
    store.store("server1.com", 443, create_ticket("server1.com", 443));
    store.store("server2.com", 443, create_ticket("server2.com", 443));
    store.store("server3.com", 8443, create_ticket("server3.com", 8443));

    EXPECT_EQ(store.size(), 3);
    EXPECT_TRUE(store.has_ticket("server1.com", 443));
    EXPECT_TRUE(store.has_ticket("server2.com", 443));
    EXPECT_TRUE(store.has_ticket("server3.com", 8443));
    EXPECT_FALSE(store.has_ticket("server3.com", 443)); // Different port
}

TEST_F(SessionTicketStoreTest, CleanupExpired)
{
    // Create one valid and one expired ticket
    auto valid_ticket = create_ticket("valid.com", 443, 1);
    auto expired_ticket = create_ticket("expired.com", 443, -1);

    store.store("valid.com", 443, valid_ticket);
    store.store("expired.com", 443, expired_ticket);

    EXPECT_EQ(store.size(), 2);

    auto removed = store.cleanup_expired();
    EXPECT_EQ(removed, 1);
    EXPECT_EQ(store.size(), 1);
    EXPECT_TRUE(store.has_ticket("valid.com", 443));
    EXPECT_FALSE(store.has_ticket("expired.com", 443));
}

TEST_F(SessionTicketStoreTest, RetrieveExpiredReturnsNullopt)
{
    auto expired_ticket = create_ticket("expired.com", 443, -1);
    store.store("expired.com", 443, expired_ticket);

    // The store has the ticket, but retrieve returns nullopt for expired
    auto retrieved = store.retrieve("expired.com", 443);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(SessionTicketStoreTest, Clear)
{
    store.store("server1.com", 443, create_ticket("server1.com", 443));
    store.store("server2.com", 443, create_ticket("server2.com", 443));

    EXPECT_EQ(store.size(), 2);

    store.clear();
    EXPECT_EQ(store.size(), 0);
}

// ============================================================================
// replay_filter Tests
// ============================================================================

class ReplayFilterTest : public ::testing::Test
{
protected:
    replay_filter filter;

    std::vector<uint8_t> create_nonce(uint8_t value)
    {
        return std::vector<uint8_t>(32, value);
    }
};

TEST_F(ReplayFilterTest, InitiallyEmpty)
{
    EXPECT_EQ(filter.size(), 0);
}

TEST_F(ReplayFilterTest, FirstNonceAccepted)
{
    auto nonce = create_nonce(0x01);
    EXPECT_TRUE(filter.check_and_record(nonce));
    EXPECT_EQ(filter.size(), 1);
}

TEST_F(ReplayFilterTest, DuplicateNonceRejected)
{
    auto nonce = create_nonce(0x01);
    EXPECT_TRUE(filter.check_and_record(nonce));
    EXPECT_FALSE(filter.check_and_record(nonce));
}

TEST_F(ReplayFilterTest, DifferentNoncesAccepted)
{
    auto nonce1 = create_nonce(0x01);
    auto nonce2 = create_nonce(0x02);
    auto nonce3 = create_nonce(0x03);

    EXPECT_TRUE(filter.check_and_record(nonce1));
    EXPECT_TRUE(filter.check_and_record(nonce2));
    EXPECT_TRUE(filter.check_and_record(nonce3));
    EXPECT_EQ(filter.size(), 3);
}

TEST_F(ReplayFilterTest, Clear)
{
    auto nonce = create_nonce(0x01);
    filter.check_and_record(nonce);

    filter.clear();
    EXPECT_EQ(filter.size(), 0);

    // After clear, the same nonce should be accepted again
    EXPECT_TRUE(filter.check_and_record(nonce));
}

TEST_F(ReplayFilterTest, CleanupRemovesOldEntries)
{
    replay_filter::config cfg;
    cfg.window_size = 1s;
    cfg.max_entries = 100;

    replay_filter short_window_filter(cfg);

    auto nonce = create_nonce(0x01);
    auto past_time = std::chrono::system_clock::now() - 2s;
    short_window_filter.check_and_record(nonce, past_time);

    EXPECT_EQ(short_window_filter.size(), 1);

    auto removed = short_window_filter.cleanup();
    EXPECT_EQ(removed, 1);
    EXPECT_EQ(short_window_filter.size(), 0);
}

TEST_F(ReplayFilterTest, CustomConfiguration)
{
    replay_filter::config cfg;
    cfg.window_size = 5s;
    cfg.max_entries = 10;

    replay_filter custom_filter(cfg);

    // Add 10 entries
    for (uint8_t i = 0; i < 10; ++i)
    {
        auto nonce = create_nonce(i);
        EXPECT_TRUE(custom_filter.check_and_record(nonce));
    }

    EXPECT_EQ(custom_filter.size(), 10);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

class SessionTicketStoreConcurrencyTest : public ::testing::Test
{
};

TEST_F(SessionTicketStoreConcurrencyTest, ConcurrentStoreAndRetrieve)
{
    session_ticket_store store;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 100;

    auto store_func = [&store](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i)
        {
            session_ticket_info info;
            info.ticket_data = {static_cast<uint8_t>(thread_id),
                                static_cast<uint8_t>(i)};
            info.expiry = std::chrono::system_clock::now() + 1h;
            info.server_name = "server" + std::to_string(thread_id) + ".com";
            info.port = static_cast<unsigned short>(443 + thread_id);
            info.received_time = std::chrono::system_clock::now();

            store.store(info.server_name, info.port, info);
        }
    };

    auto retrieve_func = [&store](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i)
        {
            std::string server = "server" + std::to_string(thread_id % num_threads) + ".com";
            auto port = static_cast<unsigned short>(443 + (thread_id % num_threads));
            [[maybe_unused]] auto result = store.retrieve(server, port);
        }
    };

    std::vector<std::thread> threads;

    // Start store threads
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(store_func, i);
    }

    // Start retrieve threads
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(retrieve_func, i);
    }

    // Wait for all threads
    for (auto& t : threads)
    {
        t.join();
    }

    // Verify store has expected number of entries
    EXPECT_EQ(store.size(), num_threads);
}

class ReplayFilterConcurrencyTest : public ::testing::Test
{
};

TEST_F(ReplayFilterConcurrencyTest, ConcurrentCheckAndRecord)
{
    replay_filter filter;
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 100;

    std::atomic<int> accepted_count{0};
    std::atomic<int> rejected_count{0};

    auto check_func = [&filter, &accepted_count, &rejected_count](int thread_id) {
        for (int i = 0; i < operations_per_thread; ++i)
        {
            std::vector<uint8_t> nonce(32);
            // Create unique nonces per iteration, but same across threads
            nonce[0] = static_cast<uint8_t>(i);
            nonce[1] = static_cast<uint8_t>(thread_id);

            if (filter.check_and_record(nonce))
            {
                ++accepted_count;
            }
            else
            {
                ++rejected_count;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(check_func, i);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // Total should equal num_threads * operations_per_thread
    EXPECT_EQ(accepted_count + rejected_count, num_threads * operations_per_thread);

    // Each unique nonce should be accepted only once
    // Since thread_id is part of nonce, all should be unique
    EXPECT_EQ(accepted_count, num_threads * operations_per_thread);
}
