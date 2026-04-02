// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include "../framework/system_fixture.h"
#include "../framework/test_helpers.h"

namespace kcenon::network::integration_tests {

/**
 * @brief Test suite for protocol integration scenarios
 *
 * Tests protocol-level functionality including:
 * - Message serialization and deserialization
 * - Request-response patterns
 * - Message fragmentation
 * - Protocol handshakes
 */
class ProtocolIntegrationTest : public NetworkSystemFixture {};

// ============================================================================
// Message Serialization Tests
// ============================================================================

TEST_F(ProtocolIntegrationTest, SmallMessageTransmission) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send small message (< 1KB)
    auto message = test_helpers::create_text_message("Hello, Network!");
    EXPECT_TRUE(SendMessage(std::move(message)));

    // Allow time for message processing
    WaitFor(50);
}

TEST_F(ProtocolIntegrationTest, MediumMessageTransmission) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send medium message (1KB - 10KB)
    auto message = CreateTestMessage(5 * 1024);  // 5KB
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(100);
}

TEST_F(ProtocolIntegrationTest, LargeMessageTransmission) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send large message (> 10KB)
    auto message = CreateTestMessage(50 * 1024);  // 50KB
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(200);
}

TEST_F(ProtocolIntegrationTest, EmptyMessageHandling) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Try to send empty message
    std::vector<uint8_t> empty_message;
    auto result = client_->send_packet(std::move(empty_message));

    // Should fail with invalid argument
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Message Pattern Tests
// ============================================================================

TEST_F(ProtocolIntegrationTest, SequentialMessages) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send messages sequentially
    for (int i = 0; i < 5; ++i) {
        auto message = test_helpers::create_text_message(
            "Message #" + std::to_string(i)
        );
        EXPECT_TRUE(SendMessage(std::move(message)));
        WaitFor(20);
    }
}

TEST_F(ProtocolIntegrationTest, BurstMessages) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send burst of messages without delay
    for (int i = 0; i < 10; ++i) {
        auto message = CreateTestMessage(256, static_cast<uint8_t>(i));
        EXPECT_TRUE(SendMessage(std::move(message)));
    }

    // Allow time for all messages to be processed
    WaitFor(500);
}

TEST_F(ProtocolIntegrationTest, AlternatingMessageSizes) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Alternate between small and large messages
    for (int i = 0; i < 5; ++i) {
        // Small message
        auto small_msg = CreateTestMessage(128);
        EXPECT_TRUE(SendMessage(std::move(small_msg)));

        // Large message
        auto large_msg = CreateTestMessage(8192);
        EXPECT_TRUE(SendMessage(std::move(large_msg)));

        WaitFor(50);
    }
}

// ============================================================================
// Message Fragmentation Tests
// ============================================================================

TEST_F(ProtocolIntegrationTest, MessageFragmentation) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send very large message that may require fragmentation
    auto message = CreateTestMessage(100 * 1024);  // 100KB
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(500);
}

TEST_F(ProtocolIntegrationTest, MultipleFragmentedMessages) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send multiple large messages
    for (int i = 0; i < 3; ++i) {
        auto message = CreateTestMessage(64 * 1024);  // 64KB each
        EXPECT_TRUE(SendMessage(std::move(message)));
        WaitFor(100);
    }
}

// ============================================================================
// Data Integrity Tests
// ============================================================================

TEST_F(ProtocolIntegrationTest, BinaryDataTransmission) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send random binary data
    auto message = test_helpers::generate_random_data(2048);
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(100);
}

TEST_F(ProtocolIntegrationTest, SequentialDataPattern) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send sequential data pattern
    auto message = test_helpers::generate_sequential_data(4096);
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(100);
}

TEST_F(ProtocolIntegrationTest, RepeatingPatternData) {
    ASSERT_TRUE(StartServer());
    ASSERT_TRUE(ConnectClient());

    // Send data with repeating pattern
    auto message = CreateTestMessage(1024, 0xAA);
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(50);
}

// ============================================================================
// Protocol Handshake Tests
// ============================================================================

TEST_F(ProtocolIntegrationTest, InitialHandshake) {
    ASSERT_TRUE(StartServer());

    // Connection establishment includes implicit handshake
    EXPECT_TRUE(ConnectClient());

    // First message after handshake
    auto message = test_helpers::create_text_message("First message");
    EXPECT_TRUE(SendMessage(std::move(message)));

    WaitFor(50);
}

} // namespace kcenon::network::integration_tests
