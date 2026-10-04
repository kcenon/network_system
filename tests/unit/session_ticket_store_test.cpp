/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/session_ticket_store.h"
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

using namespace std::chrono_literals;

/**
 * @file session_ticket_store_test.cpp
 * @brief Unit tests for QUIC session ticket store and replay filter
 *
 * Tests validate:
 * - session_ticket_info: is_valid(), get_obfuscated_age()
 * - session_ticket_store: default construction, store/retrieve round-trip,
 *   expired ticket handling, remove(), cleanup_expired(), clear(),
 *   has_ticket(), size()
 * - replay_filter: check_and_record() accepts first, rejects duplicate nonce,
 *   cleanup(), clear(), custom configuration
 */

// ============================================================================
// session_ticket_info Tests
// ============================================================================

class SessionTicketInfoTest : public ::testing::Test
{
protected:
    quic::session_ticket_info create_valid_ticket()
    {
        quic::session_ticket_info info;
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

TEST_F(SessionTicketInfoTest, DefaultTicketIsInvalid)
{
    quic::session_ticket_info info;
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

TEST_F(SessionTicketInfoTest, FutureExpiryIsValid)
{
    auto info = create_valid_ticket();
    info.expiry = std::chrono::system_clock::now() + 24h;
    EXPECT_TRUE(info.is_valid());
}

TEST_F(SessionTicketInfoTest, ObfuscatedAgeCalculation)
{
    quic::session_ticket_info info;
    info.ticket_data = {0x01};
    info.ticket_age_add = 1000;
    info.received_time = std::chrono::system_clock::now() - 100ms;

    auto age = info.get_obfuscated_age();
    // Age should be ~100ms + 1000 = ~1100
    EXPECT_GT(age, 1000u);
    EXPECT_LT(age, 2000u);
}

TEST_F(SessionTicketInfoTest, ObfuscatedAgeWithZeroAddValue)
{
    quic::session_ticket_info info;
    info.ticket_data = {0x01};
    info.ticket_age_add = 0;
    info.received_time = std::chrono::system_clock::now() - 200ms;

    auto age = info.get_obfuscated_age();
    // Age should be ~200ms with no offset
    EXPECT_GT(age, 100u);
    EXPECT_LT(age, 500u);
}

// ============================================================================
// session_ticket_store Default State Tests
// ============================================================================

class SessionTicketStoreDefaultTest : public ::testing::Test
{
};

TEST_F(SessionTicketStoreDefaultTest, InitialSizeIsZero)
{
    quic::session_ticket_store store;
    EXPECT_EQ(store.size(), 0);
}

TEST_F(SessionTicketStoreDefaultTest, RetrieveFromEmptyReturnsNullopt)
{
    quic::session_ticket_store store;
    auto result = store.retrieve("example.com", 443);
    EXPECT_FALSE(result.has_value());
}

TEST_F(SessionTicketStoreDefaultTest, HasTicketReturnsFalseForEmpty)
{
    quic::session_ticket_store store;
    EXPECT_FALSE(store.has_ticket("example.com", 443));
}

// ============================================================================
// session_ticket_store Store/Retrieve Tests
// ============================================================================

class SessionTicketStoreTest : public ::testing::Test
{
protected:
    quic::session_ticket_store store;

    quic::session_ticket_info create_ticket(const std::string& server,
                                             unsigned short port,
                                             int expiry_hours = 1)
    {
        quic::session_ticket_info info;
        info.ticket_data = {0x01, 0x02, 0x03, 0x04};
        info.expiry = std::chrono::system_clock::now()
                      + std::chrono::hours(expiry_hours);
        info.server_name = server;
        info.port = port;
        info.received_time = std::chrono::system_clock::now();
        return info;
    }
};

TEST_F(SessionTicketStoreTest, StoreAndRetrieveRoundTrip)
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

TEST_F(SessionTicketStoreTest, RetrieveNonExistentReturnsNullopt)
{
    auto retrieved = store.retrieve("nonexistent.com", 443);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(SessionTicketStoreTest, ExpiredTicketRetrieveReturnsNullopt)
{
    auto expired_ticket = create_ticket("expired.com", 443, -1);
    store.store("expired.com", 443, expired_ticket);

    auto retrieved = store.retrieve("expired.com", 443);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(SessionTicketStoreTest, RemoveExistingReturnsTrue)
{
    store.store("example.com", 443, create_ticket("example.com", 443));

    EXPECT_TRUE(store.remove("example.com", 443));
    EXPECT_EQ(store.size(), 0);
    EXPECT_FALSE(store.has_ticket("example.com", 443));
}

TEST_F(SessionTicketStoreTest, RemoveNonExistentReturnsFalse)
{
    EXPECT_FALSE(store.remove("nonexistent.com", 443));
}

TEST_F(SessionTicketStoreTest, CleanupExpiredRemovesOnlyExpired)
{
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

TEST_F(SessionTicketStoreTest, ClearEmptiesStore)
{
    store.store("server1.com", 443, create_ticket("server1.com", 443));
    store.store("server2.com", 443, create_ticket("server2.com", 443));

    EXPECT_EQ(store.size(), 2);

    store.clear();
    EXPECT_EQ(store.size(), 0);
}

TEST_F(SessionTicketStoreTest, HasTicketCorrectness)
{
    store.store("example.com", 443, create_ticket("example.com", 443));

    EXPECT_TRUE(store.has_ticket("example.com", 443));
    EXPECT_FALSE(store.has_ticket("example.com", 8443));
    EXPECT_FALSE(store.has_ticket("other.com", 443));
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

TEST_F(SessionTicketStoreTest, MultipleServersDistinctByPort)
{
    store.store("server.com", 443, create_ticket("server.com", 443));
    store.store("server.com", 8443, create_ticket("server.com", 8443));

    EXPECT_EQ(store.size(), 2);
    EXPECT_TRUE(store.has_ticket("server.com", 443));
    EXPECT_TRUE(store.has_ticket("server.com", 8443));
}

// ============================================================================
// replay_filter Tests
// ============================================================================

class ReplayFilterTest : public ::testing::Test
{
protected:
    quic::replay_filter filter;

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

TEST_F(ReplayFilterTest, ClearAllowsReuse)
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
    quic::replay_filter::config cfg;
    cfg.window_size = 1s;
    cfg.max_entries = 100;

    quic::replay_filter short_window_filter(cfg);

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
    quic::replay_filter::config cfg;
    cfg.window_size = 5s;
    cfg.max_entries = 10;

    quic::replay_filter custom_filter(cfg);

    for (uint8_t i = 0; i < 10; ++i)
    {
        auto nonce = create_nonce(i);
        EXPECT_TRUE(custom_filter.check_and_record(nonce));
    }

    EXPECT_EQ(custom_filter.size(), 10);
}
